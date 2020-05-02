#pragma once

// This file provides additional functionality over the hetcompute APIs.

#include <hetcompute/internal/task/functiontraits.hh>
#include <hetcompute/internal/task/sync_execute_internal.hh>

namespace hetcompute
{
    namespace internal
    {
        /**
         *  @brief Synchronous execution API to create buffer acquire/release scope for a CPU callable.
         *
         *  Synchronous execution API to create buffer acquire/release scope.
         *  Limited to execution of CPU functions, functors and lambdas.
         *
         *  @note
         *  Do NOT invoke recursively, the current implementation is limited and may lead to deadlock.
         *
         *  Example:
         *    void f(hetcompute::buffer_ptr<int> a, hetcompute::buffer_ptr<int> b, int x);
         *
         *    // f(buf1, buf2, 5) is executed with buf1 and buf2 guaranteed to be acquired
         *    // automatically prior to invocation, and released automatically after completion.
         *    hetcompute::internal::execute_function(f, buf1, buf2, 5);
         */
        template <typename Code, typename... Args>
        void execute_function(Code&& code, Args&&... args)
        {
            // TODO: add support for cpu_kernel, and/or add static_asserts on type of Code

            using params_tuple           = typename internal::function_traits<typename std::remove_reference<Code>::type>::args_tuple;
            size_t constexpr num_buffers = hetcompute::internal::num_buffer_ptrs_in_tuple<params_tuple>::value;

            hetcompute::internal::buffer_acquire_set<num_buffers> bas;

            internal::add_buffers<decltype(bas), params_tuple, Args...>(bas, std::forward<Args>(args)...);

            int  dummy_requestor = 0;
            auto requestor       = static_cast<void*>(&dummy_requestor);

            bas.blocking_acquire_buffers(requestor, { hetcompute::internal::executor_device::cpu });
            // code will later be extended to support CPU/GPU/Hexagon kernels
            code(std::forward<Args>(args)...);
            bas.release_buffers(requestor);
        }

#ifdef HETCOMPUTE_HAVE_GPU

        /**
         *  Synchronous execution API that creates a buffer acquire/release scope when executing a gpu kernel.
         */
        template <typename GPUKernel, typename... CallArgs>
        void execute_gpu(GPUKernel const& gk, CallArgs&&... args)
        {
            size_t constexpr num_buffers = hetcompute::internal::num_buffer_ptrs_in_args<CallArgs...>::value;
            hetcompute::internal::buffer_acquire_set<num_buffers> bas;

            HETCOMPUTE_GCC_IGNORE_BEGIN("-Wstrict-aliasing");
            auto const& int_gk = reinterpret_cast<typename GPUKernel::parent const&>(gk);
            HETCOMPUTE_GCC_IGNORE_END("-Wstrict-aliasing");
            int   dummy_requestor = 0;
            void* requestor       = static_cast<void*>(&dummy_requestor);
            int_gk.execute_with_global_range(hetcompute::internal::executor_construct(requestor, false, false),
                                             bas,
                                             true, // add buffers
                                             true, /// perform launch
                                             std::forward<CallArgs>(args)...);
        }

        /**
         *  Wrapper object for capturing a synchronous execution for later execution.
         *  Currently limited to GPU kernels.
         */
        template <typename GPUKernel, size_t Dims, typename... CallArgs>
        struct executor;

        /**
         *  Factory function call to create an executor.
         *
         *  @sa executor
         */
        template <typename GPUKernel, size_t Dims, typename... CallArgs>
        executor<GPUKernel, Dims, CallArgs...> create_executor(GPUKernel const&             gk,
                                                               hetcompute::range<Dims> const& global_range,
                                                               hetcompute::range<Dims> const& local_range,
                                                               CallArgs&&... args)
        {
            return executor<GPUKernel, Dims, CallArgs...>(gk, global_range, local_range, std::forward<CallArgs>(args)...);
        }

        /**
         * Executes a sequence of executors as bundle for the combined acquire/release of all buffers involved across executors.
         *
         * @sa executor
         */
        template <typename... Executors>
        void execute_seq(Executors const&... executors)
        {
            std::tuple<Executors const&...> tpe(static_cast<Executors const&>(executors)...);
            execute_seq_enumerator<sizeof...(Executors), std::tuple<Executors const&...>>::expand(static_cast<decltype(tpe) const &>(tpe));
        }

        /**
         *  Alternative implementation of a GPU task, that synchronously executes a GPU kernel from within a CPU lambda.
         */
        template <typename... Executors>
        hetcompute::task_ptr<> create_task(Executors const&... executors)
        {
            // Forced to create an extra tuple as the capture target, as GCC fails with variadic capture lists
            std::tuple<Executors...> tpe(const_cast<Executors&>(executors)...);
            auto                     ck = hetcompute::create_cpu_kernel([tpe] { apply_tuple(execute_seq<Executors...>, tpe); });
            ck.set_big();
            return hetcompute::create_task(ck);
        }

#endif // HETCOMPUTE_HAVE_GPU

    }; // namespace internal
};     // namespace hetcompute
