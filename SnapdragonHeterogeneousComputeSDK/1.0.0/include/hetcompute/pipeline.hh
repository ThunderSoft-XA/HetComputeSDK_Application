/** @file pipeline.hh */
#pragma once

// Include user interface headers
#include <hetcompute/pipelinedata.hh>

// Include internal headers
#ifdef HETCOMPUTE_CAPI_COMPILING_CC
#include <lib/capi.hh>
#endif
#include <hetcompute/internal/patterns/pipeline/pipeline.hh>
#include <hetcompute/internal/patterns/pipeline/pipelineutility.hh>

namespace hetcompute
{
    // ---------------------------------------------------------------------------
    // USER API
    // ---------------------------------------------------------------------------

    /////////////////////
    // pipeline creation

    //  Pipeline characteristics:
    //  1. Linear structure of stages: source -> s1 - > s2 -> s3 -> sink
    //     single output for the source
    //     single intput for the sink
    //     single input, single output for each stage, except the source and the sink
    //
    //  2. Each stage can have different number iterations; however, the iteration
    //     match rate between two consecutive stages should be specified in the
    //     succ stage as:
    //     hetcompute::iteration_rate(num_iterations_pred, num_iterations_succ)
    //     which means that num_iterations_pred iterations in the prev stage will
    //     launch num_iterations_succ iterations in the succ stage.
    //
    //     The following inequality holds all the time in a pipeline:
    //     For any two consecutive stages, prev and succ, which iteration matches the rate
    //     of (n, m) :
    //     num_iterations_finished_pred        num_iterations_finished_succ
    //    ------------------------------  >=  ------------------------------
    //                   n                                   m
    //
    //  3. Two types of stages: serial and parallel
    //     The stage type is specified by hetcompute::serail_stage() or
    //     hetcompute::parallel_stage() when adding the stage.
    //     Parallel stage can specify the degree of concurrency in
    //     hetcompute::parallel_stage(degree_of_concurrency).
    //     degree of concurrency specifies the maximum number of consecutive stage
    //     iterations that can run in parallel.
    //
    //     degree of concurrency has to be non-zero positive natural number.
    //     Note that when degree of concurrency = 1, this means that the stages will
    //     behave as a serial stage.
    //
    //  4. A stage can have "lags" against its previous stage.
    //     With the notion of lag, the following inequality holds all the time in
    //     a pipeline:
    //     For any two consecutive stages, prev and succ, which iteration matches the rate
    //     of (n, m) :
    //     num_iterations_finished_pred      num_iterations_finished_succ + lag
    //  ------------------------------ >= ------------------------------------
    //                n                                 m
    //
    //  5. Stages must subsequently be added with prior to launch
    //
    //  6. The skeleton of a launched pipeline cannot be changed once the pipeline
    //     is launched

    /** @cond PRIVATE */
    /// Hide forward declarations from
    namespace internal
    {
        namespace test
        {
            class pipeline_tester;
            class pipeline_capi_internal_tester;
        }; // namespace test
    };     // namespace internal
    /** @endcond */

    class serial_stage;
    class parallel_stage;
    class iteration_lag;
    class iteration_rate;

    namespace pattern
    {
        /** @addtogroup pipeline_doc
            @{ */
        /**
         * @brief Pipeline class.
         *
         * Pipeline class.
         *
         * @tparam UserData The type for the pipeline context data or empty,
         *                  i.e., <code> hetcompute::pattern::pipeline<size_t></code> or
         *                        <code> hetcompute::pattern::pipeline<></code>.
         */
        template <typename... UserData>
        class pipeline
        {
            /** @cond PRIVATE */
        protected:
            // pointer to the pipeline skeleton
            hetcompute::internal::hetcompute_shared_ptr<hetcompute::internal::pipeline_skeleton<UserData...>> _skeleton;

            static constexpr size_t userdata_size = sizeof...(UserData);
            static_assert(userdata_size <= 1, "HetCompute pipeline can only have, at most, one type of context data.");
            /** @endcond */

            /**
             * @brief Add a cpu stage.
             *
             * Add a cpu stage.
             *
             * @param args The features of the stage which should be the following:
             *  <code>hetcompute::serial_stage</code> or <code>hetcompute::parallel_stage</code>;
             *  <code>hetcompute::iteration_lag</code> (at most one);
             *  <code>hetcompute::iteration_rate</code> (at most one);
             *  <code>hetcompute::sliding_window_size</code> (at most one);
             *  one lambda/functor/function pointer as the function for the cpu stage
             *  (must be the last argument).
             *  <code>hetcompute::pattern::tuner</code> (at most one)
             *  Tunable parameter for pipeline CPU stages: <code>min_chunk_size</code>.
             *  Here <code>min_chunk_size</code> is the number of stage iterations that can be
             *  fused to execute as a chunk to reduce scheduling overhead.
             *  On the other hand, while increasing granuality,
             *  iteration fusion can reduce level of parallelism.
             *  For parallel stages, tuner's <code>min_chunk_size</code>
             *  should never be greater than the stage's degree of concurrency.
             *  Tuner is designed for performance optimization. It should not affect
             *  the correctness of pipeline. Degree of concurrency is for correctness.
             *  For serial stages, we currently allow min_chunk_size to be greater than 1
             *  so as to reduce scheduling overhead for special cases.
             *  However, the programmer needs to make sure that the algorithm behaves
             *  correctly with the respective chunk size, especially for memory management.
             *
             * @includelineno samples/src/pipeline.cc
             */
            template <typename... Args>
            void add_cpu_stage(Args&&... args)
            {
                using check_params = typename hetcompute::internal::pipeline_utility::check_add_cpu_stage_params<context&, Args...>;
                static_assert(check_params::value, "add_cpu_stage has illegal parameters");

                int const stage_index         = check_params::stage_index;
                int const body_index          = check_params::body_index;
                int const lag_index           = check_params::result::iteration_lag_index;
                int const rate_index          = check_params::result::iteration_rate_index;
                int const sws_index           = check_params::result::sliding_window_size_index;
                int const pattern_tuner_index = check_params::result::pattern_tuner_index;

                bool const has_iteration_lag = check_params::result::iteration_lag_num == 0 ? false : true;
                using StageType              = typename check_params::StageType;

                auto                          input_tuple = std::forward_as_tuple(args...);
                hetcompute::iteration_lag       lag(0);
                hetcompute::iteration_rate      rate(1, 1);
                hetcompute::sliding_window_size sws(0);
                hetcompute::pattern::tuner      t;
                auto                          default_tuple = std::forward_as_tuple(lag, rate, sws, t);

                using stage_param_list_type = hetcompute::internal::pipeline_utility::stage_param_list_type;
                using lag_mux               = typename hetcompute::internal::pipeline_utility::mux_param_value<hetcompute::iteration_lag,
                                                                                               decltype(input_tuple),
                                                                                               decltype(default_tuple),
                                                                                               lag_index,
                                                                                               stage_param_list_type::hetcompute_iteration_lag>;
                using rate_mux =
                    typename hetcompute::internal::pipeline_utility::mux_param_value<hetcompute::iteration_rate,
                                                                                   decltype(input_tuple),
                                                                                   decltype(default_tuple),
                                                                                   rate_index,
                                                                                   stage_param_list_type::hetcompute_iteration_rate>;
                using sws_mux =
                    typename hetcompute::internal::pipeline_utility::mux_param_value<hetcompute::sliding_window_size,
                                                                                   decltype(input_tuple),
                                                                                   decltype(default_tuple),
                                                                                   sws_index,
                                                                                   stage_param_list_type::hetcompute_sliding_window_size>;

                using pattern_tuner_mux =
                    typename hetcompute::internal::pipeline_utility::mux_param_value<hetcompute::pattern::tuner,
                                                                                   decltype(input_tuple),
                                                                                   decltype(default_tuple),
                                                                                   pattern_tuner_index,
                                                                                   stage_param_list_type::hetcompute_stage_pattern_tuner>;

                using Body = typename hetcompute::internal::pipeline_utility::get_cpu_body_type<decltype(input_tuple), body_index>::type;

                // add a cpu stage
                auto skeleton = c_ptr(_skeleton);
                skeleton->template add_cpu_stage<StageType, Body>(std::get<stage_index>(input_tuple),
                                                                  std::forward<Body>(
                                                                      hetcompute::internal::pipeline_utility::
                                                                          get_tuple_element_helper<body_index, decltype(input_tuple)>::get(
                                                                              input_tuple)),
                                                                  has_iteration_lag,
                                                                  lag_mux::get(input_tuple, default_tuple),
                                                                  rate_mux::get(input_tuple, default_tuple),
                                                                  sws_mux::get(input_tuple, default_tuple),
                                                                  pattern_tuner_mux::get(input_tuple, default_tuple));
            }

        public:
            /**
             * @brief Context type for the pipeline.
             *
             * Context type for the pipeline.
             */
            using context = pipeline_context<UserData...>;

            /**
             * @brief Constructor.
             *
             * Constructor.
             */
            pipeline() : _skeleton(new hetcompute::internal::pipeline_skeleton<UserData...>()) {}

            /**
             * @brief Destructor.
             *
             * Destructor.
             */
            virtual ~pipeline() {}

            /**
             * @brief Copy constructor.
             *
             * Copy constructor.
             */
            pipeline(pipeline const& other) : _skeleton(new hetcompute::internal::pipeline_skeleton<UserData...>(*c_ptr(other._skeleton))) {}

            /**
             * @brief Move constructor.
             *
             * Move constructor.
             */
            pipeline(pipeline&& other) : _skeleton(other._skeleton) { other._skeleton = nullptr; }

            /**
             * @brief Copy assignment operator.
             *
             * Copy assignment operator.
             */
            pipeline& operator=(pipeline const& other)
            {
                _skeleton = hetcompute::internal::hetcompute_shared_ptr<hetcompute::internal::pipeline_skeleton<UserData...>>(
                    new hetcompute::internal::pipeline_skeleton<UserData...>(*c_ptr(other._skeleton)));

                return *this;
            }

            /**
             * @brief Move assignment operator.
             *
             * Move assignment operator.
             */
            pipeline& operator=(pipeline&& other)
            {
                _skeleton       = other._skeleton;
                other._skeleton = nullptr;
                return *this;
            }

            /** @cond PRIVATE */
            ///
            /// add a stage
            ///
            template <typename... Args>
            void add_stage(Args&&... args)
            {
                add_cpu_stage(std::forward<Args>(args)...);
            }
            /** @endcond     */

            /**
             * @brief Launch and wait for the pipeline.
             *
             * Launch and wait for the pipeline.
             *
             * @param context_data Pointer to the data for the pipeline context
             *                     if the pipeline defined as having one,
             *                     i.e., sizeof...(UserData) == 1.
             * @param num_iterations The total number of iterations for the first stage.
             * @param confs        Other configurations for running a pipeline.
             *                     Currently, only support one tuner object
             *                     for the pipeline (optional).
             *
             * @note1 if num_iterations == 0, the pipeline runs infinite number
             * of iterations until the first stage stops the pipeline.
             *
             * @includelineno samples/src/pipeline_wocd_iter_lch_obj.cc
             */
            template <typename... Confs>
            void run(UserData*... context_data, size_t num_iterations, Confs&&... confs) const
            {
                using check_params = typename hetcompute::internal::pipeline_utility::check_launch_params<size_t, Confs&&...>;

                int const pattern_tuner_index = check_params::result::pattern_tuner_index;

                auto                     input_tuple = std::forward_as_tuple(num_iterations, confs...);
                hetcompute::pattern::tuner t;
                auto                     default_tuple = std::forward_as_tuple(num_iterations, t);

                using launch_param_list_type = hetcompute::internal::pipeline_utility::launch_param_list_type;
                using pattern_tuner_mux =
                    typename hetcompute::internal::pipeline_utility::mux_param_value<hetcompute::pattern::tuner,
                                                                                   decltype(input_tuple),
                                                                                   decltype(default_tuple),
                                                                                   pattern_tuner_index,
                                                                                   launch_param_list_type::hetcompute_launch_pattern_tuner>;

                auto skeleton = c_ptr(_skeleton);

                if (skeleton->is_empty())
                    return;

                skeleton->freeze();

                hetcompute::internal::hetcompute_shared_ptr<hetcompute::internal::pipeline_instance<UserData...>> pinst(
                    new hetcompute::internal::pipeline_instance<UserData...>(_skeleton));

                auto pipelineinst = c_ptr(pinst);
                pipelineinst->launch(context_data...,
                                     num_iterations,
                                     skeleton->get_launch_type(),
                                     pattern_tuner_mux::get(input_tuple, default_tuple));

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                try
                {
#endif
                    pipelineinst->wait_for();

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                }
                catch (...)
                {
                }
#endif
            }

            /**
             * @brief Create a task for the pipeline for asynchronous execution.
             *
             * Create a task for the pipeline for asynchronous execution. Do not call this
             * member function if the pipeline has no stages. This would cause a
             * fatal error.
             *
             * @param context_data Pointer to the data for the pipeline context
             *                     if the pipeline is defined as having one,
             *                     i.e., sizeof...(UserData) == 1.
             * @param num_iterations The total number of iterations for the first stage.
             *        @note1 if num_iterations == 0, the pipeline runs infinite number
             *               of iterations until the first stage stops the pipeline.
             * @param confs        Other configurations for launching a task out of pipeline.
             *                     Currently, only support one tuner object
             *                     for the pipeline (optional).
             *
             * @return hetcompute::task_ptr<> The pointer to the task in which the pipeline is running.
             *
             * @includelineno samples/src/pipeline_wocd_iter_ct_obj.cc
             */
            template <typename... Confs>
            hetcompute::task_ptr<> create_task(UserData*... context_data, size_t num_iterations, Confs&&... confs) const
            {
                using check_params = typename hetcompute::internal::pipeline_utility::check_launch_params<size_t, Confs&&...>;

                int const pattern_tuner_index = check_params::result::pattern_tuner_index;

                auto                     input_tuple = std::forward_as_tuple(num_iterations, confs...);
                hetcompute::pattern::tuner t;
                auto                     default_tuple = std::forward_as_tuple(num_iterations, t);

                using launch_param_list_type = hetcompute::internal::pipeline_utility::launch_param_list_type;
                using pattern_tuner_mux =
                    typename hetcompute::internal::pipeline_utility::mux_param_value<hetcompute::pattern::tuner,
                                                                                   decltype(input_tuple),
                                                                                   decltype(default_tuple),
                                                                                   pattern_tuner_index,
                                                                                   launch_param_list_type::hetcompute_launch_pattern_tuner>;
                auto t1 = pattern_tuner_mux::get(input_tuple, default_tuple);

                auto skeleton = c_ptr(_skeleton);
                HETCOMPUTE_API_ASSERT(skeleton->is_empty() == false, "The pipeline has no stages.");

                skeleton->freeze();

                hetcompute::internal::hetcompute_shared_ptr<hetcompute::internal::pipeline_instance<UserData...>> pinst(
                    new hetcompute::internal::pipeline_instance<UserData...>(_skeleton));

                // use std::make_tuple to workaround gcc's incapability of capturing variadic parameters
                auto datatp = std::make_tuple(context_data..., num_iterations);

                auto launch_type = skeleton->get_launch_type();

                using launch_helper = typename hetcompute::internal::pipeline_utility::apply_launch<userdata_size + 1>;

                auto ptask = hetcompute::create_task([pinst, datatp, launch_type, t1]() {

                    auto pipelineinst = c_ptr(pinst);
                    launch_helper::apply(pipelineinst, launch_type, t1, datatp);
                    pipelineinst->finish_after();
                });

                return ptask;
            }

            /**
             * @brief Create a task for the pipeline for asynchronous execution.
             *
             * Create a task for the pipeline for asynchronous execution.
             * The task arguments need to be bound later. Do not call this
             * member function if the pipeline has no stages. This would cause a
             * fatal error.
             *
             * @param t One tuner object for the pipeline (optional).
             *
             * @return hetcompute::task_ptr<void(UserData*..., size_t num_iterations)>
             *         The pointer to the task in which the pipeline is running.
             *         Here, UserData*... is for the pipeline context data (if there is one),
             *         size_t is for specifying the number of iternations.
             *         Both of them need to be bound before launching the task.
             *
             * @includelineno samples/src/pipeline_wcd_ofs_ctlchl_obj.cc
             * @includelineno samples/src/pipeline_wcd_iter_ctlchl_obj.cc
             */
            hetcompute::task_ptr<void(UserData*..., size_t)> create_task(const hetcompute::pattern::tuner& t = hetcompute::pattern::tuner()) const
            {
                auto skeleton = c_ptr(_skeleton);

                HETCOMPUTE_API_ASSERT(skeleton->is_empty() == false, "The pipeline has no stages.");

                skeleton->freeze();
                hetcompute::internal::hetcompute_shared_ptr<hetcompute::internal::pipeline_instance<UserData...>> pinst(
                    new hetcompute::internal::pipeline_instance<UserData...>(_skeleton));

                auto launch_type = skeleton->get_launch_type();

                auto ptask = hetcompute::create_task([pinst, launch_type, t](UserData*... context_data, size_t num_iterations) {
                    auto pipelineinst = c_ptr(pinst);
                    pipelineinst->launch(context_data..., num_iterations, launch_type, t);
                    pipelineinst->finish_after();
                });

                return ptask;
            }

            /**
             * @brief Pipeline sanity check for stage IO types and sliding window size.
             *
             * Pipeline sanity check for stage IO types and sliding window size.
             *
             * @return TRUE (pass) or FALSE (fail)
             *
             * @includelineno samples/src/pipeline_is_valid.cc
             */
            bool is_valid()
            {
                auto skeleton = c_ptr(_skeleton);
                return skeleton->is_valid();
            }

            /**
             * @brief Disable the pipeline sliding window launch type.
             *
             * Disable the pipeline sliding window launch type and there
             * won't be any control on the memory footprint.
             *
             */
            void disable_sliding_window()
            {
                auto skeleton = c_ptr(_skeleton);
                skeleton->set_launch_type(hetcompute::internal::without_sliding_window);
            }

            /**
             * @brief Enable the pipeline launch type to be with sliding window.
             *
             * Enable the pipeline launch type to be with sliding window.
             *
             */
            void enable_sliding_window()
            {
                auto skeleton = c_ptr(_skeleton);
                skeleton->set_launch_type(hetcompute::internal::with_sliding_window);
            }

            /** @cond PRIVATE */
            friend hetcompute::internal::test::pipeline_tester;
            friend hetcompute::internal::test::pipeline_capi_internal_tester;

// This is to only require types to be known when compiling capi.cc
// More explanation in capi.cc
#ifdef HETCOMPUTE_CAPI_COMPILING_CC
            friend int ::hetcompute_pattern_pipeline_run(::hetcompute_pattern_pipeline_ptr p, void* data, unsigned int iterations);
#endif

            /** @endcond */
        };
        /** @} */ /* end_addtogroup pipeline_doc */
        // end of class pipeline<UserData...>
    }; // namespace pattern

    /**
     * @brief Create a task for the pipeline for asynchronous execution.
     *
     * Create a task for the pipeline for asynchronous execution.
     *
     * @param p Reference to the pipeline object.
     * @param num_iterations The total number of iterations for the first stage.
     *        @note1 if num_iterations == 0, the pipeline runs infinite number
     *               of iterations until the first stage stops the pipeline.
     * @param t One tuner object for the pipeline (optional).
     *
     * @return hetcompute::task_ptr<> The pointer to the task in which the pipeline is running
     *
     * @includelineno samples/src/pipeline_wocd_ofs_ct.cc
     * @includelineno samples/src/pipeline_wocd_iter_ct.cc
     */
    hetcompute::task_ptr<> create_task(const hetcompute::pattern::pipeline<>& p,
                                     size_t                               num_iterations,
                                     const hetcompute::pattern::tuner&      t = hetcompute::pattern::tuner());

    /**
     * @brief Create a task for the pipeline for asynchronous execution.
     *
     * Create a task for the pipeline without userdata for asynchronous execution
     * and task parameter to bind later.
     *
     * @param p Reference to the pipeline object.
     * @param t One tuner object for the pipeline (optional).
     *
     * @return hetcompute::task_ptr<void(size_t)>
     *         The pointer to the task in which the pipeline is running.
     *         num_iteration of the pipeline should be binded to the task later.
     *
     * @includelineno samples/src/pipeline_wocd_ofs_ct.cc
     * @includelineno samples/src/pipeline_wocd_iter_ct.cc
     */
    hetcompute::task_ptr<void(size_t)> create_task(const hetcompute::pattern::pipeline<>& p,
                                                 const hetcompute::pattern::tuner&      t = hetcompute::pattern::tuner());

    /**
     * launch and wait for the pipeline
     * @param reference to the pipeline object
     * @param num_iterations the total number of iterations for the first stage
     */
    void launch(const hetcompute::pattern::pipeline<>& p, size_t num_iterations, const hetcompute::pattern::tuner& t = hetcompute::pattern::tuner());

    /**
     * @brief Create a task for the pipeline for asynchronous execution.
     *
     * Create a task for the pipeline for asynchronous execution.
     * Bind the task argument later before launching.
     *
     * @param p Reference to the pipeline object.
     * @param t One tuner object for the pipeline (optional).
     *
     * @return hetcompute::task_ptr<void(size_t)>
     *         The pointer to the task in which the pipeline is running.
     *         Here, size_t is for specifying the number of iternations which
     *         needs to be bound before launching the task.
     *
     * @includelineno samples/src/pipeline_wocd_ofs_ctlchl.cc
     * @includelineno samples/src/pipeline_wocd_iter_ctlchl.cc
     */
    template <typename UserData>
    hetcompute::task_ptr<> create_task(const hetcompute::pattern::pipeline<UserData>& p,
                                     UserData*                                    context_data,
                                     size_t                                       num_iterations,
                                     const hetcompute::pattern::tuner&              t = hetcompute::pattern::tuner())
    {
        return p.create_task(context_data, num_iterations, t);
    }

    /**
     * @brief Create a task for the pipeline for asynchronous execution.
     *
     * Create a task for the pipeline for asynchronous execution.
     * Bind the task argument later before launching.
     *
     * @tparam UserData The type for the pipeline context data.
     *
     * @param p Reference to the pipeline object.
     * @param t One tuner object for the pipeline (optional).
     *
     * @return hetcompute::task_ptr<void(UserData*, size_t)>
     *         The pointer to the task in which the pipeline is running.
     *         Here, UserData* is for the pipeline context data,
     *         size_t is for specifying the number of iternations.
     *         Both of them need to be bound before launching the task.
     *
     * @includelineno samples/src/pipeline_wcd_ofs_ctlchl.cc
     * @includelineno samples/src/pipeline_wcd_iter_ctlchl.cc
     */
    template <typename UserData>
    hetcompute::task_ptr<void(UserData*, size_t)> create_task(const hetcompute::pattern::pipeline<UserData>& p,
                                                            const hetcompute::pattern::tuner&              t = hetcompute::pattern::tuner())
    {
        return p.create_task(t);
    }

    /**
     * @brief Launch and wait for the pipeline.
     *
     * Launch and wait for the pipeline.
     *
     * @tparam UserData The type for the pipeline context data.
     *
     * @param p Reference to the pipeline object.
     * @param context_data Pointer to the data for the pipeline context.
     * @param num_iterations The total number of iterations for the first stage.
     * @param t One tuner object for the pipeline (optional).
     *
     * @includelineno samples/src/pipeline_wcd_ofs_lch.cc
     * @includelineno samples/src/pipeline_wcd_iter_lch.cc
     */
    template <typename UserData>
    void launch(const hetcompute::pattern::pipeline<UserData>& p,
                UserData*                                    context_data,
                size_t                                       num_iterations,
                const hetcompute::pattern::tuner&              t = hetcompute::pattern::tuner())
    {
        p.run(context_data, num_iterations, t);
    }
}; // namespace hetcompute

// HetCompute beta features.
#ifdef HETCOMPUTE_HAVE_GPU
namespace hetcompute
{
    namespace beta
    {
        namespace pattern
        {
            /** @addtogroup pipeline_doc
                @{ */
            /**
             * @brief Heterogeneous Pipeline class.
             *
             * Heterogeneous Pipeline class.
             *
             * @tparam UserData The type for the pipeline context data or empty,
             *                  i.e., <code> hetcompute::pattern::pipeline<size_t></code> or
             *                        <code> hetcompute::pattern::pipeline<></code>.
             */
            template <typename... UserData>
            class pipeline : public hetcompute::pattern::pipeline<UserData...>
            {
                /** @cond PRIVATE */
                static constexpr size_t userdata_size = sizeof...(UserData);
                static_assert(userdata_size <= 1, "HetCompute pipeline can only have, at most, one type of context data.");
                using parent_type = typename hetcompute::pattern::pipeline<UserData...>;
                /** @endcond */

                /**
                 *  @brief Add a gpu stage
                 *
                 *  Add a gpu stage
                 *
                 *  args should be the following:
                 *
                 *  One </code>hetcompute::serial_stage</code> or <code>hetcompute::parallel_stage</code>
                 *
                 *  At most one </code>hetcompute::iteration_lag</code>
                 *
                 *  At most one </code>hetcompute::iteration_rate</code>
                 *
                 *  At most one </code>hetcompute::sliding_window_size</code>
                 *
                 *  One lambda/functor/function ptr before the gpu kernel for cpu/gpu sync
                 *  right before the gpu stage. This functor should take a reference to
                 *  the pipeline context as its first parameter, and have the return type of
                 *  std::tuple of types with all the gpu kernel parameters in order.
                 *
                 *  One gpu kernel of type </code>hetcompute::kernel_ptr</code>
                 *  through </code>hetcompute::create_kernel</code>
                 *
                 *  At most one lambda/functor/function ptr after the gpu kernel
                 *  for cpu/gpu sync right after the gpu stage. This functor should take a
                 *  reference to the pipeline context as its first parameter, and take a reference
                 *  to the return type of the sync function before the gpu kernel as
                 *  its second parameter.
                 *
                 *  At most one </code>hetcompute::pattern::tuner</code>
                 *  Tunable parameter for pipeline GPU stages: min_chunk_size.
                 *  Here min_chunk_size is the number of stage iterations that can be
                 *  fused to execute as a chunk to reduce scheduling overhead.
                 *  On the other hand, while increasing granuality,
                 *  iteration fusion can reduce level of parallelism.
                 *  For parallel stages, tuner's <code>min_chunk_size</code>
                 *  should never be greater than the stage's degree of concurrency.
                 *  Tuner is designed for performance optimization. It should not affect
                 *  the correctness of pipeline. Degree of concurrency is for correctness.
                 *  Serial stage chunking is not supported for gpu stages.
                 */

                template <typename... Args>
                void add_gpu_stage(Args&&... args)
                {
                    using check_gpu_kernel = typename hetcompute::internal::pipeline_utility::check_gpu_kernel<Args...>;
                    static_assert(check_gpu_kernel::has_gpu_kernel, "gpu stage should have a gpu kernel");

                    auto input_tuple = std::forward_as_tuple(std::forward<Args>(args)...);

                    using check_params =
                        typename hetcompute::internal::pipeline_utility::check_add_gpu_stage_params<context&, decltype(input_tuple), Args...>;
                    static_assert(check_params::value, "add_gpu_stage has illegal parameters");

                    int const  stage_index         = check_params::stage_index;
                    int const  lag_index           = check_params::result::iteration_lag_index;
                    int const  rate_index          = check_params::result::iteration_rate_index;
                    int const  sws_index           = check_params::result::sliding_window_size_index;
                    int const  pattern_tuner_index = check_params::result::pattern_tuner_index;
                    bool const has_iteration_lag   = check_params::result::iteration_lag_num == 0 ? false : true;
                    using StageType                = typename check_params::StageType;

                    hetcompute::iteration_lag       lag(0);
                    hetcompute::iteration_rate      rate(1, 1);
                    hetcompute::sliding_window_size sws(0);
                    hetcompute::pattern::tuner      t;
                    auto                          default_tuple = std::forward_as_tuple(lag, rate, sws, t);

                    using stage_param_list_type = hetcompute::internal::pipeline_utility::stage_param_list_type;
                    using lag_mux =
                        typename hetcompute::internal::pipeline_utility::mux_param_value<hetcompute::iteration_lag,
                                                                                       decltype(input_tuple),
                                                                                       decltype(default_tuple),
                                                                                       lag_index,
                                                                                       stage_param_list_type::hetcompute_iteration_lag>;
                    using rate_mux =
                        typename hetcompute::internal::pipeline_utility::mux_param_value<hetcompute::iteration_rate,
                                                                                       decltype(input_tuple),
                                                                                       decltype(default_tuple),
                                                                                       rate_index,
                                                                                       stage_param_list_type::hetcompute_iteration_rate>;
                    using sws_mux =
                        typename hetcompute::internal::pipeline_utility::mux_param_value<hetcompute::sliding_window_size,
                                                                                       decltype(input_tuple),
                                                                                       decltype(default_tuple),
                                                                                       sws_index,
                                                                                       stage_param_list_type::hetcompute_sliding_window_size>;

                    using pattern_tuner_mux =
                        typename hetcompute::internal::pipeline_utility::mux_param_value<hetcompute::pattern::tuner,
                                                                                       decltype(input_tuple),
                                                                                       decltype(default_tuple),
                                                                                       pattern_tuner_index,
                                                                                       stage_param_list_type::hetcompute_stage_pattern_tuner>;

                    const auto bbody_index = check_params::bbody_index;
                    using BeforeBody       = typename check_params::BeforeBody;

                    const auto abody_index = check_params::abody_index;
                    using AfterBody        = typename check_params::AfterBody;

                    const auto gkbody_index = check_params::gkbody_index;
                    using GKBody            = typename check_params::GKBody;

                    static_assert(gkbody_index != -1, "gpu stage should have a gpu kernel");

                    // add a gpu stage
                    c_ptr(parent_type::_skeleton)
                        ->template add_gpu_stage<StageType, BeforeBody, GKBody, AfterBody>(
                            std::get<stage_index>(input_tuple),
                            std::forward<BeforeBody>(
                                hetcompute::internal::pipeline_utility::get_tuple_element_helper<bbody_index, decltype(input_tuple)>::get(
                                    input_tuple)),
                            std::forward<GKBody>(
                                hetcompute::internal::pipeline_utility::get_tuple_element_helper<gkbody_index, decltype(input_tuple)>::get(
                                    input_tuple)),
                            std::forward<AfterBody>(
                                hetcompute::internal::pipeline_utility::get_tuple_element_helper<abody_index, decltype(input_tuple)>::get(
                                    input_tuple)),
                            has_iteration_lag,
                            lag_mux::get(input_tuple, default_tuple),
                            rate_mux::get(input_tuple, default_tuple),
                            sws_mux::get(input_tuple, default_tuple),
                            pattern_tuner_mux::get(input_tuple, default_tuple));
                }

            public:
                /**
                 * @brief Context type for the pipeline.
                 *
                 * Context type for the pipeline.
                 */
                using context = typename parent_type::context;

                /**
                 * @brief Constructor.
                 *
                 * Constructor.
                 */
                pipeline() : parent_type() {}

                /**
                 * @brief Destructor.
                 *
                 * Destructor.
                 */
                virtual ~pipeline() {}

                /**
                 * @brief Copy constructor.
                 *
                 * Copy constructor.
                 */
                pipeline(pipeline const& other) : parent_type(other) {}

                /**
                 * @brief Move constructor.
                 *
                 * Move constructor.
                 */
                pipeline(pipeline&& other) : parent_type(std::forward<parent_type>(other)) {}

                /**
                 * @brief Copy assignment operator.
                 *
                 * Copy assignment operator.
                 */
                pipeline& operator=(pipeline const& other)
                {
                    parent_type::operator=(other);
                    return *this;
                }

                /**
                 * @brief Move assignment operator.
                 *
                 * Move assignment operator.
                 */
                pipeline& operator=(pipeline&& other)
                {
                    parent_type::operator=(std::forward<parent_type>(other));
                    return *this;
                }

                /**
                 * @brief Add a CPU stage
                 *
                 * Add a CPU stage.
                 *
                 * @param args The features of the cpu stage.
                 * @sa template<typename... Args> void add_cpu_stage(Args&&... args)
                 */
                template <typename... Args>
                typename std::enable_if<!internal::pipeline_utility::check_gpu_kernel<Args...>::has_gpu_kernel, void>::type
                add_stage(Args&&... args)
                {
                    parent_type::add_cpu_stage(std::forward<Args>(args)...);
                }

                /**
                 * @brief Add a GPU stage.
                 *
                 * Add a GPU stage.
                 * @param args The features of the gpu stage.
                 * @sa template<typename... Args> void add_gpu_stage(Args&&... args)
                 */
                template <typename... Args>
                typename std::enable_if<internal::pipeline_utility::check_gpu_kernel<Args...>::has_gpu_kernel, void>::type
                add_stage(Args&&... args)
                {
                    add_gpu_stage(std::forward<Args>(args)...);
                }

                /** @cond PRIVATE */
                friend hetcompute::internal::test::pipeline_tester;
                /** @endcond */
            };

            /** @} */ /* end_addtogroup pipeline_doc */
            // end of class hetcompute::beta::pattern::pipeline<UserData...>
        }; // namespace pattern
    };     // namespace beta
};         // namespace hetcompute
#endif     // HETCOMPUTE_HAVE_GPU
