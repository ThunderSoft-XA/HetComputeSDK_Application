/** @file interval.hh */
#pragma once

#include <cstddef>
#include <iterator>
#include <type_traits>

namespace hetcompute
{
    namespace internal
    {
        /**
           Traits of intervals: distance between two points of an interval, etc.
        */
        template <class T, class Enable = void>
        struct interval_traits
        {
            using difference_type = typename std::iterator_traits<T>::difference_type;
            static difference_type distance(T a, T b)
            {
                return std::distance(a, b);
            }
        };

        template <class T>
        struct interval_traits<T, typename std::enable_if<std::is_integral<T>::value>::type>
        {
            using difference_type = std::ptrdiff_t;
            static difference_type distance(T a, T b)
            {
                return b - a;
            }
        };

        template <>
        struct interval_traits<const void*>
        {
            using difference_type = std::ptrdiff_t;
            static difference_type distance(const void* a, const void* b)
            {
                return static_cast<const char*>(b) - static_cast<const char*>(a);
            }
        };

        template <>
        struct interval_traits<void*>
        {
            using difference_type = std::ptrdiff_t;
            static difference_type distance(const void* a, const void* b)
            {
                return static_cast<const char*>(b) - static_cast<const char*>(a);
            }
        };

        /**
           @brief Intervals describe sets of points.

           An interval is a set of points [start,end) denoted by a start point
           and an end point.  The start point is part of the set, whereas the
           end point is not.  Thus, intervals are half-open.
        */
        template <class T>
        class interval
        {
        public:
            /**
               @param i_begin start of the interval
               @param i_end end of the interval (i_end not included)
            */
            interval(T i_begin, T i_end) : _begin(i_begin), _end(i_end)
            {
                HETCOMPUTE_INTERNAL_ASSERT(_begin <= _end, "illegal interval");
            }

            interval(interval const& other) : _begin(other.begin()), _end(other.end())
            {
            }

            /** @returns Length of the interval, i.e., the distance between the
                start and the end point
            */
            auto length() const
                -> typename interval_traits<T>::difference_type
            {
                return interval_traits<T>::distance(_begin, _end);
            }

            /** @returns start point of the interval */
            T begin() const
            {
                return _begin;
            }

            /** @returns end point of the interval */
            T end() const
            {
                return _end;
            }
            /** @returns true if an interval is equal than another, i.e., if
                         both points are equal and both end points are equal.

                @param interval to compare with
            */
            bool operator==(interval const& other) const
            {
                return begin() == other.begin() && end() == other.end();
            }

            /** @returns true if an interval is smaller than another, i.e., if
                         its start point is smaller than the other.

                @param interval to compare with
            */
            bool operator<(interval const& other) const
            {
                return begin() < other.begin();
            }

        private:
            T _begin;
            T _end;
        };

        /**
           @returns half-open interval [start,end)

           @param begin start point of the interval
           @param end   end point of the interval
        */
        template <class T>
        interval<T> make_interval(T begin, T end)
        {
            return interval<T>(begin, end);
        }

        /**
           @returns half-open interval [start,start + length)

           @param begin start point of the interval
           @param length length of the interval
        */
        template <class T>
        interval<T> make_length_interval(T begin, std::ptrdiff_t length)
        {
            return interval<T>(begin, begin + length);
        }

        template <class T>
        std::ostream& operator<<(std::ostream& stream, interval<T> const& interval)
        {
            stream << "[" << interval.begin() << "," << interval.end() << ")";
            return stream;
        }
    };  // namespace internal
};  // namespace hetcompute
