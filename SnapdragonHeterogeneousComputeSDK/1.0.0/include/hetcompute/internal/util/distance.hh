#pragma once_flag

#include <type_traits>
#include <hetcompute/internal/compat/compat.h>

namespace hetcompute
{
    namespace internal
    {
        template <class InputIterator, bool = std::is_integral<InputIterator>::value>
        class distance_helper
        {
        public:
            typedef typename std::iterator_traits<InputIterator>::difference_type _result_type;

            static _result_type s_distance(InputIterator first, InputIterator last)
            {
                return std::distance(first, last);
            }
        };

        template <typename IntegralType>
        class distance_helper<IntegralType, true>
        {
        public:
            typedef IntegralType _result_type;

            static _result_type s_distance(IntegralType first, IntegralType last)
            {
                return last - first;
            }
        };

        template <class InputIterator>
        auto distance(InputIterator first, InputIterator last) -> typename distance_helper<InputIterator>::_result_type
        {
            return distance_helper<InputIterator>::s_distance(first, last);
        }
    }; // namespace internal
};     // namespace hetcompute
