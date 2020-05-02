/** @file preduce.hh */
#pragma once

#include <hetcompute/taskfactory.hh>
#include <hetcompute/tuner.hh>
#include <hetcompute/internal/patterns/preduce-internal.hh>

namespace hetcompute
{
    namespace pattern
    {
        /** @addtogroup preduce_doc
            @{ */

        template <typename Reduce, typename Join>
        class preducer;

        template <typename Reduce, typename Join>
        preducer<Reduce, Join> create_preduce(Reduce&& r, Join&& j);

        template <typename Reduce, typename Join>
        class preducer
        {
        public:
            template <typename T, typename InputIterator>
            T run(InputIterator first, InputIterator last, const T& identity, const hetcompute::pattern::tuner& t = hetcompute::pattern::tuner())
            {
                return internal::preduce_internal(nullptr, first, last, identity, std::forward<Reduce>(_reduce), std::forward<Join>(_join), t);
            }

            template <typename T, typename InputIterator>
            T operator()(InputIterator                   first,
                         InputIterator                   last,
                         const T&                        identity,
                         const hetcompute::pattern::tuner& t = hetcompute::pattern::tuner())
            {
                return run(first, last, identity, t);
            }

        private:
            Reduce _reduce;
            Join   _join;
            preducer(Reduce&& r, Join&& j) : _reduce(r), _join(j) {}
            friend preducer create_preduce<Reduce, Join>(Reduce&& r, Join&& j);
            template <typename T, typename R, typename J, typename... Args>
            friend hetcompute::task_ptr<T> hetcompute::create_task(const preducer<R, J>& p, Args&&... args);
        };

        /**
         * Create pattern object from function objects: <code>Reduce</code>
         * and <code>Join</code>.
         *
         * Returns a pattern object which can be invoked (1) synchronously, using the
         * run method or the () operator with arguments; or (2) asynchronously, using
         * hetcompute::create_task or hetcompute::launch.
         *
         * @par Examples
         * @code
         * // parallel sum over a vector of elements
         * vector<size_t> vec(100, 1);
         * const size_t identity = 0;
         * typedef vector<size_t>::iterator iter_type;
         *
         * auto reduce = [&vec](iter_type it1, iter_type it2, size_t init){
         *                   return std::accumulate(it1, it2, init);
         *                };
         * auto join = std::plus<size_t>();
         * auto preduce = hetcompute::pattern::create_preduce(reduce, join);
         *
         * // result == 100
         * auto result = preduce(vec.begin(), vec.end(), identity);
         * @endcode
         *
         * @param r Function object accumulating result for subrange.
         * @param j Function object combining two subrange results.
         */

        template <typename Reduce, typename Join>
        preducer<Reduce, Join> create_preduce(Reduce&& r, Join&& j)
        {
            return preducer<Reduce, Join>(std::forward<Reduce>(r), std::forward<Join>(j));
        }

        /** @} */ /* end_addtogroup preduce_doc */

    }; // namespace pattern

    /** @addtogroup preduce_doc
        @{ */

    /**
     * Performs parallel reduction by reducing the results using binary operator
     * <code>join</code>. Returns the result of the reduction.
     *
     * @note Qualcomm HetCompute parallel reduction pattern operates in two stages. In the
     *       first stage, it applies the reduction operation (reduce) to a
     *       set of subranges. In the second stage, the reduction
     *       results of all the subranges will be aggregated (join)
     *       into the final result.
     * @note The binary operation is expected to be associative, but not
     *       necessarily commutative, as the algorithm does not swap operands of
     *       the reduce operation while working on the range.
     * @note InputIterator can be either of type RandomAccess Iterators, or
     *       of integral type to represent iteration indices.
     * @note For tiny iteration range and/or trivial binary operator, it may not
     *       be worthwhile to parallelize the reduction operation.
     * @note Reduce function requires pass-by-reference semantics.
     * @note To achieve best performance, it is recommended to implement move
     *       constructor/assignment for user-defined to-be-reduced type.
     *
     * @par Complexity
     * Exactly <code>last-first-1</code> applications of <code>join</code>.
     *
     * @par Examples
     * @code
     * [...]
     * const int identity = 0;
     * // Parallel sum
     * auto p_sum = hetcompute::preduce(0, vin.size(), identity,
     *                            // reduce func
     *                            [&vin](int i, int j, int& init)
     *                            {
     *                              for(int k = i; k < j; ++k)
     *                                init += vin[k];
     *                            },
     *                            // join func
     *                            [](size_t x, size_t y){ return x + y; }
     *                            );
     * @endcode
     *
     * @param first      Start of the range to parallel reduce.
     * @param last       End of the range to parallel reduce.
     * @param identity   A special element leaves other elements unchanged
     *                   when combined with them, i.e.,
     *                   x = identity * x and x = x * identity. For example,
     *                   0 is the identity element under addition for the
     *                   real numbers.
     * @param reduce     Reduce function applied to each subrange and return
     *                   the result.
     * @param join       Join calculated results from two subranges.
     * @param tuner      Qualcomm HetCompute pattern tuner object (optional).
     * @return T         Returns the reduction result of type T.
     */

    template <typename T, class InputIterator, typename Reduce, typename Join>
    T preduce(InputIterator            first,
              InputIterator            last,
              const T&                 identity,
              Reduce&&                 reduce,
              Join&&                   join,
              hetcompute::pattern::tuner tuner = hetcompute::pattern::tuner())
    {
        // computational kernel of preduce is typically trivial, therefore
        // the runtime sets up the default chunk size to a large value
        if (!tuner.is_chunk_set())
        {
            auto chunk_size = hetcompute::internal::static_chunk_size<size_t>(static_cast<size_t>(last - first),
                                                                            hetcompute::internal::num_execution_contexts());
            tuner.set_chunk_size(chunk_size);
        }

        return internal::preduce(nullptr, first, last, identity, std::forward<Reduce>(reduce), std::forward<Join>(join), tuner);
    }

    /**
     * Create an asynchronous task from the <code>hetcompute::preduce</code> pattern.
     *
     * @param first      Start of the range to parallel reduce.
     * @param last       End of the range to parallel reduce.
     * @param identity   A special element leaves other elements unchanged
     *                   when combined with them, i.e.,
     *                   x = identity * x and x = x * identity. For example,
     *                   0 is the identity element under addition for the
     *                   real numbers.
     * @param reduce     Reduce function applied to each subrange and return
     *                   the result.
     * @param join       Join calculated results from two subranges.
     * @param tuner      Qualcomm HetCompute pattern tuner object (optional).
     * @return T         Returns the reduction result of type T.

     */

    template <typename T, class InputIterator, typename Reduce, typename Join>
    hetcompute::task_ptr<T> preduce_async(InputIterator            first,
                                        InputIterator            last,
                                        const T&                 identity,
                                        Reduce&&                 reduce,
                                        Join&&                   join,
                                        hetcompute::pattern::tuner tuner = hetcompute::pattern::tuner())
    {
        if (!tuner.is_chunk_set())
        {
            tuner.set_chunk_size(hetcompute::internal::static_chunk_size<size_t>(static_cast<size_t>(last - first),
                                                                               hetcompute::internal::num_execution_contexts()));
        }
        return hetcompute::internal::preduce_async(std::forward<Reduce>(reduce), std::forward<Join>(join), first, last, identity, tuner);
    }

    /**
     * \brief Perform parallel reduction on some container c.
     *
     * A convenient API to use for applying parallel reduction on
     * a passed-in container.
     *
     * @note1 Container must have size() defined and indexable with operator []
     *
     * \see T preduce(InputIterator, InputIterator, const T&, Reduce&&, Join&&)
     *
     * @par Examples
     * @code
     * [...]
     * vector<int> vin;
     * // Initialize vin
     * [...]
     * const int identity = 0;
     * // Parallel sum
     * auto p_sum = hetcompute::preduce(vin, identity, std::plus<int>());
     * @endcode
     *
     * @param c          Container on which parallel reduce is applied.
     * @param identity   A special element leaves other elements unchanged
     *                   when combined with them, i.e.,
     *                   x = identity * x and x = x * identity. For example,
     *                   0 is the identity element under addition for the
     *                   real numbers.
     * @param join       Join calculated results from two subranges.
     * @param tuner      Qualcomm HetCompute pattern tuner object (optional).
     * @return T         Returns the reduction result of type T.
     */

    template <typename T, typename Container, typename Join>
    T preduce(Container& c, const T& identity, Join&& join, hetcompute::pattern::tuner tuner = hetcompute::pattern::tuner())
    {
        if (!tuner.is_chunk_set())
        {
            tuner.set_chunk_size(hetcompute::internal::static_chunk_size<size_t>(c.size(), hetcompute::internal::num_execution_contexts()));
        }

        return internal::preduce(nullptr, c, identity, std::forward<Join>(join), tuner);
    }

    /**
     * Create an asynchronous task from the <code>hetcompute::preduce</code> pattern.
     *
     * @param c          Container on which parallel reduce is applied.
     * @param identity   a special element leaves other elements unchanged
     *                   when combined with them, i.e.,
     *                   x = identity * x and x = x * identity. For example,
     *                   0 is the identity element under addition for the
     *                   real numbers.
     * @param join       Join calculated results from two subranges.
     * @param tuner      Qualcomm HetCompute pattern tuner object (optional).
     * @return T         Returns the reduction result of type T.
     */

    template <typename T, typename Container, typename Join>
    hetcompute::task_ptr<T>
    preduce_async(Container& c, const T& identity, Join&& join, hetcompute::pattern::tuner tuner = hetcompute::pattern::tuner())
    {
        if (!tuner.is_chunk_set())
        {
            tuner.set_chunk_size(hetcompute::internal::static_chunk_size<size_t>(c.size(), hetcompute::internal::num_execution_contexts()));
        }

        return hetcompute::internal::preduce_async(std::forward<Join>(join), c, identity, tuner);
    }

    /**
     * \brief Perform parallel reduction on range defined by iterators.
     *
     * A convenient API to apply parallel reduction on a range specified
     * by iterators.
     *
     * @note1 Developer must ensure the validity of iterators.
     *
     * \see T preduce(InputIterator, InputIterator, const T&, Reduce&&, Join&&)
     *
     * @par Examples
     * @code
     * [...]
     * vector<int> vin;
     * // Initialize vector
     * [...]
     * const int identity = 0;
     * // Parallel sum
     * auto p_sum = hetcompute::preduce(vin.begin(), vin.end(), identity, std::plus<int>());
     * @endcode
     *
     * @param first      Iterator to range start.
     * @param last       Iterator to range end.
     * @param identity   A special element leaves other elements unchanged
     *                   when combined with them, i.e.,
     *                   x = identity * x and x = x * identity. For example,
     *                   0 is the identity element under addition for the
     *                   real numbers.
     * @param join       Join calculated results from two subranges.
     * @param tuner      Qualcomm HetCompute pattern tuner object (optional).
     * @return T         Returns the reduction result of type T.
     */

    template <typename T, typename Iterator, typename Join>
    T preduce(Iterator first, Iterator last, const T& identity, Join&& join, hetcompute::pattern::tuner tuner = hetcompute::pattern::tuner())
    {
        if (!tuner.is_chunk_set())
        {
            tuner.set_chunk_size(hetcompute::internal::static_chunk_size<size_t>(static_cast<size_t>(last - first),
                                                                               hetcompute::internal::num_execution_contexts()));
        }

        return internal::preduce(nullptr, first, last, identity, std::forward<Join>(join), tuner);
    }

    /**
     * Create an asynchronous task from the <code>hetcompute::preduce</code> pattern.
     *
     * @param first      Iterator to range start.
     * @param last       Iterator to range end.
     * @param identity   A special element leaves other elements unchanged
     *                   when combined with them, i.e.,
     *                   x = identity * x and x = x * identity. For example,
     *                   0 is the identity element under addition for the
     *                   real numbers.
     * @param join       Join calculated results from two subranges.
     * @param tuner      Qualcomm HetCompute pattern tuner object (optional.)
     * @return T         Returns the reduction result of type T.
     */

    template <typename T, typename Iterator, typename Join>
    hetcompute::task_ptr<T>
    preduce_async(Iterator first, Iterator last, const T& identity, Join&& join, hetcompute::pattern::tuner tuner = hetcompute::pattern::tuner())
    {
        if (!tuner.is_chunk_set())
        {
            tuner.set_chunk_size(hetcompute::internal::static_chunk_size<size_t>(static_cast<size_t>(last - first),
                                                                               hetcompute::internal::num_execution_contexts()));
        }

        return hetcompute::internal::preduce_async(std::forward<Join>(join), first, last, identity, tuner);
    }

    /// @cond
    // Ignore this code fragment
    template <typename T, typename Reduce, typename Join, typename... Args>
    hetcompute::task_ptr<T> create_task(const hetcompute::pattern::preducer<Reduce, Join>& pr, Args&&... args)
    {
        return hetcompute::internal::preduce_async(pr._reduce, pr._join, args...);
    }

    template <typename T, typename Reduce, typename Join, typename... Args>
    hetcompute::task_ptr<T> launch(const hetcompute::pattern::preducer<Reduce, Join>& pr, Args&&... args)
    {
        auto t = hetcompute::create_task<T>(pr, args...);
        t->launch();
        return t;
    }
    /// @endcond

    /** @} */ /* end_addtogroup preduce_doc */

}; // namespace hetcompute
