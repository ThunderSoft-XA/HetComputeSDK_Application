#pragma once

#include <atomic>
#include <climits>
#include <hetcompute/internal/util/debug.hh>

// Forward declarations
class GroupStateSuite;

namespace hetcompute
{
    namespace internal
    {
        // Forward declaration
        class group;

        namespace group_misc
        {
            /// This class stores dynamic information about a group:
            ///  - Group Type (leaf or pfor types)
            ///  - Whether the group is canceled
            ///  - Whether the group is ref_counted
            ///  - Group id.
            class group_state_base
            {
            protected:
                typedef size_t raw_type;

                static constexpr raw_type raw_type_nbits = (sizeof(raw_type) * CHAR_BIT);
                static constexpr raw_type canceled       = raw_type(1) << (raw_type_nbits - 1);
                static constexpr raw_type locked         = canceled >> 1;
                static constexpr raw_type ref_counted    = locked >> 1;
                static constexpr raw_type leaf_type_flag = ref_counted >> 1;
                static constexpr raw_type pfor_type_flag = leaf_type_flag >> 1;

                static constexpr raw_type canceled_mask       = ~canceled;
                static constexpr raw_type locked_mask         = ~locked;
                static constexpr raw_type ref_counted_mask    = ~ref_counted;
                static constexpr raw_type leaf_type_flag_mask = ~leaf_type_flag;
                static constexpr raw_type pfor_type_flag_mask = ~pfor_type_flag;
                static constexpr raw_type unlocked_mask       = ~locked;

                static constexpr raw_type id_mask = ~(canceled | locked | ref_counted | leaf_type_flag | pfor_type_flag);

            public:
                // We reserve one id for uninit state
                static const raw_type MAX_ID = id_mask - 2;
            };  // class group_state_base


            /// This class allows you to inspect a group_state object.
            class group_state_snapshot : public group_state_base
            {
                /// Friend list who can call the constructor
                friend class group_state;
                friend class ::GroupStateSuite;

                /// Indicates that the group state object hasn't been initialized
                /// We need to add this value so that we don't use the ready state
                /// (all 0s) as the default value for the state.
                static const raw_type UNINITIALIZED_STATE = id_mask - 1;

                /// Acual state
                raw_type _state;

                /// Constructor. Only group_state should be able to build
                ///    a group_state_snapshot from a raw_type.
                ///
                ///  @param state: Initial state value
                constexpr /* implicit */ group_state_snapshot(raw_type state) : _state(state) {}

            public:
                /// Unsigned integral type
                typedef size_t size_type;

                /// Default constructor
                group_state_snapshot() : _state(UNINITIALIZED_STATE) {}

                /// Returns the state raw_value.
                ///
                ///  @return
                ///     raw_type with the value of the state.
                constexpr raw_type get_raw_value() const { return _state; }

                /// Returns a string with the description of the state.
                ///
                ///  @return
                ///  std::string with the description of the state.
                std::string to_string() const { return ""; };

                ///  Checks whether the group is leaf.
                ///
                ///  @return
                ///    true - The group type is LEAF.
                ///    false - otherwise
                bool is_leaf() const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_state != UNINITIALIZED_STATE, "State hasn't been initialized yet.");

                    return ((_state & leaf_type_flag) != 0);
                }

                ///  Checks whether the group is pfor.
                ///
                ///  @return
                ///    true - The group type is PFOR.
                ///    false - otherwise
                bool is_pfor() const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_state != UNINITIALIZED_STATE, "State hasn't been initialized yet.");

                    return ((_state & pfor_type_flag) != 0);
                }

                ///  Checks whether the group is ref counted.
                ///
                ///  @return
                ///    true - The group type is ref counted.
                ///    false - otherwise
                bool is_ref_counted() const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_state != UNINITIALIZED_STATE, "State hasn't been initialized yet.");

                    return ((_state & ref_counted) == ref_counted);
                }

                ///  Checks whether the group has been canceled.
                ///
                ///  @return
                ///    true - The state is CANCELED.
                ///    false - The state is other than CANCELED.
                bool is_canceled() const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_state != UNINITIALIZED_STATE, "State hasn't been initialized yet");

                    return ((_state & canceled) == canceled);
                }

                ///  Checks whether the group is locked.
                ///
                ///  @return
                ///    true - The group is locked.
                ///    false - The group is not locked
                bool is_locked() const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_state != UNINITIALIZED_STATE, "State hasn't been initialized yet.");
                    return ((_state & locked) == locked);
                }

                /// Returns the group id.
                ///
                ///   @return
                ///     Group id.
                size_type get_id() const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_state != UNINITIALIZED_STATE, "State hasn't been initialized yet.");
                    return static_cast<size_type>(_state & id_mask);
                }
            }; // class group_state_snapshot

            /// This class stores the state of the class.  It cannot be observed
            /// directly, it has to be done via group_state_snapshot.**/
            class group_state : public group_state_base
            {
                std::atomic<raw_type> _state;

                friend class hetcompute::internal::group;
                friend class ::GroupStateSuite;

                /// Constructor. Only group can construct a group_state.
                ///
                /// @param
                /// state Initial state.
                explicit group_state(raw_type state) : _state(state) {}

            public:
                typedef group_state_base::raw_type raw_type;

                static raw_type leaf_pfor_type() { return pfor_type_flag | leaf_type_flag; };

                static raw_type meet_pfor_type() { return pfor_type_flag; };

                static raw_type leaf_type() { return leaf_type_flag; };

                /// Returns state.
                ///
                /// @param[in]
                ///    mem_order Memory order to use when loading the state value.
                ///
                /// @return
                ///   group_state_snapshot object with the current group status.
                group_state_snapshot get_snapshot(hetcompute::mem_order order = hetcompute::mem_order_relaxed) const
                {
                    return group_state_snapshot(_state.load(order));
                }

                /// Marks the group as canceled
                ///
                ///  @param
                ///      state Group state after the transition to cancel.
                ///
                ///  @return
                ///     true  - The group transitioned to cancel. \n
                ///     false - The group was already launched.
                bool set_cancel(group_state_snapshot& state)
                {
                    if (state.is_canceled())
                    {
                        return false;
                    }

                    state = _state.fetch_or(canceled, hetcompute::mem_order_acq_rel);

                    // Making sure that this only returns true when CANCEL gets set
                    if (state.is_canceled())
                    {
                        return false;
                    }
                    state = state.get_raw_value() | canceled;
                    return true;
                }

                /// Locks the group. If someone else has already locked it, we spin
                /// until we can lock the task ourselves. A deadlock will occur
                /// if a thread that already locked the task calls this method
                /// again.
                ///
                /// @return
                ///    task_state_snapshot with the task state prior to locking it.
                group_state_snapshot lock()
                {
                    group_state_snapshot prior_state;

                    while (true)
                    {
                        do
                        {
                            prior_state = get_snapshot(hetcompute::mem_order_relaxed);
                        } while (prior_state.is_locked() == true);

                        raw_type expected = prior_state.get_raw_value();
                        raw_type desired  = expected | locked;

                        if (atomic_compare_exchange_weak_explicit(&_state,
                                                                  &expected,
                                                                  desired,
                                                                  hetcompute::mem_order_acq_rel,
                                                                  hetcompute::mem_order_acquire))
                        {
                            return expected;
                        }
                    }; // outer loop

                    HETCOMPUTE_UNREACHABLE("lock returns only if successful");
                }

                /// Unlocks the group. We require that the same task that called
                /// lock() on the task calls unlock. The reason is that we are
                /// using mem_order relaxed to load the current value.
                ///
                /// @return
                ///   task_state_snapshot with the task state prior to unlocking it. */
                group_state_snapshot unlock()
                {
                    group_state_snapshot state(get_snapshot(hetcompute::mem_order_relaxed));

                    while (true)
                    {
                        HETCOMPUTE_INTERNAL_ASSERT(state.is_locked(), "Can't unlock group because it's not locked");

                        raw_type expected = state.get_raw_value();
                        raw_type desired  = expected & unlocked_mask;

                        if (atomic_compare_exchange_weak_explicit(&_state,
                                                                  &expected,
                                                                  desired,
                                                                  hetcompute::mem_order_acq_rel,
                                                                  hetcompute::mem_order_acquire))
                        {
                            return true;
                        }
                        state = expected;
                    }
                    HETCOMPUTE_UNREACHABLE("lock returns only if successful");
                }
            }; // class group_state

            /// An std::lock_guard-like structure that locks the group when it is
            /// created and releases the group lock when it is destroyed.
            class lock_guard
            {
                group_state& _state;

            public:
                /// Constructor. Locks the group.
                /// @param group Reference to the group state.
                inline explicit lock_guard(group_state& state) : _state(state) { _state.lock(); }

                /// Destructor. Unlocks the group
                inline ~lock_guard() { _state.unlock(); }
            };

        };  // namespace group_misc
    };  // namespace internal
};  // namespace hetcompute
