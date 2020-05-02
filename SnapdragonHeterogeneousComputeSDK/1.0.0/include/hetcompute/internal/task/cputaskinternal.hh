#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include <hetcompute/internal/compat/compiler_compat.h>
#include <hetcompute/internal/task/functiontraits.hh>
#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/util/demangler.hh>
#include <hetcompute/internal/util/memorder.hh>
#include <hetcompute/internal/util/templatemagic.hh>

namespace hetcompute
{
    template<typename ...Stuff> class task_ptr;
    template<typename ...Stuff> class task;

    // forward declaration of patterns
    namespace pattern
    {
        template<typename T1, typename T2>       class pfor;
        template<typename Reduce, typename Join> class preducer;
        template<typename Fn>                    class ptransformer;
        template<typename BinaryFn>              class pscan;
        template<typename Compare>               class psorter;

        template<typename IsBaseFn, typename BaseFn, typename SplitFn, typename MergeFn>
        class pdivide_and_conquerer;
    };

    namespace internal
    {
        ::hetcompute::internal::task* c_ptr(::hetcompute::task_ptr<>& t);
        ::hetcompute::internal::task* c_ptr(::hetcompute::task_ptr<> const& t);
        ::hetcompute::internal::task* c_ptr(::hetcompute::task<>*);

        // forward declaration
        template <typename ReturnType>
        class cputask_return_layer;

        namespace testing
        {
            class ArgTrackerTest;
            class TaskArgTypeInfoTest;
        }; // namespace testing

        /**
         * The following templates allow us to destroy the task body at the
         * moment we desire. This is important so that we can control the
         * lifetime of the variables captured by the lambda used as body,
         * for example.
         *
         * There are two specializations, one for task bodies with destructors
         * (lambdas, functors), and another one for bodies without them
         * (function pointers).
         *
         * The second template parameter is a boolean that indicates whether
         * the body has a destructor.
         */
        template <typename UserCode, bool>
        class user_code_container_base;

        /**
         * Specialization for bodies with destructors. Enables us to call
         * the destructor of the user_code whenever we want.
         */

        template <typename UserCode>
        class user_code_container_base<UserCode, true>
        {
        protected:
            using user_code_traits = function_traits<UserCode>;
            using user_code_type_in_task = typename user_code_traits::type_in_task;

            // Constructor. Copies the body into union.
            template <typename Code = UserCode>
            explicit user_code_container_base(Code&& code) : _user_code(std::forward<Code>(code))
            {
            }

            user_code_type_in_task& get_user_code() { return _user_code; }

        private:
            // C++11 allows union members with constructors and destructors.
            // We wrap the body in a union and explicitly call its destructor
            // whenever we want.
            union {
                user_code_type_in_task _user_code;
            };

        public:
            // Calls destructor on body object
            void destroy_user_code() { _user_code.~user_code_type_in_task(); }

            // Destructor. Does nothing.
            ~user_code_container_base() {}

            // Disable all copying and movement
            HETCOMPUTE_DELETE_METHOD(user_code_container_base(user_code_container_base const&));
            HETCOMPUTE_DELETE_METHOD(user_code_container_base(user_code_container_base&&));
            HETCOMPUTE_DELETE_METHOD(user_code_container_base& operator=(user_code_container_base const&));
            HETCOMPUTE_DELETE_METHOD(user_code_container_base& operator=(user_code_container_base&&));
        };

        /**
         * Specialization for bodies without destructors. i.e function
         * pointers.
         */
        template <typename UserCode>
        class user_code_container_base<UserCode, false>
        {
        protected:
            using user_code_traits       = function_traits<UserCode>;
            using user_code_type_in_task = typename user_code_traits::type_in_task;

            user_code_type_in_task& get_user_code() { return _user_code; }

        private:
            user_code_type_in_task _user_code;

        public:
            /**
             * Constructor
             */
            template <typename Code = UserCode>
            explicit user_code_container_base(Code&& user_code) : _user_code(user_code)
            {
            }

            ~user_code_container_base() {}

            /**
             * Calls destructor on body object. Does nothing
             * because the body does not have a destructor
             * that needs to be called at a certain moment
             */
            void destroy_user_code() {}

            // Disable all copying and movement
            HETCOMPUTE_DELETE_METHOD(user_code_container_base(user_code_container_base const&));
            HETCOMPUTE_DELETE_METHOD(user_code_container_base(user_code_container_base&&));
            HETCOMPUTE_DELETE_METHOD(user_code_container_base& operator=(user_code_container_base const&));
            HETCOMPUTE_DELETE_METHOD(user_code_container_base& operator=(user_code_container_base&&));
        };

        template <typename UserCode>
        class user_code_container : public user_code_container_base<UserCode, function_traits<UserCode>::has_destructor::value>
        {
            using parent = user_code_container_base<UserCode, function_traits<UserCode>::has_destructor::value>;

        public:
            using user_code_type = UserCode;
            using return_type    = typename function_traits<UserCode>::return_type;

            // Constructor.
            template <typename Code = UserCode>
            explicit user_code_container(Code&& body) : parent(std::forward<Code>(body))
            {
            }

            // Destructor.
            ~user_code_container() {}

            // Executes body.
            // @param args Body arguments.
            // @return
            // return_type Body return type.
            template <typename... UserCodeArgs>
            return_type operator()(UserCodeArgs&&... args)
            {
                return parent::get_user_code()(std::forward<UserCodeArgs>(args)...);
            }

            // Returns an id for the body, so that we can identify
            // the different task types in the loggers
            // @return
            // uintptr_t -- body typeid stored in an std::uintptr_t
            uintptr_t get_source() const
            {
#ifdef HETCOMPUTE_HAVE_RTTI
                return reinterpret_cast<uintptr_t>(&typeid(const_cast<user_code_container*>(this)->get_user_code()));
#else
                return 0;
#endif
            }

            // Returns a description of the task body
            // We don't have compiler support yet, so we use typeid
            // to get a description of the body
            // @return
            // std::string -- String representation of the body's typeid.
            std::string to_string()
            {
#ifdef HETCOMPUTE_HAVE_RTTI
                return demangler::demangle(typeid(parent::get_user_code()));
#else
                return std::string("Enable RTTI for demangling class name.");
#endif
            }

            // Disable all copying and movement
            HETCOMPUTE_DELETE_METHOD(user_code_container(user_code_container const&));
            HETCOMPUTE_DELETE_METHOD(user_code_container(user_code_container&&));
            HETCOMPUTE_DELETE_METHOD(user_code_container& operator=(user_code_container const&));
            HETCOMPUTE_DELETE_METHOD(user_code_container& operator=(user_code_container&&));
        };

        /// This class keeps track of which arguments have been set in the
        /// argument layer. We need this to make sure that the same argument
        /// does not get set twice. Or that we call the task body with some of
        /// its arguments missing.  Notice that we could just use a counter,
        /// or perhaps just piggyback on the predecessor counter in the task
        /// state. Unfortunately, we need to be able to tell the user which
        /// argument is not set, so we need something else other than a
        /// counter.
        class set_arg_tracker
        {
        public:
            using size_type = std::uint32_t;

        private:
            using mask_type = std::uint32_t;
            using min_arity = std::integral_constant<mask_type, 1>;

            // Use this size_type as a bitmap. Each bit represents one argument.
            std::atomic<mask_type> _args;

            // Class used for testing this class
            friend class testing::ArgTrackerTest;

        public:
            using do_not_track = bool;
            using max_arity    = std::integral_constant<mask_type, (sizeof(mask_type) * 8) - 1>;

            /// Constructor.
            /// @param arity Number of arguments to keep track of.
            explicit set_arg_tracker(size_type arity) : _args((1 << arity) - 1)
            {
                HETCOMPUTE_INTERNAL_ASSERT(arity >= min_arity::value, "Arity should be greated than %d.", static_cast<int>(min_arity::value));

                HETCOMPUTE_INTERNAL_ASSERT(arity <= max_arity::value,
                                         "Arity is larger than %d: %d.",
                                         static_cast<int>(max_arity::value),
                                         static_cast<int>(arity));
            }

            /// Constructor.
            /// @param arity Number of arguments to keep track of.
            explicit set_arg_tracker(do_not_track) : _args(0) {}

            /// Marks argument in position pos as being set.
            ///
            /// @param pos Number of argument to set.
            ///
            /// @return
            ///
            /// std::tuple<bool, bool>
            ///
            ///  -- The first element in the tuple is true if the method
            ///  successfully set the argument, and false otherwise.
            ///
            ///  -- The second element in the tuple is true if all the arguments
            ///  have been set, and false otherwise
            std::tuple<bool, bool> set(size_type pos)
            {
                HETCOMPUTE_INTERNAL_ASSERT(pos <= max_arity::value, "Out of range pos: %d", pos);

                const mask_type mask = ~(1 << (pos));

                mask_type desired;
                mask_type expected = _args.load(hetcompute::mem_order_relaxed);

                do
                {
                    desired = expected & mask;

                    if (desired == expected)
                    {
                        return std::make_tuple(false, desired == 0);
                    }

                } while (!std::atomic_compare_exchange_weak_explicit(&_args,
                                                                     &expected,
                                                                     desired,
                                                                     hetcompute::mem_order_acq_rel,
                                                                     hetcompute::mem_order_acquire));

                return std::make_tuple(true, desired == 0);
            }

            /// Marks all arguments as being set.
            ///
            /// @param arity Number of arguments to keep track of. Tasks don't
            /// store their arity in the arg_tracker to avoid increasing their
            /// size. Plus, arity is a compile-time variable known by
            /// the task, so it is easy to pass it here again
            ///
            /// @return
            ///
            /// std::tuple<bool, size_type>
            ///
            ///   -- The first element in the tuple is true if the method
            ///   successfully set the argument, and false otherwise.
            ///
            ///   -- If the first element of the tuple is true, the second
            ///   element stores the position of the first arg that was set
            ///   already. This is used to show a nicer error to the user.
            ///   Otherwise, it stores 0.
            std::tuple<bool, size_type> set_all(size_type arity)
            {
                mask_type expected = (1 << arity) - 1;
                mask_type desired  = 0;

                if (std::atomic_compare_exchange_strong_explicit(&_args,
                                                                 &expected,
                                                                 desired,
                                                                 hetcompute::mem_order_acq_rel,
                                                                 hetcompute::mem_order_acquire))
                {
                    return std::make_tuple(true, 0);
                }

                return std::make_tuple(false, get_first_set_arg(expected));
            }

            bool are_all_set() { return _args.load(hetcompute::mem_order_acquire) == 0; }

        private:
            // Finds the first argument set in mask
            //
            // @param mask
            // @return
            // size_type -- position of the first argument set in the mask
            static size_type get_first_set_arg(mask_type mask);
        };

        // We can't always store arguments in the task using the exact type the programmer
        // indicated in the body definition. using arg_containers we can have different
        // behaviors depending of wether the argument was declared as an lvalue reference,
        /// an rvalue reference or a non-reference.
        template <typename T>
        struct arg_container;

        /// This container manages arguments that are non references.
        template <typename T>
        struct arg_container
        {
            using type = T;

            static_assert(std::is_default_constructible<T>::value, "Task arguments types must be default_constructible.");
            static_assert(std::is_copy_assignable<T>::value, "Task argument must be copy_assignable.");
            static_assert(std::is_copy_constructible<T>::value, "Task argument must be copy_constructible");

            /// Default constructor.
            arg_container() : _arg() {}

            /// Constructor
            /// @param value const lvalue reference to the value of the argument. The container
            ///              will make a copy of it.
            explicit arg_container(type const& value) : _arg(value) {}

            /// Constructor
            /// @param value. Rvalue reference to the value of the argument. The container
            ///               will move it.
            explicit arg_container(type&& value) : _arg(std::move(value)) {}

            /// Copy assignment.
            /// @param value. const lvalue reference to the value of the argument. The container
            ///              will make a copy of it.
            arg_container& operator=(type const& value)
            {
                _arg = value;
                return *this;
            }

            /// Move assignment.
            /// @param value. Rvalue reference to the value of the argument. The container
            ///               will move it.
            arg_container& operator=(type&& value)
            {
                _arg = std::move(value);
                return *this;
            }

            /// Operator(). Returns a reference to the object stored in the class.
            /// @return
            /// type& -- reference to the object stored in the class
            /* implicit */ operator type&() { return _arg; }

            /// Return contained arg by lvalue
            type& get_larg() { return _arg; }

            /// Allows us to call the destructor earlier.
            void destroy() { _arg.~type(); }

            /// Destructor. Does nothing.
            ~arg_container() {}

            HETCOMPUTE_DELETE_METHOD(arg_container(arg_container const& other_arg));
            HETCOMPUTE_DELETE_METHOD(arg_container(arg_container&& other_arg));
            HETCOMPUTE_DELETE_METHOD(arg_container& operator=(arg_container const&));
            HETCOMPUTE_DELETE_METHOD(arg_container& operator=(arg_container&&));

        private:
            // Note that we are storing the type, which means that we'll
            // default construct it. The alternative would be to have a pointer
            // to it and do an extra allocation. At the end, we decided
            // to just go with the default constructor because we expect
            // people to pass around simpler types.

            union {
                type _arg;
            };
        };  // arg_container<T>

        // This container manages arguments that are rvalue references.
        // When the user requests an rvalue reference of type T as an argument,
        // we actually store and object of type T, and then we use move
        // semantics to pass it around.
        template <typename T>
        struct arg_container<T&&>
        {
            using type = T;

            static_assert(std::is_default_constructible<T>::value, "Task arguments types must be default_constructible.");
            static_assert(std::is_move_constructible<T>::value, "Task argument must be move_constructible.");
            static_assert(std::is_move_assignable<T>::value, "Task argument must be move_assignable.");

            /// Default constructor. T must be default_constructible
            arg_container() : _arg() {}

            /// Constructor
            /// @param value. Rvalue reference to the value of the argument. The container
            ///               will move it.
            explicit arg_container(type&& value) : _arg(std::move(value)) {}

            /// Move assignment.
            /// @param value. Rvalue reference to the value of the argument. The container
            ///               will move it.
            arg_container& operator=(type&& value)
            {
                _arg = std::move(value);
                return *this;
            }

            /// Operator(). Returns an rvalue reference to the argument stored
            /// within the class. The object in the class will move.
            /// @return
            /// type& -- rvalue reference to the object stored in the class
            /* implicit */ operator type &&() { return std::move(_arg); }

            /// Return contained arg by lvalue
            type& get_larg() { return _arg; }

            /// Allows us to call the destructor earlier.
            void destroy() { _arg.~type(); }

            /// Destructor. Does nothing.
            ~arg_container() {}

        private:
            union {
                type _arg;
            };

            HETCOMPUTE_DELETE_METHOD(arg_container(type const&));
            HETCOMPUTE_DELETE_METHOD(arg_container(arg_container const& other_arg));
            HETCOMPUTE_DELETE_METHOD(arg_container(arg_container&& other_arg));
            HETCOMPUTE_DELETE_METHOD(arg_container& operator=(arg_container const&));
            HETCOMPUTE_DELETE_METHOD(arg_container& operator=(arg_container&&));
        }; //  arg_container<T&&>

        // We can't always re-store the return values using the exact same type the programmer
        // indicated in the body definition. Using return_type_containers we can properly
        // handle the const-ness of the value.

        /// This container manages the const-ness of return types.
        template<typename T>
        struct retval_container
        {
            using orig_type = T;
            using type      = typename std::remove_cv<T>::type;

            static_assert(!std::is_reference<T>::value, "Task return types cannot be references.");
            static_assert(!std::is_volatile<T>::value, "Task return types cannot be volatile.");
            static_assert(std::is_move_constructible<type>::value, "Task return types must be move_constructible.");
            static_assert(std::is_move_assignable<type>::value, "Task return types must be move_assignable.");

            /// Default constructor.
            retval_container() : _retval() {}

            /// Constructor
            /// @param value Lvalue ref to the return value.
            explicit retval_container(type const& value) : _retval(value) {}

            /// Constructor
            /// @param value Rvalue ref to the return value.
            explicit retval_container(type&& value) : _retval(std::move(value)) {}

            /// Copy assignment.
            /// @param value Lvalue ref to the return value.
            retval_container& operator=(type const& value)
            {
                _retval = value;
                return *this;
            }

            /// Move assignment.
            /// @param value Rvalue ref to the return value.
            retval_container& operator=(type&& value)
            {
                _retval = std::move(value);
                return *this;
            }

            /// Destructor. Does nothing.
            ~retval_container() {}

            HETCOMPUTE_DELETE_METHOD(retval_container(retval_container const& other_arg));
            HETCOMPUTE_DELETE_METHOD(retval_container(retval_container&& other_arg));
            HETCOMPUTE_DELETE_METHOD(retval_container& operator=(retval_container const&));
            HETCOMPUTE_DELETE_METHOD(retval_container& operator=(retval_container&&));

        private:
            type _retval;

            template <typename ReturnType>
            friend class cputask_return_layer;
        };  // struct retval_container<T>

        /// This structure stores information about the arguments passed
        /// to a task.
        template <class T>
        struct task_arg_type_info
        {
            // This is the original type, as requested by the user
            using orig_type = T;

            // no_ref_type is the type requested by the user, without any references
            using no_ref_type = typename std::remove_reference<orig_type>::type;
            using no_ref_no_cv_type = typename std::remove_cv<no_ref_type>::type;

            // Decayed type.
            using decayed_type = typename std::decay<orig_type>::type;

            // std::true_type if T is lvalue
            using is_lvalue_ref = typename std::is_lvalue_reference<orig_type>::type;

            // std::true_type if T is rvalue
            using is_rvalue_ref = typename std::is_rvalue_reference<orig_type>::type;

            // Final type that will be used to store the task
            using storage_type =
                typename std::conditional<is_rvalue_ref::value, arg_container<decayed_type&&>, arg_container<decayed_type>>::type;

            static_assert(is_lvalue_ref::value == false,
                          "This version of HetCompute does not support lvalue references as task arguments. "
                          "Please check the HetCompute manual.");

            static_assert(std::is_default_constructible<decayed_type>::value, "Task arguments types must be default_constructible.");

            using valid_rvalue_ref = typename std::conditional<
                is_rvalue_ref::value,
                typename std::conditional<std::is_move_constructible<no_ref_type>::value && std::is_move_assignable<no_ref_type>::value,
                                          std::true_type,
                                          std::false_type>::type,
                std::true_type>::type;

            static_assert(valid_rvalue_ref::value, "Rvalue reference arguments must be move constructible and move assignable.");

            using valid_lvalue = typename std::conditional<
                is_rvalue_ref::value == false,
                typename std::conditional<std::is_copy_constructible<decayed_type>::value && std::is_copy_assignable<decayed_type>::value,
                                          std::true_type,
                                          std::false_type>::type,
                std::true_type>::type;

            static_assert(valid_lvalue::value, "lvalue arguments must be copy constructible and copy assignable.");

            using is_valid = typename std::conditional<(is_lvalue_ref::value == false && valid_rvalue_ref::value && valid_lvalue::value),
                                                       std::true_type,
                                                       std::false_type>::type;

            static_assert(!std::is_volatile<orig_type>::value, "This version of hetcompute does not support volatile task arguments.");
        };

        /// The following templates help checking that the types are correct and allow us
        /// to give more meaninful error messages.
        template <typename ArgTuple, std::size_t pos>
        struct check_arg
        {
            using orig_type = typename std::tuple_element<pos, ArgTuple>::type;
            using type_info = task_arg_type_info<orig_type>;
            using is_valid =
                typename std::conditional<type_info::is_valid::value, typename check_arg<ArgTuple, pos - 1>::is_valid, std::false_type>::type;
        };

        template <typename ArgTuple>
        struct check_arg<ArgTuple, 0>
        {
            using orig_type = typename std::tuple_element<0, ArgTuple>::type;
            using type_info = task_arg_type_info<orig_type>;
            using is_valid  = typename type_info::is_valid;
        };

        template <typename ArgsTuple, std::size_t arity>
        struct check_body_args
        {
            using all_valid = typename check_arg<ArgsTuple, arity - 1>::is_valid;
        };

        template <typename ArgsTuple>
        struct check_body_args<ArgsTuple, 0>
        {
            using all_valid = std::true_type;
        };

        // Existing code

        /// The following structures help testing whether a type
        /// is a task pointer
        template <typename T>
        struct is_hetcompute_task_ptr
        {
            using type                                              = T;
            static HETCOMPUTE_CONSTEXPR_CONST bool value            = false;
            static HETCOMPUTE_CONSTEXPR_CONST bool has_return_value = false;
            static HETCOMPUTE_CONSTEXPR_CONST bool has_arguments    = false;
        };

        template <typename T>
        struct is_hetcompute_task_ptr<::hetcompute::task_ptr<T>>
        {
            using type                                            = T;
            static HETCOMPUTE_CONSTEXPR_CONST bool value            = true;
            static HETCOMPUTE_CONSTEXPR_CONST bool has_return_value = true;
            static HETCOMPUTE_CONSTEXPR_CONST bool has_arguments    = false;
        };

        template <>
        struct is_hetcompute_task_ptr<::hetcompute::task_ptr<>>
        {
            using type                                            = void;
            static HETCOMPUTE_CONSTEXPR_CONST bool value            = true;
            static HETCOMPUTE_CONSTEXPR_CONST bool has_return_value = false;
            static HETCOMPUTE_CONSTEXPR_CONST bool has_arguments    = false;
        };

        template <typename R, typename... Args>
        struct is_hetcompute_task_ptr<::hetcompute::task_ptr<R(Args...)>>
        {
            using type                                            = R;
            static HETCOMPUTE_CONSTEXPR_CONST bool value            = true;
            static HETCOMPUTE_CONSTEXPR_CONST bool has_return_value = true;
            static HETCOMPUTE_CONSTEXPR_CONST bool has_arguments    = true;
        };

        template <typename T>
        struct is_hetcompute_task_ptr<::hetcompute::task<T>*>
        {
            using type                                            = T;
            static HETCOMPUTE_CONSTEXPR_CONST bool value            = true;
            static HETCOMPUTE_CONSTEXPR_CONST bool has_return_value = true;
            static HETCOMPUTE_CONSTEXPR_CONST bool has_arguments    = false;
        };

        template <>
        struct is_hetcompute_task_ptr<::hetcompute::task<>*>
        {
            using type                                            = void;
            static HETCOMPUTE_CONSTEXPR_CONST bool value            = true;
            static HETCOMPUTE_CONSTEXPR_CONST bool has_return_value = false;
            static HETCOMPUTE_CONSTEXPR_CONST bool has_arguments    = false;
        };

        template <typename R, typename... Args>
        struct is_hetcompute_task_ptr<::hetcompute::task<R(Args...)>*>
        {
            using type                                            = R;
            static HETCOMPUTE_CONSTEXPR_CONST bool value            = true;
            static HETCOMPUTE_CONSTEXPR_CONST bool has_return_value = true;
            static HETCOMPUTE_CONSTEXPR_CONST bool has_arguments    = true;
        };

        template <typename F>
        struct is_pattern
        {
            static HETCOMPUTE_CONSTEXPR_CONST bool value = false;
        };

        template <typename UnaryFn>
        struct is_pattern<::hetcompute::pattern::pfor<UnaryFn, void>>
        {
            static HETCOMPUTE_CONSTEXPR_CONST bool value = true;
        };

        template <typename Fn>
        struct is_pattern<::hetcompute::pattern::ptransformer<Fn>>
        {
            static HETCOMPUTE_CONSTEXPR_CONST bool value = true;
        };

        template <typename BinaryFn>
        struct is_pattern<::hetcompute::pattern::pscan<BinaryFn>>
        {
            static HETCOMPUTE_CONSTEXPR_CONST bool value = true;
        };

        template <typename Compare>
        struct is_pattern<::hetcompute::pattern::psorter<Compare>>
        {
            static HETCOMPUTE_CONSTEXPR_CONST bool value = true;
        };

        template <typename IsBaseFn, typename BaseFn, typename SplitFn, typename MergeFn>
        struct is_pattern<::hetcompute::pattern::pdivide_and_conquerer<IsBaseFn, BaseFn, SplitFn, MergeFn>>
        {
            static HETCOMPUTE_CONSTEXPR_CONST bool value = true;
        };

        template <typename F, typename Enable = void>
        struct is_group_launchable
        {
            static HETCOMPUTE_CONSTEXPR_CONST bool value = true;
        };

        template <typename Reduce, typename Join>
        struct is_group_launchable<::hetcompute::pattern::preducer<Reduce, Join>>
        {
            static HETCOMPUTE_CONSTEXPR_CONST bool value = false;
        };

        template <typename IsBaseFn, typename BaseFn, typename SplitFn, typename MergeFn>
        struct is_group_launchable<::hetcompute::pattern::pdivide_and_conquerer<IsBaseFn, BaseFn, SplitFn, MergeFn>,
                                   typename std::enable_if<!std::is_same<typename internal::function_traits<BaseFn>::return_type, void>::value>::type>
        {
            static HETCOMPUTE_CONSTEXPR_CONST bool value = false;
        };

        /// These templates set up the type when doing signature collapsing.
        template <typename NewReturnType, typename ReturnType, typename... Args>
        struct substitute_return_type;

        template <typename NewReturnType, typename ReturnType, typename... Args>
        struct substitute_return_type<NewReturnType, ReturnType(Args...)>
        {
            typedef NewReturnType type(Args...);
        };

        /// The following class offers static information about the task.  The
        /// first template argument (UserCode) represents the lambda, function
        /// pointer or functor that will execute when the tasks executes. The
        /// second argument indicates whether the programmer requested
        /// type collapsing at the time it created the task.
        ///
        /// What's task collapsing? Suppose that we have the following
        /// lambda:
        /// auto l = []() -> task<int> { ... }
        /// That is, a lambda that returns a task that returns int.
        ///
        /// Suppose now that we create a task with such lambda: auto t =
        /// hetcompute::create_task(l); Without collapsing, the type of t would be:
        /// task<task<int>> With type collapsing, the type of t would be
        /// task<int>.
        template <typename UserCode, bool CollapseRequested>
        struct task_type_info
        {
            using user_code          = UserCode;
            using user_code_traits   = function_traits<UserCode>;
            using collapse_requested = typename std::integral_constant<bool, CollapseRequested>;
            using collapse_actual =
                typename std::integral_constant<bool, CollapseRequested && is_hetcompute_task_ptr<typename user_code_traits::return_type>::value>;
            using collapsed_return_type =
                typename std::conditional<collapse_actual::value,
                                          typename is_hetcompute_task_ptr<typename user_code_traits::return_type>::type,
                                          typename user_code_traits::return_type>::type;

            // This is the place where we do the type collapsing:
            // task<task<T>> will be collapsed into task<T>
            using collapsed_signature = typename substitute_return_type<collapsed_return_type, typename user_code_traits::signature>::type;
            using final_signature =
                typename std::conditional<collapse_actual::value, collapsed_signature, typename user_code_traits::signature>::type;

            using container = user_code_container<UserCode>;
            using size_type = std::size_t;

            static_assert(std::is_reference<typename user_code_traits::return_type>::value == false,
                          "This version of HetCompute does not support references as the return type of tasks."
                          "Please check the HetCompute manual.");

            static_assert(user_code_traits::arity::value < set_arg_tracker::max_arity::value,
                          "Task body has too many arguments. Please refer to the manual.");

            using args_tuple     = typename user_code_traits::args_tuple;
            using args_arity     = typename user_code_traits::arity;
            using args_all_valid = typename check_body_args<args_tuple, args_arity::value>::all_valid;

            static_assert(args_all_valid::value, "Task body has illegal argument types.");
        }; // struct task_type_info<Usercode, CollapseRequested>

        // return template type for hetcompute::by_value()
        template <typename TaskType>
        struct by_value_t
        {
            explicit by_value_t(TaskType&& t) : _t(std::forward<TaskType>(t)){};

            TaskType&& _t;
        };

        // helper template to check if type T is a hetcompute::internal::by_value_t
        template <typename T>
        struct is_by_value_helper_t
        {
            static HETCOMPUTE_CONSTEXPR_CONST bool value = false;
            using type                                 = T;
        };

        template <typename T>
        struct is_by_value_helper_t<by_value_t<T>>
        {
            static HETCOMPUTE_CONSTEXPR_CONST bool value = true;
            using type                                 = T;
        };

        // check whether T is a hetcompute::internal::by_value_t and store type T
        template <typename T>
        struct is_by_value_t
        {
        private:
            using no_ref = typename std::remove_reference<T>::type;

        public:
            using helper                               = typename ::hetcompute::internal::is_by_value_helper_t<no_ref>;
            static HETCOMPUTE_CONSTEXPR_CONST bool value = helper::value;
            using type                                 = typename std::conditional<value, typename helper::type, T>::type;
        };

        ///
        /// return template type for hetcompute::by_data_dependency()
        ///
        template <typename TaskType>
        struct by_data_dep_t
        {
            explicit by_data_dep_t(TaskType&& t) : _t(std::forward<TaskType>(t)){};

            TaskType&& _t;
        };

        // helper template to check if type T is a hetcompute::internal::by_data_dep_t
        template <typename T>
        struct is_by_data_dep_helper_t
        {
            static HETCOMPUTE_CONSTEXPR_CONST bool value = false;
            using type                                 = T;
        };

        template <typename T>
        struct is_by_data_dep_helper_t<by_data_dep_t<T>>
        {
            static HETCOMPUTE_CONSTEXPR_CONST bool value = true;
            using type                                 = T;
        };

        // check whether T is a hetcompute::internal::by_data_dep_t and store type T
        template <typename T>
        struct is_by_data_dep_t
        {
        private:
            using no_ref = typename std::remove_reference<T>::type;

        public:
            using helper                               = typename ::hetcompute::internal::is_by_data_dep_helper_t<no_ref>;
            static HETCOMPUTE_CONSTEXPR_CONST bool value = helper::value;
            using type                                 = typename std::conditional<value, typename helper::type, T>::type;
        };

        // Check if the provided argument can be used as a value
        // UserProvided is the type of the provided argument
        // Expected is the expected argument type which is defined when the task is created
        template <typename UserProvided, typename Expected>
        class can_bind_as_value
        {
        private:
            static void test(Expected);

            template <typename U>
            static auto check(int) -> decltype(test(std::declval<U>()), std::true_type());

            template <typename>
            static std::false_type check(...);

            using enable_if_type = decltype(check<UserProvided>(0));

        public:
            using type                                 = typename enable_if_type::type;
            static HETCOMPUTE_CONSTEXPR_CONST bool value = type::value;

        }; // can_bind_as_value

        // Check if the provided argument can be used as data dependency
        // UserProvided is the type of the provided argument
        // Expected is the expected argument type which is defined when the task is created
        template <typename PredType, typename Expected, typename Enabled = void>
        struct can_bind_as_data_dep
        {
            static HETCOMPUTE_CONSTEXPR_CONST bool value = false;
        };

        template <typename PredType, typename Expected>
        struct can_bind_as_data_dep<PredType,
                                    Expected,
                                    typename std::enable_if<is_hetcompute_task_ptr<
                                        typename std::remove_cv<typename std::remove_reference<PredType>::type>::type>::has_return_value>::type>
        {
        private:
            using pred_type_nocvref = typename std::remove_cv<typename std::remove_reference<PredType>::type>::type;
            using return_type       = typename ::hetcompute::internal::is_hetcompute_task_ptr<pred_type_nocvref>::type;

        public:
            static HETCOMPUTE_CONSTEXPR_CONST bool value =
                (std::is_same<void, return_type>::value == false) && can_bind_as_value<return_type, Expected>::value;

        }; // can_bind_as_data_dep

        template <typename ArgType>
        static ::hetcompute::internal::task* get_cptr(ArgType* arg)
        {
            using decayed_argtype = typename std::decay<ArgType*>::type;
            static_assert(is_hetcompute_task_ptr<decayed_argtype>::value, "");

            return ::hetcompute::internal::c_ptr(static_cast<::hetcompute::task<>*>(arg));
        }

        template <typename ArgType>
        static ::hetcompute::internal::task* get_cptr(ArgType&& arg)
        {
            using decayed_argtype = typename std::decay<ArgType>::type;
            static_assert(is_hetcompute_task_ptr<decayed_argtype>::value, "");

            return ::hetcompute::internal::c_ptr(arg);
        }

    }; // namespace internal

}; // namespace hetcompute
