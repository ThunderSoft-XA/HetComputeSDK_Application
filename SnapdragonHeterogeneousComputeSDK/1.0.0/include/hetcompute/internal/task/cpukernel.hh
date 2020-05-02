#pragma once

// Include external headers
#include <hetcompute/taskptr.hh>

// Include internal headers
#include <hetcompute/internal/legacy/attr.hh>

namespace hetcompute
{
    namespace internal
    {
        template<typename UserCode>
        class cpu_kernel
        {
        public:
            using user_code_traits       = function_traits<UserCode>;
            using user_code_type_in_task = typename user_code_traits::type_in_task;

            using collapsed_task_type_info     = task_type_info<UserCode, true>;
            using non_collapsed_task_type_info = task_type_info<UserCode, false>;

            using collapsed_signature     = typename collapsed_task_type_info::final_signature;
            using non_collapsed_signature = typename non_collapsed_task_type_info::final_signature;

            using collapsed_raw_task_type     = cputask_arg_layer<collapsed_signature>;
            using non_collapsed_raw_task_type = cputask_arg_layer<non_collapsed_signature>;

            using anonymous_task_type_info = non_collapsed_task_type_info;
            using anonymous_raw_task_type  = non_collapsed_raw_task_type;

            using collapsed_task_type     = ::hetcompute::task_ptr<collapsed_signature>;
            using non_collapsed_task_type = ::hetcompute::task_ptr<non_collapsed_signature>;

            using size_type   = typename user_code_traits::size_type;
            using return_type = typename user_code_traits::return_type;
            using args_tuple  = typename user_code_traits::args_tuple;

            static HETCOMPUTE_CONSTEXPR_CONST size_type arity = user_code_traits::arity::value;

            /// Constructor.
            ///
            /// @param user_code Code that the task will run when the task executes.
            ///             This can be a function pointer, a lambda,  or a functor.
            explicit cpu_kernel(UserCode&& user_code)
                : _user_code(std::forward<UserCode>(user_code)), _attrs(legacy::create_task_attrs(::hetcompute::internal::legacy::attr::cpu))
            {
            }

            /// Constructor.
            ///
            /// @param user_code Code that the task will run when the task executes.
            ///           This can be a function pointer, a lambda,  or a functor.
            explicit cpu_kernel(UserCode const& user_code)
                : _user_code(user_code), _attrs(legacy::create_task_attrs(::hetcompute::internal::legacy::attr::cpu))
            {
            }

            /// Copy constructor.
            /// @param other Kernel used to create this kernel.
            cpu_kernel(cpu_kernel const& other) : _user_code(other._user_code), _attrs(other._attrs) {}

            /// Move constructor.
            /// @param other Kernel used to create this kernel.
            cpu_kernel(cpu_kernel&& other) : _user_code(std::move(other._user_code)), _attrs(std::move(other._attrs)) {}

            // Say that lambdas will fail to compile here b/c they don't have
            // copy-assignment
            cpu_kernel& operator=(cpu_kernel const& other)
            {
                static_assert(std::is_copy_assignable<UserCode>::value,
                              "Only bodies created using copy-assignable functors or function pointers are copy assignable.");

                if (&other == this)
                    return *this;

                _user_code = other._user_code;
                _attrs     = other._attrs;
                return *this;
            }

            cpu_kernel& operator=(cpu_kernel&& other)
            {
                if (&other == this)
                    return *this;
                _user_code   = std::move(other._user_code);
                _attrs       = other._attrs;
                other._attrs = legacy::create_task_attrs(::hetcompute::internal::legacy::attr::cpu);
                return *this;
            }

            template <typename Attribute>
            void set_attr(Attribute const& attr)
            {
                _attrs = ::hetcompute::internal::legacy::add_attr(_attrs, attr);
            }

            template <typename Attribute>
            bool has_attr(Attribute const& attr) const
            {
                return ::hetcompute::internal::legacy::has_attr(_attrs, attr);
            }

            user_code_type_in_task& get_fn() { return _user_code; }

            ::hetcompute::internal::legacy::task_attrs get_attrs() { return _attrs; }

        private:
            user_code_type_in_task _user_code;
            ::hetcompute::internal::legacy::task_attrs _attrs;

        };  // class cpu_kernel

    };  // namespace internal
};  // namespace hetcompute
