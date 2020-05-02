#pragma once

#include <tuple>

namespace hetcompute
{
    namespace internal
    {
        template <typename BAS, typename Param, typename Arg, bool IsBufferPtr>
        struct buffer_adder_helper
        {
            buffer_adder_helper(BAS&, Arg&&)
            {
                // Arg is not a buffer_ptr
            }
        };

        template <typename BAS, typename Param, typename Arg>
        struct buffer_adder_helper<BAS, Param, Arg, true>
        {
            buffer_adder_helper(BAS& bas, Arg&& arg)
            {
                static_assert(is_api20_buffer_ptr<Arg>::value, "argument was expected to be a buffer_ptr");

                if (arg == nullptr)
                {
                    return;
                }

                bas.add(arg,
                        std::conditional<is_const_buffer_ptr<Param>::value,
                                         std::integral_constant<bufferpolicy::action_t, bufferpolicy::acquire_r>,
                                         std::integral_constant<bufferpolicy::action_t, bufferpolicy::acquire_rw>>::type::value);
            }
        };

        // Suitable for adding a CPU function arg to buffer-acquire-set.
        // The access types are determined solely based on Param = buffer_ptr<T> vs buffer_ptr<const T>.
        template <typename BAS, typename Param, typename Arg>
        struct buffer_adder
        {
            buffer_adder(BAS& bas, Arg&& arg)
            {
                buffer_adder_helper<BAS, Param, Arg, is_api20_buffer_ptr<Param>::value> bah(bas, std::forward<Arg>(arg));
            }
        };

        ////

        template <typename BAS, typename ParamsTuple, size_t index, bool no_more_args, typename... Args>
        struct add_buffers_helper;

        template <typename BAS, typename ParamsTuple, size_t index, typename Arg, typename... Args>
        struct add_buffers_helper<BAS, ParamsTuple, index, false, Arg, Args...>
        {
            static void add(BAS& bas, Arg&& arg, Args&&... args)
            {
                bool constexpr next_no_more_args = (sizeof...(Args) == 0);
                buffer_adder<BAS, typename std::tuple_element<index, ParamsTuple>::type, Arg> ba(bas, std::forward<Arg>(arg));
                add_buffers_helper<BAS, ParamsTuple, index + 1, next_no_more_args, Args...>::add(bas, std::forward<Args>(args)...);
            }
        };

        template <typename BAS, typename ParamsTuple, size_t index, typename... Args>
        struct add_buffers_helper<BAS, ParamsTuple, index, true, Args...>
        {
            static void add(BAS&, Args&&...) {}
        };

        // Suitable for adding all buffer args of CPU function to buffer-acquire-set.
        // The access types are determined solely based on Param = buffer_ptr<T> vs buffer_ptr<const T>.
        template <typename BAS, typename ParamsTuple, typename... Args>
        void add_buffers(BAS& bas, Args&&... args)
        {
            bool constexpr no_more_args = (sizeof...(Args) == 0);
            add_buffers_helper<BAS, ParamsTuple, 0, no_more_args, Args...>::add(bas, std::forward<Args>(args)...);
        }

            ////

#ifdef HETCOMPUTE_HAVE_GPU
        // Captures a synchronous execution for later invocation. Limited to GPU kernels.
        template <typename GPUKernel, size_t Dims, typename... CallArgs>
        struct executor
        {
            using gpukernel_parent = typename GPUKernel::parent;
            using args_tuple       = std::tuple<CallArgs...>;
            using params_tuple     = typename GPUKernel::parent::params_tuple;

            GPUKernel                                                                                                             _gk;
            typename std::enable_if<std::tuple_size<args_tuple>::value == std::tuple_size<params_tuple>::value, args_tuple>::type _args_tuple;
            hetcompute::range<Dims> _global_range;
            hetcompute::range<Dims> _local_range;

            executor(GPUKernel const& gk, hetcompute::range<Dims> const& global_range, hetcompute::range<Dims> const& local_range, CallArgs&&... args)
                : _gk(gk), _args_tuple(std::forward<CallArgs>(args)...), _global_range(global_range), _local_range(local_range)
            {
                HETCOMPUTE_API_ASSERT(_gk.is_cl(), "Currently only OpenCL kernels are supported by the blocking execution API");
            }
        };

        ////

        // Processes the buffer arguments for a single executor.
        // Processing can involve two operations:
        //   - add buffers to a provided buffer-acquire-set
        //   - invoke the executor using the provided buffer-acquire-set
        // The two operations can be enabled/disabled in any combination
        // by passing two corresponding flags: add_buffers, perform_launch
        //
        // Currently limited to executors of GPU kernels.
        template <typename BAS, typename Executor>
        struct process_executor_buffers;

        template <typename BAS, typename GPUKernel, size_t Dims, typename... CallArgs>
        struct process_executor_buffers<BAS, executor<GPUKernel, Dims, CallArgs...>>
        {
            using executor_type = executor<GPUKernel, Dims, CallArgs...>;

            static void process(hetcompute::internal::executor_construct const& exec_cons,
                                BAS&                                          bas,
                                executor<GPUKernel, Dims, CallArgs...> const& exec,
                                bool                                          add_buffers,
                                bool                                          perform_launch)
            {
                HETCOMPUTE_DLOG("exec=%p add_buffers=%d perform_launch=%d",
                              static_cast<void const*>(&exec),
                              static_cast<int>(add_buffers),
                              static_cast<int>(perform_launch));
                HETCOMPUTE_INTERNAL_ASSERT(exec_cons.is_blocking(), "Must not be a task");
                HETCOMPUTE_INTERNAL_ASSERT(exec_cons.is_bundled(), "Must perform a bundled execution via this mechanism");

                auto f = [&](CallArgs&&... args) {
                    HETCOMPUTE_GCC_IGNORE_BEGIN("-Wstrict-aliasing");
                    auto const& int_gk = reinterpret_cast<typename executor_type::gpukernel_parent const&>(exec._gk);
                    HETCOMPUTE_GCC_IGNORE_END("-Wstrict-aliasing");
                    int_gk.execute_with_global_and_local_ranges(exec_cons,
                                                                bas,
                                                                add_buffers,
                                                                perform_launch,
                                                                exec._global_range,
                                                                exec._local_range,
                                                                std::forward<CallArgs>(args)...);
                };

                apply_tuple(f, exec._args_tuple);
            }
        };

        ////

        template <typename BAS, typename TP_P_EC_EX, size_t execute_index, bool Finished>
        struct process_executors_in_sequence_helper
        {
            static void process(BAS& bas, TP_P_EC_EX const& tp_p_ec_ex, bool add_buffers, bool perform_launch)
            {
                auto const& p_ec_ex = std::get<execute_index>(tp_p_ec_ex);
                HETCOMPUTE_INTERNAL_ASSERT(p_ec_ex.first != nullptr, "null executor_construct");
                HETCOMPUTE_INTERNAL_ASSERT(p_ec_ex.second != nullptr, "null executor");
                auto const& exec_cons = *(p_ec_ex.first);
                auto const& executor  = *(p_ec_ex.second);

                using Executor = typename std::remove_cv<typename std::remove_reference<decltype(executor)>::type>::type;
                process_executor_buffers<BAS, Executor>::process(exec_cons, bas, executor, add_buffers, perform_launch);

                bool constexpr finished = (std::tuple_size<TP_P_EC_EX>::value == execute_index + 1);
                process_executors_in_sequence_helper<BAS, TP_P_EC_EX, execute_index + 1, finished>::process(bas,
                                                                                                            tp_p_ec_ex,
                                                                                                            add_buffers,
                                                                                                            perform_launch);
            }
        };

        template <typename BAS, typename TP_P_EC_EX, size_t execute_index>
        struct process_executors_in_sequence_helper<BAS, TP_P_EC_EX, execute_index, true>
        {
            static void process(BAS&, TP_P_EC_EX const&, bool, bool) {}
        };

        // Applies process_executor_buffers to each executor in a sequence, applying
        // common add_buffers and perform_launch flags to all executors.
        //
        // Takes as input a tuple of pairs of executor-constructs and executor objects.
        // Tuple type is TP_P_EC_EX and tuple data is tp_p_ec_ex.
        // The tuple elements occur in order of the executor sequence.
        template <typename BAS, typename TP_P_EC_EX>
        void process_executors_in_sequence(BAS& bas, TP_P_EC_EX const& tp_p_ec_ex, bool add_buffers, bool perform_launch)
        {
            bool constexpr finished = (std::tuple_size<TP_P_EC_EX>::value == 0);
            process_executors_in_sequence_helper<BAS, TP_P_EC_EX, 0, finished>::process(bas, tp_p_ec_ex, add_buffers, perform_launch);
        }

        ////

        // Synchronously executes a sequence of executors, bundling the acquire/release of all buffers involved.
        // An intermediate layer, which expects the executors to be paired with their location indices.
        template <typename... Executors>
        void execute_seq_bundle_dispatch(std::pair<size_t, Executors const&> const&... indexed_executors)
        {
            using dyn_sized_bas = hetcompute::internal::buffer_acquire_set<0, false>;

            int   dummy_requestor = 0;
            void* requestor       = static_cast<void*>(&dummy_requestor);

            dyn_sized_bas bas;

            hetcompute::internal::executor_construct const non_last_exec_cons(requestor, true, false);
            hetcompute::internal::executor_construct const last_exec_cons(requestor, true, true);

            // Pass pointers instead of const& to work around OSX clang++ std::pair's apparent need
            // for the arguments to be default-constructible.
            // tp_p_ec_ex is a tuple of pairs of executor-constructs and executor objects.
            auto const& tp_p_ec_ex = std::make_tuple(
                std::make_pair<hetcompute::internal::executor_construct const*, Executors const*>((indexed_executors.first + 1 ==
                                                                                                         sizeof...(Executors) ?
                                                                                                     &last_exec_cons :
                                                                                                     &non_last_exec_cons),
                                                                                                &(indexed_executors.second))...);

            process_executors_in_sequence(bas, tp_p_ec_ex, true, false);

            // TODO: add support for GL / Vulkan, etc. All executors must have the same kernel type (CL/GL/Vulkan)
            bas.blocking_acquire_buffers(requestor, { hetcompute::internal::executor_device::gpucl });
            if (!bas.acquired())
            {
                HETCOMPUTE_FATAL("Failed to acquire arenas");
            }

            // Execute kernel for each executor
            process_executors_in_sequence(bas, tp_p_ec_ex, false, true);

            // Note: release buffers will be done when the last executor is processed
        }

        // Convert a tuple of Executors to a variadic list of index-executor pairs,
        // and invokes GPU bundle dispatch over the indexed-executor pairs.
        //
        // The index in each pair gives the position of the corresponding executor in
        // the executor sequence.
        template <size_t prev_index, typename TPE, typename... IndexedExecutors>
        struct execute_seq_enumerator
        {
            static void expand(TPE const& tp, IndexedExecutors const&... ies)
            {
                using executor_type = typename std::tuple_element<prev_index - 1, TPE>::type;
                execute_seq_enumerator<prev_index - 1, TPE, std::pair<size_t, executor_type const&>, IndexedExecutors...>::
                    expand(tp,
                           std::make_pair<size_t, executor_type const&>(prev_index - 1,
                                                                        static_cast<executor_type const&>(std::get<prev_index - 1>(tp))),
                           static_cast<IndexedExecutors const&>(ies)...);
            }
        };

        template <typename TPE, typename... IndexedExecutors>
        struct execute_seq_enumerator<0, TPE, IndexedExecutors...>
        {
            static void expand(TPE const&, IndexedExecutors const&... ies)
            {
                execute_seq_bundle_dispatch(static_cast<IndexedExecutors const&>(ies)...);
            }
        };

#endif // HETCOMPUTE_HAVE_GPU

    }; // namespace internal
};     // namespace hetcompute
