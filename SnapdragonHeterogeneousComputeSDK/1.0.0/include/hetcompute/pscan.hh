/** @file pscan.hh */
#pragma once

#include <hetcompute/taskfactory.hh>
#include <hetcompute/tuner.hh>
#include <hetcompute/internal/patterns/pscan-internal.hh>

namespace hetcompute
{
    namespace pattern
    {
        /** @addtogroup pscan_doc
        @{ */

        template <typename BinaryFn>
        class pscan;

        template <typename BinaryFn>
        pscan<BinaryFn> create_pscan_inclusive(BinaryFn&& fn);

        template <typename BinaryFn>
        class pscan
        {
        public:
            template <typename RandomAccessIterator>
            void run(RandomAccessIterator first, RandomAccessIterator last, hetcompute::pattern::tuner t = hetcompute::pattern::tuner())
            {
                if (!t.is_chunk_set())
                {
                    t.set_chunk_size(1024);
                }

                hetcompute::internal::pscan_inclusive_internal(nullptr, first, last, std::forward<BinaryFn>(_fn), t);
            }

            template <typename RandomAccessIterator>
            void
            operator()(RandomAccessIterator first, RandomAccessIterator last, const hetcompute::pattern::tuner& t = hetcompute::pattern::tuner())
            {
                run(first, last, t);
            }

        private:
            BinaryFn _fn;
            explicit pscan(BinaryFn&& fn) : _fn(fn) {}
            friend pscan create_pscan_inclusive<BinaryFn>(BinaryFn&& fn);
            template <typename Fn, typename... Args>
            friend hetcompute::task_ptr<void()> hetcompute::create_task(const pscan<Fn>& ps, Args&&... args);
        };

        /**
         * Create pattern object from function object <code>fn</code>.
         *
         * Returns a pattern object which can be invoked (1) synchronously, using the
         * run method or the () operator with arguments; or (2) asynchronously, using
         * hetcompute::create_task or hetcompute::launch.
         *
         * @par Examples
         * @code
         * auto l = std::plus<size_t>();
         * auto pscan = hetcompute::pattern::create_pscan_inclusive(l);
         * pscan(vec.begin(), vec.end());
         * @endcode
         */

        template <typename BinaryFn>
        pscan<BinaryFn> create_pscan_inclusive(BinaryFn&& fn)
        {
            using traits = internal::function_traits<BinaryFn>;

            static_assert(traits::arity::value == 2, "pscan takes a function accepting two arguments");

            return pscan<BinaryFn>(std::forward<BinaryFn>(fn));
        }

        /** @} */ /* end_addtogroup pscan_doc */

    }; // namespace pattern

    /** @addtogroup pscan_doc
        @{ */

    /**
     * Sklansky-style parallel inclusive scan.
     *
     * Performs an in-place parallel prefix computation using the
     * function object <code>fn</code> for the range [first, last).
     *
     * <code>fn</code> should be associative, because the order of
     * applications is not fixed.
     *
     * @note This function returns only after <code>fn</code> has been applied
     * to the whole iteration range.
     *
     * @note Similar to <code>hetcompute::ptransform</code>, range iterators
     * are restricted to type RandomAccessIterator.
     *
     * @note The usual rules for cancellation apply, i.e.,
     * within <code>fn</code> the cancellation must be acknowledged using
     * <code>abort_on_cancel</code>.
     *
     * @par Examples
     * @code
     * // After: v' = { v[0], v[0] x v[1], v[0] x v[1] x v[2], ... }
     * pscan_inclusive(begin(v), end(v),
     *                 [] (size_t const& i, size_t const& j) {
     *                     return i + j;
     *                 });
     * @endcode
     *
     * @param first Start of the range to which to apply <code>fn</code>.
     * @param last  End of the range to which to apply <code>fn</code>.
     * @param fn    Binary function object to be applied.
     * @param tuner HetCompute pattern tuner object (optional).
     */

    template <typename RandomAccessIterator, typename BinaryFn>
    void pscan_inclusive(RandomAccessIterator     first,
                         RandomAccessIterator     last,
                         BinaryFn&&               fn,
                         hetcompute::pattern::tuner tuner = hetcompute::pattern::tuner())
    {
        // Initial performance evaluation on 8974 indicates that chunk size of
        // 1024 seems to be the sweet spot
        if (!tuner.is_chunk_set())
        {
            tuner.set_chunk_size(1024);
        }

        hetcompute::internal::pscan_inclusive_internal(nullptr, first, last, std::forward<BinaryFn>(fn), tuner);
    }

    /**
     * Create an asynchronous task from the <code>hetcompute::pscan_inclusive</code> pattern.
     *
     * Returns a task that represents the pattern's execution. Operations on the
     * task translates into operations on the executing pattern. The caller must
     * launch the task.
     *
     * @param first Start of the range to which to apply <code>fn</code>.
     * @param last  End of the range to which to apply <code>fn</code>.
     * @param fn    Binary function object to be applied.
     * @param tuner HetCompute pattern tuner object (optional).
     */
    template <typename RandomAccessIterator, typename BinaryFn>
    hetcompute::task_ptr<void()> pscan_inclusive_async(RandomAccessIterator     first,
                                                     RandomAccessIterator     last,
                                                     BinaryFn                 fn,
                                                     hetcompute::pattern::tuner tuner = hetcompute::pattern::tuner())
    {
        if (!tuner.is_chunk_set())
        {
            tuner.set_chunk_size(1024);
        }

        return hetcompute::internal::pscan_inclusive_async(std::forward<BinaryFn>(fn), first, last, tuner);
    }

    /// @cond
    // Ignore this code fragment
    template <typename BinaryFn, typename... Args>
    hetcompute::task_ptr<void()> create_task(const hetcompute::pattern::pscan<BinaryFn>& ps, Args&&... args)
    {
        return hetcompute::internal::pscan_inclusive_async(ps._fn, args...);
    }

    template <typename BinaryFn, typename... Args>
    hetcompute::task_ptr<void()> launch(hetcompute::pattern::pscan<BinaryFn>& ps, Args&&... args)
    {
        auto t = hetcompute::create_task(ps, args...);
        t->launch();
        return t;
    }
    /// @endcond

    /** @} */ /* end_addtogroup pscan_doc */

}; // namespace hetcompute
