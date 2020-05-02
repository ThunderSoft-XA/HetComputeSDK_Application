#pragma once

#include <vector>

#include <hetcompute/internal/legacy/types.hh>
#include <hetcompute/internal/task/functiontraits.hh>
#include <hetcompute/internal/task/task_shared_ptr.hh>
#include <hetcompute/internal/task/task_state.hh>
#include <hetcompute/internal/util/debug.hh>

namespace hetcompute
{
    namespace internal
    {
        // Forward declarations
        class task;
        class scheduler;
        class task_bundle_dispatch;

        namespace testing
        {
            class SuccessorListSuite;
            class task_tests;
        }; // namespace hetcompute::internal::testing

        namespace task_internal
        {
            /// An std::lock_guard-like structure that locks the task when is
            /// created and releases the task lock when it is destroyed.
            class lock_guard
            {
                task&                _task;
                const state_snapshot _snapshot;

                /// Constructor. Locks the task.
                /// @param task Reference to the task.
                inline explicit lock_guard(task& task);

                /// Constructor. Only tasks can create them.
                /// @param task Reference to the task.
                /// @param state Task snapthost.
                inline lock_guard(task& task, state_snapshot snapshot);

                friend class ::hetcompute::internal::task;

            public:
                /// Returns the state of the task when the lock_guard
                /// was created.
                ///
                /// @return state_snapshot with the task state.
                state_snapshot get_snapshot() const
                {
                    return _snapshot;
                }

                /// Destructor. Unlocks the task
                inline ~lock_guard();
            };

            /// This functor is called by the shared_ptr object when the
            /// reference counter for the object gets to 0.
            struct release_manager
            {
                /// Deletes tasks t.
                /// @param t task to release.
                static void release(task* t);

            private:
                /// Checks whether it is ok to release the task @param t task to
                /// release. This method is expensive so we only call it in
                /// DEV builds.
                ///
                /// @return
                /// true - The task can be released.
                /// false - The task cannot be released.
                static bool debug_is_ok_to_release(task* t);
            };

            /// Encapsulates state needed to implement finish_after(t) or finish_after(g)
            class finish_after_state
            {
            public:
                task_shared_ptr _finish_after_stub_task; // stub task to implement finish_after

                // We really need to save group_ptrs instead of group* to ensure the groups
                // are not garbage-collected. However, only opaque pointers are permitted
                // here. We do explicit refs and unrefs on group* to extend and shorten
                // lifetimes.
                std::vector<group*> _finish_after_groups;

                finish_after_state() : _finish_after_stub_task(nullptr), _finish_after_groups(0)
                {
                }

                /// Reset _finish_after_groups
                void cleanup_groups();

                ~finish_after_state()
                {
                    cleanup_groups();
                }
            };

            /// This class stores the successors for a task.  In order to
            /// understand the design on this class, we must understand the
            /// following:
            ///
            /// - Many (if not most) tasks have zero successors.
            /// - Of the tasks that have successors:
            ///     - Most of them have only one or two.
            ///     - Most of them have have control dependencies.
            ///
            /// With this in mind, the successor list inlines pointers to 2
            /// control dependendent successors. That is, we don't need
            /// to do any extra allocation in a task if it has two or
            /// fewer control dependencies.
            ///
            /// If the tasks needs more control-dependent successors, or just one
            /// data-dependent successor, then it creates a list of buckets and
            /// transfers the inlined task pointers to the newly created
            /// bucket. At no point there will pointers to successors in the
            /// inlined spots and in the buckets.
            ///
            /// Then, successor_list stores the pointer to the bucket in the first
            /// of the inlined successor pointers. Look at the state class within
            /// successor_list to understand better how this works.
            ///
            /// Notes:
            /// - successor_list is not thread-safe. Its owner task ensures that
            ///      there are no data races accessing it.
            /// - GPU successors can be notified earlier. When
            ///      this happens, their entries in the list get reset and thus it is
            ///      possible that there are empty entries in the list.
            /// - successor_list does not provide random access to task pointers
            ///
            class successor_list
            {
                class dependency
                {
                public:
                    dependency() : _succ(nullptr), _move(false), _dst(nullptr)
                    {
                    }

                    explicit dependency(task* succ) : _succ(succ), _move(false), _dst(nullptr)
                    {
                    }

                    ~dependency()
                    {
                    }

                    dependency(task* succ, void* dst, bool move) : _succ(succ), _move(move), _dst(dst)
                    {
                    }

                    dependency(const dependency& other) : _succ(other._succ), _move(other._move), _dst(other._dst)
                    {
                    }

                    dependency& operator=(const dependency& other)
                    {
                        _succ = other._succ;
                        _move = other._move;
                        _dst  = other._dst;
                        return *this;
                    }

                    dependency(dependency&& other) : _succ(other._succ), _move(other._move), _dst(other._dst)
                    {
                    }

                    dependency& operator=(dependency&& other)
                    {
                        _succ = other._succ;
                        _move = other._move;
                        _dst  = other._dst;
                        return *this;
                    }

                    void update(task* t)
                    {
                        _succ = t;
                    }

                    void update(task* t, void* dst, bool move)
                    {
                        _succ = t;
                        _dst  = dst;
                        _move = move;
                    }

                    template <typename ReturnType>
                    void notify(void* val)
                    {
                        if (_dst != nullptr)
                        {
                            propagate_data<ReturnType>(val);
                        }
                    }

                    task*& get_succ()
                    {
                        return _succ;
                    }

                private:
                    task* _succ;
                    bool  _move; // Should the value be moved (true) to the successor or copied (false)
                    void* _dst;  // Destination of value

                    template <typename ReturnType>
                    void propagate_data(void* val, typename std::enable_if<!std::is_void<ReturnType>::value, ReturnType>::type* = nullptr)
                    {
                        if (_move)
                        {
                            *(static_cast<ReturnType*>(_dst)) = std::move(*(static_cast<ReturnType*>(val)));
                        }
                        else
                        {
                            *(static_cast<ReturnType*>(_dst)) = *(static_cast<ReturnType*>(val));
                        }
                    }

                    template <typename ReturnType>
                    void propagate_data(void*, typename std::enable_if<std::is_void<ReturnType>::value, ReturnType>::type* = nullptr)
                    {
                        // Do nothing
                    }
                };

                /// Instead of having a list of successors, successor_list creates
                /// buckets of successors to amortize allocation costs.
                class bucket
                {
                public:
                    using size_type = std::uint32_t;

                    // Number of entries in bucket.
                    static HETCOMPUTE_CONSTEXPR_CONST size_type s_max_entries = 8;

                    /// Constructor. Creates an empty bucket. Used to create the
                    /// first bucket.
                    bucket() : _dependencies(), _next(nullptr), _offset(0)
                    {
                    }

                    /// Constructor. Creates a new bucket, sets ```next``` as the next
                    /// bucket, and stores task t in the bucket. Notice that next
                    /// cannot be null, because the first bucket in a successor list
                    /// gets created using the default constructor. Used to create
                    /// subsequent buckets, with head-of-list insertion
                    ///
                    /// @param t Task to add to bucket.
                    /// @param next Bucket to follow this one.
                    bucket(task* t, void* dst, bool move, bucket* next) : _dependencies(), _next(next), _offset(1)
                    {
                        HETCOMPUTE_INTERNAL_ASSERT(next != nullptr, "Invalid null pointer.");

                        _dependencies[0].update(t, dst, move);
                    }

                    /// Destructor. Does nothing
                    ~bucket()
                    {
                    }

                    /// Returns the location of the next available entry in the
                    /// bucket. Notice that it is entirely possible that one or
                    /// more entries in [0 - offset] are empty.
                    ///
                    /// @return size_type with the location of the next available entry in the bucket.
                    size_type get_offset() const
                    {
                        return _offset;
                    }

                    /// Checks whether it is possible to add a dependency to this bucket.
                    /// @return
                    ///  true -- The bucket can accept a new dependency.
                    ///  false -- The bucket is full, wont' accept a new dependency.
                    bool can_add_dependency() const
                    {
                        return _offset < s_max_entries;
                    }

                    /// Inserts task into bucket. It does not do bounds check.
                    /// It assumes successor_list has called can_add_dependency first.
                    /// @param t Pointer of the task to add.
                    void insert(task* t, void* dst, bool move = false)
                    {
                        HETCOMPUTE_INTERNAL_ASSERT(can_add_dependency() == true, "No entries available in bucket.");
                        HETCOMPUTE_INTERNAL_ASSERT(_dependencies[_offset].get_succ() == nullptr, "Bucket already taken.");
                        _dependencies[_offset].update(t, dst, move);
                        _offset++;
                    }

                    /// Returns next bucket.
                    /// @return Pointer to next bucket.
                    bucket* get_next() const
                    {
                        return _next;
                    }

                    /// Provides random access to elements in bucket
                    /// @param index
                    /// @return lvalue reference to the element.
                    dependency& operator[](size_type index)
                    {
                        HETCOMPUTE_INTERNAL_ASSERT(index < s_max_entries, "Out of bounds: %d", static_cast<int>(index));
                        return _dependencies[index];
                    }

                private:
                    HETCOMPUTE_DELETE_METHOD(bucket(bucket const&));
                    HETCOMPUTE_DELETE_METHOD(bucket(bucket&&));
                    HETCOMPUTE_DELETE_METHOD(bucket& operator=(bucket const&));
                    HETCOMPUTE_DELETE_METHOD(bucket& operator=(bucket&&));

                    dependency _dependencies[s_max_entries];
                    bucket*    _next;
                    size_type  _offset;
                }; // bucket

                /// Because we try to minimize the size of the successor_list,
                /// we use one of the inlined task pointers to store information
                /// about the list. Because we need to do some bit masking, etc,
                /// we decided to put all that logic in this class.
                class state
                {
                    using raw_type = uintptr_t;

                    static HETCOMPUTE_CONSTEXPR_CONST raw_type s_bucket_mask = 1;
                    static HETCOMPUTE_CONSTEXPR_CONST raw_type s_empty_mask  = 2;

                    raw_type _state;

                public:
                    /// Checks whether the successor list is empty.
                    /// @return
                    ///   true -- There are no successor tasks.
                    ///   false -- There is at least one successor task.
                    bool is_empty()
                    {
                        return _state == s_empty_mask;
                    }

                    /// Marks the list as empty. Note that it is the responsibility
                    /// of the successor list to ensure that there are no successors.
                    void set_empty()
                    {
                        _state = s_empty_mask;
                    }

                    /// Sets the pointer to the first bucket in the bucket list.
                    /// @param b Bucket
                    void set_first_bucket(bucket* b)
                    {
                        HETCOMPUTE_INTERNAL_ASSERT((reinterpret_cast<uintptr_t>(b) & s_bucket_mask) == 0,
                                                 "Unexpected bucket pointer address: %p",
                                                 b);
                        // we mark that it is a bucket pointer by setting its LSB
                        _state = reinterpret_cast<uintptr_t>(b) | s_bucket_mask;
                    }

                    /// Checks whether there is one or more overflow buckets.
                    /// We know that there is an overflow bucket because
                    /// the LSB of state is set.
                    ///@return
                    /// true - There is at least one overflow bucket.
                    /// false - There are no overflow buckets.
                    bool has_buckets() const
                    {
                        return (_state & s_bucket_mask) != 0;
                    }

                    /// Returns a pointer to the first bucket. Note that it is the responsibility
                    /// of the successor list to ensure that there is at least one bucket.
                    /// @return
                    ///    bucket* -  Pointer to the first bucket.
                    bucket* get_first_bucket()
                    {
                        HETCOMPUTE_INTERNAL_ASSERT(has_buckets(), "Can't return a bucket becasue there isn't any.");
                        return reinterpret_cast<bucket*>(_state & ~s_bucket_mask);
                    }

                    /// Returns a task* with the initial state so that it can be used in the successor
                    /// list constructor
                    static task* create_initial_state()
                    {
                        return reinterpret_cast<task*>(s_empty_mask);
                    }

                    HETCOMPUTE_DELETE_METHOD(state());
                    HETCOMPUTE_DELETE_METHOD(state(state const&));
                    HETCOMPUTE_DELETE_METHOD(state(state&&));
                    HETCOMPUTE_DELETE_METHOD(state& operator=(state const&));
                    HETCOMPUTE_DELETE_METHOD(state& operator=(state&&));

                }; // state

                /// Returns a pointer to the list state
                /// @return
                ///  state* - Pointer to the list state.
                state* get_state()
                {
                    return reinterpret_cast<state*>(&_inlined_tasks[0]);
                }

                /// Returns a const pointer to the list state
                /// @return
                ///  state* const - Pointer to the list state.
                state* get_state() const
                {
                    return reinterpret_cast<state*>(&(const_cast<successor_list*>(this)->_inlined_tasks[0]));
                }

                static_assert(sizeof(task*) == sizeof(state), "task* should be as big as state");
                static_assert(sizeof(task*) == sizeof(uintptr_t), "task* should be as big as uinptr_t");

            public:
                using size_type = size_t;

            private:
                static HETCOMPUTE_CONSTEXPR_CONST size_type s_bucket_size       = bucket::s_max_entries;
                static HETCOMPUTE_CONSTEXPR_CONST size_type s_max_inlined_tasks = 2;

                static_assert(s_max_inlined_tasks >= 1, "Not enough inlined tasks.");
                static_assert(s_bucket_size >= s_max_inlined_tasks + 1, "Need to be able to at least store the inlined tasks plus one.");

            public:
#ifndef _MSC_VER
                /// Constructor.
                successor_list() : _inlined_tasks{ state::create_initial_state(), nullptr }
                {
                }
#else

                // MSVC doesn't like the way I initialize the inlined tasks above
                /// Constructor.
                successor_list() : _inlined_tasks()
                {
                    _inlined_tasks[0] = state::create_initial_state();
                }

#endif

                /// Assumes that the user has called reset already.
                ~successor_list()
                {
                    HETCOMPUTE_INTERNAL_ASSERT(is_empty(), "Reset list before deleting it.");
                    HETCOMPUTE_INTERNAL_ASSERT(!has_buckets(), "Overflow buckets exist.");
                }

                /// Checks whether successor_list is empty.
                /// @return
                /// true - Data structure is empty.
                /// false - Data structure is not empty.
                bool is_empty()
                {
                    return get_state()->is_empty();
                }

                /// Move constructor.
                /// @param other Successor list to move from.
                successor_list(successor_list&& other)
                {
                    move_elements(other);
                }

                /// Move assignment.
                /// @param other Successor list to move from.
                successor_list& operator=(successor_list&& other)
                {
                    move_elements(other);
                    return *this;
                }

                /// Adds task to successor list. It does not check for duplicates.
                /// @param t task
                void add_control_dependency(task* t);

                void add_data_dependency(task* t, void* dst, bool move);

                template <typename ReturnType>
                void propagate_return_value(void* val)
                {
                    if (is_empty())
                    {
                        return;
                    }

                    // If it doesn't have any buckets, just traverse inlined tasks
                    if (has_buckets() == false)
                    {
                        // Only control deps, so nothing to do
                        return;
                    }

                    // We traverse the overflow buckets
                    auto b = get_first_bucket();
                    do
                    {
                        for (bucket::size_type i = 0; i < bucket::s_max_entries; i++)
                        {
                            if ((*b)[i].get_succ() == nullptr)
                            {
                                continue;
                            }
                            (*b)[i].notify<ReturnType>(val);
                        }

                        b = b->get_next();
                    } while (b != nullptr);
                }

                /**
                 * Propagate exceptions (if any) from task t
                 * @param t task from which to propagate exceptions
                 */
                inline void propagate_exception(task* t);

                /// Calls task::predecessor_finished() on each of the tasks of the
                /// list and resets the list. This means that there will be no
                /// tasks of any type in the list and all buckets will be deleted.
                ///
                /// @param canceled If true, it calls
                ///        task::predecessor_requests_cancellation() prior to calling
                ///        task::predecessor_finished()
                ///
                /// @param sched Scheduler that executed the predecessor task. It
                ///        could be nullptr.
                inline void predecessor_finished(bool canceled, scheduler* sched);

                /// Calls task::predecessor_finished() on any GPU task in the
                /// successor list.
                ///
                /// @param sched Scheduler that executed the predecessor task. It
                ///        could be nullptr.
                /// @param tbd Predecessor task was scheduled in this task bundle. Likely nullptr.
                void gpu_predecessor_finished(scheduler* sched, task_bundle_dispatch* tbd);

                /// Adds all successors to container c.
                /// This is used during cancellation traverse the DAG of successors of a task.
                /// Calls Container::push(task*) for each successor.
                /// @param c Container.
                template <typename Container>
                void add_to_container(Container& c)
                {
                    if (is_empty())
                    {
                        return;
                    }

                    auto l = [&c](task* t) { c.push(t); };

                    for_each_successor(std::forward<decltype(l)>(l));
                }

                /// Returns a string with the status of this data structure
                /// @return std::string with the status of this data structure.
                std::string to_string();

            private:
                /// Resets all task pointers and deletes all buckets. After it executes,
                /// the list is empty.
                void reset();

                void add_dependency(task* t, void* dst, bool move = false);

                /// Inserts task dependency into bucket.
                /// @param t Task to add
                void add_dependency_in_bucket(task* t, void* dst, bool move = false);

                /// Executes method on each successor, resets the inlined pointers
                /// and deletes any buckets.
                ///
                /// @param  f method to execute on each item. f's
                ///         signature must be void f(task* t). t
                ///         will never be null.
                template <typename F>
                void apply_for_each_and_clear(F&& f)
                {
                    using traits = typename ::hetcompute::internal::function_traits<F>;

                    static_assert(traits::arity::value == 1, "F can only have one argument. ");
                    static_assert(std::is_same<typename traits::return_type, void>::value, "F must return void. ");

                    HETCOMPUTE_INTERNAL_ASSERT(!is_empty(), "List shouldn't be empty by now");

                    // If it doesn't have any buckets, just traverse inlined tasks
                    if (has_buckets() == false)
                    {
                        for (size_t i = 0; i < s_max_inlined_tasks; ++i)
                        {
                            if (_inlined_tasks[i] == nullptr)
                            {
                                continue;
                            }
                            f(_inlined_tasks[i]);
                            _inlined_tasks[i] = nullptr;
                        }

                        get_state()->set_empty();
                        return;
                    }

                    // We traverse the overflow buckets
                    auto b = get_first_bucket();
                    do
                    {
                        for (bucket::size_type i = 0; i < bucket::s_max_entries; ++i)
                        {
                            if ((*b)[i].get_succ() == nullptr)
                            {
                                continue;
                            }

                            f((*b)[i].get_succ());
                        }

                        auto next = b->get_next();
                        delete b;
                        b = next;

                    } while (b != nullptr);

                    get_state()->set_empty();
                }

                /// Executes method on each entry.
                /// @param  f method to execute on each entry. f's
                ///         signature must be void f(task** e). e
                ///         could be null.
                template <typename F>
                void for_each_entry(F&& f)
                {
                    using traits = ::hetcompute::internal::function_traits<F>;

                    static_assert(traits::arity::value == 1, "F can only have one argument. ");
                    static_assert(std::is_same<typename traits::return_type, void>::value, "F must return void. ");

                    if (is_empty())
                    {
                        return;
                    }

                    // If it doesn't have any buckets, just traverse inlined tasks
                    if (has_buckets() == false)
                    {
                        for (size_t i = 0; i < s_max_inlined_tasks; ++i)
                        {
                            f(&_inlined_tasks[i]);
                        }
                        return;
                    }

                    // We traverse the overflow buckets
                    auto b = get_first_bucket();
                    do
                    {
                        for (bucket::size_type i = 0; i < bucket::s_max_entries; ++i)
                        {
                            f(&(*b)[i].get_succ());
                        }

                        b = b->get_next();
                    } while (b != nullptr);
                }

                /// Executes method on each successor.
                ///
                /// @param  f method to execute on each item. f's
                ///         signature must be void f(task* t). t
                ///         will never be null.
                template <typename F>
                void for_each_successor(F&& f)
                {
                    using traits = ::hetcompute::internal::function_traits<F>;

                    static_assert(traits::arity::value == 1, "F can only have one argument. ");
                    static_assert(std::is_same<typename traits::return_type, void>::value, "F must return void. ");

                    auto l = [&f](task** t)
                    {
                        if (*t != nullptr)
                        {
                            f(*t);
                        }
                    };

                    for_each_entry(l);
                }

                /// Stores pointer to first bucket.
                /// @param b bucket pointer.
                void set_first_bucket(bucket* b)
                {
                    get_state()->set_first_bucket(b);
                }

                /// Checks whether there is one or more overflow buckets.
                /// @return
                /// true - There is at least one overflow bucket.
                /// true - There are no overflow buckets.
                bool has_buckets() const
                {
                    return get_state()->has_buckets();
                }

                /// Returns pointer to first overflow bucket.
                /// @return pointer to first overflow bucket.
                bucket* get_first_bucket() const
                {
                    return get_state()->get_first_bucket();
                }

                /// Copies inlined tasks from other to this and resets other's
                /// inlined tasks. Note that this means that we're moving the
                /// overflow buckets.
                void move_elements(successor_list& other)
                {
                    for (size_type i = 0; i < s_max_inlined_tasks; ++i)
                    {
                        _inlined_tasks[i]       = other._inlined_tasks[i];
                        other._inlined_tasks[i] = nullptr;
                    }
                    other.set_empty();
                }

                /// Resets inlined task pointer.
                /// @param pos Pointer to reset.
                void reset_inlined_ptr(size_type pos)
                {
                    _inlined_tasks[pos] = nullptr;
                }

                /// Marks list as unused.
                void set_empty()
                {
                    get_state()->set_empty();
                }

                /// Counts number of allocated buckets.
                /// This is a slow method and it is only intended for debugging purposes.
                ///
                /// @return Number of allocated buckets.
                size_type get_num_buckets() const;

                /// Returns number of elements in data structure.
                /// This is a slow method and it is only intended for debugging purposes.
                ///
                /// @return Number of elements in data structure.
                size_type get_num_successors();

                // The copy constructor and the copy assignment operator are implicitly
                // deleted due to the below user-defined move constructor. However, we
                // explicitly delete them to make -Weffc++ happy.
                HETCOMPUTE_DELETE_METHOD(successor_list(successor_list const& other));
                HETCOMPUTE_DELETE_METHOD(successor_list& operator=(successor_list const& other));

                task* _inlined_tasks[s_max_inlined_tasks];

                friend class testing::SuccessorListSuite;
                friend class testing::task_tests;

            }; // successor_list

            class blocking_code_container_base
            {
            public:
                virtual void execute() = 0;
                virtual void cancel()  = 0;
                virtual ~blocking_code_container_base()
                {
                }
            };

            template <typename BlockingFunction, typename CancelFunction>
            class blocking_code_container : public blocking_code_container_base
            {
                BlockingFunction _bf;
                CancelFunction   _cf;

            public:
                blocking_code_container(BlockingFunction&& bf, CancelFunction&& cf)
                    : _bf(std::forward<BlockingFunction>(bf)), _cf(std::forward<CancelFunction>(cf))
                {
                }

                virtual void execute()
                {
                    _bf();
                }

                virtual void cancel()
                {
                    _cf();
                }

                virtual ~blocking_code_container()
                {
                }

                HETCOMPUTE_DELETE_METHOD(blocking_code_container(blocking_code_container const&));
                HETCOMPUTE_DELETE_METHOD(blocking_code_container& operator=(blocking_code_container const&));
                HETCOMPUTE_DELETE_METHOD(blocking_code_container(blocking_code_container&&));
                HETCOMPUTE_DELETE_METHOD(blocking_code_container& operator=(blocking_code_container&&));
            };

        }; // namespace hetcompute::internal::task_internal
    };     // namespace hetcompute::internal
};         // namespace hetcompute
