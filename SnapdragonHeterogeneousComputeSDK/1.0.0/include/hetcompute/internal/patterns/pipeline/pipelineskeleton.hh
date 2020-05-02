/** @file pipelineskeleton.hh */
#pragma once

// Include user interface headers
#include <hetcompute/internal/synchronization/mutex.hh>
#include <hetcompute/pipelinedata.hh>
#include <hetcompute/tuner.hh>

// Include internal headers
#include <hetcompute/internal/util/macros.hh>
#include <hetcompute/internal/patterns/pipeline/pipelinebuffers.hh>
#include <hetcompute/internal/patterns/pipeline/pipelinefactory.hh>
#include <hetcompute/internal/patterns/pipeline/pipelineutility.hh>

namespace hetcompute
{
    namespace internal
    {
        ///
        namespace test
        {
            class pipeline_tester;
        }; // namespace test

        size_t const _num_init_pipeline_tasks = 8;

        // template class declaration
        class pipeline_stage_skeleton_base;
        template <typename F, typename... UserData>
        class pipeline_cpu_stage_skeleton;

        class pipeline_skeleton_base;
        template <typename... UserData>
        class pipeline_skeleton;

        class pipeline_stage_instance;
        class pipeline_instance_base;
        template <typename... UserData>
        class pipeline_instance;

        /**
         * Pipeline stage type
         */
        enum class pipeline_stage_type
        {
            serial_in_order = 0,
            serial_out_of_order,
            parallel,
            unknown
        };
        // end of enum class pipeline_stage_type

        /**
         * pipeline stage skeleton type
         */
        enum class pipeline_stage_hetero_type
        {
            cpu = 0,
            gpu
        };

        /**
         * pipeline stage skeleton base class (typeless)
         */
        class pipeline_stage_skeleton_base
        {
        protected:
            // actual iter rate for curr stage
            size_t _arate_curr;

            // actual iter rate for succ stage
            size_t _arate_succ;

            size_t _data_buf_size;

            // degree of concurrency
            size_t _doc;

            // has lag or not
            bool _has_lag;

            // heterogeneous type
            pipeline_stage_hetero_type _hetero_type;

            // stage numerical id
            size_t _id;

            // stage lag against its predecessor
            size_t _lag;

            // min sliding window size
            size_t _min_sw_size;

            // whether to process the next stage without checking
            bool _process_next_stage_wo_check;

            // iteartion rate for the pred stage
            size_t _rate_pred;

            // iteartion rate for the curr stage
            size_t _rate_curr;

            // stage type
            pipeline_stage_type _type;

            // size of the sliding window
            size_t _sw_size;

            // pattern tuner
            hetcompute::pattern::tuner _tuner;

        public:
            /**
             * constructor for serial skeleton
             */
            pipeline_stage_skeleton_base(bool const                      has_lag,
                                         pipeline_stage_hetero_type      hetero_type,
                                         size_t const                    id,
                                         size_t const                    lag,
                                         size_t const                    rpred,
                                         size_t const                    rcurr,
                                         size_t const                    sws,
                                         serial_stage const&             ss,
                                         hetcompute::pattern::tuner const& t)
                : _arate_curr(1),
                  _arate_succ(1),
                  _data_buf_size(0),
                  _doc(1),
                  _has_lag(has_lag),
                  _hetero_type(hetero_type),
                  _id(id),
                  _lag(lag),
                  _min_sw_size(0),
                  _process_next_stage_wo_check(false),
                  _rate_pred(rpred),
                  _rate_curr(rcurr),
                  _type(pipeline_stage_type::unknown),
                  _sw_size(sws),
                  _tuner(t)
            {
                if (ss._type == serial_stage_type::in_order)
                {
                    _type = pipeline_stage_type::serial_in_order;
                }
                else
                {
                    _type = pipeline_stage_type::serial_out_of_order;
                }
            }

            /**
             * constructor for parallel stage skeleton
             */
            pipeline_stage_skeleton_base(bool const                      has_lag,
                                         pipeline_stage_hetero_type      hetero_type,
                                         size_t const                    id,
                                         size_t const                    lag,
                                         size_t const                    rpred,
                                         size_t const                    rcurr,
                                         size_t const                    sws,
                                         parallel_stage const&           pp,
                                         hetcompute::pattern::tuner const& t)
                : _arate_curr(1),
                  _arate_succ(1),
                  _data_buf_size(0),
                  _doc(1),
                  _has_lag(has_lag),
                  _hetero_type(hetero_type),
                  _id(id),
                  _lag(lag),
                  _min_sw_size(0),
                  _process_next_stage_wo_check(false),
                  _rate_pred(rpred),
                  _rate_curr(rcurr),
                  _type(pipeline_stage_type::unknown),
                  _sw_size(sws),
                  _tuner(t)
            {
                _type = pipeline_stage_type::parallel;
                _doc  = pp._degree_of_concurrency;
            }

            /**
             * copy constructor
             */
            pipeline_stage_skeleton_base(pipeline_stage_skeleton_base const& other)
                : _arate_curr(other._arate_curr),
                  _arate_succ(other._arate_succ),
                  _data_buf_size(other._data_buf_size),
                  _doc(other._doc),
                  _has_lag(other._has_lag),
                  _hetero_type(other._hetero_type),
                  _id(other._id),
                  _lag(other._lag),
                  _min_sw_size(other._min_sw_size),
                  _process_next_stage_wo_check(other._process_next_stage_wo_check),
                  _rate_pred(other._rate_pred),
                  _rate_curr(other._rate_curr),
                  _type(other._type),
                  _sw_size(other._sw_size),
                  _tuner(other._tuner)
            {
            }

            /**
             * destructor
             */
            virtual ~pipeline_stage_skeleton_base() {}

            /**
             * Allocate buffers between stages
             *
             * @param buf reference to the pointer of the buffer to allocate
             * @param size size of the buffer to allocate
             * @param pipeline_launch_type
             */
            virtual void alloc_stage_buf(stagebuffer*& buf, size_t size, pipeline_launch_type type) = 0;

            /**
             * Perform stage function on the token
             *
             * @param in_first_idx the index to the first token in the the inbuf for
             *                     this stage iteration.
             * @param in_size      the number of input tokens for this iteration
             * @param inbuf        pointer to the input token buffer
             * @param out_idx      buffer index for the output token
             * @param outbuf       pointer to the output token buffer
             * @param iter_id      current iteration id
             * @param max_iter     maximum stage iterations
             * @param pstage       pointer to current pipeline_stage_instance
             * @param pinst        pointer to current pipeline_instance
             * @param launch_type
             *   hetcompute::internal::with_sliding_window
             *   hetcompute::internal::without_sliding_window
             */

            virtual void apply(size_t                   in_first_idx,
                               size_t                   in_size,
                               stagebuffer*             inbuf,
                               size_t                   out_idx,
                               stagebuffer*             outbuf,
                               size_t                   iter_id,
                               size_t                   max_iter,
                               pipeline_stage_instance* pstage,
                               pipeline_instance_base*  pinst,
                               pipeline_launch_type     launch_type) = 0;

#ifdef HETCOMPUTE_HAVE_GPU
            /**
             * Perform the after function of a gpu stage
             *
             * @param out_idx      buffer index for the output token
             * @param outbuf       pointer to the output token buffer
             * @param iter_id      current iteration id
             * @param max_iter     maximum stage iterations
             * @param pstage       pointer to current pipeline_stage_instance
             * @param pinst        pointer to current pipeline_instance
             * @param launch_type
             *   hetcompute::with_sliding_window
             *   hetcompute::without_sliding_window
             */
            virtual void
            apply_after(size_t, stagebuffer*, size_t, size_t, pipeline_stage_instance*, pipeline_instance_base*, pipeline_launch_type, void*) = 0;

            /**
             * Perform the before function of a gpu stage
             *
             * @param in_first_idx the index to the first token in the the inbuf for
             *                     this stage iteration.
             * @param in_size      the number of input tokens for this iteration
             * @param inbuf        pointer to the input token buffer
             * @param iter_id      current iteration id
             * @param pstage       pointer to current pipeline_stage_instance
             * @param pinst        pointer to current pipeline_instance
             * @param launch_type
             *   hetcompute::with_sliding_window
             *   hetcompute::without_sliding_window
             * @return std::pair<task_ptr<>, void*> where task_ptr<> points to the gpu task
             *   and void* points to the tuple returned by the before lambda.
             */
            virtual std::pair<task_ptr<>, void*> apply_before(size_t,
                                                              size_t,
                                                              stagebuffer*,
                                                              size_t,
                                                              size_t,
                                                              pipeline_stage_instance*,
                                                              pipeline_instance_base*,
                                                              pipeline_launch_type) = 0;
#endif // HETCOMPUTE_HAVE_GPU

            /**
             * clone the stage
             */
            virtual pipeline_stage_skeleton_base* clone() = 0;

            /**
             * Release the output buffer for this stage
             * @param reference of the pointer to the buffer, the value will be reset
             */
            virtual void free_stage_buf(stagebuffer*& buf) = 0;

            /**
             * Get the info of the return type of the stage function for type checking
             * @return size_t
             */
            virtual size_t get_return_type() const = 0;

            /**
             * Get the info of the argument type of the stage function for type checking
             * @return size_t
             */
            virtual size_t get_arg_type() const = 0;

            /**
             * Check if the stage is a parallel stage
             * a parallel stage with _doc=1 is considered as a serial stage
             * @return
             * true - the stage is a parallel stage
             * false - the stage is a serial stage
             */
            bool is_parallel() const { return (_type == pipeline_stage_type::parallel) && (_doc > 1); }

            /**
             * Check if the stage is a serial stage
             * a parallel stage with _doc=1 is considered as a serial stage
             * @return
             * true - the stage is a serial stage
             * false - the stage is a parallel stage
             */
            bool is_serial() const
            {
                return (_type == pipeline_stage_type::serial_in_order) || (_type == pipeline_stage_type::serial_out_of_order) || (_doc == 1);
            }

            // Forbid moving and assignment
            HETCOMPUTE_DELETE_METHOD(pipeline_stage_skeleton_base(pipeline_stage_skeleton_base&& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_stage_skeleton_base& operator=(pipeline_stage_skeleton_base const& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_stage_skeleton_base& operator=(pipeline_stage_skeleton_base&& other));

            // Make friend classes
            friend class pipeline_skeleton_base;
            friend class pipeline_stage_instance;
            friend class pipeline_instance_base;
        };
        // end of class pipeline_stage_skeleton_base

        /**
         * Stage buffer allocation helper
         */
        template <typename T>
        inline void allocate_stage_buffer(stagebuffer*& buf, size_t size, pipeline_launch_type launch_type)
        {
            if (size == 0)
            {
                buf = nullptr;
                return;
            }

            if (launch_type == with_sliding_window)
            {
                buf = new ringbuffer<T>(size);
            }
            else
            {
                buf = new dynamicbuffer<T>(size);

                // allocate the first pool
                static_cast<dynamicbuffer<T>*>(buf)->allocate_pool();
                static_cast<dynamicbuffer<T>*>(buf)->allocate_pool();
            }
        }

        /**
         * Stage buffer allocation helper specialized with type void
         */
        template <>
        inline void allocate_stage_buffer<void>(stagebuffer*& buf, size_t, pipeline_launch_type)
        {
            buf = nullptr;
        }

        /**
         * Pipeline cpu stage skeleton
         */
        template <typename F, typename... UserData>
        class pipeline_cpu_stage_skeleton : public pipeline_stage_skeleton_base
        {
        private:
            cpu_stage_function<F> _fct; // function for the stage

        public:
            // typdefs
            using return_type  = typename cpu_stage_function<F>::return_type;
            using context_type = typename cpu_stage_function<F>::context_type;
            using input_type   = typename pipeline_utility::strip_stage_input_type<typename cpu_stage_function<F>::arg1_type>::type;

            /**
             * constructor for the serial stage
             */
            pipeline_cpu_stage_skeleton(F&&                             f,
                                        bool const&                     has_lag,
                                        size_t const&                   id,
                                        iteration_lag const&            lag,
                                        iteration_rate const&           rate,
                                        sliding_window_size const&      sw,
                                        serial_stage const&             ss,
                                        hetcompute::pattern::tuner const& t)
                : pipeline_stage_skeleton_base(has_lag,
                                               pipeline_stage_hetero_type::cpu,
                                               id,
                                               lag._iter_lag,
                                               rate._iter_rate_pred,
                                               rate._iter_rate_curr,
                                               sw._size,
                                               ss,
                                               t),
                  _fct(std::forward<F>(f))
            {
            }

            /**
             * constructor for the parallel stage
             */
            pipeline_cpu_stage_skeleton(F&&                             f,
                                        bool const&                     has_lag,
                                        size_t const&                   id,
                                        iteration_lag const&            lag,
                                        iteration_rate const&           rate,
                                        sliding_window_size const&      sw,
                                        parallel_stage const&           ps,
                                        hetcompute::pattern::tuner const& t)
                : pipeline_stage_skeleton_base(has_lag,
                                               pipeline_stage_hetero_type::cpu,
                                               id,
                                               lag._iter_lag,
                                               rate._iter_rate_pred,
                                               rate._iter_rate_curr,
                                               sw._size,
                                               ps,
                                               t),
                  _fct(std::forward<F>(f))
            {
            }

            /**
             * copy constructor
             */
            pipeline_cpu_stage_skeleton(pipeline_cpu_stage_skeleton const& other) : pipeline_stage_skeleton_base(other), _fct(other._fct) {}

            /**
             * destructor
             */
            virtual ~pipeline_cpu_stage_skeleton() {}

            /**
             * Allocate buffers between stages
             *
             * @param buf reference to the pointer of the buffer to allocate
             * @param size size of the buffer to allocate
             * @param pipeline_launch_type
             */
            virtual void alloc_stage_buf(stagebuffer*& buf, size_t size, pipeline_launch_type launch_type)
            {
                allocate_stage_buffer<return_type>(buf, size, launch_type);
            }

            /**
             *
             * Perform stage function on the token
             *
             * @param in_first_idx the index to the first token in the the inbuf for
             *                     this stage iteration.
             * @param in_size      the number of input tokens for this iteration
             * @param inbuf        pointer to the input token buffer
             * @param out_idx      buffer index for the output token
             * @param outbuf       pointer to the output token buffer
             * @param iter_id      current iteration id
             * @param max_iter     maximum stage iterations
             * @param pstage       pointer to current pipeline_stage_instance
             * @param pinst        pointer to current pipeline_instance
             * @param launch_type
             *   hetcompute::internal::with_sliding_window
             *   hetcompute::internal::without_sliding_window
             */
            virtual void apply(size_t                   in_first_idx,
                               size_t                   in_size,
                               stagebuffer*             inbuf,
                               size_t                   out_idx,
                               stagebuffer*             outbuf,
                               size_t                   iter_id,
                               size_t                   max_iter,
                               pipeline_stage_instance* pstage,
                               pipeline_instance_base*  pinst,
                               pipeline_launch_type     launch_type)
            {
                using ringbuffer_input_type     = typename pipeline_utility::get_ringbuffer_type<input_type>::type;
                using ringbuffer_return_type    = typename pipeline_utility::get_ringbuffer_type<return_type>::type;
                using dynamicbuffer_input_type  = typename pipeline_utility::get_dynamicbuffer_type<input_type>::type;
                using dynamicbuffer_return_type = typename pipeline_utility::get_dynamicbuffer_type<return_type>::type;

                pipeline_context<UserData...> input_context(iter_id, max_iter, pstage, pinst);

                if (launch_type == with_sliding_window)
                {
                    _fct.template apply_function<ringbuffer_input_type, ringbuffer_return_type, input_type>(in_first_idx,
                                                                                                            in_size,
                                                                                                            static_cast<ringbuffer_input_type*>(
                                                                                                                inbuf),
                                                                                                            out_idx,
                                                                                                            static_cast<ringbuffer_return_type*>(
                                                                                                                outbuf),
                                                                                                            static_cast<void*>(&input_context),
                                                                                                            launch_type);
                }
                else
                {
                    _fct.template apply_function<dynamicbuffer_input_type,
                                                 dynamicbuffer_return_type,
                                                 input_type>(in_first_idx,
                                                             in_size,
                                                             static_cast<dynamicbuffer_input_type*>(inbuf),
                                                             out_idx,
                                                             static_cast<dynamicbuffer_return_type*>(outbuf),
                                                             static_cast<void*>(&input_context),
                                                             launch_type);
                }
            }

#ifdef HETCOMPUTE_HAVE_GPU
            /**
             * Perform the after function of a stage
             *
             * @param out_idx      buffer index for the output token
             * @param outbuf       pointer to the output token buffer
             * @param iter_id      current iteration id
             * @param max_iter     maximum stage iterations
             * @param pstage       pointer to current pipeline_stage_instance
             * @param pinst        pointer to current pipeline_instance
             * @param launch_type
             *   hetcompute::with_sliding_window
             *   hetcompute::without_sliding_window
             */
            virtual void
            apply_after(size_t, stagebuffer*, size_t, size_t, pipeline_stage_instance*, pipeline_instance_base*, pipeline_launch_type, void*)
            {
                HETCOMPUTE_INTERNAL_ASSERT(false, "shoud never be called for a cpu stage");
            };

            /**
             * Perform the before function of a stage
             * should never be called for a cpu stage
             *
             * @param in_first_idx the index to the first token in the the inbuf for
             *                     this stage iteration.
             * @param in_size      the number of input tokens for this iteration
             * @param inbuf        pointer to the input token buffer
             * @param iter_id      current iteration id
             * @param max_iter     maximum stage iterations
             * @param pstage       pointer to current pipeline_stage_instance
             * @param pinst        pointer to current pipeline_instance
             * @param launch_type
             *   hetcompute::with_sliding_window
             *   hetcompute::without_sliding_window
             * @return std::pair<task_ptr<>, void*> where task_ptr<> points to the gpu task
             *   and void* points to the tuple returned by the before lambda.
             */
            virtual std::pair<task_ptr<>, void*>
            apply_before(size_t, size_t, stagebuffer*, size_t, size_t, pipeline_stage_instance*, pipeline_instance_base*, pipeline_launch_type)
            {
                HETCOMPUTE_INTERNAL_ASSERT(false, "shoud never be called for a cpu stage");
                return std::make_pair(nullptr, nullptr);
            }
#endif // HETCOMPUTE_HAVE_GPU

            /**
             * clone the stage
             */
            virtual pipeline_stage_skeleton_base* clone() { return new pipeline_cpu_stage_skeleton<F, UserData...>(*this); }

            /**
             * Release the output buffer for this stage
             * @param reference of the pointer to the buffer, the value will be reset
             */
            virtual void free_stage_buf(stagebuffer*& buf)
            {
                delete buf;
                buf = nullptr;
            }

            /**
             * Get the info of the return type of the stage function for type checking
             * @return size_t
             */
            virtual size_t get_return_type() const
            {
#ifdef HETCOMPUTE_HAVE_RTTI
                return typeid(return_type).hash_code();
#else
                return pipeline_utility::sizeof_type<return_type>::size;
#endif // HETCOMPUTE_HAVE_RTTI
            }

            /**
             * Get the info for the argument type of the stage function for type checking
             * @return size_t
             */
            virtual size_t get_arg_type() const
            {
#ifdef HETCOMPUTE_HAVE_RTTI
                return typeid(input_type).hash_code();
#else
                return pipeline_utility::sizeof_type<input_type>::size;
#endif // HETCOMPUTE_HAVE_RTTI
            }

            // Forbid moving and assignment
            HETCOMPUTE_DELETE_METHOD(pipeline_cpu_stage_skeleton(pipeline_cpu_stage_skeleton&& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_cpu_stage_skeleton& operator=(pipeline_cpu_stage_skeleton const& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_cpu_stage_skeleton& operator=(pipeline_cpu_stage_skeleton&& other));

            // Make friend classes
            friend class pipeline_stage_instance;
            friend class pipeline_skeleton_base;
        };
        // end of class pipeline_cpu_stage_skeleton<F, UserData...>

#ifdef HETCOMPUTE_HAVE_GPU
        /**
         * helper to extrat all the gpu kernel parameters so as to
         * create the task for a gpu stage.
         */
        template <typename Range, typename GK, typename TP, int n, int... N>
        struct helper_create_pipeline_stage_gpu_task
        {
            static task_ptr<> create_gpu_stage_task(Range& range, GK& gk, TP& tp)
            {
                return helper_create_pipeline_stage_gpu_task<Range, GK, TP, n - 1, n, N...>::create_gpu_stage_task(range, gk, tp);
            }
        };

        template <typename Range, typename GK, typename TP, int... N>
        struct helper_create_pipeline_stage_gpu_task<Range, GK, TP, -1, N...>
        {
            static task_ptr<> create_gpu_stage_task(Range& range, GK& gk, TP& tp)
            {
                return hetcompute::create_task(gk, range, std::get<N + 1>(tp)...);
            }
        };

        template <typename BeforeBody, typename GK, typename AfterBody, typename... UserData>
        class pipeline_gpu_stage_skeleton : public pipeline_stage_skeleton_base
        {
        private:
            gpu_stage_sync_function<BeforeBody> _before_fct; // before sync function for the stage
            gpu_stage_sync_function<AfterBody>  _after_fct;  // after sync function for the stage
            GK                                  _gpu_kernel; // gpu kernel for the stage

        public:
            // typdefs
            typedef typename gpu_stage_sync_function<AfterBody>::return_type   return_type;
            typedef typename gpu_stage_sync_function<BeforeBody>::return_type  before_return_type;
            typedef typename gpu_stage_sync_function<BeforeBody>::context_type context_type;
            typedef typename gpu_stage_sync_function<BeforeBody>::context_type before_context_type;
            typedef typename gpu_stage_sync_function<AfterBody>::context_type  after_context_type;

            static_assert(!std::is_same<before_context_type, void>::value && (std::is_same<after_context_type, void>::value ||
                                                                              std::is_same<before_context_type, after_context_type>::value),
                          "the context types of the before and after gpu sync functions should match.");

            typedef
                typename pipeline_utility::strip_stage_input_type<typename gpu_stage_sync_function<BeforeBody>::arg1_type>::type input_type;

            typedef typename pipeline_utility::type_helper<BeforeBody>::org_type before_body_org_type;
            typedef typename pipeline_utility::type_helper<AfterBody>::org_type  after_body_org_type;

            /**
             * constructor
             */
            template <typename StageType>
            pipeline_gpu_stage_skeleton(BeforeBody&&                    before_fct,
                                        GK&&                            gpu_kernel,
                                        AfterBody&&                     after_fct,
                                        bool                            has_lag,
                                        const size_t                    id,
                                        iteration_lag const&            lag,
                                        iteration_rate const&           rate,
                                        sliding_window_size const&      sw,
                                        StageType const&                ss,
                                        hetcompute::pattern::tuner const& t)
                : pipeline_stage_skeleton_base(has_lag,
                                               pipeline_stage_hetero_type::gpu,
                                               id,
                                               lag._iter_lag,
                                               rate._iter_rate_pred,
                                               rate._iter_rate_curr,
                                               sw._size,
                                               ss,
                                               t),
                  _before_fct(std::forward<before_body_org_type>(before_fct)),
                  _after_fct(std::forward<after_body_org_type>(after_fct)),
                  _gpu_kernel(gpu_kernel)
            {
            }

            /**
             * copy constructor
             */
            pipeline_gpu_stage_skeleton(pipeline_gpu_stage_skeleton const& other)
                : pipeline_stage_skeleton_base(other),
                  _before_fct(other._before_fct),
                  _after_fct(other._after_fct),
                  _gpu_kernel(other._gpu_kernel)
            {
            }

            /**
             * destructor
             */
            virtual ~pipeline_gpu_stage_skeleton() {}

            /**
             * Allocate buffers between stages
             *
             * @param buf reference to the pointer of the buffer to allocate
             * @param size size of the buffer to allocate
             * @param pipeline_launch_type
             */
            virtual void alloc_stage_buf(stagebuffer*& buf, size_t size, pipeline_launch_type launch_type)
            {
                allocate_stage_buffer<return_type>(buf, size, launch_type);
            }

            /**
             * Perform stage function on the token
             *
             * @param in_first_idx the index to the first token in the the inbuf for
             *                     this stage iteration.
             * @param in_size      the number of input tokens for this iteration
             * @param inbuf        pointer to the input token buffer
             * @param out_idx      buffer index for the output token
             * @param outbuf       pointer to the output token buffer
             * @param iter_id      current iteration id
             * @param max_iter     maximum stage iterations
             * @param pstage       pointer to current pipeline_stage_instance
             * @param pinst        pointer to current pipeline_instance
             * @param launch_type
             *   hetcompute::with_sliding_window
             *   hetcompute::without_sliding_window
             */
            virtual void apply(size_t,
                               size_t,
                               stagebuffer*,
                               size_t,
                               stagebuffer*,
                               size_t,
                               size_t,
                               pipeline_stage_instance*,
                               pipeline_instance_base*,
                               pipeline_launch_type)
            {
                // The scheduler will never call this function for GPU stages,
                // instead it calls apply_before(), gpu_kernel and apply_after().
                HETCOMPUTE_UNREACHABLE("The apply function of the gpu stage should never be called.");
            }

            /**
             * Perform the after lambda of a gpu stage
             *
             * @param out_idx      buffer index for the output token
             * @param outbuf       pointer to the output token buffer
             * @param iter_id      current iteration id
             * @param max_iter     maximum stage iterations
             * @param pstage       pointer to current pipeline_stage_instance
             * @param pinst        pointer to current pipeline_instance
             * @param launch_type
             *   hetcompute::with_sliding_window
             *   hetcompute::without_sliding_window
             * @param gk_tuple
             */
            virtual void apply_after(size_t                   out_idx,
                                     stagebuffer*             outbuf,
                                     size_t                   iter_id,
                                     size_t                   max_iter,
                                     pipeline_stage_instance* pstage,
                                     pipeline_instance_base*  pinst,
                                     pipeline_launch_type     launch_type,
                                     void*                    gk_tuple_ptr)
            {
                typedef typename pipeline_utility::get_return_tuple_type<before_return_type>::type before_body_return_type;
                typedef typename pipeline_utility::get_ringbuffer_type<return_type>::type          ringbuffer_return_type;
                typedef typename pipeline_utility::get_dynamicbuffer_type<return_type>::type       dynamicbuffer_return_type;

                pipeline_context<UserData...> input_context(iter_id, max_iter, pstage, pinst);

                if (launch_type == with_sliding_window)
                {
                    _after_fct.template apply_after_sync<before_body_return_type,
                                                         ringbuffer_return_type>(out_idx,
                                                                                 static_cast<ringbuffer_return_type*>(outbuf),
                                                                                 static_cast<void*>(&input_context),
                                                                                 *static_cast<before_body_return_type*>(gk_tuple_ptr));
                }
                else
                {
                    _after_fct.template apply_after_sync<before_body_return_type,
                                                         dynamicbuffer_return_type>(out_idx,
                                                                                    static_cast<dynamicbuffer_return_type*>(outbuf),
                                                                                    static_cast<void*>(&input_context),
                                                                                    *static_cast<before_body_return_type*>(gk_tuple_ptr));
                }
                delete static_cast<before_body_return_type*>(gk_tuple_ptr);
            };

            /**
             * Perform the before lambda of a gpu stage
             *
             * @param in_first_idx the index to the first token in the the inbuf for
             *                     this stage iteration.
             * @param in_size      the number of input tokens for this iteration
             * @param inbuf        pointer to the input token buffer
             * @param iter_id      current iteration id
             * @param max_iter     maximum stage iterations
             * @param pstage       pointer to current pipeline_stage_instance
             * @param pinst        pointer to current pipeline_instance
             * @param launch_type
             *   hetcompute::with_sliding_window
             *   hetcompute::without_sliding_window
             * @return std::pair<task_ptr<>, void*> where task_ptr<> points to the gpu task
             *   and void* points to the tuple returned by the before lambda.
             */
            virtual std::pair<task_ptr<>, void*> apply_before(size_t                   in_first_idx,
                                                              size_t                   in_size,
                                                              stagebuffer*             inbuf,
                                                              size_t                   iter_id,
                                                              size_t                   max_iter,
                                                              pipeline_stage_instance* pstage,
                                                              pipeline_instance_base*  pinst,
                                                              pipeline_launch_type     launch_type)
            {
                typedef typename pipeline_utility::get_return_tuple_type<before_return_type>::type before_body_return_type;
                typedef typename pipeline_utility::get_ringbuffer_type<input_type>::type           ringbuffer_input_type;
                typedef typename pipeline_utility::get_dynamicbuffer_type<input_type>::type        dynamicbuffer_input_type;

                pipeline_context<UserData...> input_context(iter_id, max_iter, pstage, pinst);

                if (launch_type == with_sliding_window)
                {
                    auto&& rt_bbody =
                        _before_fct.template apply_before_sync<input_type, ringbuffer_input_type>(in_first_idx,
                                                                                                  in_size,
                                                                                                  static_cast<ringbuffer_input_type*>(inbuf),
                                                                                                  static_cast<void*>(&input_context),
                                                                                                  launch_type);

                    using range_type = typename std::tuple_element<0, before_body_return_type>::type;

                    auto gpu_tptr =
                        helper_create_pipeline_stage_gpu_task<range_type,
                                                              GK,
                                                              before_body_return_type,
                                                              std::tuple_size<before_body_return_type>::value -
                                                                  2>::create_gpu_stage_task(std::get<0>(rt_bbody), _gpu_kernel, rt_bbody);

                    return std::make_pair(gpu_tptr, new before_body_return_type(rt_bbody));
                }
                else
                {
                    auto&& rt_bbody =
                        _before_fct.template apply_before_sync<input_type, dynamicbuffer_input_type>(in_first_idx,
                                                                                                     in_size,
                                                                                                     static_cast<dynamicbuffer_input_type*>(
                                                                                                         inbuf),
                                                                                                     static_cast<void*>(&input_context),
                                                                                                     launch_type);

                    using range_type = typename std::tuple_element<0, before_body_return_type>::type;
                    auto gpu_tptr =
                        helper_create_pipeline_stage_gpu_task<range_type,
                                                              GK,
                                                              before_body_return_type,
                                                              std::tuple_size<before_body_return_type>::value -
                                                                  2>::create_gpu_stage_task(std::get<0>(rt_bbody), _gpu_kernel, rt_bbody);
                    return std::make_pair(gpu_tptr, new before_body_return_type(rt_bbody));
                }
            }

            /**
             * clone the stage
             */
            virtual pipeline_stage_skeleton_base* clone()
            {
                return new pipeline_gpu_stage_skeleton<BeforeBody, GK, AfterBody, UserData...>(*this);
            }

            /**
             * Release the output buffer for this stage
             * @param reference of the pointer to the buffer, the value will be reset
             */
            virtual void free_stage_buf(stagebuffer*& buf)
            {
                delete buf;
                buf = nullptr;
            }

            /**
             * Get the info of the return type of the stage output buffer for type checking
             * @return size_t
             */
            virtual size_t get_return_type() const
            {
#ifdef HETCOMPUTE_HAVE_RTTI
                return typeid(return_type).hash_code();
#else
                return pipeline_utility::sizeof_type<return_type>::size;
#endif // HETCOMPUTE_HAVE_RTTI
            }

            /**
             * Get the info of the argument type of the stage input buffer for type checking
             * @return size_t
             */
            virtual size_t get_arg_type() const
            {
#ifdef HETCOMPUTE_HAVE_RTTI
                return typeid(input_type).hash_code();
#else
                return pipeline_utility::sizeof_type<input_type>::size;
#endif // HETCOMPUTE_HAVE_RTTI
            }

            // Forbid moving and assignment
            HETCOMPUTE_DELETE_METHOD(pipeline_gpu_stage_skeleton(pipeline_gpu_stage_skeleton const&& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_gpu_stage_skeleton& operator=(pipeline_gpu_stage_skeleton const& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_gpu_stage_skeleton& operator=(pipeline_gpu_stage_skeleton&& other));

            // Make friend classes
            friend class pipeline_stage_instance;
            friend class pipeline_skeleton_base;
        };
// end of class pipeline_gpu_stage_skeleton<BeforeBody, GK, AfterBody, UserData...>: public pipeline_stage_skeleton_base
#endif // HETCOMPUTE_HAVE_GPU

        /**
         * pipeline skeleton base
         */
        class pipeline_skeleton_base
        {
        protected:
            std::atomic<bool>                          _frozen;
            bool                                       _has_sliding_window;
            pipeline_launch_type                       _launch_type;
            size_t                                     _num_stages;
            bool                                       _preprocessed;
            hetcompute::internal::mutex                _skeleton_lock;
            std::vector<pipeline_stage_skeleton_base*> _stages;

        public:
            /**
             * constructor
             */
            pipeline_skeleton_base()
                : _frozen(false),
                  _has_sliding_window(false),
                  _launch_type(with_sliding_window),
                  _num_stages(0),
                  _preprocessed(false),
                  _skeleton_lock(),
                  _stages()
            {
            }

            /**
             * destructor  hetcompute::pipeline<info> p;
             */
            ~pipeline_skeleton_base()
            {
                for (auto stage : _stages)
                    delete stage;
                _stages.clear();
            }

            /**
             * copy constructor
             */
            pipeline_skeleton_base(pipeline_skeleton_base const& other)
                : _frozen(false),
                  _has_sliding_window(false),
                  _launch_type(other._launch_type),
                  _num_stages(other._num_stages),
                  _preprocessed(false),
                  _skeleton_lock(),
                  _stages()
            {
                for (auto stage : other._stages)
                {
                    _stages.push_back(stage->clone());
                }
            }

            /**
             * freeze the pipeline so that
             * no stages can be added once the pipeline is launched
             */
            void freeze() { _frozen = true; }

            /**
             * compute the gcd of the iteration rate pair
             * @param i the ith stage
             * @return size_t the gcd of the iteraiton rate of stage i in the skeleton
             */
            size_t gcd_stage_iter_rate(size_t i) const
            {
                size_t a = _stages[i]->_rate_curr;
                size_t b = _stages[i]->_rate_pred;

                HETCOMPUTE_INTERNAL_ASSERT(a != 0, "Stage iter rate cannot be zero.");
                HETCOMPUTE_INTERNAL_ASSERT(b != 0, "Stage iter rate cannot be zero.");

                return pipeline_utility::get_gcd(a, b);
            }

            /**
             * Check whether the pipeline skeleton is empty or not
             */
            bool is_empty() const { return _stages.size() == 0 ? true : false; }

            /**
             * Preprocessing pipeline stages
             */
            void preprocess();

            /**
             * pipeline sanity check
             * check: 1. stage IO types
             *        2. sliding window size
             *        ...
             * @return
             *   true - pass
             *   false - fail
             */
            bool is_valid();

            /**
             * Set launch type.
             */
            void set_launch_type(pipeline_launch_type launch_type) { _launch_type = launch_type; }

            /**
             * Get launch type.
             */
            pipeline_launch_type get_launch_type() { return _launch_type; }

            // Forbid moving and assignment
            HETCOMPUTE_DELETE_METHOD(pipeline_skeleton_base(pipeline_skeleton_base&& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_skeleton_base& operator=(pipeline_skeleton_base const& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_skeleton_base& operator=(pipeline_skeleton_base&& other));

            // friend classes
            friend class pipeline_instance_base;
            friend class test::pipeline_tester;
            template <typename... UserData>
            friend class pipeline_instance;
        };
        // class pipeline_skeleton_base

        /**
         * pipeline skeleton class
         */
        template <typename... UserData>
        class pipeline_skeleton : public pipeline_skeleton_base,
                                  public ref_counted_object<pipeline_skeleton<UserData...>, hetcomputeptrs::default_logger>
        {
        public:
            /**
             * constructor
             */
            pipeline_skeleton() : pipeline_skeleton_base() {}

            /**
             * copy constructor
             */
            pipeline_skeleton(pipeline_skeleton const& other)
                : pipeline_skeleton_base(other), ref_counted_object<pipeline_skeleton<UserData...>, hetcomputeptrs::default_logger>()
            {
            }

            /**
             * Add a cpu stage to the pipeline skeleton
             *
             * @throw hetcompute::api_exception if pipeline is already launched.
             *
             * @param StageType hetcompute::serial_stage or hetcompute::parallel_stage
             * @param bool has_lag define lag or not
             * @param lag hetcompute::iteration_lag
             * @param rate hetcompute::iteration_rate
             * @param sw hetcompute::sliding_window_size specify the size of the sliding window
             * @param body body of the pipeline stage
             *  i.e. a function pointer or a lambda expression or a callable object
             */
            template <typename StageType, typename Body>
            void add_cpu_stage(StageType const&                stage,
                               Body&&                          body,
                               bool const&                     has_lag,
                               iteration_lag const&            lag,
                               iteration_rate const&           rate,
                               sliding_window_size const&      sw,
                               hetcompute::pattern::tuner const& t)
            {
                HETCOMPUTE_API_THROW(_frozen == false, "Cannot add stage after the pipeline is launched.");

                _stages.push_back(
                    new pipeline_cpu_stage_skeleton<Body, UserData...>(std::forward<Body>(body), has_lag, _num_stages++, lag, rate, sw, stage, t));

                if (sw._size > 0)
                    _has_sliding_window = true;
            }

#ifdef HETCOMPUTE_HAVE_GPU
            /**
             * Add a serial gpu stage to the pipeline skeleton
             * @param StageType hetcompute::serial_stage or hetcompute::parallel_stage
             * @param BeforeBody&& before lambda for a gpu stage,
             * @param GKBody the pointer to the gpu kernel of type hetcompute::kernel_ptr
             * @param AfterBody&& before lambda for a gpu stage,
             *        AfterBody = void if no before lambda is provided, for this case,
             *        abody is of dummy type int*
             *        If provided, abody should be a function pointer or a lambda expression or a callable object
             * @param bool has_lag define lag or not
             * @param lag hetcompute::iteration_lag
             * @param rate hetcompute::iteration_rate
             * @param sw hetcompute::sliding_window_size specify the size of the sliding window
             * @param body body of the pipeline stage
             */
            template <typename StageType, typename BeforeBody, typename GKBody, typename AfterBody>
            void add_gpu_stage(StageType const&                stage,
                               BeforeBody&&                    bbody,
                               GKBody&&                        gk_ptr,
                               AfterBody&&                     abody,
                               bool                            has_lag,
                               iteration_lag const&            lag,
                               iteration_rate const&           rate,
                               sliding_window_size const&      sw,
                               hetcompute::pattern::tuner const& t)
            {
                HETCOMPUTE_API_ASSERT(_frozen == false, "Cannot add stage after the pipeline is launched.");

                _stages.push_back(new pipeline_gpu_stage_skeleton<BeforeBody, GKBody, AfterBody, UserData...>(std::forward<BeforeBody>(bbody),
                                                                                                              std::forward<GKBody>(gk_ptr),
                                                                                                              std::forward<AfterBody>(abody),
                                                                                                              has_lag,
                                                                                                              _num_stages++,
                                                                                                              lag,
                                                                                                              rate,
                                                                                                              sw,
                                                                                                              stage,
                                                                                                              t));

                if (sw._size > 0)
                {
                    _has_sliding_window = true;
                }
            }
#endif // HETCOMPUTE_HAVE_GPU

            // Make class pipeline friend
            friend class pipeline_instance_base;

            // Forbid moving and assignment
            HETCOMPUTE_DELETE_METHOD(pipeline_skeleton(pipeline_skeleton&& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_skeleton& operator=(pipeline_skeleton const& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_skeleton& operator=(pipeline_skeleton&& other));
        };
        // end of class pipeline_skeleton<UserData...> : public pipeline_skeleton_base
    }; // namespace internal
};     // namespace hetcompute
