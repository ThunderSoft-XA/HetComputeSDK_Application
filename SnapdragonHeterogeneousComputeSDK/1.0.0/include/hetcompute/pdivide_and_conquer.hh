/** @file pdivide_and_conquer.hh */
#pragma once

#include <hetcompute/taskfactory.hh>
#include <hetcompute/tuner.hh>
#include <hetcompute/internal/patterns/pdivide-and-conquer-internal.hh>

namespace hetcompute
{
    /** @addtogroup pdnc_doc
        @{ */

    // we need to forward declare the class to be referenced
    namespace pattern
    {
        template <typename IsBaseFn, typename BaseFn, typename SplitFn, typename MergeFn>
        class pdivide_and_conquerer;
    };

    /**
     * Parallel divide-and-conquer
     *
     * Solve a problem by splitting it into independent subproblems, which may
     * be solved in parallel, and merging the solutions to the subproblems.
     * A subproblem may recursively be split into yet more problems, yielding
     * significant parallelism, e.g., Fibonacci.
     *
     * @note1 For best performance, make <code>split</code> and <code>merge</code>
     * relatively inexpensive compared to <code>base</code>.
     *
     * @par Example: Compute n-th Fibonacci term
     * @includelineno samples/src/pdivide_and_conquer_fibonacci.cc
     *
     * @param p         Problem data structure operated on by the functions.
     * @param is_base   Returns TRUE if <code>p</code> is a base case problem,
     *                  else FALSE.
     * @param base      Solves a base case problem.
     * @param split     Splits <code>p</code> into subproblems and returns them.
     * @param merge     Merges the solutions to subproblems of <code>p</code>
     *                  returned by <code>split</code>.
     * @param tuner     Qualcomm HetCompute pattern tuner object (optional).
     * @return Solution data structure representing solution to the problem.
     */
    template <typename Problem, typename Solution, typename Fn1, typename Fn2, typename Fn3, typename Fn4>
    Solution pdivide_and_conquer(Problem                         p,
                                 Fn1&&                           is_base,
                                 Fn2&&                           base,
                                 Fn3&&                           split,
                                 Fn4&&                           merge,
                                 const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
    {
        HETCOMPUTE_UNUSED(tuner);
        auto t = hetcompute::internal::pdivide_and_conquer<Problem, Solution>(nullptr,
                                                                            std::forward<Problem>(p),
                                                                            std::forward<Fn1>(is_base),
                                                                            std::forward<Fn2>(base),
                                                                            std::forward<Fn3>(split),
                                                                            std::forward<Fn4>(merge));
        return t->move_value();
    }

    /**
     * Asynchronous Parallel divide-and-conquer
     *
     * Returns a task that represents the pattern's execution. Operations on the
     * task translate into operations on the executing pattern. The caller must
     * launch the task.
     *
     * @param p         Problem data structure operated on by the functions.
     * @param is_base   Returns TRUE if <code>p</code> is a base case problem,
     *                  else FALSE.
     * @param base      Solves a base case problem.
     * @param split     Splits <code>p</code> into subproblems and returns them.
     * @param merge     Merges solutions to subproblems of <code>p</code>
     *                  returned by <code>split</code>.
     * @param tuner     Qualcomm HetCompute pattern tuner object (optional).
     * @return task_ptr Unlaunched task representing pattern's execution.
     */

    template <typename Problem, typename Solution, typename Fn1, typename Fn2, typename Fn3, typename Fn4>
    hetcompute::task_ptr<Solution> pdivide_and_conquer_async(Problem                         p,
                                                           Fn1&&                           is_base,
                                                           Fn2&&                           base,
                                                           Fn3&&                           split,
                                                           Fn4&&                           merge,
                                                           const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
    {
        HETCOMPUTE_UNUSED(tuner);
        return hetcompute::internal::pdivide_and_conquer_async<Problem, Solution>(std::forward<Fn1>(is_base),
                                                                                std::forward<Fn2>(base),
                                                                                std::forward<Fn3>(split),
                                                                                std::forward<Fn4>(merge),
                                                                                p);
    }

    /**
     * Parallel divide-and-conquer specialized for not
     * returning a solution, e.g., mergesort.
     *
     * @param p       Problem data structure operated on by the functions.
     * @param is_base Returns TRUE if <code>p</code> is a base case problem,
     *                else FALSE.
     * @param base    Solves a base case problem, but does not return any solution.
     * @param split   Splits <code>p</code> into subproblems and returns them.
     * @param merge   Merges subproblems returned by <code>split</code>.
     * @param tuner   Qualcomm HetCompute pattern tuner object (optional).
     */

    template <typename Problem, typename Fn1, typename Fn2, typename Fn3, typename Fn4>
    void pdivide_and_conquer(Problem                         p,
                             Fn1&&                           is_base,
                             Fn2&&                           base,
                             Fn3&&                           split,
                             Fn4&&                           merge,
                             const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
    {
        HETCOMPUTE_UNUSED(tuner);
        hetcompute::internal::pdivide_and_conquer(nullptr,
                                                p,
                                                std::forward<Fn1>(is_base),
                                                std::forward<Fn2>(base),
                                                std::forward<Fn3>(split),
                                                std::forward<Fn4>(merge));
    }

    /**
     * Asynchronous parallel divide-and-conquer specialized for not returning
     * a solution, e.g., mergesort.
     *
     * Returns a task that represents the pattern's execution. Operations on the
     * task translate into operations on the executing pattern. The caller must
     * launch the task.
     *
     * @param p         Problem data structure operated on by the functions.
     * @param is_base   Returns TRUE if <code>p</code> is a base case problem,
     *                  else FALSE.
     * @param base      Solves a base case problem.
     * @param split     Splits <code>p</code> into subproblems and returns them.
     * @param merge     Merges solutions to subproblems of <code>p</code>
     *                  returned by <code>split</code>.
     * @param tuner     Qualcomm HetCompute pattern tuner object (optional).
     * @return task_ptr Unlaunched task representing pattern's execution.
     */

    template <typename Problem, typename Fn1, typename Fn2, typename Fn3, typename Fn4>
    hetcompute::task_ptr<> pdivide_and_conquer_async(Problem                         p,
                                                   Fn1&&                           is_base,
                                                   Fn2&&                           base,
                                                   Fn3&&                           split,
                                                   Fn4&&                           merge,
                                                   const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
    {
        HETCOMPUTE_UNUSED(tuner);
        return hetcompute::internal::pdivide_and_conquer_async<Problem>(std::forward<Fn1>(is_base),
                                                                      std::forward<Fn2>(base),
                                                                      std::forward<Fn3>(split),
                                                                      std::forward<Fn4>(merge),
                                                                      p);
    }

    /**
     * Parallel divide-and-conquer specialized for no merge of subproblems and not
     * returning a solution, e.g., quicksort.
     *
     * @par Example: Quicksort n random integers
     * @includelineno samples/src/pdivide_and_conquer_quicksort.cc
     *
     * @param p       Problem data structure operated on by the functions.
     * @param is_base Returns TRUE if <code>p</code> is a base case problem,
     *                else FALSE.
     * @param base    Solves a base case problem, but does not return any solution.
     * @param split   Splits <code>p</code> into subproblems and returns them.
     * @param tuner   Qualcomm HetCompute pattern tuner object (optional).
     */
    template <typename Problem, typename Fn1, typename Fn2, typename Fn3>
    void
    pdivide_and_conquer(Problem p, Fn1&& is_base, Fn2&& base, Fn3&& split, const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
    {
        HETCOMPUTE_UNUSED(tuner);
        hetcompute::internal::pdivide_and_conquer(nullptr, p, std::forward<Fn1>(is_base), std::forward<Fn2>(base), std::forward<Fn3>(split));
    }

    /**
     * Asynchronous parallel divide-and-conquer specialized for no merge of
     * subproblems and not returning a solution, e.g., quicksort.
     *
     * Returns a task that represents the pattern's execution. Operations on the
     * task translate into operations on the executing pattern. The caller must
     * launch the task.
     *
     * @param p         Problem data structure operated on by the functions.
     * @param is_base   Returns TRUE if <code>p</code> is a base case problem,
     *                  else FALSE.
     * @param base      Solves a base case problem, but does not return
     *                  any solution.
     * @param split     Splits <code>p</code> into subproblems and returns them.
     * @param tuner     Qualcomm HetCompute pattern tuner object (optional).
     * @return task_ptr unlaunched task representing pattern's execution.
     */

    template <typename Problem, typename Fn1, typename Fn2, typename Fn3>
    hetcompute::task_ptr<> pdivide_and_conquer_async(Problem                         p,
                                                   Fn1&&                           is_base,
                                                   Fn2&&                           base,
                                                   Fn3&&                           split,
                                                   const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
    {
        HETCOMPUTE_UNUSED(tuner);
        return hetcompute::internal::pdivide_and_conquer_async<Problem>(std::forward<Fn1>(is_base),
                                                                      std::forward<Fn2>(base),
                                                                      std::forward<Fn3>(split),
                                                                      p);
    }

    /// @cond
    // Ignore this code fragment
    template <typename Problem, typename Solution, typename Fn1, typename Fn2, typename Fn3, typename Fn4>
    hetcompute::task_ptr<Solution> create_task(const hetcompute::pattern::pdivide_and_conquerer<Fn1, Fn2, Fn3, Fn4>& pdnc,
                                             Problem                                                             p,
                                             const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
    {
        HETCOMPUTE_UNUSED(tuner);
        return hetcompute::internal::pdivide_and_conquer_async<Problem, Solution>(pdnc._is_base_fn,
                                                                                pdnc._base_fn,
                                                                                pdnc._split_fn,
                                                                                pdnc._merge_fn,
                                                                                p);
    }

    template <typename Problem, typename Solution, typename Fn1, typename Fn2, typename Fn3, typename Fn4>
    hetcompute::task_ptr<Solution> launch(const hetcompute::pattern::pdivide_and_conquerer<Fn1, Fn2, Fn3, Fn4>& pdnc,
                                        Problem                                                             p,
                                        const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
    {
        auto t = create_task<Problem, Solution>(pdnc, p, tuner);
        t->launch();
        return t;
    }

    template <typename Problem, typename Fn1, typename Fn2, typename Fn3, typename Fn4>
    hetcompute::task_ptr<> create_task(const hetcompute::pattern::pdivide_and_conquerer<Fn1, Fn2, Fn3, Fn4>& pdnc,
                                     Problem                                                             p,
                                     const hetcompute::pattern::tuner&                                     tuner = hetcompute::pattern::tuner())
    {
        HETCOMPUTE_UNUSED(tuner);
        return hetcompute::internal::pdivide_and_conquer_async(pdnc._is_base_fn, pdnc._base_fn, pdnc._split_fn, pdnc._merge_fn, p);
    }

    template <typename Problem, typename Fn1, typename Fn2, typename Fn3, typename Fn4>
    hetcompute::task_ptr<> launch(const hetcompute::pattern::pdivide_and_conquerer<Fn1, Fn2, Fn3, Fn4>& pdnc,
                                Problem                                                             p,
                                const hetcompute::pattern::tuner&                                     tuner = hetcompute::pattern::tuner())
    {
        auto t = create_task<Problem>(pdnc, p, tuner);
        t->launch();
        return t;
    }
    /// @endcond

    /** @} */ /* end_addtogroup pdnc_doc */

    namespace pattern
    {
        /** @addtogroup pdnc_doc
        @{ */

        template <typename IsBaseFn, typename BaseFn, typename SplitFn, typename MergeFn>
        pdivide_and_conquerer<IsBaseFn, BaseFn, SplitFn, MergeFn>
        create_pdivide_and_conquer(IsBaseFn&& isbase, BaseFn&& base, SplitFn&& split, MergeFn&& merge);

        template <typename IsBaseFn, typename BaseFn, typename SplitFn, typename MergeFn>
        class pdivide_and_conquerer
        {
            using _Problem  = typename internal::function_traits<BaseFn>::template arg_type<0>;
            using _Solution = typename internal::function_traits<BaseFn>::return_type;

        public:
            _Solution run(_Problem& p, const hetcompute::pattern::tuner& pt = hetcompute::pattern::tuner())
            {
                // TODO: Dispatch to different variants based on Solution type (void or not)
                // TODO: How to deal with no MergeFn? Another class?
                HETCOMPUTE_UNUSED(pt);
                return hetcompute::pdivide_and_conquer<_Problem, _Solution>(p,
                                                                          std::forward<IsBaseFn>(_is_base_fn),
                                                                          std::forward<BaseFn>(_base_fn),
                                                                          std::forward<SplitFn>(_split_fn),
                                                                          std::forward<MergeFn>(_merge_fn));
            }

            _Solution operator()(_Problem& p, const hetcompute::pattern::tuner& pt = hetcompute::pattern::tuner()) { return run(p, pt); }

        private:
            IsBaseFn _is_base_fn;
            BaseFn   _base_fn;
            SplitFn  _split_fn;
            MergeFn  _merge_fn;

            pdivide_and_conquerer(IsBaseFn&& isbase, BaseFn&& base, SplitFn&& split, MergeFn&& merge)
                : _is_base_fn(isbase), _base_fn(base), _split_fn(split), _merge_fn(merge)
            {
            }

            friend pdivide_and_conquerer create_pdivide_and_conquer<IsBaseFn, BaseFn, SplitFn, MergeFn>(IsBaseFn&& isbase,
                                                                                                        BaseFn&&   base,
                                                                                                        SplitFn&&  split,
                                                                                                        MergeFn&&  merge);

            template <typename Problem, typename Solution, typename Fn1, typename Fn2, typename Fn3, typename Fn4>
            friend hetcompute::task_ptr<Solution>
            hetcompute::create_task(const pdivide_and_conquerer<Fn1, Fn2, Fn3, Fn4>& pdnc, Problem p, const hetcompute::pattern::tuner& t);

            template <typename Problem, typename Fn1, typename Fn2, typename Fn3, typename Fn4>
            friend hetcompute::task_ptr<>
            hetcompute::create_task(const pdivide_and_conquerer<Fn1, Fn2, Fn3, Fn4>& pdnc, Problem p, const hetcompute::pattern::tuner& t);

            template <typename Code, typename IsPattern, class Enable>
            friend struct hetcompute::internal::task_factory;
        };

        /**
         * Create a pattern object from function objects <code>isbase</code>,
         * <code>base</code>, <code>split</code>, and <code>merge</code>.
         *
         * Returns a pattern object which can be invoked (1) synchronously, using the
         * run method or the () operator with arguments; or (2) asynchronously, using
         * hetcompute::create_task or hetcompute::launch.
         *
         * @par Examples
         * @code
         * // Calculate Fibonacci sequence in parallel
         * auto fib = hetcompute::pattern::create_pdivide_and_conquer(
         *       [](size_t m){ return m <= 1; },
         *       [](size_t m){ return m; },
         *       [](size_t m){return std::vector<size_t>({m - 1, m - 2});},
         *       [](size_t, std::vector<size_t>& sols){return sols[0] + sols[1];}
         *       );
         *
         * size_t input = 10;
         * auto par = fib.run(input);
         * @endcode
         *
         * @param isbase  Indicates if the problem is a base case problem.
         * @param base    Solves a base case problem.
         * @param split   Splits the problem into subproblems and returns them.
         * @param merge   Merges the solutions to subproblems of <code>p</code>
         *                returned by <code>split</code>.
         */

        template <typename IsBaseFn, typename BaseFn, typename SplitFn, typename MergeFn>
        pdivide_and_conquerer<IsBaseFn, BaseFn, SplitFn, MergeFn>
        create_pdivide_and_conquer(IsBaseFn&& isbase, BaseFn&& base, SplitFn&& split, MergeFn&& merge)
        {
            using is_base_fn_traits = internal::function_traits<IsBaseFn>;
            using base_fn_traits    = internal::function_traits<BaseFn>;

            static_assert(is_base_fn_traits::arity::value == 1, "is_base function takes exactly one argument");

            static_assert(std::is_same<typename is_base_fn_traits::return_type, bool>::value, "is_base function must return bool");

            static_assert(std::is_same<typename is_base_fn_traits::template arg_type<0>, typename base_fn_traits::template arg_type<0>>::value,
                          "arguments to is_base and base functions must be of the same type");

            // TODO: static asserts for split and merge compatibility
            return pdivide_and_conquerer<IsBaseFn, BaseFn, SplitFn, MergeFn>(std::forward<IsBaseFn>(isbase),
                                                                             std::forward<BaseFn>(base),
                                                                             std::forward<SplitFn>(split),
                                                                             std::forward<MergeFn>(merge));
        }

        /** @} */ /* end_addtogroup pdnc_doc */

    }; // namespace pattern
};     // namespace hetcompute
