/** @file scopedstorage.hh */
#pragma once

#include <memory>

#include <hetcompute/exceptions.hh>
#include <hetcompute/internal/storage/storagemap.hh>

namespace hetcompute
{
    /** @addtogroup scoped_storage
    @{ */
    /**
        Scoped storage allows sharing of information on a per-task basis,
        similar as thread-local storage does for threads.  A
        `scoped_storage_ptr<Scope,T,Allocator>` stores a pointer-to-T (`T*`)
        for a given scope `Scope<T,A>`, defining life time, persistence, etc..
        `Allocator` controls the allocation of T objects.
    */
    template <template <class, class> class Scope, typename T, class Allocator>
    class scoped_storage_ptr
    {
    public:
        typedef Scope<T, Allocator> scope_type;
        typedef T const*            pointer_type;

        HETCOMPUTE_DELETE_METHOD(scoped_storage_ptr(scoped_storage_ptr const&));
        HETCOMPUTE_DELETE_METHOD(scoped_storage_ptr& operator=(scoped_storage_ptr const&));

#ifndef _MSC_VER
        // Valid use of volatile, since we delete the method!
        // %mint: pause
        HETCOMPUTE_DELETE_METHOD(scoped_storage_ptr& operator=(scoped_storage_ptr const&) volatile);
        // %mint: resume
#endif

        // No assignment because of unclear ownership
        HETCOMPUTE_DELETE_METHOD(scoped_storage_ptr& operator=(T* const&));

        /**
            @throws hetcompute::tls_exception if `scoped_storage_ptr` could not be reserved
            @note   If exceptions are disabled, logs error if key create is unsuccessful.
         */
        scoped_storage_ptr() : _key() { init_key(); }
        virtual ~scoped_storage_ptr() {}

        /**
            @returns Stored pointer value; a new object of type `T` is
            created and stored, if it has not been stored before.
        */
        T* get() const
        {
            auto pvalue = static_cast<T*>(scope_type::get_specific(_key));
            if (!pvalue)
            {
                pvalue = _alloc.allocate(1); // owned by Scope
                _alloc.construct(pvalue);
                scope_type::set_specific(_key, pvalue);
            }
            return pvalue;
        }

        /**
            @returns Stored pointer value; a new object of type `T` is
            created and stored, if it has not been stored before.
        */
        T* operator->() const { return get(); }

        /**
            @returns Reference to value pointed to by stored pointer; A new
            object of type `T` is created and stored, if it has not been
            stored before.
        */
        T& operator*() const { return *get(); }

        /**
            @returns Constantly false.
        */
        bool operator!() const
        {
#ifdef HETCOMPUTE_CHECK_INTERNAL
            pointer_type t = get();
            HETCOMPUTE_INTERNAL_ASSERT(t != nullptr, "invalid value for scoped storage %p", this);
            return t == nullptr;
#else
            return false;
#endif
        }

        bool operator==(T* const& other) { return get() == other; }

        bool operator!=(T* const& other) { return get() != other; }

        /** Casting operator to `T*` pointer type. */
        /* implicit */ operator pointer_type() const { return get(); }

        /** Casting operator to bool (constantly true). */
        explicit operator bool() const
        {
#ifdef HETCOMPUTE_CHECK_INTERNAL
            return get() != nullptr;
#else
            return true;
#endif
        }

    private:
        static void maybe_invoke_dtor(void* ptr)
        {
            if (!ptr)
                return;
            auto obj = static_cast<T*>(ptr);
            _alloc.destroy(obj);
            _alloc.deallocate(obj, 1);
        }

        void init_key()
        {
            int err = scope_type::key_create(&_key, &maybe_invoke_dtor);
            if (err != 0)
            {
#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                throw hetcompute::tls_exception("Unable to allocate storage key.", __FILE__, __LINE__, __FUNCTION__);
#else
                HETCOMPUTE_ELOG("Unable to allocate storage key");
#endif
            }
        }

    private:
        internal::storage_key _key; // stores scheduler-local storage key
        static Allocator      _alloc;
    };

    template <template <class, class> class S, typename T, typename A>
    A scoped_storage_ptr<S, T, A>::_alloc;

    /** @} */ /* end_addtogroup scoped_storage */

}; // namespace hetcompute
