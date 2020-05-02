/** @file psort.hh */
#pragma once

#include <hetcompute/taskfactory.hh>
#include <hetcompute/tuner.hh>
#include <hetcompute/internal/patterns/psort-internal.hh>

namespace hetcompute
{
    namespace pattern
    {
        /** @addtogroup psort_doc
            @{ */

        template <typename Compare>
        class psorter;

        template <typename Compare>
        psorter<Compare> create_psort(Compare&& cmp);

        template <typename Compare>
        class psorter
        {
        public:
            template <typename RandomAccessIterator>
            void run(RandomAccessIterator first, RandomAccessIterator last, const hetcompute::pattern::tuner& t = hetcompute::pattern::tuner())
            {
                internal::psort_internal(first, last, _cmp, t);
            }

            template <typename RandomAccessIterator>
            void
            operator()(RandomAccessIterator first, RandomAccessIterator last, const hetcompute::pattern::tuner& t = hetcompute::pattern::tuner())
            {
                run(first, last, t);
            }

            // TODO: This should be private, but the compiler complains
            explicit psorter(Compare&& cmp) : _cmp(cmp) {}

        private:
            Compare        _cmp;
            friend psorter create_psort<Compare>(Compare&& cmp);
            template <typename Cmp, typename... Args>
            friend hetcompute::task_ptr<void()> hetcompute::create_task(const psorter<Cmp>& p, Args&&... args);
        };

        // TODO: create_psort without compare function
        /**
         * Create <code>hetcompute::psort</code> from function objects <code>cmp</code>.
         *
         * Returns a pattern object which can be invoked (1) synchronously, using the
         * run method or the () operator with arguments; or (2) asynchronously, using
         * hetcompute::create_task or hetcompute::launch.
         *
         * @par Examples
         * @code
         * vector<int> vin(100000);
         * Rand_int rnd{0, int(vin.size() - 1)};
         *
         * // Generate 100,000 random integers
         * for (auto& i : vin)
         *   i = rnd();
         *
         * // Sort the vector using <code>hetcompute::psort</code>
         * auto p = hetcompute::pattern::create_psort([](int l, int r){return r < l;});
         * p(vin.begin(), vin.end());
         * @endcode
         *
         * @param cmp  User-customized compare function object to be applied.
         */

        template <typename Compare>
        psorter<Compare> create_psort(Compare&& cmp)
        {
            using traits = internal::function_traits<Compare>;

            static_assert(traits::arity::value == 2, "psort takes a function accepting two arguments");

            return psorter<Compare>(std::forward<Compare>(cmp));
        }

        /** @} */ /* end_addtogroup psort_doc */

    }; // namespace pattern

    /** @addtogroup psort_doc
        @{ */

    /**
     * Parallel version of <code>std::sort</code>.
     *
     * Performs an unstable in-place comparison sort of a container using the
     * supplied cmp function.
     *
     * @par Examples
     * @code
     * // Sort vin using <code>hetcompute::psort</code>
     * psort(vin.begin(), vin.end(), [](int l, int r){ return r < l; });
     * @endcode
     *
     * @sa psort(RandomAccessIterator, RandomAccessIterator)
     *
     * @param first Start of the range to sort.
     * @param last  End of the range to sort.
     * @param cmp   User-customized compare function object to be applied.
     * @param tuner Qualcomm HetCompute pattern tuner object (optional).
     */
    template <class RandomAccessIterator, class Compare>
    void psort(RandomAccessIterator            first,
               RandomAccessIterator            last,
               Compare                         cmp,
               const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
    {
        auto p = hetcompute::pattern::create_psort(cmp);
        p(first, last, tuner);
    }

    /**
     * Create an asynchronous task from the <code>hetcompute::psort</code> pattern.
     *
     * @param first Start of the range to sort.
     * @param last  End of the range to sort.
     * @param cmp   User-customized compare function object to be applied.
     * @param tuner Qualcomm HetCompute pattern tuner object (optional).
     */
    template <class RandomAccessIterator, class Compare>
    hetcompute::task_ptr<void()> psort_async(RandomAccessIterator            first,
                                           RandomAccessIterator            last,
                                           Compare&&                       cmp,
                                           const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
    {
        return hetcompute::internal::psort_async(std::forward<Compare>(cmp), first, last, tuner);
    }

    /**
     * Parallel version of <code>std::sort</code>.
     *
     * Performs an unstable in-place comparison sort of a container.
     * Equivalent to psort(first, last, std::less<T>()) where T is the value type
     * of the iterators.
     *
     * @sa psort(RandomAccessIterator, RandomAccessIterator, Compare)
     *
     * @param first Start of the range to sort.
     * @param last  End of the range to sort.
     * @param tuner Qualcomm HetCompute pattern tuner object (optional).
     */
    template <class RandomAccessIterator>
    void psort(RandomAccessIterator first, RandomAccessIterator last, const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
    {
        auto p = hetcompute::pattern::create_psort(std::less<typename std::iterator_traits<RandomAccessIterator>::value_type>());
        p(first, last, tuner);
    }

    /**
     * Parallel version of <code>std::sort</code> (asynchronous).
     *
     * Returns a task that represents the pattern's execution. Operations on the
     * task translate into operations on the executing pattern. The caller must
     * launch the task.
     *
     * @sa psort(RandomAccessIterator, RandomAccessIterator)
     *
     * @param first Start of the range to sort.
     * @param last  End of the range to sort.
     * @param tuner Qualcomm HetCompute pattern tuner object (optional).
     */
    template <class RandomAccessIterator>
    hetcompute::task_ptr<void()>
    psort_async(RandomAccessIterator first, RandomAccessIterator last, const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
    {
        auto g    = internal::legacy::create_group();
        auto t    = hetcompute::create_task([g, first, last, tuner] {
            internal::psort_internal(first, last, std::less<typename std::iterator_traits<RandomAccessIterator>::value_type>(), tuner);
            internal::legacy::finish_after(g);
        });
        auto gptr = internal::c_ptr(g);
        gptr->set_representative_task(internal::c_ptr(t));
        return t;
    }

    /// @cond
    // Ignore this code fragment
    template <typename Compare, typename... Args>
    hetcompute::task_ptr<void()> create_task(const hetcompute::pattern::psorter<Compare>& p, Args&&... args)
    {
        return hetcompute::internal::psort_async(p._cmp, args...);
    }

    template <typename Compare, typename... Args>
    hetcompute::task_ptr<void()> launch(const hetcompute::pattern::psorter<Compare>& p, Args&&... args)
    {
        auto t = hetcompute::create_task(p, args...);
        t->launch();
        return t;
    }
    /// @endcond

    /** @} */ /* end_addtogroup psort_doc */

}; // namespace hetcompute
