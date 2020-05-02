#pragma once

#include <string>
#include <tuple>
#include <typeinfo>
#include <type_traits>

// Include user-visible header first
#include <hetcompute/buffer.hh>

// Include internal headers next
#include <hetcompute/internal/buffer/bufferpolicy.hh>
#include <hetcompute/internal/task/cputaskinternal.hh>
#include <hetcompute/internal/task/task.hh>
#include <hetcompute/internal/util/demangler.hh>
#include <hetcompute/internal/util/scopeguard.hh>
#include <hetcompute/internal/util/strprintf.hh>

// This code is most easily comprehended if read from bottom to top.

namespace hetcompute
{
    // forward declarations
    template <typename... Stuff>
    class task_ptr;
    template <typename... Stuff>
    class task;

    template <typename BlockingFunction, typename CancelFunction>
    void blocking(BlockingFunction&& bf, CancelFunction&& cf)
    {
        auto t = hetcompute::internal::current_task();
        if (t != nullptr)
        {
            HETCOMPUTE_API_THROW(t->is_blocking(),
                               "hetcompute::blocking can only be called from within a blocking task or from outside a task");

            hetcompute::internal::task_internal::blocking_code_container<BlockingFunction, CancelFunction> bcc(std::forward<BlockingFunction>(bf),
                                                                                                             std::forward<CancelFunction>(cf));
            t->execute_blocking_code(&bcc);
        }
        else
        {
            // not called from within a task
            bf();
        }
    }

    namespace internal
    {
        /// This class template takes care of the arguments in the task body.
        template <typename ReturnType, typename... ArgTypes>
        class cputask_arg_layer;

        // Forward declaration for group
        class group;

        class task_bundle_dispatch;

        // We split the functionality of the cpu tasks in three layers:
        // (from bottom to top)
        //
        // - Return layer: This class template stores the return
        //                 value (if any) of the task.
        // - Argument layer: This class template stores the arguments
        //                 (if any) required to execute the body.
        // - Body layer: This class template stores the actual body of the task.
        //
        // Notice that:
        //
        // - cputask inherits from cputask_argument_layer
        // - cputask_argument_layer inherits from cputask_return_layer
        // - cputask_return_layer inherits from task
        //
        // Thus, this file can be better understood if read from bottom to top.
        //
        // The constructros to cputask_return_layer and cputask_argument_layer
        // are protected because construction of a cputask always requires
        // a cputask.
        //

        /// This class template stores the return value of the
        /// task.
        template <typename ReturnType>
        class cputask_return_layer : public task
        {
        public:
            using container_type = retval_container<ReturnType>;

            /// Task return type (strip const)
            using return_type = typename container_type::type;

            /// Destructor. In this version, it will always call the destructor
            /// of the return value.
            virtual ~cputask_return_layer(){};

            /// Returns task return value. If the task hasn't completed yet it
            /// will return the default value for return_type. HetCompute will make
            /// sure that the user doesn't see this. The programmer will be
            /// indirectly calling it when the call task<...>::get_value().
            ///
            /// @return
            /// return_type -- Task return value.
            return_type const& get_retval() const { return _retval_container._retval; }

            /// Moves the task return value out fo the task. If the task hasn't
            /// completed yet it will return the default value for return_type.
            /// Programmers will be indirectly calling it when they call
            /// task<...>::get_value().
            ///
            /// @return
            ///   return_type -- Rvalue reference to the task return value.
            return_type&& move_retval() { return std::move(_retval_container._retval); }

            template <std::uint32_t Pos, typename SuccReturnType, typename... SuccArgTypes>
            void add_data_dependency(cputask_arg_layer<SuccReturnType(SuccArgTypes...)>* succ)
            {
                static_assert(sizeof...(SuccArgTypes) >= 1, "Invalid task arity.");
                bool should_copy_data = false; // Is this task should copy the data of its predecessor's return value
                if (!prepare_to_add_task_dependence(this, succ, should_copy_data))
                {
                    if (should_copy_data)
                    {
                        succ->template set_arg<Pos>(_retval_container._retval);
                    }
                    return;
                }

                get_successors().add_data_dependency(succ, succ->template get_arg<Pos>(), succ->template should_move<Pos>());

                cleanup_after_adding_task_dependence(this, succ);
            }

            /// TODO: @see execute_dispatch that invokes finish_after.
            /// This is public due to a bug in gcc. Lambdas cannot access protected
            /// members of parent class. A workaround may be to just define a function
            /// and explicitly bind arguments.
            /// TODO: Do we need any move semantics here? Note that even though a task
            /// may return another task_ptr t, t may have escaped and t->copy_value() may
            /// be possible. Therefore, we cannot move in general.
            template <typename ReturnT>
            void store_retval(ReturnT&& retval)
            {
                _retval_container = retval;
            }

        protected:
            /// Constructor. Only to be called by an object of the type
            /// cputask_arg_layer<ReturnType(...)> Notice that we are calling
            /// the constructor for the return value. In a future version we
            /// could think of optimizing this because we are going to overwrite
            /// it anyways.
            ///
            /// @param g Group the task belongs to
            /// @param a Initial task attributes
            /// @param args Needed to initialize retval (only used for value tasks)
            template <typename... Args>
            explicit cputask_return_layer(state_snapshot initial_state, group* g, legacy::task_attrs a, Args&&... args)
                : task(initial_state, g, a), _retval_container(return_type(std::forward<Args>(args)...))
            {
            }

            /// Constructor. Only to be called by an object of the type
            /// cputask_arg_layer<ReturnType(...)> Notice that we are calling
            /// the constructor for the return value. In a future version we
            /// could think of optimizing this because we are going to overwrite
            /// it anyways.
            ///
            /// @param g Group the task belongs to
            /// @param a Initial task attributes
            /// @param args Needed to initialize retval (only used for value tasks)
            template <typename... Args>
            explicit cputask_return_layer(group* g, legacy::task_attrs a, Args&&... args)
                : task(g, a), _retval_container(return_type(std::forward<Args>(args)...))
            {
            }

            /// Describes return_type.
            /// @return
            ///  std::string -- A string with the description of the return type.
            std::string describe_return_value() { return demangler::demangle<ReturnType>(); };

            /// Calls b(args...) and stores the result in _retval.
            ///
            /// @param b Body to execute.
            /// @param args Arguments to be pass to b.
            template <typename Body, typename... Args>
            void execute_and_store_retval(Body& b, Args&&... args)
            {
                _retval_container = b(args...);
            }

            virtual void propagate_return_value_to_successors(successor_list& successors, void* value)
            {
                // When propagate the return value, we don't need the const-ness.
                successors.propagate_return_value<return_type>(value);
            }

        private:
            virtual void* get_value_ptr() { return &_retval_container._retval; }

            // Container for the value returned by the task
            retval_container<ReturnType> _retval_container;

        }; // cputask_return_layer<ReturnType>

        // This specialization of the cputask_return_layer takes care of the
        // case where the task returns nothing. Thus, it doesn't have to store
        // anything.
        template <>
        class cputask_return_layer<void> : public task
        {
        public:
            // Desctructor. Does nothing.
            virtual ~cputask_return_layer(){};

        protected:
            // Constructor. Only to be called by an object
            // of the type cputask_arg_layer<ReturnType(...)>
            //
            // @param g Group the task belongs to
            cputask_return_layer(state_snapshot initial_state, group* g, legacy::task_attrs a) : task(initial_state, g, a) {}

            // Constructor. Only to be called by an object
            // of the type cputask_arg_layer<ReturnType(...)>
            //
            // @param g Group the task belongs to
            cputask_return_layer(group* g, legacy::task_attrs a) : task(g, a) {}

            // Describes return_type.
            // @return
            //  std::string -- A string with the description of the return type.
            std::string describe_return_value() { return "void"; };

            // Calls b(args...). Body returns nothing so there is no need
            // to store its result.
            //
            // @param b Body to execute.
            // @param args Arguments to be pass to b.
            template <typename Body, typename... Args>
            void execute_and_store_retval(Body& b, Args&&... args)
            {
                b(args...);
            }

        }; // cputask_return_layer<ReturnType>

        /// Some algorithms (think fibonnacci) might need to return a
        /// constant for corner cases. A completed_cpu_task is just a
        /// task whose return value is known at task creation time
        /// and it's only there to make the code prettier.
        /// value_cputasks do not belong to any group.
        template <typename ReturnType>
        class value_cputask : public cputask_return_layer<ReturnType>
        {
            using state_snapshot = typename cputask_return_layer<ReturnType>::state_snapshot;

        public:
            /// Constructor
            /// @param args Arguments for constructing the return value.
            template <typename... Args>
            explicit value_cputask(state_snapshot initial_state, group* g, legacy::task_attrs attrs, Args&&... args)
                : cputask_return_layer<ReturnType>(initial_state, g, attrs, std::forward<Args>(args)...)
            {
            }

            using return_type = ReturnType;

            /// Returns a string with the text value_cputask<ReturnType>
            /// @return
            ///    std::string with the text "value_cputask<ReturnType>"
            virtual std::string describe_body() { return strprintf("value_cputask<%s>", demangler::demangle<ReturnType>().c_str()); }

            /// Destructor. Does nothing
            virtual ~value_cputask() {}

        private:
            /// Execute. Should never happen.
            virtual bool do_execute(task_bundle_dispatch* tbd = nullptr)
            {
                HETCOMPUTE_UNUSED(tbd);
                HETCOMPUTE_UNREACHABLE("Completed tasks cannot execute.");
                return false;
            }

            /// Destroys body and args. Should never happen.
            virtual void destroy_body_and_args() { HETCOMPUTE_UNREACHABLE("Cannot destroy the body of a completed task."); }

            // Disable all copying and movement,
            HETCOMPUTE_DELETE_METHOD(value_cputask(value_cputask const&));
            HETCOMPUTE_DELETE_METHOD(value_cputask(value_cputask&&));
            HETCOMPUTE_DELETE_METHOD(value_cputask& operator=(value_cputask const&));
            HETCOMPUTE_DELETE_METHOD(value_cputask& operator=(value_cputask&&));

        }; // value_cputask<ReturnType>

        /// This specialization is used when the task has no arguments. This is the
        /// easy case as we don't neet to store them.
        template <typename ReturnType>
        class cputask_arg_layer<ReturnType()> : public cputask_return_layer<ReturnType>
        {
            using parent             = cputask_return_layer<ReturnType>;
            using parent_return_type = typename std::decay<ReturnType>::type;

            static_assert(std::is_same<ReturnType, void>::value || std::is_default_constructible<parent_return_type>::value,
                          "Task return type for a non-value task must be default_constructible.");

        public:
            // Destructor, does nothing
            virtual ~cputask_arg_layer() {}

            void release_buffers(void*)
            {
                // No args, so no buffer to release
            }

        protected:
            using state_snapshot = typename parent::state_snapshot;

            /// Describes the task arguments
            /// @return
            /// std::string -- A string with the description of the task arguments
            std::string describe_arguments() { return "()"; };

            /// Constructor.
            /// @param g Group the task belongs to
            /// @param a Initial task attributes
            cputask_arg_layer(state_snapshot initial_state, group* g, legacy::task_attrs a) : parent(initial_state, g, a) {}

            /// Constructor.
            /// @param g Group the task belongs to
            /// @param a Initial task attributes
            cputask_arg_layer(group* g, legacy::task_attrs a) : parent(g, a) {}

            /// Prepares arguments that the body needs. Because this class
            /// template deals with bodies with no arguments, we don't have
            /// anything to do here. We just call execute_and_store_retval
            /// in the parent class.
            ///
            /// @param b lvalue reference to the body that will be executed
            template <typename Body>
            void prepare_args(Body& b)
            {
                parent::template execute_and_store_retval<Body>(b);
            }

            /// Prepares arguments that the body needs. Because this class
            /// template deals with bodies with no arguments, we don't have
            /// anything to do here. We just call execute_and_store_retval in
            /// the parent class.
            ///
            /// @param b lvalue reference to the body that will be executed
            template <typename Body>
            ::hetcompute::task_ptr<ReturnType> prepare_args_but_do_not_store_retval(Body& b)
            {
                return b();
            }

            void destroy_args()
            {
                // No args, so nothing to destroy
            }

            void* acquire_buffers()
            {
                // No args, so no buffer to bring
                return nullptr;
            }
        }; // class cputask_arg_layer<ReturnType()>

        /// This specialization is used when the task has arguments.  This
        /// layer stores them.
        template <typename ReturnType, typename Arg1, typename... Rest>
        class cputask_arg_layer<ReturnType(Arg1, Rest...)> : public cputask_return_layer<ReturnType>
        {
            using parent = cputask_return_layer<ReturnType>;

            // Number of arguments in body
            using arity = std::integral_constant<std::uint32_t, 1 + sizeof...(Rest)>;

            // Types of the arguments, as specified in the body
            using orig_arg_types = std::tuple<Arg1, Rest...>;

            // We can't always store the arguments as they are specified in the body.
            // If we did so, we could not set an arg if the argument was specified
            // as const.
            using storage_arg_types =
                std::tuple<typename task_arg_type_info<Arg1>::storage_type, typename task_arg_type_info<Rest>::storage_type...>;

            // We use this template to check the type of an individual argument
            template <std::uint32_t Pos>
            using orig_type = typename std::tuple_element<Pos, orig_arg_types>::type;

            /// Set argument at position Pos to value. If the user declared the argument
            /// at Pos as an rvalue reference, then set_arg will move value. Programmers
            /// call this method when they call task<...>::set_arg()
            ///
            /// Othewise, it'll copy it.
            template <std::uint32_t Pos, typename ArgType>
            void set_arg(ArgType&& value)
            {
                static_assert(Pos < arity::value, "Out-of-range Pos value");

                // type the user declared
                using expected_type = orig_type<Pos>;

                // did the user declared an rvalue reference?
                using is_rvalue_ref = typename std::is_rvalue_reference<expected_type>::type;

                // so we could se the arg, let's copy/move value to args<pos>
                set_arg_impl<Pos, ArgType>(std::forward<ArgType>(value), is_rvalue_ref());
            }

            /// Moves argument into the arg tuple.  Only called from set_arg.
            /// Pos indicates the argument to be set.
            ///
            /// @param value Value to move to the argument.
            /// @param nothing This second argument is just used to dispatch
            /// to the right set_arg implementation.
            template <std::uint32_t Pos, typename ArgType>
            void set_arg_impl(ArgType&& value, std::true_type)
            {
                static_assert(Pos < arity::value, "Out-of-range Pos value");
                std::get<Pos>(_args) = std::move(value);
            }

            /// Copies argument into the arg tuple.  Only called from set_arg.
            /// Pos indicates the argument to be set.
            ///
            /// @param value Value to move to the argument.
            /// @param nothing This second argument is just used to dispatch
            /// to the right set_arg implementation.
            template <std::uint32_t Pos, typename ArgType>
            void set_arg_impl(ArgType&& value, std::false_type)
            {
                static_assert(Pos < arity::value, "Out-of-range Pos value");
                std::get<Pos>(_args) = value;
            }

            template <std::uint32_t Pos, typename PredReturnType>
            void set_data_dependency_impl(cputask_return_layer<PredReturnType>* pred)
            {
                static_assert(std::is_same<PredReturnType, void>::value == false, "Can't bind a task that returns void.");

                HETCOMPUTE_INTERNAL_ASSERT(pred != nullptr, "Unexpected null pointer.");

                pred->template add_data_dependency<Pos, ReturnType, Arg1, Rest...>(this);
            }

            /// Bind an argument by value
            ///
            /// @param value Value to move to the argument.
            /// @param nothing This second argument is just used to dispatch
            /// to the right set_arg implementation.
            template <std::uint32_t Pos, typename ArgType>
            struct bind_by_value
            {
                static void bind_impl(ArgType&& value, cputask_arg_layer<ReturnType(Arg1, Rest...)>* curr_task)
                {
                    static_assert(is_by_value_t<ArgType&&>::value == true, "Argument has to be bound by hetcompute::by_value.");

                    using user_arg = typename is_by_value_t<ArgType&&>::type;

                    curr_task->set_arg<Pos, user_arg>(std::forward<user_arg>(value._t));
                }
            };

            /// Bind an argument by data dependency
            ///
            /// @param value Value to move to the argument.
            /// @param nothing This second argument is just used to dispatch
            /// to the right set_arg implementation.
            template <std::uint32_t Pos, typename ArgType>
            struct bind_by_data_dep
            {
                static void bind_impl(ArgType&& value, cputask_arg_layer<ReturnType(Arg1, Rest...)>* curr_task)
                {
                    static_assert(is_by_data_dep_t<ArgType&&>::value == true, "Argument has to be bound by hetcompute::by_data_dependency.");
                    using user_arg         = typename is_by_data_dep_t<ArgType&&>::type;
                    using user_arg_noref   = typename std::remove_reference<user_arg>::type;
                    using user_arg_nocvref = typename std::remove_cv<user_arg_noref>::type;

                    static_assert(is_hetcompute_task_ptr<user_arg_nocvref>::has_return_value == true,
                                  "Argument has to be a hetcompute api20 task_ptr with return type information");
                    using pred_return_type = typename is_hetcompute_task_ptr<user_arg_nocvref>::type;

                    auto pred_ptr = static_cast<cputask_return_layer<pred_return_type>*>(::hetcompute::internal::get_cptr(value._t));
                    curr_task->set_data_dependency_impl<Pos, pred_return_type>(pred_ptr);
                }
            };

            /// Bind an argument as value
            ///
            /// @param value Value to move to the argument.
            /// @param nothing This second argument is just used to dispatch
            /// to the right set_arg implementation.
            template <std::uint32_t Pos, typename ArgType>
            struct bind_as_value
            {
                static void bind_impl(ArgType&& value, cputask_arg_layer<ReturnType(Arg1, Rest...)>* curr_task)
                {
                    static_assert(is_by_value_t<ArgType&&>::value == false, "Argument has to be bound as value.");
                    curr_task->set_arg<Pos, ArgType&&>(std::forward<ArgType>(value));
                }
            };

            /// Bind an argument as dependency
            ///
            /// @param value Value to move to the argument.
            /// @param nothing This second argument is just used to dispatch
            /// to the right set_arg implementation.
            template <std::uint32_t Pos, typename ArgType>
            struct bind_as_data_dep
            {
                static void bind_impl(ArgType&& value, cputask_arg_layer<ReturnType(Arg1, Rest...)>* curr_task)
                {
                    static_assert(is_by_data_dep_t<ArgType&&>::value == false, "Argument has to be bound as data dependency.");

                    using user_arg_noref   = typename std::remove_reference<ArgType&&>::type;
                    using user_arg_nocvref = typename std::remove_cv<user_arg_noref>::type;

                    static_assert(is_hetcompute_task_ptr<user_arg_nocvref>::has_return_value == true,
                                  "Argument has to be a hetcompute api20 task_ptr with return type information");
                    using pred_return_type = typename is_hetcompute_task_ptr<user_arg_nocvref>::type;

                    auto pred_ptr = static_cast<cputask_return_layer<pred_return_type>*>(::hetcompute::internal::get_cptr(value));
                    curr_task->set_data_dependency_impl<Pos, pred_return_type>(pred_ptr);
                }
            };

            ///
            /// Bind argument at position Pos to value.
            /// If the user declared the argument
            /// at Pos as an rvalue reference, then set_arg will move value. Othewise, it'll copy it.
            /// The function will bind the argument either as value or as data dependency
            /// base on the result of type matching. If the bind-as type is ambiguous,
            /// the programmer is supposed to explicitly specific the type, otherwise,
            /// compilation will fail.
            ///
            template <std::uint32_t Pos, typename ArgType>
            void bind(ArgType&& value)
            {
                HETCOMPUTE_CONSTEXPR_CONST bool is_by_value    = is_by_value_t<ArgType&&>::value;
                HETCOMPUTE_CONSTEXPR_CONST bool is_by_data_dep = is_by_data_dep_t<ArgType&&>::value;
                // the following static assert should always pass
                static_assert(!is_by_value || !is_by_data_dep, "Cannot bind by value and by data dependency at the same time.");

                using user_arg =
                    typename std::conditional<is_by_value, typename is_by_value_t<ArgType&&>::type, typename is_by_data_dep_t<ArgType&&>::type>::type;
                using expected = typename std::tuple_element<Pos, orig_arg_types>::type;

                HETCOMPUTE_CONSTEXPR_CONST bool can_as_value    = can_bind_as_value<user_arg, expected>::value;
                HETCOMPUTE_CONSTEXPR_CONST bool can_as_data_dep = can_bind_as_data_dep<user_arg, expected>::value;

                HETCOMPUTE_CONSTEXPR_CONST bool by_value    = is_by_value && can_as_value;
                HETCOMPUTE_CONSTEXPR_CONST bool by_data_dep = is_by_data_dep && can_as_data_dep;
                HETCOMPUTE_CONSTEXPR_CONST bool as_value    = !is_by_value && can_as_value;
                HETCOMPUTE_CONSTEXPR_CONST bool as_data_dep = !is_by_data_dep && can_as_data_dep;

                HETCOMPUTE_CONSTEXPR_CONST bool undecided = (as_value && as_data_dep && !by_value && !by_data_dep) == true;
                HETCOMPUTE_CONSTEXPR_CONST bool error     = (as_value || as_data_dep || by_value || by_data_dep) == false;

                static_assert(undecided == false, "Ambiguous argument binding. Please specifiy by using hetcompute::by_value() or "
                                                  "hetcompute::by_data_dependency().");
                static_assert(error == false, "Argument binding type mis-match. Cannot bind argument.");

                using bind_policy = typename std::conditional<
                    by_value,
                    bind_by_value<Pos, ArgType>,
                    typename std::conditional<
                        by_data_dep,
                        bind_by_data_dep<Pos, ArgType>,
                        typename std::conditional<as_value, bind_as_value<Pos, ArgType>, bind_as_data_dep<Pos, ArgType>>::type>::type>::type;
                bind_policy::bind_impl(std::forward<ArgType>(value), this);
            }

            // The following structs (integer_sequence and
            // create_integer_sequence) will allow us to take a tuple, and pass
            // it as separate arguments to a function call.

            /// This structure is similar to integer_sequence in C++14.
            /// We don't use C++14 yet. As soon as we enable it in
            /// our builds, we'll replace this with std::integer_sequence.
            template <std::uint32_t... numbers>
            struct integer_sequence
            {
                static HETCOMPUTE_CONSTEXPR_CONST size_t get_size() { return sizeof...(numbers); }
            };

            /// The following structs creates an integer sequence from 0 to Top-1.
            template <std::uint32_t Top, std::uint32_t... Sequence>
            struct create_integer_sequence : create_integer_sequence<Top - 1, Top - 1, Sequence...>
            {
            };

            template <std::uint32_t... Sequence>
            struct create_integer_sequence<0, Sequence...>
            {
                using type = integer_sequence<Sequence...>;
            };

            template <typename Body, uint32_t... Number>
            void expand_args(Body& b, integer_sequence<Number...>)
            {
                parent::template execute_and_store_retval(b, std::move(std::get<Number>(_args))...);
            }

            template <typename Body, uint32_t... Number>
            ::hetcompute::task_ptr<ReturnType> expand_args_but_do_not_store_retval(Body& b, integer_sequence<Number...>)
            {
                return b(std::move(std::get<Number>(_args))...);
            }

            storage_arg_types _args;
            set_arg_tracker _set_arg_tracker;

            template <std::uint32_t Pos>
            void destroy_args_impl(std::false_type)
            {
                std::get<Pos>(_args).destroy();
                destroy_args_impl<Pos + 1>(typename std::conditional<(Pos + 1) < arity::value, std::false_type, std::true_type>::type());
            }

            template <std::uint32_t Pos>
            void destroy_args_impl(std::true_type)
            {
                // Do nothing, base case
            }

            static constexpr size_t num_buffer_args = num_buffer_ptrs_in_tuple<std::tuple<Arg1, Rest...>>::value;
            using buffer_acquire_set_t              = buffer_acquire_set<num_buffer_args>;

            template <typename BufferPtr>
            void acquire_buffer(buffer_acquire_set_t& bas, BufferPtr& b, std::true_type, bool do_not_dispatch)
            {
                if (b == nullptr)
                {
                    return;
                }

                if (do_not_dispatch == true)
                {
                    bas.add(b,
                            std::conditional<is_const_buffer_ptr<BufferPtr>::value,
                                             std::integral_constant<bufferpolicy::action_t, bufferpolicy::acquire_r>,
                                             std::integral_constant<bufferpolicy::action_t, bufferpolicy::acquire_rw>>::type::value);
                }
                else
                { // dispatch
                    auto acquired_arena = bas.find_acquired_arena(b);
                    HETCOMPUTE_INTERNAL_ASSERT(acquired_arena != nullptr, "Error. Acquired arena is nullptr");
                    arena_storage_accessor::access_mainmem_arena_for_cputask(acquired_arena);
                    reinterpret_cast<hetcompute::internal::buffer_ptr_base&>(b).allocate_host_accessible_data(true);
                }
            }

            template <typename NotBufferPtr>
            void acquire_buffer(buffer_acquire_set_t&, NotBufferPtr&, std::false_type, bool)
            {
                // Do nothing, it's not a buffer
            }

            template <std::uint32_t Pos>
            void acquire_buffers_impl(buffer_acquire_set_t& bas, std::false_type, bool do_not_dispatch)
            {
                using type      = typename std::tuple_element<Pos, storage_arg_types>::type::type;
                using is_buffer = is_api20_buffer_ptr<type>;

                acquire_buffer(bas, std::get<Pos>(_args).get_larg(), is_buffer(), do_not_dispatch);
                acquire_buffers_impl<Pos + 1>(bas,
                                              typename std::conditional<(Pos + 1) < arity::value, std::false_type, std::true_type>::type(),
                                              do_not_dispatch);
            }

            template <std::uint32_t Pos>
            void acquire_buffers_impl(buffer_acquire_set_t&, std::true_type, bool)
            {
                // base case
            }

            ///
            /// Implementation for binding all the arguments
            /// @param arg1  the first argument
            /// @param rest  the rest of the arguments
            ///
            template <size_t Pos, typename Arg1Type, typename... RestType>
            void bind_all_impl(Arg1Type&& arg1, RestType&&... rest)
            {
                bind<Pos>(std::forward<Arg1Type>(arg1));
                bind_all_impl<Pos + 1>(std::forward<RestType>(rest)...);
            }

            template <size_t Pos, typename Arg1Type>
            void bind_all_impl(Arg1Type&& arg1)
            {
                bind<Pos>(std::forward<Arg1Type>(arg1));
            }

            template <std::uint32_t Pos>
            void* get_arg()
            {
                return &(std::get<Pos>(_args));
            }

            template <std::uint32_t Pos>
            bool should_move()
            {
                using expected_type = orig_type<Pos>;
                using is_rvalue_ref = typename std::is_rvalue_reference<expected_type>::type;
                return is_rvalue_ref();
            }

            // friend class
            template <typename RT>
            friend class cputask_return_layer;

        protected:
            /// Returns a string describing the task arguments.
            ///
            /// @return
            ///   std::string -- String describing the task arguments.
            std::string describe_arguments()
            {
                std::string description = "(";
                description += demangler::demangle<Arg1, Rest...>();
                description += ")";
                return description;
            };

            using state_snapshot = typename parent::state_snapshot;

            /// Constructor
            /// @param g Group the task belongs to
            /// @param a Initial task attributes
            cputask_arg_layer(group* g, legacy::task_attrs a)
                : parent(g, a), _args(), _set_arg_tracker(arity::value), _lock_buffers(true), _preacquired_arenas()
            {
            }

            /// Constructor
            /// @param g Group the task belongs to
            /// @param a Initial task attributes
            cputask_arg_layer(state_snapshot initial_state, group* g, legacy::task_attrs a)
                : parent(initial_state, g, a), _args(), _set_arg_tracker(arity::value), _lock_buffers(true), _preacquired_arenas()
            {
            }

            /// Constructor. Used when we know the arguments at construction time.
            ///
            /// @param g Group the task belongs to
            /// @param a Initial task attributes
            /// @param arg1 First task argument.
            /// @param rest Rest of the task arguments...
            template <typename Arg1Type, typename... RestType>
            cputask_arg_layer(state_snapshot initial_state, group* g, legacy::task_attrs a, Arg1Type&& arg1, RestType&&... rest)
                : parent(initial_state, g, a),
                  _args(),
                  _set_arg_tracker(set_arg_tracker::do_not_track()),
                  _lock_buffers(true),
                  _preacquired_arenas()
            {
                static_assert(sizeof...(RestType) + 1 == arity::value, "Invalid number of arguments");

                // @todo: this set_args is a hack. Ideally, we would like to create the arguments
                // directly in the args container, and consider the task bound from the beginning.
                // Notice that we are now default-constructing the args first, then setting their
                // values.
                bind_all_impl<0, Arg1Type, RestType...>(std::forward<Arg1Type>(arg1), std::forward<RestType>(rest)...);
            }

            template <typename Body>
            void prepare_args(Body& b)
            {
                HETCOMPUTE_INTERNAL_ASSERT(_set_arg_tracker.are_all_set() == true, "Arguments are not ready");
                expand_args<Body>(b, typename create_integer_sequence<arity::value>::type());
            }

            // Prepares arguments that the body needs. Because this class
            // template deals with bodies with no arguments, we don't have
            // anything to do here. We just call execute_and_store_retval
            // in the parent class.
            template <typename Body>
            ::hetcompute::task_ptr<ReturnType> prepare_args_but_do_not_store_retval(Body& b)
            {
                HETCOMPUTE_INTERNAL_ASSERT(_set_arg_tracker.are_all_set() == true, "Arguments are not ready");
                return expand_args_but_do_not_store_retval<Body>(b, typename create_integer_sequence<arity::value>::type());
            }

            void destroy_args() { destroy_args_impl<0>(std::false_type()); }

            buffer_acquire_set_t acquire_buffers()
            {
                buffer_acquire_set_t bas;
                if (!_lock_buffers)
                {
                    bas.enable_non_locking_buffer_acquire();
                }

                // Pass 1: construct bas, do not dispatch buffer arguments
                acquire_buffers_impl<0>(bas, std::false_type(), true);

                bas.blocking_acquire_buffers(this,
                                             { hetcompute::internal::executor_device::cpu },
                                             _preacquired_arenas.has_any() ? &_preacquired_arenas : nullptr);

                // Pass 2: dispatch buffer arguments
                acquire_buffers_impl<0>(bas, std::false_type(), false);

                return bas;
            }

        public:
            void release_buffers(buffer_acquire_set_t& bas) {
                bas.release_buffers(this);
              }

              /// Destructor.
              virtual ~cputask_arg_layer() {
                // @TODO Will be ever need to destroy args before the task is over?
              };

              ///
              /// Bind all the arguments
              /// @param arg1  the first argument
              /// @param rest  the rest of the arguments
              ///
              template <typename Arg1Type, typename... RestType>
              void bind_all(Arg1Type&& arg1, RestType&&... rest)
              {
                  static_assert(sizeof...(RestType) + 1 == arity::value, "Invalid number of arguments");

                  set_arg_tracker::size_type error_pos;
                  bool                       success;

                  // try to mark all arguments as ready
                  std::tie(success, error_pos) = _set_arg_tracker.set_all(arity::value);

                  // if it's not successful, that means that at least one of the arguments was already set.
                  HETCOMPUTE_API_THROW(success, "Argument %d was already set", error_pos);

                  // traverse all arguments and set their args
                  bind_all_impl<0, Arg1Type, RestType...>(std::forward<Arg1Type>(arg1), std::forward<RestType>(rest)...);

                  // update task state
                  task::set_bound();
              }

              // refer to class internal::task
              void unsafe_enable_non_locking_buffer_acquire() { _lock_buffers = false; }

              // refer to class internal::task
              void unsafe_register_preacquired_arena(bufferstate* bufstate, arena* preacquired_arena)
              {
                  _preacquired_arenas.register_preacquired_arena(bufstate, preacquired_arena);
              }

          protected:
              // Whether the buffers in the buffer-acquire-set should be locked/unlocked
              // during acquire/release. Default =true.
              bool _lock_buffers;

              // arenas that have been pre-acquired for some buffers.
              preacquired_arenas<true, num_buffer_args> _preacquired_arenas;

        };  // class cputask_arg_layer<ReturnType(Arg1, Rest...)>

        /// Stores the body of the task. Notice that we will never use an
        /// object of this class directly. We will only use pointers to parent
        /// classes.
        template <typename TaskTypeInfo>
        class cputask : public cputask_arg_layer<typename TaskTypeInfo::final_signature>
        {
            using task_info           = TaskTypeInfo;
            using user_code_container = typename task_info::container;
            using parent              = cputask_arg_layer<typename task_info::final_signature>;

            user_code_container _user_code_container;

        protected:
            using state_snapshot = typename parent::state_snapshot;

        public:
            // Constructor.
            // @param code Code that the task will execute.
            // @param g Group the task will belong to.
            // @param a Initial task attributes
            // @param Args... args to initialize the body with
            template <typename UserCode, typename... Args>
            cputask(state_snapshot initial_state, group* g, legacy::task_attrs a, UserCode&& user_code, Args&&... args)
                : parent(initial_state, g, a, std::forward<Args>(args)...), _user_code_container(std::forward<UserCode>(user_code))
            {
            }

            // Constructor.
            // @param body Code that the task will execute.
            // @param g Group the task will belong to.
            // @param a Initial task attributes
            // @param Args... args to initialize the body with
            template <typename UserCode, typename... Args>
            cputask(group* g, legacy::task_attrs a, UserCode&& user_code, Args&&... args)
                : parent(g, a, std::forward<Args>(args)...), _user_code_container(std::forward<UserCode>(user_code))
            {
            }

            // Destructor
            virtual ~cputask() {}

            /// Returns a string that describes the body.
            /// @return
            /// std::string -- String that describes the task body.
            virtual std::string describe_body()
            {
                return strprintf("%s%s %s",
                                 parent::describe_return_value().c_str(),
                                 parent::describe_arguments().c_str(),
                                 _user_code_container.to_string().c_str());
            }

        private:
            /// Returns an id for the source used for logging
            /// @return
            /// uintptr_t Integer that identifies the body.
            virtual uintptr_t do_get_source() const { return _user_code_container.get_source(); }

            /// Executes the task body.
            virtual bool do_execute(task_bundle_dispatch* tbd)
            {
                HETCOMPUTE_UNUSED(tbd);

                auto bas                         = parent::acquire_buffers();
                auto release_buffers_scope_guard = make_scope_guard([this, &bas] { parent::release_buffers(bas); });

                // Notice that this layer cannot execute the task on its own
                // anymore. The reason is that the body might have arguments
                // or it might return some value. Thus, we pass the container
                // up to our parent, so it can pass the arguments to the body.

                // The way we execute depends on whether we need to do type
                // collapsing. The reason is that it affects the way we
                // deal with the returned data. For more info about
                // task collapsing, look into task_type_info
                do_execute_dispatch(typename task_info::collapse_actual());

                return true;
            }

            /// Calls the body destructor. We need this because we need to guarantee
            /// that bodies get destroyed exactly when we want. We can't just wait
            /// for the task destructor to call the body destructor because that
            /// might be too late.
            virtual void destroy_body_and_args()
            {
                _user_code_container.destroy_user_code();
                parent::destroy_args();
            }

            void do_execute_dispatch(std::true_type)
            {
                auto inner_task = parent::prepare_args_but_do_not_store_retval(_user_code_container);
                this->finish_after(c_ptr(inner_task), [this, inner_task] {
                    if (!inner_task->canceled())
                    {
                        parent::template store_retval(inner_task->copy_value());
                    }
                });
                // INNER_TASK MUST BE LAUNCHED
                inner_task->launch();
            }

            void do_execute_dispatch(std::false_type) { parent::prepare_args(_user_code_container); }

            // Disable all copying and movement
            HETCOMPUTE_DELETE_METHOD(cputask(cputask const&));
            HETCOMPUTE_DELETE_METHOD(cputask(cputask&&));
            HETCOMPUTE_DELETE_METHOD(cputask& operator=(cputask const&));
            HETCOMPUTE_DELETE_METHOD(cputask& operator=(cputask&&));
        }; // class cputask

    }; // namespace internal

}; // namespace hetcompute
