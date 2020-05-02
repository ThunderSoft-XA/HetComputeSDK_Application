#pragma once

#if !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)

#include <hetcompute/internal/memalloc/threadlocalallocator.hh>

/// This namespace implements the interface needed for initializing and
/// calling into thread-local allocator from the hetcompute task system.

namespace hetcompute
{
    namespace internal
    {
        // Forward declaration
        class task;

        namespace task_allocator
        {
            /// Initializes the thread-local allocator
            /// @param threads: the initial number of the device threads
            void init(size_t threads);

            /// Shutdown the thread-local allocator
            void shutdown();

            /// Allocate an object using thread-local allocator
            /// @param object_size: the size of the object
            /// @return the allocator object or nullptr on failure
            char* allocate(size_t object_size);

            /// Allocate an object using the default allocator
            /// @param object_size: the size of the object
            /// @return the allocator object or nullptr on failure
            char* allocate_default(size_t object_size);

            /// Deallocates an object back into the thread-local allocator
            /// @param object: pointer to the object
            /// @return true on success
            void deallocate(task* object);

        }; // namespace hetcompute::internal::task_allocator
    };     // namespace hetcompute::internal
};         // namespace hetcompute

#endif // !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)
