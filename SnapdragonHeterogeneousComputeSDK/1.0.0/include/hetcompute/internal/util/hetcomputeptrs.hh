#pragma once

#include <atomic>
#include <memory>
#include <utility>

// TODO: Uncomment when Log is migrated
// #include <hetcompute/internal/log/log.hh>
#include <hetcompute/internal/util/debug.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace testing
        {
            class HetComputePtrSuite;
        }; // namespace testing

        template <typename Object>
        class hetcompute_shared_ptr;

        template <typename Object>
        void explicit_unref(Object* obj);

        template <typename Object>
        void explicit_ref(Object* obj);

        typedef uint32_t ref_counter_type;

        namespace hetcomputeptrs
        {
            template <typename Object>
            struct always_delete
            {
                static void release(Object* obj)
                {
                    delete obj;
                }
            }; // always_delete

            struct default_logger
            {
                /// Logs ref event.
                /// param g Object whose ref counted was increased.
                /// param count Objects's new ref count value.
                static void ref(void* o, ref_counter_type count)
                {
                    // TODO: Uncomment when Log is migrated
                    // log::fire_event<log::events::object_reffed>(o, count);
                }

                /// Logs unref event
                /// param o Object whose ref counted was decreased.
                /// param count Object's new ref count value.
                static void unref(void* o, ref_counter_type count)
                {
                    // TODO: Uncomment when Log is migrated
                    // log::fire_event<log::events::object_reffed>(o, count);
                }
            };  // default_logger

            enum class ref_policy
            {
                do_initial_ref,
                no_initial_ref
            };
        };  // namespace hetcomputeptrs

        template <typename Object, class Logger = hetcomputeptrs::default_logger, class ReleaseManager = hetcomputeptrs::always_delete<Object>>
        class ref_counted_object
        {
        public:
            using size_type = ref_counter_type;

            size_type use_count() const
            {
                return _num_refs.load();
            }

        protected:
            explicit ref_counted_object(size_type initial_value) : _num_refs(initial_value)
            {
            }

            constexpr ref_counted_object() : _num_refs(0)
            {
            }

            ~ref_counted_object()
            {
            }

        private:
            void unref()
            {
                auto new_num_refs = --_num_refs;
                Logger::unref(static_cast<Object*>(this), new_num_refs);

                if (new_num_refs == 0)
                {
                    ReleaseManager::release(static_cast<Object*>(this));
                }
            }

            void ref()
            {
                auto new_num_refs = ++_num_refs;
                Logger::ref(static_cast<Object*>(this), new_num_refs);
            }

            friend class hetcompute_shared_ptr<Object>;
            template <typename Descendant>

            friend void explicit_unref(Descendant* obj);
            template <typename Descendant>

            friend void explicit_ref(Descendant* obj);

            std::atomic<size_type> _num_refs;
        };  // class ref_counted_object

        template <typename Object>
        void explicit_unref(Object* obj)
        {
            obj->unref();
        }

        template <typename Object>
        void explicit_ref(Object* obj)
        {
            obj->ref();
        }

        // forward declarations for hetcompute_shared_ptr
        template<typename Object>
        bool operator==(hetcompute_shared_ptr<Object> const&, hetcompute_shared_ptr<Object> const&);

        template<typename Object>
        bool operator!=(hetcompute_shared_ptr<Object> const&, hetcompute_shared_ptr<Object> const&);

        /// hetcompute_shared_ptr is a smart pointer that destroys the object it
        /// points to when the last hetcompute_shared_ptr pointing to the same
        /// object goes out of scope, or when it gets reassigned to another
        /// object
        ///
        /// Object must implement ref/unref methods
        template <typename Object>
        class hetcompute_shared_ptr
        {
        public:
            typedef Object type;
            typedef Object* pointer;
            typedef hetcomputeptrs::ref_policy ref_policy;

            // Constructors
            constexpr hetcompute_shared_ptr() : _target(nullptr)
            {
            }

            constexpr /* implicit */ hetcompute_shared_ptr(std::nullptr_t) : _target(nullptr)
            {
            }

            // Copy constructor
            hetcompute_shared_ptr(hetcompute_shared_ptr const& other) : _target(other._target)
            {
                if (_target != nullptr)
                {
                    _target->ref();
                }
            }

            // Move constructor
            hetcompute_shared_ptr(hetcompute_shared_ptr&& other) : _target(other._target)
            {
                // we must not use reset_counter here because it would cause
                // the reference counter to go down
                other._target = nullptr;
            }

            // Destructor. It will destroy the object if it is the last
            // hetcompute_shared_ptr pointing to it.
            ~hetcompute_shared_ptr()
            {
                reset(nullptr);
            }

            // Assignment
            hetcompute_shared_ptr& operator=(hetcompute_shared_ptr const& other)
            {
                if (other._target == _target)
                {
                    return *this;
                }

                reset(other._target);
                return *this;
            }

            // Move assignment
            hetcompute_shared_ptr& operator=(hetcompute_shared_ptr&& other)
            {
                if (other._target == _target)
                {
                    return *this;
                }
                unref();

                _target       = other._target;
                other._target = nullptr;

                return *this;
            }

            void swap(hetcompute_shared_ptr& other)
            {
                std::swap(_target, other._target);
            }

            Object* get_raw_ptr() const
            {
                return _target;
            }

            Object* reset_but_not_unref()
            {
                pointer t = _target;
                _target   = nullptr;
                return t;
            }

            void reset()
            {
                reset(nullptr);
            }

            void reset(pointer ref)
            {
                unref();
                acquire(ref);
            }

            // Operators
            explicit operator bool() const
            {
                return _target != nullptr;
            }

            Object& operator*() const
            {
                return *_target;
            }

            // Same as get_raw_ptr
            Object* operator->() const
            {
                return _target;
            }

            size_t use_count() const
            {
                if (_target != nullptr)
                {
                    return _target->use_count();
                }
                return 0;
            }

            bool is_unique() const
            {
                return use_count() == 1;
            }

            explicit hetcompute_shared_ptr(pointer ref) : _target(ref)
            {
                if (_target != nullptr)
                {
                    _target->ref();
                }
            }

            hetcompute_shared_ptr(pointer ref, ref_policy policy) : _target(ref)
            {
                if ((_target != nullptr) && policy == ref_policy::do_initial_ref)
                {
                    _target->ref();
                }
            }

        private:
            void acquire(pointer other)
            {
                _target = other;
                if (_target != nullptr)
                {
                    _target->ref();
                }
            }

            void unref()
            {
                if (_target != nullptr)
                {
                    _target->unref();
                }
            }

            friend bool operator==<>(hetcompute_shared_ptr<Object> const&, hetcompute_shared_ptr<Object> const&);
            friend bool operator!=<>(hetcompute_shared_ptr<Object> const&, hetcompute_shared_ptr<Object> const&);

            pointer _target;

        };  // class hetcompute_shared_ptr

        template<typename Object>
        bool operator==(hetcompute_shared_ptr<Object> const& a_ptr, hetcompute_shared_ptr<Object> const& b_ptr)
        {
            return a_ptr.get_raw_ptr() == b_ptr.get_raw_ptr();
        }

        template <typename Object>
        bool operator==(hetcompute_shared_ptr<Object> const& ptr, std::nullptr_t)
        {
            return !ptr;
        }

        template <typename Object>
        bool operator==(std::nullptr_t, hetcompute_shared_ptr<Object> const& ptr)
        {
            return !ptr;
        }

        template <typename Object>
        bool operator!=(hetcompute_shared_ptr<Object> const& a_ptr, hetcompute_shared_ptr<Object> const& b_ptr)
        {
            return a_ptr.get_raw_ptr()!=b_ptr.get_raw_ptr();
        }

        template <typename Object>
        bool operator!=(hetcompute_shared_ptr<Object> const& ptr, std::nullptr_t)
        {
            return static_cast<bool>(ptr);
        }

        template <typename Object>
        bool operator!=(std::nullptr_t, hetcompute_shared_ptr<Object> const& ptr)
        {
            return static_cast<bool>(ptr);
        }

        template <typename Object>
        Object* c_ptr(hetcompute_shared_ptr<Object>& t)
        {
            return static_cast<Object*>(t.get_raw_ptr());
        }

        template <typename Object>
        Object* c_ptr(hetcompute_shared_ptr<Object> const& t)
        {
            return static_cast<Object*>(t.get_raw_ptr());
        }

        template <typename Object>
        Object* c_ptr(Object* p)
        {
            return p;
        }

    };  // namespace internal
};  // namespace hetcompute
