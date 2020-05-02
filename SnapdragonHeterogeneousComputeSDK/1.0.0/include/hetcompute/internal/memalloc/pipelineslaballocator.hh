#pragma once

// Include std headers
#include <atomic>

// Include internal headers
#include <hetcompute/internal/memalloc/poolidassigner.hh>
#include <hetcompute/internal/memalloc/pipelinelinearpool.hh>
#include <hetcompute/internal/util/ffs.hh>
#include <hetcompute/internal/util/memorder.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace allocatorinternal
        {
            /// This allocator implements a concurrent slab allocator.
            /// The allocator has a set of pre-allocated fixed size linear pools.
            /// It manages (acquire and release) pools concurrently.
            /// To keep track of the alloated pools the slab allocator uses an concurrent lock-free
            /// bit map of 32 bits. So it can maintain only up to 32 slab pools.
            template <typename T>
            class pipeline_slab_allocator
            {
            public:
                /**
                 * Constructor
                 * @param pool_entries: the number of entries in each of the linear pools
                 */
                explicit pipeline_slab_allocator(size_t pool_entries)
                    : _id_assigner(), _max_pool_count(_id_assigner.get_max_ids()), _pool_entries(pool_entries), _pools(nullptr)
                {
                    // Allocate the memory needed for all pools
                    _pools = new pipeline_fixed_size_linear_pool<T>*[_max_pool_count];
                    // Initialize pools one by one
                    for (size_t i = 0; i < _max_pool_count; i++)
                    {
                        _pools[i] = new pipeline_fixed_size_linear_pool<T>(i, pool_entries);
                    }
                }

                /**
                 * Acquire a pool
                 * @return pointer to the pool if the pool is acquired successfully, otherwise nullptr
                 */
                pipeline_fixed_size_linear_pool<T>* acquire_pool()
                {
                    size_t id;
                    if (_id_assigner.acquire(id, _max_pool_count))
                    {
                        return _pools[id];
                    }
                    return nullptr;
                }

                size_t get_max_pool_count() { return _max_pool_count; }

                /**
                 * Destructor
                 */
                ~pipeline_slab_allocator()
                {
                    // Deallocates the buffer
                    delete[] _pools;
                }

                /**
                * Release a pool
                * @param pointer to the pool to be released
                */
                void release_pool(pipeline_fixed_size_linear_pool<T>* pool)
                {
                    HETCOMPUTE_INTERNAL_ASSERT(pool != nullptr, "trying to release a null pool.");
                    size_t id = pool->get_id();
                    if (id < _max_pool_count)
                    {
                        _id_assigner.release(pool->get_id());
                    }
                }

            private:
                /**
                 * Pool id assigner, maintains a maximum of 32 pools.
                 */
                pool_id_assigner<uint32_t> _id_assigner;

                /**
                 * Pools in the allocator
                 */
                const size_t _max_pool_count;

                /**
                 * number of entries for the pool
                 */
                size_t _pool_entries;

                /**
                 * List of preallocated pools
                 */
                pipeline_fixed_size_linear_pool<T>** _pools;

                // Give access to private members to release the pool
                friend pipeline_fixed_size_linear_pool<T>;

                HETCOMPUTE_DELETE_METHOD(pipeline_slab_allocator(pipeline_slab_allocator const&));
                HETCOMPUTE_DELETE_METHOD(pipeline_slab_allocator& operator=(pipeline_slab_allocator const&));
                HETCOMPUTE_DELETE_METHOD(pipeline_slab_allocator(pipeline_slab_allocator const&&));
                HETCOMPUTE_DELETE_METHOD(pipeline_slab_allocator& operator=(pipeline_slab_allocator const&&));
            };

        }; // namespace allocatorinternal

    }; // namespace internal

}; // namespace hetcompute
