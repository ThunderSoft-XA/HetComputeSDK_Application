#pragma once

#include <atomic>
#include <string>
#include <hetcompute/internal/memalloc/pool.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace allocatorinternal
        {
            /**
             * This allocator is used for allocating oversized objects. It simply uses new
             * to create the object and then wraps the object in a pool header that includes
             * the allocation data (check allocate function).
             * The goal is to make deallocation API of all objects independent of the allocator.
            */
            class default_allocator : public pool
            {
            public:
                /**
                 *  Default constructor, does nothing
                 */
                default_allocator()
                {
                }

                /**
                 * Creates and returns an object of a particular size.
                 *
                 * The allocated object is wrapped in a buffer which has a header
                 * including the allocating pool (this)
                 *
                 * @param size: the size of the object
                 * @return the allocated object
                */
                char* allocate_object(size_t size)
                {
                    // Create the wrapper buffer
                    char* wrapper = new char[size + pool::s_embedded_pool_offset];
                    // embed the pool (this) into the buffer
                    char* object = embed_pool(wrapper, this);
                    // return the object
                    return object;
                }

                /**
                 * Deallocates the object allocated using the allocate_object method.
                 *
                 * @param object: the object to be deallocated.
                */
                virtual bool deallocate_object(char* object)
                {
                    return deallocate_object_impl(object);
                }

                /**
                 * Deallocates the object allocated using the allocate_object method.
                 *
                 * @param object: the object to be deallocated.
                 * @param threadid Calling's thread thread id. Not used.
                */
                virtual bool deallocate_object(char* object, size_t threadid)
                {
                    HETCOMPUTE_UNUSED(threadid);
                    return deallocate_object_impl(object);
                }

                /**
                 * Returns a textual description of the pool.
                 *
                 * @return std::string with a textual description of the pool.
                */
                virtual std::string to_string()
                {
                    return "DEFAULT";
                }

            private:
                bool deallocate_object_impl(char* object)
                {
                    // Extract the wrapper
                    char* wrapper = object - pool::s_embedded_pool_offset;
                    // Delete the wrapper
                    delete[] wrapper;
                    return true;
                }

                HETCOMPUTE_DELETE_METHOD(default_allocator(default_allocator const&));
                HETCOMPUTE_DELETE_METHOD(default_allocator& operator=(default_allocator const&));
                HETCOMPUTE_DELETE_METHOD(default_allocator(default_allocator const&&));
                HETCOMPUTE_DELETE_METHOD(default_allocator& operator=(default_allocator const&&));
            };  // class default_allocator

        };  // namespace allocatorinternal
    };  // namespace internal
};  // namespace hetcompute
