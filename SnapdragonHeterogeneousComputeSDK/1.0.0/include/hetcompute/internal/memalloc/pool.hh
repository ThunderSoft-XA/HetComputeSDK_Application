#pragma once

#include <hetcompute/internal/util/macros.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace allocatorinternal
        {
            /**
             * Allocation pool base class
             *
             * Implements a basic interface for deallocate_object method.
             * Also methods for embedding/extracting a pointer to the owner pool in a
             * fixed offset from the allocated object.
             *
             * All the low level pools used by thread-local allocator and slab allocator
             * use this base class
             */
            class pool
            {
            public:
                pool()
                {
                }

                virtual ~pool()
                {
                }

                /**
                 * Deallocates object.
                 *
                 * The interface for deallocation which must be implemented by
                 * all the pools inherited from this class.
                */
                virtual bool deallocate_object(char* object) = 0;

                /**
                 * Deallocates object.
                 *
                 * @param object. Allocated object
                 * @param threadid. Id of the thread calling this method.
                */
                virtual bool deallocate_object(char* object, size_t threadid) = 0;

                /**
                 * Takes an allocated object and extracts its embedded pool
                 *
                 * @param object The allocated object
                 * @return Pool the object was allocated on.
                */
                static pool* get_embedded_pool(char* object)
                {
                    using pool_elem    = embedded_pool_element;
                    pool_elem* element = reinterpret_cast<pool_elem*>(object - get_embedded_pool_header_size());
                    return element->_pool;
                }

                /**
                 * Takes an allocated object (buffer) and a pool and embeds that pool
                 * in a fixed offset from the object.
                 * @param object: the allocated object
                 * @param pool: the pool to be embedded in the object
                 * @return the address in which the pool is embedded
                */
                static char* embed_pool(char* object, pool* p)
                {
                    using pool_elem    = embedded_pool_element;
                    pool_elem* element = reinterpret_cast<pool_elem*>(object);
                    element->_pool     = p;
                    return object + get_embedded_pool_header_size();
                }

                /**
                 * Returns size of embedded pool header.
                 *
                 * @return Embedded pool offset.
                */
                static constexpr size_t get_embedded_pool_header_size()
                {
                    return s_embedded_pool_offset;
                }

                /**
                 * Returns a textual description of the pool.
                 *
                 * @return std::string with a textual description of the pool.
                */
                virtual std::string to_string() = 0;

            protected:
                // Each object in the pool will have a header that stores
                // two pointers:
                //  - Pointer to the pool itself.
                //  - Pointer for allocator specific data or just padding.
                //
                // Some allocators may need to give objects certain padding to make them
                // fit in a particular layout (for example 64-bit atomics need extra
                // padding).
                static constexpr size_t s_embedded_pool_offset = sizeof(pool*) * 2;

                struct embedded_pool_element
                {
                    // Some allocators may need additional padding before embedding the pool
                    // in the allocation record
                    char _padding[s_embedded_pool_offset - sizeof(pool*)];
                    pool* _pool;
                };

                HETCOMPUTE_DELETE_METHOD(pool(pool const&));
                HETCOMPUTE_DELETE_METHOD(pool& operator=(pool const&));
                HETCOMPUTE_DELETE_METHOD(pool(pool const&&));
                HETCOMPUTE_DELETE_METHOD(pool& operator=(pool const&&));

            }; // class pool

        }; // namespace allocatorinternal
    };     // namespace internal
};         // namespace hetcompute
