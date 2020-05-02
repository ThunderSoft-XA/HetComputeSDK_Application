#pragma once

#include <atomic>
#include <string>

#include <hetcompute/internal/memalloc/pool.hh>
#include <hetcompute/internal/util/debug.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace allocatorinternal
        {
            class slab_allocator;

            /**
             * This class represent a bump pointer pool. In this pool, the objects
             * are allocated sequentially from a single thread but deallocation is done in bulk.
             *
             * The pool can be concurrent or non-concurrent which dictates how deallocation is done.
             *
             *  - If the pool is non-concurrent: the pool must be deallocated by the user and that will
             *    reset the bump counter so releasing all the pool's memory all at once (bulk deallocation).
             *
             *  - If the pool is concurrent (ref counter): they can be deallocated from different
             *    threads. But every deallocation decrement the ref counter of the pool and once it
             *    hits zero the pool is deallocated automatically.
             *
             * Note that deallocation DOES NOT call the destructor of the objects in the pool.
             * It must be done by the client.
             */
            class concurrent_bump_pool : public pool
            {
            public:
                /**
                 * Constructor: using a preallocated memory
                 */
                concurrent_bump_pool(size_t id, size_t pool_size_in_bytes, char* pool_buffer, slab_allocator* allocator)
                    : _buffer(pool_buffer),
                      _pointer(pool_buffer),
                      _allocator(allocator),
                      _id(id),
                      _pool_size_in_bytes(pool_size_in_bytes),
                      _total_allocs(0),
                      _current_allocs(0),
                      _ref_count(0),
                      _concurrent(false)
                {
                    HETCOMPUTE_INTERNAL_ASSERT(pool_size_in_bytes > 0, "Cannot allocate an empty bump pool");
                }

                ~concurrent_bump_pool() {}

                /**
                 * Decrements the pool's ref_count (only called when the pool is ref counted) and releases the pool if ref count becomes 0.
                 *
                 * @param object: the object to be deallocated.
                 * @return true if the pool is released.
                 */
                virtual bool deallocate_object(char* object) { return deallocate_object_impl(object); }

                /**
                 * Decrements the pool's ref_count (only called when the pool is ref counted).
                 * Concurrent bump allocator ignores threadid.
                 * NOTE: This implementation is required since inheriting the pool class.
                 * @param object: the object to be deallocated.
                 * @param threadid Calling's thread thread id. Not used.
                 */
                virtual bool deallocate_object(char* object, size_t threadid)
                {
                    HETCOMPUTE_UNUSED(threadid);
                    return deallocate_object_impl(object);
                }

                /**
                 * Allocates an object from the pool.
                 *
                 * @param size: Size of the needed object.
                 * @return allocated object or null on failure
                 */
                void* allocate_object(size_t size)
                {
                    char* object = nullptr;
                    if ((_pointer + size + pool::s_embedded_pool_offset) <= (_buffer + _pool_size_in_bytes))
                    {
                        _pointer = embed_pool(_pointer, this);
                        object   = _pointer;
                        _pointer += size;
                        _current_allocs++;
                        return static_cast<void*>(object);
                    }
                    return nullptr;
                }

                /**
                 * Releases the pool.
                 * Must be called only on non-ref-counted pool when the client is done by the pool.
                 */
                void release();

                /**
                 * Acquires the pool, discards any record of previously allocated objects.
                 * @param ref_count: the initial pool's ref count, if zero the pool is not ref counted.
                 * In this design, we assume that the user knows how many RCed object will be allocated to
                 * this pool across the life time of this pool. So it can be set initially.
                 */
                bool acquire(size_t ref_count = 0)
                {
                    // reset _pointer to the beginning
                    _pointer = _buffer;
                    // accumulate the old alloc count before resetting
                    _total_allocs += _current_allocs;

                    if (ref_count > 0)
                    {
                        _ref_count.store(ref_count, hetcompute::mem_order_release);
                        _concurrent = true;
                    }
                    else
                    {
                        _concurrent = false;
                    }
                    _current_allocs = 0;
                    return true;
                }

                /**
                 * Returns id of the bump pool.
                 *
                 * @return the id of the bump pool.
                 */
                size_t get_id() const { return _id; }

                /**
                 * Checks whether an object is in this pool.
                 *
                 * @param object: the input object to be checked.
                 * @return: true if the object is in the pool. Otherwise, false
                 */
                bool is_owned(char* object) const { return ((object >= _buffer) && (object < _buffer + _pool_size_in_bytes)); }

                /**
                 * Returns the size of the header for each object in the pool.
                 * @return Size of the header for each object.
                 */
                static size_t get_object_header_size() { return pool::s_embedded_pool_offset; }

                /**
                 * Returns pointer to the allocator that owns the pool.
                 * @return Pointer to the the pool owner.
                 */
                slab_allocator* get_allocator() const { return _allocator; }

                /**
                 * Returns the currently allocated objects.
                 *
                 * @return currently allocated objects.
                 */
                size_t get_current_allocs() const { return _current_allocs; };

                /**
                 * Returns a textual description of the pool.
                 *
                 * @return std::string with a textual description of the pool.
                 */
                virtual std::string to_string()
                {
                    std::string str = strprintf("Concurrent Bump pool info: pool[%zu] %p allocs %zu RC %zu pointer = %p size = %zu",
                                                _id,
                                                _buffer,
                                                _total_allocs,
                                                _ref_count.load(hetcompute::mem_order_relaxed),
                                                _pointer,
                                                _pool_size_in_bytes);
                    return str;
                }

                HETCOMPUTE_DELETE_METHOD(concurrent_bump_pool(concurrent_bump_pool const&));
                HETCOMPUTE_DELETE_METHOD(concurrent_bump_pool(concurrent_bump_pool const&&));
                HETCOMPUTE_DELETE_METHOD(concurrent_bump_pool& operator=(concurrent_bump_pool const&));
                HETCOMPUTE_DELETE_METHOD(concurrent_bump_pool&& operator=(concurrent_bump_pool const&&));

            private:
                /**
                 * Actual implementation of deallocate_object.
                 * @param object Object to deallocate.
                 */
                bool deallocate_object_impl(char* object);

                /** The pool's raw buffer, doesn't change */
                char* _buffer;

                /** A running pointer to keep track of already used memory. */
                char* _pointer;

                /** The slab allocator owner of this pool */
                slab_allocator* _allocator;

                /** Number of total allocations to this pool */
                size_t _total_allocs;

                /** Number of current allocated objects */
                size_t _current_allocs;

                /** The size of the pool in bytes */
                size_t _pool_size_in_bytes;

                /** Pool's id */
                size_t _id;

                /** Pool's ref count (RC) */
                std::atomic<size_t> _ref_count;

                /** Whether the pool is concurrent */
                bool _concurrent;
            };

        }; // namespace allocatorinternal

    }; // namespace internal

}; // namespace hetcompute
