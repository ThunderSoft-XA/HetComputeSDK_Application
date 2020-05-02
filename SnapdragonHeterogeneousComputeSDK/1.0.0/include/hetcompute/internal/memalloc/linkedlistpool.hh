#pragma once

#include <atomic>
#include <string>
#include <hetcompute/internal/memalloc/alignedmalloc.hh>
#include <hetcompute/internal/util/strprintf.hh>
#include <hetcompute/internal/util/debug.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace allocatorinternal
        {
            class thread_local_fixed_pool_set;

            /// This class represents a linked list pool.
            /// Famous next-avail embedded linked list data structure for implementing the pool.
            /// The pool has a next-avail pointer and also a link list entry associated with each entry in the list.
            /// Initially next-avail points to the first element in the pool and the elements in the embedded linked list
            /// point to their corresponding next entries in pool.
            /// So for a pool of size 4 the pool and its embedded linked list will look like this
            /// (o0 .. o3 are the object entries in the pool):
            /// next-avail = 0
            /// 1:o0:2:o1:3:o2:END:o3, linked list = 0, 1, 2, 3
            /// So initially the content of the linked list is 0, 1 ,2 and 3. Upon the first allocation,
            /// next-avail will be pointing to next-avail = pool[next-avail].linklist which is 1 (no
            /// change to pool[0]). At this point the free linked list has entries 1, 2, and 3. Upon the
            /// second allocation, next-avail will become 2. So the content of the linked list is 2 and 3 and
            /// the pool looks like this:
            /// next-avail = 2
            /// 1:o0:2:o1:3:o2:END:o3, linked list = 2, 3
            /// Now if o0 is released the corresponding linked list entry will point to the current next-avail
            /// and next-avail will point to that entry. So the linked list will look like this and the content of
            /// the linked list will be 0, 2, 3:
            /// next-avail = 0
            /// 2:o0:2:o1:3:o2:END:o3 // linked list 0, 2, 3
            /// And so on. Once next-avail points to END that indicates that the pool is full. This algorithm has a
            /// fast allocation/deallocation and good locality (LIFO deallocation) but using the embedded linked list
            /// for book keeping has some overhead (at least one byte per pool entry).

            /// In this class, allocate_raw and deallocate_raw can be accessed by local or external threads.
            /// This implementation assumes if the thread is local (the owner thread) the free list passed to those
            /// functions is the local_free_list only owned by that thread. If the accessing thread is external
            /// (any thread other than the owner) the free list is a shared pointer and accessing it is protected.
            /// The class thread_local_allocator uses linkedlistpools are arranges accesses and free lists.
            typedef thread_local_fixed_pool_set owner_pool_t;

            class linkedlist_pool
            {
                /// This structure represents the book keeping header for each element in the list.
                /// The header includes the next pointer in the linked list and the pool containing
                /// the linked list. These fields are used during alloc/dealloc
                /// The internal buffer is arranged as follows. The part before the object is the header.
                /// .... |next|pool|    object     |next|pool|....object....|
                struct element_header
                {
                    // The next element in the linked list
                    element_header* _next_empty;
                    // The pool associated with this element (object)
                    owner_pool_t* _pool;
                };

                /// The offset of the the element header from the object in the buffer
                static constexpr size_t s_header_offset = sizeof(element_header);

            public:
                /// Constructor:
                /// @param pool_size: max number of elements in the pool
                /// @param object_size: max object sizes in the pool (size class)
                /// @param pool: the shared pool containing this pool
                linkedlist_pool(size_t pool_size, size_t object_size, owner_pool_t* pool)
                    : _pool_size(pool_size),
                      // Need to ensure that the elements within the buffer are aligned. Here we choose 4-byte alignment
                      _element_size((static_cast<int>(sizeof(element_header) + object_size + 3) / 4) * 4),
                      _owner_pool(pool),
                      _total_deallocs(0),
                      _buffer(nullptr),
                      _next_pool(nullptr),
                      _prev_pool(nullptr)
                {
                    _buffer = reinterpret_cast<char*>(hetcompute_aligned_malloc(128, _pool_size * _element_size));
                    if (_buffer == nullptr)
                    {
#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                        throw std::bad_alloc();
#else
                        HETCOMPUTE_ELOG("HetCompute Memory allocation failed in linkedlist_pool");
#endif
                    }
                    initialize_buffer();
                }

                element_header* get_first_entry()
                {
                    return reinterpret_cast<element_header*>(_buffer);
                }

                /// Destructor: simply deallocate the buffer
                virtual ~linkedlist_pool()
                {
                    hetcompute_aligned_free(_buffer);
                }

                HETCOMPUTE_DELETE_METHOD(linkedlist_pool(linkedlist_pool const&));
                HETCOMPUTE_DELETE_METHOD(linkedlist_pool& operator=(linkedlist_pool const&));
                HETCOMPUTE_DELETE_METHOD(linkedlist_pool(linkedlist_pool const&&));
                HETCOMPUTE_DELETE_METHOD(linkedlist_pool& operator=(linkedlist_pool const&&));

                /// Initializes the buffer by filling up the headers of all elements in the buffer
                void initialize_buffer()
                {
                    char* buffer = _buffer;
                    for (size_t i = 0; i < _pool_size; i++)
                    {
                        auto element      = reinterpret_cast<element_header*>(buffer);
                        auto next_element = reinterpret_cast<element_header*>(buffer + _element_size);
                        // Setting the next empty part of the header
                        if (i == _pool_size - 1)
                        {
                            element->_next_empty = nullptr;
                        }
                        else
                        {
                            element->_next_empty = next_element;
                        }
                        // Setting the pool part of the header
                        element->_pool = _owner_pool;
                        buffer         = reinterpret_cast<char*>(next_element);
                    }
                }

                /// Allocates the next element pointed by next empty pointer
                // @param shared_next_empty: pointer to the shared free list
                /// @return a pointer to the object associated with that element
                inline static char* allocate_raw(element_header*& shared_next_empty)
                {
                    if (shared_next_empty != nullptr)
                    {
                        // Retrieve the object in the next element
                        char* object = (reinterpret_cast<char*>(shared_next_empty) + s_header_offset);
                        // Update the next empty pointer to point to the next element in the linked list
                        shared_next_empty = shared_next_empty->_next_empty;
                        // Return the object
                        return object;
                    }
                    return nullptr;
                }

                /// Extracts the shared pool associated with an object allocated by this type of allocator
                /// @param object: the object
                /// @return extracted pool
                inline static owner_pool_t* extract_owner_pool(char* object)
                {
                    element_header* element = reinterpret_cast<element_header*>(object - s_header_offset);
                    return element->_pool;
                }

                /// Deallocates the object (memory) and returns it back to the pool buffer
                /// @para memory: object to deallocate
                /// @para shared_next_empty: shared next empty pointer
                inline static bool deallocate_raw(char* memory, element_header*& shared_next_empty)
                {
                    // Retrieve the element
                    element_header* element = reinterpret_cast<element_header*>(memory - s_header_offset);
                    element->_next_empty    = shared_next_empty;
                    // Add the freed element into the linked list
                    shared_next_empty = element;
                    return false;
                }

                /// Counts the number of elements in the linked list (only for debugging usage)
                static size_t get_linked_list_size(element_header* next_empty)
                {
                    size_t count = 0;
                    while (next_empty != nullptr)
                    {
                        next_empty = next_empty->_next_empty;
                        count++;
                    }
                    return count;
                }

                size_t get_total_deallocs()
                {
                    return _total_deallocs;
                }

                /// Returns the next pool in the chain
                inline linkedlist_pool* get_next_pool()
                {
                    return _next_pool;
                }

                /// Returns the previous pool in the chain
                inline linkedlist_pool* get_prev_pool()
                {
                    return _prev_pool;
                }

                /// Sets the next pool in the chain
                inline void set_next_pool(linkedlist_pool* next)
                {
                    _next_pool = next;
                }

                /// Sets the previous pool in the chain
                inline void set_prev_pool(linkedlist_pool* prev)
                {
                    _prev_pool = prev;
                }

            private:
                /// Size of the pool
                size_t _pool_size;

                /// the size of object plus the its header in the buffer
                size_t _element_size;

                /// The owner shared pool
                owner_pool_t* _owner_pool;

                /// totoal number of deallocs
                size_t _total_deallocs;

                /// the internal buffer
                char* _buffer;

                /// Next pool in the size class chain
                linkedlist_pool* _next_pool;

                /// Prev pool in the size class chain
                linkedlist_pool* _prev_pool;

                friend thread_local_fixed_pool_set;
            };  // class linkedlist_pool
        };  // namespace allocatorinternal
    };  // namespace internal
};  // namespace hetcompute
