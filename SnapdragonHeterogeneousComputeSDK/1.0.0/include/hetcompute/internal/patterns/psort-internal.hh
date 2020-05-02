#pragma once

#include <hetcompute/internal/patterns/pdivide-and-conquer-internal.hh>

namespace hetcompute
{
    namespace internal
    {
        // The following two functions are used to determine a suitable pivot,
        // resilient to adversarial inputs. Rather than choosing a fixed pivot, pick
        // the median of nine input elements.

        // A direct implementation of 3-way quicksort based on the classic paper
        // "Engineering a Sort Function" by Bentley and McIlroy
        template <typename RandomAccessIterator, typename Compare>
        inline size_t median_of_three(const RandomAccessIterator& first, size_t a, size_t b, size_t c, Compare cmp)
        {
            return cmp(first[a], first[b]) ? (cmp(first[b], first[c]) ? b : cmp(first[a], first[c]) ? c : a) :
                                             (cmp(first[c], first[b]) ? b : cmp(first[c], first[a]) ? c : a);
        }

        // Pseudo median of nine, aka Tukey's ninther
        template <typename RandomAccessIterator, typename Compare>
        inline size_t ninther(const RandomAccessIterator& first, size_t n, Compare cmp)
        {
            auto size = n / 8;
            return median_of_three(first,
                                   median_of_three(first, 0, size, size * 2, cmp),
                                   median_of_three(first, size * 3, size * 4, size * 5, cmp),
                                   median_of_three(first, size * 6, size * 7, n - 1, cmp),
                                   cmp);
        }

        template <class RandomAccessIterator, class Compare>
        void psort_internal(RandomAccessIterator first, RandomAccessIterator last, Compare cmp, const hetcompute::pattern::tuner& tuner)
        {
            if (tuner.is_serial())
            {
                std::sort(first, last, cmp);
                return;
            }

            struct quicksort_t
            {
                quicksort_t(RandomAccessIterator f, RandomAccessIterator l) : _first(f), _last(l), _middle() {}
                RandomAccessIterator _first, _last, _middle;
            };

            const size_t granularity = 512;

            hetcompute::internal::pdivide_and_conquer(
                nullptr,
                // Main problem
                quicksort_t(first, last),
                // When should arrays be sorted sequentially?
                // If the array is smaller than granularity.
                [&](quicksort_t& q) {
                    size_t n = std::distance(q._first, q._last);
                    if (n <= granularity)
                    {
                        return true;
                    }
                    // Choice of first element as pivot is arbitrary
                    auto pivot = ninther(q._first, n, cmp);
                    q._middle  = std::partition(q._first, q._last, std::bind(cmp, std::placeholders::_1, q._first[pivot]));
                    // If middle == first, elements in [first, last) are greater than or
                    // equal to pivot. We could either find a new pivot or as we do here,
                    // just sort sequentially.
                    return q._middle == q._first;
                },
                // Sequential sort used
                [&](quicksort_t& q) { std::sort(q._first, q._last, cmp); },
                // Split problem into two subproblems
                [&](quicksort_t& q) {
                    std::array<quicksort_t, 2> subarrays{ { quicksort_t(q._first, q._middle), quicksort_t(q._middle, q._last) } };
                    return subarrays;
                });
        }

        template <class RandomAccessIterator, class Compare>
        hetcompute::task_ptr<void()> psort_async(Compare&&                       cmp,
                                               RandomAccessIterator            first,
                                               RandomAccessIterator            last,
                                               const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())

        {
            auto g    = legacy::create_group();
            auto t    = hetcompute::create_task([g, first, last, cmp, tuner] {
                internal::psort_internal(first, last, cmp, tuner);
                legacy::finish_after(g);
            });
            auto gptr = internal::c_ptr(g);
            gptr->set_representative_task(internal::c_ptr(t));
            return t;
        }


    }; // namespace internal
};     // namespace hetcompute
