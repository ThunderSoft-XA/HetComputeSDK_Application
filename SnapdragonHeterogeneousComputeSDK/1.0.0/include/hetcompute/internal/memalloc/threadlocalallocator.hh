#pragma once

#include <atomic>
#include <string>

#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/memalloc/defaulttaskallocator.hh>
#include <hetcompute/internal/memalloc/linkedlistpool.hh>
#include <hetcompute/internal/util/memorder.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace allocatorinternal
        {
            /**
             * The class represent the thread local allocator size class pool chain
             * Each pool set has a linked list of pools and an active pool
             * Thread local allocator has a pool set for each size class
             */
            class thread_local_fixed_pool_set : public pool
            {
                /**
                 * Simple spin lock for use by the futex_queue
                 */
                class spin_lock
                {
                private:
                    std::atomic<bool> _held;

                public:
                    /**
                     * Constructor. Spin lock is not held.
                     */
                    spin_lock() : _held(false) {}
                    HETCOMPUTE_DELETE_METHOD(spin_lock(spin_lock&));
                    HETCOMPUTE_DELETE_METHOD(spin_lock(spin_lock&&));
                    HETCOMPUTE_DELETE_METHOD(spin_lock& operator=(spin_lock const&));
                    HETCOMPUTE_DELETE_METHOD(spin_lock& operator=(spin_lock&&));

                    /**
                     * Spins until it manages to acquire the lock.
                     */
                    void lock()
                    {
                        bool acquired = false;

                        while (acquired == false)
                        {
                            // Avoid the CAS until it's more likely to succeed
                            while (_held.load(hetcompute::mem_order_relaxed) == true)
                            {
                            }

                            bool old_value = false;
                            acquired       = _held.compare_exchange_strong(old_value, true, hetcompute::mem_order_acquire);
                        }
                    }

                    /**
                     * Unlocks spin lock.
                     */
                    void unlock() { _held.store(false, hetcompute::mem_order_release); }
                };

                /**
                 * This padding guarantees that local and external list pointer
                 * are far enough from each so they are in different L2 lines
                 */

                static constexpr size_t s_alignment_padding_bytes = 128;

            public:
                /**
                 * Constructor
                 *
                 * @param threadid Thread id of this allocator
                 * @param pool_size Number of elements in each linked list pool
                 * @param object_size Object size in the pools.
                 */
                thread_local_fixed_pool_set(size_t threadid, size_t pool_size, size_t object_size)
                    : _merged_local_free_list(nullptr),
                      _allocs(0),
                      _local_deallocs(0),
                      _tid(threadid),
                      _first_pool(pool_size, object_size, this), // Set the first pool
                      _active_pool(nullptr),
                      _last_pool(&_first_pool),
                      _lock(),
                      _external_deallocs(0),
                      _list_swaps(0),
                      _merged_external_free_list(nullptr)
                {
                    set_active_pool(&_first_pool);
                    HETCOMPUTE_UNUSED(_padding);
                }

                /**
                 * Destructor: destroyes the pool linked list chain.
                 */
                ~thread_local_fixed_pool_set()
                {
                    // Deallocating all the pools of this size class except the first one
                    linkedlist_pool* curr_pool = _first_pool.get_next_pool();
                    linkedlist_pool* next_pool;
                    while (curr_pool != nullptr)
                    {
                        next_pool = curr_pool->get_next_pool();
                        delete curr_pool;
                        curr_pool = next_pool;
                    }
                }

                /**
                 * Removes a given pool from the pool linked list
                 * @param old_pool: the pool to be removed
                 */
                void remove_pool(linkedlist_pool* old_pool)
                {
                    HETCOMPUTE_INTERNAL_ASSERT(old_pool != &_first_pool, "The first pool cannot be removed!");
                    linkedlist_pool* prev_pool = old_pool->get_prev_pool();
                    linkedlist_pool* next_pool = old_pool->get_prev_pool();
                    if (prev_pool != nullptr)
                    {
                        prev_pool->set_next_pool(next_pool);
                    }
                    if (next_pool != nullptr)
                    {
                        next_pool->set_prev_pool(prev_pool);
                    }
                }

                /**
                 * Adds a given pool to the pool linked list
                 * @param new_pool: the pool to be added
                 */
                void add_pool(linkedlist_pool* new_pool)
                {
                    HETCOMPUTE_INTERNAL_ASSERT(new_pool->_next_pool == nullptr, "New pool not initialized properly!");
                    _last_pool->set_next_pool(new_pool);
                    if (new_pool != nullptr)
                    {
                        new_pool->set_prev_pool(_last_pool);
                        _last_pool = new_pool;
                    }
                }

                /**
                 * Sets the active pool
                 * @param active_pool: the new active pool (the pool must be newly created)
                 */
                void set_active_pool(linkedlist_pool* active_pool)
                {
                    _active_pool            = active_pool;
                    _merged_local_free_list = _active_pool->get_first_entry();
                }

                /**
                 * Deallocates an allocated object
                 * @param object: to be deallocated
                 * @return: true on success
                 */
                virtual bool deallocate_object(char* object)
                {
                    // Extract thread id using thread-local storage
                    auto ti       = tls::get();
                    auto threadid = ti->allocator_id();
                    return deallocate_object_impl(object, threadid);
                }

                /**
                 * Deallocates an allocated object from a particular thread
                 * @param object to be deallocated
                 * @return: true on success
                 */
                virtual bool deallocate_object(char* object, size_t threadid) { return deallocate_object_impl(object, threadid); }
                /**
                 * Allocates a new object.
                 *
                 * @return Pointer to allocated object.
                 */
                char* allocate_object()
                {
                    // Allocate from the local free list
                    _allocs++;
                    char* object = linkedlist_pool::allocate_raw(_merged_local_free_list);
                    if (object != nullptr)
                    {
                        return object;
                    }
                    // Swap local and external lists
                    _lock.lock();
                    _list_swaps++;
                    std::swap(_merged_local_free_list, _merged_external_free_list);
                    _lock.unlock();
                    // Try to allocate locally again
                    object = linkedlist_pool::allocate_raw(_merged_local_free_list);
                    if (object != nullptr)
                    {
                        return object;
                    }
                    return nullptr;
                }

                /**
                 * Returns a textual description of the pool.
                 *
                 * @return std::string with a textual description of the pool.
                 */
                std::string to_string()
                {
                    std::string str   = "";
                    float       ratio = 0.0;
                    if (_allocs > 0)
                    {
                        ratio = _external_deallocs / float(_allocs);
                    }

                    str = strprintf("Total allocs %zu, local deallocs %zu, extranl deallocs %zu (%2.1f percent), swaps %zu total pools %zu",
                                    _allocs,
                                    _local_deallocs,
                                    _external_deallocs,
                                    ratio * 100,
                                    _list_swaps,
                                    get_pool_count());
                    return str;
                }

                /**
                 * Return the current active pool.
                 *
                 * @return linkiedlist_pool Pointer to active pool.
                 */
                linkedlist_pool* get_active_pool() const
                {
                    return _active_pool;
                };
                /**
                 * Returns number of pools in the chain.
                 *
                 * @return Number of pools in chain.
                 */
                size_t get_pool_count()
                {
                    linkedlist_pool* curr_pool  = &_first_pool;
                    size_t           pool_count = 0;
                    while (curr_pool != nullptr)
                    {
                        pool_count++;
                        curr_pool = curr_pool->get_next_pool();
                    }
                    return pool_count;
                }

                /**
                 * Returns internal list size.
                 *
                 * @return Internal list size.
                 */
                size_t get_local_free_list_size() const
                {
                    return linkedlist_pool::get_linked_list_size(_merged_local_free_list);
                }

                /// Returns external list size
                size_t get_external_free_list_size()
                {
                    _lock.lock();
                    size_t size = linkedlist_pool::get_linked_list_size(_merged_external_free_list);
                    _lock.unlock();
                    return size;
                }

            private:
                bool deallocate_object_impl(char* object, size_t threadid)
                {
                    if (threadid == _tid)
                    {
                        _local_deallocs++;
                        return linkedlist_pool::deallocate_raw(object, _merged_local_free_list);
                    }
                    else
                    {
                        _lock.lock();
                        _external_deallocs++;
                        bool b = linkedlist_pool::deallocate_raw(object, _merged_external_free_list);
                        _lock.unlock();
                        return b;
                    }
                    return true;
                }

                // Pointer to the beginning of the local free list
                linkedlist_pool::element_header* _merged_local_free_list;

                /// Total allocs
                size_t _allocs;

                /// Total local deallocs
                size_t _local_deallocs;

                size_t _tid;

                /// Alignment padding.
                char _padding[s_alignment_padding_bytes];

                /// First pool in the chain (statically created).
                linkedlist_pool _first_pool;

                /// Active pool in the chain from which allocation takes place.
                linkedlist_pool* _active_pool;

                /// Last pool in the chain.
                linkedlist_pool* _last_pool;

                /// Spin lock for accessing external free list or switching local/external free lists. */
                spin_lock _lock;

                /// Total external deallocs.
                size_t _external_deallocs;

                /// Total list swaps
                size_t _list_swaps;

                /// Pointer to the beginning of the external free list
                linkedlist_pool::element_header* _merged_external_free_list;

                HETCOMPUTE_DELETE_METHOD(thread_local_fixed_pool_set(thread_local_fixed_pool_set const&));
                HETCOMPUTE_DELETE_METHOD(thread_local_fixed_pool_set& operator=(thread_local_fixed_pool_set const&));
                HETCOMPUTE_DELETE_METHOD(thread_local_fixed_pool_set(thread_local_fixed_pool_set const&&));
                HETCOMPUTE_DELETE_METHOD(thread_local_fixed_pool_set& operator=(thread_local_fixed_pool_set const&&));
            };

            /**
             * Thread local allocator will be used by each device thread for allocation. However deallocation can
             * happen from any other threads.
             *
             * Given the one-producer-multiple-consumer nature of HetCompute task queues and
             * also the prior work on allocators with thread-private heaps for multi-threading (such as Hoard),
             * our current design decision is to thread-private heaps which means each main or worker thread
             * has its own allocator. This can work especially pretty well for worker threads in which we can
             * assume the stealing is rare. Therefore potentially most of allocation/deallocation can be done
             * locally with no synchronization. We may only need synchronization when a stealer is releasing task
             * owned by another worker thread.
             *
             * This allocator supports a chain of shared pool per size class (we limit t
             * one size class for now).
             * For each size classes there is an active pool (of type SharedPool) to which allocation.
             */
            class thread_local_allocator
            {
                enum class size_class
                {
                    small_size = 0,
                    // This object size class is not used right now but we may need it later
                    large_size = 1,
                    giant_size = 2
                };

                /// Default task allocator for over-sized tasks
                static default_allocator* s_default_allocator;

            public:
                /**
                 * Constructor:
                 * @param threadid: thread id of this allocator
                 * @param pool_size: number of elements in each linked list pool
                 * @param object_size: object size in the pools
                 */
                thread_local_allocator(size_t threadid, size_t pool_size, size_t object_size)
                    : _tid(threadid),
                      _pool_size(pool_size),
                      _default_object_size(object_size),
                      _default_size_class_pools(_tid, _pool_size, _default_object_size)
                {
                }

                /** Destructor */
                ~thread_local_allocator()
                {
                }

                /**
                 * Returns the small size class value (only for testing).
                 *
                 * @return Small size class value.
                 */
                static size_class get_small_size_class()
                {
                    return size_class::small_size;
                }

                /**
                 * Returns the giant size class value (only for testing).
                 *
                 * @return Giant size class value.
                 */
                static size_class get_giant_size_class()
                {
                    return size_class::giant_size;
                }

                /**
                 * Returns the default class size pool set.
                 *
                 * @return Default class size pool set.
                 */
                thread_local_fixed_pool_set* get_default_size_class_pool_set() const
                {
                    return const_cast<thread_local_fixed_pool_set*>(&_default_size_class_pools);
                }

                /**
                 * Returns allocator used to handle oversized objects.
                 * (Default_allocator by default).
                 *
                 * @return Pointer to allocator used to handle oversized objects.
                 */
                static default_allocator* get_oversize_allocator()
                {
                    return s_default_allocator;
                }

                /**
                 * Finds the class size of the object.
                 *
                 * @param object_size: size of the object.
                 * @return Class size.
                 */
                size_class find_size_class(size_t object_size) const
                {
                    if (object_size <= _default_object_size)
                    {
                        return size_class::small_size;
                    }
                    return size_class::giant_size;
                }

                /**
                 * Allocates a new object of size object_size.
                 *
                 * @param object_size: the object size.
                 *  @return pointer to the allocated object.
                 */
                char* allocate(size_t object_size)
                {
                    char*      object;
                    size_class object_size_class = find_size_class(object_size);
                    // Only allocate if the size class is small
                    if (object_size_class == size_class::small_size)
                    {
                        thread_local_fixed_pool_set* pool_chain = &_default_size_class_pools;
                        object                                  = pool_chain->allocate_object();
                        if (object != nullptr)
                        {
                            return object;
                        }
                        linkedlist_pool* new_pool = new linkedlist_pool(_pool_size, _default_object_size, pool_chain);
                        _default_size_class_pools.add_pool(new_pool);
                        _default_size_class_pools.set_active_pool(new_pool);
                        object = pool_chain->allocate_object();
                        return object;
                    }
                    // Else use the default allocator
                    if (object_size_class == size_class::giant_size)
                    {
                        object = s_default_allocator->allocate_object(object_size);
                        return object;
                    }
                    return nullptr;
                }

                /**
                 * Initializes default allocator.
                 */
                static void init_default_allocator()
                {
                    s_default_allocator = new default_allocator();
                }

                /**
                 * Shuts default allocator down.
                 */
                static void shutdown_default_allocator()
                {
                    delete s_default_allocator;
                }

                /**
                 * Allocates task using the default allocator.
                 *
                 * @param object_size: task size.
                 * @return Pointer to allocated task.
                 */
                static char* allocate_default(size_t object_size)
                {
                    return s_default_allocator->allocate_object(object_size);
                }

                /**
                 * Returns a string describing the allocator
                 */
                std::string to_string()
                {
                    std::string str = strprintf("Thread Local Allocator %zu\n%s", _tid, _default_size_class_pools.to_string().c_str());
                    return str;
                }

                /**
                 * Returns threadid for allocator.
                 */
                size_t get_threadid()
                {
                    return _tid;
                }

                HETCOMPUTE_DELETE_METHOD(thread_local_allocator(thread_local_allocator const&));
                HETCOMPUTE_DELETE_METHOD(thread_local_allocator& operator=(thread_local_allocator const&));
                HETCOMPUTE_DELETE_METHOD(thread_local_allocator(thread_local_allocator const&&));
                HETCOMPUTE_DELETE_METHOD(thread_local_allocator& operator=(thread_local_allocator const&&));

            private:
                /// threadid of this allocator
                size_t _tid;

                /// The pool size of each of the pools in this allocator
                size_t _pool_size;

                /// The only object size class supported
                size_t _default_object_size;

                /// the pool chain
                thread_local_fixed_pool_set _default_size_class_pools;
            };

        }; // namespace hetcompute::internal::allocatorinternal
    };     // namespace hetcompute::internal
};         // namespace hetcompute
