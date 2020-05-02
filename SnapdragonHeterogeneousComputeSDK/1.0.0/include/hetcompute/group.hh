#pragma once

#include <list>
#include <string>
#include <hetcompute/internal/util/macros.hh>
#include <hetcompute/hetcompute_error.hh>

namespace hetcompute
{
    /** @cond PRIVATE */
    template <typename... Stuff>
    class task_ptr;
    template <typename... Stuff>
    class task;
    /** @endcond */

    class group;
    class group_ptr;

    namespace internal
    {
        class group;

        ::hetcompute::internal::group* c_ptr(::hetcompute::group* g);

        template <typename Code>
        void launch(::hetcompute::group_ptr& gptr, std::list<Code>& kernels);

    }; // namespace hetcompute::internal

    /** @addtogroup groupclass_doc
     @{
    */

    /**
     * @brief Groups represent sets of tasks, which are used to simplify waiting
     * and canceling multiple tasks.
     *
     */
    class group
    {
        /** @cond PRIVATE */

        friend ::hetcompute::internal::group* ::hetcompute::internal::c_ptr(::hetcompute::group* g);

        template <typename Kernel>
        friend void ::hetcompute::internal::launch(::hetcompute::group_ptr& gptr, std::list<Kernel>& kernels);

        using internal_raw_group_ptr = ::hetcompute::internal::group*;

        /** @endcond */

    public:
        /**
         * @brief Binds arguments to task and launches it into the
         * group.
         *
         * Binds arguments to task and launches it into the
         * group. <code>task</code> must be a fully-typed task-pointer to
         * allow argument binding, and it should not be bound already.
         * Otherwise, <code>launch</code> causes a runtime error.
         * For more information about binding, check @ref taskclass_doc.
         *
         * Tasks do not execute unless they are launched. By launching a
         * task, the programmer informs the Qualcomm HetCompute runtime that the task is
         * ready to execute as soon as all its (control and data)
         * dependencies have been satisfied, required buffers, if any, are
         * available, and a hardware context is available.  For more
         * information about task launching, see @ref taskclass_doc.
         *
         * Tasks can launch only once. Any subsequent calls to
         * <code>g->launch()</code> do not cause the task to execute
         * again. Instead, they cause the task to be added to group
         * <code>g</code>, *if the task was not part of that group already*.
         * When launching a task into many groups, remember that group
         * intersection is a somewhat expensive operation. If you need to
         * launch into multiple groups several times, intersect the groups
         * once and launch the tasks into the intersection. For more
         * information about tasks joining multiple groups, see @ref tasks_groups.
         *
         *
         * @tparam FullType Task pointer type. Should be a full type
         *                  (i.e., void(int, float)).
         * @tparam FirstArg Type of the first argument to be bound to the task.
         * @tparam RestArgs Type of the rest of the arguments to be bound to the task.
         *
         * @param task Fully-typed task pointer.
         * @param first_arg First task argument.
         * @param rest_args Rest of the task arguments.
         *
         * @throws api_exception If task pointer is <code>nullptr</code>.
         * @note   If exceptions are disabled by application, this API will terminate the app
         *         if pointer to task is <code>nullptr</code>
         *
         * @par Example
         * @includelineno samples/src/group_launch_bind1.cc
         *
         * @sa <code>hetcompute::group::launch(hetcompute::task_ptr<> const&) </code>
         */
        template <typename FullType, typename FirstArg, typename... RestArgs>
        void launch(hetcompute::task_ptr<FullType> const& task, FirstArg&& first_arg, RestArgs&&... rest_args);

        /**
         * @brief Binds arguments to task and launches it into the group.
         *
         * Similar to <code>hetcompute::group::launch(hetcompute::task_ptr<FullType> const&,
         * FirstArg&& first_arg, RestArgs&& ...rest_args)</code> except it
         * takes a pointer to a base task instead of a base task pointer.
         *
         * @tparam FullType Task type. Should be a full type (i.e., void(int, float)).
         * @tparam FirstArg Type of the first argument to be bound to the task.
         * @tparam RestArgs Type of the the rest of the arguments to be bound to the
         *                  task.
         *
         * @param task  Pointer to task.
         * @param first_arg First argument value to bind.
         * @param rest_args The rest of the argument values to bind.
         *
         * @throws api_exception If pointer to task is <code>nullptr</code>.
         * @note   If exceptions are disabled by application, this API will terminate the app
         *         if  pointer to task is <code>nullptr</code>
         *
         * @sa <code>hetcompute::group::launch(hetcompute::task_ptr<FullType> const&, FirstArg&& first_arg, RestArgs&& ...rest_args)</code>
         *
         */
        template <typename TaskType, typename FirstArg, typename... RestArgs>
        void launch(hetcompute::task<TaskType>* task, FirstArg&& first_arg, RestArgs&&... rest_args);

        /**
         * @brief Launches task and into group.
         *
         * Launches task and into group. Tasks do not execute unless they are launched.
         * By launching a task, the programmer informs the Qualcomm HetCompute runtime that the task
         * is ready to execute as soon as all its (control and data) dependencies have
         * been satisfied, required buffers (if any) are available, and a hardware
         * context is available. For more information about task launching, see  @ref taskclass_doc.
         *
         * A task executes only once regardless of how many times it has been launched.
         * Therefore, any subsequent call to <code>launch</code> does not cause the task
         * to execute again. Instead, it causes the task to be added to a new group,
         * *if the task was not part of that group already*.  For more information about
         * tasks joining multiple groups, see @ref tasks_groups.
         *
         * @param task Base task pointer.
         *
         * @throws api_exception If task pointer is <code>nullptr</code>.
         * @note   If exceptions are disabled by application, this API will terminate the app
         *         if task pointer is <code>nullptr</code>
         *
         * @par Example
         * @includelineno samples/src/group_launch_already_bound1.cc
         */
        void launch(hetcompute::task_ptr<> const& task);

        /**
         * @brief Launches task into group.
         *
         * Similar to <code>hetcompute::group::launch(hetcompute::task_ptr<>
         * const&)</code> except it takes a pointer to a base task instead
         * of a base task-pointer.
         *
         * @param task Pointer to base task.
         *
         * @throws api_exception If <code>task</code> is <code><code>nullptr</code></code>.
         * @note   If exceptions are disabled by application, this API will terminate the app
         *         if <code>task</code> is <code>nullptr</code>
         *
         * @par Example
         * @includelineno samples/src/group_launch_already_bound2.cc
         */
        void launch(hetcompute::task<>* task);

        /**
         * @brief Creates a new task, binds arguments (if given) and launches it into
         * a group.
         *
         * Creates a new task, binds arguments (if given) and launches it into a group.
         * *This is the fastest way to create and launch a task* into a group. It is
         * recommended that it be used as much as possible. Note, however, that this
         * method does not return a pointer to the task. Therefore, only use this
         * method if the new task will not be part of a task graph. Qualcomm HetCompute runtime will
         * execute the task as soon as all its (control and data) dependencies have
         * been satisfied, required buffers if any are available, and a hardware
         * context is available. For more information about task launching, see @ref taskclass_doc.
         *
         * The new task executes the <code>Code</code> passed as an argument
         * to this method.
         *
         * When creating a task that will execute in the CPU, the preferred types
         * for <code>Code</code> are C++11 lambda and <code>hetcompute::cpu_kernel</code>,
         * although it is possible to use other types such as function objects and
         * function pointers. Use <code>hetcompute::dsp_kernel</code> or
         * <code>hetcompute::gpu_kernel</code> to create a task that runs in the
         * Qualcomm Hexagon DSP or in the GPU. Regardless of the <code>Code</code>
         * type, it can take up to 31 arguments.
         *
         * Notice that <code>launch</code> makes a copy of <code>code</code>
         * so that the programmer does not need to worry about the lifetime
         * of the code object.
         *
         * <code>launch</code> can launch a hetcompute pattern object directly just as
         * launching a regular task, as shown in the following example:
         * @par Examples
         * @code
         * // increment every element in input vector vin
         * auto l = [&vin](size_t i){ vin[i]++; };
         * auto pfor = hetcompute::pattern::create_pfor_each(l);
         * g->launch(pfor, size_t(0), vin.size());
         * @endcode
         *
         * Notice that <code>launch</code> does not support launching patterns with
         * non-void return value. Therefore programmers cannot launch preduce or
         * pdivide_and_conquer using this group launch semantic.
         *
         * @tparam Code Code that the task will execute. It can be a lambda expression,
         * function pointer, functor, pattern\n
         * <code>cpu_kernel</code>, <code>gpu_kernel</code> or a <code>dsp_kernel</code>.
         *
         * @param code Task body.
         * @param args Arguments to bind to the parameters.
         *
         * @par Example
         * @includelineno samples/src/group_add1.cc
         *
         * @sa <code>hetcompute::launch(Code&& code, Args&& ...args)</code>
         * @sa <code>hetcompute::group::launch(hetcompute::task_ptr<FullType> const&, FirstArg&& first_arg, RestArgs&& ...rest_args)</code>
         * @sa <code>hetcompute::group::launch(hetcompute::task_ptr<> const&)</code>
         */
        template <typename Code, typename... Args>
        void launch(Code&& code, Args&&... args);

        /**
         * @brief Specifies that the task invoking this function should be deemed
         * to finish only after tasks the group. This method returns immediately.
         *
         * @throws api_exception If invoked from outside a task or from within a
         *                       <code>hetcompute::pfor_each</code> or if 'task' points to
         *                       null
         * @note                 If exceptions are disabled by application, this API will
         *                       terminate the app, if  pointer to task is <code>nullptr</code>,
         *                       invoked from outside a task or from within a <code>hetcompute::pfor_each</code>
         * @par Example
         *  @includelineno samples/src/compose_pages.cc
         */
        void finish_after();

        /**
         * @brief  Blocks until all tasks in the group complete execution or are
         * canceled (that is, the group is empty)
         *
         * Blocks until all tasks in the group complete execution or are
         * canceled. If new tasks are added to the group while <code>
         * wait_for</code> is blocking, <code>wait_for</code> does not
         * return until all those new tasks also complete.
         *
         * If <code>wait_for</code> is called from within a task, Qualcomm HetCompute
         * context switches the task and finds another task to run. If
         * called from outside a task, this <code>wait_for</code> blocks the
         * calling thread until it returns.
         * 
         * @note   If exceptions are disabled by application, <code>wait_for</code>
         *         returns <code>hetcompute::hc_error</code> instead of throwing
         *         exceptions.
         *
         * <code>wait_for</code> is a safe point.
         *
         * @par Example 1
         *  @includelineno samples/src/wait_for_group1.cc
         *
         * Waiting for a group intersection means that Qualcomm HetCompute returns once the
         * tasks in the intersection group have completed or executed.
         *
         * @par Example 2
         *   @includelineno snippets/wait_for_group2.cc
         *
         *   @sa <code>hetcompute::group::finish_after</code>
         *   @sa <code>hetcompute::task::finish_after</code>
         *   @sa <code>hetcompute::task::wait_for</code>
         *   @sa <code>hetcompute::intersection</code>
         */
        hc_error wait_for();

        /**
         * @brief Cancels group.
         *
         * Marks the group as canceled and returns immediately. Once a group
         * is canceled, it cannot revert to a non-canceled state. Canceling
         * a group means that:
         *
         * - The tasks in the group that have not started execution will
         *   never execute.
         *
         * - The tasks in the group that are executing will be canceled only
         *   when they call <code>hetcompute::abort_on_cancel</code>. If any of
         *   these executing tasks is a blocking executing a
         *   <code>hetcompute::blocking</code> construct, Qualcomm HetCompute executes the
         *   constructs's cancellation handler if they had not executed it
         *   before.
         *
         * - Any tasks added to the group after the group is canceled are
         *   also canceled.
         *
         * <code>cancel</code> returns immediately. Call
         * <code>hetcompute::group::wait_for()</code> afterwards to wait for all
         * the running tasks to be completed. For more information about
         * cancellation, check @ref taskclass_doc.
         *
         * @par Example 1
         * @includelineno samples/src/cancel_group2.cc
         *
         * In the example above, launch 10000 tasks are launched into group
         * <code>g</code>. Each task prints a message when it starts
         * execution and another one right before it ends execution. The
         * latter one will only print if the task does not notice that the
         * group has been canceled. (See <code>hetcompute::abort_on_cancel</code>).
         *
         * Right after launching the tasks, <code>main</code> sleeps for a
         * second before canceling the group. This means that next time the
         * running tasks execute <code>hetcompute::abort_on_cancel()</code>, they
         * will see that their group has been canceled and will
         * abort. <code>wait_for</code> will not return before the running
         * tasks end their execution -- either because they call
         * <code>hetcompute::abort_on_cancel()</code>, or because they complete
         * their execution without being canceled.
         *
         * @par Example 2
         * @includelineno samples/src/cancel_group.cc
         *
         * @par Output
         * @code
         * wait_for returned after 87 tasks executed.
         * @endcode
         *
         * Note that this is an example output. Actual output is timing dependent.
         *
         * @sa <code>hetcompute::abort_on_cancel()</code>
         * @sa <code>hetcompute::group::wait_for()</code>
         * @sa <code>hetcompute::task<>::cancel()</code>
         */
        void cancel();

        /**
         *  @brief Adds a task to group without launching it.
         *
         *  Use <code>add</code> to add a task to a group without launching
         *  it. Because of performance reasons, it is *recommended that
         *  tasks are added to groups at the time they are launched* using
         *  <code>hetcompute::group::launch</code>. Use <code>add</code> when
         *  your algorithm requires that the task belongs to a group, but
         *  you are not yet ready to launch the task.  For example, perhaps
         *  you want to prevent the group from being empty, so you can wait
         *  on it somewhere else.
         *
         *  It is possible, though not recommended because of performance
         *  reasons, to use <code>add</code> repeatedly to add a task to
         *  multiple groups. Repeatedly adding a task to the same group is
         *  not an error, Qualcomm HetCompute ignores subsequent launches. If the task has
         *  previously been launched, <code>hetcompute::group::launch(task_ptr<>
         *  const&)</code> and <code>hetcompute::group::add(task_ptr<>
         *  const&)</code> are equivalent. For more information about tasks
         *  joining multiple groups, see @ref tasks_groups.
         *
         *  Regardless of the method used to add tasks to a group, the
         *  following rules always apply:
         *
         *  - Tasks stay in the group until they finish execution
         *    (successfully or unsuccessfully due to exceptions or
         *    cancellation). Once a task is added to a group, there is no way
         *    to remove it from the group.
         *
         *  - Once a task belonging to multiple groups completes execution,
         *    Qualcomm HetCompute removes it from all the groups to which it belongs.
         *
         *  - Neither completed nor canceled tasks can join groups.
         *
         *  - Tasks cannot be added to a canceled group.
         *
         *  Do not call this method if <code>task</code> is <code>nullptr</code>.
         *  This would cause a fatal error.
         *
         *  @param task Base task-pointer.
         *
         *  @par Example 1
         *  @includelineno samples/src/group_add3.cc
         *
         *  @par Example 2
         *  @includelineno samples/src/group_add4.cc
         *
         *  @sa <code>hetcompute::group::launch(task_ptr<>const&)</code>
         */
        void add(task_ptr<> const& task);

        /**
         *  @brief Adds a task to group without launching it.
         *
         *  Similar to <code>add(task_ptr<> const&)</code> except that it takes a
         *  pointer to a base task instead of a base task-pointer.
         *
         *  Do not call this method if <code>task</code> is <code>nullptr</code>.
         *  This would cause a fatal error.
         *
         *  @param task Pointer to base task.
         *
         *  @sa <code>hetcompute::group::launch(task_ptr<>const&)</code>
         *  @sa <code>hetcompute::group::add(task_ptr<> const&)</code>
         */
        void add(task<>* task);

        /**
         * @brief Returns a pointer to a group that represents the intersection of
         * two groups.
         *
         * Returns a pointer to a group that represents the intersection of
         * the group managed by <code>*this</code> and <code>other</code>.
         *
         * Some applications require that tasks join more than one group. It
         * is possible, though not recommended for performance reasons, to
         * use <code>hetcompute::group::launch(hetcompute::task_ptr<> const&)</code> or
         * <code>hetcompute::group::add(hetcompute::task_ptr<> const&)</code> repeatedly
         * to add a task to several groups. Instead, use
         * <code>hetcompute::group::intersect(group_ptr const&)</code> to create a
         * new group that represents the intersection of all the groups
         * where the tasks need to launch. Again, this method is more
         * performant than repeatedly launching the same task into different
         * groups.
         *
         * Launching a task into the intersection group also simultaneously
         * launches it into all the groups that are part of the intersection.
         *
         * Consecutive calls to <code>hetcompute::group::intersect</code> with the
         * same group pointer as argument return a pointer to the same group.
         *
         * Group intersection is a commutative operation.
         *
         * You can use the <code>&</code> operator instead of
         * <code>hetcompute::group::intersect</code>.
         *
         * @param other group pointer to the group to intersect with.
         *
         * @return group_ptr -- Group pointer that points to a group that
         *  represents the intersection of <code>*this</code> and <code>other</code>.
         *
         * @sa <code>hetcompute::intersect(hetcompute::group_ptr const&, hetcompute::group_ptr const&)</code>.
         * @sa <code>hetcompute::operator&(hetcompute::group_ptr const&,  hetcompute::group_ptr const&)</code>
         */
        group_ptr intersect(group_ptr const& other);
        group_ptr intersect(group* other);

        /**
         *  @brief Checks whether the group is canceled.
         *
         *  Returns true if the group has been canceled; otherwise,
         *  returns false. For more about cancellation, see @ref taskclass_doc.
         *
         *  @return
         *    <code>true</code> -- The group is canceled. \n
         *    <code>false</code> -- The group is not canceled.
         *
         *   @sa <code>hetcompute::group::cancel()</code>
         */
        bool canceled() const;

        /** @cond PRIVATE */
        /**
         *  @brief Returns a human-readable description of the group and its state.
         *
         *  Returns a human-readable description of the group and its state.
         *
         *  @return std::string containing the description.
         */
        std::string to_string() const;

        /** @endcond */

        /**
         * @brief Returns the group name.
         *
         * Returns string with the name of the group. If the group has no
         * name, the returned string is empty.
         *
         * @return <code>std::string</code> containing the name of the group.
         *
         * @sa <code>hetcompute::create_group(std::string const&)</code>
         */
        std::string get_name() const;

        /** @cond PRIVATE */
    protected:
        // nobody should delete an object of this type because they are never "created"
        ~group() {}

        /** @endcond */
    private:
        internal_raw_group_ptr get_raw_ptr() const { return _ptr; }
        internal_raw_group_ptr _ptr;

        HETCOMPUTE_DELETE_METHOD(group());
        HETCOMPUTE_DELETE_METHOD(group(group const&));
        HETCOMPUTE_DELETE_METHOD(group(group&&));
        HETCOMPUTE_DELETE_METHOD(group& operator=(group const&));
        HETCOMPUTE_DELETE_METHOD(group& operator=(group&&));
    };  // class group

    /** @} */ /* end_addtogroup groupclass_doc */

};  // namespace hetcompute
