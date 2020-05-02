#pragma once

#include <type_traits>

namespace hetcompute
{
    namespace internal
    {
        // Counts the number of types
        template <typename... TS>
        struct count_typenames;

        template <>
        struct count_typenames<>
        {
            enum
            {
                value = 0
            };
        };

        template <typename T, typename... TS>
        struct count_typenames<T, TS...>
        {
            enum
            {
                value = 1 + count_typenames<TS...>::value
            };
        };

        // Checks whether a type exists
        template <typename T, typename... TS>
        struct type_exists;

        template <typename T>
        struct type_exists<T>
        {
            enum
            {
                value = 0
            };
        };

        template <typename T, typename T2, typename... TS>
        struct type_exists<T, T2, TS...>
        {
            enum
            {
                value = ((std::is_same<T, T2>::value == 1) ? 1 : type_exists<T, TS...>::value)
            };
        };

        // Checks whether there are duplicated types
        template <typename... TS>
        struct duplicated_types;

        template <>
        struct duplicated_types<>
        {
            enum
            {
                value = 0
            };
        };

        template <typename T, typename... TS>
        struct duplicated_types<T, TS...>
        {
            enum
            {
                value = ((type_exists<T, TS...>::value == 1) ? 1 : duplicated_types<TS...>::value)
            };
        };

        // Checks whether type T defines operator()
        template <typename T>
        class has_operator_parenthesis
        {
        private:
            template <typename U>
            static auto check(int) -> decltype(&U::operator(), std::true_type());

            template <typename>
            static std::false_type check(...);

            typedef decltype(check<T>(0)) enable_if_type;

        public:
            typedef typename enable_if_type::type type;

            enum
            {
                value = type::value
            };

        }; // has_operator_parenthesis

        /** Type is the larger of the two*/
        template <typename A, typename B>
        struct larger_type
        {
            typedef typename std::conditional<(sizeof(A) > sizeof(B)), A, B>::type type;
        };

        /** Generator of list of integers */
        template <size_t... N>
        struct integer_list_gen
        {
            template <size_t M>
            struct append
            {
                typedef integer_list_gen<N..., M> type;
            };
        };

        template <size_t N>
        struct integer_list
        {
            typedef typename integer_list<N - 1>::type::template append<N>::type type;
        };

        template <>
        struct integer_list<0>
        {
            typedef integer_list_gen<> type;
        };

        // Give N, instead of generating [1, 2,..., N], we also need to generate [1+x, 2+x,..., N+x]
        template <size_t N, size_t Offset>
        struct integer_list_offset
        {
            typedef typename integer_list_offset<N - 1, Offset>::type::template append<N + Offset>::type type;
        };

        template <size_t Offset>
        struct integer_list_offset<0, Offset>
        {
            typedef integer_list_gen<> type;
        };

        /**
         * Obtain subset of elements from tuple at specified indices
         * @param t tuple
         * @param index list
         * @return tuple containing elements drawn from specified index list
         */
        template <size_t... Indices, typename Tuple>
        auto tuple_subset(Tuple& t, integer_list_gen<Indices...>) -> decltype(std::make_tuple(std::get<Indices>(t)...))
        {
            return std::make_tuple(std::get<Indices>(t)...);
        }

        /**
         * Obtain head of tuple split at pos
         * @param t tuple
         * @param index list
         * @return tuple head subset before pos-generated index list
         */

        template <size_t... Indices, typename Tuple>
        auto tuple_head(Tuple& t, integer_list_gen<Indices...>) -> decltype(std::make_tuple(std::get<Indices - 1>(t)...))
        {
            return std::make_tuple(std::get<Indices - 1>(t)...);
        }

        /**
         * Returns all but the first element of a tuple
         * @param t tuple
         * @return rest of the tuple (without the first element)
         */
        template <typename First, typename... Rest>
        std::tuple<Rest...> tuple_rest(std::tuple<First, Rest...>& t)
        {
            return tuple_subset(t, typename integer_list<sizeof...(Rest)>::type());
        }

        ////

        template <typename F, typename TP, size_t prev_index, typename... ExpandedArgs>
        struct apply_tuple_helper
        {
            static void expand(F& f, TP&& tp, ExpandedArgs&&... eargs)
            {
                using bare_TP = typename std::remove_reference<typename std::remove_cv<TP>::type>::type;
                using Arg     = typename std::tuple_element<prev_index - 1, bare_TP>::type;
                apply_tuple_helper<F, TP, prev_index - 1, Arg, ExpandedArgs...>::expand(f,
                                                                                        std::forward<TP>(tp),
                                                                                        std::forward<Arg>(std::get<prev_index - 1>(tp)),
                                                                                        std::forward<ExpandedArgs>(eargs)...);
            }
        };

        template <typename F, typename TP, typename... ExpandedArgs>
        struct apply_tuple_helper<F, TP, 0, ExpandedArgs...>
        {
            static void expand(F& f, TP&&, ExpandedArgs&&... eargs) { f(std::forward<ExpandedArgs>(eargs)...); }
        };

        /**
         *  Applies a function/functor to a tuple-of-args
         */
        template <typename F, typename TP>
        void apply_tuple(F& f, TP&& tp)
        {
            using bare_TP               = typename std::remove_reference<typename std::remove_cv<TP>::type>::type;
            size_t constexpr prev_index = std::tuple_size<bare_TP>::value;
            apply_tuple_helper<F, TP, prev_index>::expand(f, std::forward<TP>(tp));
        }

        /*
         *  Strips consts, volatiles and references from T
         *  U const & const --> U
         */
        template <typename T>
        struct strip_ref_cv
        {
            using type = typename std::remove_cv<typename std::remove_reference<typename std::remove_cv<T>::type>::type>::type;
        };

    }; // namespace internal
};     // namespace hetcompute
