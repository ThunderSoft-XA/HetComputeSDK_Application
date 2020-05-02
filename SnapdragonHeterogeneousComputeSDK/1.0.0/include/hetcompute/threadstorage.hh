/** @file threadstorage.hh */
#pragma once

// Include user-visible headers first
#include <hetcompute/scopedstorage.hh>

// Include internal headers
#include <hetcompute/internal/storage/threadstorage.hh>

namespace hetcompute
{
    /** @addtogroup thread_storage
    @{ */
    /**
     Thread-local storage allows sharing of information across tasks on a
     per-thread basis. A `thread_storage_ptr\<T\>` stores a pointer-to-T (`T*`). In
     contrast to `task_storage_ptr`, the contents are persistent
     across tasks. In contrast to `scheduler_storage_ptr`, the
     thread-local storage may be accessed by other tasks while a task is suspended.

     @sa task_storage_ptr
     @sa scheduler_storage_ptr

     @par Example
     @includelineno samples/src/thread_storage01.cc
    */
    template <typename T, class Allocator = std::allocator<T>>
    class thread_storage_ptr : public scoped_storage_ptr<thread_storage_ptr, T, Allocator>
    {
    public:
        typedef Allocator allocator_type;
        thread_storage_ptr() {}
#if __cplusplus >= 201103L
    private:
        friend class scoped_storage_ptr<::hetcompute::thread_storage_ptr, T, Allocator>;
#else
    public:
#endif
        inline static int   key_create(internal::storage_key* key, void (*dtor)(void*)) { return tls_key_create(key, dtor); }
        inline static int   set_specific(internal::storage_key key, void const* value) { return tls_set_specific(key, value); }
        inline static void* get_specific(internal::storage_key key) { return tls_get_specific(key); }
    };

    /** @} */ /* end_addtogroup thread_storage */

}; // namespace hetcompute
