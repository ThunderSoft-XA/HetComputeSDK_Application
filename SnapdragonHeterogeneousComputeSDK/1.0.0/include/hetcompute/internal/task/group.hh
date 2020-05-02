#pragma once

#include <atomic>
#include <forward_list>

#include <hetcompute/internal/log/log.hh>
#include <hetcompute/internal/memalloc/groupallocator.hh>
#include <hetcompute/internal/task/exception_state.hh>
#include <hetcompute/internal/task/group_signature.hh>
#include <hetcompute/internal/task/group_state.hh>
#include <hetcompute/internal/task/group_misc.hh>
#include <hetcompute/internal/task/group_shared_ptr.hh>
#include <hetcompute/internal/task/lattice.hh>
#include <hetcompute/internal/task/task.hh>
#include <hetcompute/internal/util/debug.hh>

namespace hetcompute
{
    namespace internal
    {
        class group_factory;

        namespace testing
        {
            class group_tester;
        };  // namespace testing

        class group;

        /// The following struct takes care of logging the
        /// refcount events for groups
        struct group_refcount_logger
        {
            /// Logs ref event.
            /// @param g Group whose ref counted was increased.
            /// @param count Group's new ref count value.
            static void ref(group* g, ref_counter_type count)
            {
                log::fire_event<log::events::group_reffed>(g, count);
            }

            /// Logs unref event
            /// @param g Group whose ref counted was decreased.
            /// @param count Group's new ref count value.
            static void unref(group* g, ref_counter_type count) { log::fire_event<log::events::group_unreffed>(g, count); }

        }; // group_refcount_logger

        struct group_release_manager
        {
            /// Deletes tasks t.
            /// @param g: group to release.
            static void release(group* g);
        };

        /// This is the base class group. It includes everything
        /// all different group types have including
        /// _state
        /// _trigger_tas:
        /// _lattice node
        /// _tasks counter
        /// _representative_task
        /// The group class hierarchy is as follows:
        ///            group
        ///         |          |
        ///        leaf       meet
        ///         |
        ///    named_leaf
        ///
        class group : public hetcompute::internal::ref_counted_object<group, group_refcount_logger, group_release_manager>
        {
        public:
            using self_ptr = void*;

        private:
            self_ptr _self;
            using exception_state_ptr = hetcompute::internal::exception_state::exception_state_ptr;

        protected:
            /// State includes type, id and trigger task lock bit
            group_misc::group_state _state;

            /// Number of tasks in the group
            std::atomic<size_t> _tasks;

            /// Task launched when _tasks reaches 0, and lock protecting it
            task_shared_ptr _trigger_task;

            /// Task that acts as an alternative handle to the group.
            /// Canceling/waiting-for/etc. the task is akin to canceling/waiting-for/etc.
            /// the group. Used to implement asynchronous pattern APIs.
            /// The _trigger_task cannot be used as the _representative_task because:
            /// (1) The _trigger_task is launched implicitly only when a group finishes,
            ///     whereas we want the _representative_task to be something that can be
            ///     explicitly launched by the programmer.
            /// (2) The launch of internally created tasks into the group must be
            ///     deferred until the externally exposed _representative_task is ready
            ///     to execute. Therefore, the trigger task would essentially need to
            ///     have a body that launches the internally created task, participate in
            ///     dependencies, etc., thereby making it a full-fledged task. It would
            ///     be preferable to keep the common case use of groups lightweight
            ///     through the use of a special _trigger_task as currently exists.
            /// See internal documentation for further details.
            task* _representative_task;

            exception_state_ptr _exception_state;

            /// Lattice node for this group
            /// All assignments to lattice node happens at group creation time (meets)
            /// or are protected by the lattice lock (leaves). So no need to make this
            /// pointer atomic.
            group_misc::lattice_node* _lattice_node;

            /// log_id of the group
            const log::group_id _log_id;

            /// Returns the list of parents (ancestors) of the group
            ///
            /// @return group parents in the lattice
            group_misc::lattice_node::parent_list& parents() const { return get_lattice_node()->get_parents(); };

            /// Returns the list of children of the group
            ///
            /// @return group children in the lattice
            group_misc::lattice_node::children_list& children() const { return get_lattice_node()->get_children(); };

        protected:
            /// Constructor for leaf groups
            ///
            /// @param id, leaf group id
            /// @param type, type of leaf group (leaf_pfor or just leaf)
            group(group_misc::id::type id, group_misc::group_state::raw_type type)
                : _self(this),
                  _state(type | id), // For leaves id is also stored in the state
                  _tasks(0),
                  _trigger_task(nullptr),
                  _representative_task(nullptr),
                  _exception_state(nullptr),
                  _lattice_node(nullptr),
                  _log_id()
            {
                log::fire_event<log::events::group_created>(this);

                HETCOMPUTE_INTERNAL_ASSERT(id != group_misc::id_generator::default_meet_id, "Too many alive leaf groups: %zu", id);
            }

            /// Constructor for non-pfor meet groups
            group()
                : _self(this),
                  _state(0),
                  _tasks(0),
                  _trigger_task(nullptr),
                  _representative_task(nullptr),
                  _exception_state(nullptr),
                  _lattice_node(nullptr),
                  _log_id()
            {
                log::fire_event<log::events::group_created>(this);
            }

            /// Constructor for pfor meet groups
            ///
            /// @param type, type of leaf group (meet_pfor or just meet)
            explicit group(group_misc::group_state::raw_type type)
                : _self(this),
                  _state(type),
                  _tasks(0),
                  _trigger_task(nullptr),
                  _representative_task(nullptr),
                  _exception_state(nullptr),
                  _lattice_node(nullptr),
                  _log_id()
            {
                log::fire_event<log::events::group_created>(this);
            }

            // Disable all copying and movement, since other objects may have
            // pointers to the internal fields of this object.
            HETCOMPUTE_DELETE_METHOD(group(group const&));
            HETCOMPUTE_DELETE_METHOD(group(group&&));
            HETCOMPUTE_DELETE_METHOD(group& operator=(group const&));
            HETCOMPUTE_DELETE_METHOD(group& operator=(group&&));

        public:
            /// Returns the snapshot of the current state of the group
            ///
            /// @return group state snapshot
            inline group_misc::group_state_snapshot get_state() const { return _state.get_snapshot(); };

            /// Returns the log id of the group object.
            ///
            /// @return log::group_id
            const log::group_id get_log_id() const { return _log_id; }

            /// Returns the empty string
            ///
            /// @return const reference to the name of the group.
            virtual std::string const get_name() const { return std::string(""); }

            /// Describes group. Useful for debugging.
            /// @return String that describes the group info.
            std::string to_string(std::string const& msg = "") const;

            hc_error wait();

            /// If the group is not empty it creates and returns the trigger task.
            /// The wait for will use that pointer to wait.
            /// @param succ task_ptr to a successor to the trigger task. May be nullptr.
            /// @return task_ptr for waiting on until the next time the group
            ///     becomes empty.  Returns nullptr if group is already empty.
            task_shared_ptr ensure_trigger_task_unless_empty(task* succ = nullptr);

            /// Set the representative task of the group
            /// @param t naked pointer to task that will be the representative of this
            ///          group. May be nullptr.
            void set_representative_task(task* t)
            {
                HETCOMPUTE_INTERNAL_ASSERT(t->get_current_group() != this,
                                         "Representative task "
                                         "cannot be part of the group. Otherwise, we will end up in a vicious "
                                         "cycle while checking whether the representative/group is canceled.");
                _representative_task = t;
            }

            group_misc::lock_guard get_lock_guard() { return group_misc::lock_guard(_state); }

            /**
             * Set exception eptr in group
             * @param eptr pointer to exception to set
             */
            void set_exception(std::exception_ptr& eptr)
            {
                HETCOMPUTE_INTERNAL_ASSERT(eptr != nullptr, "cannot set empty exception");
                auto guard = get_lock_guard();
                if (_exception_state == nullptr)
                {
                    _exception_state = exception_state_ptr(new exception_state());
                }
                _exception_state->add(eptr);
            }

            /**
             * Propagate exception state from eptr (from some task belonging to the group)
             * @param eptr exception state to propagate
             */
            void propagate_exception_from(exception_state_ptr& eptr)
            {
                HETCOMPUTE_INTERNAL_ASSERT(eptr != nullptr, "cannot set empty exception");
                auto guard = get_lock_guard();
                if (_exception_state == nullptr)
                {
                    _exception_state = exception_state_ptr(new exception_state());
                }
                _exception_state->propagate(eptr);
            }

            /// Cancels all tasks in the group and prevents any new tasks from
            /// being added to it. There might be tasks in the group running after
            /// cancel() returns. Eventually, all tasks will get removed from
            /// the group. In order to make sure that the group is empty, you
            /// must call group::wait() after group::cancel().
            void cancel();

            /// Cancels task if it belongs to this group.
            ///
            /// @param task, task to cancel
            /// @return true of task was canceled, false otherwise
            bool cancel_if_group_member(task* t)
            {
                HETCOMPUTE_INTERNAL_ASSERT(t,
                                         "invalid task pointer checked for group "
                                         "membership in %p",
                                         this);

                // We can use relaxed here because of the barrier in the
                // lock in cancel_blocking_tasks and cancel_blocking_deferred_tasks.
                auto g = t->get_current_group(hetcompute::mem_order_relaxed);
                if (g && this->is_ancestor_of(g))
                {
                    t->cancel();
                    return true;
                }
                return false;
            }

            /// Returns true if the group is canceled.
            ///
            /// @return true if the group is canceled. Otherwise, false.
            bool is_canceled(hetcompute::mem_order mem_order = hetcompute::mem_order_acquire) const
            {
                HETCOMPUTE_UNUSED(mem_order);
                return get_state().is_canceled() || ((_representative_task != nullptr) && _representative_task->is_canceled(true));
            }

            /// Returns true if the group contains one or more tasks that have thrown an
            /// exception
            ///
            /// @return true if group is exceptional, otherwise false
            bool is_exceptional() { return _exception_state != nullptr; }

            /// Checks whether the group contains a task that has thrown an exception;
            /// if so, it rethrows that exception
            void check_and_handle_exceptions()
            {
                if (is_exceptional())
                {
                    _exception_state->rethrow();
                    HETCOMPUTE_UNREACHABLE("exception should have been thrown");
                }
            }

            /// Takes note of the fact that a task in the group was canceled
            inline void task_canceled()
            {
                auto eptr = std::make_exception_ptr(hetcompute::canceled_exception());
                set_exception(eptr);
            }

            /// Takes note of the fact that a task in the group was canceled due to an
            /// exception
            ///
            /// @param eptr exception pointer thrown by task
            inline void task_canceled(std::exception_ptr& eptr)
            {
                HETCOMPUTE_INTERNAL_ASSERT(eptr != nullptr, "eptr cannot be nullptr");
                set_exception(eptr);
            }

            /// Returns the bitmap corresponding to the group's index in the cache
            ///
            /// @return Bitmap corresponding to the group's index in the cache
            group_misc::group_signature& get_signature() const { return get_lattice_node()->get_signature(); }

            /// Returns the number of ones in the group's bitmap
            size_t get_order() const { return get_lattice_node()->get_order(); }

            bool is_empty(hetcompute::mem_order mem_order = hetcompute::mem_order_seq_cst) const
            {
                auto num_tasks = _tasks.load(mem_order);
                return (num_tasks == 0);
            }

            /// Returns true if the node has a lattice node constructed
            /// Leaves with children or meets groups will only have constructed
            /// lattice nodes
            bool has_lattice_node() const { return (get_lattice_node() != nullptr); };

            group_misc::lattice_node* get_lattice_node() const { return _lattice_node; };

            /// Increases the task counter in g by 1. If it's the first task added
            /// to the group, then we also increase the reference counting to simulate
            /// that the runtime has a shared pointer to it.
            ///
            /// @param lattice_lock_policy, do_not_acquire for the recursive calls
            inline void inc_task_counter(group_misc::lattice_lock_policy p = group_misc::acquire_lattice_lock);

            /// Decreases the task counoter in g by 1. If the counter reaches 0,
            /// the shared_ptr to self in the group is reset.
            ///
            /// @param lattice_lock_policy, do_not_acquire for the recursive calls
            inline void dec_task_counter(group_misc::lattice_lock_policy p = group_misc::acquire_lattice_lock);

            /// Checks whether this group is an ancestor of group g
            /// Group g1 is an ancestor of group g2 if g2's bitmap is a
            /// superset of g1's bitmap. Notice that any group is an ancestor
            /// of itself
            ///
            /// @param g, the group for which we want to check is an ancestor of
            bool is_ancestor_of(group* g)
            {
                HETCOMPUTE_INTERNAL_ASSERT(g, "nullptr group");
                // Optimization 1: equal groups are ancestor!
                if (this == g)
                    return true;
                // Optimization 2: if the group is not in lattice it cannot be an ancestor!
                if (!has_lattice_node() || !g->has_lattice_node())
                    return false;
                // Slowpath: A is ancestor of if and only if
                // A.bitmap is a subset of B.bitmap
                return (g->get_signature().subset(get_signature()));
            }

            /// Checks whether this group is an strict ancestor of group g
            /// Group g1 is an ancestor of group g2 if g2's bitmap is a
            /// superset of g1's bitmap and g1 is different than g2.
            ///
            /// @param g, the group for which we want to check is an ancestor of
            bool is_strict_ancestor_of(group* g)
            {
                HETCOMPUTE_INTERNAL_ASSERT(g, "nullptr group");
                return ((this != g) && is_ancestor_of(g));
            }

            /// Checks whether the group is a leaf
            ///
            /// @return true this group is a leaf
            bool is_leaf() const { return get_state().is_leaf(); }

            /// Creates lattice_node for a leave group
            ///
            /// @return true if the lattice node is created successfully
            bool create_lattice_node_leaf()
            {
                if (get_lattice_node() != nullptr)
                    return false;
                _lattice_node = new group_misc::lattice_node_leaf(get_id());
                return true;
            }

            /// Checks whether the group is a pfor
            ///
            /// @return true this group is a pfor
            bool is_pfor() const { return get_state().is_pfor(); }

            /// Destructor
            ///
            /// This destructor is empty (essential for pfor performance).
            /// Except for sanity checks in debug mode.
            virtual ~group()
            {
                log::fire_event<log::events::group_destroyed>(this);

                // The group should not have task
                HETCOMPUTE_INTERNAL_ASSERT(is_empty() == true, "Non-empty group: %s", to_string().c_str());

                // It's ref count must be zero
                // HETCOMPUTE_INTERNAL_ASSERT(use_count() == 0, "there are references to group" );
                auto xxx = use_count();
                if (xxx != 0)
                {
                    HETCOMPUTE_ILOG("%d %s", xxx, to_string().c_str());
                    // hetcompute_breakpoint();
                }

// Trigger task must be null
#ifdef HETCOMPUTE_CHECK_INTERNAL
                _state.lock();
                HETCOMPUTE_INTERNAL_ASSERT(!_trigger_task, "trigger task should be nullptr");
                _state.unlock();
#endif

                // Proper releasing of leaf lattice node
                // lattice node for leaf nodes is dynamically allocated
                // but for meet nodes it is statically generated as an inlined member
                if (_lattice_node != nullptr && is_leaf())
                {
                    delete _lattice_node;
                }
            }

            // gcc is very aggressive in trying to detect alias violations
            HETCOMPUTE_GCC_IGNORE_BEGIN("-Wstrict-aliasing")

            template <typename Facade>
            Facade* get_facade()
            {
                static_assert(sizeof(Facade) == sizeof(void*), "Can't allocate Facade.");
                return reinterpret_cast<Facade*>(&_self);
            }

            HETCOMPUTE_GCC_IGNORE_END("-Wstrict-aliasing")

        protected:
            /// Returns the group id after reading it from state
            ///
            /// @return id associated with the leaf group
            group_misc::id::type get_id() const { return _state.get_snapshot().get_id(); }

        private:
            /// After increasing the task counter by one, if the group is a meet,
            /// it grabs the lattice lock and propagates the counter increase to
            /// its ancestors.
            ///
            /// @param lattice_lock_policy, do_not_acquire for the recursive calls
            void inc_task_counter_meet(group_misc::lattice_lock_policy p = group_misc::acquire_lattice_lock);

            /// After increasing the task counter by one, if the group is a meet,
            /// it grabs the lattice lock and propagates the counter increase to
            /// its ancestors.
            ///
            /// @param lattice_lock_policy, do_not_acquire for the recursive calls
            void dec_task_counter_meet(group_misc::lattice_lock_policy p = group_misc::acquire_lattice_lock);

            /// Propagates cancellation across group descendants in the lattice
            void propagate_cancellation()
            {
                group_misc::group_state_snapshot state = get_state();
                if (_state.set_cancel(state))
                    if (has_lattice_node())
                        for (auto& child : children())
                            child->propagate_cancellation();
            }

            /// Update the migration of a task to this from an origin group
            void migrate_task_from(group* origin)
            {
                // The flag will be true if the origin is one of the ancestor
                // of this group
                bool propagated_to_origin = false;
                propagate_task_migration(origin, propagated_to_origin);

                // Decrease origin's task counter if not an ancestor
                if (!propagated_to_origin)
                    origin->dec_task_counter(group_misc::do_not_acquire_lattice_lock);
            }

            /// Propagates task increment across group ancestors
            void propagate_inc_task_counter()
            {
                HETCOMPUTE_INTERNAL_ASSERT(!is_leaf(),
                                         "Can't propagate task increase in leaf "
                                         "group: %s",
                                         to_string().c_str());
                HETCOMPUTE_INTERNAL_ASSERT(!get_lattice_node()->get_parent_sees_tasks(),
                                         "Can't propagate task increase "
                                         "in group: %s",
                                         to_string().c_str());
                get_lattice_node()->set_parent_sees_tasks(true);
                for (auto& parent : parents())
                    c_ptr(parent)->inc_task_counter(group_misc::do_not_acquire_lattice_lock);
            }

            /// Propagates task decrements across group ancestors
            void propagate_dec_task_counter()
            {
                HETCOMPUTE_INTERNAL_ASSERT(get_lattice_node()->get_parent_sees_tasks(),
                                         "Can't propagate task decrease "
                                         "in group: %s",
                                         to_string().c_str());
                get_lattice_node()->set_parent_sees_tasks(false);
                for (auto& parent : parents())
                    c_ptr(parent)->dec_task_counter(group_misc::do_not_acquire_lattice_lock);
            }

            /// Increases the task counter in g by 1. If it's the first task added
            /// to the group, then we also increase the reference counting to simulate
            /// that the runtime has a shared pointer to it. If the group is not a leaf,
            /// it propagates the counter increase to its ancestors without grabing the
            /// lattice lock. It'll propagate it only until it finds the origin group.
            void propagate_task_migration(group* origin, bool& propgte_to_origin);

            static std::string get_meet_name();

            /// Resets trigger task and returns it
            inline task_shared_ptr reset_trigger_task()
            {
                _state.lock();
                // Making sure a trigger task is only acquired when the group lock is
                // taken and the group is empty.
                // Although the task counter is checked in dec_task_counter before calling
                // launch_trigger task, it could be that competing tasks can be injected
                // into the group and this can lead to early launch of trigger task
                // before wait_for is reached, which can lead to pre-mature return of
                // wait_for! This is a common pattern in pfor_each codes.
                if (!is_empty())
                {
                    _state.unlock();
                    return nullptr;
                }
                task_shared_ptr t = std::move(_trigger_task);
                HETCOMPUTE_INTERNAL_ASSERT(!_trigger_task, "std::move() did not reset group trigger task to nullptr");
                _state.unlock();
                return t;
            }

            /// Resets and cleans up trigger task (puting it in complete state)
            /// when task counter transition from 1 to 0 is observed
            void launch_trigger_task()
            {
                auto t = reset_trigger_task();
                // Using this idiom to make Klockwork happy
                auto t_ptr = c_ptr(t);
                if (t_ptr != nullptr)
                {
                    // Use mem_order_relaxed since reset_trigger_task has the necessary
                    // barrier
                    t_ptr->execute_trigger_task(is_canceled(hetcompute::mem_order_relaxed));
                }
            }

        public:
            /// Returns the number of created leaves
            static size_t get_num_leaves() { return group_misc::id_generator::get_num_leaves(); };

            typedef testing::group_tester tester;

            friend class group_misc::lattice;
            friend class group_factory;
            friend struct group_release_manager;
            friend class testing::group_tester;
        };  //  class group

        /// This is the class representing the leaf groups
        /// The only difference with group is cleaning up dense bitmap in desteructor
        class leaf : public group
        {
        public:
            /// Constructor
            /// Calling the original leaf constructor
            leaf(group_misc::id::type id, group_misc::group_state::raw_type type) : group(id, type){};
        };  // class leaf

        /// This is the class representing the named leaf groups
        /// The only difference with group is additional name field
        class named_leaf : public leaf
        {
        public:
            /// Constructor
            /// This can only be called for leaves
            ///
            /// @param name, name of the leaf
            /// @id id of the leaf
            named_leaf(const char* name, group_misc::id::type id) : leaf(id, group_misc::group_state::leaf_type()), _name(name) {}

            /// Returns the group name
            virtual std::string const get_name() const { return _name; }

        private:
            std::string _name;
        };  // class named_leaf

        /// This is the class representing meet groups
        /// The lattice node is allocated an inlined member
        class meet : public group
        {
        public:
            /// Constructor
            ///
            /// @param signature, group signature (bitmap)
            /// @param order, group order in the lattice
            /// @param db, group db owner in the lattice (the meetdb of a leaf ancestor
            /// with smallest id)
            meet(group_misc::group_signature& signature, size_t order, group_misc::meet_db* db)
                : group(), _inlined_lattice_node(signature, order, db)
            {
                _lattice_node = &_inlined_lattice_node;
            }
            /// Constructor
            /// Currently used only for pfor meets
            /// pfor meets do not use signature because different from
            /// normal meets they do not have two parents. They are tied to
            /// only one parent and so their bitmap can be considered identical
            /// to their only parent. Additionally, they are fully managed by the
            /// patterns and not exposed to the user. So they do not need to be stored
            /// in meet db and so they do not need the bitmap to be used as the DB key.
            ///
            /// @param type, group type (meet pfor or meet)
            /// @param order, group order in the lattice
            /// @param db, group db owner in the lattice (the meetdb of a leaf ancestor
            /// with smallest id)
            meet(group_misc::group_state::raw_type type, size_t order, group_misc::meet_db* db)
                : group(type), _inlined_lattice_node(order, db)
            {
                _lattice_node = &_inlined_lattice_node;
            }
            virtual ~meet();

        private:
            group_misc::lattice_node _inlined_lattice_node;
        };  // class meet

        /// This class manages all group constructions (and intersections) for
        /// different group types
        class group_factory
        {
        public:
            typedef allocatorinternal::group_allocator<sizeof(named_leaf)> allocator_t;

            /// Creates a leaf group using an input bitmap
            ///
            /// @param name, of the new group
            /// @return the new group
            static group* create_leaf_group(std::string const& name)
            {
                auto id = group_misc::id_generator::get_leaf_id();
#if !defined(HETCOMPUTE_NO_GROUP_ALLOCATOR)
                char* buffer = s_allocator.allocate_leaf(id);
                if (buffer != nullptr)
                {
                    return new (buffer) named_leaf(name.c_str(), id);
                }
                else
                {
                    return new named_leaf(name.c_str(), id);
                }
#else
                return new named_leaf(name.c_str(), id);
#endif // !defined(HETCOMPUTE_NO_GROUP_ALLOCATOR)
            }

            /// Creates a leaf group using an input bitmap
            ///
            /// @param name, of the new group
            /// @return the new group
            static group* create_leaf_group(const char* name)
            {
                auto id = group_misc::id_generator::get_leaf_id();
#if !defined(HETCOMPUTE_NO_GROUP_ALLOCATOR)
                char* buffer = s_allocator.allocate_leaf(id);
                if (buffer != nullptr)
                {
                    return new (buffer) named_leaf(name, id);
                }
                else
                {
                    return new named_leaf(name, id);
                }
#else
                return new named_leaf(name, id);
#endif // !defined(HETCOMPUTE_NO_GROUP_ALLOCATOR)
            }

            /// Creates a nameless leaf group
            ///
            /// @return the new leaf group
            static group* create_leaf_group()
            {
                auto id = group_misc::id_generator::get_leaf_id();
#if !defined(HETCOMPUTE_NO_GROUP_ALLOCATOR)
                char* buffer = s_allocator.allocate_leaf(id);
                if (buffer != nullptr)
                {
                    return new (buffer) leaf(id, group_misc::group_state::leaf_type());
                }
                else
                {
                    return new leaf(id, group_misc::group_state::leaf_type());
                }
#else
                return new leaf(id, group_misc::group_state::leaf_type());
#endif // !defined(HETCOMPUTE_NO_GROUP_ALLOCATOR)
            }

            /// Creates a meet group using an input bitmap.
            ///
            /// @param name, of the new group
            /// @return the new group
            static group* create_meet_group(group_misc::group_signature& signature, group_misc::meet_db* db)
            {
                HETCOMPUTE_INTERNAL_ASSERT(db != nullptr, "DB for a meet group must not be empty");
                // Read order before creating group
                size_t order = signature.popcount();
                group* g     = new meet(signature, order, db);
                return g;
            }

            /// Creates a pfor group as a children of the input group and
            ///
            /// @param parent_group, the group under whcih we want to create this pofor
            /// @return a pfor group_ptr to it.
            static group* create_pfor_group(group* const& parent_group)
            {
                group* g = nullptr;
                if (parent_group == nullptr)
                {
                    g = new group(group_misc::group_state::leaf_pfor_type());
                    return g;
                }
                if (!parent_group->has_lattice_node())
                {
                    // Guaranteed parent is a leaf
                    parent_group->create_lattice_node_leaf();
                }
                g = new meet(group_misc::group_state::meet_pfor_type(),
                             parent_group->get_order() + 1,
                             parent_group->get_lattice_node()->get_meet_db());
                g->parents().push_front(group_shared_ptr(parent_group));
                // we have to acquire the lock because we are changing the lattice
                std::lock_guard<group_misc::lattice::lock_type> lattice_lock(group_misc::lattice::get_mutex());
                parent_group->children().push_front(g);
                return g;
            }

            /// Use the lattice to intersect a and b and
            /// also moves the tasks from a possible donor group to new created one.
            ///
            /// @param a, first group to be intersected
            /// @param b, first group to be intersected
            /// @param current_group, the donor group that
            /// its tasks will move to the intersection group
            /// @return the new or found meet node
            static group_shared_ptr merge_groups(group* a, group* b, group* current_group = nullptr)
            {
                return group_misc::lattice::create_meet_node(a, b, current_group);
            }

            static size_t get_leaf_pool_size() { return allocator_t::get_leaf_pool_size(); }

        private:
            static allocator_t s_allocator;
        };  // class group_factory

        inline void group::inc_task_counter(group_misc::lattice_lock_policy lock_policy)
        {
            if (is_leaf())
            {
                if (_tasks.fetch_add(1, hetcompute::mem_order_seq_cst) != 0)
                {
                    // No 0->1 transition happened
                    return;
                }
                explicit_ref(this);
                // To make sure inc/dec ordering of tasks is always observed!
                // If a task is just injected an not executed yet, task counter will
                // never be zero
                HETCOMPUTE_INTERNAL_ASSERT(!is_empty(), "Group must not be empty");
                return;
            }
            inc_task_counter_meet(lock_policy);
        }

        inline void group::dec_task_counter(group_misc::lattice_lock_policy lock_policy)
        {
            if (is_leaf())
            {
                if (_tasks.fetch_sub(1, hetcompute::mem_order_seq_cst) != 1)
                {
                    // No 1->0 transition happened
                    return;
                }
                launch_trigger_task();
                explicit_unref(this);
                return;
            }
            dec_task_counter_meet(lock_policy);
        }

        //  We define some inline tasks methods here and not in task.hh
        //  because they interact with groups and we can't include
        //  group.hh in task.hh to avoid a circular dependency

        inline void group_release_manager::release(group* g)
        {
            auto id           = g->get_id();
            auto has_valid_id = (g->is_pfor() == false) && g->is_leaf();
#if !defined(HETCOMPUTE_NO_GROUP_ALLOCATOR)
            if (has_valid_id && (id < group_factory::get_leaf_pool_size()))
            {
                // If the allocation is done through the allocator, just call the destructor
                g->~group();
            }
            else
            {
                // If the allocation is not done through the allocator, the group must be released
                delete (g);
            }
#else
            delete (g);
#endif // !defined(HETCOMPUTE_NO_GROUP_ALLOCATOR)
            // Only return id to id pool when the group is successfully destroyed
            if (has_valid_id)
                group_misc::id_generator::release_id(id);
        }

        inline void task::leave_groups_unlocked(group* g, bool must_cancel)
        {
            if (g == nullptr)
                return;

            if (must_cancel)
            {
                // Must happen before dec_task_counter so that any wait_for(g) does not get
                // released too early
                g->task_canceled();
            }
            g->dec_task_counter();
        }

        inline void task::propagate_exception_to(group* g)
        {
            if (g == nullptr)
                return;

            g->propagate_exception_from(_exception_state);
        }

        inline void task::leave_groups_unlocked(bool must_cancel)
        {
            // We can use mem_order_relaxed because leave_groups
            // always gets called with the task locked (and therefore
            // in between two barriers). Plus, setting up the group
            // pointer requires that the programmer acquires
            // the task lock. So it is totally safe to
            // use relaxed memory ordering here.
            auto g = get_current_group(hetcompute::mem_order_relaxed);
            if (must_cancel && is_exceptional())
            {
                propagate_exception_to(g);
                must_cancel = false;
            }
            leave_groups_unlocked(g, must_cancel);

            set_current_group_unlocked(nullptr);

            // Before you think about getting rid of this ASSERT because it looks
            // silly, let me tell you that we spent 3 weeks chasing a bug in
            // which the compiler decided to not reset the nullptr.
            HETCOMPUTE_INTERNAL_ASSERT(get_current_group(hetcompute::mem_order_relaxed) == nullptr, "Unexpected non-null group pointer");
        }

        inline bool task::set_running()
        {
            // We can use mem_order_relaxed because of the barriers in
            // task::schedule_sync() or in request_ownership.
            auto g = get_current_group(hetcompute::mem_order_relaxed);
            if (g && g->is_canceled(hetcompute::mem_order_relaxed))
                return false;

            return _state.set_running(is_anonymous(), is_cancelable(), this);
        }

        inline bool task::is_canceled(bool has_cancel_request)
        {
            if (!is_cancelable())
                return false;

            // Lock the task to protect us from the case where the
            // tasks completes execution and removes itself from
            // the group(s)

            auto l             = get_lock_guard();
            auto current_state = l.get_snapshot();

            if (!has_cancel_request && (current_state.is_running() || current_state.is_completed()))
                return false;

            if (current_state.is_canceled())
                return true;

            // By now we know that when we read current_state, the task wasn't
            // running. If it was marked for cancellation, then we know for sure
            // that the task will never execute and we can return true.

            if (current_state.has_cancel_request())
                return true;

            // By now we know that when we read current_state, the task wasn't
            // running and it wasn't marked for cancellation. Because we grab the
            // lock, we can guarantee that the task is, at most, in running state
            // (remember that transitioning from RUNNING to COMPLETED or CANCELED
            // requires that we grab the task lock).

            if (auto g = get_current_group(hetcompute::mem_order_relaxed))
            {
                return g->is_canceled();
            }

            return false;
        }

        inline void task::join_group_and_set_in_utcache(group* g)
        {
            HETCOMPUTE_INTERNAL_ASSERT(is_locked(), "Task must be locked: %s", to_string().c_str());
            HETCOMPUTE_INTERNAL_ASSERT(g != nullptr, "Nullptr");
            HETCOMPUTE_INTERNAL_ASSERT(get_current_group() == nullptr,
                                     "Task %s already belongs to group %p:%s",
                                     to_string().c_str(),
                                     g,
                                     g->to_string().c_str());

            auto prev_state = _state.set_in_utcache(this);
            HETCOMPUTE_INTERNAL_ASSERT(!prev_state.in_utcache(), "Task already in utcache: %s", to_string().c_str());

            HETCOMPUTE_UNUSED(prev_state);

            g->inc_task_counter();
            // We must use unlocked because the lock has already been
            // taken in join_group() (See first assert in method)
            set_current_group_unlocked(g);
        }

        inline void task::finish_after_state::cleanup_groups()
        {
            // Ensure that explicit refs in task::finish_after(finish_after_state*,
            // group*) are matched with unrefs
            for (auto i : _finish_after_groups)
            {
                explicit_unref(i);
            }

            // cleanup_groups may be called from launch_finish_after_stub_task and again
            // from the destructor of finish_after_state. (The call from
            // finish_after_state is necessary so that in exceptional circumstances, the
            // groups are unreffed and deallocated.) The groups must be unreffed only
            // once though. So clear the container here.
            _finish_after_groups.clear();
        }

    }; // namespace hetcompute::internal
}; // namespace hetcompute

#include <hetcompute/internal/log/coreloggerevents.hh>
