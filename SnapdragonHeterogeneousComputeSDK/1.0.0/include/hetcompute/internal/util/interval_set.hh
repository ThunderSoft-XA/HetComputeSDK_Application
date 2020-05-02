/** @file interval_set.hh */
#pragma once

#include <iterator>
#include <set>

#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/util/interval.hh>

namespace hetcompute
{
    namespace internal
    {
        struct interval_set_base {};

        /**
           An interval set stores (non-overlapping) intervals.  Intervals can
           be looked up uniquely through any of their contained points.
        */
        template<class T, template<class> class Interval = interval>
        class interval_set : public interval_set_base
        {
        private:
            using set_type = typename std::set<Interval<T>>;
            set_type _set;

        public:
            using interval_type  = Interval<T>;
            using element_type   = T;
            using iterator       = typename set_type::iterator;
            using const_iterator = typename set_type::const_iterator;

            interval_set() : _set()
            {
            }

            /** @returns iterator to the end of the set */
            iterator end()
            {
                return _set.end();
            }

            /** @returns const iterator to the end of the set */
            const_iterator end() const
            {
                return _set.end();
            }

            /** @returns const iterator to the end of the set */
            const_iterator cend() const
            {
                return end();
            }

            /** @returns iterator to the start of the set */
            iterator begin()
            {
                return _set.begin();
            }

            /** @returns const iterator to the start of the set */
            const_iterator begin() const
            {
                return _set.begin();
            }

            /** @returns const iterator to the start of the set */
            const_iterator cbegin() const
            {
                return begin();
            }

            /**
               @returns iterator to the first interval that contains point, or
                        end(), if none is found.
               @param   point point that is contained in any interval to be looked
                        up.
            */
            iterator find(T const& point)
            {
                iterator it = _set.lower_bound(interval_type(point, point));
                if (it != end())
                {
                    if (it->begin() <= point && point < it->end())
                    {
                        return it;
                    }
                }

                if (it == begin())
                {
                    // empty set -> return nothing
                    return end();
                }

                // try previous interval
                --it;
                HETCOMPUTE_INTERNAL_ASSERT(it->begin() <= point, "invalid lower bound");
                if (point < it->end())
                {
                    return it;
                }

                return end();
            }

            /**
               @returns const iterator to the first interval that contains
                        point, or end(), if none is found.
               @param   point point that is contained in any interval to be looked up.
            */
            const_iterator find(T const& point) const
            {
                const_iterator it = _set.lower_bound(interval_type(point, point));
                if (it != end())
                {
                    if (it->begin() <= point && point < it->end())
                    {
                        return it;
                    }
                }

                if (it == begin())
                {
                    // empty set -> return nothing
                    return end();
                }

                // try previous interval
                --it;
                HETCOMPUTE_INTERNAL_ASSERT(it->begin() <= point, "invalid lower bound");
                if (point < it->end())
                {
                    return it;
                }

                return end();
            }

            /**
               Inserts an interval into the set.
               @param interval interval to be inserted
            */
            void insert(interval_type const& interval)
            {
                _set.insert(interval);
            }

            /**
               Removes interval from set.
               @returns number of intervals removed
               @param interval interval to be removed
            */
            size_t erase(interval_type const&& interval)
            {
                return _set.erase(std::forward<interval_type const>(interval));
            }
        };
    };  // namespace internal
};  // namespace hetcompute
