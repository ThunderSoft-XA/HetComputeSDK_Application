/** @file interval_map.hh */
#pragma once

#include <iterator>
#include <map>
#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/util/interval.hh>

namespace hetcompute
{
    namespace internal
    {
        struct interval_map_base
        {
            virtual ~interval_map_base(){};
        };

        /**
           An interval map is a dictionary that associates data to
           (non-overlapping) intervals.  Intervals can be looked up uniquely
           through any of their contained points.
        */
        template <class K, class V, template <class> class Interval = interval>
        class interval_map : public interval_map_base
        {
        private:
            using map_type = typename std::map<Interval<K>, V>;
            map_type _map;

        public:
            using interval_type  = Interval<K>;
            using key_type       = K;
            using mapped_type    = V;
            using value_type     = typename map_type::value_type;
            using iterator       = typename map_type::iterator;
            using const_iterator = typename map_type::const_iterator;

            interval_map() : _map()
            {
            }

            /** @returns iterator to the end of the map */
            iterator end()
            {
                return _map.end();
            }

            /** @returns size of the map */
            size_t size()
            {
                return _map.size();
            }

            /** @returns const iterator to the end of the map */
            const_iterator end() const
            {
                return _map.end();
            }

            /** @returns const iterator to the end of the map */
            const_iterator cend() const
            {
                return end();
            }

            /** @returns iterator to the start of the map */
            iterator begin()
            {
                return _map.begin();
            }

            /** @returns const iterator to the start of the map */
            const_iterator begin() const
            {
                return _map.begin();
            }

            /** @returns const iterator to the start of the map */
            const_iterator cbegin() const
            {
                return begin();
            }

            /**
               @returns iterator to the first interval that contains point, or
                        end(), if none is found.
               @param   point point that is contained in any interval to be looked up.
            */
            iterator find(K const& point)
            {
                iterator it = _map.lower_bound(interval_type(point, point));
                if (it != end())
                {
                    if (it->first.begin() <= point && point < it->first.end())
                    {
                        return it;
                    }
                }

                if (it == begin())
                {
                    // empty map -> return nothing
                    return end();
                }

                // try previous interval
                --it;
                HETCOMPUTE_INTERNAL_ASSERT(it->first.begin() <= point, "invalid lower bound");
                if (point < it->first.end())
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
            const_iterator find(K const& point) const
            {
                const_iterator it = _map.lower_bound(interval_type(point, point));
                if (it != end())
                {
                    if (it->first.begin() <= point && point < it->first.end())
                    {
                        return it;
                    }
                }

                if (it == begin())
                {
                    // empty map -> return nothing
                    return end();
                }

                // try previous interval
                --it;
                HETCOMPUTE_INTERNAL_ASSERT(it->first.begin() <= point, "invalid lower bound");
                if (point < it->first.end())
                {
                    return it;
                }

                return end();
            }

            /**
               @returns data associated with interval
               @param interval used as a index
            */
            mapped_type& operator[](interval_type const& interval)
            {
                return _map[interval];
            }

            /**
               Inserts a pair of interval and associated data into map
               @param value pair(interval,data)
            */
            void insert(value_type&& value)
            {
                _map.insert(std::forward<value_type>(value));
            }

            /**
               Removes associations to passed interval from map
               @returns number of intervals removed
               @param interval interval to be removed
            */
            size_t erase(interval_type const&& interval)
            {
                return _map.erase(std::forward<interval_type const>(interval));
            }
        };
    };  // namespace internal
};  // namespace hetcompute
