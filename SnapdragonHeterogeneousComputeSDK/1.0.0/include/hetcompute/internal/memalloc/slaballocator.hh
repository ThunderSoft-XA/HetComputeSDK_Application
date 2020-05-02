#pragma once

#include <atomic>
#include <string>

#include <hetcompute/internal/memalloc/concurrentbumppool.hh>
#include <hetcompute/internal/memalloc/poolidassigner.hh>
#include <hetcompute/internal/util/ffs.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace allocatorinternal
        {
            /// This allocator implements a concurrent slab allocator
            /// The allocator has a set of pre-allocated slab pools (refer to slab pool documents)
            /// The slab allocator manages (allocates and deallocates) slab pools concurrently.
            /// To keep track of the alloated slab pools the slab allocator uses an concurrent lock-free
            /// bit map of 32 bits. So it can maintain only up to 32 slab pools.
            class slab_allocator
            {
            public:
                /// Constructor
                /// @param pool_size_in_bytes: the size of each bump pool in bytes
                /// @param pool_count: the number of bump pools managed by the allocator
                explicit slab_allocator(size_t pool_size_in_bytes = 1024, size_t pool_count = 10)
                    : _pools(nullptr),
                      _id_assigner(),
                      _buffer(nullptr),
                      _pool_size_in_bytes(pool_size_in_bytes),
                      _pool_count(pool_count),
                      _buffer_size(_pool_size_in_bytes * _pool_count)
                {
                    HETCOMPUTE_INTERNAL_ASSERT(pool_count <= _id_assigner.get_max_ids(), "Allocating too many bump pools");
                    HETCOMPUTE_INTERNAL_ASSERT(pool_count > 0, "Need at least one pool in the slab");
                    HETCOMPUTE_INTERNAL_ASSERT(pool_size_in_bytes > 0, "Need at least one byte for each pool in the slab");

                    // Acquire the memory needed for all pools
                    _buffer = new char[_buffer_size];
                    _pools  = new concurrent_bump_pool*[_pool_count];
                    // Initialize pools one by one
                    for (size_t i = 0; i < _pool_count; i++)
                    {
                        _pools[i] = new concurrent_bump_pool(i, _pool_size_in_bytes, _buffer + i * _pool_size_in_bytes, this);
                    }
                }

                /// Acquire a pool
                /// @param ref_count: initial pool ref count
                /// @return pointer to the pool if the pool is acquired successfully
                concurrent_bump_pool* acquire_pool(size_t ref_count = 0)
                {
                    size_t id;
                    if (_id_assigner.acquire(id, _pool_count))
                    {
                        _pools[id]->acquire(ref_count);
                        return _pools[id];
                    }
                    return nullptr;
                }

                /// Destructor
                ~slab_allocator()
                {
                    // Deallocated the buffer
                    delete[] _buffer;
                    delete[] _pools;
                }
                HETCOMPUTE_DELETE_METHOD(slab_allocator(slab_allocator const&));
                HETCOMPUTE_DELETE_METHOD(slab_allocator& operator=(slab_allocator const&));
                HETCOMPUTE_DELETE_METHOD(slab_allocator(slab_allocator const&&));
                HETCOMPUTE_DELETE_METHOD(slab_allocator& operator=(slab_allocator const&&));

                /// Returns a string representation of the allocator
                std::string to_string()
                {
                    std::string str = "Slab allocator info: ";
                    str += strprintf("buffer %p %d\n", _buffer, int(_id_assigner.get_ids()));
                    for (size_t i = 0; i < _pool_count; i++)
                    {
                        str += strprintf("Pool[%zu]: %s\n", i, _pools[i]->to_string().c_str());
                    }
                    return str;
                }

            private:
                /// Takes an object and returns its corresponding pool in the poolset
                /// @param object: the object to find the pool for
                /// @param pool_id: the found pool_id associated with the object
                /// @return false if the object is not in any of the pools
                bool find_object_pool_id(char* object, size_t& pool_id)
                {
                    if ((object < _buffer) || (object >= _buffer + _buffer_size))
                    {
                        return false;
                    }
                    pool_id = (object - _buffer) / _pool_size_in_bytes;
                    return true;
                }

                void release_pool(concurrent_bump_pool* pool) { _id_assigner.release(pool->get_id()); }

                /// Preallocated pools
                concurrent_bump_pool** _pools;

                /// Pool id assigner, maintains a maximum of 32 pools.
                pool_id_assigner<uint32_t> _id_assigner;

                /// The buffer used for initializing all bump pools
                char* _buffer;

                /// Each pool size
                const size_t _pool_size_in_bytes;

                /// Pools in the allocator
                const size_t _pool_count;

                /// Size of the buffer for all pools
                const size_t _buffer_size;

                friend concurrent_bump_pool;
            };

        }; // namespace allocatorinternal

    }; // namespace internal

}; // namespace hetcompute
