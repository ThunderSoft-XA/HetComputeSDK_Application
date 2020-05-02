/** @file pipelinefactory.hh */
#pragma once

// Include user interface headers
#include <hetcompute/taskfactory.hh>

// Include internal headers
#include <hetcompute/internal/util/macros.hh>
#include <hetcompute/internal/patterns/pipeline/pipelinebuffers.hh>

namespace hetcompute
{
    namespace internal
    {
        /**
         * pipeline stage functions
         * Implements stage functions with no input arguments
         */
        template <typename F>
        class stage_function_unary
        {
        public:
            using f_type      = typename function_traits<F>::type_in_task;
            using return_type = typename function_traits<F>::return_type;
            using arg0_type   = typename function_traits<F>::template arg_type<0>;
            using arg1_type   = void;

            /**
             * constructor
             */
            explicit stage_function_unary(F&& f) : _f(f) {}

            /**
             * copy constructor
             */
            stage_function_unary(stage_function_unary const& other) : _f(other._f) {}

            /**
             * destructor
             */
            virtual ~stage_function_unary(){};

            // forbid moving and assignment
            HETCOMPUTE_DELETE_METHOD(stage_function_unary(stage_function_unary&& other));
            HETCOMPUTE_DELETE_METHOD(stage_function_unary& operator=(stage_function_unary const& other));
            HETCOMPUTE_DELETE_METHOD(stage_function_unary& operator=(stage_function_unary&& other));

        public:
            f_type _f;
        };
        // end of class stage_function_unary

        /**
         * Implements stage functions with only one argument
         */
        template <typename F>
        class stage_function_binary
        {
        public:
            using f_type      = typename function_traits<F>::type_in_task;
            using return_type = typename function_traits<F>::return_type;
            using arg0_type   = typename function_traits<F>::template arg_type<0>;
            using arg1_type   = typename function_traits<F>::template arg_type<1>;

            /**
             * constructor
             */
            explicit stage_function_binary(F&& f) : _f(f) {}

            /**
             * copy constructor
             */
            stage_function_binary(stage_function_binary const& other) : _f(other._f) {}

            /**
             *
             * destructor
             */
            virtual ~stage_function_binary(){};

            // Forbid moving and assignment
            HETCOMPUTE_DELETE_METHOD(stage_function_binary(stage_function_binary&& other));
            HETCOMPUTE_DELETE_METHOD(stage_function_binary& operator=(stage_function_binary const& other));
            HETCOMPUTE_DELETE_METHOD(stage_function_binary& operator=(stage_function_binary&& other));

        public:
            f_type _f;
        };
        // end of class stage_function_binary

        /**
         * Dispatches to the correct implementation
         * of the stage function for a given arity
         */
        template <typename F, size_t Arity, typename RT>
        class cpu_stage_function_arity_dispatch;
        // end of class cpu_stage_function_arity_dispatch

        /**
         * Dispatches to the correct implementation
         * of the function only takes one argument which is the stage context (arity = 1)
         */
        template <typename F, typename RT>
        class cpu_stage_function_arity_dispatch<F, 1, RT> : public stage_function_unary<F>
        {
        public:
            using context_type       = typename stage_function_unary<F>::arg0_type;
            using context_type_noref = typename std::remove_reference<context_type>::type;
            using return_type        = typename stage_function_unary<F>::return_type;
            using arg1_type          = void;

            static_assert(std::is_default_constructible<return_type>::value, "The return type of a stage should be default constructible.");

            static_assert(std::is_copy_assignable<return_type>::value, "The return type of a stage should be copy-assignable.");

            /**
             * constructor
             */
            explicit cpu_stage_function_arity_dispatch(F&& f) : stage_function_unary<F>(std::forward<F>(f)) {}

            /**
             * copy constructor
             */
            cpu_stage_function_arity_dispatch(cpu_stage_function_arity_dispatch const& other) : stage_function_unary<F>(other) {}

            /**
             * Perform the stage function on the input token
             *
             * @param in_first_idx the index to the first token in the the inbuf for
             *                     this stage iteration.
             * @param in_size      the number of input tokens for this iteration
             * @param inbuf        pointer to the input token buffer
             * @param out_idx      buffer index for the output token
             * @param outbuf       pointer to the output token buffer
             * @param context      the reference to the iteration context
             * @param launch_type
             *   hetcompute::internal::with_sliding_window
             *   hetcompute::internal::without_sliding_window
             */

            template <typename IBT, typename OBT, typename>
            void apply_function(size_t, size_t, IBT*, size_t out_idx, OBT* outbuf, void* context, pipeline_launch_type)
            {
                HETCOMPUTE_INTERNAL_ASSERT(outbuf != nullptr, "The input buffer for the pipeline stage should not be allocated.");

                size_t oid     = outbuf->get_buffer_index(out_idx);
                (*outbuf)[oid] = this->_f(*static_cast<context_type_noref*>(context));
            }

            // Forbid moving and assignment
            HETCOMPUTE_DELETE_METHOD(cpu_stage_function_arity_dispatch(cpu_stage_function_arity_dispatch&& other));
            HETCOMPUTE_DELETE_METHOD(cpu_stage_function_arity_dispatch& operator=(cpu_stage_function_arity_dispatch const& other));
            HETCOMPUTE_DELETE_METHOD(cpu_stage_function_arity_dispatch& operator=(cpu_stage_function_arity_dispatch&& other));
        };
        // end of class cpu_stage_function_arity_dispatch<F, 1, RT>

        /**
         * Dispatches to the correct implementation
         * of the function for the first stage with
         * one argument (arity = 1) and returns void
         */
        template <typename F>
        class cpu_stage_function_arity_dispatch<F, 1, void> : public stage_function_unary<F>
        {
        public:
            using context_type       = typename stage_function_unary<F>::arg0_type;
            using context_type_noref = typename std::remove_reference<context_type>::type;
            using return_type        = typename stage_function_unary<F>::return_type;
            using arg1_type          = void;

            /**
             * constructor
             */
            explicit cpu_stage_function_arity_dispatch(F&& f) : stage_function_unary<F>(std::forward<F>(f)) {}

            /**
             * copy constructor
             */
            cpu_stage_function_arity_dispatch(cpu_stage_function_arity_dispatch const& other) : stage_function_unary<F>(other) {}

            /**
             * Perform the stage function on the input token
             *
             * @param in_first_idx the index to the first token in the the inbuf for
             *                     this stage iteration.
             * @param in_size      the number of input tokens for this iteration
             * @param inbuf        pointer to the input token buffer
             * @param out_idx      buffer index for the output token
             * @param outbuf       pointer to the output token buffer
             * @param context      the reference to the iteration context
             * @param launch_type
             *   hetcompute::internal::with_sliding_window
             *   hetcompute::internal::without_sliding_window
             */
            template <typename IBT, typename OBT, typename>
            void apply_function(size_t, size_t, IBT* inbuf, size_t, OBT*, void* context, pipeline_launch_type)
            {
                HETCOMPUTE_INTERNAL_ASSERT(inbuf == nullptr, "The input buffer for the first pipeline stage should not be allocated.");

                this->_f(*static_cast<context_type_noref*>(context));
            }

            // Forbid moving and assignment
            HETCOMPUTE_DELETE_METHOD(cpu_stage_function_arity_dispatch(cpu_stage_function_arity_dispatch&& other));
            HETCOMPUTE_DELETE_METHOD(cpu_stage_function_arity_dispatch& operator=(cpu_stage_function_arity_dispatch const& other));
            HETCOMPUTE_DELETE_METHOD(cpu_stage_function_arity_dispatch& operator=(cpu_stage_function_arity_dispatch&& other));
        };
        // end of class cpu_stage_function_arity_dispatch<F, 1, void>

        /**
         * Dispatches to the correct implementation
         * of the function for the stage in the middle with 2 arguments (arity = 2)
         */
        template <typename F, typename RT>
        class cpu_stage_function_arity_dispatch<F, 2, RT> : public stage_function_binary<F>
        {
        public:
            using context_type       = typename stage_function_binary<F>::arg0_type;
            using context_type_noref = typename std::remove_reference<context_type>::type;
            using arg1_type          = typename stage_function_binary<F>::arg1_type;
            using arg1_type_noref    = typename std::remove_reference<arg1_type>::type;
            using return_type        = typename stage_function_binary<F>::return_type;

            static_assert(std::is_default_constructible<return_type>::value, "The return type of a stage should be default constructible.");

            static_assert(std::is_copy_assignable<return_type>::value || std::is_move_assignable<return_type>::value,
                          "The return type of a stage should be either copy-assignable or move-assignable.");

            /**
             * constructor
             */
            explicit cpu_stage_function_arity_dispatch(F&& f) : stage_function_binary<F>(std::forward<F>(f)) {}

            /**
             * copy constructor
             */
            cpu_stage_function_arity_dispatch(cpu_stage_function_arity_dispatch const& other) : stage_function_binary<F>(other) {}

            /**
             * Perform the stage function on the input token
             *
             * @param in_first_idx the index to the first token in the the inbuf for
             *                     this stage iteration.
             * @param in_size      the number of input tokens for this iteration
             * @param inbuf        pointer to the input token buffer
             * @param out_idx      buffer index for the output token
             * @param outbuf       pointer to the output token buffer
             * @param context      the reference to the iteration context
             * @param launch_type
             *   hetcompute::internal::with_sliding_window
             *   hetcompute::internal::without_sliding_window
             */
            template <typename IBT, typename OBT, typename InputType>
            void apply_function(size_t               in_first_idx,
                                size_t               in_size,
                                IBT*                 inbuf,
                                size_t               out_idx,
                                OBT*                 outbuf,
                                void*                context,
                                pipeline_launch_type launch_type)
            {
                HETCOMPUTE_INTERNAL_ASSERT(in_size > 0, "The size for the input for the stage should be great than zero.");
                HETCOMPUTE_INTERNAL_ASSERT(inbuf != nullptr, "The input buffer for the pipeline stage should be allocated.");

                size_t iid = inbuf->get_buffer_index(in_first_idx);

                stage_input<InputType> input(static_cast<stagebuffer*>(inbuf), iid, in_size, in_first_idx, launch_type);

                if (outbuf)
                {
                    size_t oid     = outbuf->get_buffer_index(out_idx);
                    (*outbuf)[oid] = this->_f(*static_cast<context_type_noref*>(context), input);
                }
                else
                {
                    this->_f(*static_cast<context_type_noref*>(context), input);
                }
            }

            // Forbid moving and assignment
            HETCOMPUTE_DELETE_METHOD(cpu_stage_function_arity_dispatch(cpu_stage_function_arity_dispatch&& other));
            HETCOMPUTE_DELETE_METHOD(cpu_stage_function_arity_dispatch& operator=(cpu_stage_function_arity_dispatch const& other));
            HETCOMPUTE_DELETE_METHOD(cpu_stage_function_arity_dispatch& operator=(cpu_stage_function_arity_dispatch&& other));
        };
        // end of class cpu_stage_function_arity_dispatch<F, 2, RT>

        /**
         * Dispatches to the correct implementation
         * of the function for the last stage
         * with 2 arguments (arity = 2) and returns void
         */
        template <typename F>
        class cpu_stage_function_arity_dispatch<F, 2, void> : public stage_function_binary<F>
        {
        public:
            using context_type       = typename stage_function_binary<F>::arg0_type;
            using context_type_noref = typename std::remove_reference<context_type>::type;
            using arg1_type          = typename stage_function_binary<F>::arg1_type;
            using arg1_type_noref    = typename std::remove_reference<arg1_type>::type;
            using return_type        = typename stage_function_binary<F>::return_type;

            static_assert(std::is_base_of<stage_input_base, arg1_type_noref>::value,
                          "The 2nd param for a stage function should be of type stage_input<xxx>.");

            /**
             * constructor
             */
            explicit cpu_stage_function_arity_dispatch(F&& f) : stage_function_binary<F>(std::forward<F>(f)) {}

            /**
             * copy constructor
             */
            cpu_stage_function_arity_dispatch(cpu_stage_function_arity_dispatch const& other) : stage_function_binary<F>(other) {}

            /**
             * Perform the stage function on the input token
             *
             * @param in_first_idx the index to the first token in the the inbuf for
             *                     this stage iteration.
             * @param in_size      the number of input tokens for this iteration
             * @param inbuf        pointer to the input token buffer
             * @param out_idx      buffer index for the output token
             * @param outbuf       pointer to the output token buffer
             * @param context      the pointer to the iteration context
             * @param launch_type
             *   hetcompute::internal::with_sliding_window
             *   hetcompute::internal::without_sliding_window
             */

            template <typename IBT, typename OBT, typename InputType>
            void
            apply_function(size_t in_first_idx, size_t in_size, IBT* inbuf, size_t, OBT* outbuf, void* context, pipeline_launch_type launch_type)
            {
                HETCOMPUTE_INTERNAL_ASSERT(outbuf == nullptr, "Last stage does not have the output buffer.");
                HETCOMPUTE_INTERNAL_ASSERT(inbuf != nullptr, "Input buffer should not be nullptr. ");

                size_t iid = inbuf->get_buffer_index(in_first_idx);

                stage_input<InputType> input(static_cast<stagebuffer*>(inbuf), iid, in_size, in_first_idx, launch_type);

                this->_f(*static_cast<context_type_noref*>(context), input);
            }

            // Forbid moving and assignment
            HETCOMPUTE_DELETE_METHOD(cpu_stage_function_arity_dispatch(cpu_stage_function_arity_dispatch&& other));
            HETCOMPUTE_DELETE_METHOD(cpu_stage_function_arity_dispatch& operator=(cpu_stage_function_arity_dispatch const& other));
            HETCOMPUTE_DELETE_METHOD(cpu_stage_function_arity_dispatch& operator=(cpu_stage_function_arity_dispatch&& other));
        };
        // end of class cpu_stage_function_arity_dispatch<F, 2, void>

        /**
         * Represents the function (work) for a pipeline cpu stage
         */
        template <typename F>
        class cpu_stage_function
            : public cpu_stage_function_arity_dispatch<F, function_traits<F>::arity::value, typename function_traits<F>::return_type>
        {
        public:
            using f_type      = typename function_traits<F>::type_in_task;
            using return_type = typename function_traits<F>::return_type;
            using context_type =
                typename cpu_stage_function_arity_dispatch<F, function_traits<F>::arity::value, typename function_traits<F>::return_type>::context_type;

            using arg1_type =
                typename cpu_stage_function_arity_dispatch<F, function_traits<F>::arity::value, typename function_traits<F>::return_type>::arg1_type;

            /**
             * constructor
             */
            explicit cpu_stage_function(F&& f)
                : cpu_stage_function_arity_dispatch<F, function_traits<F>::arity::value, return_type>(std::forward<F>(f))
            {
            }

            /**
             * copy constructor
             */
            cpu_stage_function(cpu_stage_function<F> const& other)
                : cpu_stage_function_arity_dispatch<F, function_traits<F>::arity::value, return_type>(other)
            {
            }

            /**
             * destructor
             */
            virtual ~cpu_stage_function() {}

            // Forbid moving and assignment
            HETCOMPUTE_DELETE_METHOD(cpu_stage_function(cpu_stage_function<F>&& other));
            HETCOMPUTE_DELETE_METHOD(cpu_stage_function<F>& operator=(cpu_stage_function<F> const& other));
            HETCOMPUTE_DELETE_METHOD(cpu_stage_function<F>& operator=(cpu_stage_function<F>&& other));
        };
        // end of class cpu_stage_function

#ifdef HETCOMPUTE_HAVE_GPU

        // gpu stage functions
        /**
         * Dispatches to the correct implementation
         * of the stage function for a given arity
         */
        template <typename F, size_t Arity, typename RT>
        class gpu_stage_function_arity_dispatch;
        // end of class gpu_stage_function_arity_dispatch

        /**
         * Dispatches to the correct implementation
         * of the function only takes one argument which is the stage context (arity = 1)
         */
        template <typename F, typename RT>
        class gpu_stage_function_arity_dispatch<F, 1, RT> : public stage_function_unary<F>
        {
        public:
            // typdefs
            typedef typename stage_function_unary<F>::arg0_type   context_type;
            typedef typename stage_function_unary<F>::return_type return_type;
            typedef void                                          arg1_type;
            using context_type_noref = typename std::remove_reference<context_type>::type;

            /**
             * constructor
             */
            explicit gpu_stage_function_arity_dispatch(F&& f) : stage_function_unary<F>(std::forward<F>(f)) {}

            /**
             * copy constructor
             */
            gpu_stage_function_arity_dispatch(gpu_stage_function_arity_dispatch const& other) : stage_function_unary<F>(other) {}

            /**
             * apply gpu before sync function, i.e. the before lambda
             */
            template <typename InputType, typename IBT>
            RT apply_before_sync(size_t, size_t, IBT*, void* context, pipeline_launch_type&)
            {
                return this->_f(*static_cast<context_type_noref*>(context));
            }

            /**
             * apply gpu after sync function, i.e. the after lambda
             */
            template <typename GKTuple, typename OBT>
            void apply_after_sync(size_t out_idx, OBT* outbuf, void* context, GKTuple&)
            {
                if (outbuf)
                {
                    size_t oid     = outbuf->get_buffer_index(out_idx);
                    (*outbuf)[oid] = this->_f(*static_cast<context_type_noref*>(context));
                }
                else
                {
                    this->_f(*static_cast<context_type_noref*>(context));
                }
            }

            // Forbid moving and assignment
            HETCOMPUTE_DELETE_METHOD(gpu_stage_function_arity_dispatch(gpu_stage_function_arity_dispatch&& other));
            HETCOMPUTE_DELETE_METHOD(gpu_stage_function_arity_dispatch& operator=(gpu_stage_function_arity_dispatch const& other));
            HETCOMPUTE_DELETE_METHOD(gpu_stage_function_arity_dispatch& operator=(gpu_stage_function_arity_dispatch&& other));
        };
        // end of class gpu_stage_function_arity_dispatch<F, 1, RT>

        /**
         * Dispatches to the correct implementation
         * of the function for the first stage with
         * one argument (arity = 1) and returns void
         */
        template <typename F>
        class gpu_stage_function_arity_dispatch<F, 1, void> : public stage_function_unary<F>
        {
        public:
            // typdefs
            typedef typename stage_function_unary<F>::arg0_type   context_type;
            typedef typename stage_function_unary<F>::return_type return_type;
            typedef void                                          arg1_type;
            using context_type_noref = typename std::remove_reference<context_type>::type;

            /**
             * constructor
             */
            explicit gpu_stage_function_arity_dispatch(F&& f) : stage_function_unary<F>(std::forward<F>(f)) {}

            /**
             * copy constructor
             */
            gpu_stage_function_arity_dispatch(gpu_stage_function_arity_dispatch const& other) : stage_function_unary<F>(other) {}

            // No need to add apply_before_sync here since before lambdas have to return
            // the tuple for launching GPU kernels.

            /**
             * apply gpu after sync function, i.e. the after lambda
             */
            template <typename GKTuple, typename OBT>
            void apply_after_sync(size_t, OBT*, void* context, GKTuple&)
            {
                this->_f(*static_cast<context_type_noref*>(context));
            }

            // Forbid moving and assignment
            HETCOMPUTE_DELETE_METHOD(gpu_stage_function_arity_dispatch(gpu_stage_function_arity_dispatch&& other));
            HETCOMPUTE_DELETE_METHOD(gpu_stage_function_arity_dispatch& operator=(gpu_stage_function_arity_dispatch const& other));
            HETCOMPUTE_DELETE_METHOD(gpu_stage_function_arity_dispatch& operator=(gpu_stage_function_arity_dispatch&& other));
        };
        // end of class gpu_stage_function_arity_dispatch<F, 1, void>

        /**
         * Dispatches to the correct implementation
         * of the function for the stage in the middle with 2 arguments (arity = 2)
         */
        template <typename F, typename RT>
        class gpu_stage_function_arity_dispatch<F, 2, RT> : public stage_function_binary<F>
        {
        public:
            // typdefs
            typedef typename stage_function_binary<F>::arg0_type context_type;
            using context_type_noref = typename std::remove_reference<context_type>::type;
            typedef typename stage_function_binary<F>::return_type return_type;
            typedef typename stage_function_binary<F>::arg1_type   arg1_type;

            /**
             * constructor
             */
            explicit gpu_stage_function_arity_dispatch(F&& f) : stage_function_binary<F>(std::forward<F>(f)) {}

            /**
             * copy constructor
             */
            gpu_stage_function_arity_dispatch(gpu_stage_function_arity_dispatch const& other) : stage_function_binary<F>(other) {}

            /**
             * apply gpu before sync function, i.e. the before lambda
             */
            template <typename InputType, typename IBT>
            RT apply_before_sync(size_t in_first_idx, size_t in_size, IBT* inbuf, void* context, pipeline_launch_type& launch_type)
            {
                size_t iid = inbuf->get_buffer_index(in_first_idx);

                stage_input<InputType> input(static_cast<stagebuffer*>(inbuf), iid, in_size, in_first_idx, launch_type);

                return this->_f(*static_cast<context_type_noref*>(context), input);
            }

            /**
             * apply gpu after sync function, i.e. the after lambda
             */
            template <typename GKTuple, typename OBT>
            void apply_after_sync(size_t out_idx, OBT* outbuf, void* context, GKTuple& gk_tuple)
            {
                if (outbuf)
                {
                    size_t oid     = outbuf->get_buffer_index(out_idx);
                    (*outbuf)[oid] = this->_f(*static_cast<context_type_noref*>(context), gk_tuple);
                }
                else
                {
                    this->_f(*static_cast<context_type_noref*>(context), gk_tuple);
                }
            }

            // Forbid moving and assignment
            HETCOMPUTE_DELETE_METHOD(gpu_stage_function_arity_dispatch(gpu_stage_function_arity_dispatch&& other));
            HETCOMPUTE_DELETE_METHOD(gpu_stage_function_arity_dispatch& operator=(gpu_stage_function_arity_dispatch const& other));
            HETCOMPUTE_DELETE_METHOD(gpu_stage_function_arity_dispatch& operator=(gpu_stage_function_arity_dispatch&& other));
        };
        // end of class gpu_stage_function_arity_dispatch<F, 2, RT>

        /**
         * Dispatches to the correct implementation
         * of the function for the last stage
         * with 2 arguments (arity = 2) and returns void
         */
        template <typename F>
        class gpu_stage_function_arity_dispatch<F, 2, void> : public stage_function_binary<F>
        {
        public:
            // typdefs
            typedef typename stage_function_binary<F>::arg0_type   context_type;
            typedef typename stage_function_binary<F>::return_type return_type;
            typedef typename stage_function_binary<F>::arg1_type   arg1_type;
            using context_type_noref = typename std::remove_reference<context_type>::type;

            /**
             * constructor
             */
            explicit gpu_stage_function_arity_dispatch(F&& f) : stage_function_binary<F>(std::forward<F>(f)) {}

            /**
             * copy constructor
             */
            gpu_stage_function_arity_dispatch(gpu_stage_function_arity_dispatch const& other) : stage_function_binary<F>(other) {}

            // No need to add apply_before_sync here since before lambdas have to return
            // the tuple for launching GPU kernels.

            /**
             * apply gpu after sync function, i.e. the after lambda
             */
            template <typename GKTuple, typename OBT>
            void apply_after_sync(size_t, OBT*, void* context, GKTuple& gk_tuple)
            {
                this->_f(*static_cast<context_type_noref*>(context), gk_tuple);
            }

            // Forbid moving and assignment
            HETCOMPUTE_DELETE_METHOD(gpu_stage_function_arity_dispatch(gpu_stage_function_arity_dispatch&& other));
            HETCOMPUTE_DELETE_METHOD(gpu_stage_function_arity_dispatch& operator=(gpu_stage_function_arity_dispatch const& other));
            HETCOMPUTE_DELETE_METHOD(gpu_stage_function_arity_dispatch& operator=(gpu_stage_function_arity_dispatch&& other));
        };
        // end of class gpu_stage_function_arity_dispatch<F, 2, void>

        /**
         * Represents the synchronization functors for a pipeline gpu stage
         */
        template <typename F>
        class gpu_stage_sync_function
            : public gpu_stage_function_arity_dispatch<F, function_traits<F>::arity::value, typename function_traits<F>::return_type>
        {
        public:
            // typdefs
            typedef typename function_traits<F>::type_in_task f_type;
            typedef typename function_traits<F>::return_type  return_type;
            typedef
                typename gpu_stage_function_arity_dispatch<F, function_traits<F>::arity::value, typename function_traits<F>::return_type>::context_type
                    context_type;

            typedef
                typename gpu_stage_function_arity_dispatch<F, function_traits<F>::arity::value, typename function_traits<F>::return_type>::arg1_type
                    arg1_type;

            /**
             * constructor
             */
            explicit gpu_stage_sync_function(F&& f)
                : gpu_stage_function_arity_dispatch<F, function_traits<F>::arity::value, return_type>(std::forward<F>(f))
            {
            }

            /**
             * copy constructor
             */
            gpu_stage_sync_function(gpu_stage_sync_function<F> const& other)
                : gpu_stage_function_arity_dispatch<F, function_traits<F>::arity::value, return_type>(other)
            {
            }

            /**
             * destructor
             */
            virtual ~gpu_stage_sync_function() {}

            // Forbid moving and assignment
            HETCOMPUTE_DELETE_METHOD(gpu_stage_sync_function(gpu_stage_sync_function<F>&& other));
            HETCOMPUTE_DELETE_METHOD(gpu_stage_sync_function<F>& operator=(gpu_stage_sync_function<F> const& other));
            HETCOMPUTE_DELETE_METHOD(gpu_stage_sync_function<F>& operator=(gpu_stage_sync_function<F>&& other));
        };
        // end of class gpu_stage_function

        template <>
        class gpu_stage_sync_function<void*>
        {
        public:
            // typdefs
            typedef void f_type;
            typedef void return_type;
            typedef void context_type;
            typedef void arg1_type;

            /**
             * constructor
             */
            explicit gpu_stage_sync_function(void*) {}

            /**
             * copy constructor
             */
            gpu_stage_sync_function(gpu_stage_sync_function<void*> const&) {}

            /**
             * destructor
             */
            virtual ~gpu_stage_sync_function() {}

            /**
             * apply gpu after sync function
             */
            template <typename GKTuple, typename OBT>
            void apply_after_sync(size_t, OBT*, void*, GKTuple&)
            {
            }

            // Forbid moving and assignment
            HETCOMPUTE_DELETE_METHOD(gpu_stage_sync_function(gpu_stage_sync_function<void*>&& other));
            HETCOMPUTE_DELETE_METHOD(gpu_stage_sync_function<void>& operator=(gpu_stage_sync_function<void*> const& other));
            HETCOMPUTE_DELETE_METHOD(gpu_stage_sync_function<void>& operator=(gpu_stage_sync_function<void*>&& other));
        };
// end of class gpu_stage_function
#endif // HETCOMPUTE_HAVE_GPU

    }; // namespace internal
};     // namespace hetcompute
