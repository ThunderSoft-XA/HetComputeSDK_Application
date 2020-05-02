#pragma once

// Include std headers
#include <string>
#include <typeinfo>
#include <utility>

// Include internal headers
#include <hetcompute/internal/task/cputask.hh>
#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/hetcompute_error.hh>

namespace hetcompute
{
    /** @addtogroup taskclass_doc
        @{ */

    /** @cond PRIVATE */
    template <typename... Stuff>
    class task;

    template <typename... Stuff>
    class task_ptr;

    namespace internal
    {
        ::hetcompute::internal::task* c_ptr(::hetcompute::task_ptr<>& t);
        ::hetcompute::internal::task* c_ptr(::hetcompute::task_ptr<> const& t);
        ::hetcompute::internal::task* c_ptr(::hetcompute::task<>*);

        template <typename R, typename... As>
        cputask_arg_layer<R(As...)>* c_ptr2(::hetcompute::task<R(As...)>& t);
    }; // namespace hetcompute::internal
    /** @endcond */

    /**
     * @brief Tasks as the basic unit of work.
     *
     * @note1 This is the basic task without function signature information.
     *
     * @note1 An object of this class should not be instantiated.
     *        It is a facade to the internal implementation.
    */
    template<>
    class task<>
    {
        friend ::hetcompute::internal::task* ::hetcompute::internal::c_ptr(::hetcompute::task<>* t);

    protected:
        /** @cond PRIVATE*/
        using internal_raw_task_ptr = ::hetcompute::internal::task*;
        /** @endcond */

        internal_raw_task_ptr get_raw_ptr() const { return _ptr; }

        // nobody should delete an object of this type because they are never "created" ~task(){} private:
        internal_raw_task_ptr _ptr;

        HETCOMPUTE_DELETE_METHOD(task());
        HETCOMPUTE_DELETE_METHOD(task(task const&));
        HETCOMPUTE_DELETE_METHOD(task(task&&));
        HETCOMPUTE_DELETE_METHOD(task& operator=(task const&));
        HETCOMPUTE_DELETE_METHOD(task& operator=(task&&));

    public:
        /**
         *  Unsigned integral type.
         */
        using size_type = ::hetcompute::internal::task::size_type;

        /**
         * @brief Launches task.
         *
         * Launches task.
         *
         *  This method informs the Qualcomm HetCompute runtime that the task is ready to
         *  execute as soon as there is an available hardware context *and*
         *  after all its predecessors (both data- and control-dependent)
         *  have executed.
         *
         *  @par Example
         *  @includelineno samples/src/launch_wait_task.cc
         *
         *  @sa <code>hetcompute::group::launch(Code)</code>
         */
        void launch()
        {
            HETCOMPUTE_INTERNAL_ASSERT(get_raw_ptr() != nullptr, "Unexpected null task<>.");
            get_raw_ptr()->launch(nullptr, nullptr);
        }

        /**
         * @brief Waits for the task to complete execution.
         *
         * Waits for the task to complete execution.
         *
         *  This method does not return until the task completes its
         *  execution. It returns immediately if the task has already
         *  finished.
         *
         *  If <code>hetcompute::task<>::wait_for()</code> is called from within a task, Qualcomm HetCompute
         *  context-switches the task and finds another one to run. If
         *  called from outside a task (i.e., the main thread), Qualcomm HetCompute blocks
         *  the thread until <code>wait_for(hetcompute::task_ptr)</code>
         *  returns.
         *
         *  This method is a safe point. Safe points are Qualcomm HetCompute API methods
         *  where the following property holds:
         *  @par
         *  The thread on which the task executes before the API call might not
         *  be the same as the thread on which the task executes after the API call.
         *
         *  @par Example
         *  @includelineno samples/src/launch_wait_task.cc
         *
         *  @throws hetcompute::canceled_exception If the task or any task on
         *  which it is dependent was canceled.
         *  @note If exceptions are disabled by application, in the above case API will return
         *  hc_error::HC_TaskCanceled
         * 
         *  @throws hetcompute::aggregate_exception If the task or any tasks on
         *  which it is dependent threw two or more exceptions.
         *  @note If exceptions are disabled by application, in the above case API will return
         *  hc_error::HC_TaskAggregateFailure
         *
         *  @throws hetcompute::gpu_exception If the task or any tasks on which
         *  it is dependent encountered a runtime error on the GPU.
         *  @note If exceptions are disabled by application, in the above case API will return
         *  hc_error::HC_TaskGpuFailure
         *
         *  @throws hetcompute::hexagon_exception If the task or any tasks on which
         *  it is dependent encountered a runtime error on the Hexagon DSP.
         *  @note If exceptions are disabled by application, in the above case API will return
         *  hc_error::HC_TaskDspFailure
         *
         *  @throws any other exception that may be thrown by the task or any tasks on
         *  which it is dependent.
         *
         *  @sa <code>hetcompute::group::wait_for()</code>
         */
        hc_error wait_for()
        {
            HETCOMPUTE_INTERNAL_ASSERT(get_raw_ptr() != nullptr, "Unexpected null task<>.");
            return get_raw_ptr()->wait();
        }

        /**
         * @brief finish_after the task.
         *
         * Specifies that the current task should be deemed to finish only after
         * the task on which this method is invoked finishes. This method returns
         * immediately.
         *
         * @par Example
         * @includelineno samples/src/finish_after_mergesort.cc
         *
         * @throws api_exception If invoked from outside a task or from within a
         *                      <code>hetcompute::pfor_each</code> or if 'task' points to
         *                      null.
         * @note                If exceptions are disabled by application, this API will
         *                      terminate the app, if  pointer to task is <code>nullptr</code>,
         *                      invoked from outside a task or from within a <code>hetcompute::pfor_each</code>
         * @sa hetcompute::group::finish_after()
         */
        void finish_after()
        {
            auto p = get_raw_ptr();
            HETCOMPUTE_INTERNAL_ASSERT(p != nullptr, "Unexpected null task<>.");
            auto t = ::hetcompute::internal::current_task();
            HETCOMPUTE_INTERNAL_ASSERT(t != nullptr, "finish_after must be called from within a task");
            HETCOMPUTE_INTERNAL_ASSERT(!t->is_pfor(), "finish_after cannot be called from within a pfor_each");
            t->finish_after(p);
        }

        /**
         * @brief Creates a control dependency between two tasks.
         *
         *  Creates a control dependency between two tasks.
         *
         *  The Qualcomm HetCompute runtime ensures that <code>succ</code> starts
         *  executing only after <code>this</code> has completed its execution,
         *  regardless of how many hardware execution contexts are available
         *  in the device. Use this method to create task dependency
         *  graphs.
         *
         *  @note1 The programmer is responsible for ensuring that there are
         *  no cycles in the task graph.
         *
         *  If <code>succ</code> has already been launched,
         *  <code>hetcompute::task<>::then()</code> will throw an api_exception. This is
         *  because it makes little sense to add a predecessor to a task
         *  that might already be running. On the other hand, if
         *  <code>this</code> successfully completed execution, no dependency will be
         *  created, and <code>hetcompute::task<>::then</code> returns immediately. If
         *  <code>this</code> gets canceled (or if it is canceled in the future),
         *  <code>succ</code> will be canceled as well due to cancellation
         *  propagation.
         *
         *  Do not call this member if <code>succ</code> is <code>nullptr</code>.
         *  This would cause a fatal error.
         *
         *  @param succ Successor task. Cannot be <code>nullptr</code>.
         *
         *  @throws api_exception   If <code>succ</code> has already been launched.
         *  @note                   If exceptions are disabled by Application,
         *                          terminates the app if successor is already launched 
         *
         *  @par Example
         *  @includelineno samples/src/after.cc
         *  Output:
         *  @code
         *  Hello World! from task t1
         *  Hello World! from task t2
         *  @endcode
         *
         *  No other output is possible because of the dependency between t1
         *  and t2.
         */
        task_ptr<>& then(task_ptr<>& succ)
        {
            HETCOMPUTE_INTERNAL_ASSERT(get_raw_ptr() != nullptr, "Unexpected null task.");

            auto succ_ptr = ::hetcompute::internal::c_ptr(succ);
            HETCOMPUTE_API_ASSERT(succ_ptr != nullptr, "null task_ptr<>");

            get_raw_ptr()->add_control_dependency(succ_ptr);
            return succ;
        }

        task<>* then(task<>* succ)
        {
            HETCOMPUTE_INTERNAL_ASSERT(get_raw_ptr() != nullptr, "Unexpected null task.");

            auto succ_ptr = ::hetcompute::internal::c_ptr(succ);
            HETCOMPUTE_API_ASSERT(succ_ptr != nullptr, "null task_ptr<>");

            get_raw_ptr()->add_control_dependency(succ_ptr);
            return succ;
        }

        /**
         * @brief Cancels task.
         *
         * Cancels task.
         *
         *  Use <code>hetcompute::task<>::cancel()</code> to cancel a task and its
         *  successors. The effects of <code>hetcompute::task<>::cancel()</code> depend
         *  on the task status:
         *
         *  - If a task is canceled before it launches, it never executes
         *  -- even if it is launched afterwards. In addition, it propagates
         *  the cancellation to the task's successors. This is called
         *  "cancellation propagation".
         *
         *  - If a task is canceled after it is launched, but before it starts
         *  executing, it will never execute and it will propagate cancellation
         *  to its successors.
         *
         *  - If the task is running when someone else calls
         *  <code>hetcompute::task<>::cancel()</code>, it is up to the task to ignore the
         *  cancellation request and continue its execution, or to honor the
         *  request via <code>hetcompute::abort_on_cancel()</code>, which aborts the
         *  task's execution and propagates the cancellation to the task's
         *  successors.
         *
         *  - Finally, if a task is canceled after it completes its
         *   execution (successfully or not), it does not change its status and
         *   it does not propagate cancellation.
         *
         *  @par Example 1: Canceling a task before launching it:
         *
         *  @includelineno samples/src/cancel_task1.cc
         *
         *  In the example above, a control dependency is created betwen two tasks,
         *  <code>t1</code> and <code>t2</code>. Notice that, if any
         *  of the tasks executes, it will raise an assertion. In line 17,
         *  <code>t1</code> is canceled, which causes <code>t2</code> to be
         *  canceled as well. In line 20, <code>t2</code> is launched, but it does
         *  not matter as it will not execute because it was canceled when
         *  <code>t1</code> propagated its cancellation.
         *
         *  @par Example 2: Canceling a task after launching it, but before it executes:
         *
         *  @includelineno samples/src/cancel_task2.cc
         *
         *  In the example above, three tasks are created and chained: <code>t1</code>,
         *  <code>t2</code>, and <code>t3</code>. In line 22, <code>t2</code> is
         *  launched, but it cannot execute because its predecessor has not yet executed.
         *  In line 25, <code>t2</code> is canceled, which means that it will
         *  never execute. Because <code>t3</code> is <code>t2</code>'s successor, it
         *  is also canceled -- if <code>t3</code> had a successor, it would also
         *  be canceled.
         *
         *  @par Example 3: Canceling a task while it executes:
         *
         *  @includelineno samples/src/abort_on_cancel1.cc
         *
         *  In the example above, task <code>t</code>'s will never finish unless
         *  it is canceled. <code>t</code> is launched in line 16. After
         *  launching the task, it is blocked for 2 seconds in line 19 to ensure
         *  that <code>t</code> is scheduled and prints its messages. In line
         *  22, Qualcomm HetCompute is asked to cancel the task, which should be running by
         *  now. <code>hetcompute::task<>::cancel()</code> returns immediately after it
         *  marks the task as "pending for cancellation". This means that
         *  <code>t</code> might still be executing after <code>t->cancel()</code>
         *  returns. That is why <code>t->wait_for()</code> is called in line 26,
         *  to ensure that we wait for <code>t</code> to complete its execution.
         *  Remember: a task does not know whether someone has requested its
         *  cancellation unless it calls <code>hetcompute::abort_on_cancel()</code>
         *  during its execution.
         *
         *  @par Example 4: Canceling a completed task.
         *
         *  @includelineno samples/src/cancel_finished_task1.cc
         *
         *  In the example above, <code>t1</code> and <code>t2</code> are
         *  launched after a dependency is set up between them. On line 28,
         *  is canceled <code>t1</code> after it has completed. By then,
         *  <code>t1</code> has finished execution (waiting for it in line 24) so
         *  <code>cancel(t1)</code> has no effect. Thus, nobody cancels
         *  <code>t2</code> and <code>wait_for(t2)</code> in line 31 never returns.
         *
         *  @sa <code>hetcompute::abort_on_cancel()</code>
         *  @sa <code>hetcompute::task<>::wait_for()</code>
         */
        void cancel()
        {
            HETCOMPUTE_INTERNAL_ASSERT(get_raw_ptr() != nullptr, "Unexpected null task<>.");
            get_raw_ptr()->cancel();
        }

        /**
         * @brief Checks whether a task is canceled.
         *
         *  Use the method to check whether a task is canceled. If the task
         *  was canceled -- via cancellation propagation,
         *  <code>hetcompute::group::cancel()</code> or <code>hetcompute::task<>::cancel()</code>
         *  -- before it started executing, <code>hetcompute::task<>::canceled()</code>
         *  returns true.
         *
         *  If the task was canceled -- via <code>hetcompute::group::cancel()</code>
         *  or <code>hetcompute::group::cancel()</code> -- while it was executing,
         *  then <code>hetcompute::task<>::canceled()</code> returns true only if the
         *  task is not executing any more and it exited via
         *  <code>hetcompute::abort_on_cancel()</code>.
         *
         *  Finally, if the task completed successfully, hetcompute::task<>::canceled()
         *  always returns false.
         *
         *  @return
         *  true -- The task has transitioned to a canceled state. \n
         *  false -- The task has not yet transitioned to a canceled state.
         *
         *  @par Example:
         *  @includelineno snippets/canceled1.cc
         *
         *  @sa <code>hetcompute::task<>::cancel()</code>
         *  @sa <code>hetcompute::group::cancel()</code>
         */
        bool canceled() const
        {
            HETCOMPUTE_INTERNAL_ASSERT(get_raw_ptr() != nullptr, "Unexpected null task<>.");
            return get_raw_ptr()->is_canceled();
        }

        /**
         * @brief Checks whether a task is bound.
         *
         * Use this method to check whether all the task parameters are
         * bound.  Returns true if the task had no parameters. Remember that
         * only bound tasks can be launched.
         *
         * @return
         * true  -- All the task parameters are bound. If the task has none,
         *          then is_bound always return true.
         * false -- At least one of the task parameters is not bound.
         */
        bool is_bound() const
        {
            HETCOMPUTE_INTERNAL_ASSERT(get_raw_ptr() != nullptr, "Unexpected null task<>.");
            return get_raw_ptr()->is_bound();
        }

        /** @cond PRIVATE*/
        /// Not yet implemented
        std::string to_string() const
        {
            HETCOMPUTE_INTERNAL_ASSERT(get_raw_ptr() != nullptr, "Unexpected null task<>.");
            return get_raw_ptr()->to_string();
        }
        /** @endcond */
    };  // class task<>


    /**
     * @brief Tasks with a function of void return type.
     *
     * @note1 An object of this class should not be instantiated.
     *        It is a facade to the internal implementation.
     */
    template<>
    class task<void> : public task<>
    {
    protected:

          /** @cond PRIVATE*/
          // nobody should delete an object of this type because they are never "created"
          ~task() { }

          HETCOMPUTE_DELETE_METHOD(task());
          HETCOMPUTE_DELETE_METHOD(task(task const&));
          HETCOMPUTE_DELETE_METHOD(task(task&&));
          HETCOMPUTE_DELETE_METHOD(task& operator=(task const&));
          HETCOMPUTE_DELETE_METHOD(task& operator=(task&&));
          /** @endcond */
    };  // class task<void>


    /**
     * @brief Tasks with a function of non-void return type.
     *
     * @tparam ReturnType Return type of the task function.
     *
     * @note1 An object of this class should not be instantiated.
     *        It is a facade to the internal implementation.
     */
    template <typename ReturnType>
    class task<ReturnType> : public task<>
    {
    public:
        /**
         *  Type returned by the task
         */
        using return_type = ReturnType;

        /**
         * @brief Returns a const reference to the value returned by the task.
         *
         * Returns a const reference to the value returned by the task.
         *
         *  This method behaves like <code>hetcompute::task<>::wait_for</code>, and it
         *  does not return until the task completes its execution. It
         *  returns immediately if the task has already finished.
         *  <code>hetcompute::task<ReturnType>::copy_value()</code> can be called
         *  multiple times (unlike
         *  <code>hetcompute::task<ReturnType>::move_value</code>).
         *
         *  @par Example
         *  @includelineno samples/src/copy_value1.cc
         *
         *  @sa <code>hetcompute::task<ReturnType>::move_value()</code>
         *  @sa <code>hetcompute::task<>::wait_for()</code>
         */
        return_type const& copy_value()
        {
            auto t = get_typed_raw_ptr();
            HETCOMPUTE_INTERNAL_ASSERT(t != nullptr, "Unexpected null task<ReturnType>");
            t->wait();
            return t->get_retval();
        }

        /**
         * @brief Moves the value returned by value returned by the task.
         *
         * Moves the value returned by value returned by the task.
         *
         *  This method behaves like <code>hetcompute::task<>::wait_for</code>, and it
         *  does not return until the task completes its execution. It
         *  returns immediately if the task has already finished.
         *
         *  <code>hetcompute::task<ReturnType>::move_value()</code> can only be
         *  called once. Any subsequent call may raise an exception.
         *
         *  @par Example
         *  @includelineno samples/src/move_value1.cc
         *
         *  @sa <code>hetcompute::task<ReturnType>::copy_value()</code>
         *  @sa <code>hetcompute::task<>::wait_for()</code>
         */
        return_type&& move_value()
        {
            auto t = get_typed_raw_ptr();
            HETCOMPUTE_INTERNAL_ASSERT(t != nullptr, "Unexpected null task<ReturnType>");
            t->wait();
            return t->move_retval();
        }

    private:
        using raw_ptr = hetcompute::internal::cputask_return_layer<return_type>*;

        raw_ptr get_typed_raw_ptr() const
        {
            HETCOMPUTE_INTERNAL_ASSERT(get_raw_ptr() != nullptr, "Unexpected null task*");
            return static_cast<raw_ptr>(get_raw_ptr());
        }

        // nobody should delete an object of this type because they are never "created"
        ~task() {}

        HETCOMPUTE_DELETE_METHOD(task());
        HETCOMPUTE_DELETE_METHOD(task(task const&));
        HETCOMPUTE_DELETE_METHOD(task(task&&));
        HETCOMPUTE_DELETE_METHOD(task& operator=(task const&));
        HETCOMPUTE_DELETE_METHOD(task& operator=(task&&));
    };

    /**
     * @brief Tasks with the full function signature (return type + parameter list).
     *
     * @tparam Args...    Parameter type list of the task function.
     *
     * @tparam ReturnType Return type of the task function.
     *
     * @note1 An object of this class should not be instantiated.
     *        It is a facade to the internal implementation.
     */
    template <typename ReturnType, typename... Args>
    class task<ReturnType(Args...)> : public task<ReturnType>
    {
    public:
        /**
         *  Unsigned integral type.
         */
        using size_type = task<>::size_type;

        /**
         *  Type returned by the task.
         */
        using return_type = ReturnType;

        /**
         *  Tuple whose types are the types of the task arguments.
         */
        using args_tuple = std::tuple<Args...>;

        /**
         * Number of task arguments.
         */
        static HETCOMPUTE_CONSTEXPR_CONST size_type arity = sizeof...(Args);

        /* CC: I don't understand why doxygen insists on moving this to the class doc chapter
           and not include it in the task_docs, so making it private for now. */
        /** @cond PRIVATE*/
        /**
         *  @brief Helper structure to get the type of the task argument.
         *
         *  Helper structure to get the type of the task argument.
         *
         *  @tparam ArgIndex The index of the task argument.
         */
        template <size_type ArgIndex>
        struct argument
        {
            static_assert(ArgIndex < sizeof...(Args), "Out of range argument indexing.");
            using type = typename std::tuple_element<ArgIndex, args_tuple>::type;
        };
        /** @endcond */

        /**
         * @brief Bind all arguments to a task with a full-function signature.
         *
         * Bind all arguments to a task with a full-function signature.
         *
         *  The arguments should be provided in the same order as the task's parameter list.
         *  If an <code>arg</code> is not a <code>hetcompute::task_ptr</code> or
         *  <code>hetcompute::task*</code>, its type should match
         *  the corresponding task parameter (bound by value).
         *
         *  If an <code>arg</code> is a <code>hetcompute::task_ptr</code> or <code>hetcompute::task*</code>,
         *  its return type should be c++ compatible with the corresponding task parameter
         *  (bound as data dependency); or the type of the <code>arg</code>
         *  should be c++ compatible with the corresponding task paramter type (bound by value).
         *  When an <code>arg</code> can be bound either as data dependency or by value,
         *  the binding type needs to be explicitly specificed using
         *  <code>hetcompute::bind_as_data_dependency</code> or <code>hetcompute::bind_by_value</code> to avoid ambiguity.
         *
         *  The number of arguments should be of the same <code>arity</code>.
         *
         *  @tparam Arguments... Task arguments type.
         *
         *  @param args Arguments to the task.
         *
         *  @par Example
         *  @includelineno samples/src/task_argument_bind.cc
         *
         *  @sa <code>hetcompute::bind_as_dependency()</code>
         *  @sa <code>hetcompute::bind_by_value()</code>
         */
        template <typename... Arguments>
        void bind_all(Arguments&&... args)
        {
            static_assert(sizeof...(Arguments) == sizeof...(Args), "Invalide number of arguments");

            auto t = get_typed_raw_ptr();
            t->bind_all(std::forward<Arguments>(args)...);
        }

        /**
         *  @brief Launches task and (optionally) binds arguments.
         *
         *  Launches task and (optionally) binds arguments.
         *
         *  This method informs the Qualcomm HetCompute runtime that the task is ready to
         *  execute as soon as there is an available hardware context *and*
         *  after all its predecessors (both data- and control-dependent)
         *  have executed.
         *
         *  <code>launch()</code> can have arguments. If so, the number of
         *  arguments in <code>launch()</code> should be the same as <code>arity</code>.
         *
         *  @par Example
         *  @includelineno samples/src/launch1_with_args.cc
         *
         * @sa <code>hetcompute::group::launch(Code)</code>
         */
        template <typename... Arguments>
        void launch(Arguments&&... args)
        {
            using should_bind = typename std::conditional<sizeof...(Arguments) != 0, std::true_type, std::false_type>::type;

            launch_impl(should_bind(), std::forward<Arguments>(args)...);
        }

    private:
        using raw_ptr = hetcompute::internal::cputask_arg_layer<return_type(Args...)>*;

        raw_ptr get_typed_raw_ptr() const
        {
            HETCOMPUTE_API_ASSERT(task<>::get_raw_ptr(), "Unexpected null ptr");
            return static_cast<raw_ptr>(task<>::get_raw_ptr());
        }

        template <typename... Arguments>
        void launch_impl(std::true_type, Arguments&&... args)
        {
            bind_all(std::forward<Arguments>(args)...);
            task<>::launch();
        }

        template <typename... Arguments>
        void launch_impl(std::false_type, Arguments&&...)
        {
            task<>::launch();
        }

        // nobody should delete an object of this type because they are never "created"
        ~task() {}

        HETCOMPUTE_DELETE_METHOD(task());
        HETCOMPUTE_DELETE_METHOD(task(task const&));
        HETCOMPUTE_DELETE_METHOD(task(task&&));
        HETCOMPUTE_DELETE_METHOD(task& operator=(task const&));
        HETCOMPUTE_DELETE_METHOD(task& operator=(task&&));

        template <typename R, typename... As>
        friend ::hetcompute::internal::cputask_arg_layer<R(As...)>* c_ptr2(::hetcompute::task<R(As...)>& t);
    };

    /**
     * @brief Explicitly bind a hetcompute::task_ptr<...> or hetcompute::task<...>* as data dependency.
     *
     * Explicitly bind a hetcompute::task_ptr<...> or hetcompute::task<...>* as data dependency.
     * The type of the return value of the hetcompute::task_ptr<...> or the hetcompute::task<...>*
     * should match the correpsonding parameter type for the task to bind.
     *
     * @param t a <code>hetcompute::task_ptr</code> or <code>hetcompute::task*</code> which has the
     *          return type information.
     */
    template <typename Task>
    hetcompute::internal::by_data_dep_t<Task&&> bind_as_data_dependency(Task&& t)
    {
        return hetcompute::internal::by_data_dep_t<Task&&>(std::forward<Task>(t));
    }

    /**
     *  @brief Enclose user-code that blocks on external activity.
     *
     *  Used to enclose user-code that blocks on external activity
     *  and needs to be cancelable when an enclosing task gets canceled.
     *
     *  A function/functor containing the blocking code <code>bf</code> is
     *  executed immediately. If cancellation is asynchronously requested
     *  for the enclosing task while <code>bf</code> is currently executing,
     *  the cancellation handler function/functor <code>cf</code> is
     *  asynchronously executed. Once <code>bf</code> completes, <code>blocking</code>
     *  throws <code>hetcompute::canceled_exception</code> if task cancellation
     *  was requested.
     *
     *  If cancellation of the task had already been requested prior to the
     *  execution of <code>blocking</code>, <code>blocking</code> immediately throws
     *  <code>hetcompute::canceled_exception</code> without executing
     *  <code>bf</code> or <code>cf</code>.
     *
     *  The programmer must write <code>bf</code> and <code>cf</code> to satisfy the
     *  following requirements:
     *  -# <code>cf</code> can be safely called concurrently with <code>bf</code>.
     *  -# The blocked work inside <code>bf</code> must somehow be able to unblock
     *     and resume execution when signalled to do so by <code>cf</code>.
     *
     *  @par Example
     *  <code>bf</code> may block on a network access causing its thread to sleep.
     *  <code>cf</code> writes special data into the network handle causing <code>bf</code>
     *  to unblock.
     *
     *  @code
     *  auto t = hetcompute::create_task([&x, &handle]()
     *                         {
     *                            hetcompute::blocking(
     *                                          [&]() { x = network_fetch(handle); }, //blocking code
     *                                          [&]() { write_spurious(handle); },    //cancellation handler
     *                                         );
     *                           // throws hetcompute::canceled_exception if encapsulating task is canceled
     *
     *                           do_whole_bunch_of_work(x, ...);
     *                         });
     *  @endcode
     *
     *  @note1 It is not required that the blocking construct be enclosed in a task. Without an
     *         enclosing task <code>bf</code> will execute as a normal function and <code>cf</code> will
     *         never be invoked.
     *
     *  @param bf Function/functor/lambda with signature <code>void(void)</code> that encapsulates
     *            blocking work.
     *  @param cf Function/functor/lambda with signature <code>void(void)</code> that is capable
     *            of canceling blocked work in <code>bf</code>.
     */
    template <typename BlockingFunction, typename CancelFunction>
    void blocking(BlockingFunction&& bf, CancelFunction&& cf);

    /** @cond PRIVATE*/
    /// Hide this from doxygen
    template <typename ReturnType, typename... Args>
    HETCOMPUTE_CONSTEXPR_CONST typename task<ReturnType(Args...)>::size_type task<ReturnType(Args...)>::arity;
    /** @endcond */

    /**
     *  @brief Explicitly bind a <code>hetcompute::task_ptr<...></code> or
     *         <code>hetcompute::task<...>*</code> by value.
     *
     *  Explicitly bind a <code>hetcompute::task_ptr<...></code> or <code>hetcompute::task<...>*</code> by value.
     *  The type of the <code>hetcompute::task_ptr<...></code> or the <code>hetcompute::task<...>*</code>
     *  should match the corresponding parameter type for the task to bind.
     *
     *  @param t a <code>hetcompute::task_ptr</code> or <code>hetcompute::task*</code>, which has
     *  the return type information.
     */
    template <typename Task>
    hetcompute::internal::by_value_t<Task&&> bind_by_value(Task&& t)
    {
        return hetcompute::internal::by_value_t<Task&&>(std::forward<Task>(t));
    }

    /**
     * @brief Aborts execution of calling task if any of its groups is
     * canceled or if someone has canceled it by calling <code>hetcompute::cancel()</code>.
     *
     *  HetCompute uses cooperative multitasking. Therefore, it cannot abort an
     *  executing task without help from the task. In HETCOMPUTE, each executing
     *  task is responsible for periodically checking whether it should
     *  abort. Thus, tasks call <code>hetcompute::abort_on_cancel()</code> to test whether
     *  they, or any of the groups to which they belong, have been
     *  canceled.  If true, <code>hetcompute::abort_on_cancel()</code> does not
     *  return. Instead, it throws <code>hetcompute::abort_task_exception</code>, which the
     *  HetCompute runtime catches. The runtime then transitions the task to a
     *  canceled state and propagates cancellation to the task's
     *  successors, if any.
     *
     *  Because <code>hetcompute::abort_on_cancel()</code> does not return if the
     *  task has been canceled, we recommend that
     *  you use use RAII to allocate and deallocate the resources used
     *  inside a task. If using RAII in your code is not an option,
     *  surround <code>hetcompute::abort_on_cancel()</code> with <code>try</code> --
     *  <code>catch</code>, and call <code>throw</code> from within the
     *  <code>catch</code> block after the cleanup code.
     *
     *  @throws <code>abort_task_exception</code> If called from a task that has been
     *  canceled via <code>hetcompute::cancel()</code> or that belongs to a canceled group.
     *
     *  @throws <code>api_exception</code>  If called from outside a task.
     *  @note   If exceptions are disabled in application, will terminate the app if called
     *          from outside a task. Another caveat to note with usage of <code>abort_on_cancel</code> with
     *          exceptions disabled is that the application code can get sandwidched between
     *          functions that are able to handle exceptions resulting in improper cleanups in 
     *          the function where exceptions are disabled.
     * 
     *  @par Example 1
     *  @includelineno samples/src/abort_on_cancel.cc
     *
     *  @par Output
     *  @code
     *  Task has executed 1 iterations!
     *  Task has executed 2 iterations!
     *  ...
     *  Task has executed 47 iterations!
     *  @endcode
     *
     *  @par Example 2
     *  @includelineno samples/src/abort_on_cancel3.cc
     */
    inline void abort_on_cancel()
    {
        auto t = internal::current_task();
        HETCOMPUTE_API_THROW(t, "Error: you must call abort_on_cancel from within a task.");
        t->abort_on_cancel();
    }

    /**
     * @brief Aborts execution of calling task.
     *
     * Use this method from within a running task to immediately abort
     * it and all its successors. <code>hetcompute::abort_task()</code> never
     * returns. Instead, it throws
     * <code>hetcompute::abort_task_exception</code>, which the HetCompute runtime catches. The
     * runtime then transitions the task to a canceled state and propagates
     * propagation to the task's successors, if any.
     *
     * @throws abort_task_exception If called from a task has been
     *     canceled via hetcompute::cancel() or a task that belongs to a canceled group.
     *
     *  @throws api_exception   If called from outside a task.
     *  @note   If exceptions are disabled in application, will terminate the app if called
     *          from outside a task
     *  @par Example
     *  @includelineno samples/src/abort_task1.cc
     *
     *  @par Output
     *  @code
     *  Hello World!
     *  Hello World!
     *  ..
     *  Hello World!
     *  @endcode
     */
    void abort_task();

    /** @cond PRIVATE */
    namespace internal
    {
        inline ::hetcompute::internal::task* c_ptr(::hetcompute::task<>* t) { return t->get_raw_ptr(); }

        template <typename R, typename... As>
        ::hetcompute::internal::cputask_arg_layer<R(As...)>* c_ptr2(::hetcompute::task<R(As...)>& t)
        {
            return t.get_typed_raw_ptr();
        }

    }; // namespace hetcompute::internal
    /** @endcond */

    /** @} */ /* end_addtogroup taskclass_doc */
};  // namespace hetcompute
