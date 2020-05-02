/** @file pipeline.hh */
#pragma once

// Include std headers
#include <vector>

// Include internal headers
#include <hetcompute/internal/util/macros.hh>
#include <hetcompute/internal/patterns/pipeline/pipelineskeleton.hh>

namespace hetcompute
{
    namespace internal
    {
        template <hetcompute::mem_order const mem_order>
        struct comp_result_w_total_iters;

        template <>
        struct comp_result_w_total_iters<hetcompute::mem_order_relaxed>
        {
            static void comp(size_t& result, size_t total_iters) { result = result > total_iters ? total_iters : result; }
        };

        template <>
        struct comp_result_w_total_iters<hetcompute::mem_order_seq_cst>
        {
            static void comp(size_t& result, size_t total_iters)
            {
                if (total_iters > 0)
                {
                    result = result > total_iters ? total_iters : result;
                }
            }
        };

        /**
         * pipeline_stage_instance class
         */
        class pipeline_stage_instance
        {
        protected:
            // data buffer size
            size_t _data_buf_size;

            // degree of concurrency for the stage
            size_t const _doc;

            // numerical stage id
            size_t const _id;

            // number of iterations ahead for doc dependency
            size_t _iter_ahead;

            // current stage lag
            size_t const _lag;

            // lag value for the successor stage
            size_t _lag_succ;

            // iteration weight of the successor's lag
            size_t _lag_succ_iter_weight;
            size_t _lag_succ_iter_offset;

            // last token that is consumed in this stage
            size_t _last_token_consumed;

            // last token that is released from this stage
            size_t _last_token_released;

            // the id of the last token that has been passed to the next stage
            // be accessed by multiple threads sequentially, mem_order_related
            std::atomic<size_t> _last_token_passed;

            pipeline_instance_base* _pipeline_instance;

            size_t _rate_curr;
            size_t _rate_succ;

            pipeline_stage_skeleton_base* _stage_skeleton_ptr;

            // total iteration numbers for the current stage
            // be accessed by multiple threads sequentially, mem_order_related
            std::atomic<size_t> _total_iters;

            pipeline_stage_type const _type;

            // stage stage for state-based scheduling
            size_t                   _iters_active;
            size_t                   _iters_done;
            std::mutex               _mutex;
            bool*                    _done;
            size_t                   _max_doc_rate;
            size_t                   _min_init_iters;
            pipeline_stage_instance* _next;
            pipeline_stage_instance* _prev;
            bool                     _split_stage_work;
            size_t                   _chunk_size;

        public:
            /**
             * constructor
             */
            pipeline_stage_instance(pipeline_stage_skeleton_base* stage_ptr, pipeline_instance_base* pinst)
                : _data_buf_size(0),
                  _doc(stage_ptr->_doc),
                  _id(stage_ptr->_id),
                  _iter_ahead(0),
                  _lag(stage_ptr->_lag),
                  _lag_succ(0),
                  _lag_succ_iter_weight(0),
                  _lag_succ_iter_offset(0),
                  _last_token_consumed(1),
                  _last_token_released(0),
                  _last_token_passed(0),
                  _pipeline_instance(pinst),
                  _rate_curr(0),
                  _rate_succ(0),
                  _stage_skeleton_ptr(stage_ptr),
                  _total_iters(0),
                  _type(stage_ptr->_type), // state-based implementation
                  _iters_active(0),
                  _iters_done(0),
                  _mutex(),
                  _done(nullptr),
                  _max_doc_rate(0),
                  _min_init_iters(0),
                  _next(nullptr),
                  _prev(nullptr),
                  _split_stage_work(true),
                  _chunk_size(0)
            {
                if (is_serial())
                {
                    _split_stage_work = false;
                }

                auto tuner_chunk_size = _stage_skeleton_ptr->_tuner.get_chunk_size();
                // _chunk_size: pipeline stage iteration chunk size for iteration fusion
                // This is meant for performance tuning, but not for correctness.
                // Therefore, for parallel stages, _chunk_size <= _doc
                // For serial stages, we currently allow _chunk_size > 1 but the programmer
                // needs to make sure the algorithm is correct for chunking like this.
                if ((is_parallel() && tuner_chunk_size > 1) || is_serial())
                {
                    _chunk_size = tuner_chunk_size;
                }

                if (is_serial() && stage_ptr->_hetero_type != pipeline_stage_hetero_type::gpu)
                {
                    _iter_ahead = tuner_chunk_size;
                }
                else
                {
                    _iter_ahead = stage_ptr->_doc;
                }

                _done = new bool[_iter_ahead];
                memset(_done, 0, sizeof(bool) * _iter_ahead);

                HETCOMPUTE_INTERNAL_ASSERT(_iter_ahead != 0, "Stage degree of concurrency cannot be 0.");
            }

            /**
             * destructor
             */
            virtual ~pipeline_stage_instance() { delete[] _done; }

            /**
             * Perform stage function on the token
             *
             * @param in_first_idx the index to the first token in the the inbuf for
             *                     this stage iteration.
             * @param in_size      the number of input tokens for this iteration
             * @param in_buf       pointer to the input token buffer
             * @param out_idx      buffer index for the output token
             * @param out_buf      pointer to the output token buffer
             * @param iter_id      current iteration id
             * @param launch_type
             *   hetcompute::internal::with_sliding_window
             *   hetcompute::internal::without_sliding_window
             */
            void apply(size_t               in_first_idx,
                       size_t               in_size,
                       stagebuffer*         in_buf,
                       size_t               out_idx,
                       stagebuffer*         out_buf,
                       size_t               iter_id,
                       pipeline_launch_type launch_type)
            {
                auto total_iters = _total_iters.load(hetcompute::mem_order_relaxed);

                _stage_skeleton_ptr
                    ->apply(in_first_idx, in_size, in_buf, out_idx, out_buf, iter_id, total_iters, this, _pipeline_instance, launch_type);
            };

#ifdef HETCOMPUTE_HAVE_GPU
            /**
             * Perform GPU stage before function
             *
             * @param in_first_idx the index to the first token in the the inbuf for
             *                     this stage iteration.
             * @param in_size      the number of input tokens for this iteration
             * @param in_buf       pointer to the input token buffer
             * @param iter_id      current iteration id
             * @param launch_type
             *   hetcompute::with_sliding_window
             *   hetcompute::without_sliding_window
             * @return std::pair<task_ptr<>, void*> where task_ptr<> points to the
             *         GPU task of the gpu kernel; void* points to the return :
             */
            std::pair<task_ptr<>, void*>
            apply_before(size_t in_first_idx, size_t in_size, stagebuffer* in_buf, size_t iter_id, pipeline_launch_type launch_type)
            {
                auto total_iters = _total_iters.load(hetcompute::mem_order_relaxed);

                return _stage_skeleton_ptr
                    ->apply_before(in_first_idx, in_size, in_buf, iter_id, total_iters, this, _pipeline_instance, launch_type);
            };

            /**
             * Perform GPU stage after function
             *
             * @param out_idx      buffer index for the output token
             * @param out_buf      pointer to the output token buffer
             * @param iter_id      current iteration id
             * @param launch_type
             *   hetcompute::with_sliding_window
             *   hetcompute::without_sliding_window
             * @param gk_tp gpu parameter tuple
             */
            void apply_after(size_t out_idx, stagebuffer* out_buf, size_t iter_id, pipeline_launch_type launch_type, void* gk_tp)
            {
                auto total_iters = _total_iters.load(hetcompute::mem_order_relaxed);
                _stage_skeleton_ptr->apply_after(out_idx, out_buf, iter_id, total_iters, this, _pipeline_instance, launch_type, gk_tp);
            };
#endif // HETCOMPUTE_HAVE_GPU

            /**
             * Allocate buffers between stages
             *
             * @param buf reference to the pointer of the buffer to allocate
             * @param size size of the buffer to allocate
             * @param pipeline_launch_type
             */
            void alloc_stage_buf(stagebuffer*& buf, size_t size, pipeline_launch_type type)
            {
                _stage_skeleton_ptr->alloc_stage_buf(buf, size, type);
            };

            /**
             * Release the output buffer for this stage
             * @param reference of the pointer to the buffer, the value will be reset
             */
            void free_stage_buf(stagebuffer*& buf) { _stage_skeleton_ptr->free_stage_buf(buf); }

            /**
             * Get the info of the return type of the stage function for type checking
             * @return size_t
             */
            size_t get_return_type() const { return _stage_skeleton_ptr->get_return_type(); }

            /**
             * Get the info of the argument type of the stage function for type checking
             * @return size_t
             */
            size_t get_arg_type() const { return _stage_skeleton_ptr->get_arg_type(); }

            /**
             * check how much more iterations can be done
             */
            template <hetcompute::mem_order const mem_order>
            size_t fetch_some_work(pipeline_launch_type launch_type) const
            {
                // if an iteration is active for serial stage, no need to generate more
                if (_doc == 1 && _iters_active > _iters_done)
                {
                    return 0;
                }

                // check how many is ready from the previous stage
                size_t from_prev  = 0;
                size_t result     = std::numeric_limits<size_t>::max();
                auto   prev_stage = _prev;

                if (prev_stage)
                {
                    from_prev = prev_stage->template get_num_iters_for_next_stage_can_be_done<mem_order>();
                    result    = from_prev;
                }

                // check how many is released from the next stage due to sliding window usage
                size_t from_next  = 0;
                auto   next_stage = _next;

                if (next_stage && launch_type == with_sliding_window)
                {
                    from_next = ((next_stage->_iters_done + _lag_succ + 1) * _rate_curr - 1) / _rate_succ + _iter_ahead;

                    from_next = from_next > _min_init_iters ? from_next : _min_init_iters;

                    result = result > from_next ? from_next : result;
                }

                // do not exceed the total number of iterations
                // _total_iters may be set by the other tasks dynamically
                auto total_iters = _total_iters.load(mem_order);
                comp_result_w_total_iters<mem_order>::comp(result, total_iters);

                // do not exceed the doc boundary
                // result should not be smaller than _iters_active
                // (may happen for on the fly stop, need special handling so that result won't underflow)
                result = result > _iters_done + _iter_ahead ? _iters_done + _iter_ahead : result;
                result = result > _iters_active ? result - _iters_active : 0;

                if (_chunk_size == 0)
                {
                    return (result + 1) >> 1;
                }

                return result > _chunk_size ? _chunk_size : result;
            }

            /**
             * get the iter_id for the next stage give the _iters_done in the current stage
             */
            template <hetcompute::mem_order const mem_order>
            size_t get_num_iters_for_next_stage_can_be_done() const
            {
                size_t const iters_done        = _iters_done;
                size_t       scaled_iters_done = iters_done * _rate_succ / _rate_curr;
                size_t       total_iters       = _total_iters.load(mem_order);

                if (total_iters > 0 && iters_done >= total_iters)
                {
                    return scaled_iters_done;
                }

                if (scaled_iters_done < _lag_succ)
                {
                    return 0;
                }

                return scaled_iters_done - _lag_succ;
            }

            /**
             * Check if current stage is done
             */
            template <hetcompute::mem_order const mem_order>
            bool is_finished() const
            {
                size_t total_iters = _total_iters.load(mem_order);

                // if total_iters == 0, on-the-fly stop, total iters not set yet
                return (total_iters > 0) && (_iters_done >= total_iters);
            }

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
                return _type == pipeline_stage_type::serial_in_order || _type == pipeline_stage_type::serial_out_of_order || _doc == 1;
            }

            /**
             * Check if the stage is a serial in-order stage
             * @return
             * true - the stage is a serial in-order stage
             * false - the stage is not a serial in-order stage
             */
            bool is_serial_in_order() const { return _type == pipeline_stage_type::serial_in_order; }

            /**
             * Check if the stage is a serial out-of-order stage
             * @return
             * true - the stage is a serial out-of-order stage
             * false - the stage is not a serial out-of-order stage
             */
            bool is_serial_out_of_order() const { return _type == pipeline_stage_type::serial_out_of_order; }

            // Forbid copying and assignment
            HETCOMPUTE_DELETE_METHOD(pipeline_stage_instance(pipeline_stage_instance const& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_stage_instance(pipeline_stage_instance&& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_stage_instance& operator=(pipeline_stage_instance const& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_stage_instance& operator=(pipeline_stage_instance&& other));

            // friend classes
            friend class pipeline_instance_base;
            friend class ::hetcompute::pipeline_context_base;
        };
        // end of class pipeline_stage_instance

        /**
         * pipeline_instance class
         */
        class pipeline_instance_base
        {
            static size_t const _max_active_tasks;
            static size_t const _aggressive_fetch_cutoff_num_iters;

        protected:
            hetcompute::group_ptr                   _group;
            std::atomic<bool>                     _is_stopped;
            bool                                  _launch_with_iterations;
            size_t                                _num_stages;
            size_t                                _num_tokens;
            pipeline_skeleton_base*               _skeleton;
            std::vector<pipeline_stage_instance*> _stages;
            std::vector<stagebuffer*>             _stage_buf_ptrs;
            bool                                  _split_serial_stage_work;
            // state-based implementation
            std::atomic<size_t> _active_tasks;
            std::atomic<bool>   _pipeline_finished;
#ifdef HETCOMPUTE_USE_PIPELINE_LOGGER
            std::atomic<size_t> _max_tasks;
            std::atomic<size_t> _num_tasks;
#endif

        public:
            /**
             * pipeline_instance constructor
             */
            explicit pipeline_instance_base(pipeline_skeleton_base* p)
                : _group(nullptr),
                  _is_stopped(false),
                  _launch_with_iterations(false),
                  _num_stages(p->_num_stages),
                  _num_tokens(0),
                  _skeleton(p),
                  _stages(),
                  _stage_buf_ptrs(p->_num_stages),
                  _split_serial_stage_work(false),
                  _active_tasks(0),
                  _pipeline_finished(false)
#ifdef HETCOMPUTE_USE_PIPELINE_LOGGER
                  ,
                  _max_tasks(0),
                  _num_tasks(0),
                  _task_counts()
#endif
            {
                for (size_t i = 0; i < _skeleton->_stages.size(); i++)
                {
                    _stages.push_back(new pipeline_stage_instance(_skeleton->_stages[i], this));
                    if (i == 0)
                    {
                        continue;
                    }
                    _stages[i]->_prev     = _stages[i - 1];
                    _stages[i - 1]->_next = _stages[i];
                }

                _group = hetcompute::create_group("pipieline instance.");
            };

            /**
             * pipeline_instance destructor
             */
            ~pipeline_instance_base()
            {
                for (auto stage : _stages)
                {
                    delete _stage_buf_ptrs[stage->_id];
                    delete stage;
                }
                _stages.clear();
            }

            /**
             * check the IO data type between two stages
             */
            void check_stage_io_type() const;

            /**
             * check the sliding windows size for the stages
             */
            void check_stage_sliding_window_size() const;

            /**
             * Cancel the pipeline.
             */
            void cancel() { _group->cancel(); }

            /**
             * Check if the first stage is serial or parallel
             */
            bool is_first_stage_serial()
            {
                HETCOMPUTE_INTERNAL_ASSERT(_stages.size() > 0, "Pipeline cannot be empty.");
                return _stages[0]->is_serial();
            }

            /**
             * @param stage_id
             * @param iter_id
             * @param launch_type, i.e. with_sliding_window or without_sliding_window
             * @param mem_order
             */
            void perform_cpu_stage(size_t stage_id, size_t iter_id, pipeline_launch_type launch_type, std::memory_order mem_order);

#ifdef HETCOMPUTE_HAVE_GPU
            /**
             * Perform the pipeline gpu stage function for iteration of one stage
             * @param stage_id
             * @param iter_id
             * @param launch_type, i.e. with_sliding_window or without_sliding_window
             * @param mem_order
             */
            template <const std::memory_order mem_order>
            void perform_gpu_stage(size_t stage_id, size_t iter_id, pipeline_launch_type launch_type)
            {
                auto tp = prepare_gpu_stage(stage_id, iter_id, launch_type, mem_order);

                auto gpu_tptr          = std::get<0>(tp);
                auto after_body_tp_ptr = std::get<1>(tp);

                _group->launch(gpu_tptr);

                auto cpu_tptr = hetcompute::create_task([stage_id, iter_id, launch_type, after_body_tp_ptr, this] {
                    postprocess_gpu_stage(stage_id, iter_id, launch_type, mem_order, after_body_tp_ptr);
                    if (get_load_on_current_thread() == 0)
                    {
                        _active_tasks++;
#ifdef HETCOMPUTE_USE_PIPELINE_LOGGER
                        _num_tasks.fetch_add(1, hetcompute::mem_order_relaxed);
#endif

                        size_t first_search_stage = stage_id == _num_stages - 1 ? _num_stages : stage_id + 2;
                        pipeline_task_aggressive_fetch<mem_order>(first_search_stage, launch_type);
                    }
                });

                gpu_tptr >> cpu_tptr;
                _group->launch(cpu_tptr);
            }

            /**
             * Perform the pipeline gpu stage function for iteration of one stage
             * @param stage_id
             * @param iter_id
             * @param launch_type, i.e. with_sliding_window or without_sliding_window
             * @param mem_order
             * @return a pair of a task_ptr<> which points to the gpu task for the gpu stage,
             */
            std::pair<task_ptr<>, void*>
            prepare_gpu_stage(size_t stage_id, size_t iter_id, pipeline_launch_type launch_type, std::memory_order mem_order);

            /**
             * Perform the pipeline gpu stage function for iteration of one stage
             * @param stage_id
             * @param iter_id
             * @param launch_type, i.e. with_sliding_window or without_sliding_window
             * @param mem_order
             */
            void
            postprocess_gpu_stage(size_t stage_id, size_t iter_id, pipeline_launch_type launch_type, std::memory_order mem_order, void* gk_tp);
#endif // HETCOMPUTE_HAVE_GPU

            /**
             * compute the gcd of the iteration rate pair
             * @param i the ith stage
             * @return size_t the gcd of the iteraiton rate of stage i in the skeleton
             */
            size_t gcd_stage_iter_rate(size_t i)
            {
                size_t a = _stages[i]->_stage_skeleton_ptr->_rate_curr;
                size_t b = _stages[i]->_stage_skeleton_ptr->_rate_pred;

                HETCOMPUTE_INTERNAL_ASSERT(a != 0, "Stage iter rate cannot be zero.");
                HETCOMPUTE_INTERNAL_ASSERT(b != 0, "Stage iter rate cannot be zero.");

                return pipeline_utility::get_gcd(a, b);
            }

            /**
             * Get the information of predecessor iterations
             * @param stage_id
             * @param curr_iter
             * @param size_t& pred_input_biter the beginning predecessor iteration whose
             *        token will be consumed by curr_iter
             * @param size_t& pred_input_eiter the end predecessor iteration whose
             *        token will be consumed by curr_iter. This iteration is also the
             *        one who launches curr_iter
             * @param size_t& pred_input_size the input size from the predecessor stage
             */
            void get_predecessor_iter_info(size_t              stage_id,
                                           size_t              curr_iter,
                                           size_t&             pred_input_biter,
                                           size_t&             pred_input_eiter,
                                           size_t&             pred_input_size,
                                           hetcompute::mem_order mem_order) const
            {
                // first stage, no input
                if (stage_id == 0)
                {
                    pred_input_biter = 0;
                    pred_input_eiter = 0;
                    pred_input_size  = 0;
                    return;
                }

                // Arithmatic for how to get the iteration info for the predecessor stage
                // the other stages
                // a = pred_stage->_rate_curr
                // b = pred_stage->_rate_succ
                //
                // --------------------??????----------------------------------
                // a == b (== 1), iter_offset = curr_lag
                // pred_launch_iter = curr_iter + curr_lag;
                // input_size = curr_lag + 1;
                // if pred_launch_iter within pred_stage._total_iters:
                //    pred_input_eiter = pred_launch_iter;
                //    pred_input_biter = pred_launch_iter - input_size + 1;
                // Otherwise.
                //    pred_input_eiter = pred_stage._total_iters
                //    pred_input_biter = pred_launch_iter - input_size + 1;
                //
                // a < b, iter_offset = ceiling{(curr_lag + 1) * a / b} - 1
                // pred_launch_iter = ceiling{(curr_iter + curr_lag) * a / b}
                // input_size = iter_offset + 1;
                // if pred_launch_iter within pred_stage._total_iters:
                //    pred_input_eiter = pred_launch_iter;
                //    pred_input_biter = pred_launch_iter - input_size + 1;
                // Otherwise.
                //    pred_input_eiter = pred_stage._total_iters
                //    pred_input_biter = pred_launch_iter - input_size + 1;
                // -----------------------------------------------------------
                //
                // a <= b
                // pred_launch_iter = ceiling{(curr_iter + curr_lag) * a / b}
                // pred_launch_iter_wo_lag = ceiling{curr_iter * a / b}
                //
                // if pred_launch_iter within pred_stage._total_iters:
                //    pred_input_eiter = pred_launch_iter;
                //    pred_input_biter = pred_launch_iter_wo_lag;
                // Otherwise.
                //    pred_input_eiter = pred_stage._total_iters
                //    pred_input_biter = pred_launch_iter_wo_lag;
                //
                // a > b,
                // pred_launch_iter = ceiling{(curr_iter + curr_lag) * a / b}
                // pred_launch_prev_iter_wo_lag = ceiling{(curr_iter - 1) * a / b}
                // if pred_launch_iter within pred_stage._total_iters:
                //    pred_input_eiter = pred_launch_iter;
                //    pred_input_biter = pred_launch_prev_iter_wo_lag + 1;
                //  otherwise:
                //    pred_input_eiter = pred_stage._total_iters
                //    pred_input_biter = pred_launch_prev_iter_wo_lag + 1;

                auto   pred_stage = _stages[stage_id - 1];
                auto   curr_stage = _stages[stage_id];
                size_t curr_lag   = curr_stage->_lag;
                size_t a          = pred_stage->_rate_curr;
                size_t b          = pred_stage->_rate_succ;

                auto pred_total_iters = pred_stage->_total_iters.load(mem_order);

                if (a <= b)
                {
                    size_t pred_launch_iter        = pipeline_utility::get_pred_iter(a, b, curr_iter + curr_lag);
                    size_t pred_launch_iter_wo_lag = pipeline_utility::get_pred_iter(a, b, curr_iter);

                    pred_input_biter = pred_launch_iter_wo_lag;

                    pred_input_eiter = pred_total_iters == 0 ? pred_launch_iter :
                                                               pred_launch_iter <= pred_total_iters ? pred_launch_iter : pred_total_iters;
                }
                else
                {
                    size_t pred_launch_iter             = pipeline_utility::get_pred_iter(a, b, curr_iter + curr_lag);
                    size_t pred_launch_prev_iter_wo_lag = pipeline_utility::get_pred_iter(a, b, curr_iter - 1);

                    pred_input_biter = pred_launch_prev_iter_wo_lag + 1;

                    pred_input_eiter = pred_total_iters == 0 ? pred_launch_iter :
                                                               pred_launch_iter <= pred_total_iters ? pred_launch_iter : pred_total_iters;
                }
                pred_input_size = pred_input_eiter - pred_input_biter + 1;
            }

            // state-based impelmentation

            /**
             * initialize basic stage features
             */
            void initialize_stages();

            /**
             * initialize the pipeline instance (with sliding window)
             * state-based internal
             */
            void initialize_stb(pipeline_launch_type launch_type);

            /**
             * state-based pipeline internal task
             * @param stage_id
             * @param iter_id
             * @param mem_order
             *    hetcompute::mem_order_relaxed for launch with num_iterations specified
             *    hetcompute::mem_order_seq_cst for launch with on-the-fly stop
             */
            template <hetcompute::mem_order const mem_order>
            void pipeline_task_aggressive_fetch(size_t init_stage, pipeline_launch_type launch_type)
            {
                size_t       count                             = 0;
                size_t       first_search_stage                = init_stage;
                size_t const aggressive_fetch_cutoff_num_iters = 4;

                while (1)
                {
                    bool found_work = false;

                    for (size_t i = first_search_stage; i > 0; i--)
                    {
                        auto curr_stage = _stages[i - 1];

                        if (curr_stage->_mutex.try_lock() == false)
                            continue;

                        // at this point, the lock of the curr_stage is taken
                        // try to find some work to do
                        size_t fetch_work = curr_stage->template fetch_some_work<mem_order>(launch_type);

                        if (fetch_work == 0)
                        {
                            curr_stage->_mutex.unlock();
                            continue;
                        }

                        // found work, update state first
                        found_work        = true;
                        size_t first_iter = curr_stage->_iters_active + 1;
                        curr_stage->_iters_active += fetch_work;
                        curr_stage->_mutex.unlock();

#ifdef HETCOMPUTE_HAVE_GPU
                        if (curr_stage->_stage_skeleton_ptr->_hetero_type == pipeline_stage_hetero_type::gpu)
                        {
                            const size_t last_iter = first_iter + fetch_work;

                            size_t total_iters = _stages[i - 1]->_total_iters.load(mem_order);
                            for (size_t y = first_iter; (total_iters == 0 || y <= total_iters) && y < last_iter; ++y)
                            {
                                perform_gpu_stage<mem_order>(i - 1, y, launch_type);
                                total_iters = _stages[i - 1]->_total_iters.load(mem_order);
                            }
                            continue;
                        }
#endif // HETCOMPUTE_HAVE_GPU

                        // perform the work
                        // do not exceed the total number of iterations
                        // _total_iters may be set by the other tasks dynamically
                        size_t const last_iter = first_iter + fetch_work;
                        size_t       y_done    = first_iter;

                        size_t total_iters = _stages[i - 1]->_total_iters.load(mem_order);
                        for (; (total_iters == 0 || y_done <= total_iters) && y_done < last_iter; ++y_done)
                        {
                            perform_cpu_stage(i - 1, y_done, launch_type, mem_order);
                            total_iters = _stages[i - 1]->_total_iters.load(mem_order);
                        }

                        // update the stage state again after the work is done
                        HETCOMPUTE_INTERNAL_ASSERT(!curr_stage->is_parallel() || fetch_work <= curr_stage->_doc, "fetch too much work.");

                        if (curr_stage->is_parallel())
                        {
                            std::lock_guard<std::mutex> l(curr_stage->_mutex);

                            for (size_t y = first_iter; y < y_done; ++y)
                                curr_stage->_done[y % curr_stage->_doc] = true;

                            size_t inc_iters_done = curr_stage->_iters_done + 1;

                            while (curr_stage->_done[(inc_iters_done) % curr_stage->_doc] == true)
                            {
                                curr_stage->_done[inc_iters_done % curr_stage->_doc] = false;
                                inc_iters_done++;
                            }
                            curr_stage->_iters_done = inc_iters_done - 1;
                        }
                        else
                        {
                            std::lock_guard<std::mutex> l(curr_stage->_mutex);
                            curr_stage->_iters_done += y_done - first_iter;
                        }

                        // launch more tasks if needed
                        first_search_stage = i == _num_stages ? _num_stages : i + 1;

                        if (get_load_on_current_thread() == 0)
                        {
                            _active_tasks++;
#ifdef HETCOMPUTE_USE_PIPELINE_LOGGER
                            _num_tasks.fetch_add(1, hetcompute::mem_order_relaxed);
#endif
                            _group->launch([this, i, launch_type] { pipeline_task_aggressive_fetch<mem_order>(i, launch_type); });
                        }
                        break;
                    }

                    // start stage traversal again
                    if (found_work)
                    {
                        count = 0;
                        continue;
                    }

                    // increment the counter for trials
                    count++;

                    if (count < aggressive_fetch_cutoff_num_iters)
                        continue;

                    // task will die if there's no work to do after couple of trials
                    auto new_active_tasks = --_active_tasks;

                    // task will continue if it is the only one running
                    // and the pipeline is not finished yet
                    if (new_active_tasks == 0 && !_stages[_num_stages - 1]->template is_finished<mem_order>())
                    {
                        first_search_stage = _num_stages;
                        count              = 0;
                        // this only happens when there is only one task in the system for this
                        // pipeline instance. There will be a memory fence happen before any
                        // further read or write to _active_tasks in other threads since
                        // a mutex needs to be aquired for stage state checking/updating which
                        // is the only reason more tasks will be further created and run in
                        // different threads. Therefore, mem_order_relaxed should be good
                        // enough for here
                        _active_tasks.store(1, hetcompute::mem_order_relaxed);

                        continue;
                    }

                    // task dies
                    break;
                }
            }

            /**
             * launch the pipeline
             * state-base pipeline internal
             * @param num_iterations the total number of iterations for the first stage
             * @param launch_type
             *   with_sliding_window
             *   without_sliding_window
             * @param t pattern tuner for the pipeline.
             *        Current tunable parameter for a pipeline is <code>doc</code>,
             *        i.e. the initial number of tasks can be launched for running a pipeline.
             */
            template <hetcompute::mem_order const mem_order>
            void launch_stb(size_t num_iterations, pipeline_launch_type launch_type, hetcompute::pattern::tuner const& t)
            {
                // check the stage io types
                check_stage_io_type();

                // preprocess and initialization
                _skeleton->preprocess();

                initialize_stb(launch_type);

                // launch when the total number of pipeline iteration is already known
                if (mem_order == hetcompute::mem_order_relaxed)
                    setup_stage_total_iterations(num_iterations);

#ifdef HETCOMPUTE_USE_PIPELINE_LOGGER
                _task_counts[_active_tasks.load()]++;
#endif

                // launch tasks
                _active_tasks = 1;

#ifdef HETCOMPUTE_USE_PIPELINE_LOGGER
                _num_tasks.fetch_add(1, hetcompute::mem_order_relaxed);
#endif

                // launch the first task
                // If the first stage is a serial stage, launch only one task.
                // Otherwise, launch multiple tasks according to the tuner setting.

                size_t num_init_tasks = is_first_stage_serial() ? 1 : t.get_doc();

                for (size_t i = 0; i < num_init_tasks; i++)
                    _group->launch([this, launch_type] { pipeline_task_aggressive_fetch<mem_order>(1, launch_type); });

#ifdef HETCOMPUTE_USE_PIPELINE_LOGGER
                HETCOMPUTE_ILOG("\033[31m *** creating %zu task, max %zu at one time *** \033[0m\n", _num_tasks.load(), _max_tasks.load());
#endif
            }

            /**
             * setup total iters for all the stages if the num_iterations is known
             * size_t num_iterations
             */
            void setup_stage_total_iterations(size_t num_iterations);

            /**
             * Set whether to split the serial stage work or not
             */
            void set_split_serial_stage_work(bool s) { _split_serial_stage_work = s; }

            // Forbid copying and assignment
            HETCOMPUTE_DELETE_METHOD(pipeline_instance_base(pipeline_instance_base const& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_instance_base(pipeline_instance_base&& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_instance_base& operator=(pipeline_instance_base const& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_instance_base& operator=(pipeline_instance_base&& other));

            friend class ::hetcompute::pipeline_context_base;
        };
        // end of class pipeline_instance_base

        template <typename... UserData>
        class pipeline_instance;

        /**
         * pipeline_instance class without user data
         */
        template <>
        class pipeline_instance<> : public pipeline_instance_base,
                                    public ref_counted_object<pipeline_instance<>, hetcomputeptrs::default_logger>
        {
            hetcompute_shared_ptr<pipeline_skeleton<>> _skeleton_shared_ptr;

        public:
            /**
             * pipeline_instance constructor
             */
            explicit pipeline_instance(hetcompute_shared_ptr<pipeline_skeleton<>> p)
                : pipeline_instance_base(static_cast<pipeline_skeleton_base*>(c_ptr(p))), _skeleton_shared_ptr(p){};

            /**
             * pipeline_instance destructor
             */
            virtual ~pipeline_instance() {}

            /**
             * wait for the pipeline group
             */
            void finish_after() { hetcompute::finish_after(_group); }

            /**
             * launch the pipeline
             * @param num_iterations the total number of iterations for the first stage
             * @param launch_type  with_sliding_window or without_sliding_window
             * @param t pattern tuner for the pipeline.
             *        Current tunable parameter for a pipeline is <code>doc</code>,
             *        i.e. the initial number of tasks can be launched for running a pipeline.
             */
            void launch(size_t num_iterations, pipeline_launch_type launch_type, const hetcompute::pattern::tuner& t)
            {
                if (launch_type == without_sliding_window)
                {
                    HETCOMPUTE_API_ASSERT(!_skeleton->_has_sliding_window,
                                        "Cannot launch without sliding window because some stages are using sliding windows.");
                }

                if (num_iterations == 0)
                {
                    _launch_with_iterations = false;
                    launch_stb<hetcompute::mem_order_seq_cst>(0, launch_type, t);
                }
                else
                {
                    _launch_with_iterations = true;
                    launch_stb<hetcompute::mem_order_relaxed>(num_iterations, launch_type, t);
                }
            }

            /**
             * wait for the pipeline group
             */
            hc_error wait_for() { return _group->wait_for(); }

            // Forbid copying and assignment
            HETCOMPUTE_DELETE_METHOD(pipeline_instance(pipeline_instance const& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_instance(pipeline_instance&& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_instance& operator=(pipeline_instance const& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_instance& operator=(pipeline_instance&& other));

            // Make friend classes
            friend class pipeline_context<>;
        };
        // end of class pipeline_instance<>:public pipeline_instance_base{

        /**
         * pipeline_instance class with user data
         */
        template <typename UserData>
        class pipeline_instance<UserData> : public pipeline_instance_base,
                                            public ref_counted_object<pipeline_instance<UserData>, hetcomputeptrs::default_logger>
        {
        private:
            hetcompute_shared_ptr<pipeline_skeleton<UserData>> _skeleton_shared_ptr;
            UserData*                                        _user_data;

        public:
            /**
             * pipeline_instance constructor
             */
            explicit pipeline_instance(hetcompute_shared_ptr<pipeline_skeleton<UserData>> p)
                : pipeline_instance_base(static_cast<pipeline_skeleton_base*>(c_ptr(p))), _skeleton_shared_ptr(p), _user_data(nullptr)
            {
            }

            /**
             * pipeline_instance destructor
             */
            virtual ~pipeline_instance() {}

            /**
             * finishe after the pipeline group
             */
            void finish_after() { hetcompute::finish_after(_group); }

            /**
             * launch the pipeline
             * @param context_data pointer to the data for the pipeline context
             * @param num_iterations the total number of iterations for the first stage
             * @param launch_type  with_sliding_window or without_sliding_window
             * @param t pattern tuner for the pipeline.
             *        Current tunable parameter for a pipeline is <code>doc</code>,
             *        i.e. the initial number of tasks can be launched for running a pipeline.
             */
            void launch(UserData* context_data, size_t num_iterations, pipeline_launch_type launch_type, const hetcompute::pattern::tuner& t)
            {
                if (launch_type == without_sliding_window)
                {
                    HETCOMPUTE_API_ASSERT(!_skeleton->_has_sliding_window,
                                        "Cannot launch without sliding window because some stages are using sliding windows.");
                }

                _user_data = context_data;

                if (num_iterations == 0)
                {
                    _launch_with_iterations = false;
                    launch_stb<hetcompute::mem_order_seq_cst>(0, launch_type, t);
                }
                else
                {
                    _launch_with_iterations = true;
                    launch_stb<hetcompute::mem_order_relaxed>(num_iterations, launch_type, t);
                }
            }

            /**
             * wait for the pipeline group
             */
            hc_error wait_for() { return _group->wait_for(); }

            // Forbid copying and assignment
            HETCOMPUTE_DELETE_METHOD(pipeline_instance(pipeline_instance const& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_instance(pipeline_instance&& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_instance& operator=(pipeline_instance const& other));
            HETCOMPUTE_DELETE_METHOD(pipeline_instance& operator=(pipeline_instance&& other));

            // Make friend classes
            friend class pipeline_context<UserData>;
        };
        // end of class pipeline_instance<UserData>:public pipeline_instance_base

    }; // namespace internal
};     // namespace hetcompute
