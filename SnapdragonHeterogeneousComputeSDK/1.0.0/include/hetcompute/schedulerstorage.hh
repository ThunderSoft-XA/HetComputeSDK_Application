/** @file schedulerstorage.hh */
#pragma once

// Include user-visisble headers first
#include <hetcompute/scopedstorage.hh>

// Include internal headers
#include <hetcompute/internal/storage/schedulerstorage.hh>

namespace hetcompute
{
    /** @addtogroup scheduler_storage
    @{ */
    /**
        Scheduler-local storage allows sharing of information across tasks on a
        per-scheduler basis, like what thread-local storage does for threads.
        A `scheduler_storage_ptr\<T\>` stores a pointer-to-T (`T*`).  In
        contrast to `task_storage_ptr`, the contents are persistent
        across tasks.  In contrast to `thread_storage_ptr`, the contents
        are guaranteed to not be changed while a task is suspended. To
        maintain these guarantees, the runtime system is free to create
        new objects of type T whenever needed.

        @sa task_storage_ptr
        @sa thread_storage_ptr

        @par Example
        @includelineno samples/src/sched_storage01.cc
        @includelineno samples/src/sched_storage02.cc
        @includelineno samples/src/sched_storage03.cc
    */
    template <typename T, class Allocator = std::allocator<T>>
    class scheduler_storage_ptr : public scoped_storage_ptr<scheduler_storage_ptr, T, Allocator>
    {
    public:
        typedef Allocator allocator_type;
        scheduler_storage_ptr() {}
#if __cplusplus >= 201103L
    private:
        friend class scoped_storage_ptr<::hetcompute::scheduler_storage_ptr, T, Allocator>;
#else
    public:
#endif
        inline static int   key_create(internal::storage_key* key, void (*dtor)(void*)) { return sls_key_create(key, dtor); }
        inline static int   set_specific(internal::storage_key key, void const* value) { return sls_set_specific(key, value); }
        inline static void* get_specific(internal::storage_key key) { return sls_get_specific(key); }
    };
    /** @} */ /* end_addtogroup scheduler_storage */

}; // namespace hetcompute
