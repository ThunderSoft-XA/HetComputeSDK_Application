#pragma once

#include <algorithm>
#include <memory>

// Include coresponding header first
#include <hetcompute/internal/legacy/attr.hh>
#include <hetcompute/internal/log/log.hh>
#include <hetcompute/internal/storage/taskstorage.hh>
#include <hetcompute/internal/task/exception_state.hh>
#include <hetcompute/internal/task/task_misc.hh>
#include <hetcompute/internal/util/tls.hh>
#include <hetcompute/hetcompute_error.hh>

#if !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)
#include <hetcompute/internal/memalloc/taskallocator.hh>
#endif // !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)

namespace hetcompute
{
    namespace internal
    {
        // Forward declaration
        class scheduler;
        class task_bundle_dispatch;
        class arena;
        class bufferstate;
        class executor_construct;

        // Must do forward declaration so they can be friends ot task.
        template <typename Object>
        void explicit_unref(Object* obj);
        template <typename Object>
        void explicit_ref(Object* obj);

        /**
         *
         * This is the base class task. It includes the features that are
         * common to all tasks. All the HetCompute internal structures use task*.
         *
         * This class does not have any knowledge about the code the
         * task will execute, and that's why we need to use virtual
         * functions to access to the task code, its arguments and its
         * return values.
         *
        */
        class task
        {
        public:
            using finish_after_state     = task_internal::finish_after_state;
            using finish_after_state_ptr = std::unique_ptr<finish_after_state>;
            using self_ptr               = void*;
            using size_type              = std::size_t;
            typedef task_internal::state_snapshot state_snapshot;
            typedef task_internal::successor_list successor_list;

        private:
            typedef task_internal::lock_guard lock_guard;
            typedef task_internal::state      state;

            using exception_state_ptr          = hetcompute::internal::exception_state::exception_state_ptr;
            using blocking_code_container_base = hetcompute::internal::task_internal::blocking_code_container_base;

            successor_list          _successors;
            state                   _state;
            std::atomic<scheduler*> _scheduler;
            storage_map             _taskls_map;
            std::atomic<group*>     _current_group;
            self_ptr                _self;
            legacy::task_attrs      _attrs;

            // finish_after_state needed for inlined tasks and blocking tasks.
            // Owned by task and passed around by reference, hence a unique_ptr.
            // Valid only if finish_after is invoked at least once.
            finish_after_state_ptr _finish_after_state;
            exception_state_ptr    _exception_state;
            const log::task_id     _log_id;

            // Holds the currently executing blocking code block (if any)
            // = nullptr if no blocking_construct is currently executing.
            blocking_code_container_base* _blocking_code_state;

            // Alternative implementations of the same task
            std::unique_ptr<std::vector<task*>> _alternatives;

        public:
            /**
             *  Runs the task body and cleans the task up afterwards
             *  (as long as it's not a gputask).
             *  If the task is tagged for cancellation, or if its group
             *  has been canceled, the task will not run.
             *  Make sure to modify the other schedule methods
             *   if you make changes to this one:
             *  @param tbd Task is part of this task bundle, if tbd is not nullptr.
             *  @param variant Poly kernel variant to execute. Zeroth by default.
            */
            void execute(task_bundle_dispatch* tbd = nullptr, size_t variant = 0);

            /**
             * Runs the task body and cleans the task up afterwards.
             * If the task is tagged for cancellation, or if its group
             * has been canceled, the task will not run.
             *
             * @param sched reference to the scheduler that runs
             * the task.
             * @param variant Poly kernel variant to execute. Zeroth by default.
            */
            void execute(scheduler& sched, size_t variant = 0)
            {
                set_scheduler(sched);
                execute_sync(nullptr, variant);
            }

            /**
             * Same as schedule() but additionally establishes a synchronization point
             * for data written by the current thread prior to the task getting
             * scheduled.
             * For use in case of tasks for which such a synchronization point does not
             *  already exist, e.g. blocking, gpu, inline, and anonymous tasks.
             * @param tbd Task is part of this task bundle.
             * @param variant Poly kernel variant to execute. Zeroth by default.
            */
            inline void execute_sync(task_bundle_dispatch* tbd = nullptr, size_t variant = 0);

            /**
             * Schedules the task in the current context, without going through
             * the scheduler. It'll destroy the task after it executes.
             *
             * @param g Group the task should joing
             */
            inline void execute_inline_task(group* g);

            bool launch(group* g, scheduler* sched)
            {
                return launch_legacy(g, sched, true);
            }

            /**
             * Marks task as launched and, if possible,
             * sends it to the runtime for execution.
             *
             * The caller can specify the policy for the reference counting.
             * In some situations, launching a task does not require that its
             * reference count gets increased. For example, this happens when
             * the uses launch(group_ptr, lambda expression).
             *
             * We're about to retire this method. The problem is that we should
             * always increase ref count, except in two cases that are either
             * legacy(legacy/taskfactory)
             *
             *
             *
             * @param g Target group
             * @param sched Scheduler used to launch the task
             * @param increase_ref_count If true, the refcount for the task increases
             *                           because it gets launched.
             * @return
             * true - The task was sent to the runtime for execution.\n
             * false - The task was not set to the runtime for execution.
            */
            bool launch_legacy(group* g, scheduler* sched, bool increase_ref_count);

            bool set_launched(bool increase_ref_count);

            /**
             * Sets up the task to not lock the buffers accessed by the task
             * during the buffer acquire/release steps. Not thread safe.
             *
             * Implementation will be overridden by derived task classes
             * that handle buffers.
             *
             * Expected use case: special tasks internal to runtime where
             * there is a guarantee of no conflicting concurrent executions.
             * Hence, task is not locked to enable non-locking buffer acquires.
            */
            virtual void unsafe_enable_non_locking_buffer_acquire()
            {
                HETCOMPUTE_UNREACHABLE("Expect to be called only for tasks that involve buffers");
            }

            /**
             * Sets up the task to execute on an already acquired substitute arena
             * instead of acquiring a suitable arena for a buffer. Not thread safe.
             *
             * @param bufstate          The buffer whose acquire/release should be
             *                          skipped by the task.
             *
             * @param preacquired_arena A substitute arena that the task must use to
             *                          access bufstate's data. The arena must
             *                          already be in a state accessible by the
             *                          task's kernel from whatever device
             *                          the task's kernel will execute on.
             *
             *                          If the task may execute on alternative
             *                          devices or multiple devices, multiple
             *                          preacquired arenas must be specified for
             *                          the same buffer via multiple invocations
             *                          of this method corresponding to each of
             *                          the alternative/multiple devices.
             *
             * @note Implementation will be overridden by derived task classes
             *       that handle buffers.
             *
             * @note In addition to thread-unsafe access to the task object,
             *       this call is also not thread-safe w.r.t the specified bufstate.
             *       Consequently, an implementation may only record the bufstate
             *       and the preacquired_arena information, and may not
             *       attempt to access the bufstate or arena objects.
             *
             * Expected use case: special tasks internal to runtime where
             * an outer scope (such as a hetero-pfor pattern) has already
             * acquired the buffers needed by the task.
            */
            virtual void unsafe_register_preacquired_arena(bufferstate* bufstate, arena* preacquired_arena)
            {
                HETCOMPUTE_UNUSED(bufstate);
                HETCOMPUTE_UNUSED(preacquired_arena);
                HETCOMPUTE_UNREACHABLE("Expect to be called only for tasks that involve buffers");
            }

            /**
             * Returns task attributes
             *
             * @return legacy::task_attrs object with all the task attributes
             *
            */
            legacy::task_attrs get_attrs() const
            {
                return _attrs;
            }

            /**
             *  Checks whether the task is anonymous.
             *
             *  @return
             *  true - The task is anonymous.
             *  false - The task is not anonymous.
            */
            bool is_anonymous() const
            {
                return has_attr(_attrs, internal::attr::anonymous);
            }

            /**
             *  Checks whether the task is blocking.
             *
             *  @return
             *  true - The task is blocking.
             *  false - The task is not blocking.
            */
            bool is_blocking() const
            {
                return has_attr(_attrs, hetcompute::internal::legacy::attr::blocking);
            }

            /**
             *  Checks whether the task is meant for execution on a big core.
             *
             *  @return
             *  true - The task is a big task.
             *  false - The task is not a big task.
            */
            bool is_big() const
            {
                return has_attr(_attrs, hetcompute::internal::legacy::attr::big);
            }

            /**
             *  Checks whether the task is meant for execution on a LITTLE core.
             *
             *  @return
             *  true - The task is a LITTLE task.
             *  false - The task is not a LITTLE task.
            */
            bool is_little() const
            {
                return has_attr(_attrs, hetcompute::internal::legacy::attr::little);
            }

            /**
             *  Checks whether the task is long running.
             *
             *  @return
             *  true - The task is long running.
             *  false - The task is not long running.
            */
            bool is_long_running() const
            {
                return has_attr(_attrs, hetcompute::internal::attr::long_running);
            }

            /**
             *  Checks whether the task is a stub task.
             *
             *  @return
             *  true - The task is a stub task.
             *  false - The task is not a stub task.
            */
            bool is_stub() const
            {
                return has_attr(_attrs, internal::attr::stub);
            }

            /**
             *  Checks whether the task is a trigger task.
             *
             *  @return
             *  true - The task is a trigger task.
             *  false - The task is not a trigger task.
            */
            bool is_trigger() const
            {
                return has_attr(_attrs, internal::attr::trigger);
            }

            /**
             *  Checks whether the task is a pfor task.
             *
             *  @return
             *  true - The task is a pfor task
             *  false - The task is not a pfor task.
             */
            bool is_pfor() const
            {
                return has_attr(_attrs, internal::attr::pfor);
            }

            /**
             *  Checks whether the task is cancelable.
             *
             *  @return
             *  true - The task is cancelable.
             *  false - The task is not cancelable.
             */
            bool is_cancelable() const
            {
                return !has_attr(_attrs, internal::attr::non_cancelable);
            }

            /**
             *  Checks whether the task is a yield task.
             *
             *  @return
             *  true - The task is a yield task.
             *  false - The task is not a yield task.
             */
            bool is_yield() const
            {
                return has_attr(_attrs, internal::attr::yield);
            }

            /**
             *  Checks whether the task is a gpu task.
             *
             *  @return
             *  true - The task is a gpu task.
             *  false - The task is not a gpu task.
             */
            bool is_gpu() const
            {
                return has_attr(_attrs, hetcompute::internal::legacy::attr::gpu);
            }

            /**
             *  Checks whether the task is a cpu task.
             *
             *  @return
             *  true - The task is a cpu task.
             *  false - The task is not a cpu task.
             */
            bool is_cpu() const
            {
                return has_attr(_attrs, hetcompute::internal::legacy::attr::cpu);
            }

            /**
             *  Checks whether the task is a dsp task.
             *
             *  @return
             *  true - The task is a dsp task.
             *  false - The task is not a dsp task.
             */
            bool is_dsp() const
            {
                return has_attr(_attrs, hetcompute::internal::legacy::attr::dsp);
            }

            /**
             *  Checks whether the task is scheduled inline.
             *
             *  @return
             *  true - The task is scheduled inline.
             *  false - The task is scheduled inline.
             */
            bool is_inlined() const
            {
                return has_attr(_attrs, internal::attr::inlined);
            }

            /**
             *  Checks whether all the task arguments are bound.
             *
             *  @return
             *  true - All the arguments in the task are bound.
             *  false - At least one argument in the task is not bound.
             */
            bool is_bound()
            {
                return _state.get_snapshot().is_bound();
            }

            /**
             *  Checks whether the task is ready or not.
             *
             *  @param snapshot task state snapshot.
             *  @return
             *    true - The task is ready to be sent to the runtime
             *    false - The task cannot be sent to the runtime
             */
            bool is_ready(state_snapshot const& snapshot)
            {
                return snapshot.is_ready(is_cancelable());
            }

            /**
             * Checks whether the task is launched.
             *
             * @return
             *    true - Task is already launched
             *    false - Task is not yet launched
             */
            bool is_launched()
            {
                return get_snapshot().is_launched();
            }

            /**
             * Transitions trigger task to finish state and signals any task
             * that could be waiting on it.
             *
             * If schedule() changes, this method needs to be changed accordingly.
             *
             *  @param g_was_canceled Specifies whether trigger task associated
             *        with some group g should complete or cancel. Group g may have
             *        been canceled and any task that invoked finish_after(g) should
             *        see the cancellation.
             */
            inline void execute_trigger_task(bool g_was_canceled);

            /**
             * Updates the task on entering a hetcompute::blocking code block,
             * and blocks to execute the entire code block
             * (including completion of the cancellation handler, if invoked).
             *
             * @param bc Pointer to blocking code to execute.
             */
            void execute_blocking_code(blocking_code_container_base* bc);

            /**
             * A wait_for(t) might attempt to execute 't' inline at the same time as
             * some other thread (or the same thread later) attempts to dequeue and
             * execute 't'. This function ensures that only one of them succeeds in
             * owning 't' for execution. This function establishes idempotency of task
             * scheduling (and therefore task execution) in HETCOMPUTE.
             * i.e. For a ready task 't', schedule(schedule(t)) = schedule(t)
             *
             * @param sched Scheduler that should run the task.
             *
             * @return
             *  true - The caller owns the task.
             *  false - Some other caller owns the task.
            */
            bool request_ownership(scheduler& sched)
            {
                scheduler* expected = nullptr;
                return std::atomic_compare_exchange_strong_explicit(&_scheduler, // obj
                                                                    &expected,   // expected
                                                                    &sched,      // desired
                                                                    hetcompute::mem_order_acq_rel,
                                                                    hetcompute::mem_order_relaxed);
            }

            /**
             * Mark the task as no longer owned by any scheduler.
             *
             * A task may relinquish its scheduler if the task failed to acquire buffers,
             * and is marked for re-scheduling at a later point.
             */
            void relinquish_scheduler()
            {
                _scheduler.store(nullptr, hetcompute::mem_order_release);
            }

            /**
             * Returns an id for the body, so that we can identify
             * the different task types in the loggers.
             *
             * @return
             *   uintptr_t -- body typeid stored in an std::uintptr_t
             */
            uintptr_t get_source() const
            {
                return do_get_source();
            }

            /**
             * Returns a snapshot of the task state.
             *
             * @param
             *   mem_order Memory order to use when loading the state value. Default is
             *             hetcompute::mem_order_acquire.
             *
             *   @return
             *    state_snapshot object with the current task status
             */
            state_snapshot get_snapshot(hetcompute::mem_order order = hetcompute::mem_order_acquire) const
            {
                return _state.get_snapshot(order);
            }

            /**
             * Returns initial state for anonymous tasks.
             *
             *   @return
             *    state_snapshot object with the default initial state for anonymous
             *                   task.
             */
            static constexpr state_snapshot get_initial_state_anonymous()
            {
                return state_snapshot(task_internal::state::initial_states::anonymous);
            }

            /**
             * Returns initial state for non-anonymous, bound tasks.
             *
             *   @return
             *    state_snapshot object with the default initial state non-anonymous,
             *                   bound tasks.
             */
            static constexpr state_snapshot get_initial_state_bound()
            {
                return state_snapshot(task_internal::state::initial_states::unlaunched);
            }

            /**
             * Returns initial state for non-anonymous, unbound tasks.
             *
             *   @return
             *    state_snapshot object with the default initial state non-anonymous,
             *                   unbound tasks.
             */
            static constexpr state_snapshot get_initial_state_unbound()
            {
                return state_snapshot(task_internal::state::initial_states::unbound);
            }

            /**
             * Returns initial state for value tasks.
             *
             *   @return
             *    state_snapshot object with the default initial state for value tasks.
             */
            static constexpr state_snapshot get_initial_state_value_task()
            {
                return state_snapshot(task_internal::state::initial_states::completed_with_value);
            }

            /**
             *  Decreases the predecessor counter of the task by 1.
             *
             *  When the predecessor counter reaches 0:
             *
             *  - If the task is ready, send it to the runtime
             *  - if it has no predecessors, but it is marked for cancellation,
             *    then cancel it.
             *
             *  If the previous task executed within a scheduler, it might
             *  pass a pointer to it as well. By using the same scheduler
             *  we can improve performance.
             *
             *  @param sched Scheduler that executed the predecessor task. It
             *         could be nullptr.
             *
             *  @param tbd Predecessor task was scheduled in this task bundle. Likely nullptr.
             */
            void predecessor_finished(scheduler* sched, task_bundle_dispatch* tbd = nullptr);

            /**
             *  Returns scheduler of current task, or nullptr if none.
             *
             *  @param order Memory order for the load operation.
             *
             *  @return task's scheduler or nullptr if none.
             */
            scheduler* get_scheduler(hetcompute::mem_order order)
            {
                return _scheduler.load(order);
            }

            /**
             * Suspends the caller until this task has finished execution.
            */
            hc_error wait();

            /**
             * Yields from task back to scheduler.
             */
            void yield();

            /**
             *  Cancels task, propagates cancellation and leaves groups if the
             *  task is not already canceled.
             *  @return
             *   true - The task wasn't mark for cancellation before, but it is now.
             *   false - The task was already marked for cancellation.
             */
            bool cancel()
            {
                auto l = get_lock_guard();
                return cancel_unlocked();
            }

            /**
             * Cancels tasks and leaves groups if the task is not already
             * canceled. This method is only supposed to be called when some
             * form of cancellation propagation
             *
             * @return
             *  true - The task wasn't mark for cancellation before, but it is now.
             *  false - The task was already marked for cancellation.
            */
            bool predecessor_canceled();

            /**
             * Returns true if the user has requested the task to be canceled.
             * Always returns false if the task is non-cancelable
             *
             * @param has_cancel_request Return true if the task has a cancel request
             *         regardless of whether the task is already running or completed
             *  @return
             *  true - The user has canceled the task or one of the groups
             *          it belongs to.
             *  false - Task is either non-cancelable or the user has not canceled the
             *          task nor any of the groups it belongs to
             */
            bool is_canceled(bool has_cancel_request = false);

            /**
             * Throws an exception that will be caught by HETCOMPUTE's runtime.
             * The programmer calls this method within a task to acknowledge
             * that the task is canceled.
             */
            void abort_on_cancel();

            /**
             *  Adds task to group g. It locks the task to prevent it from
             *  transitioning to completed.
             *
             *  @param g Target group
             *  @param add_utcache Indicates whether the task should be added to
             *         the unlaunched task cache.
             */
            void join_group(group* g, bool add_utcache);

            /**
             *  Returns the group the task belongs to.
             *
             *  @param mem_order Memory synchronization ordering for the
             *  operation. The default is hetcompute::mem_order_acquire.
             *
             *  @return Pointer to the group the task belongs to.
             */
            group* get_current_group(hetcompute::mem_order mem_order = hetcompute::mem_order_acquire) const
            {
                return _current_group.load(mem_order);
            }

            /**
             *  Joins task to group and marks it as being in the utcache.
             *  It is an error if the task already belongs to a group or
             *  it was already in the utcache.
             *
             *  @param g Target group.
             */
            inline void join_group_and_set_in_utcache(group* g);

            /**
             * Marks the task as not being in the utcache
             */
            void reset_in_utcache()
            {
                _state.reset_in_utcache();
            }

            /**
             *  Chooses which add_task_dependence to call
             *
             *  @param succ Pointer to successor task.
             */
            void add_control_dependency(task* succ);

            /**
             * Used to enforce rescheduling when a buffer conflict occurs.
             *
             * Both the predecessor and successor must be launched and ready.
             * The successor's state is reset to not-running.
             *
             * @param succ Pointer to successor task.
             *
             * @returns true if dependency was added, false if predecessor has already
             *          finished.
             */
            bool add_dynamic_control_dependency(task* succ);

            /**
             *  Returns a string that describes the task, including
             *  its state and its body.
             *
             *  @return std::string Task description.
             */
            std::string to_string();

            /**
             *  Returs log id for task object
             *
             *  @return log::task_id
             */
            const log::task_id get_log_id() const
            {
                return _log_id;
            }

            /**
             * Returns reference to task local storage.
             *
             * @return storage_map reference to local task storage.
             */
            storage_map& get_taskls()
            {
                return _taskls_map;
            }

            /**
             * Mark current task as finishing after task 't'.
             *
             * @param t Task after which current task must finish
             */
            void finish_after(task* t);

            /**
             * Mark current task as finishing after group 'g'.
             *
             * Ensures that 'g' does not get garbage collected before finish_after is
             * done with it.
             *
             * @param g Group after which current task must finish
             */
            void finish_after(group* g);

            /**
             * finish after task t and execute fn before finishing the current task
             *
             * @param t Task after which current task must finish
             * @param fn Function to execute prior to finishing current task
             */
            template <typename StubTaskFn>
            void finish_after(task* t, StubTaskFn&& fn)
            {
                // If task 't' has already finished execution, finish_after_impl will do the
                // needful.
                finish_after_impl(t, this, [this, fn] {
                    fn();
                    finish_after_stub_task_body(this);
                });
            }

            /**
             * Add alternative implementation, encapsulated in t, to current task.
             *
             * The scheduler is free to dispatch either the current task or its alternative.
             *
             * @param t Task encapsulating the alternative implementation of the current task.
             */
            void add_alternative(task* t)
            {
                if (_alternatives == nullptr)
                {
                    _alternatives = std::unique_ptr<std::vector<task*>>(new std::vector<task*>());
                }
                _alternatives->push_back(t);
            }

            /**
             * Get all alternative implementations of current task.
             *
             * @return std::vector of alternative task implementations
             */
            std::vector<task*>& get_alternatives()
            {
                HETCOMPUTE_INTERNAL_ASSERT(is_poly(), "must call get_alternatives only on a poly task");
                return *_alternatives;
            }

            /**
             * Get alternative task specified by variant
             * @param variant Alternative to get. Starts from 1.
             *
             * @return pointer to alternative task (never nullptr)
             */
            task* get_alternative(size_t variant)
            {
                HETCOMPUTE_INTERNAL_ASSERT(variant > 0, "variants start from 1");
                HETCOMPUTE_INTERNAL_ASSERT(_alternatives->size() >= variant, "specified variant does not exist");
                auto t = get_alternatives()[variant - 1];
                HETCOMPUTE_INTERNAL_ASSERT(t != nullptr, "specified alternative task does not exist");
                return t;
            }

            /**
             * Returns whether the task is a poly task (i.e. contains a polykernel)
             * @return true if poly task, else false
             */
            bool is_poly() const
            {
                return (_alternatives != nullptr) && (_alternatives->size() > 0);
            }

            /**
             * Get variant of task suitable for execution in specified task domain
             *
             * NOTE: Method returns variants starting from 1, i.e. 1, 2, ...
             *       This may differ from how the variants are stored inside the task.
             *
             * @param td task domain in which to execute task
             * @return variant of task suitable for execution in specified domain
             */
            inline size_t get_suitable_alternative(::hetcompute::internal::task_domain td);

            /**
             * Returns whether specified variant is the user-facing alternative of the
             * poly task.
             *
             * @param variant variant to check
             */
            inline bool is_poly_user_facing(size_t variant) const
            {
                return variant == 0;
            }

#ifdef HETCOMPUTE_HAVE_GPU
            /**
             * Task launched into the GPU runtime. Take appropriate followup actions
             * such as notifying GPU successors.
             *
             * @param sched Scheduler that executed the current task, could be nullptr.
             * @param tbd Task is part of this task bundle. Likely nullptr.
             */
            void launched_gpu(scheduler* sched, task_bundle_dispatch* tbd)
            {
                // It is important to lock the successors list at this point so that if
                // concurrently a task is added as a predecessor, either
                // (1) the dependence is added first and eliminated below, or
                // (2) the dependence is not added at all because the current task's
                //     transition to RUNNING would be visible in the other thread.
                // See task::add_task_dependence
                state_snapshot snapshot = _state.lock();
                _successors.gpu_predecessor_finished(sched, tbd);
                snapshot = unlock();
                log::fire_event<log::events::task_launched_into_gpu>(this);
            }
#endif // HETCOMPUTE_HAVE_GPU

            /**
             * Propagates exception to task t.
             *
             *  _exception_state is not being accessed concurrently.
             *
             * @param t Task to which propagate
             */
            void propagate_exception_to(task* t)
            {
                if (_exception_state == nullptr)
                {
                    return;
                }
                // Exception state may be propagating from multiple predecessors of t to t
                // concurrently. Lock t.
                auto l = t->get_lock_guard();
                if (t->_exception_state == nullptr)
                {
                    t->_exception_state = exception_state_ptr(new exception_state());
                }
                t->_exception_state->propagate(_exception_state);
            }

            /**
             * Removes task from its groups AND sets group pointer to null.
             * It ends up calling leave_groups_unlocked(group, must_cancel).
             * This method assumes that the caller holds the task mutex.
             *
             * @param must_cancel Indicates whether the group must be canceled or not.
             *        By default, it's false
             */
            inline void leave_groups_unlocked(bool must_cancel = false);

        public:
            /**
             * Destructor.
             */
            virtual ~task();

            /**
             * Returns a pointer to an object of type Facade.
             *
             * Each task stores a pointer to it self. We reinterpret
             * this pointer as task<Something(Something)> in the
             * user side of things.
             *
             * @return Pointer to an already initialized object of type Facade
             */
            template <typename Facade>
            Facade* get_facade()
            {
                // static_assert(sizeof(Facade) == sizeof(void*), "Can't allocate Facade.");
                return reinterpret_cast<Facade*>(&_self);
            }

            /**
             * YOU SHALL NOT use this method. This is here just to make
             * debugging easier.
             *
             * @todo It can cause issues because task has virtual functions. It works
             *       99% of the time. Should we remove it?
             *
             * @return Offset of the _state variable.
             */
            static size_type get_state_offset();

            // TODO: Change these constructors protected once create_task is migrated

            /**
             *  Constructor for API20 tasks.
             *
             *  @param initial_state Initial state of the task.
             *  @param g Target group.
             *  @param a Task attributes.
             */
            task(state_snapshot initial_state, group* g, legacy::task_attrs a)
                : _successors(),
                  _state(initial_state),
                  _scheduler(nullptr),
                  _taskls_map(),
                  _current_group(g),
                  _self(this),
                  _attrs(a),
                  _finish_after_state(nullptr),
                  _exception_state(nullptr),
                  _log_id(),
                  _blocking_code_state(nullptr),
                  _alternatives(nullptr)
            {
                log::fire_event<log::events::task_created>(this);
            }

            /**
             *  Constructor for API10 tasks
             *
             *  @param g Target group.
             *  @param a Task attributes.
             */
            task(group* g, legacy::task_attrs a)
                : _successors(),
                  _state(has_attr(a, attr::anonymous) ? get_initial_state_anonymous() : get_initial_state_bound()),
                  _scheduler(nullptr),
                  _taskls_map(),
                  _current_group(g),
                  _self(this),
                  _attrs(a),
                  _finish_after_state(nullptr),
                  _exception_state(nullptr),
                  _log_id(),
                  _blocking_code_state(nullptr),
                  _alternatives(nullptr)
            {
                log::fire_event<log::events::task_created>(this);
            }

        protected:

            /**
             *  Sets the state to either COMPLETED or CANCELED and cleans up the
             *  task. Cleaning the task up means that:
             *
             *  - The successors of the task get their predecessor count
             *  decremented.
             *
             *  - The task gets removed from the utcache.
             *
             *  - The task leaves its groups
             *
             *  - The runtime drops its reference to the task. Therefore, if the
             *  user does not own a task_ptr to it, the task MIGHT NOT EXIST
             *  after this method returns.
             *
             *  Notice that this method locks the task in order to transition to
             *  a finished state (unless the task is anonymous).
             *
             *  @param must_cancel
             *    true - the task must transition to CANCELED state.
             *    false - the task must transition to COMPLETED state.
             *
             *  @param eptr
             *    Pointer to a exception launched by this task or one of
             *            its predecessors.
             *
             *
             *  @param should_set_exeception
             *    true - set any thrown exceptions inside the task's exceptions data structure
             *    false - do not set -ditto-
             *
             *  @param from_foreign_thread
             *    true - finish() is being called from a foreign thread (e.g. OpenCL runtime thread)
             *    false - finish() is not being called from a foreign thread
             */
            void finish(bool must_cancel, std::exception_ptr eptr, bool should_set_exception = true, bool from_foreign_thread = false);

            /**
             *  Marks the task as bound. This means that all its arguments
             *  have been bound.
             */
            void set_bound()
            {
                // Notice that marking the task as bound cannot cause a transition
                // from no-ready to ready. The reason is that We don't allow tasks
                // to be launched if they are not bound. Thus, when the user binds
                // the task arguments, the task is not launched, thus not ready.
                _state.set_bound(this);
            }

            /**
             * Returns reference to successors data structure.
             *
             * @return
             *  successor_list Reference to successors data structure.
             */
            successor_list& get_successors()
            {
                return _successors;
            }

            /**
             * Propagates return value to the successors.
             *
             * Propagation might imply both moving or copying, depending
             * on the semantics.
             *
             * @param succs Reference to a successor list.
             * @param value Pointer to value to copy
             */
            virtual void propagate_return_value_to_successors(successor_list& succs, void* value)
            {
                succs.propagate_return_value<void>(value);
            }

            void propagate_exceptions_to_successors(successor_list& succs)
            {
                succs.propagate_exception(this);
            }

            /**
             *  Prepares to add a task dependence
             *
             *  @param pred Predecessor task
             *  @param succ Successor task
             *  @return
             *    true -- If task dependency needs to be added.
             *    false -- If task dependency need not be added.
             */
            static bool prepare_to_add_task_dependence(task* pred, task* succ, bool& should_copy_data);

            /**
             *  Cleans up after a task dependence has been added
             *
             *  @param pred Predecessor task
             *  @param succ Successor task
             */
            static void cleanup_after_adding_task_dependence(task* pred, task* succ);

            /**
             * Adds a dependence between pred and succ.
             *
             *  @param pred task to execute first.
             *  @param succ task to execute second.
             */
            static void add_task_dependence(task* pred, task* succ);

            /**
             * Locks the task and returns a lock_guard that will unlock the
             * task when it goes out of scope.
             *
             * @return
             *   lock_guard lock_guard that will unlock the task when it goes
             *              out of scope.
             */
            lock_guard get_lock_guard()
            {
                return lock_guard(*this);
            }
        private:
            /**
             * Cancels task and its children without grabbing the task lock.
             * It assumes that the caller holds the task mutex.
             *
             * @return
             *  true - The task wasn't mark for cancellation before, but it is now.
             *  false - The task was already marked for cancellation.
             */
            bool cancel_unlocked();

            /**
             * Calls task::predecessor_canceled on all the task that succeed this
             * in the task graph. It assumes that the caller holds the task mutex.
             */
            void propagate_cancellation_unlocked();

            /**
             * Removes task from group g.
             *
             * TODO: This should be a group method, because it doesn't do anything
             * on the task itself
             *
             * @param g group from which to remove task
             */
            inline void leave_groups_unlocked(group* g, bool must_cancel = false);

            /**
             * Makes g the task' current group.
             * This method assumes that the caller holds the task mutex.
             *
             *  @param g Group the task has joined
             */
            void set_current_group_unlocked(group* g)
            {
                // We can use relaxed because we own the task lock when
                // we call this method
                _current_group.store(g, hetcompute::mem_order_relaxed);
            }

            /**
             *  Checks whether the task may bypass scheduling by a CPU device thread.
             *
             * @return
             *   true -- The task may bypass scheduling.
             *   false -- The task won't bypass scheduling.
             */
            bool may_bypass_scheduler() const
            {
                return is_gpu() || is_blocking() || is_inlined() || is_dsp() || is_poly();
            }

            /**
             * Notifies (direct) successors that task has finished.
             *
             *
             * TODO This method should probably be deleted.
             * It has nothing to do with the task. I tried
             * to get rid of it quickly, but there are issues
             * with the eptr.
             *
             *
             * @param must_cancel Indicates whether successors must be canceled
             * @param sched Scheduler where the task shou
             * @param successors List of tasks that are control or data dependent on this
             */
            void notify_successors(bool must_cancel, scheduler* sched, successor_list& successors)
            {
                successors.predecessor_finished(must_cancel, sched);
            }

            /**
             * Sets the scheduler of the task
             *
             *  @param sched Reference to the scheduler than runs or finishes the task
             */
            void set_scheduler(scheduler& sched)
            {
                // Some types of tasks -- gpu, blocking, inline, and anonymous -- are not
                // subject to the inlined wait_for optimization, and therefore need not
                // be owned by a scheduler by calling request_ownership before executing
                // the task. For these tasks, we can just store the scheduler pointer. We
                // can use relaxed because the first instruction in schedule is a barrier.
                _scheduler.store(&sched, hetcompute::mem_order_relaxed);
            }

            /**
             *  Returns the task's group and sets the task's group to null.
             *  Assumes that the task lock has
             *
             *  @param mem_order Memory synchronization ordering for the
             *  operation. The default is hetcompute::mem_order_acquire.
             *
             *  @return Pointer to the group the task belongs to.
             */
            group* move_current_group_unlocked(hetcompute::mem_order mem_order = hetcompute::mem_order_acquire)
            {
                auto g = get_current_group(mem_order);
                set_current_group_unlocked(nullptr);
                HETCOMPUTE_INTERNAL_ASSERT(get_current_group() == nullptr, "Unexpected non-null group pointer");
                return g;
            }

            /**
             * Moves successor list out of this task.
             *
             * @return Rvalue reference this task's successor list
             */
            successor_list&& move_successors_unlocked()
            {
                return std::move(_successors);
            }

            /**
             * Returns whether current task can be executed in the specified domain
             * @param td domain in which to execute task
             * @return true if current task may be executed in domain td, else false
             */
            bool is_task_executable_in_domain(::hetcompute::internal::task_domain td);

            /**
             * Base implementation of do_get_source. It does nothing. The base
             * task calss has no knowledge about the code the task will execute.
             * It's up to its descendants to return the proper info.
             *
             * @return Returns 0.
             */
            virtual uintptr_t do_get_source() const
            {
                return 0;
            }

            /**
             * Checks whether the task or any of its predecessors threw
             * an exception.
             *
             * @return
             *  true  -- The task or any of its predecessors threw an exception.
             *  false -- Neither the task or any of its predecessors threw an exception.
             */
            bool is_exceptional()
            {
                return _exception_state != nullptr;
            }

            /**
             * Call only after checking that the task is exceptional.
             *
             * Throws all exceptions in the task exception state.
             */
            void throw_exceptions()
            {
                HETCOMPUTE_INTERNAL_ASSERT(is_exceptional(), "must be called only if exceptional");
                auto guard = get_lock_guard();
                _exception_state->rethrow();
                HETCOMPUTE_UNREACHABLE("exception should have been thrown");
            }

            /**
             * Adds exception to exception state.
             *
             * This method assumes that the caller holds the task mutex.
             *
             * @param eptr Reference to exception pointer to set.
             */
            void set_exception_unlocked(std::exception_ptr& eptr)
            {
                HETCOMPUTE_INTERNAL_ASSERT(eptr != nullptr, "cannot set empty exception");
                if (_exception_state == nullptr)
                {
                    _exception_state = exception_state_ptr(new exception_state());
                }
                _exception_state->add(eptr);
            }

            /**
             * Propagate exception to group g.
             *
             * Invariant: _exception_state is not being accessed concurrently.
             *
             * @param g Group to which propagate
             */
            void propagate_exception_to(group* g);

            /**
             * Returns a string that describes the body of the task
             *
             * @return std::string Task body description.
             */
            virtual std::string describe_body();

            /**
             * Get a reference to the finish_after_state
             * @return Reference to the finish_after object. Returns nullptr if
             *         finish_after has never been invoked by the current task.
             */
            finish_after_state_ptr& get_finish_after_state()
            {
                return _finish_after_state;
            }

            /**
             * Set stub task to track finish_after
             *
             * @tparam StubTask StubTask type.
             * @param tfa Reference to pointer to finish_after_state.
             * @param stub Pointer to stub task.
             */
            template <typename StubTask>
            void set_finish_after_state(finish_after_state_ptr& tfa, StubTask const& s)
            {
                HETCOMPUTE_INTERNAL_ASSERT((tfa == nullptr), "function must be invoked exactly once for each task");
                tfa                          = finish_after_state_ptr(new finish_after_state());
                tfa->_finish_after_stub_task = s;
            }

            /**
             * Checks whether current task is set to finish_after some other task/group.
             *
             * @param tfa Reference to pointer to finish_after_state
             *
             * @return true if current task has already invoked finish_after, false
             *         otherwise
             */
            bool should_finish_after(finish_after_state_ptr& tfa)
            {
                if (tfa == nullptr)
                {
                    return false;
                }
                return tfa.get()->_finish_after_stub_task != nullptr;
            }

            template <typename StubTaskFn>
            void finish_after_impl(task* pred, task* succ, StubTaskFn&& fn);

            /**
             *
             * Checks whether task t has any exceptions and either throw or propagate them
             * to this.
             *
             * @param t Task to check
             *
             * @param t_canceled set to true if t is already known to be canceled
             *
             * @param propagate set to true if this task's exceptions should be
             *                   propagated to t; if set to false, the exceptions are
             *                  thrown immediately
             *
             *
             * @returns true if no exceptions (or cancellation), false otherwise
             */
            bool check_and_handle_exceptions(task* t, bool t_canceled, bool propagate = false);

            /**
             * Launches finish_after stub task. This should happen after the task body executed.
             *
             * @param fa Reference to pointer to finish_after_state
             * @param must_cancel Task has been canceled; take appropriate action
             * @param sched Scheduler used to launch the task
             */
            void launch_finish_after_task(finish_after_state_ptr& fa, bool must_cancel, scheduler* sched);

            /**
             * Notifies the currently executing hetcompute::blocking code block (if any)
             * of cancellation.
             *
             * Assumes that the caller holds the task lock.
             */
            void cancel_blocking_code_unlocked();

            /**
             * Actually executes the task body.
             *
             * This class knows nothing about the task body, so it's up the descendants to
             * take care of it.
             *
             * @param tbd structure to be used if this task is scheduled as part of a
             *            bigger task subgraph
             *
             * @returns true if task actually executed, false otherwise (may return false
             *          to indicate buffers were not successfully acquired)
             */
            virtual bool do_execute(task_bundle_dispatch* tbd = nullptr) { return false; }

            /**
             * Body of stub task created for task invoking finish_after
             *
             * @param t Task which invoked finish_after
             */
            inline void finish_after_stub_task_body(task* t);

            /**
             * Destroys body and arguments
             * TODO: must destroy return value as well
             */
            virtual void destroy_body_and_args(){};

            /**
             * An orphan task is a task that has no predecessors, hasn't been
             * launched and whose last reference has gone out of scope.
             * This method cleans up orphan task. Cleaning the task up means that:
             *
             * - The successors of the task get their predecessor count
             * decremented.
             *
             * - The task gets removed from the utcache.
             *
             * - The task leaves its groups
             *
             * The task WILL NO LONGER EXIST after this method returns.
             *
             * Notice that this method does not lock the task because the
             * are no other pointers to the task. Similarly, it doesn't
             * transition the task to CANCELED or COMPLETED because
             * nobody would see it.
             *
             * @param in_utcache
             * true - the task is in ut_cache.
             * false - the task is not in ut_cache.
             */
            void finish_orphan_task(bool in_utcache);

            /**
             * Mark alternatives of current task as finish()ed
             *
             * Primarily meant to leave_groups() and unref()
             *
             * @param skip_alternative do not finish this alternative task
             */
            void finish_poly_task(task* skip_alternative);

            /**
             * Returns reference count
             *
             * @return The number of hetcompute_shared_ptr and predecessors pointing
             * to this task
             */
            ref_counter_type use_count() const
            {
                return _state.get_snapshot().get_num_references();
            }

            /**
             * Removes one reference to the task. If the ref count
             * gets to zero, releases the task.
             */
            inline void unref();

            /**
             * Increases the reference count of the task by one.
             */
            void ref()
            {
                auto snapshot = _state.ref();
                auto num_refs = snapshot.get_num_references() + 1;
                log::fire_event<log::events::task_reffed>(this, num_refs);
            }

            /**
             * Locks the task. If someone else has already locked it, we spin
             * until we can lock the task ourselves. A deadlock will occur
             * if a thread that already locked the task calls this method
             * again.
             *
             * @return
             *   state_snapshot with the task state prior to locking it.
             */
            state_snapshot lock()
            {
                return _state.lock();
            }

            /**
             * Unlocks the task. Does not return until the task is unblocked.
             *
             *  @return
             *    state_snapshot with the task state prior to unlocking it.
             *    Thus, the only difference between the state returned by
             *    this method and the actual one is the lock bit.
             */
            state_snapshot unlock()
            {
                return _state.unlock(this);
            }

            /**
             * Checks whether the task is locked.
             * @return
             *  true - the task is locked.
             *  false - the task is not locked.
             */
            bool is_locked()
            {
                return get_snapshot().is_locked();
            }

            /**
             * Increases number of predecessors
             *
             * @param dynamic_dep set to true if a dynamic dependency (after task launch)
             *                    is being created, false otherwise
             */
            void predecessor_added(bool dynamic_dep = false)
            {
                _state.add_predecessor(this, dynamic_dep);
            }

            /** Sets the state to RUNNING if neither the task nor the
             * group have been marked for cancellation. See group.hh
             *
             * @return
             *   true - the task was able to transition to running
             *   false - the task could not transition to running
             */
            bool set_running();

            /**
             * Changes the state from RUNNING to NOT RUNNING.
             *
             * Used when a buffer conflict is detected and a dynamic dependency has been
             * set up to the current task.
             */
            void reset_running()
            {
                _state.reset_running();
            }

            /**
             * Returns pointer to the task return value.
             *
             * @return This method always returns nullptr.
             */
            virtual void* get_value_ptr()
            {
                return nullptr;
            }

            friend struct task_release_manager;
            friend class testing::task_tests;
            friend class task_internal::lock_guard;

            friend class hetcompute_shared_ptr<task>;
            friend void explicit_unref<task>(task* obj);
            friend void explicit_ref<task>(task* obj);

#ifdef HETCOMPUTE_HAVE_OPENCL
            friend void CL_CALLBACK completion_callback(cl_event event, cl_int, void* user_data);
            friend void CL_CALLBACK task_bundle_completion_callback(cl_event event, cl_int, void* user_data);
#endif // HETCOMPUTE_HAVE_OPENCL

#ifdef HETCOMPUTE_HAVE_GLES
            template <typename GLKernel, typename Range, typename GLFence>
            friend void launch_gl_kernel(executor_construct const& exec_cons,
                                         GLKernel*                 gl_kernel,
                                         Range&                    global_range,
                                         Range&                    local_range,
                                         GLFence&                  gl_fence);
#endif // HETCOMPUTE_HAVE_GLES

            // Disable all copying and movement, since other objects may have
            // pointers to the internal fields of this object.
            HETCOMPUTE_DELETE_METHOD(task(task const&));
            HETCOMPUTE_DELETE_METHOD(task(task&&));
            HETCOMPUTE_DELETE_METHOD(task& operator=(task const&));
            HETCOMPUTE_DELETE_METHOD(task& operator=(task&&));
        }; // class task

        inline void task::unref()
        {
            // Remember that _state.unref() returns the state of the task before
            // removing one reference, so we have to play accordingly
            auto snapshot = _state.unref();

            // Not storing get_num_references in local variable to avoid crazy MSVC 2013 bug
            log::fire_event<log::events::task_unreffed>(this, snapshot.get_num_references() - 1);

            if (snapshot.get_num_references() > 1)
            {
                return;
            }

            HETCOMPUTE_INTERNAL_ASSERT(snapshot.is_ready_running_or_finished(is_cancelable()) || !snapshot.is_launched(),
                                     "Unexpected task state."
                                     "Snapshot: %s\n"
                                     "Actual: %s\n",
                                     snapshot.to_string().c_str(),
                                     to_string().c_str());

            if (snapshot.is_finished())
            {
                task_internal::release_manager::release(this);
                return;
            }

            // If we get here, it is because the last task_ptr to the task
            // has gone out of scope before the task was launched.
            finish_orphan_task(snapshot.in_utcache());
        }

        inline void task::execute_sync(task_bundle_dispatch* tbd, size_t variant)
        {
            // This fence is really important. It allows us to use
            // mem_order_relaxed when loading group information.  If the
            // group changes and we don't see it, it's ok, because that means
            // that the user did it from a differet thread and it's a data race.
            // We'll get the final group after the task executes, once we grab
            // the task lock.
            std::atomic_thread_fence(hetcompute::mem_order_acquire);
            execute(tbd, variant);
        }

        inline void task::execute_trigger_task(bool g_was_canceled)
        {
            HETCOMPUTE_INTERNAL_ASSERT(is_trigger(), "Task %p is not a trigger task: %s", this, to_string().c_str());

            HETCOMPUTE_INTERNAL_ASSERT(!get_snapshot().is_launched(),
                                     "Can't set trigger task %p to COMPLETED because it's"
                                     "already launched: %s",
                                     this,
                                     to_string().c_str());

            HETCOMPUTE_INTERNAL_ASSERT(!get_snapshot().is_finished(),
                                     "Can't set trigger task %p to COMPLETED because it's"
                                     "already launched: %s",
                                     this,
                                     to_string().c_str());

            HETCOMPUTE_INTERNAL_ASSERT(!is_cancelable(), "Trigger task %p cannot be cancelable: %s", this, to_string().c_str());

            log::fire_event<log::events::trigger_task_scheduled>(this);
            finish(g_was_canceled, nullptr);
        }

        inline void task::add_control_dependency(task* succ)
        {
            HETCOMPUTE_INTERNAL_ASSERT(succ != nullptr, "null task_ptr succ.");

            // succ is shared, so it is possible that it has been launched
            // already. Because we don't allow to add a predecessor to a task
            // that has been launched, we have to pay the penalty and do an
            // atomic load
            HETCOMPUTE_API_THROW(!succ->get_snapshot().is_launched(),
                               "Successor task %p has already been launched: %s",
                               succ,
                               succ->to_string().c_str());

            add_task_dependence(this, succ);

            HETCOMPUTE_INTERNAL_ASSERT(succ->get_snapshot().is_launched() == false,
                                     "Successor task %p was launched while "
                                     "setting up dependences: %s",
                                     succ,
                                     succ->to_string().c_str());
        }

        inline bool task::add_dynamic_control_dependency(task* succ)
        {
            HETCOMPUTE_INTERNAL_ASSERT(succ != nullptr, "null task_ptr succ.");
            HETCOMPUTE_INTERNAL_ASSERT(succ->get_snapshot().is_launched(),
                                     "Successor task %p should already be launched: %s",
                                     succ,
                                     succ->to_string().c_str());
            HETCOMPUTE_INTERNAL_ASSERT(succ->get_snapshot().is_ready_running_or_finished(succ->is_cancelable()),
                                     "Successor task %p should already be ready, running, or finished: %s",
                                     succ,
                                     succ->to_string().c_str());
            HETCOMPUTE_INTERNAL_ASSERT(get_snapshot().is_launched(),
                                     "Predecessor task %p should already be launched: %s",
                                     this,
                                     to_string().c_str());
            HETCOMPUTE_INTERNAL_ASSERT(get_snapshot().is_ready_running_or_finished(succ->is_cancelable()),
                                     "Predecessor task %p should already be ready, running, or finished: %s",
                                     this,
                                     to_string().c_str());

            // Lock predecessor to prevent it from transitioning to a final
            // state (COMPLETED, CANCELED)
            // or to (RUNNING) in case of a GPU task
            auto snapshot = lock();

            // If the predecessor task is marked for cancellation, we have to
            // check its state. If isn't running yet, then we can cancel it
            // right away. If the task is finished, then simply return.
            // In either case, we don't need to add the dependence.
            // Note that if the task is running we have to add the dependence
            // regardless of whether the task has been canceled or not because
            // the user might decide to ignore the cancellation request.

            if ((snapshot.has_cancel_request() && snapshot.is_not_running_yet()) || snapshot.is_canceled() || snapshot.is_completed())
            {
                unlock();
                return false;
            }

            _successors.add_control_dependency(succ);
            succ->predecessor_added(true);
            succ->relinquish_scheduler();
            succ->reset_running();

            log::fire_event<log::events::task_dynamic_dep>(this, succ);

            unlock();
            return true;
        }

        /**
         *
         * Sets the current task into TLS.
         *
         * @return
         *   task* - Previous task pointer in TLS.
         */
        inline task* set_current_task(task* t)
        {
            auto ti = tls::get();
            return ti->set_current_task(t);
        }

        /**
         * Returns pointer to the current task.
         *
         * @return
         * task* - Pointer to the task that calls this method.
         * nullptr - The caller is not inside a task.
         */
        inline task* current_task()
        {
            auto ti = tls::get();
            if (ti)
            {
                return ti->current_task();
            }
            else
            {
                return nullptr;
            }
        }

        /**
         * Returns the scheduler that owns the caller.
         *
         * @return
         * scheduler* - Pointer to the scheduler running the current task.
         * nullptr - The caller is not inside a task, or the task does not
         *           have an scheduler.
         */
        inline scheduler* current_scheduler()
        {
            // We can use mem_order_relaxed because _scheduler is
            // set by the same thread that calls current_scheduler.
            if (auto t = current_task())
            {
                return t->get_scheduler(hetcompute::mem_order_relaxed);
            }
            return nullptr;
        }

        /**
         * Returns both the scheduler and the task of the caller.
         *
         * @return
         * std::tuple<task*, scheduler*> - Tuple with pointers to the scheduler
         *                               and the task of the caller.
         * std::tuple<nullptr, nullptr> - The caller is not inside a task
         * std::tuple<task*, nullptr> - The caller is inside a task, but does not have
         *                              an associated scheduler.
         */
        inline std::tuple<task*, scheduler*> current_task_and_scheduler()
        {
            // We can use mem_order_relaxed because _scheduler is
            // set by the same thread that calls current_task_and_scheduler.
            if (auto t = current_task())
            {
                return std::make_tuple(t, t->get_scheduler(hetcompute::mem_order_relaxed));
            }
            return std::make_tuple<task*, scheduler*>(nullptr, nullptr);
        }

        inline bool task::is_task_executable_in_domain(::hetcompute::internal::task_domain td)
        {
            // big/little checks must happen before the cpu check since big/little tasks
            // are also cpu tasks.
            if (is_big())
            {
                return (td == task_domain::cpu_big);
            }
            else if (is_little())
            {
                return (td == task_domain::cpu_little);
            }
            else if (is_cpu())
            {
                return (td == task_domain::cpu_all || (td == task_domain::cpu_big) || (td == task_domain::cpu_little));
            }
            else if (is_gpu())
            {
                return (td == task_domain::gpu);
            }
            else if (is_dsp())
            {
                return (td == task_domain::dsp);
            }
            else
            {
                return false;
            }
        }

        size_t task::get_suitable_alternative(::hetcompute::internal::task_domain td)
        {
            HETCOMPUTE_INTERNAL_ASSERT(is_poly(), "can be called only for a poly task");

            if (is_task_executable_in_domain(td))
            {
                return 0;
            }

            auto alternatives = get_alternatives();
            auto alt = std::find_if(alternatives.begin(), alternatives.end(), [=](task* t) { return t->is_task_executable_in_domain(td); });

            HETCOMPUTE_INTERNAL_ASSERT(alt != alternatives.end(), "must have found a suitable alternative");
            return std::distance(alternatives.begin(), alt) + 1;
        }

        inline void task::finish_after_stub_task_body(task* t)
        {
            HETCOMPUTE_INTERNAL_ASSERT(t != nullptr, "cannot execute a finish_after stub task for a null task");
            task*      st;
            scheduler* sched;
            std::tie(st, sched) = current_task_and_scheduler();
            HETCOMPUTE_INTERNAL_ASSERT(sched != nullptr, "finish_after stub tasks must be executed by a valid scheduler");
            // Task 't' might have been executed by a different scheduler on a different
            // device thread from the ones executing the stub task. Any successors of 't'
            // sent by the stub task to the runtime must use the scheduler of the stub
            // task, not the scheduler of task 't' so that the successors of 't' are
            // enqueued in the local queue of the current device thread
            // (see task::finish which calls get_scheduler on task 't').
            // Consequently, we set the scheduler of task 't' to be the currently
            // executing stub task's scheduler.
            t->set_scheduler(*sched);
            auto snapshot = st->get_snapshot();
            st->propagate_exception_to(t);
            t->finish(snapshot.has_cancel_request(), nullptr);
        }

        /**
         * Yields from the current task to the scheduler
         * This function has no effect if called outside of a Hetcompute task.
         *
         * \par
         *  NOTE: The ability to yield may go away in future releases of HetCompute.
         */
        inline void yield()
        {
            if (auto task = current_task())
            {
                auto t_ptr = c_ptr(task);
                HETCOMPUTE_INTERNAL_ASSERT(t_ptr != nullptr, "null task_ptr");
                t_ptr->yield();
            }
        }

        inline void task::execute_inline_task(group* g = nullptr)
        {
            state_snapshot snapshot;

            HETCOMPUTE_INTERNAL_ASSERT(is_gpu() == false, "Inline execution of GPU tasks is not supported");

            // launch task
            _state.set_launched(snapshot, true);
            HETCOMPUTE_INTERNAL_ASSERT(snapshot.is_launched(),
                                     "Cannot execute task %p inline "
                                     "because it has been launched already: %s.",
                                     this,
                                     to_string().c_str());

            // caller holds reference, runtime does not own task
            // Because we are about to execute it, we are not going
            // to add it to the utcache.
            if (g != nullptr)
            {
                join_group(g, false);
            }

            // We find our parent's scheduler. If we can find it, we schedule ourselves
            // in that scheduler. If we don't find one is not an error. For example,
            // we could call schedule_inline from the main thread (think pfor_each)
            auto sched = current_scheduler();
            if (sched != nullptr)
            {
                execute(*sched);
            }
            else
            {
                execute_sync();
            }
        }

        namespace task_internal
        {
            inline lock_guard::lock_guard(task& task) : _task(task), _snapshot(_task.lock())
            {
            }

            inline lock_guard::lock_guard(task& task, state_snapshot snapshot) : _task(task), _snapshot(snapshot)
            {
            }

            inline lock_guard::~lock_guard()
            {
                _task.unlock();
            }

            inline void release_manager::release(task* t)
            {
                HETCOMPUTE_INTERNAL_ASSERT(debug_is_ok_to_release(t) == true, "Cannot release task t");
#if !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)
                if (!t->is_gpu() && !t->is_dsp())
                {
                    t->~task();
                    task_allocator::deallocate(t);
                }
                else
                {
                    delete (t);
                }
                return;
#else
                delete (t);
                return;
#endif // !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)
            }

            inline void successor_list::predecessor_finished(bool canceled, scheduler* sched)
            {
                if (is_empty())
                {
                    return;
                }

                if (canceled)
                {
                    auto notify_cancel_to_successor = [sched](task* succ) {
                        succ->predecessor_canceled();
                        succ->predecessor_finished(sched);
                    };

                    apply_for_each_and_clear(notify_cancel_to_successor);
                }
                else
                {
                    auto notify_complete_to_successor = [sched](task* succ) { succ->predecessor_finished(sched); };
                    apply_for_each_and_clear(notify_complete_to_successor);
                }
            }

            inline void successor_list::propagate_exception(task* t)
            {
                for_each_successor([t](task* succ) { t->propagate_exception_to(succ); });
            }

        };  // namespace hetcompute::internal::task_internal
    };  // namespace internal
};  // namespace hetcompute
