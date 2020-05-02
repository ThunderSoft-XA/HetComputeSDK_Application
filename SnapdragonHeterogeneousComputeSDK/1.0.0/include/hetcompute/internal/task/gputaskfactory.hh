#pragma once

#ifdef HETCOMPUTE_HAVE_GPU

#include <hetcompute/gpukernel.hh>
#include <hetcompute/task.hh>

namespace hetcompute
{
    namespace internal
    {
        template <typename... KernelArgs>
        struct task_factory_dispatch<hetcompute::gpu_kernel<KernelArgs...>>
        {
        public:
            using collapsed_task_type         = typename hetcompute::task_ptr<void(void)>;
            using non_collapsed_task_type     = typename hetcompute::task_ptr<void(void)>;
            using collapsed_raw_task_type     = hetcompute::internal::task;
            using non_collapsed_raw_task_type = hetcompute::internal::task;

            template <typename GPUKernel, typename... Args>
            static collapsed_raw_task_type* create_collapsed_task(GPUKernel const& gk, Args&&... args)
            {
                return create_non_collapsed_task(gk, std::forward<Args>(args)...);
            }

            template <typename GPUKernel, typename... Args>
            static typename std::enable_if<GPUKernel::num_kernel_args + 1 == sizeof...(Args), non_collapsed_raw_task_type*>::type
            create_non_collapsed_task(GPUKernel const& gk, Args&&... args)
            {
                auto const& int_gk = reinterpret_cast<typename GPUKernel::parent const&>(gk);
                return int_gk.create_task_helper_with_global_range(std::forward<Args>(args)...);
            }

            template <typename GPUKernel, typename... Args>
            static typename std::enable_if<GPUKernel::num_kernel_args + 2 == sizeof...(Args), non_collapsed_raw_task_type*>::type
            create_non_collapsed_task(GPUKernel const& gk, Args&&... args)
            {
                auto const& int_gk = reinterpret_cast<typename GPUKernel::parent const&>(gk);
                return int_gk.create_task_helper_with_global_and_local_ranges(std::forward<Args>(args)...);
            }

            template <typename GPUKernel, typename... Args>
            static bool launch(bool notify, group* g, GPUKernel const& gk, Args&&... args)
            {
                HETCOMPUTE_UNUSED(notify);
                auto t = create_non_collapsed_task(gk, std::forward<Args>(args)...);
                if (t == nullptr)
                {
                    return false;
                }

                // FIXME: Both lines below are workarounds.
                // Must launch first to avoid a potential count to 0.
                t->launch_legacy(g, nullptr, false);
                g->dec_task_counter();
                return true;
            }
        }; // struct task_factory_dispatch

    };  // namespace internal
};  // namespace hetcompute

#endif // HETCOMPUTE_HAVE_GPU
