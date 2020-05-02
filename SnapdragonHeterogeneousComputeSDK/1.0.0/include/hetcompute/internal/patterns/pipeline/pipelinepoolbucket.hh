/** @file pipelinepoolbucket.hh */
#pragma once

namespace hetcompute
{
    namespace internal
    {
        template <typename T>
        class pipeline_pool_buckets;

        /**
         * class for pool bucket nodes
         */
        template <typename T>
        class pipeline_chunk_list_node
        {
            T                            _data;
            pipeline_chunk_list_node<T>* _next;

        public:
            explicit pipeline_chunk_list_node(T data) : _data(data), _next(nullptr) {}

            // Forbid copying and assignment
            HETCOMPUTE_DELETE_METHOD(pipeline_chunk_list_node(pipeline_chunk_list_node const& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_chunk_list_node(pipeline_chunk_list_node&& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_chunk_list_node& operator=(pipeline_chunk_list_node const& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_chunk_list_node& operator=(pipeline_chunk_list_node&& other));

            // make friends
            template <typename C>
            friend class pipeline_pool_buckets;
        };

        /**
         * class for pipeline pool buckets
         * This is a dynamic growing linked list, each list node has a big array
         * of pool buckets for fast random access
         * Pool buckets are only allocated sequentially (may happen from different threads)
         * Pool bucktes are reset (deallocated) sequentially (may happen from different threads)
         * Alloction always happen before deallocation for a specific pool bucket array
         * Allocation and deallocations for different pool bucket arrays may happen in parallel
         * Read always happen after allocation and before deallocation
         * Write always happen after allocation and before deallocation
         * Read and write can happen in parallel
         * Random access to the pool buckets needs to be very efficient
         */
        template <typename T>
        class pipeline_pool_buckets
        {
            static HETCOMPUTE_CONSTEXPR_CONST size_t s_chunk_size      = 1024;
            static HETCOMPUTE_CONSTEXPR_CONST size_t s_chunk_size_mask = s_chunk_size - 1;
            static HETCOMPUTE_CONSTEXPR_CONST size_t s_chunk_size_bits = 10;

            pipeline_chunk_list_node<std::array<T, s_chunk_size>*>* _first_chunk;
            pipeline_chunk_list_node<std::array<T, s_chunk_size>*>* _last_chunk;
            size_t                                                  _num_elems;

        public:
            /**
             * constructor
             */
            pipeline_pool_buckets() : _first_chunk(nullptr), _last_chunk(nullptr), _num_elems(0) {}

            /**
             * destructor
             */
            ~pipeline_pool_buckets()
            {
                auto p = _first_chunk;
                while (p)
                {
                    auto n = p->_next;
                    delete p->_data;
                    delete p;
                    p = n;
                }
            }

            /**
             * push an elem to the back
             */
            void push_back(T elem)
            {
                auto elem_id = _num_elems & s_chunk_size_mask;

                if (elem_id == 0)
                {
                    auto curr_chunk = new pipeline_chunk_list_node<std::array<T, s_chunk_size>*>(new std::array<T, s_chunk_size>());

                    if (_first_chunk == nullptr)
                    {
                        _first_chunk = curr_chunk;
                    }
                    if (_last_chunk != nullptr)
                    {
                        _last_chunk->_next = curr_chunk;
                    }
                    _last_chunk = curr_chunk;
                }
                (*_last_chunk->_data)[elem_id] = elem;
                _num_elems++;
            }

            /**
             * [] operator to get one element
             * @param id
             * @return reference to the element
             */
            T& operator[](size_t id)
            {
                HETCOMPUTE_INTERNAL_ASSERT(id <= _num_elems, "out of range bucket access");

                auto elem_id  = id & s_chunk_size_mask;
                auto chunk_id = id >> s_chunk_size_bits;

                auto curr_chunk = _first_chunk;
                for (size_t i = 0; i < chunk_id; i++)
                {
                    HETCOMPUTE_INTERNAL_ASSERT(curr_chunk != nullptr, "pool buckets chunk is broken");
                    curr_chunk = curr_chunk->_next;
                }

                return (*curr_chunk->_data)[elem_id];
            }

            /**
             * get the last element
             * @return reference to the last element
             */
            T& back()
            {
                HETCOMPUTE_INTERNAL_ASSERT(_last_chunk != nullptr, "pool is empty");
                auto elem_id = (_num_elems - 1) & s_chunk_size_mask;
                return (*_last_chunk->_data)[elem_id];
            }

            /**
             * get the size of the pool bucket
             * @return size_t
             */
            size_t size() { return _num_elems; }

            // Forbid copying and assignment
            HETCOMPUTE_DELETE_METHOD(pipeline_pool_buckets(pipeline_pool_buckets const& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_pool_buckets(pipeline_pool_buckets&& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_pool_buckets& operator=(pipeline_pool_buckets const& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_pool_buckets& operator=(pipeline_pool_buckets&& other));
        };

    }; // namespace internal
};     // namespace hetcompute
