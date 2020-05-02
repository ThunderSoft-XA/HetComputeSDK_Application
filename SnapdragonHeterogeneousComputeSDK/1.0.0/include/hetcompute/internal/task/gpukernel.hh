#pragma once

#ifdef HETCOMPUTE_HAVE_GPU

#include <hetcompute/range.hh>

#include <hetcompute/internal/legacy/gpukernel.hh>
#include <hetcompute/internal/legacy/group.hh>
#include <hetcompute/internal/legacy/task.hh>

namespace hetcompute
{
    namespace internal
    {
        // Forward declaration
        template <typename GPUKernel, typename... CallArgs>
        void execute_gpu(GPUKernel const& gk, CallArgs&&... args);

        template <typename GPUKernel, size_t Dims, typename... CallArgs>
        struct executor;

        template <typename BAS, typename Executor>
        struct process_executor_buffers;

    }; // namespace internal
};     // namespace hetcompute

namespace hetcompute
{
    namespace internal
    {
        inline hetcompute::internal::legacy::device_ptr get_gpu_device()
        {
#ifdef HETCOMPUTE_HAVE_OPENCL
            // get devices.
            HETCOMPUTE_INTERNAL_ASSERT(!::hetcompute::internal::s_dev_ptrs->empty(), "No OpenCL GPU devices found on the platform");
            ::hetcompute::internal::cldevice* d_ptr = ::hetcompute::internal::c_ptr(::hetcompute::internal::s_dev_ptrs->at(0));
            HETCOMPUTE_INTERNAL_ASSERT(d_ptr != nullptr, "null device_ptr");
            hetcompute::internal::legacy::device_ptr device = (*::hetcompute::internal::s_dev_ptrs)[0];
#else  // HETCOMPUTE_HAVE_OPENCL
            // device currently not needed for non-OpenCL GPU frameworks
            hetcompute::internal::legacy::device_ptr device;
#endif // HETCOMPUTE_HAVE_OPENCL

            return device;
        }

        template <size_t Dims, typename Body>
        inline ::hetcompute::internal::task*
        api_create_ndrange_task(const hetcompute::range<Dims>& global_range, const hetcompute::range<Dims>& local_range, Body&& body)
        {
            typedef ::hetcompute::internal::legacy::body_wrapper<Body> wrapper;
            auto                                                     device = get_gpu_device();

            for (auto i : global_range.stride())
            {
                HETCOMPUTE_API_ASSERT((i == 1), "GPU global ranges must be unit stride");
            }

            for (auto i : local_range.stride())
            {
                HETCOMPUTE_API_ASSERT((i == 1), "GPU local ranges must be unit stride");
            }

            ::hetcompute::internal::task* t = wrapper::create_task(device, global_range, local_range, std::forward<Body>(body));

            return t;
        }

        template <size_t Dims, typename Body>
        ::hetcompute::internal::task* api_create_ndrange_task(const hetcompute::range<Dims>& global_range, Body&& body)
        {
            hetcompute::range<Dims> local_range;
            return api_create_ndrange_task(global_range, local_range, std::forward<Body>(body));
        }

        template <typename... Args>
        class gpu_kernel_implementation
        {
            ::hetcompute::internal::legacy::kernel_ptr<Args...> _kernel;

            template <typename Code, class Enable>
            friend struct ::hetcompute::internal::task_factory_dispatch;

            template <typename GPUKernel, typename... CallArgs>
            friend void hetcompute::internal::execute_gpu(GPUKernel const& gk, CallArgs&&... args);

            template <typename GPUKernel, size_t Dims, typename... CallArgs>
            friend struct hetcompute::internal::executor;

            template <typename BAS, typename Executor>
            friend struct hetcompute::internal::process_executor_buffers;

        public:
            using params_tuple = std::tuple<Args...>;

        protected:
            gpu_kernel_implementation(std::string const& kernel_str, std::string const& kernel_name, std::string const& build_options)
                : _kernel(::hetcompute::internal::legacy::create_kernel<Args...>(kernel_str, kernel_name, build_options))
            {
            }

            explicit gpu_kernel_implementation(std::string const& gl_kernel_str)
                : _kernel(::hetcompute::internal::legacy::create_kernel<Args...>(gl_kernel_str))
            {
            }

            gpu_kernel_implementation(void const* kernel_bin, size_t kernel_len, std::string const& kernel_name, std::string const& build_options)
                : _kernel(::hetcompute::internal::legacy::create_kernel<Args...>(kernel_bin, kernel_len, kernel_name, build_options))
            {
            }

#ifdef HETCOMPUTE_HAVE_OPENCL
            std::pair<void const*, size_t> get_cl_kernel_binary() const
            {
                auto k_ptr = internal::c_ptr(_kernel);
                return k_ptr->get_cl_kernel_binary();
            }
#endif // HETCOMPUTE_HAVE_OPENCL

            HETCOMPUTE_DEFAULT_METHOD(gpu_kernel_implementation(gpu_kernel_implementation const&));
            HETCOMPUTE_DEFAULT_METHOD(gpu_kernel_implementation& operator=(gpu_kernel_implementation const&));
            HETCOMPUTE_DEFAULT_METHOD(gpu_kernel_implementation(gpu_kernel_implementation&&));
            HETCOMPUTE_DEFAULT_METHOD(gpu_kernel_implementation& operator=(gpu_kernel_implementation&&));

            /// Creates a hetcompute::task that will execute this GPU kernel.
            /// Only first TaskParam is hetcompute::range for global range. No local range provided.
            template <typename TaskParam1, typename... TaskParams>
            hetcompute::internal::task* create_task_helper_with_global_range(TaskParam1 const& global_range, TaskParams&&... task_params) const
            {
                auto attrs = hetcompute::internal::legacy::create_task_attrs(hetcompute::internal::legacy::attr::gpu);
                auto gpu_task =
                    api_create_ndrange_task(global_range,
                                              hetcompute::internal::legacy::with_attrs(attrs, _kernel, std::forward<TaskParams>(task_params)...));
                return gpu_task;
            }

            /// Creates a hetcompute::task that will execute this GPU kernel.
            /// First two TaskParams are hetcompute::range for global and local ranges.
            template <typename TaskParam1, typename TaskParam2, typename... TaskParams>
            hetcompute::internal::task* create_task_helper_with_global_and_local_ranges(TaskParam1 const& global_range,
                                                                                      TaskParam2 const& local_range,
                                                                                      TaskParams&&... task_params) const
            {
                auto attrs = hetcompute::internal::legacy::create_task_attrs(hetcompute::internal::legacy::attr::gpu);
                auto gpu_task =
                    api_create_ndrange_task(global_range,
                                              local_range,
                                              hetcompute::internal::legacy::with_attrs(attrs, _kernel, std::forward<TaskParams>(task_params)...));
                return gpu_task;
            }

            /// Synchronously execute this GPU kernel.
            /// First two TaskParams are hetcompute::range for global and local ranges.
            template <typename BAS, size_t Dims, typename... TaskParams>
            void execute_with_global_and_local_ranges(executor_construct const&      exec_cons,
                                                      BAS&                           bas,
                                                      bool                           add_buffers,
                                                      bool                           perform_launch,
                                                      ::hetcompute::range<Dims> const& global_range,
                                                      ::hetcompute::range<Dims> const& local_range,
                                                      TaskParams&&... task_params) const
            {
                auto device = get_gpu_device();

                for (auto i : global_range.stride())
                {
                    HETCOMPUTE_API_ASSERT((i == 1), "GPU global ranges must be unit stride");
                }

                for (auto i : local_range.stride())
                {
                    HETCOMPUTE_API_ASSERT((i == 1), "GPU local ranges must be unit stride");
                }

                gpu_launch_info gli(device);

                using args_tuple = std::tuple<typename strip_ref_cv<TaskParams>::type...>;

                auto                                            args = std::make_tuple(std::forward<TaskParams>(task_params)...);
                normal_args_container<params_tuple, args_tuple> my_normal_args_container(args);

                // track state across multiple launch attempts
                bool first_execution = true;

                auto   k_ptr      = internal::c_ptr(_kernel);
                bool   conflict   = true;
                size_t spin_count = 10;

                while (conflict)
                {
                    if (spin_count > 0)
                    {
                        spin_count--;
                    }
                    else
                    {
                        usleep(1);
                    }

                    conflict = !gpu_do_execute<Dims,
                                               params_tuple,
                                               args_tuple,
                                               decltype(k_ptr),
                                               decltype(my_normal_args_container._tp),
                                               decltype(bas),
                                               preacquired_arenas_base>(k_ptr,
                                                                        exec_cons,
                                                                        gli,
                                                                        device,
                                                                        global_range,
                                                                        local_range,
                                                                        std::make_tuple(std::forward<TaskParams>(task_params)...),
                                                                        my_normal_args_container._tp,
                                                                        bas,
                                                                        nullptr,
                                                                        first_execution && add_buffers,
                                                                        perform_launch); // dispatch args and execute

                    HETCOMPUTE_INTERNAL_ASSERT(perform_launch || !conflict, "Conflict not expected when launch is not attempted");
                    first_execution = false;
                }
            }

            /// Synchronously execute this GPU kernel.
            /// Only first TaskParam is hetcompute::range for global range. No local range provided.
            template <typename BAS, size_t Dims, typename... TaskParams>
            void execute_with_global_range(executor_construct const&      exec_cons,
                                           BAS&                           bas,
                                           bool                           add_buffers,
                                           bool                           perform_launch,
                                           ::hetcompute::range<Dims> const& global_range,
                                           TaskParams&&... task_params) const
            {
                ::hetcompute::range<Dims> local_range;
                execute_with_global_and_local_ranges(exec_cons,
                                                     bas,
                                                     add_buffers,
                                                     perform_launch,
                                                     global_range,
                                                     local_range,
                                                     std::forward<TaskParams>(task_params)...);
            }

            /// Destructor. gpu_kernel_implementation is not ref-counted.
            ~gpu_kernel_implementation() {}
        };

    }; // namespace internal
};     // namespace hetcompute

#endif // HETCOMPUTE_HAVE_GPU
