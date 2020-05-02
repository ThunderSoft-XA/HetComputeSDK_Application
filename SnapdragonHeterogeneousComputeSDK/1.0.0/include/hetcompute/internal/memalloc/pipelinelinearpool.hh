#pragma once

#include <hetcompute/internal/util/debug.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace allocatorinternal
        {
            /**
             * This class is used by the pipeline_slab_allocator to manage pipeline dynamic buffer.
             */
            template <typename T>
            class pipeline_fixed_size_linear_pool
            {
            public:
                /**
                 * Constructor
                 */
                pipeline_fixed_size_linear_pool(size_t id, size_t pool_entries) : _id(id), _pool_entries(pool_entries)
                {
                    _buffer = new T[_pool_entries];
                    if (_buffer == nullptr)
                    {
#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                        throw std::bad_alloc();
#else
                        HETCOMPUTE_ELOG("HetCompute Memory allocation failed in pipeline_fixed_size_linear_pool");
#endif
                    }
                }

                /**
                 * Destructor
                 */
                ~pipeline_fixed_size_linear_pool()
                {
                    if (_buffer != nullptr)
                    {
                        delete[] _buffer;
                    }
                }

                /**
                 * Returns a copy of the object value associated with a particular index
                 * @param entry_index: object index
                 * @return the object value
                 */
                T get_object(size_t entry_index) const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(entry_index < _pool_entries,
                                               "out of range access to the pool. Index: %zu Range: %zu.",
                                               entry_index,
                                               _pool_entries);

                    return _buffer[entry_index];
                }

                /**
                 * Get the object
                 * @param entry_index: object index
                 * @return the object reference
                 */
                T& operator[](size_t entry_index) const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(entry_index < _pool_entries,
                                               "out of range access to the pool. Index: %zu Range: %zu.",
                                               entry_index,
                                               _pool_entries);

                    return _buffer[entry_index];
                }

                /**
                 * get the id of the pool
                 */
                size_t get_id() const { return _id; }

                /**
                 * Assigns the object at a particular index
                 * @param input: the object to assign
                 * @param entry_index: object index
                 */
                void set_object(T input, size_t entry_index)
                {
                    HETCOMPUTE_INTERNAL_ASSERT(entry_index < _pool_entries,
                                               "out of range access to the pool. Index: %zu Range: %zu.",
                                               entry_index,
                                               _pool_entries);

                    _buffer[entry_index] = input;
                }

                HETCOMPUTE_DELETE_METHOD(pipeline_fixed_size_linear_pool(pipeline_fixed_size_linear_pool const&));
                HETCOMPUTE_DELETE_METHOD(pipeline_fixed_size_linear_pool& operator=(pipeline_fixed_size_linear_pool const&));
                HETCOMPUTE_DELETE_METHOD(pipeline_fixed_size_linear_pool(pipeline_fixed_size_linear_pool const&&));
                HETCOMPUTE_DELETE_METHOD(pipeline_fixed_size_linear_pool& operator=(pipeline_fixed_size_linear_pool const&&));

            private:
                // The internal buffer
                T* _buffer;

                /// Identifier to be used with the allocator
                size_t _id;

                /// Number of objects in the pool
                size_t _pool_entries;
            };

        }; // namespace allocatorinternal

    }; // namespace internal

}; // namespace hetcompute
