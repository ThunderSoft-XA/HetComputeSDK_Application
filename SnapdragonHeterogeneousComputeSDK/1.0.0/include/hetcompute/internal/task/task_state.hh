#pragma once

#include <atomic>

#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/util/macros.hh>
#include <hetcompute/internal/util/memorder.hh>

class TaskStateSuite;

namespace hetcompute
{
    namespace internal
    {
        namespace testing
        {
            class task_tests;
        };  // namespace testing

        namespace task_internal
        {
            // forward declaration for friend in state_base.
            class state;

            /// This class stores dynamic information about a task:
            ///   - Task State
            ///   - Whether someone has requested the task be canceled
            ///   - Whether the task is in ut cache
            ///   - Number of predecessors.
            ///
            /// You might ask: why do we put all this info in one place? This is
            /// because when we decide to which state we should transition
            /// next, we need all this data. By putting it into one location,
            /// we can load it all atomically at once.
            ///
            /// Here are the task states:
            ///
            /// "canceled": A task t will eventually transition to "canceled" if:
            ///            - t->cancel() was called before the task started running.
            ///            - t->cancel() was called while the task was running and
            ///              the task acknowledges that it saw the cancellation.
            ///            - The task throws an exception during execution and
            ///               the programmer didn't catch it.
            ///
            /// "completed": Task state after it successfully finishes execution.
            ///
            /// "running": Task state once HETCOMPUTE's scheduler starts executing a
            ///          task. If a task is in this state, it doesn't mean that it
            ///          is being executed.  For example, a task state can be
            ///          "running" but the task might be blocked waiting for another
            ///          task to complete.
            ///
            /// "unlaunched": Task in "unlaunched" state cannot execute, even if they have
            ///             no predecessors.
            ///
            /// The following diagram depicts the all the possible state transitions
            /// for a task. Notice that there are no back arrows.
            ///
            /// "unlaunched"--> "ready" --> "running" ~~> "completed"
            ///                   |
            ///                    ~~~~> "canceled"
            ///
            /// "unlaunched" ~~> "canceled"
            /// "ready" ~~> "canceled"
            /// "running" ~~> "canceled"
            ///
            /// (Transitions depicted by ~~> grab the task's lock)
            ///
            ///
            /// In addition to task states, state base also stores more dynamic
            /// information about the task that could affect its status:
            ///
            ///
            /// cancel_req: Someone has invoked task::cancel(). Notice that this doesn't
            ///           mean that the task will be "canceled". For example, if someone
            ///           calls cancel on a "completed" task, the "completed" task will
            ///           not transition to "canceled"
            ///
            /// in_utcache: The task is in the unlaunched task cache:
            ///
            /// Number of predecessors: number of tasks that must execute before the
            /// task can execute
            ///
            ///
            /// Notice that "ready" means that the task is "not unlaunched",
            /// that nobody has requested its cancellation, and that it has zero
            /// predecessors.
            ///
            class state_base
            {
            protected:
                typedef uint64_t raw_type;
                typedef uint32_t ref_counter_type;

                // [63] "canceled"
                // [62] "completed"
                // [61] "running"
                // [60] "unlaunched"
                // [59] cancel_req
                // [58] ut_cache
                // [57] locked
                // [56] bound
                // [55-32] NUM_PREDS
                // [31-00] REF_CNTR
                enum value : raw_type
                {
                    // Number of bits used by the state.
                    raw_type_nbits = (sizeof(raw_type) * 8),

                    // Bits employed by reference counting.
                    ref_cntr_type_nbits = (sizeof(ref_counter_type) * 8),

                    // Mask for the reference counting bits.
                    ref_cntr_mask = (raw_type(1) << (ref_cntr_type_nbits)) - 1,

                    // Maximum value for reference counter.
                    ref_cntr_max = ref_cntr_mask,

                    // State bits. These are mutually exclusive ------------------------------

                    // Indicates that the task did NOT finish execution as expected.
                    canceled = raw_type(1) << (raw_type_nbits - 1),
                    // Indicates that the task finished execution as expected.
                    completed = canceled >> 1,
                    // Indicates that the task is running.
                    running = completed >> 1,
                    // Indicates that the task hasn't been launched yet.
                    unlaunched = running >> 1,

                    canceled_mask   = ~canceled,
                    completed_mask  = ~completed,
                    running_mask    = ~running,
                    unlaunched_mask = ~unlaunched,

                    // Indicates that the task state object hasn't been initialized
                    // We need to add this value so that we don't use the ready state
                    // (all 0s) as the default value for the state. We pick
                    // "canceled" | "completed" | "running" | "unlaunched" because it's an
                    // impossible state
                    uninitialized = canceled | completed | running | unlaunched,

                    // Initial state value for anonymous tasks.
                    ready_anonymous = 1,
                    // State value for anonymous tasks in "running" state.
                    running_anonymous = running | ready_anonymous,

                    // Info bits. They complement the state bits -----------------------------

                    // If set, it indicates that the task has been marked for cancellation.
                    // Notice that this doesn't mean that the task will be "canceled".
                    // For example, a "completed" task might have its cancel_req bit
                    // set if the user tried to cancel it after if finished its
                    // execution.
                    cancel_req = unlaunched >> 1,
                    // If set, it indicates that the task is in the unlaunched task cache.
                    in_utcache = cancel_req >> 1,
                    // If set, it indicates that the task is locked.
                    locked = in_utcache >> 1,
                    // If set, it indicates that one or more of the task arguments is not bound
                    unbound = locked >> 1,

                    cancel_req_mask = ~cancel_req,
                    in_utcache_mask = ~in_utcache,
                    unlocked_mask   = ~locked,
                    unbound_mask    = ~unbound,

                    // This mask is used to test whether a task is ready.
                    // A task is ready when:
                    //   1) it has no predecessors,
                    //   2) it has been launched (it's not "unlaunched")
                    //   3) it hasn't run yet
                    //   4) it has no cancel request pending.
                    //   5) all its arguments are bound
                    //
                    // Notice that we don't care whether it's locked, it's in the ut_cache or
                    // how many pointers point to it. Thus, we need a mask to get rid
                    // of those values.
                    ready_mask = ~(locked | in_utcache | ref_cntr_mask),

                    // This mask is used to test whether a non-cancelable task is ready.
                    // A non-cancelable task is ready when:
                    //   1) it has no predecessors,
                    //   2) it has been launched (it's not "unlaunched")
                    //   3) it hasn't run yet
                    //   4) all its arguments are bound
                    //
                    // Notice that we don't care whether it's locked, it's in the
                    // ut_cache, whether it has a cancel request pending, or how many pointers
                    // point to it. Thus, we need a mask to get rid of those values.
                    ready_mask_non_cancelable = ~(locked | in_utcache | ref_cntr_mask | cancel_req),

                    // Dispacement for the predecessor counter
                    pred_cntr_disp = ref_cntr_type_nbits,

                    // This mask is used to get the number of task predecessors
                    pred_cntr_mask =
                        ~(canceled | completed | running | unlaunched | cancel_req | in_utcache | locked | unbound | ref_cntr_mask),

                    // Maximum value for predecessor counter.
                    pred_cntr_max = (pred_cntr_mask) >> pred_cntr_disp,

                    // Adding a predecessor means +1 to predecessors and +1 ref counts
                    one_pred = (raw_type(1) << (ref_cntr_type_nbits)) + 1,
                    // Adding a refcount means +1 to refcounts
                    one_ref = 1
                };

            public:
                friend TaskStateSuite;
            };  // class state_base

            // This class allows you to inspect a state object
            class state_snapshot : public state_base
            {
            private:
                // actual snapshot
                raw_type _snapshot;

                /// Constructor. Only state and hetcompute::internal::task should be able
                /// to build a state_snapshot from a raw_type.
                ///
                /// @param snapshot state snapshot;
                explicit constexpr state_snapshot(raw_type snapshot) : _snapshot(snapshot)
                {
                }

                // We need to make state and task friends so they can call the
                // above constructor
                friend class ::hetcompute::internal::task_internal::state;
                friend class ::hetcompute::internal::task;

                // Resets the state value;
                void reset(raw_type snapshot)
                {
                    _snapshot = snapshot;
                }

            public:
                /// Unsigned integral type
                using size_type = ref_counter_type;

                /// Default constructor
                state_snapshot() : _snapshot(value::uninitialized)
                {
                }

                /// Returns the state raw_value.
                ///
                /// @return
                ///    raw_type with the value of the state.
                constexpr raw_type get_raw_value() const
                {
                    return _snapshot;
                }

                /// Returns a string with the description of the state.
                ///
                /// @return
                ///   std::string with the description of the state.
                std::string to_string() const;

                /// Checks whether the task finished execution.
                ///
                /// @return
                ///   true - The state is "completed" or "canceled". Notice that we don't care
                ///          whether the task has been marked for cancellation.
                ///   false - The state is other than "completed" or "canceled".
                bool is_finished() const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_snapshot != value::uninitialized, "State hasn't been initialized yet.");

                    return _snapshot >= completed;
                }

                /// Checks whether the task successfully finished execution.
                ///
                ///  @return
                ///    true - The state is "completed". Notice that we don't care whether
                ///           the task has been marked for cancellation.
                ///    false - The state is other than "completed".
                bool is_completed() const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_snapshot != value::uninitialized, "State hasn't been initialized yet.");

                    return ((_snapshot & completed) != 0);
                }

                /// Checks whether the task successfully finished execution.
                ///
                /// @return
                ///     true - The state is "canceled". Notice that we don't care whether
                ///            the task has been marked for cancellation.
                ///     false - The state is other than "canceled".
                bool is_canceled() const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_snapshot != value::uninitialized, "State hasn't been initialized yet.");

                    return _snapshot >= canceled;
                }

                /// Checks whether the task is executing.
                ///
                /// @return
                ///   true - The state is "running". Notice that we don't care whether
                ///          the task has been marked for cancellation.
                ///   false - The state is other than "running".
                bool is_running() const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_snapshot != value::uninitialized, "State hasn't been initialized yet.");

                    return ((_snapshot & running) != 0);
                }

                /// Checks whether a task is owned by the runtime.
                /// A task is runtime owned for sure if:
                ///  - it has been launched AND
                ///  - it's not finished AND
                ///  - its ref count is one
                bool is_runtime_owned() const
                {
                    static constexpr raw_type RUNTIME_OWNED = (ref_cntr_mask | pred_cntr_mask | unlaunched | completed | canceled);

                    return ((_snapshot & RUNTIME_OWNED) == raw_type(1));
                }

                /// Checks whether the task can be placed in the ready queue.
                ///
                ///  @param cancelable_task Indicates whether the task is cancelable.
                ///  A non-cancelable task might have its cancel_req bit on, but that doesn't
                ///  mean that the task is not ready.
                ///
                ///  @return
                ///   true - The task is launched, has no predecessors and it hasn't
                ///          been marked for cancellation. It might be in utcache.
                ///   false - The task fails any of the avobe conditions.
                bool is_ready(bool cancelable_task) const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_snapshot != value::uninitialized, "State hasn't been initialized yet.");
                    return (((_snapshot & ready_mask) == 0) || (!cancelable_task && (_snapshot & ready_mask_non_cancelable) == 0));
                }

                /// Checks whether the task is ready, running or finished.
                ///
                ///  @return
                ///   true - The task is ready, running, or finished.
                ///   false - The task is not yet ready.
                bool is_ready_running_or_finished(bool cancelable_task) const
                {
                    return _snapshot >= running || is_ready(cancelable_task);
                }

                /// Checks whether the task has been launched.
                ///
                ///  @return
                ///   true - The task has been launched, regardless of whether the
                ///          task has been marked for cancellation.
                ///   false - The task hasn't been launched yet.
                bool is_launched() const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_snapshot != value::uninitialized, "State hasn't been initialized yet.");
                    return ((_snapshot & unlaunched) == 0);
                }

                /// Checks whether the task is in the unlaunched task cache
                ///
                /// @return
                ///   true  - The task is in the unlaunched task cache.
                ///   false - The task is not in the unlaunched task cache.
                bool in_utcache() const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_snapshot != value::uninitialized, "State hasn't been initialized yet.");
                    return ((_snapshot & state_base::in_utcache) != 0);
                }

                /// Checks whether all the task arguments are bound
                ///
                /// @return
                ///   true  - All the arguments in the task are bound.
                ///   false - At least one argument in the task is not bound.
                bool is_bound() const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_snapshot != value::uninitialized, "State hasn't been initialized yet.");
                    return ((_snapshot & state_base::unbound) == 0);
                }

                /// Checks whether the task has started running.
                ///
                /// @return
                ///    true - The task is not running yet, regardless of
                ///           whether the task has been marked for cancellation.
                ///   false - The task is in "running" state or it has finished.
                bool is_not_running_yet() const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_snapshot != value::uninitialized, "State hasn't been initialized yet.");

                    return _snapshot < running;
                }

                /// Checks whether the task is locked.
                ///
                /// @return
                ///   true - The task is locked.
                ///   false - The task is not locked.
                bool is_locked() const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_snapshot != value::uninitialized, "State hasn't been initialized yet.");
                    return ((_snapshot & locked) == locked);
                }

                /// Checks whether the task has been marked for cancellation
                ///
                /// @return
                ///  true - The task has been marked for cancellation,
                ///        regardless of which state it is.
                ///  false - The task hasn't been marked for cancellation.
                bool has_cancel_request() const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_snapshot != value::uninitialized, "State hasn't been initialized yet.");
                    return ((_snapshot & cancel_req) != 0);
                }

                ///  Returns the number of task that must complete, cancel, or
                ///  except before the task can be executed.
                ///
                /// @return
                ///   Number of task predecessors.
                size_type get_num_predecessors() const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_snapshot != value::uninitialized, "State hasn't been initialized yet.");
                    return static_cast<size_type>((_snapshot & pred_cntr_mask) >> pred_cntr_disp);
                }

                /// Returns the number of references to the task.
                ///
                /// @return
                ///   Number of references.
                size_type get_num_references() const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_snapshot != value::uninitialized, "State hasn't been initialized yet.");
                    return static_cast<size_type>(_snapshot & ref_cntr_mask);
                }
            };  // class state_snapshot

            /// This class stores the state of the task.  It cannot be observed
            /// directly, it has to be done via state_snapshot.
            class state : public state_base
            {
            private:
                std::atomic<raw_type> _state;

                /// Returns description of the task that owns this state.
                ///
                /// @return
                ///   std::string with the desciption of the task that owns this state.
                std::string get_task_description(task*);

                friend class ::TaskStateSuite;
                friend class ::hetcompute::internal::testing::task_tests;
                friend class ::hetcompute::internal::task;

                enum initial_states : state_base::raw_type
                {
                    anonymous            = 1,
                    unlaunched           = state_base::unlaunched | 1,
                    unbound              = state_base::unlaunched | state_base::unbound | 1,
                    completed_with_value = state_base::completed | 1
                };

                friend class ::hetcompute::internal::task;

                /// Constructor. Only hetcompute::internal::task should construct a state.
                ///
                /// @param
                ///     raw_value Initial state.
                explicit state(raw_type raw_value) : _state(raw_value)
                {
                }

                /// Constructor. Only hetcompute::internal::task should construct a state.
                ///
                /// @param
                ///     raw_value Initial state.
                explicit state(state_snapshot snapshot) : _state(snapshot.get_raw_value())
                {
                }

                /// Sets the taks state to finished_state and acquires the task lock
                ///
                /// @param finished_state Desired end state, either "completed" or "canceled".
                /// @param extra_refs Indicates number of refcounts to add. Normally 0,
                ///        but trigger tasks need to add 1 because they are not
                ///        really launched
                ///
                /// @return
                ///   state_snapshot with the task state prior to locking it.
                state_snapshot set_finished_and_lock_impl(raw_type finished_state, ref_counter_type extra_refs)
                {
                    HETCOMPUTE_INTERNAL_ASSERT(finished_state == completed || finished_state == canceled, "Unknown finished task state.");

                    state_snapshot snapshot;
                    raw_type       desired;

                    while (true)
                    {
                        // Spin until we can get the lock
                        do
                        {
                            snapshot = get_snapshot(hetcompute::mem_order_relaxed);
                        } while (snapshot.is_locked() == true);

                        desired = finished_state | locked | (snapshot.get_num_references() + extra_refs);

                        // We expect that nobody is going to modify the state
                        raw_type expected = snapshot.get_raw_value();
                        if (atomic_compare_exchange_weak_explicit(&_state,
                                                                  &expected,
                                                                  desired,
                                                                  hetcompute::mem_order_acq_rel,
                                                                  hetcompute::mem_order_acquire))
                        {
                            return state_snapshot(expected);
                        }
                    }; // outer loop

                    HETCOMPUTE_UNREACHABLE("");
                }

                state_snapshot lock_and_optional_ref_impl(ref_counter_type extra_refs)
                {
                    state_snapshot snapshot;
                    raw_type       desired;

                    while (true)
                    {
                        // Spin until we can get the lock
                        do
                        {
                            snapshot = get_snapshot(std::memory_order_relaxed);
                        } while (snapshot.is_locked() == true);

                        auto raw_snapshot = snapshot.get_raw_value() & ~ref_cntr_mask;
                        desired           = raw_snapshot | locked | (snapshot.get_num_references() + extra_refs);

                        // We expect that nobody is going to modify the state
                        raw_type expected = snapshot.get_raw_value();
                        if (atomic_compare_exchange_weak_explicit(&_state, &expected, desired, std::memory_order_acq_rel, std::memory_order_acquire))
                        {
                            return state_snapshot(expected);
                        }
                    }; // outer loop

                    HETCOMPUTE_UNREACHABLE("");
                }

                /// Must be called with the lock held
                state_snapshot set_finished_and_unref_and_unlock(bool must_cancel, task* t)
                {
                    ref_counter_type ref_count_dec = 1;

                    raw_type finished_state = must_cancel ? canceled : completed;

                    HETCOMPUTE_INTERNAL_ASSERT(finished_state == completed || finished_state == canceled, "Unknown finished task state.");

                    while (true)
                    {
                        auto snapshot = get_snapshot(std::memory_order_relaxed);

                        HETCOMPUTE_INTERNAL_ASSERT(snapshot.is_locked(),
                                                 "Can't unlock task %p because it's not locked: %s",
                                                 t,
                                                 get_task_description(t).c_str());

                        // We expect that nobody is going to modify the state.
                        // However, ref_count may decrease. So try in a loop.
                        raw_type expected = snapshot.get_raw_value();

                        raw_type desired = (finished_state | (snapshot.get_num_references() - ref_count_dec)) & unlocked_mask;

                        if (atomic_compare_exchange_weak_explicit(&_state, &expected, desired, std::memory_order_acq_rel, std::memory_order_acquire))
                        {
                            return state_snapshot(expected);
                        }
                    }
                    HETCOMPUTE_UNREACHABLE("");
                    HETCOMPUTE_UNUSED(t);
                }

            public:
                typedef state_base::raw_type raw_type;

                /// Returns state.
                /// @param
                ///    mem_order Memory order to use when loading the state value.
                ///
                /// @return
                ///    state_snapshot object with the current task status.
                state_snapshot get_snapshot(hetcompute::mem_order order = hetcompute::mem_order_acquire) const
                {
                    return state_snapshot(_state.load(order));
                }

                /**
                 * Increases number of task predecessors by 1.
                 * Adding a predecessor also means that there is a reference from it, so this
                 * member function also increases the number of references.
                 *
                 * @param dynamic_dep set to true if a dynamic dependency (after task launch)
                 *                    is being created, false otherwise
                 */
                void add_predecessor(task* t, bool dynamic_dep = false)
                {
                    state_snapshot snapshot(get_snapshot(hetcompute::mem_order_relaxed));

                    while (true)
                    {
                        if (((dynamic_dep == false) && (snapshot.is_launched() == true)) || (snapshot.is_finished() == true))
                        {
                            HETCOMPUTE_FATAL("Can't add predecessors to task %p State: %s", t, get_task_description(t).c_str());
                        }

                        raw_type expected = snapshot.get_raw_value();
                        raw_type desired  = expected + one_pred;

                        if (atomic_compare_exchange_weak_explicit(&_state,
                                                                  &expected,
                                                                  desired,
                                                                  hetcompute::mem_order_acq_rel,
                                                                  hetcompute::mem_order_acquire))
                        {
                            if (snapshot.get_num_predecessors() == pred_cntr_max)
                            {
                                HETCOMPUTE_FATAL("Task has reached the maximum number of predecessors");
                            }
                            return;
                        }
                        // Expected contains the new state after the compare exchange, we
                        // used acquire for this load.
                        snapshot.reset(expected);
                    }
                    HETCOMPUTE_UNREACHABLE("");
                    HETCOMPUTE_UNUSED(t);
                }

                /// Decreases number of task predecessors by 1.
                /// Adding a predecessor also means that there is a reference from it,
                /// so this member function also increases the number of references.
                /// @param[out]
                ///   updated_state stores the task state after removing the predecessor.
                void remove_predecessor(state_snapshot& updated_state)
                {
                    updated_state.reset(_state.fetch_sub(one_pred, hetcompute::mem_order_acq_rel) - one_pred);
                }

                /// Marks the task as launched.
                ///
                /// @param[out]
                ///   snapshot Task state snapshot after transitioning to launched.
                /// @param[in]
                ///   increate_ref count. If true, also increase the task's refcount.
                ///
                /// @return
                ///   true  - The task transitioned from "unlaunched" to "launched". \n
                ///   false - The task was already launched.
                bool set_launched(state_snapshot& snapshot, bool increase_ref_count)
                {
                    ref_counter_type count = increase_ref_count ? 1 : 0;
                    snapshot               = get_snapshot(hetcompute::mem_order_relaxed);

                    while (true)
                    {
                        if ((snapshot.is_launched()) || (snapshot.is_bound() == false))
                        {
                            snapshot.reset(get_snapshot(hetcompute::mem_order_acquire).get_raw_value());
                            return false;
                        }

                        raw_type expected = snapshot.get_raw_value();
                        raw_type desired  = (expected & unlaunched_mask) + count;

                        if (atomic_compare_exchange_weak_explicit(&_state,
                                                                  &expected,
                                                                  desired,
                                                                  hetcompute::mem_order_acq_rel,
                                                                  hetcompute::mem_order_acquire))
                        {
                            snapshot.reset(desired);
                            return true;
                        }
                        snapshot.reset(expected);
                    }
                    HETCOMPUTE_UNREACHABLE("");
                }

                /// Sets the task state to "running". It won't succeed if someone
                ///  has requested the task to be "canceled".
                ///
                /// @return
                ///   true - The state changed from "ready" to "running".
                ///   false - The state did not change from "ready" to "running".
                bool set_running(bool is_anonymous, bool is_cancelable, task* t)
                {
                    HETCOMPUTE_INTERNAL_ASSERT(get_snapshot().get_num_predecessors() == 0,
                                             "Can't transition task %p to running because it has "
                                             "predecessors: %s.",
                                             t,
                                             get_task_description(t).c_str());

                    HETCOMPUTE_INTERNAL_ASSERT(get_snapshot().is_bound() == true,
                                             "Can't transition task %p to running because it isn't "
                                             "bound: %s.",
                                             t,
                                             get_task_description(t).c_str());

                    HETCOMPUTE_INTERNAL_ASSERT(get_snapshot().is_launched(),
                                             "Can't transition task %p to running because is not "
                                             "launched yet: %s.",
                                             t,
                                             get_task_description(t).c_str());

                    // Anonymous tasks can't have their in_utcache nor locked bits
                    // set. All we have to do is to set the state to "running". We can
                    // do it using relaxed operations because we will only read it
                    // from task::schedule() and from abort_on_cancel.
                    if (is_anonymous)
                    {
                        HETCOMPUTE_INTERNAL_ASSERT(get_snapshot().get_raw_value() == 1,
                                                 "Anonymous task %p has unexpected state: %s ",
                                                 t,
                                                 get_task_description(t).c_str());
                        _state.store(running_anonymous, hetcompute::mem_order_relaxed);
                        return true;
                    }

                    // Marking the task as "running" means setting the "running"
                    // bit. We can only transition to "running" if the cancel_req bit
                    // is not set (unless the task is non-cancelabe). Plus, we have to
                    // ignore the in_utcache and locked bits.
                    state_snapshot snapshot(get_snapshot(hetcompute::mem_order_relaxed));

                    while (true)
                    {
                        if (is_cancelable && snapshot.has_cancel_request())
                        {
                            return false;
                        }

                        raw_type expected = snapshot.get_raw_value() & (in_utcache | locked | ref_cntr_mask | cancel_req);
                        raw_type desired  = expected | running;

                        // We could have used hetcompute::mem_order_relaxed for fail. But
                        // we expect this compare_exchange to pretty much always succeed.
                        // We've looked at the assembly, and by using acquire we will have
                        // no branch in between ldrex and strex. (In the case we succeed,
                        // we need the barrier anyway. (See wiki)
                        if (atomic_compare_exchange_weak_explicit(&_state,
                                                                  &expected,
                                                                  desired,
                                                                  hetcompute::mem_order_acq_rel,
                                                                  hetcompute::mem_order_acquire))
                        {
                            return true;
                        }
                        // Expected contains the new state after the compare exchange, we
                        // used acquire for this load.
                        snapshot.reset(expected);
                    }

                    HETCOMPUTE_UNREACHABLE("");
                    HETCOMPUTE_UNUSED(t);
                }

                /**
                 * Change state from RUNNING to NOT RUNNING.
                 * @returns true
                 */
                bool reset_running()
                {
                    state_snapshot snapshot(get_snapshot(hetcompute::mem_order_relaxed));
                    while (true)
                    {
                        raw_type expected = snapshot.get_raw_value();
                        raw_type desired  = expected & running_mask;

                        if (atomic_compare_exchange_weak_explicit(&_state,
                                                                  &expected,
                                                                  desired,
                                                                  hetcompute::mem_order_acq_rel,
                                                                  hetcompute::mem_order_acquire))
                        {
                            return true;
                        }
                        // Expected contains the new state after the compare exchange (we
                        // used acquire for this load).
                        snapshot.reset(expected);
                    }

                    HETCOMPUTE_UNREACHABLE("");
                }

                /// Sets the task state to "canceled" and acquires the state lock.
                /// @return
                ///   state_snapshot with the task state prior to locking it.
                state_snapshot set_finished_and_lock(bool must_cancel, bool is_trigger)
                {
                    raw_type         finished_state = must_cancel ? canceled : completed;
                    ref_counter_type ref_count_inc  = is_trigger ? 1 : 0;
                    return set_finished_and_lock_impl(finished_state, ref_count_inc);
                }

                /// Acquires the state lock and optionally increments ref count.
                /// @param inc_ref_count If set, increment ref count
                /// @return
                ///   state_snapshot with the task state prior to locking it.
                state_snapshot lock_and_optional_ref(bool inc_ref_count)
                {
                    ref_counter_type ref_count_inc = inc_ref_count ? 1 : 0;
                    return lock_and_optional_ref_impl(ref_count_inc);
                }

                /// Marks the task as being in the unlaunched task cache.
                /// @return
                ///   Task state before the marking the task
                state_snapshot set_in_utcache(task* t)
                {
                    HETCOMPUTE_INTERNAL_ASSERT(get_snapshot().is_finished() == false,
                                             "Can't set utcache bit on task %p "
                                             "because it's done: %s",
                                             t,
                                             get_task_description(t).c_str());

                    HETCOMPUTE_UNUSED(t);
                    return state_snapshot(_state.fetch_or(in_utcache, hetcompute::mem_order_acq_rel));
                }

                ///  Marks the task as not being in the unlaunched task cache
                void reset_in_utcache()
                {
                    _state.fetch_and(in_utcache_mask, hetcompute::mem_order_acq_rel);
                }

                ///  Marks the task as being bound
                void set_bound(task* t)
                {
                    HETCOMPUTE_INTERNAL_ASSERT(get_snapshot().is_bound() == false,
                                             "Task %p is already bound: %s",
                                             t,
                                             get_task_description(t).c_str());

                    HETCOMPUTE_UNUSED(t);
                    _state.fetch_and(unbound_mask, hetcompute::mem_order_acq_rel);
                }

                /// Transitions tasks state to cancellation requested.
                ///
                /// @param[out] snapshot Snapshot of the task state after the
                ///  transition to cancellation requested.
                ///
                /// @return
                /// true  - The caller was the first one to request that the task
                ///         gets "canceled" and the task is not done yet.
                /// false - The caller wasn't the first one to request that the task
                ///         gets "canceled" or the task was done already.
                bool set_cancel_request(state_snapshot& snapshot)
                {
                    snapshot.reset(_state.fetch_or(cancel_req, hetcompute::mem_order_acq_rel));
                    if (snapshot.has_cancel_request() || snapshot.is_finished())
                        return false;

                    snapshot.reset(snapshot.get_raw_value() | cancel_req);
                    return true;
                }

                /// Locks the task. If someone else has already locked it, we spin
                /// until we can lock the task ourselves. A deadlock will occur
                /// if a thread that already locked the task calls this method
                /// again.
                ///
                /// @return
                ///    state_snapshot with the task state prior to locking it.
                state_snapshot lock()
                {
                    state_snapshot snapshot;

                    while (true)
                    {
                        // Spin until we can get the lock
                        do
                        {
                            snapshot = get_snapshot(hetcompute::mem_order_relaxed);
                        } while (snapshot.is_locked() == true);

                        // We expect that nobody is going to modify the state
                        raw_type expected = snapshot.get_raw_value();
                        raw_type desired  = expected | locked;

                        if (atomic_compare_exchange_weak_explicit(&_state,
                                                                  &expected,
                                                                  desired,
                                                                  hetcompute::mem_order_acq_rel,
                                                                  hetcompute::mem_order_acquire))
                        {
                            return state_snapshot(expected);
                        }
                    }; // outer loop

                    HETCOMPUTE_UNREACHABLE("");
                }

                /// Unlocks the task. We require that the same task that called
                /// lock() on the task calls unlock. The reason is that we are
                ///  using mem_order relaxed to load the current value.
                ///
                ///  @return
                ///   state_snapshot with the task state prior to unlocking it.
                state_snapshot unlock(task* t)
                {
                    state_snapshot snapshot(get_snapshot(hetcompute::mem_order_relaxed));

                    while (true)
                    {
                        HETCOMPUTE_INTERNAL_ASSERT(snapshot.is_locked(),
                                                 "Can't unlock task %p because it's not locked: %s",
                                                 t,
                                                 get_task_description(t).c_str());

                        raw_type expected = snapshot.get_raw_value();
                        raw_type desired  = expected & unlocked_mask;

                        if (atomic_compare_exchange_weak_explicit(&_state,
                                                                  &expected,
                                                                  desired,
                                                                  hetcompute::mem_order_acq_rel,
                                                                  hetcompute::mem_order_acquire))
                        {
                            return state_snapshot(desired);
                        }
                        snapshot.reset(expected);
                    }
                    HETCOMPUTE_UNREACHABLE("");
                    HETCOMPUTE_UNUSED(t);
                }

                /// Increases number of task references by 1.
                /// @return
                ///   state_snapshot with the task state prior to
                ///   increasing the number of task references
                state_snapshot ref()
                {
                    auto old_value = _state.fetch_add(one_ref, hetcompute::mem_order_acq_rel);
                    HETCOMPUTE_INTERNAL_ASSERT(old_value != ref_cntr_max, "Maximum number of references exceeded.");
                    return state_snapshot(old_value);
                }

                /// Decreases number of task references by 1.
                /// @return
                ///   state_snapshot with the task state prior to
                ///   decreasing the number of task references
                state_snapshot unref()
                {
                    auto old_value = _state.fetch_sub(one_ref, hetcompute::mem_order_acq_rel);
                    HETCOMPUTE_INTERNAL_ASSERT(old_value != 0, "Can't unref task with no references.");
                    return state_snapshot(old_value);
                }
            };  // class state

        };  // namespace task_internal
    };  // namespace internal
};  // namespace hetcompute
