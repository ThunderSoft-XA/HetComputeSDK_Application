/** @file pipelinedata.hh */
#pragma once

// Include standard headers
#include <stdexcept>

// Include internal headers
#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/patterns/pipeline/pipelinebuffers.hh>

/** @cond PRIVATE */
namespace hetcompute
{
    namespace internal
    {
        // template class declaration
        template <typename... UserData>
        class pipeline_skeleton;

        class pipeline_stage_skeleton_base;

        template <typename F, typename... UserData>
        class pipeline_cpu_stage_skeleton;
        template <typename BeforeBody, typename GK, typename AfterBody, typename... UserData>
        class pipeline_gpu_stage_skeleton;

        class pipeline_stage_instance;
        class pipeline_instance_base;
        template <typename... UserData>
        class pipeline_instance;

        template <typename F, size_t Arity, typename RT>
        class cpu_stage_function_arity_dispatch;
        template <typename F, size_t Arity, typename RT>
        class gpu_stage_function_arity_dispatch;

    }; // namespace internal/
};     // namespace hetcompute
/** @endcond */

namespace hetcompute
{
    /** @addtogroup pipeline_doc
        @{ */

    /**
     * @brief Serial pipeline stage types.
     *
     * Serial pipeline stage types.
     */
    typedef enum serial_stage_type { in_order = 0 } serial_stage_type;
    // end of enum class serial_stage_type

    /**
     * @brief Serial stage for specifying the type of the stages when adding to the pipeline.
     *
     * Serial stage for specifying the type of the stages when adding to the pipeline.
     */
    class serial_stage
    {
        /**
         * @brief Serial stage type.
         *
         * Serial stage type: hetcompute::in_order
         */
        serial_stage_type _type;

    public:
        /**
         * @brief Constructor.
         *
         * Constructor.
         *
         * @param t
         *   hetcompute::in_order (default) or
         */
        explicit serial_stage(serial_stage_type t = serial_stage_type::in_order) : _type(t) {}

        /**
         * @brief Get the type of the serial stage.
         *
         * Get the type of the serial stage.
         *
         * @return serial_stage_type The type for the serial stage.
         */
        serial_stage_type get_type() const { return _type; }

        /**
         * @brief Copy constructor.
         *
         * Copy constructor.
         */
        /* implicit  */ serial_stage(serial_stage const& other) : _type(other._type){};

        /**
         * @brief Move constructor.
         *
         * Move constructor.
         */
        explicit serial_stage(serial_stage&& other) : _type(other._type){};

        /**
         * @brief Copy assignment operator.
         *
         * Copy assignment operator.
         */
        serial_stage& operator=(serial_stage const& other)
        {
            _type = other._type;
            return *this;
        }

        // delete methods
        HETCOMPUTE_DELETE_METHOD(serial_stage& operator=(serial_stage&& other));

        /** @cond PRIVATE */
        // make friend classes
        friend class hetcompute::internal::pipeline_stage_skeleton_base;
        /** @endcond */
    };
    // end of class serial_stage

    /**
     * @brief Parallel pipeline stage for specifying the type of the stages when adding to the pipeline.
     *
     * Parallel pipeline stage for specifying the type of the stages when adding to the pipeline.
     */
    class parallel_stage
    {
        /**
         * @brief Parallel stage degree of concurrency.
         *
         * Parallel stage degree of concurrency.
         */
        size_t _degree_of_concurrency;

    public:
        /**
         * @brief Constructor.
         *
         * Constructor.
         *
         * @param doc Degree of concurrency for the parallel stage.
         *   Degree of concurrency (doc):
         *       should be a positive integer number.
         *       It specifies the maximum number of consecutive stage
         *       iterations that can run in parallel for this stage.
         *       When doc = 1, the parallel stage will behave like a serial stage.
         */
        explicit parallel_stage(size_t doc) : _degree_of_concurrency(doc)
        {
            HETCOMPUTE_API_ASSERT(doc > 0, "degree of concurrency should be larger than 0.");
        }

        /**
         * @brief Get the degree of concurrency for a parallel stage.
         *
         * Get the degree of concurrency for a parallel stage.
         *
         * @return size_t Degree of concurrency.
         */
        size_t get_degree_of_concurrency() const { return _degree_of_concurrency; }

        /**
         * @brief Copy constructor.
         *
         * Copy constructor.
         */
        /* implicit */ parallel_stage(parallel_stage const& other) : _degree_of_concurrency(other._degree_of_concurrency) {}

        /**
         * @brief Copy assignment operator.
         *
         * Copy assignment operator.
         */
        parallel_stage& operator=(parallel_stage const& other)
        {
            _degree_of_concurrency = other._degree_of_concurrency;
            return *this;
        }

        // delete methods
        HETCOMPUTE_DELETE_METHOD(parallel_stage& operator=(parallel_stage&& other));

        /** @cond PRIVATE */
        // make friend classes
        friend class hetcompute::internal::pipeline_stage_skeleton_base;
        /** @endcond*/
    };
    // end of class parallel_stage

    /**
     * @brief Pipeline stage iteration lag.
     *
     * Pipeline stage iteration lag
     */
    class iteration_lag
    {
        /**
         * @brief Iteration lag.
         *
         * Iteration lag.
         */
        size_t _iter_lag;

    public:
        /**
         * @brief Constructor.
         *
         * Constructor.
         */
        explicit iteration_lag(size_t lag) : _iter_lag(lag) {}

        /**
         * @brief Get the stage lag.
         *
         * Get the stage lag.
         *
         * @return size_t Stage lag.
         */
        size_t get_iter_lag() const { return _iter_lag; }

        /**
         * @brief Copy constructor.
         *
         * Copy constructor.
         */
        // cannot be explicit because of the copy needed in mux_param_value
        /* implicit */ iteration_lag(iteration_lag const& other) : _iter_lag(other._iter_lag) {}

        /**
         * @brief Move constructor.
         *
         * Move constructor.
         */
        explicit iteration_lag(iteration_lag&& other) : _iter_lag(other._iter_lag) {}

        /**
         * @brief Copy assignment operator.
         *
         * Copy assignment operator.
         */
        iteration_lag& operator=(iteration_lag const& other)
        {
            _iter_lag = other._iter_lag;
            return *this;
        }

        // delete methods
        HETCOMPUTE_DELETE_METHOD(iteration_lag& operator=(iteration_lag&& other));

        /** @cond PRIVATE */
        // make friend classes
        template <typename F, typename... UserData>
        friend class hetcompute::internal::pipeline_cpu_stage_skeleton;

        template <typename BeforeBody, typename GK, typename AfterBody, typename... UserData>
        friend class hetcompute::internal::pipeline_gpu_stage_skeleton;
        /** @endcond */
    };
    // end of class iteration_lag

    /**
     * @brief Pipeline stage iteration match rate.
     *
     * Pipeline stage iteration match rate.
     */
    class iteration_rate
    {
        /**
         * @brief Iteration rate for the prev stage.
         *
         * Iteration rate for the prev stage.
         */
        size_t _iter_rate_pred;

        /**
         * @brief Iteration rate for the succ stage.
         *
         * Iteration rate for the succ stage.
         */
        size_t _iter_rate_curr;

    public:
        /**
         * @brief Constructor.
         *
         * Constructor.
         */
        iteration_rate(size_t p, size_t c) : _iter_rate_pred(p), _iter_rate_curr(c) {}

        /**
         * @brief Get the iteration rate for the prev stage.
         *
         * Get the iteration rate for the prev stage.
         *
         * @return size_t Iteration rate for the previous stage.
         */
        size_t get_iter_rate_pred() const { return _iter_rate_pred; };

        /**
         * @brief Get the iteration rate for the curr stage.
         *
         * Get the iteration rate for the curr stage.
         *
         * @return size_t Iteration rate for the current stage.
         */
        size_t get_iter_rate_curr() const { return _iter_rate_curr; };

        /**
         * @brief Copy constructor.
         *
         * Copy constructor.
         */
        // cannot be explicit because of the copy needed in mux_param_value
        /* implicit */ iteration_rate(iteration_rate const& other)
            : _iter_rate_pred(other._iter_rate_pred), _iter_rate_curr(other._iter_rate_curr)
        {
        }

        /**
         * @brief Move constructor.
         *
         * Move constructor.
         */
        explicit iteration_rate(iteration_rate&& other) : _iter_rate_pred(other._iter_rate_pred), _iter_rate_curr(other._iter_rate_curr) {}

        /**
         * @brief Copy assignment operator.
         *
         * Copy assignment operator.
         */
        iteration_rate& operator=(iteration_rate const& other)
        {
            _iter_rate_pred = other._iter_rate_pred;
            _iter_rate_curr = other._iter_rate_curr;
            return *this;
        }

        // delete methods
        HETCOMPUTE_DELETE_METHOD(iteration_rate& operator=(iteration_rate&& other));

        /** @cond PRIVATE */
        // make friend classes
        template <typename F, typename... UD>
        friend class hetcompute::internal::pipeline_cpu_stage_skeleton;

        template <typename BeforeBody, typename GK, typename AfterBody, typename... UD>
        friend class hetcompute::internal::pipeline_gpu_stage_skeleton;
        /** @endcond */
    };
    // end of class iteration_rate

    /**
     * @brief Pipeline stage sliding window size.
     *
     * Pipeline stage sliding window size.
     */
    class sliding_window_size
    {
        /**
         * Size of the sliding window.
         */
        size_t _size;

    public:
        /**
         * @brief Constructor.
         *
         * Constructor.
         */
        explicit sliding_window_size(size_t size) : _size(size) {}

        /**
         * @brief Get the size of the sliding window.
         *
         * Get the size of the sliding window.
         *
         * @return size_t Sliding window size.
         */
        size_t get_size() const { return _size; }

        /**
         * @brief Copy constructor.
         *
         * Copy constructor.
         */
        // cannot be explicit because of the copy needed in mux_param_value
        /* implicit */ sliding_window_size(sliding_window_size const& other) : _size(other._size) {}

        /**
         * @brief Move constructor.
         *
         * Move constructor.
         */
        explicit sliding_window_size(sliding_window_size&& other) : _size(other._size) {}

        /**
         * @brief Copy assignment operator.
         *
         * Copy assignment operator.
         */
        sliding_window_size& operator=(sliding_window_size const& other)
        {
            _size = other._size;
            return *this;
        }

        // delete methods
        HETCOMPUTE_DELETE_METHOD(sliding_window_size& operator=(sliding_window_size&& other));

        /** @cond PRIVATE */
        // make friend classes
        template <typename F, typename... UserData>
        friend class hetcompute::internal::pipeline_cpu_stage_skeleton;

        template <typename BeforeBody, typename GK, typename AfterBody, typename... UserData>
        friend class hetcompute::internal::pipeline_gpu_stage_skeleton;

        template <typename... UserData>
        friend class hetcompute::internal::pipeline_skeleton;
        /** @endcond */
    };
    // end of class sliding_window_size

    /** @cond PRIVATE */
    class stage_input_base
    {
    protected:
        stage_input_base() {}

    public:
        virtual ~stage_input_base(){};

        // delete methods
        // only created internally, will never be copied or assigned
        HETCOMPUTE_DELETE_METHOD(stage_input_base(stage_input_base const& other));
        HETCOMPUTE_DELETE_METHOD(stage_input_base(stage_input_base&& other));
        HETCOMPUTE_DELETE_METHOD(stage_input_base& operator=(stage_input_base const& other));
        HETCOMPUTE_DELETE_METHOD(stage_input_base& operator=(stage_input_base&& other));
    };
    // end of class stage_input_base
    /** @endcond */

    /**
     * @brief Pipeline stage input class.
     *
     * Pipeline stage input class.
     *
     * @tparam InputType The data type for the stage_input,
     *                   which should match the return type of the previous stage.
     */
    template <typename InputType>
    class stage_input : public stage_input_base
    {
    private:
        // pointer to the ringbuffer
        hetcompute::internal::stagebuffer* _buffer;

        // iter_id which generates the first element in this set of inputs
        size_t _first;

        // first element index offset
        size_t _offset;

        // number of tokens for current iteration in the ringbuffer
        size_t _size;

        // pipeline launch type
        internal::pipeline_launch_type _launch_type;

        /**
         * @brief Constructor.
         *
         * Constructor.
         */
        stage_input(hetcompute::internal::stagebuffer* buffer,
                    size_t                           offset,
                    size_t                           input_size,
                    size_t                           first,
                    internal::pipeline_launch_type   launch_type)
            : _buffer(buffer), _first(first), _offset(offset), _size(input_size), _launch_type(launch_type)
        {
        }

    public:
        /**
         * @brief Type of the input data.
         *
         * Type of the input data.
         */
        typedef InputType input_type;

        /**
         * @brief Destructor.
         *
         * Destructor.
         */
        virtual ~stage_input(){};

        /**
         * @brief [] operator to get the ith element from the input.
         *
         * [] operator to get the ith element from the input.
         *
         * @param i The index of the element to retrieve.
         *
         * @return InputType The ith element in the input.
         */
        InputType& operator[](size_t i)
        {
            HETCOMPUTE_API_THROW_CUSTOM(i < _size, std::out_of_range, "Out of range access to stage_input.");
            if (_launch_type == hetcompute::internal::with_sliding_window)
            {
                auto   bptr = static_cast<hetcompute::internal::ringbuffer<input_type>*>(_buffer);
                size_t id   = bptr->get_buffer_index(i + _offset);
                return (*bptr)[id];
            }
            else
            {
                auto   bptr = static_cast<hetcompute::internal::dynamicbuffer<input_type>*>(_buffer);
                size_t id   = bptr->get_buffer_index(i + _offset);
                return (*bptr)[id];
            }
        }

        /**
         * @brief Get the iter_id for the stage iteration that generates the first element.
         *
         * Get the iter_id for the stage iteration that generates the first element.
         *
         * @return size_t The iteration id in the previous stage that
         *                generates the first element in the stage_input.
         */
        size_t get_first_elem_iter_id() const { return _first; }

        /**
         * @brief Get the ith element from the input.
         *
         * Get the ith element from the input.
         *
         * @param i The index of the element to retrieve.
         *
         * @return InputType The ith element in the input.
         */
        InputType& get_ith_element(size_t i)
        {
            HETCOMPUTE_API_THROW_CUSTOM(i < _size, std::out_of_range, "Out of range access to stage_input.");
            if (_launch_type == hetcompute::internal::with_sliding_window)
            {
                auto   bptr = static_cast<hetcompute::internal::ringbuffer<input_type>*>(_buffer);
                size_t id   = bptr->get_buffer_index(i + _offset);
                return (*bptr)[id];
            }
            else
            {
                auto   bptr = static_cast<hetcompute::internal::dynamicbuffer<input_type>*>(_buffer);
                size_t id   = bptr->get_buffer_index(i + _offset);
                return (*bptr)[id];
            }
        }

        /**
         * @brief Get the number of elements of type InputType in the stage input.
         *
         * Get the number of elements of type InputType in the stage input.
         *
         * @return size_t The number of elements in the input for current iteration.
         */
        size_t size() const { return _size; }

        // delete methods
        // only created internally, will never be copied or assigned
        HETCOMPUTE_DELETE_METHOD(stage_input(stage_input const& other));
        HETCOMPUTE_DELETE_METHOD(stage_input(stage_input&& other));
        HETCOMPUTE_DELETE_METHOD(stage_input& operator=(stage_input const& other));
        HETCOMPUTE_DELETE_METHOD(stage_input& operator=(stage_input&& other));

        /** @cond PRIVATE */
        // make friend class
        template <typename F, size_t Arity, typename RT>
        friend class hetcompute::internal::cpu_stage_function_arity_dispatch;
        template <typename F, size_t Arity, typename RT>
        friend class hetcompute::internal::gpu_stage_function_arity_dispatch;
        /** @endcond */
    };
    // end of class stage_input: public stage_input

    /**
     * @brief Pipeline context class.
     *
     * Pipeline context class.
     *
     * The user will be able to get information/limited control from the pipeline
     * in the user-defined pipeline function through this structure.
     * The user will be able to know the stage_id and the iteration_id
     * during execution through pipeline_context and have some control
     * of the execution of the underlying pipeline, such as stopping the pipeline during execution.
     * When defining a pipeline stage function (function or lambda or callable object),
     * the first parameter should always be of type pipeline_context.
     */
    class pipeline_context_base
    {
    protected:
        /** @cond PRIVATE */
        // pipeline stage iteration id
        size_t _iter_id;

        // the max number of iterations for the current stage if it is set
        size_t _max_stage_iter;

        // pipeline stage pointer
        // information, such as stage_id will be retrieved through this pointer
        hetcompute::internal::pipeline_stage_instance* _stage;

        // pipeline inst pointer
        // information and operations, such as stop the pipeline
        // be accessed/implemented through this pointer
        hetcompute::internal::pipeline_instance_base* _pipeline_instance;

        /**
         * Constructor
         * @param iter_id Pipeline stage iteration id.
         * @param stage Pipeline stage pointer.
         * @param instance The pipeline instance pointer.
         */
        pipeline_context_base(size_t                                       iter_id,
                              size_t                                       max_stage_iter,
                              hetcompute::internal::pipeline_stage_instance* stage,
                              hetcompute::internal::pipeline_instance_base*  instance)
            : _iter_id(iter_id), _max_stage_iter(max_stage_iter), _stage(stage), _pipeline_instance(instance)
        {
        }
        /** @endcond */

    public:
        /**
         * @brief Destructor.
         *
         * Destructor.
         */
        virtual ~pipeline_context_base() {}

        /**
         * @brief Cancel the pipeline.
         *
         * Use this method to cancel a pipeline.
         * Note that hetcompute::abort_on_cancel() needs to be called in the
         * pipeline user-defined stage functions for proper pipeline cancellation.
         * A pipeline can be cancelled in any stages, however the internal state of
         * the pipeline could be non-deterministic
         *
         * @includelineno samples/src/pipeline_cancel.cc
         */
        void cancel_pipeline();

        /**
         * @brief Get the current iteration id.
         *
         * Get the current iteration id (begins from 0).
         *
         * @return size_t Stage iteration id.
         */
        size_t get_iter_id() const
        {
            // In the internal, iterations begin from 1 to make the calculation simple.
            // We minus 1 here so that externally the iterations begin from 0 for
            // the convenience of application programming.
            return _iter_id - 1;
        }

        /**
         * @brief Get the maximum number of iterations for this stage.
         *
         * Get the maximum number of iterations for this stage.
         *
         * @return size_t maximum number of iterations for this stage.
         * 0 means the maximum number is unknown and the pipeline will be
         * stopped or canceled dynamically during execution.
         *
         */
        size_t get_max_stage_iter() const { return has_iter_limit() ? _max_stage_iter : 0; }

        /**
         * @brief Get the current stage id.
         *
         * Get the current stage id.
         *
         * @return size_t Stage id.
         */
        size_t get_stage_id() const;

        /**
         * @brief Check whether the maximum number of iterations for this stage is set.
         *
         * Check whether the maximum number of iterations for this stage is set.
         *
         * @return
         *  true - The pipeline has an iteration limit known before running.
         *  false- The pipeline does not have an iteration limit known before running.
         *
         * @sa <code>pipeline_context_base::stop_pipeline()</code>
         */
        bool has_iter_limit() const;

        /**
         * @brief Stop the pipeline.
         *
         * Use this method to stop a pipeline launched with an
         * iteration limit. Calling this method on a pipeline
         * without an iteration limit will cause a fatal error.
         * This method can only be called from the first stage of the pipeline.
         *
         * @sa <code>pipeline_context_base::has_iter_limit()</code>
         */
        void stop_pipeline();

        // delete methods
        // only created internally, will never be copied for assigned
        HETCOMPUTE_DELETE_METHOD(pipeline_context_base(pipeline_context_base const& other));
        HETCOMPUTE_DELETE_METHOD(pipeline_context_base(pipeline_context_base&& other));
        HETCOMPUTE_DELETE_METHOD(pipeline_context_base& operator=(pipeline_context_base const& other));
        HETCOMPUTE_DELETE_METHOD(pipeline_context_base& operator=(pipeline_context_base&& other));
    };
    // end of class pipeline_context_base

    /** @cond PRIVATE */
    template <typename... UserData>
    class pipeline_context;
    /** @endcond */

    /**
     * @brief Pipeline_context with no user data.
     *
     * Pipeline_context with no user data.
     *
     * @note1 This is the pipeline_context type for the pipeline without context data,
     *        i.e., hetcompute::pattern::pipeline<>.
     *        So not use this type directly.
     *        Instead, get the member type from the pipeline that the context
     *        is associated with, i.e.,
     *        <code> using context = hetcompute::pattern::pipeline<>::context</code>.
     */
    template <>
    class pipeline_context<> : public pipeline_context_base
    {
    private:
        /**
         * Constructor
         * @param iter_id Pipeline stage iteration id.
         * @param stage Pipeline stage pointer.
         * @param inst Pipeline_instance  pointer.
         */
        pipeline_context(size_t                                       iter_id,
                         size_t                                       max_stage_iter,
                         hetcompute::internal::pipeline_stage_instance* stage,
                         hetcompute::internal::pipeline_instance_base*  inst)
            : pipeline_context_base(iter_id, max_stage_iter, stage, inst)
        {
        }

    public:
        /**
         * @brief Destructor.
         *
         * Destructor.
         */
        virtual ~pipeline_context() {}

        // delete methods
        // only created internally, will never be copied for assigned
        HETCOMPUTE_DELETE_METHOD(pipeline_context(pipeline_context const& other));
        HETCOMPUTE_DELETE_METHOD(pipeline_context(pipeline_context&& other));
        HETCOMPUTE_DELETE_METHOD(pipeline_context& operator=(pipeline_context const& other));
        HETCOMPUTE_DELETE_METHOD(pipeline_context& operator=(pipeline_context&& other));

        /** @cond PRIVATE */
        // make friend class
        template <typename F, typename... UserData>
        friend class hetcompute::internal::pipeline_cpu_stage_skeleton;
        template <typename BeforeBody, typename GK, typename AfterBody, typename... UserData>
        friend class hetcompute::internal::pipeline_gpu_stage_skeleton;
        /** @endcond */
    };
    // end of class pipeline_context<>:public pipeline_context_base

    /**
     * @brief Pipeline_context with one user data.
     *
     * Pipeline_context with one user data.
     *
     * @tparam UserData The type for the pipeline context data.
     *
     * @note1 This is the pipeline_context type for the pipeline with context data,
     *        of type UserData, i.e., hetcompute::pattern::pipeline<UserData>.
     *        Do not use this type directly.
     *        Instead, get the member type from the pipeline that the context
     *        is associated with, i.e.,
     *        <code> using context = hetcompute::pattern::pipeline<UserData>::context</code>.
     */
    template <typename UserData>
    class pipeline_context<UserData> : public pipeline_context_base
    {
    private:
        /**
         * Constructor
         * @param iter_id Pipeline stage iteration id.
         * @param stage Pipeline stage pointer.
         * @param inst Pipeline_instance pointer.
         */
        pipeline_context(size_t                                       iter_id,
                         size_t                                       max_stage_iter,
                         hetcompute::internal::pipeline_stage_instance* stage,
                         hetcompute::internal::pipeline_instance_base*  inst)
            : pipeline_context_base(iter_id, max_stage_iter, stage, inst)
        {
        }

    public:
        /**
         * @brief Destructor.
         *
         * Destructor.
         */
        virtual ~pipeline_context() {}

        /**
         * @brief Get the pointer to the programmer-defined context data.
         *
         * Get the pointer to the programmer-defined context data.
         *
         * @return UserData* The pointer to the user-defined context data, which
         *         is provided by the user when launching the pipeline.
         */
        UserData* get_data() const { return static_cast<hetcompute::internal::pipeline_instance<UserData>*>(_pipeline_instance)->_user_data; }

        // delete methods
        // only created internally, will never be copied for assigned
        HETCOMPUTE_DELETE_METHOD(pipeline_context(pipeline_context const& other));
        HETCOMPUTE_DELETE_METHOD(pipeline_context(pipeline_context&& other));
        HETCOMPUTE_DELETE_METHOD(pipeline_context& operator=(pipeline_context const& other));
        HETCOMPUTE_DELETE_METHOD(pipeline_context& operator=(pipeline_context&& other));

        /** @cond PRIVATE */
        // make friend class
        template <typename F, typename... UD>
        friend class hetcompute::internal::pipeline_cpu_stage_skeleton;
        template <typename BeforeBody, typename GK, typename AfterBody, typename... UD>
        friend class hetcompute::internal::pipeline_gpu_stage_skeleton;
        /** @endcond */
    };
    // end of class pipeline_context<UserData>:public pipeline_context_base
    /** @} */ /* end_addtogroup pipeline_doc */
};  // namespace hetcompute
