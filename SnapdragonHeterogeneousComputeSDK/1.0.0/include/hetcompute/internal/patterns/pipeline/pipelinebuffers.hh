/** @file pipelinebuffers.hh */
#pragma once

// Include standard headers
#include <stdexcept>

// Include internal headers
#include <hetcompute/internal/memalloc/pipelinelinearpool.hh>
#include <hetcompute/internal/memalloc/pipelineslaballocator.hh>
#include <hetcompute/internal/patterns/pipeline/pipelinepoolbucket.hh>

namespace hetcompute
{
    namespace internal
    {
        /**
         * @brief Pipeline launch types.
         *
         * Pipeline launch types.
         */
        typedef enum pipeline_launch_type { with_sliding_window = 0, without_sliding_window } pipeline_launch_type;
        // end of enum pipeline_launch_type

        /**
         * pipeline stage buffer base class
         */
        class stagebuffer
        {
        protected:
            size_t _size;

        public:
            /**
             * constructor
             */
            explicit stagebuffer(size_t size) : _size(size) {}

            /**
             * destructor
             */
            virtual ~stagebuffer() {}

            /**
             * Add a new pool to the buffer
             */
            virtual void allocate_pool() = 0;

            /**
             * get the size of the buffer
             * @return size_t
             */
            size_t get_size() const { return _size; }

            /**
             * reclaim the space of the first pool in the buffer
             */
            virtual void reset_pool() = 0;

            // Forbid copying and assignment
            HETCOMPUTE_DELETE_METHOD(stagebuffer(stagebuffer const& other));
            HETCOMPUTE_DELETE_METHOD(stagebuffer(stagebuffer&& other));
            HETCOMPUTE_DELETE_METHOD(stagebuffer& operator=(stagebuffer const& other));
            HETCOMPUTE_DELETE_METHOD(stagebuffer& operator=(stagebuffer&& other));
        };
        // end of class stagebuffer

        /**
         * pipeline ringbuffer
         */
        template <typename T>
        class ringbuffer : public stagebuffer
        {
            T* _buffer;

        public:
            /**
             * constructor
             */
            explicit ringbuffer(size_t s) : stagebuffer(s), _buffer(new T[s]) {}

            /**
             * destructor
             */
            virtual ~ringbuffer()
            {
                delete[] _buffer;
                _buffer = nullptr;
            }

            /**
             * Add a new pool to the buffer
             */
            virtual void allocate_pool() { HETCOMPUTE_UNREACHABLE("Cannot allocate pool for ringbuffer."); }

            /**
             * operator[] overload to get the pointer to the ith element from the buffer
             * @param id element index
             * @return T& the reference to the ith element
             */
            T& operator[](size_t id)
            {
                HETCOMPUTE_INTERNAL_ASSERT(id < _size, "Out of range buffer access.");
                return _buffer[id];
            };

            /**
             * get buffer index for a certain iteration
             * @param iter_id
             * @return size_t
             */
            size_t get_buffer_index(size_t iter_id) const { return iter_id % _size; }

            /**
             * get the size of the buffer
             * @return size_t
             */
            size_t get_size() const { return _size; }

            /**
             * Set the (id)th element out of the buffer
             * @param id
             * @param T value
             * @return T
             */
            void set(size_t id, T value)
            {
                HETCOMPUTE_INTERNAL_ASSERT(id < _size, "Out of range buffer access.");
                _buffer[id] = value;
            }

            /**
             * reclaim the space of the first pool in the buffer
             */
            virtual void reset_pool() { HETCOMPUTE_UNREACHABLE("Cannot allocate pool for ringbuffer."); }

            // Forbid copying and assignment
            HETCOMPUTE_DELETE_METHOD(ringbuffer(ringbuffer const& other));
            HETCOMPUTE_DELETE_METHOD(ringbuffer(ringbuffer&& other));
            HETCOMPUTE_DELETE_METHOD(ringbuffer& operator=(ringbuffer const& other));
            HETCOMPUTE_DELETE_METHOD(ringbuffer& operator=(ringbuffer&& other));
        };
        // end of class ringbuffer: public stagebuffer

        template <>
        class ringbuffer<void> : public stagebuffer
        {
        public:
            /**
             * constructor
             */
            explicit ringbuffer(size_t s) : stagebuffer(s)
            {
                HETCOMPUTE_INTERNAL_ASSERT(false, "Cannot construct a stage buffer with data of type void");
            }

            /**
             * destructor
             */
            virtual ~ringbuffer() {}

            // Forbid copying and assignment
            HETCOMPUTE_DELETE_METHOD(ringbuffer(ringbuffer const& other));
            HETCOMPUTE_DELETE_METHOD(ringbuffer(ringbuffer&& other));
            HETCOMPUTE_DELETE_METHOD(ringbuffer& operator=(ringbuffer const& other));
            HETCOMPUTE_DELETE_METHOD(ringbuffer& operator=(ringbuffer&& other));
        };
        // end of class ringbuffer: public stagebuffer

        /**
         * pipeline dynamic buffer
         */
        template <typename T>
        class dynamicbuffer : public stagebuffer
        {
            static hetcompute::mem_order const pool_mem_order = hetcompute::mem_order_relaxed;

        public:
            using size_type       = size_t;
            using pipe_slab_alloc = allocatorinternal::pipeline_slab_allocator<T>;
            using pipe_fsl_pool   = allocatorinternal::pipeline_fixed_size_linear_pool<T>;

        private:
            static HETCOMPUTE_CONSTEXPR_CONST size_type s_bucket_size = 64;
#ifdef HETCOMPUTE_PIPELINE_DEBUG_BUFFER
            size_t _num_alloc;
            size_t _num_dalloc;
            size_t _num_reset;
#endif

            std::atomic<size_t>                                               _first_pool;
            std::atomic<size_t>                                               _num_pools;
            pipeline_pool_buckets<std::array<pipe_fsl_pool*, s_bucket_size>*> _pool_buckets;
            size_t                                                            _pool_entries;
            pipe_slab_alloc                                                   _slab;

        public:
            /**
             *constructor
             */
            explicit dynamicbuffer(size_t pool_entries)
                :
#ifdef HETCOMPUTE_PIPELINE_DEBUG_BUFFER
                  _num_alloc(0),
                  _num_dalloc(0),
                  _num_reset(0),
#endif
                  stagebuffer(pool_entries),
                  _first_pool(0),
                  _num_pools(0),
                  _pool_buckets(),
                  _pool_entries(pool_entries),
                  _slab(pool_entries)
            {
            }

            /**
             *destructor
             */
            virtual ~dynamicbuffer()
            {
            // clean the buckets
#ifdef HETCOMPUTE_PIPELINE_DEBUG_BUFFER
                HETCOMPUTE_ILOG("DYNAMIC BUFFER %p, %zu out of %zu alloc from the slab, %zu resets",
                              this,
                              _num_alloc - _num_dalloc,
                              _num_alloc,
                              _num_reset);
#endif

                for (size_t i = 0; i < _pool_buckets.size(); i++)
                {
                    auto bucket = _pool_buckets[i];
                    if (bucket == nullptr)
                        continue;

                    for (auto const& pool : *bucket)
                    {
                        // if the pool is not from the slab allocator, i.e. dynamically allocated
                        // delete it here.
                        if (pool == nullptr)
                            continue;
                        if (pool->get_id() == _slab.get_max_pool_count() + 1)
                            delete pool;
                    }
                    delete bucket;
                }
            }

            /**
             * Add a new pool to the buffer
             */
            virtual void allocate_pool()
            {
                // try to allocate from the slab allocator
                pipe_fsl_pool* p = _slab.acquire_pool();

#ifdef HETCOMPUTE_PIPELINE_DEBUG_BUFFER
                _num_alloc++;
#endif

                // if fails, new a pool
                if (p == nullptr)
                {
#ifdef HETCOMPUTE_PIPELINE_DEBUG_BUFFER
                    _num_dalloc++;
#endif
                    p = new pipe_fsl_pool(_slab.get_max_pool_count()+1, _pool_entries);
                }

                size_t elem_id = _num_pools.load(pool_mem_order) % s_bucket_size;

                // allocate another bucket if needed
                if (elem_id == 0)
                {
                    auto new_bucket = new std::array<pipe_fsl_pool*, s_bucket_size>();
                    new_bucket->fill(nullptr);
                    _pool_buckets.push_back(new_bucket);
                }

                (*_pool_buckets.back())[elem_id] = p;
                _num_pools.fetch_add(1, pool_mem_order);

#if defined(HETCOMPUTE_PIPELINE_DEBUG_BUFFER) && defined(DEBUG_PIPELINE)
                HETCOMPUTE_ILOG(
                    "\033[31m allocating pool %p(%zu) in %p, first_pool %zu, num_pools %zu, pool_entries %zu, elem_id %zu\033[0m\n",
                    p,
                    p->get_id(),
                    this,
                    _first_pool.load(pool_mem_order),
                    _num_pools.load(pool_mem_order),
                    _pool_entries,
                    elem_id);
#endif
            }

            /**
             * get buffer index for a certain iteration
             * @param iter_id
             * @return size_t
             */
            size_t get_buffer_index(size_t iter_id) const
            {
                // for dynamic buffer, use the iter_id as what it is, no need to module by the size
                // since the buffer keeps growing
                return iter_id;
            }

            /**
             * get the number of pool entries
             * @return size_t
             */
            size_t get_size() const { return _pool_entries; }

            /**
             * reclaim the space of the first pool in the buffer
             */
            virtual void reset_pool()
            {
#ifdef HETCOMPUTE_PIPELINE_DEBUG_BUFFER
                _num_reset++;
#endif
                size_t elem_id   = _first_pool.load(pool_mem_order) % s_bucket_size;
                size_t bucket_id = _first_pool.load(pool_mem_order) / s_bucket_size;

                HETCOMPUTE_INTERNAL_ASSERT(_pool_buckets[bucket_id] != nullptr, "pool buckets should not be empty.");

                _slab.release_pool((*_pool_buckets[bucket_id])[elem_id]);
                (*_pool_buckets[bucket_id])[elem_id] = nullptr;

                // release the memory of the bucket
                if (elem_id == s_bucket_size - 1)
                {
                    delete _pool_buckets[bucket_id];
                    _pool_buckets[bucket_id] = nullptr;
                }
                _first_pool.fetch_add(1, pool_mem_order);
            }

            /**
             * operator[] overload to get the pointer to the ith element from the buffer
             * @param id element index
             * @return T& the reference to the ith element
             */
            T& operator[](size_t id)
            {
                HETCOMPUTE_API_THROW_CUSTOM(id < _num_pools.load(pool_mem_order) * _pool_entries &&
                                              id >= _first_pool.load(pool_mem_order) * _pool_entries,
                                          std::out_of_range,
                                          "Out of range buffer access.");

                size_t pool_id   = id / _pool_entries;
                size_t bucket_id = pool_id / s_bucket_size;
                pool_id          = pool_id % s_bucket_size;
                size_t elem_id   = id % _pool_entries;

                return (*(*_pool_buckets[bucket_id])[pool_id])[elem_id];
            };

            // Forbid copying and assignment
            HETCOMPUTE_DELETE_METHOD(dynamicbuffer(dynamicbuffer const& other));
            HETCOMPUTE_DELETE_METHOD(dynamicbuffer(dynamicbuffer&& other));
            HETCOMPUTE_DELETE_METHOD(dynamicbuffer& operator=(dynamicbuffer const& other));
            HETCOMPUTE_DELETE_METHOD(dynamicbuffer& operator=(dynamicbuffer&& other));
        };
        // end of class dynamicbuffer:public stagebuffer

        template <>
        class dynamicbuffer<void> : public stagebuffer
        {
        public:
            /**
             * constructor
             */
            explicit dynamicbuffer(size_t pool_entries) : stagebuffer(pool_entries)
            {
                HETCOMPUTE_INTERNAL_ASSERT(false, "Cannot construct a dynamic buffer with data of type void.");
            }

            /**
             * destructor
             */
            virtual ~dynamicbuffer() {}

            // Forbid copying and assignment
            HETCOMPUTE_DELETE_METHOD(dynamicbuffer(dynamicbuffer const& other));
            HETCOMPUTE_DELETE_METHOD(dynamicbuffer(dynamicbuffer&& other));
            HETCOMPUTE_DELETE_METHOD(dynamicbuffer& operator=(dynamicbuffer const& other));
            HETCOMPUTE_DELETE_METHOD(dynamicbuffer& operator=(dynamicbuffer&& other));
        };
        // end of class dynamicbuffer:public stagebuffer

    }; // namespace internal
};     // namespace hetcompute
