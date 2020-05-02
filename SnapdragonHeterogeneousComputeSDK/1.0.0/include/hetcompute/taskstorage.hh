/** @file taskstorage.hh */
#pragma once

// Include user facing headers
#include <hetcompute/exceptions.hh>

// Include internal headers
#include <hetcompute/internal/storage/taskstorage.hh>

namespace hetcompute
{
    /** @addtogroup task_storage
    @{ */
    /**
        Task-local storage enables allocation of task-specific data, like what
        thread-local storage does for threads.  The value of a `task_storage_ptr`
        is local to a task.  A `task_storage_ptr\<T\>` stores a
        pointer-to-T (`T*`).

        @sa thread_storage_ptr

        @par Example
        @includelineno samples/src/task_storage01.cc
    */
    template <typename T>
    class task_storage_ptr
    {
    public:
        typedef T const* pointer_type;

        HETCOMPUTE_DELETE_METHOD(task_storage_ptr(task_storage_ptr const&));
        HETCOMPUTE_DELETE_METHOD(task_storage_ptr& operator=(task_storage_ptr const&));
#ifndef _MSC_VER
        // Valid use of volatile, since we delete the method!
        // %mint: pause
        HETCOMPUTE_DELETE_METHOD(task_storage_ptr& operator=(task_storage_ptr const&) volatile);
        // %mint: resume
#endif

        /** @throws hetcompute::tls_exception If `task_storage_ptr` could not be reserved. 
         *  @note If exceptions are disabled in application, logs error if `task_storage_ptr` could not be reserved.
        */

        task_storage_ptr() : _key() { init_key(); }

        /**
            @throws hetcompute::tls_exception If `task_storage_ptr` could not be reserved.
            @param ptr Initial value of task-local storage.
        */
        explicit task_storage_ptr(T* const& ptr) : _key()
        {
            init_key();
            *this = ptr;
        }

        /**
            @throws hetcompute::tls_exception if `task_storage_ptr` could not be reserved.

            @param ptr Initial value of task-local storage.
            @param dtor Destructor function.
        */
        task_storage_ptr(T* const& ptr, void (*dtor)(T*)) : _key()
        {
            init_key(dtor);
            *this = ptr;
        }

        /** @returns Pointer to stored pointer value. */
        T* get() const { return static_cast<T*>(task_get_specific(_key)); }

        /** @returns Pointer to stored pointer value. */
        T* operator->() const { return get(); }

        /** @returns Reference to value pointed to by stored pointer.

            @note1 No checking for `nullptr` is performed.
        */
        T& operator*() const { return *get(); }

        /**
            Assignment operator, stores T*.

            @param ptr Pointer value to store.
        */
        task_storage_ptr& operator=(T* const& ptr)
        {
            task_set_specific(_key, ptr);
            return *this;
        }

        /** @returns True if stored pointer is `nullptr`. */
        bool operator!() const
        {
            pointer_type t = get();
            return t == nullptr;
        }

        bool operator==(T* const& other) const { return get() == other; }

        bool operator!=(T* const& other) const { return get() != other; }

        /** Casting operator to `T*` pointer type. */
        /* implicit */ operator pointer_type() const { return get(); }

        /** Casting operator to bool. */
        explicit operator bool() const { return get() != nullptr; }

    private:
        void init_key(void (*dtor)(T*) = nullptr)
        {
            int err = task_key_create(&_key, reinterpret_cast<void (*)(void*)>(dtor));
            if (err != 0)
            {
#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                throw hetcompute::tls_exception("Unable to allocate task-local storage key.", __FILE__, __LINE__, __FUNCTION__);
#else
                HETCOMPUTE_ELOG("Unable to allocate task-local storage key");
#endif
            }
        }

    private:
        internal::storage_key _key; /** Stores task-local storage key. */
    };

    /** @} */ /* end_addtogroup task_storage */

}; // namespace hetcompute
