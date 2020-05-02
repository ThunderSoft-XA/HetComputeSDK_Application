#pragma once

#include <tuple>
#include <typeinfo>
#include <type_traits>
#include <hetcompute/internal/util/templatemagic.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace user_code_types
        {
            using functor             = std::integral_constant<size_t, 0>;
            using const_functor       = std::integral_constant<size_t, 1>;
            using functor_const       = std::integral_constant<size_t, 2>;
            using const_functor_const = std::integral_constant<size_t, 3>;
            using function            = std::integral_constant<size_t, 4>;
            using function_pointer    = std::integral_constant<size_t, 5>;
            using function_ref        = std::integral_constant<size_t, 6>;
            using invalid             = std::integral_constant<size_t, 42>;
        };

        // Matches lambdas and member functions.
        template <typename F>
        struct function_traits_dispatch;

        // Matches C::operator(A...) and [](A...) mutable { }
        template <typename ReturnType, typename ClassType, typename... Args>
        struct function_traits_dispatch<ReturnType (ClassType::*)(Args...)>
        {
            using size_type = std::size_t;
            using arity     = std::integral_constant<size_type, sizeof...(Args)>;

            using kind = user_code_types::functor;

            using return_type = ReturnType;
            using args_tuple  = std::tuple<Args...>;
            using class_type  = ClassType;
            using signature   = return_type(Args...);

            template <size_type Index>
            using arg_type = typename std::tuple_element<Index, args_tuple>::type;
        };

        // Matches const C::operator(A...) and const [](A...) mutable { }
        template <typename ReturnType, typename ClassType, typename... Args>
        struct function_traits_dispatch<const ReturnType (ClassType::*)(Args...)>
        {
            using size_type = std::size_t;
            using arity     = std::integral_constant<size_type, sizeof...(Args)>;

            using kind = user_code_types::const_functor;

            using return_type = const ReturnType;
            using args_tuple  = std::tuple<Args...>;
            using class_type  = ClassType;
            using signature   = return_type(Args...);

            template <size_type Index>
            using arg_type = typename std::tuple_element<Index, args_tuple>::type;
        };

        // Matches const C::operator(A...) and const [](A...) { }
        template <typename ReturnType, typename ClassType, typename... Args>
        struct function_traits_dispatch<ReturnType (ClassType::*)(Args...) const>
        {
            using size_type = std::size_t;
            using arity     = std::integral_constant<size_type, sizeof...(Args)>;

            using kind = user_code_types::functor_const;

            using return_type = ReturnType;
            using args_tuple  = std::tuple<Args...>;
            using class_type  = ClassType;
            using signature   = return_type(Args...);

            template <size_type Index>
            using arg_type = typename std::tuple_element<Index, args_tuple>::type;
        };

        template <typename ReturnType, typename ClassType, typename... Args>
        struct function_traits_dispatch<const ReturnType (ClassType::*)(Args...) const>
        {
            using size_type = std::size_t;
            using arity     = std::integral_constant<size_type, sizeof...(Args)>;

            using kind = user_code_types::const_functor_const;

            using return_type = ReturnType const;
            using args_tuple  = std::tuple<Args...>;
            using class_type  = ClassType;
            using signature   = return_type(Args...);

            template <size_type Index>
            using arg_type = typename std::tuple_element<Index, args_tuple>::type;
        };

        // This struct is needed because of VS2012. Neither GCC or CLANG need it
        template <typename>
        struct function_traits_dispatch
        {
            using size_type = std::size_t;
            using arity     = std::integral_constant<size_type, 0>;

            using kind = user_code_types::invalid;

            using return_type = void;
            using args_tuple  = std::tuple<void>;
            using class_type  = void;
            using signature   = void();
        };

        // Computes properties of a lambda or function : its arity,
        // its return type and the types of its arguments.
        //
        // We need to use std::remove_references because forwarding causes
        // some drama with references. By removing the references, the
        // function_traits_dispatch templates can accurately match F.
        template <typename F, class Enable = void>
        struct function_traits;

        // Matches lambdas and member functions (both mutable and const)
        template <typename F>
        struct function_traits<F, typename std::enable_if<has_operator_parenthesis<typename std::remove_reference<F>::type>::value>::type>
        {
            using type_in_task   = typename std::remove_reference<F>::type;
            using pointer_type   = decltype(&type_in_task::operator());
            using has_destructor = std::true_type;

            using kind        = typename function_traits_dispatch<pointer_type>::kind;
            using arity       = typename function_traits_dispatch<pointer_type>::arity;
            using return_type = typename function_traits_dispatch<pointer_type>::return_type;
            using args_tuple  = typename function_traits_dispatch<pointer_type>::args_tuple;
            using class_type  = typename function_traits_dispatch<pointer_type>::class_type;
            using signature   = typename function_traits_dispatch<pointer_type>::signature;

            using size_type = typename function_traits_dispatch<pointer_type>::size_type;
            template <size_type Index>
            using arg_type = typename std::tuple_element<Index, args_tuple>::type;
        };

        // Matches functions
        template <typename ReturnType, typename... Args>
        struct function_traits<ReturnType (&)(Args...)>
        {
            using kind = user_code_types::function_pointer;

            using pointer_type = ReturnType (*)(Args...);
            using type_in_task = ReturnType (*)(Args...);
            using return_type  = ReturnType;
            using args_tuple   = std::tuple<Args...>;
            using signature    = return_type(Args...);

            using size_type      = std::size_t;
            using arity          = std::integral_constant<size_type, sizeof...(Args)>;
            using has_destructor = std::false_type;

            template <size_type Index>
            using arg_type = typename std::tuple_element<Index, args_tuple>::type;
        };

        // Matches functions
        template <typename ReturnType, typename... Args>
        struct function_traits<ReturnType (*)(Args...)>
        {
            using kind = user_code_types::function_pointer;

            using pointer_type = ReturnType (*)(Args...);
            using type_in_task = ReturnType (*)(Args...);
            using return_type  = ReturnType;
            using args_tuple   = std::tuple<Args...>;
            using signature    = return_type(Args...);

            using size_type      = std::size_t;
            using arity          = std::integral_constant<size_type, sizeof...(Args)>;
            using has_destructor = std::false_type;

            template <size_type Index>
            using arg_type = typename std::tuple_element<Index, args_tuple>::type;
        };

        // Matches functions
        template <typename ReturnType, typename... Args>
        struct function_traits<ReturnType(Args...)>
        {
            using kind = user_code_types::function_pointer;

            using pointer_type   = ReturnType (*)(Args...);
            using type_in_task   = ReturnType (*)(Args...);
            using return_type    = ReturnType;
            using args_tuple     = std::tuple<Args...>;
            using signature      = return_type(Args...);
            using has_destructor = std::false_type;

            using size_type = std::size_t;
            using arity     = std::integral_constant<size_type, sizeof...(Args)>;

            template <size_type Index>
            using arg_type = typename std::tuple_element<Index, args_tuple>::type;
        };

        /**
         * Checks if a callable object is const qualified
         * The intention of it is to prevent mutable callable objects to be called upon
         * hetcompute patterns. In general, it's a bad idea to pass a copy of the variable
         * to the closure and modify it within in a multi-threaded context. The expected
         * result is in general undefined.
         * As a rule of thumb, we disallow mutable kernels pass to pfor_each and ptransform
         * in hetcompute.
         */
        template <typename F, typename Enable = void>
        struct callable_object_is_mutable : std::integral_constant<bool, true>
        {
        };

        template <typename F>
        struct callable_object_is_mutable<
            F,
            typename std::enable_if<std::is_same<typename function_traits<F>::kind, user_code_types::functor_const>::value ||
                                    std::is_same<typename function_traits<F>::kind, user_code_types::const_functor_const>::value>::type>
            : std::integral_constant<bool, false>
        {
        };

    }; // namespace hetcompute::internal
};     // namespace hetcompute
