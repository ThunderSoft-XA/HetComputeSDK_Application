#pragma once

#if defined(HETCOMPUTE_HAVE_QTI_DSP)

#include <string>
#include <tuple>
#include <typeinfo>
#include <type_traits>

// Include external headers first
#include <hetcompute/exceptions.hh>
#include <hetcompute/taskptr.hh>

// Include internal headers next

#include <hetcompute/internal/task/dsptraits.hh>
#include <hetcompute/internal/task/task.hh>
#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/util/demangler.hh>
#include <hetcompute/internal/util/strprintf.hh>

// This code is most easily comprehended if read from bottom to top.

// forward declarations
namespace hetcompute
{
    template <typename... Stuff>
    class task_ptr;

    namespace internal
    {
        // This class template takes care of the arguments in the task body.
        // The task is templated on task argument types to transform buffers

        // forward declaration
        class group;
        class task_bundle_dispatch;

        // We split the functionality of the dsp tasks in three layers to mimic the cpu tasks:
        // (from bottom to top)
        //
        // - Return layer: This class template stores the return
        //                 value (if any) of the task.
        // - Argument layer: This class template transforms the task arguments from the hetcompute
        //                 side to the IDL side; namely, buffers arguments are replaced by
        //                 their initial address and size.
        // - Body layer: This class template stores the actual body of the task.
        //
        // Notice that:
        //
        // - dsptask inherits from dsptask_argument_layer
        // - dsptask_argument_layer inherits from dsptask_return_layer
        // - dsptask_return_layer inherits from task
        //
        // The constructors to dsptask_return_layer and dsptask_argument_layer
        // are protected because construction of a dsptask always requires
        // a dsptask.
        //

        /// This class template stores the return value of the
        /// task.
        /// As of now, we only support void return types
        /// The template exists to provide support in the future implementation
        /// of dataflow with dsp
        class dsptask_return_layer : public task
        {
        public:
            uint64_t get_exec_time() const { return _exec_time; }

            void set_exec_time(uint64_t elapsed_time) { _exec_time = elapsed_time; }

            /// Destructor. In this version, it will always call the destructor
            /// of the return value.
            virtual ~dsptask_return_layer(){};

        protected:
            /// Constructor.
            ///
            /// @param g Group the task belongs to
            /// @param a Initial task attributes
            /// @param args Needed to initialize retval (only used for value tasks)
            explicit dsptask_return_layer(state_snapshot initial_state, group* g, ::hetcompute::internal::legacy::task_attrs a)
                : task(initial_state, g, a), _lock_buffers(true), _exec_time(0)
            {
            }

            /// Constructor
            ///
            /// @param g Group the task belongs to
            /// @param a Initial task attributes
            explicit dsptask_return_layer(group* g, ::hetcompute::internal::legacy::task_attrs a) : task(g, a) {}

            // Describes return_type.
            // @return
            //  std::string -- A string with the description of the return type.
            std::string describe_return_value() { return demangler::demangle<void>(); };

            // refer to class internal::task
            void unsafe_enable_non_locking_buffer_acquire() { _lock_buffers = false; }

            void handleError(int status);

            // Calls fn() without arguments
            template <typename Fn, typename PreacquiredArenas>
            void execute_and_store_retval(Fn& fn, const void*, PreacquiredArenas const*)
            {
                int status = fn();
                if (status != 0)
                {
                    handleError(status);
                }
            }

            // Calls fn(args...) dsp kernel
            //
            // @param fn Dsp kernel to execute.
            // @param args Arguments to be pass to fn
            //
            template <typename Fn, typename PreacquiredArenas, typename... TaskArgs>
            void execute_and_store_retval(Fn& fn, const void* requestor, PreacquiredArenas const* p_preacquired_arenas, TaskArgs&&... args)
            {
                HETCOMPUTE_INTERNAL_ASSERT(requestor != nullptr, "The requestor should be not null");

                using tuple_args_type = std::tuple<TaskArgs...>;

                using args_buffers_acquire_set_type = buffer_acquire_set<num_buffer_ptrs_in_tuple<tuple_args_type>::value>;

                tuple_args_type args_in_tuple(args...);

                static_assert(std::tuple_size<std::tuple<TaskArgs...>>::value > 0, "Argument tuple should contain at least 1 element");

                args_buffers_acquire_set_type bas;
                if (!_lock_buffers)
                    bas.enable_non_locking_buffer_acquire();

                // Insert all buffers into the buffer acquire set
                parse_and_add_buffers_to_acquire_set<args_buffers_acquire_set_type, tuple_args_type, 0, Fn, 0, false>
                    parsed_params(bas, args_in_tuple);

                // TODO This call will block one thread from the dsp thread pool
                // We could think in better ways of yielding and running other task instead
                bas.blocking_acquire_buffers(requestor, { hetcompute::internal::executor_device::dsp }, p_preacquired_arenas);

                // Translate buffers and acquire the arenas
                // The two step process is required because until the buffer is acquired, we cannot
                // guarantee that the corresponding arena has valid data
                translate_TP_args_to_dsp<Fn, 0, tuple_args_type, 0, args_buffers_acquire_set_type, 0 == sizeof...(TaskArgs)>
                    dsp_args_container(args_in_tuple, bas);

                // _htp_till_index contains the tuple of arguments already converted
                auto& dsp_args = dsp_args_container._htp_till_index;

                // These arguments have been already converted from the HetCompute side to the IDL side
                int status = apply(fn, dsp_args);

                // release buffers
                bas.release_buffers(requestor);

                if (status != 0)
                {
                    handleError(status);
                }
            }

        private:
            // Whether the buffers in the buffer-acquire-set should be locked/unlocked
            // during acquire/release. Default =true.
            bool _lock_buffers;

            // Record execution time when profile is enabled by tuner
            uint64_t _exec_time;

        }; // dsptask_return_layer

        template <typename... TaskArgs>
        class dsptask_arg_layer : public dsptask_return_layer
        {
            using parent = dsptask_return_layer;

            // Number of arguments in body
            // This number can be larger than the arity of TaskArgs because buffers are
            // translated into two arguments
            using arity = std::integral_constant<std::uint32_t, sizeof...(TaskArgs)>;

            using storage_arg_types = std::tuple<typename std::remove_cv<typename std::remove_reference<TaskArgs>::type>::type...>;

            // The following structs (integer_sequence and
            // create_integer_sequence) will allow us to take a tuple, and pass
            // it as separate arguments to a function call.

            // This structure is similar to integer_sequence in C++14.
            // We don't use C++14 yet. As soon as we enable it in
            // our builds, we'll replace this with std::integer_sequence.
            template <std::uint32_t... numbers>
            struct integer_sequence
            {
                static constexpr size_t get_size() { return sizeof...(numbers); }
            };

            // The following structs creates an integer sequence from 0 to Top-1.
            // TODO This code may be moved to a shared taskfactorytraits.hh file
            template <std::uint32_t Top, std::uint32_t... Sequence>
            struct create_integer_sequence : create_integer_sequence<Top - 1, Top - 1, Sequence...>
            {
            };

            template <std::uint32_t... Sequence>
            struct create_integer_sequence<0, Sequence...>
            {
                using type = integer_sequence<Sequence...>;
            };

            template <typename DspKernel, uint32_t... Number>
            void expand_args(DspKernel& hk, const void* requestor, integer_sequence<Number...>)
            {
                // convert buffers and acquire
                parent::template execute_and_store_retval(hk.get_fn(),
                                                          requestor,
                                                          _preacquired_arenas.has_any() ? &_preacquired_arenas : nullptr,
                                                          std::move(std::get<Number>(_args))...);

                // release buffers
            }

            storage_arg_types _args;

            preacquired_arenas<true, num_buffer_ptrs_in_tuple<storage_arg_types>::value> _preacquired_arenas;

        protected:
            /// Returns a string describing the task arguments.
            ///
            /// @return
            ///   std::string -- String describing the task arguments.
            std::string describe_arguments()
            {
                std::string description = "(";
                description += demangler::demangle<TaskArgs...>();
                description += ")";
                return description;
            };

            using state_snapshot = typename parent::state_snapshot;

            /// Constructor. Used when we know the arguments at construction time.
            ///
            /// @tparam g Group the task belongs to
            /// @tparam a Initial task attributes
            /// @tparam args task arguments...
            dsptask_arg_layer(state_snapshot initial_state, group* g, ::hetcompute::internal::legacy::task_attrs a, TaskArgs&&... args)
                : parent(initial_state, g, a), _args(std::forward<TaskArgs>(args)...), _preacquired_arenas()
            {
            }

            template <typename DspKernel>
            void prepare_args(DspKernel& hk, void const* requestor)
            {
                expand_args<DspKernel>(hk, requestor, typename create_integer_sequence<arity::value>::type());
            }

        public:
            /// Destructor.
            virtual ~dsptask_arg_layer(){
                // @TODO Will be ever need to destroy args before the task is over?
            };

            // refer to class internal::task
            void unsafe_register_preacquired_arena(bufferstate* bufstate, arena* preacquired_arena)
            {
                _preacquired_arenas.register_preacquired_arena(bufstate, preacquired_arena);
            }

        }; // dsptask_arg_layer

        // Stores the body of the task. Notice that we will never use an
        // object of this class directly. We will only use pointers to parent
        // classes.
        template <typename DspKernel, typename... TaskArgs>
        class dsptask : public dsptask_arg_layer<TaskArgs...>
        {
            DspKernel _kernel;
            using parent      = dsptask_arg_layer<TaskArgs...>;
            using grandfather = dsptask_return_layer;

        protected:
            using state_snapshot = typename parent::state_snapshot;

        public:
            // Constructor.
            // @tparam body Dsp Code that the task will execute.
            // @param g Group the task will belong to.
            // @param a Initial task attributes
            // @tparam Args... Arguments for the Dsp Code
            dsptask(state_snapshot initial_state, group* g, ::hetcompute::internal::legacy::task_attrs a, DspKernel&& kernel, TaskArgs&&... args)
                : parent(initial_state, g, a, std::forward<TaskArgs>(args)...), _kernel(kernel)
            {
            }

            /// Destructor
            virtual ~dsptask(){};

            /// Returns a string that describes the body.
            /// @return
            /// std::string -- String that describes the task body.
            virtual std::string describe_body()
            {
                return strprintf("%s%s %" HETCOMPUTE_FMT_TID,
                                 parent::describe_return_value().c_str(),
                                 parent::describe_arguments().c_str(),
                                 do_get_source());
            };

            /**
             * Query dsp task execution time (if profile is enabled through hetcompute tuner)
             * @returns dsp task execution time
             */
            uint64_t get_exec_time() const { return grandfather::get_exec_time(); }

            /**
             * Set dsp task execution time (if profile is enabled through hetcompute tuner)
             * @param elapsed_time dsp task execution time.
             */
            void set_exec_time(uint64_t elapsed_time) { grandfather::set_exec_time(elapsed_time); }

        private:
            /// Returns an id for the source used for logging
            /// @return
            /// uintptr_t Integer that identifies the body.
            virtual uintptr_t do_get_source() const { return reinterpret_cast<uintptr_t>(const_cast<DspKernel&>(_kernel).get_fn()); }

            /// Executes the task body.
            virtual bool do_execute(task_bundle_dispatch* tbd)
            {
                HETCOMPUTE_UNUSED(tbd);
                parent::prepare_args(_kernel, this);

                auto start_time = this->get_exec_time();
                if (start_time != 0)
                {
                    uint64_t elapsed_time = hetcompute_get_time_now() - start_time;
                    this->set_exec_time(elapsed_time);
                }

                return true;
            }

            // Disable all copying and movement
            HETCOMPUTE_DELETE_METHOD(dsptask(dsptask const&));
            HETCOMPUTE_DELETE_METHOD(dsptask(dsptask&&));
            HETCOMPUTE_DELETE_METHOD(dsptask& operator=(dsptask const&));
            HETCOMPUTE_DELETE_METHOD(dsptask& operator=(dsptask&&));
        };

    }; // namespace internal
};     // namespace hetcompute

#endif // defined(HETCOMPUTE_HAVE_QTI_DSP)
