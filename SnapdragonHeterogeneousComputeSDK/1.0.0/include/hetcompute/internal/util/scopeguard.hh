#pragma once

#include <hetcompute/internal/util/macros.hh>

namespace hetcompute
{
    namespace internal
    {
        /// \brief ensures execution of an action at the end of a scope
        ///
        /// The effect of defining a scope guard is similar to Java's
        /// `try ... finally` statement.
        ///
        /// \par Example
        /// \code
        /// {
        ///   scope_guard guard([] { std::cout << "hi" << std::endl; });
        ///   throw std::exception("die");
        /// } // still prints "hi"
        /// \endcode

        template <typename NullaryFn>
        struct scope_guard
        {
            /* implicit */ scope_guard(NullaryFn&& fn) : _active(true), _fn(std::move(fn))
            {
            }

            /* implicit */ scope_guard(scope_guard&& other) : _active(other._active), _fn(std::move(other._fn))
            {
                other.reset();
            }

            HETCOMPUTE_DELETE_METHOD(scope_guard());
            HETCOMPUTE_DELETE_METHOD(scope_guard(const scope_guard&));
            HETCOMPUTE_DELETE_METHOD(scope_guard& operator=(scope_guard const&));

            /// \brief prohibits scope_guard from executing
            void reset()
            {
                _active = false;
            }

            ~scope_guard()
            {
                if (_active)
                {
                    _fn();
                }
            }

        private:
            bool _active;
            NullaryFn const _fn;
        };

        template <typename NullaryFn>
        inline scope_guard<NullaryFn> make_scope_guard(NullaryFn&& fn)
        {
            return scope_guard<NullaryFn>(std::forward<NullaryFn>(fn));
        }

    }; // namespace internal
}; // namespace hetcompute
