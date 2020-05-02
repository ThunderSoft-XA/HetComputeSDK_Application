/** @file pfor_each.hh */
#pragma once

#include <hetcompute/taskfactory.hh>
#include <hetcompute/tuner.hh>
#include <hetcompute/internal/patterns/pfor-each-internal.hh>
#include <hetcompute/internal/util/templatemagic.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace pointkernel
        {
            // forward declaration
            template <typename RT, typename... Args>
            class pointkernel;
        }; // namespace pointkernel

    }; // namespace internal

    namespace pattern
    {
        /** @addtogroup pfor_each_doc
            @{ */

        template <typename T1, typename T2>
        class pfor;

        template <typename UnaryFn>
        pfor<UnaryFn, void> create_pfor_each(UnaryFn&& fn);
    }; // namespace pattern

    namespace beta
    {
        namespace pattern
        {
            template <typename KernelTuple, typename ArgTuple>
            hetcompute::pattern::pfor<KernelTuple, ArgTuple> create_pfor_each_helper(KernelTuple&& ktpl, ArgTuple&& atpl);

        }; // namespace pattern
    }; // namespace beta

    namespace pattern
    {
        template <typename T1, typename T2>
        class pfor
        {
        public:
            template <size_t Dims>
            void run(const hetcompute::range<Dims>& r, hetcompute::pattern::tuner& t = hetcompute::pattern::tuner().set_cpu_load(100))
            {
                hetcompute::internal::pfor_each_run_helper(this,
                                                         r,
                                                         _ktpl,
                                                         t,
                                                         typename hetcompute::internal::integer_list<std::tuple_size<decltype(_atpl)>::value>::type(),
                                                         _atpl);
            }

            template <size_t Dims>
            void operator()(const hetcompute::range<Dims>& r, hetcompute::pattern::tuner& t = hetcompute::pattern::tuner().set_cpu_load(100))
            {
                run<Dims>(r, t);
            }

            // return relative gpu profile to cpu (normalized as 1.0)
            double query_gpu_profile() const { return _gpu_profile; }

            // return relative dsp profile to cpu (normalized as 1.0)
            double query_dsp_profile() const { return _dsp_profile; }

            // return absolute exec time (in microsec) of cpu task
            uint64_t get_cpu_task_time() const { return _cpu_task_time; }

            // return absolute exec time (in microsec) of gpu task
            uint64_t get_gpu_task_time() const { return _gpu_task_time; }

            // return absolute exec time (in microsec) of dsp task
            uint64_t get_dsp_task_time() const { return _dsp_task_time; }

#ifndef _MSC_VER
            // MSVC is not friend with friends, so we make the data members public
        private:
            template <typename KT, typename AT>
            friend pfor<KT, AT> hetcompute::beta::pattern::create_pfor_each_helper(KT&& ktpl, AT&& atpl);

            template <size_t Dims, typename KernelTuple, typename ArgTuple, typename KernelFirst, typename... KernelRest, typename... Args, typename Boolean, typename Buf_Tuple>
            friend void hetcompute::internal::pfor_each(hetcompute::pattern::pfor<KernelTuple, ArgTuple>* const p,
                                                        const hetcompute::range<Dims>&                          r,
                                                        std::tuple<KernelFirst, KernelRest...>&                 klist,
                                                        hetcompute::pattern::tuner&                             tuner,
                                                        const Boolean                                           called_with_pointkernel,
                                                        Buf_Tuple&&                                             buf_tup,
                                                        Args&&... args);
#else  // _MSC_VER
        public:
#endif // _MSC_VER

            T1       _ktpl;
            T2       _atpl;
            double   _gpu_profile;
            double   _dsp_profile;
            uint64_t _cpu_task_time;
            uint64_t _gpu_task_time;
            uint64_t _dsp_task_time;
            size_t   _num_runs;

            pfor(T1&& ktpl, T2&& atpl)
                : _ktpl(ktpl),
                  _atpl(atpl),
                  _gpu_profile(0),
                  _dsp_profile(0),
                  _cpu_task_time(0),
                  _gpu_task_time(0),
                  _dsp_task_time(0),
                  _num_runs(0)
            {
            }

            // profile based on moving average
            // @param gp relative gpu profile to cpu (1.0) for the current run
            void set_gpu_profile(double gp) { _gpu_profile = (_gpu_profile * _num_runs + gp) / static_cast<double>(_num_runs + 1); }

            // profile based on moving average
            // @param dp relative dsp profile to cpu (1.0) for the current run
            void set_dsp_profile(double dp) { _dsp_profile = (_dsp_profile * _num_runs + dp) / static_cast<double>(_num_runs + 1); }

            // @param absolute exec time for cpu task in microsec
            void set_cpu_task_time(uint64_t ct) { _cpu_task_time = ct; }

            // @param absolute exec time for gpu task in microsec
            void set_gpu_task_time(uint64_t gt) { _gpu_task_time = gt; }

            // @param absolute exec time for dsp task in microsec
            void set_dsp_task_time(uint64_t ht) { _dsp_task_time = ht; }

            // update number of runs
            void add_run() { _num_runs++; }
        };

        template <typename RT, typename... PKType, typename T2>
        class pfor<hetcompute::internal::pointkernel::pointkernel<RT, PKType...>, T2>
        {
            using pointkernel_type = hetcompute::internal::pointkernel::pointkernel<RT, PKType...>;

        public:
            template <size_t Dims>
            void run(const hetcompute::range<Dims>& r, hetcompute::pattern::tuner& t = hetcompute::pattern::tuner().set_cpu_load(100))
            {
                hetcompute::internal::pfor_each_run_helper(this,
                                                         r,
                                                         _pk,
                                                         t,
                                                         typename hetcompute::internal::integer_list<std::tuple_size<decltype(_atpl)>::value>::type(),
                                                         _atpl);
            }

            template <size_t Dims>
            void operator()(const hetcompute::range<Dims>& r, hetcompute::pattern::tuner& t = hetcompute::pattern::tuner().set_cpu_load(100))
            {
                run<Dims>(r, t);
            }

            // return relative gpu profile to cpu (normalized as 1.0)
            double query_gpu_profile() const { return _gpu_profile; }

            // return relative dsp profile to cpu (normalized as 1.0)
            double query_dsp_profile() const { return _dsp_profile; }

            // return absolute exec time (in microsec) of cpu task
            uint64_t get_cpu_task_time() const { return _cpu_task_time; }

            // return absolute exec time (in microsec) of gpu task
            uint64_t get_gpu_task_time() const { return _gpu_task_time; }

            // return absolute exec time (in microsec) of dsp task
            uint64_t get_dsp_task_time() const { return _dsp_task_time; }

#ifndef _MSC_VER
            // As noted by @hanzhao, MSVC does not like friends, so we make the data members public
        private:
            template <typename RetType, typename... PKArgs, typename ArgTuple>
            friend pfor<hetcompute::internal::pointkernel::pointkernel<RetType, PKArgs...>, ArgTuple>
            hetcompute::beta::pattern::create_pfor_each_helper(hetcompute::internal::pointkernel::pointkernel<RetType, PKArgs...>& pk,
                                                             ArgTuple&&                                                        atpl);

            template <size_t Dims, typename KernelTuple, typename ArgTuple, typename KernelFirst, typename... KernelRest, typename... Args, typename Boolean, typename Buf_Tuple>
            friend void hetcompute::internal::pfor_each(hetcompute::pattern::pfor<KernelTuple, ArgTuple>* const p,
                                                        const hetcompute::range<Dims>&                          r,
                                                        std::tuple<KernelFirst, KernelRest...>&                 klist,
                                                        hetcompute::pattern::tuner&                             tuner,
                                                        const Boolean                                           called_with_pointkernel,
                                                        Buf_Tuple&&                                             buf_tup,
                                                        Args&&... args);
#else  // _MSC_VER
        public:
#endif // _MSC_VER
            pointkernel_type& _pk;
            T2                _atpl;
            double            _gpu_profile;
            double            _dsp_profile;
            uint64_t          _cpu_task_time;
            uint64_t          _gpu_task_time;
            uint64_t          _dsp_task_time;
            size_t            _num_runs;

            pfor(pointkernel_type& pk, T2&& atpl)
                : _pk(pk),
                  _atpl(atpl),
                  _gpu_profile(0),
                  _dsp_profile(0),
                  _cpu_task_time(0),
                  _gpu_task_time(0),
                  _dsp_task_time(0),
                  _num_runs(0)
            {
            }

            // profile based on moving average
            // @param gp relative gpu profile to cpu (1.0) for the current run
            void set_gpu_profile(double gp) { _gpu_profile = (_gpu_profile * _num_runs + gp) / static_cast<double>(_num_runs + 1); }

            // profile based on moving average
            // @param dp relative dsp profile to cpu (1.0) for the current run
            void set_dsp_profile(double dp) { _dsp_profile = (_dsp_profile * _num_runs + dp) / static_cast<double>(_num_runs + 1); }

            // @param absolute exec time for cpu task in microsec
            void set_cpu_task_time(uint64_t ct) { _cpu_task_time = ct; }

            // @param absolute exec time for gpu task in microsec
            void set_gpu_task_time(uint64_t gt) { _gpu_task_time = gt; }

            // @param absolute exec time for dsp task in microsec
            void set_dsp_task_time(uint64_t ht) { _dsp_task_time = ht; }

            // update number of runs
            void add_run() { _num_runs++; }
        };

        template <typename T1>
        class pfor<T1, void>
        {
        public:
            template <typename InputIterator>
            void run(InputIterator first, InputIterator last, const hetcompute::pattern::tuner& t = hetcompute::pattern::tuner())
            {
                hetcompute::internal::pfor_each_internal(nullptr, first, last, std::forward<T1>(_fn), 1, t);
            }

            template <typename InputIterator>
            void operator()(InputIterator first, InputIterator last, const hetcompute::pattern::tuner& t = hetcompute::pattern::tuner())
            {
                run(first, last, t);
            }

            // can dispatch to cpu or gpu
            template <typename InputIterator>
            void
            run(InputIterator first, const size_t stride, InputIterator last, const hetcompute::pattern::tuner& t = hetcompute::pattern::tuner())
            {
                hetcompute::internal::pfor_each_internal(nullptr, first, last, std::forward<T1>(_fn), stride, t);
            }

            template <typename InputIterator>
            void operator()(InputIterator                   first,
                            const size_t                    stride,
                            InputIterator                   last,
                            const hetcompute::pattern::tuner& t = hetcompute::pattern::tuner())
            {
                run(first, stride, last, t);
            }

            // can dispatch to cpu or gpu
            template <size_t Dims>
            void run(const hetcompute::range<Dims>& r, const hetcompute::pattern::tuner& t = hetcompute::pattern::tuner())
            {
                hetcompute::internal::pfor_each_internal(nullptr, r, std::forward<T1>(_fn), 1, t);
            }

            template <size_t Dims>
            void operator()(const hetcompute::range<Dims>& r, const hetcompute::pattern::tuner& t = hetcompute::pattern::tuner())
            {
                run<Dims>(r, t);
            }

#ifndef _MSC_VER
            // MSVC is not friend with friends, so we make the data members public
        private:
            friend pfor create_pfor_each<T1>(T1&& fn);

            template <typename Fn, typename... Args>
            friend hetcompute::task_ptr<void()> hetcompute::create_task(const pfor<Fn, void>& pf, Args&&... args);

#else  // _MSC_VER
        public:
#endif // _MSC_VER
            T1 _fn;
            explicit pfor(T1&& fn) : _fn(fn) {}
        };

        /**
         * Create a pattern object from function object <code>fn</code>
         *
         * Returns a pattern object which can be invoked (1) synchronously, using the
         * run method or the () operator with arguments; or (2) asynchronously, using
         * hetcompute::create_task or hetcompute::launch
         *
         * @par Examples
         * @code
         * auto l = [](size_t) {};
         * auto pfor = hetcompute::pattern::create_pfor_each(l);
         * pfor(0, 100);
         * @endcode
         *
         * @param fn   Function object apply to range.
         */

        template <typename UnaryFn>
        pfor<UnaryFn, void> create_pfor_each(UnaryFn&& fn)
        {
            // No longer true for gpu fn
            // TODO: api20 static assert for both cpu and gpu
            // using traits = internal::function_traits<UnaryFn>;
            // static_assert(traits::arity == 1,
            //     "pfor_each takes a function accepting one argument");

            return pfor<UnaryFn, void>(std::forward<UnaryFn>(fn));
        }

        /** @} */ /* end_addtogroup pfor_each_doc */

    }; // namespace pattern

    /** @addtogroup pfor_each_doc
        @{ */
    namespace beta
    {
        namespace pattern
        {
            template <typename KernelTuple, typename ArgTuple>
            hetcompute::pattern::pfor<KernelTuple, ArgTuple> create_pfor_each_helper(KernelTuple&& ktpl, ArgTuple&& atpl)
            {
                return hetcompute::pattern::pfor<KernelTuple, ArgTuple>(std::forward<KernelTuple>(ktpl), std::forward<ArgTuple>(atpl));
            }

            template <typename RT, typename... PKArgs, typename ArgTuple>
            hetcompute::pattern::pfor<hetcompute::internal::pointkernel::pointkernel<RT, PKArgs...>, ArgTuple>
            create_pfor_each_helper(hetcompute::internal::pointkernel::pointkernel<RT, PKArgs...>& pk, ArgTuple&& atpl)
            {
                return hetcompute::pattern::pfor<hetcompute::internal::pointkernel::pointkernel<RT, PKArgs...>, ArgTuple>(pk,
                                                                                                                      std::forward<ArgTuple>(
                                                                                                                          atpl));
            }

            /**
             * Create a heterogeneous pfor object from type <code>T</code> and arguments
             * <code>args</code>,
             * where: T may be a kernel tuple, or
             *  a pointkernel object.
             *
             * Returns a heterogeneous pfor object which can be invoked synchronously, using
             * the run method or () operator with arguments. Asynchronous launch of the heterogeneous
             * pfor pattern will be provided in future version of HETCOMPUTE.
             *
             * @note If <code>T</code> is a kernel tuple, the kernels can be specified in any combination,
             *       but at most one kernel per device type is allowed.
             * @note Write from random input index to random output index is not supported,
             *       Multiple output buffers are not supported neither. Computation over
             *       index i of input buffer(s) must write result to index i of output buffer.
             * @note If <code>hetcompute::pattern::tuner</code> set_serial is specified,
             *       loop execution will be serialized on CPU. Therefore, developers must
             *       provide a valid CPU kernel if HETCOMPUTE tuner is set to serial execution mode.
             *
             * @par Examples
             * @code
             * [...]
             * // Heterogeneous pfor_each example
             * static void cpu_kernel_2d(const int first, const int stride, const int last,
             *                           hetcompute::buffer_ptr<const int> input,
             *                           hetcompute::buffer_ptr<int> output,
             *                           const int width)
             * {
             *   for (int i = first; i < last; i += stride) {
             *     for (int j = 0; j < width; ++j) {
             *       output[i * width + j] = 2 * input[i * width + j];
             *     }
             *   }
             * }
             *
             * #define OCL_KERNEL(name, k) std::string const name##_string = #k
             * OCL_KERNEL(gpu_kernel_2d,
             *  __kernel void gpu_test_2d(__constant int* input,
             *                            __global int* output,
             *                            int width) {
             *    unsigned int i = get_global_id(0);
             *
             *    for (int j = 0; j < width; ++j) {
             *      *(output + i * width + j) = 2 * input[i * width + j];
             *    }
             *  });
             *
             * // declare in .idl file and implemented in .c
             * int hetcompute_dsp_hex_kernel_2d(const int first, const int stride, const int last,
             *                                const int* input, int inputlen,
             *                                int* output, int outputlen,
             *                                const int width)
             * {
             *   int i, j;
             *   for (i = first; i < last; i += stride) {
             *     for (j = 0; j < width; ++j) {
             *       output[i * width + j] = 2 * input[i * width + j];
             *     }
             *   }
             *
             *   return 0;
             * }
             *
             *  size_t height = 1024, width = 1024;
             *  size_t sz = height * width;
             *  // vptr is the pointer points to main-memory backing store
             *  auto input_buffer = hetcompute::create_buffer<const int>(vptr, sz, true, {hetcompute::cpu, hetcompute::dsp});
             *  auto output_buffer = hetcompute::create_buffer<int>(sz, true, {hetcompute::cpu});
             *
             *  auto r = hetcompute::range<2>(height, width);
             *
             *  auto cl_kernel = hetcompute::create_gpu_kernel<hetcompute::in<hetcompute::buffer_ptr<const int>>,
             *                                           hetcompute::out<hetcompute::buffer_ptr<int>>,
             *                                           int>(gpu_kernel_2d_string, "gpu_test_2d");
             *  auto hex_kernel = hetcompute::create_dsp_kernel<>(hetcompute_dsp_hex_kernel_2d);
             *
             *  hetcompute::pattern::tuner tuner = hetcompute::pattern::tuner().set_cpu_load(30).set_gpu_load(30).set_dsp_load(40);
             *
             *  auto pfor = hetcompute::beta::pattern::create_pfor_each
             *                (std::make_tuple(cpu_range_2d, cl_kernel, hex_kernel), input_buffer, output_buffer, width);
             *  pfor(r, tuner);
             *
             * [...]
             * @endcode
             */

            template <typename T, typename... Args>
            auto create_pfor_each(T&& type, Args&&... args)
                -> decltype(create_pfor_each_helper(std::forward<T>(type),
                                                    std::forward<decltype(std::forward_as_tuple(args...))>(std::forward_as_tuple(args...))))
            {
                auto arg_tuple = std::forward_as_tuple(args...);

                return create_pfor_each_helper(std::forward<T>(type), std::forward<decltype(arg_tuple)>(arg_tuple));
            }

        }; // namespace pattern
    }; // namespace beta

    /** @} */ /* end_addtogroup pfor_each_doc */

    /** @addtogroup pfor_each_doc
        @{ */

    /**
     * Create an asynchronous task from the <code>hetcompute::pfor_each</code> pattern.
     *
     * Returns a task that represents the pattern's execution. Operations on the
     * task translate into operations on the executing pattern. The caller must
     * launch the task. This API has a default stride size of one.
     *
     * @param first     Start of the range to which to apply <code>fn</code>.
     * @param last      End of the range to which to apply <code>fn</code>.
     * @param fn        Unary function object to be applied.
     * @param tuner     Qualcomm HETCOMPUTE pattern tuner object (optional).
     * @return task_ptr Unlaunched task representing pattern's execution.
     */
    template <class InputIterator, typename UnaryFn>
    hetcompute::task_ptr<void()>
    pfor_each_async(InputIterator first, InputIterator last, UnaryFn fn, const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
    {
        return hetcompute::internal::pfor_each_async(fn, first, last, 1, tuner);
    }

    /**
     * Create an asynchronous task from the <code>hetcompute::pfor_each</code> pattern
     * (with step size).
     *
     * @param first     start of the range to which to apply <code>fn</code>.
     * @param stride    step size.
     * @param last      end of the range to which to apply <code>fn</code>.
     * @param fn        unary function object to be applied.
     * @param tuner     Qualcomm HETCOMPUTE pattern tuner object (optional)
     * @return task_ptr unlaunched task representing pattern's execution
     */
    template <class InputIterator, typename UnaryFn>
    hetcompute::task_ptr<void()> pfor_each_async(InputIterator                   first,
                                               const size_t                    stride,
                                               InputIterator                   last,
                                               UnaryFn                         fn,
                                               const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
    {
        return hetcompute::internal::pfor_each_async(fn, first, last, stride, tuner);
    }

    /**
     * Create an asynchronous task from the <code>hetcompute::pfor_each</code> pattern.
     *
     * Returns a task that represents the pattern's execution. Operations on the
     * task translate into operations on the executing pattern. The caller must
     * launch the task. This API has a default step size of one.
     * @param r         range object (1D, 2D or 3D) representing the iteration space.
     * @param fn        unary function object to be applied.
     * @param tuner     HETCOMPUTE pattern tuner object (optional)
     * @return task_ptr unlaunched task representing pattern's execution
     */
    template <size_t Dims, typename UnaryFn>
    hetcompute::task_ptr<void()>
    pfor_each_async(const hetcompute::range<Dims>& r, UnaryFn fn, const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
    {
        return hetcompute::internal::pfor_each_async(fn, r, 1, tuner);
    }

    /**
     * Create an asynchronous task from the <code>hetcompute::pfor_each</code> pattern
     * (with step size).
     *
     * @param r         range object (1D, 2D or 3D) representing the iteration space.
     * @param stride    step size.
     * @param fn        unary function object to be applied.
     * @param tuner     HETCOMPUTE pattern tuner object (optional)
     * @return task_ptr unlaunched task representing pattern's execution
     */
    template <size_t Dims, typename UnaryFn>
    hetcompute::task_ptr<void()> pfor_each_async(const hetcompute::range<Dims>&    r,
                                               const size_t                    stride,
                                               UnaryFn                         fn,
                                               const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
    {
        return hetcompute::internal::pfor_each_async(fn, r, stride, tuner);
    }

    /// @cond
    // Ignore this code fragment
    template <typename Fn, typename... Args>
    hetcompute::task_ptr<void()> create_task(const hetcompute::pattern::pfor<Fn, void>& pf, Args&&... args)
    {
        return hetcompute::internal::pfor_each_async(pf._fn, args...);
    }

    template <typename Fn, typename... Args>
    hetcompute::task_ptr<void()> launch(const hetcompute::pattern::pfor<Fn, void>& pf, Args&&... args)
    {
        auto t = hetcompute::create_task(pf, args...);
        t->launch();
        return t;
    }
    /// @endcond

    /**
     * Parallel version of <code>std::for_each</code>.
     *
     * Applies function object <code>fn</code> in parallel to every iterator in
     * the range [first, last). It has a default step size of one.
     *
     * @note An "iterator" refers to an object that enables a programmer to
     * traverse a container. It can be indices of integral type, or pointers of
     * RandomAccessIterator type.
     *
     * @note In contrast to <code>std::for_each</code> and
     * <code>ptransform</code>, the iterator is passed to the function,
     * instead of the element.
     *
     * It is permissible to modify the elements of the range from <code>fn</code>,
     * provided that <code>InputIterator</code> is a mutable iterator.
     *
     * @note This function returns only after <code>fn</code> has been applied
     * to the whole iteration range.
     *
     * @note The usual rules for cancellation apply, i.e.,
     * within <code>fn</code> the cancellation must be acknowledged using
     * <code>abort_on_cancel</code>.
     *
     * @par Complexity
     * Exactly <code>std::distance(first,last)</code> applications of
     * <code>fn</code>.
     *
     * @sa ptransform(InputIterator, InputIterator, UnaryFn)
     * @sa abort_on_cancel()
     *
     * @par Examples
     * @code
     * [...]
     * // Parallel for-loop using indices
     * pfor_each(size_t(0), vin.size(),
     *           [=,&vin] (size_t i) {
     *             vin[i] = 2 * vin[i];
     *           });
     * [...]
     * @endcode
     *
     * @param first  Start of the range to which to apply <code>fn</code>.
     * @param last   End of the range to which to apply <code>fn</code>.
     * @param fn     Unary function object to be applied.
     * @param t      Qualcomm HETCOMPUTE pattern tuner object (optional)
     */
    template <class InputIterator, typename UnaryFn>
    void pfor_each(InputIterator first, InputIterator last, UnaryFn&& fn, const hetcompute::pattern::tuner& t = hetcompute::pattern::tuner())
    {
        internal::pfor_each(nullptr, first, last, std::forward<UnaryFn>(fn), 1, t);
    }
    /**
     * Parallel version of <code>std::for_each</code> with step size.
     *
     * Applies function object <code>fn</code> in parallel to every iterator
     * in the range [first, last) with step size defined by the stride parameter.
     *
     * @note The function object will be applied to iterators with an
     * incremental step size (iter+=stride)
     *
     * @param first  start of the range to which to apply <code>fn</code>.
     * @param stride step size.
     * @param last   end of the range to which to apply <code>fn</code>.
     * @param fn     unary function object to be applied.
     *               set to one by default.
     * @param t      Qualcomm HETCOMPUTE pattern tuner object (optional)
     */
    template <class InputIterator, typename UnaryFn>
    void pfor_each(InputIterator                   first,
                   const size_t                    stride,
                   InputIterator                   last,
                   UnaryFn&&                       fn,
                   const hetcompute::pattern::tuner& t = hetcompute::pattern::tuner())
    {
        internal::pfor_each(nullptr, first, last, std::forward<UnaryFn>(fn), stride, t);
    }

    /**
     * \brief Parallel version of <code>std::for_each</code>.
     *
     * Instead of passing in a pair of iterators, this form accepts a
     * <code>hetcompute::range</code> object. Internally the indices are linearized
     * before passing to the kernel function. It has a default step size
     * of one.
     *
     * @param r      Range object (1D, 2D or 3D) representing the iteration space.
     * @param fn     Unary function object to be applied.
     * @param t      Qualcomm HETCOMPUTE pattern tuner object (optional).
     */
    template <size_t Dims, typename UnaryFn>
    void pfor_each(const hetcompute::range<Dims>& r, UnaryFn&& fn, const hetcompute::pattern::tuner& t = hetcompute::pattern::tuner())
    {
        internal::pfor_each(nullptr, r, std::forward<UnaryFn>(fn), t);
    }

    /** @} */ /* end_addtogroup pfor_each_doc */
}; // namespace hetcompute
