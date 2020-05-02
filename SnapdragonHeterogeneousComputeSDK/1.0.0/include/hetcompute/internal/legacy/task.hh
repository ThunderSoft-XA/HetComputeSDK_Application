#pragma once

#include <hetcompute/internal/legacy/taskfactory.hh>
#include <hetcompute/internal/task/task.hh>
#include <hetcompute/hetcompute_error.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace legacy
        {
            /*
             * TODO: This file refers to several example files for the docs that will be included later.
             *       The referred line numbers need to be updated as well.
             */

            // -----------------------------------
            // User APIs
            // -----------------------------------

            /** @addtogroup sync
            @{ */
            /**
                Waits for task to complete execution.

                This method does not return until the task completes its
                execution.  It returns immediately if the task has already
                finished.  If <tt>wait_for(hetcompute::task_shared_ptr)</tt> is called from
                within a task, HetCompute context-switches the task and finds another
                one to run. If called from outside a task (i.e., the main thread),
                HetCompute blocks the thread until <tt>wait_for(hetcompute::task_shared_ptr)</tt>
                returns. Note that this logic is implemented inside the task::wait method.

                This method is a safe point. Safe points are HetCompute API methods
                where the following property holds:
                @par
                The thread on which the task executes before the API call might not
                be the same as the thread on which the task executes after the API call.

                @param task_ref reference to the target task.

                @throws api_exception if task points to null.
                @note If exceptions are disabled, <code>wait_for</code> returns <code>
                       hetcompute::hc_error</code>.

                @par Example
                @includelineno samples/src/tasks10.cc

                @sa hetcompute::wait_for(internal::group_shared_ptr const& group)
            */
            inline hc_error wait_for(task_shared_ptr const& task_ref)
            {
                auto task_ptr = hetcompute::internal::c_ptr(task_ref);
                HETCOMPUTE_API_ASSERT(task_ptr != nullptr, "null pointer passed as task reference.");
                return task_ptr->wait();
            }

            /** @} */ /* end_addtogroup sync */

            /** @addtogroup tasks_cancellation
            @{ */

            /**
                Cancels task.

                Use <tt>hetcompute::cancel()</tt> to cancel a task and its
                successors. The effects of <tt>hetcompute::cancel()</tt> depend on the
                task status:

                - If a task is canceled before it launches, it never executes
                -- even if it is launched afterwards. In addition, it propagates
                the cancellation to the task's successors. This is called
                "cancellation propagation".

                - If a task is canceled after it is launched, but before it starts
                executing, it will never execute and it will propagate cancellation
                to its successors.

                - If the task is running when someone else calls
                <tt>hetcompute::cancel()</tt>, it is up to the task to ignore the
                cancellation request and continue its execution, or to honor the
                request via <tt>hetcompute::abort_on_cancel()</tt>, which aborts the
                task's execution and propagates the cancellation to the task's
                successors.

                - Finally, if a task is canceled after it completes its
                execution (successfully or not), it does not change its status and
                it does not propagate cancellation.

                @param task Task to cancel.

                @throws api_exception If task pointer is NULL.

                @par Example 1: Canceling a task before launching it:
                @includelineno samples/src/cancel_task1.cc

                In the example above, we create two tasks <tt>t1</tt> and
                <tt>t2</tt> and create a dependency between them. Notice that, if
                any of the tasks executes, it will raise an assertion. In line 13,
                we cancel <tt>t1</tt>, which causes <tt>t2</tt> to be canceled as
                well. In line 16, we launch <tt>t2</tt>, but it does not matter as
                it will not execute because it was canceled when <tt>t1</tt>
                propagated its cancellation.

                @par Example 2: Canceling a task after launching it, but before it
                executes.
                @includelineno samples/src/cancel_task2.cc

                In the example above, we create and chain three tasks <tt>t1</tt>,
                <tt>t2</tt>, and <tt>t3</tt>. In line 18, we launch <tt>t2</tt>,
                but it cannot execute because its predecessor has not executed
                yet. In line 21, we cancel <tt>t2</tt>, which means that it will
                never execute. Because <tt>t3</tt> is <tt>t2</tt>'s successor, it
                is also canceled -- if <tt>t3</tt> had a successor, it would also
                be canceled.

                @par Example 3: Canceling a task while it executes.
                @includelineno samples/src/abort_on_cancel1.cc

                In the example above, task <tt>t</tt>'s will never finish unless
                it is canceled. <tt>t</tt> is launched in line 12. After
                launching the task, we block for 2 seconds in line 15 to ensure
                that <tt>t</tt> is scheduled and prints its messages. In line 18,
                we ask HetCompute to cancel the task, which should be running by now.
                <tt>hetcompute::cancel()</tt> returns immediately after it marks the
                task as "pending for cancellation". This means that <tt>t</tt>
                might still be executing after <tt>hetcompute::cancel(t)</tt> returns.
                That is why we call <tt>hetcompute::wait_for(t)</tt> in line
                21, to ensure that we wait for <tt>t</tt> to complete its
                execution.  Remember: a task does not know whether someone has
                requested its cancellation unless it calls
                <tt>hetcompute::abort_on_cancel()</tt> during its execution.

                @par Example 4: Canceling a completed task.
                @includelineno samples/src/cancel_finished_task1.cc

                In the example above, we launch <tt>t1</tt> and <tt>t2</tt> after
                we set up a dependency between them. On line 25, we cancel
                <tt>t1</tt> after it has completed.  By then, <tt>t1</tt> has
                finished execution (we wait for it in line 21) so
                <tt>cancel(t1)</tt> has no effect. Thus, nobody cancels
                <tt>t2</tt> and <tt>wait_for(t2)</tt> in line 28 never returns.

                @sa hetcompute::after(task_shared_ptr const &, task_shared_ptr const &).
                @sa hetcompute::abort_on_cancel().
                @sa hetcompute::wait_for(task_shared_ptr const&);
            */
            inline void cancel(task_shared_ptr const& task)
            {
                auto t_ptr = internal::c_ptr(task);
                HETCOMPUTE_API_ASSERT(t_ptr, "null task_shared_ptr");
                t_ptr->cancel();
            }

            /** @addtogroup dependencies
            @{ */
            /**
                Adds the <tt>succ</tt> task to <tt>pred</tt>'s list of successors.

                Creates a dependency between two tasks (<tt>pred</tt> and
                <tt>succ</tt>) so that <tt>succ</tt> starts executing only after
                <tt>pred</tt> has completed its execution, regardless of how many
                hardware execution contexts are available in the device.  Use this
                method to create task dependency graphs.

                @note1 The programmer is responsible for ensuring that there are
                no cycles in the task graph.

                If <tt>succ</tt> has already been launched, hetcompute::after() will
                throw an api_exception. This is because it makes little sense to
                add a predecessor to a task that might already be running. On the
                other hand, if <tt>pred</tt> successfully completed execution,
                no dependency is created, and hetcompute::after returns immediately.
                If <tt>pred</tt> was canceled (or if it is canceled in the
                future), <tt>succ</tt> will be canceled as well due to cancellation
                propagation.

                @param pred Predecessor task.
                @param succ Successor task.

                @throws api_exception   If <tt>pred</tt> or <tt>succ</tt> are NULL.
                @throws api_exception   If <tt>succ</tt> has already been launched.

                @par Example
                @includelineno samples/src/after.cc
                Output:
                @code
                Hello World! from task t1
                Hello World! from task t2
                @endcode
                No other output is possible because of the dependency between t1
                and t2.

                @sa hetcompute::before(task_shared_ptr&, task_shared_ptr&).
            */
            inline void after(task_shared_ptr const& pred, task_shared_ptr const& succ)
            {
                auto pred_ptr = ::hetcompute::internal::c_ptr(pred);
                auto succ_ptr = ::hetcompute::internal::c_ptr(succ);

                HETCOMPUTE_API_ASSERT(pred_ptr != nullptr, "null task_shared_ptr");
                HETCOMPUTE_API_ASSERT(succ_ptr != nullptr, "null task_shared_ptr");

                pred_ptr->add_control_dependency(succ_ptr);
            }

            /** @} */ /* end_addtogroup dependencies */


            /** @addtogroup tasks_creation
            @{ */
            /**
                Creates a new task and returns a task_shared_ptr that points to that task.

                This template method creates a task and returns a pointer to it.
                The task executes the <tt>Body</tt> passed as parameter. The
                preferred type of <tt>\<typename Body\></tt> is a C++11 lambda,
                although it is possible to use other types such as function
                objects and function pointers. Regardless of the <tt>Body</tt>
                type, it cannot take any arguments.

                @param body Code that the task will run when the tasks executes.

                @return
                task_shared_ptr -- Task pointer that points to the new task.

                @par Example 1 Create a task using a C++11 lambda (preferred).
                @includelineno samples/src/create_task1.cc

                @par Example 2 Create a task using a user-defined class.
                @includelineno snippets/task_class5.cc

                @par Example 3 Create a task using a function pointer.
                @includelineno samples/src/function_pointer1.cc

                @warning
                Due to limitations in the Visual Studio C++ compiler, this does not
                work on Visual Studio.
                You can get around it by using a lambda function:
                @includelineno samples/src/function_pointer2.cc

                @sa hetcompute::launch(internal::group_shared_ptr const&, Body&&)
            */
            template <typename Body>
            inline task_shared_ptr create_task(Body&& body)
            {
                typedef hetcompute::internal::legacy::body_wrapper<Body> wrapper;
                hetcompute::internal::task*                              t = wrapper::create_task(std::forward<Body>(body));

                // NOTE: no_initial_ref was used because we create a task with ref_count=1,
                // which allows us to return a task_shared_ptr without doing any atomic operation.
                return task_shared_ptr(t, task_shared_ptr::ref_policy::no_initial_ref);
            }

            /** @} */ /* end_addtogroup tasks_creation */

            /** @addtogroup execution
            @{ */
            /**
                Launches task created with hetcompute::create_task(...).

                Use this method to launch a task created with
                hetcompute::create_task(...).  In general, you create tasks using
                hetcompute::create_task(...) if they are part of a DAG. This method
                informs the HetCompute runtime that the task is ready to execute as soon
                as there is an available hardware context *and* after all its
                predecessors have executed.  Notice that the task is launched into
                a group only if the task already belongs to a group.

                @param task reference to the task to launch.

                @throws api_exception if task pointer is NULL.

                @par Example
                @includelineno samples/src/launch1.cc

                @sa hetcompute::create_task(Body&& b)
                @sa hetcompute::launch(internal::group_shared_ptr const&, task_shared_ptr const&)
            */
            inline void launch(task_shared_ptr const& task) { hetcompute::internal::legacy::launch_dispatch(task); }

            /**
                Creates a new task and launches it into a group.

                *This is the fastest way to create and launch a task* into a
                group. We recommend you use it as much as possible. However,
                notice that this method does not return a task_shared_ptr. Therefore, use
                this method if the new task is not part of a DAG. The HetCompute runtime
                will execute the task as soon as there is an available hardware
                context.

                The new task executes the <tt>Body</tt> passed as parameter.
                The preferred type of <tt>\<typename Body\></tt> is a C++11
                lambda, although it is possible to use other types such as
                function objects and function pointers. Regardless of the
                <tt>Body</tt> type, it cannot take any arguments.

                When launching into many groups, remember that
                group intersection is a somewhat expensive operation. If
                you need to launch into multiple groups several times, intersect
                the groups once and launch the tasks into the intersection.

                @param a_group Pointer to group.
                @param body Task body.

                @throws api_exception If group points to null.

                @par Example 1 Creating and launching tasks into one group.
                @includelineno samples/src/groups_add1.cc

                @par Example 2 Creating and launching tasks into multiple groups.
                @includelineno samples/src/group_intersection3.cc

                @sa hetcompute::create_task(Body&& b)
                @sa hetcompute::launch(internal::group_shared_ptr const&, task_shared_ptr const&);
            */
            template <typename Body>
            inline void launch(internal::group_shared_ptr const& a_group, Body&& body, bool notify = true)
            {
                hetcompute::internal::legacy::launch_dispatch<Body>(a_group, std::forward<Body>(body), notify);
            }

            /** @} */ /* end_addtogroup execution */

        }; // namespace legacy

        /// Creates stub task
        template <typename F>
        static ::hetcompute::internal::task_shared_ptr create_stub_task(F f, task* pred = nullptr)
        {
            auto attrs = legacy::create_task_attrs(attr::stub, attr::non_cancelable);
            if (pred == nullptr) // yield
                attrs = ::hetcompute::internal::legacy::add_attr(attrs, attr::yield);

            auto stub = ::hetcompute::internal::legacy::create_task(with_attrs(attrs, f));
            if (pred)
            {
                pred->add_control_dependency(c_ptr(stub));
            }
            return stub;
        }

        /// Creates and launches stub_task
        template <typename F>
        static void launch_stub_task(F f, task* pred = nullptr)
        {
            auto stub = create_stub_task(f, pred);
            legacy::launch_dispatch(stub);
        }

        /// Creates a trigger task to be used in group
        inline ::hetcompute::internal::task_shared_ptr create_trigger_task()
        {
            static auto no_body = [] {};
            auto        t_attr  = legacy::create_task_attrs(attr::trigger, attr::non_cancelable);
            return legacy::create_task(with_attrs(t_attr, no_body));
        }

    }; // namespace internal

}; // namespace hetcompute
