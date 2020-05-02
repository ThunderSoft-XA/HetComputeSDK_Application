/** @file ptransform.hh */
#pragma once

#include <hetcompute/pfor_each.hh>
#include <hetcompute/taskfactory.hh>
#include <hetcompute/tuner.hh>
#include <hetcompute/internal/patterns/ptransform-internal.hh>

namespace hetcompute
{
    namespace pattern
    {
        /** @addtogroup ptransform_doc
        @{ */

        template <typename Fn>
        class ptransformer;

        template <typename Fn>
        ptransformer<Fn> create_ptransform(Fn&& fn);

        template <typename Fn>
        class ptransformer
        {
        public:
            template <typename InputIterator>
            void run(InputIterator first, InputIterator last, const hetcompute::pattern::tuner& t = hetcompute::pattern::tuner())
            {
                internal::ptransform(nullptr, first, last, std::forward<Fn>(_fn), t);
            }

            template <typename InputIterator>
            void operator()(InputIterator first, InputIterator last, const hetcompute::pattern::tuner& t = hetcompute::pattern::tuner())
            {
                run(first, last, t);
            }

            template <typename InputIterator, typename OutputIterator>
            void run(InputIterator                   first,
                     InputIterator                   last,
                     OutputIterator                  d_first,
                     const hetcompute::pattern::tuner& t = hetcompute::pattern::tuner())
            {
                internal::ptransform(nullptr, first, last, d_first, std::forward<Fn>(_fn), t);
            }

            template <typename InputIterator, typename OutputIterator>
            void operator()(InputIterator                   first,
                            InputIterator                   last,
                            OutputIterator                  d_first,
                            const hetcompute::pattern::tuner& t = hetcompute::pattern::tuner())
            {
                run(first, last, d_first, t);
            }

            template <typename InputIterator1, typename InputIterator2, typename OutputIterator>
            void run(InputIterator1                  first1,
                     InputIterator1                  last1,
                     InputIterator2                  first2,
                     OutputIterator                  d_first,
                     const hetcompute::pattern::tuner& t = hetcompute::pattern::tuner())
            {
                internal::ptransform(nullptr, first1, last1, first2, d_first, std::forward<Fn>(_fn), t);
            }

            template <typename InputIterator1, typename InputIterator2, typename OutputIterator>
            void operator()(InputIterator1                  first1,
                            InputIterator1                  last1,
                            InputIterator2                  first2,
                            OutputIterator                  d_first,
                            const hetcompute::pattern::tuner& t = hetcompute::pattern::tuner())
            {
                run(first1, last1, first2, d_first, t);
            }

        private:
            Fn _fn;
            explicit ptransformer(Fn&& fn) : _fn(fn) {}
            friend ptransformer create_ptransform<Fn>(Fn&& fn);
            template <typename F, typename... Args>
            friend hetcompute::task_ptr<void()> hetcompute::create_task(const ptransformer<F>& ptf, Args&&... args);
        };

        /**
         * Create a pattern object from a function object <code>fn</code>.
         *
         * Returns a pattern object which can be invoked (1) synchronously, using the
         * run method or the () operator with arguments; or (2) asynchronously, using
         * hetcompute::create_task or hetcompute::launch.
         *
         * @par Examples
         * @code
         * auto l = [](size_t) {};
         * auto ptransform = hetcompute::pattern::create_ptransform(l);
         * ptransform(vin.begin(), vin.end());
         * @endcode
         *
         * @param fn Function object to be applied.
         */

        template <typename Fn>
        ptransformer<Fn> create_ptransform(Fn&& fn)
        {
            using traits = internal::function_traits<Fn>;

            static_assert(traits::arity::value == 1 || traits::arity::value == 2,
                          "ptransform takes a function accepting either one or two arguments");

            return ptransformer<Fn>(std::forward<Fn>(fn));
        }

        /** @} */ /* end_addtogroup ptransform_doc */

    }; // namespace pattern

    /** @addtogroup ptransform_doc
        @{ */

    /**
     * Parallel version of <code>std::transform</code>.
     *
     * Applies function object <code>fn</code> in parallel to every dereferenced
     * iterator in the range [first, last) and stores the return value in
     * another range, starting at <code>d_first</code>.
     *
     * @note This function returns only after <code>fn</code> has been applied
     * to the whole iteration range.
     *
     * @note In contrast to <code>pfor_each</code>, arguments specifying ranges are
     * restricted to RandomAccessIterator, where as <code>pfor_each</code>
     * allows them to be of integral type representing indices.
     *
     * @par Complexity
     * Exactly <code>std::distance(first,last)</code> applications of
     * <code>fn</code>.
     *
     * @sa ptransform(InputIterator, InputIterator, UnaryFn&&, const hetcompute::pattern::tuner&)
     * @sa pfor_each(InputIterator, InputIterator, UnaryFn&&, const hetcompute::pattern::tuner&)
     *
     * @par Examples
     * @code
     * // arr[i] == 2*vin[i]
     * size_t arr[vin.size()];
     * ptransform(begin(vin), end(vin), arr,
     *            [=] (size_t const& v) {
     *              return 2*v;
     *            });
     * @endcode
     *
     * @param first   Start of the range to which to apply <code>fn</code>.
     * @param last    End of the range to which to apply <code>fn</code>.
     * @param d_first Start of the destination range.
     * @param fn      Unary function object to be applied.
     * @param tuner   Qualcomm HetCompute pattern tuner object (optional).
     */
    template <typename InputIterator, typename OutputIterator, typename UnaryFn>
    // Without enable_if we will have overload mismatch problem because
    // tuner is a default argument. The compiler will incorrectly match
    // (it1, it2, fn, t) to this API.
    typename std::enable_if<!std::is_same<hetcompute::pattern::tuner, typename std::remove_reference<UnaryFn>::type>::value, void>::type
    ptransform(InputIterator                   first,
               InputIterator                   last,
               OutputIterator                  d_first,
               UnaryFn&&                       fn,
               const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
    {
        internal::ptransform(nullptr, first, last, d_first, std::forward<UnaryFn>(fn), tuner);
    }

    /**
     * Create an asynchronous task from the <code>hetcompute::ptransform</code> pattern.
     *
     * @note The usual rules for cancellation apply, i.e.,
     * within <code>fn</code> the cancellation must be acknowledged using
     * <code>abort_on_cancel</code>.
     *
     * @sa ptransform(InputIterator, InputIterator, OutputIterator, UnaryFn&&, const hetcompute::pattern::tuner&)
     *
     * @par Examples
     * @code
     * // create an async task from the ptransform pattern
     * auto t = ptransform_async(begin(vin), end(vin), arr,
     *            [=] (size_t const& i) {
     *              return 2 * i;
     *            });
     * t->launch();
     * t->wait_for();
     * @endcode
     */

    template <typename InputIterator, typename OutputIterator, typename UnaryFn>
    hetcompute::task_ptr<void()> ptransform_async(InputIterator                   first,
                                                InputIterator                   last,
                                                OutputIterator                  d_first,
                                                UnaryFn&&                       fn,
                                                const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
    {
        return hetcompute::internal::ptransform_async(std::forward<UnaryFn>(fn), first, last, d_first, tuner);
    }

    /**
     * Parallel version of <code>std::transform</code>.
     *
     * Applies function object <code>fn</code> in parallel to every pair of
     * dereferenced destination iterators in the range [first1, last1)
     * and [first2,...), and stores the return value in another range,
     * starting at <code>d_first</code>.
     *
     * @note This function returns only after <code>fn</code> has been applied
     * to the whole iteration range.
     *
     * @note In contrast to <code>pfor_each</code>, arguments specifying range are
     * restricted to RandomAccessIterator, where as <code>pfor_each</code>
     * allows them to be of integral type representing indices.
     *
     * @par Complexity
     * Exactly <code>std::distance(first1,last1)</code> applications of
     * <code>fn</code>.
     *
     * @sa ptransform(InputIterator, InputIterator, UnaryFn&&, const hetcompute::pattern::tuner&)
     * @sa pfor_each(InputIterator, InputIterator, UnaryFn&&, const hetcompute::pattern::tuner&)
     *
     * @par Examples
     * @code
     * // vout[i] == vin[i] + vin[i+1]
     * vector<size_t> vout(vin.size()-1);
     * ptransform(begin(vin), begin(vin)+vout.size(),
     *            begin(vin)+1,
     *            begin(vout),
     *            [=] (size_t const& op1, size_t const& op2) {
     *              return op1 + op2;
     *            });
     * @endcode
     *
     * @param first1  Start of the range to which to apply <code>fn</code>.
     * @param last1   End of the range to which to apply <code>fn</code>.
     * @param first2  Start of the second range to which to apply <code>fn</code>.
     * @param d_first Start of the destination range.
     * @param fn      Binary function object to be applied.
     * @param tuner   Qualcomm HetCompute pattern tuner object (optional).
     */

    template <typename InputIterator, typename OutputIterator, typename BinaryFn>
    // Without enable_if we will have overload mismatch problem because
    // tuner is a default argument. The compiler will incorrectly match
    // (it1, it2, output_iter, fn, t) to this API.
    typename std::enable_if<!std::is_same<hetcompute::pattern::tuner, typename std::remove_reference<BinaryFn>::type>::value, void>::type
    ptransform(InputIterator                   first1,
               InputIterator                   last1,
               InputIterator                   first2,
               OutputIterator                  d_first,
               BinaryFn&&                      fn,
               const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
    {
        internal::ptransform(nullptr, first1, last1, first2, d_first, std::forward<BinaryFn>(fn), tuner);
    }

    /**
     * Create an asynchronous task from the <code>hetcompute::ptransform</code> pattern.
     *
     * @note The usual rules for cancellation apply, i.e.,
     * within <code>fn</code> the cancellation must be acknowledged using
     * <code>abort_on_cancel</code>.
     *
     * @sa ptransform(InputIterator, InputIterator, InputIterator, OutputIterator, BinaryFn&&, const hetcompute::pattern::tuner&)
     */

    template <typename InputIterator, typename OutputIterator, typename BinaryFn>
    hetcompute::task_ptr<void()> ptransform_async(InputIterator                   first1,
                                                InputIterator                   last1,
                                                InputIterator                   first2,
                                                OutputIterator                  d_first,
                                                BinaryFn&&                      fn,
                                                const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
    {
        return hetcompute::internal::ptransform_async(std::forward<BinaryFn>(fn), first1, last1, first2, d_first, tuner);
    }

    /**
     * Parallel version of <code>std::transform</code>.
     *
     * Applies function object <code>fn</code> in parallel to every dereferenced
     * iterator in the range [first, last).
     *
     * @note In contrast to <code>pfor_each</code>, the dereferenced iterator is passed
     * to the function.
     *
     * @note In contrast to <code>pfor_each</code>, arguments specifying range are
     * restricted to RandomAccessIterator, where as <code>pfor_each</code>
     * allows them to be of integral type representing indices.
     *
     * It is permissible to modify the elements of the range from <code>fn</code>,
     * assuming that <code>InputIterator</code> is a mutable iterator.
     *
     * @note This function returns only after <code>fn</code> has been applied
     * to the whole iteration range.
     *
     * @par Complexity
     * Exactly <code>std::distance(first,last)</code> applications of
     * <code>fn</code>.
     *
     * @sa pfor_each(InputIterator, InputIterator, UnaryFn&&, const hetcompute::pattern::tuner&)
     *
     * @par
     * @code
     * // In-place double the value for all elements in range
     * ptransform(begin(vin), end(vin),
     *            [] (size_t& v) {
     *                v *= 2;
     *            });
     * @endcode
     *
     * @param first Start of the range to which to apply <code>fn</code>.
     * @param last  End of the range to which to apply <code>fn</code>.
     * @param fn    Unary function object to be applied.
     * @param tuner Qualcomm HetCompute pattern tuner object (optional).
     */
    template <typename InputIterator, typename UnaryFn>
    void ptransform(InputIterator first, InputIterator last, UnaryFn&& fn, const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
    {
        internal::ptransform(nullptr, first, last, std::forward<UnaryFn>(fn), tuner);
    }

    /**
     * Create an asynchronous task from the <code>hetcompute::ptransform</code> pattern.
     *
     * @note The usual rules for cancellation apply, i.e.,
     * within <code>fn</code> the cancellation must be acknowledged using
     * <code>abort_on_cancel</code>.
     *
     * @sa ptransform(InputIterator, InputIterator, UnaryFn&&, const hetcompute::pattern::tuner&)
     */

    template <typename InputIterator, typename UnaryFn>
    hetcompute::task_ptr<void()> ptransform_async(InputIterator                   first,
                                                InputIterator                   last,
                                                UnaryFn&&                       fn,
                                                const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
    {
        return hetcompute::internal::ptransform_async(std::forward<UnaryFn>(fn), first, last, tuner);
    }

    /// @cond
    // Ignore this code fragment
    template <typename Fn, typename... Args>
    hetcompute::task_ptr<void()> create_task(const hetcompute::pattern::ptransformer<Fn>& ptf, Args&&... args)
    {
        return hetcompute::internal::ptransform_async(ptf._fn, args...);
    }

    template <typename Fn, typename... Args>
    hetcompute::task_ptr<void()> launch(const hetcompute::pattern::ptransformer<Fn>& ptf, Args&&... args)
    {
        auto t = hetcompute::create_task(ptf, args...);
        t->launch();
        return t;
    }
    /// @endcond

    /** @} */ /* end_addtogroup ptransform_doc */

}; // namespace hetcompute
