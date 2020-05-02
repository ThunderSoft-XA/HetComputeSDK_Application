#pragma once

#include <hetcompute/internal/patterns/hetero/utility.hh>
#include <hetcompute/internal/buffer/buffertraits.hh>

namespace hetcompute
{
    namespace internal
    {
        // Helper function to normalize index divisible by stride
        inline size_t normalize_idx(const size_t idx, const size_t stride) { return (idx / stride + 1) * stride; }

        // Dispatch functions to create subrange with Dims at runtime
        template <size_t Dims>
        struct create_range;

        template <>
        struct create_range<1>
        {
            static hetcompute::range<1> create(const hetcompute::range<1>& r, const size_t first, const size_t last)
            {
                return hetcompute::range<1>(first, last, r.stride(0));
            }
        };

        template <>
        struct create_range<2>
        {
            static hetcompute::range<2> create(const hetcompute::range<2>& r, const size_t first, const size_t last)
            {
                return hetcompute::range<2>(first, last, r.stride(0), r.begin(1), r.end(1), r.stride(1));
            }
        };

        template <>
        struct create_range<3>
        {
            static hetcompute::range<3> create(const hetcompute::range<3>& r, const size_t first, const size_t last)
            {
                return hetcompute::range<3>(first, last, r.stride(0), r.begin(1), r.end(1), r.stride(1), r.begin(2), r.end(2), r.stride(2));
            }
        };

        // Dispatch functions to create gpu subrange with Dims at runtime
        // Note that GPU is a special case because gpu kernel would not allow for
        // non-unit stride (API assert in gpukernel.hh)
        template <size_t Dims>
        struct create_gpu_range;

        template <>
        struct create_gpu_range<1>
        {
            static hetcompute::range<1> create(const hetcompute::range<1>& r, const size_t first, const size_t last)
            {
                HETCOMPUTE_UNUSED(r);
                return hetcompute::range<1>(first, last);
            }
        };

        template <>
        struct create_gpu_range<2>
        {
            static hetcompute::range<2> create(const hetcompute::range<2>& r, const size_t first, const size_t last)
            {
                return hetcompute::range<2>(first, last, r.begin(1), r.end(1));
            }
        };

        template <>
        struct create_gpu_range<3>
        {
            static hetcompute::range<3> create(const hetcompute::range<3>& r, const size_t first, const size_t last)
            {
                return hetcompute::range<3>(first, last, r.begin(1), r.end(1), r.begin(2), r.end(2));
            }
        };

        // Transform index<N> idx to corresponding linearized index in N-dim range (one-way)
        template <size_t Dims>
        struct idx2lin;

        template <>
        struct idx2lin<1>
        {
            static size_t transform(const index<1>& idx, const hetcompute::range<1>& r)
            {
                HETCOMPUTE_UNUSED(r);
                return idx[0];
            }
        };

        template <>
        struct idx2lin<2>
        {
            static size_t transform(const index<2>& idx, const hetcompute::range<2>& r)
            {
                size_t height = r.end(1) - r.begin(1);
                return idx[0] * height + idx[1];
            }
        };

        template <>
        struct idx2lin<3>
        {
            static size_t transform(const index<3>& idx, const hetcompute::range<3>& r)
            {
                size_t height = r.end(1) - r.begin(1);
                size_t depth  = r.end(2) - r.begin(2);
                return idx[0] * height * depth + idx[1] * depth + idx[2];
            }
        };

        // Check if range<N> has stride one on all dimensions
        template <size_t Dims>
        struct range_check;

        template <>
        struct range_check<1>
        {
            static bool all_single_stride(const hetcompute::range<1>& r) { return (r.stride(0) == 1); }
        };

        template <>
        struct range_check<2>
        {
            static bool all_single_stride(const hetcompute::range<2>& r) { return (r.stride(0) == 1) && (r.stride(1) == 1); }
        };

        template <>
        struct range_check<3>
        {
            static bool all_single_stride(const hetcompute::range<3>& r)
            {
                return (r.stride(0) == 1) && (r.stride(1) == 1) && (r.stride(2) == 1);
            }
        };

        // Represents the situation when an item is not found in the search tuple.
        HETCOMPUTE_CONSTEXPR_CONST size_t invalid_pos = std::numeric_limits<size_t>::max();

#if defined(HETCOMPUTE_HAVE_OPENCL)
        template <size_t Dims, typename KernelType, typename ArgTuple, size_t... Indices>
        void gpu_task_launch_wrapper(hetcompute::group_ptr&            g,
                                     hetcompute::task_ptr<void(void)>& gpu_task,
                                     const hetcompute::range<Dims>&    range,
                                     KernelType&&                    kernel,
                                     ArgTuple&                       arglist,
                                     const hetcompute::pattern::tuner& tuner,
                                     integer_list_gen<Indices...>)
        {
            HETCOMPUTE_INTERNAL_ASSERT(g != nullptr, "Unexpected null group!");
            HETCOMPUTE_API_ASSERT(range.dims() >= 1, "Range object must have at least one dimension!");
            gpu_task = hetcompute::create_task(std::forward<KernelType>(kernel), range, std::get<Indices - 1>(arglist)...);

            if (tuner.has_profile())
            {
                (static_cast<gputask_base*>(c_ptr(gpu_task)))->set_exec_time(hetcompute_get_time_now());
            }

            pattern::utility::extract_input_arenas_for_gpu<sizeof...(Indices), decltype(arglist)>::preacquire_input_arenas(c_ptr(gpu_task),
                                                                                                                           arglist);

            // Since we know output buffers is in local scope therefore not
            // be accessed by other tasks, we can save another lock here.
            // For input buffers, arenas have been preacquired for them.
            // Consequently, buffer state of the input buffer will not be
            // accessed by gpu_task's buffer acquire set. This avoids the
            // the case when input is read while write by another task.
            c_ptr(gpu_task)->unsafe_enable_non_locking_buffer_acquire();
            g->launch(gpu_task);
        }
#endif // defined(HETCOMPUTE_HAVE_OPENCL)

#if defined(HETCOMPUTE_HAVE_QTI_DSP)

        template <size_t Dims>
        struct dsp_task_launch_wrapper;

        template <>
        struct dsp_task_launch_wrapper<1>
        {
            template <typename KernelType, typename ArgTuple, size_t... Indices>
            static void launch(hetcompute::group_ptr&            g,
                               hetcompute::task_ptr<void(void)>& dsp_task,
                               const hetcompute::range<1>&       range,
                               KernelType&&                    kernel,
                               ArgTuple&                       arglist,
                               const hetcompute::pattern::tuner& tuner,
                               integer_list_gen<Indices...>)
            {
                HETCOMPUTE_INTERNAL_ASSERT(g != nullptr, "Unexpected null group!");
                HETCOMPUTE_API_ASSERT(range.dims() == 1, "Expect a 1D range object!");
                dsp_task =
                    hetcompute::create_task(std::forward<KernelType>(kernel), range.begin(0), range.end(0), std::get<Indices - 1>(arglist)...);

                if (tuner.has_profile())
                {
                    (static_cast<dsptask_return_layer*>(c_ptr(dsp_task)))->set_exec_time(hetcompute_get_time_now());
                }

                pattern::utility::extract_input_arenas_for_dsp<sizeof...(Indices),
                                                                   decltype(arglist)>::preacquire_input_arenas(c_ptr(dsp_task), arglist);

                // Since we know output buffers is in local scope therefore not
                // be accessed by other tasks, we can save another lock here.
                c_ptr(dsp_task)->unsafe_enable_non_locking_buffer_acquire();
                g->launch(dsp_task);
            }
        };

        template <>
        struct dsp_task_launch_wrapper<2>
        {
            template <typename KernelType, typename ArgTuple, size_t... Indices>
            static void launch(hetcompute::group_ptr&            g,
                               hetcompute::task_ptr<void(void)>& dsp_task,
                               const hetcompute::range<2>&       range,
                               KernelType&&                    kernel,
                               ArgTuple&                       arglist,
                               const hetcompute::pattern::tuner& tuner,
                               integer_list_gen<Indices...>)
            {
                HETCOMPUTE_INTERNAL_ASSERT(g != nullptr, "Unexpected null group!");
                HETCOMPUTE_API_ASSERT(range.dims() == 2, "Expect a 2D range object!");
                dsp_task = hetcompute::create_task(std::forward<KernelType>(kernel),
                                                     range.begin(0),
                                                     range.end(0),
                                                     range.begin(1),
                                                     range.end(1),
                                                     std::get<Indices - 1>(arglist)...);

                if (tuner.has_profile())
                {
                    (static_cast<dsptask_return_layer*>(c_ptr(dsp_task)))->set_exec_time(hetcompute_get_time_now());
                }

                pattern::utility::extract_input_arenas_for_dsp<sizeof...(Indices),
                                                                   decltype(arglist)>::preacquire_input_arenas(c_ptr(dsp_task), arglist);

                // Since we know output buffers is in local scope therefore not
                // be accessed by other tasks, we can save a nother lock here.
                c_ptr(dsp_task)->unsafe_enable_non_locking_buffer_acquire();
                g->launch(dsp_task);
            }
        };

        template <>
        struct dsp_task_launch_wrapper<3>
        {
            template <typename KernelType, typename ArgTuple, size_t... Indices>
            static void launch(hetcompute::group_ptr&            g,
                               hetcompute::task_ptr<void(void)>& dsp_task,
                               const hetcompute::range<3>&       range,
                               KernelType&&                    kernel,
                               ArgTuple&                       arglist,
                               const hetcompute::pattern::tuner& tuner,
                               integer_list_gen<Indices...>)
            {
                HETCOMPUTE_INTERNAL_ASSERT(g != nullptr, "Unexpected null group!");
                HETCOMPUTE_API_ASSERT(range.dims() == 3, "Expect a 3D range object!");
                dsp_task = hetcompute::create_task(std::forward<KernelType>(kernel),
                                                     range.begin(0),
                                                     range.end(0),
                                                     range.begin(1),
                                                     range.end(1),
                                                     range.begin(2),
                                                     range.end(2),
                                                     std::get<Indices - 1>(arglist)...);

                if (tuner.has_profile())
                {
                    (static_cast<dsptask_return_layer*>(c_ptr(dsp_task)))->set_exec_time(hetcompute_get_time_now());
                }

                pattern::utility::extract_input_arenas_for_dsp<sizeof...(Indices),
                                                                   decltype(arglist)>::preacquire_input_arenas(c_ptr(dsp_task), arglist);

                // Since we know output buffers is in local scope therefore not
                // be accessed by other tasks, we can save a nother lock here.
                c_ptr(dsp_task)->unsafe_enable_non_locking_buffer_acquire();
                g->launch(dsp_task);
            }
        };

#endif // defined(HETCOMPUTE_HAVE_QTI_DSP)

        template <typename T, bool = is_api20_buffer_ptr<T>::value>
        struct call_host_data_if_buffer_arg;

        template <typename T>
        struct call_host_data_if_buffer_arg<T, false>
        {
            static T&& wrap(T&& arg) { return std::forward<T>(arg); }
        };

        template <typename T>
        struct call_host_data_if_buffer_arg<T, true>
        {
            static T&& wrap(T&& arg)
            {
                arg.host_data();
                return std::forward<T>(arg);
            }
        };

        // For hetcompute::cpu_kernels, we need to extract the protected get_fn method, and then synchronously execute it
        struct cpu_kernel_caller
        {
            template <typename UnaryFn, size_t Dims, typename ArgTuple, size_t... Indices>
            typename std::enable_if<!pattern::utility::is_hetcompute_cpu_kernel<typename std::remove_reference<UnaryFn>::type>::value>::
                type static cpu_kernel_call_wrapper(UnaryFn&&                       fn,
                                                    const hetcompute::range<Dims>&    r,
                                                    const hetcompute::pattern::tuner& tuner,
                                                    ArgTuple&                       arglist,
                                                    integer_list_gen<Indices...>)
            {
                fn(r,
                   tuner,
                   call_host_data_if_buffer_arg<typename std::tuple_element<Indices - 1, ArgTuple>::type>::wrap(
                       std::forward<typename std::tuple_element<Indices - 1, ArgTuple>::type>(std::get<Indices - 1>(arglist)))...);
            }

            template <typename UnaryFn, size_t Dims, typename ArgTuple, size_t... Indices>
            typename std::enable_if<pattern::utility::is_hetcompute_cpu_kernel<typename std::remove_reference<UnaryFn>::type>::value>::
                type static cpu_kernel_call_wrapper(UnaryFn&&                       cpu_k,
                                                    const hetcompute::range<Dims>&    r,
                                                    const hetcompute::pattern::tuner& tuner,
                                                    ArgTuple&                       arglist,
                                                    integer_list_gen<Indices...>)
            {
                auto fn = cpu_k.get_fn();
                fn(r,
                   tuner,
                   call_host_data_if_buffer_arg<typename std::tuple_element<Indices - 1, ArgTuple>::type>::wrap(
                       std::forward<typename std::tuple_element<Indices - 1, ArgTuple>::type>(std::get<Indices - 1>(arglist)))...);
            }
        };

#if defined(HETCOMPUTE_HAVE_OPENCL)
        // Conditional compile on GPU if a valid CL kernel is found in the given kernel tuple.
        // Template parameter: KPos         -- corresponding kernel index in kernel tuple
        //                     ArgPos       -- corresponding output buffer index in kernel tuple
        //                     CalledWithPK -- a boolean that is set to true when the kernel list is generated from a pointkernel.
        //                                     In this case the buffer arguments in arglist need to be expanded into <buffer,size> pairs.
        //                                     When this is false, arglist is directly used without any expansions.

        template <size_t KPos, size_t ArgPos, size_t Dims, typename KernelTuple, typename ArgTuple, typename T, bool CalledWithPK>
        struct execute_on_gpu
        {
            static void kernel_launch(hetcompute::task_ptr<void(void)>& gpu_task,
                                      const hetcompute::range<Dims>&    r,
                                      hetcompute::group_ptr&            g,
                                      KernelTuple&                    klist,
                                      ArgTuple                        arglist, // pass by copy here to avoid arglist modified
                                                                               // and output pointer not host accessible by cpu
                                      hetcompute::buffer_ptr<T>         gpu_local_buffer,
                                      const hetcompute::pattern::tuner& tuner)
            {
                HETCOMPUTE_INTERNAL_ASSERT(g != nullptr, "Unexpected null group!");

                auto gpu_kernel           = std::get<KPos>(klist);
                std::get<ArgPos>(arglist) = gpu_local_buffer;
                gpu_task_launch_wrapper(g,
                                        gpu_task,
                                        r,
                                        std::forward<decltype(gpu_kernel)>(gpu_kernel),
                                        arglist,
                                        tuner,
                                        typename integer_list<std::tuple_size<ArgTuple>::value>::type());
            }
        };

        // true specialization. Here all the buffers within arglist are expanded to <buffer,size> pairs and stored in
        // new_arglist. This is required in the case where the kernels are generated from a pointkernel.
        template <size_t KPos, size_t ArgPos, size_t Dims, typename KernelTuple, typename ArgTuple, typename T>
        struct execute_on_gpu<KPos, ArgPos, Dims, KernelTuple, ArgTuple, T, true>
        {
            static void kernel_launch(hetcompute::task_ptr<void(void)>& gpu_task,
                                      const hetcompute::range<Dims>&    r,
                                      hetcompute::group_ptr&            g,
                                      KernelTuple&                    klist,
                                      ArgTuple                        arglist,
                                      hetcompute::buffer_ptr<T>         gpu_local_buffer,
                                      const hetcompute::pattern::tuner& tuner)
            {
                HETCOMPUTE_INTERNAL_ASSERT(g != nullptr, "Unexpected null group!");

                auto gpu_kernel           = std::get<KPos>(klist);
                std::get<ArgPos>(arglist) = gpu_local_buffer;
                ::hetcompute::internal::buffer::utility::expand_buffers_in_args_for_gpu<ArgTuple, 0, false> parsed_params(arglist);
                auto new_arglist = parsed_params._expanded_args_list;
                gpu_task_launch_wrapper(g,
                                        gpu_task,
                                        r,
                                        std::forward<decltype(gpu_kernel)>(gpu_kernel),
                                        new_arglist,
                                        tuner,
                                        typename integer_list<std::tuple_size<decltype(new_arglist)>::value>::type());
            }
        };

        // If valid OpenCL kernel is not found in the given kernel tuple, do nothing
        template <size_t ArgPos, size_t Dims, typename KernelTuple, typename ArgTuple, typename T>
        struct execute_on_gpu<invalid_pos, ArgPos, Dims, KernelTuple, ArgTuple, T, true>
        {
            static void kernel_launch(hetcompute::task_ptr<void(void)>&,
                                      const hetcompute::range<Dims>&,
                                      hetcompute::group_ptr&,
                                      KernelTuple&,
                                      ArgTuple,
                                      hetcompute::buffer_ptr<T>,
                                      const hetcompute::pattern::tuner&)
            {
            }
        };

        template <size_t ArgPos, size_t Dims, typename KernelTuple, typename ArgTuple, typename T>
        struct execute_on_gpu<invalid_pos, ArgPos, Dims, KernelTuple, ArgTuple, T, false>
        {
            static void kernel_launch(hetcompute::task_ptr<void(void)>&,
                                      const hetcompute::range<Dims>&,
                                      hetcompute::group_ptr&,
                                      KernelTuple&,
                                      ArgTuple,
                                      hetcompute::buffer_ptr<T>,
                                      const hetcompute::pattern::tuner&)
            {
            }
        };

#endif // defined(HETCOMPUTE_HAVE_OPENCL)

#if defined(HETCOMPUTE_HAVE_QTI_DSP)
        // Conditional compile on Hexagon if a valid dsp kernel is found in the given kernel tuple.
        // Template parameter: KPos   -- corresponding kernel index in kernel tuple
        //                     ArgPos -- corresponding output buffer index in kernel tuple
        template <size_t KPos, size_t ArgPos, size_t Dims, typename KernelTuple, typename ArgTuple, typename T>
        struct execute_on_dsp
        {
            static void kernel_launch(hetcompute::task_ptr<void(void)>& dsp_task,
                                      const hetcompute::range<Dims>&    r,
                                      hetcompute::group_ptr&            g,
                                      KernelTuple&                    klist,
                                      ArgTuple& arglist, // OK, since it is arg_list_for_dsp that is being passed by reference.
                                      hetcompute::buffer_ptr<T>         dsp_local_buffer,
                                      const hetcompute::pattern::tuner& tuner)
            {
                HETCOMPUTE_INTERNAL_ASSERT(g != nullptr, "Unexpected null group!");
                std::get<ArgPos>(arglist) = dsp_local_buffer;
                auto dsp_kernel       = std::get<KPos>(klist);
                dsp_task_launch_wrapper<Dims>::launch(g,
                                                          dsp_task,
                                                          r,
                                                          std::forward<decltype(dsp_kernel)>(dsp_kernel),
                                                          arglist,
                                                          tuner,
                                                          typename integer_list<std::tuple_size<ArgTuple>::value>::type());
            }
        };

        // If valid dsp kernel is not found in the given kernel tuple, do nothing
        template <size_t ArgPos, size_t Dims, typename KernelTuple, typename ArgTuple, typename T>
        struct execute_on_dsp<invalid_pos, ArgPos, Dims, KernelTuple, ArgTuple, T>
        {
            static void kernel_launch(hetcompute::task_ptr<void(void)>&,
                                      const hetcompute::range<Dims>&,
                                      hetcompute::group_ptr&,
                                      KernelTuple&,
                                      ArgTuple&,
                                      hetcompute::buffer_ptr<T>,
                                      const hetcompute::pattern::tuner&)
            {
            }
        };
#endif // defined(HETCOMPUTE_HAVE_QTI_DSP)

        // Conditional compile on CPU if a valid cpu kernel is found in the given kernel tuple.
        // Template parameter: KPos         -- corresponding kernel index in kernel tuple
        //                     CalledWithPK -- a boolean that is set to true when the kernel list is generated from a pointkernel.
        //                                     In this case the buffer arguments in arglist need to be expanded into <pointer,size> pairs.
        //                                     When this is false, arglist is directly used without any expansions.

        // Use arglist directly in the case that the kernels are not generated from pointkernels.
        template <size_t KPos, size_t Dims, typename KernelTuple, typename ArgTuple, bool CalledWithPK>
        struct execute_on_cpu
        {
            static void kernel_launch(const hetcompute::range<Dims>&    r,
                                      KernelTuple&                    klist,
                                      ArgTuple&                       arglist, // cpu directly modifies output in arglist
                                      const hetcompute::pattern::tuner& tuner)
            {
                auto cpu_kernel = std::get<KPos>(klist);
                cpu_kernel_caller::cpu_kernel_call_wrapper(std::forward<decltype(cpu_kernel)>(cpu_kernel),
                                                           r,
                                                           tuner,
                                                           arglist,
                                                           typename integer_list<std::tuple_size<ArgTuple>::value>::type());
            }
        };

        // true specialization. Expand all buffers in arglist to <pointer, size> pairs and store them in new_arglist.
        // Then, execute the kernel with new_arglist.
        template <size_t KPos, size_t Dims, typename KernelTuple, typename ArgTuple>
        struct execute_on_cpu<KPos, Dims, KernelTuple, ArgTuple, true>
        {
            static void
            kernel_launch(const hetcompute::range<Dims>& r, KernelTuple& klist, ArgTuple& arglist, const hetcompute::pattern::tuner& tuner)
            {
                auto cpu_kernel = std::get<KPos>(klist);
                ::hetcompute::internal::buffer::utility::expand_buffers_in_args_for_cpu<ArgTuple, 0, false> parsed_params(arglist);
                auto new_arglist = parsed_params._expanded_args_list;
                cpu_kernel_caller::cpu_kernel_call_wrapper(std::forward<decltype(cpu_kernel)>(cpu_kernel),
                                                           r,
                                                           tuner,
                                                           new_arglist,
                                                           typename integer_list<std::tuple_size<decltype(new_arglist)>::value>::type());
            }
        };

        // If valid cpu kernel is not found in the given kernel tuple, do nothing
        template <size_t Dims, typename KernelTuple, typename ArgTuple>
        struct execute_on_cpu<invalid_pos, Dims, KernelTuple, ArgTuple, true>
        {
            static void kernel_launch(const hetcompute::range<Dims>&, KernelTuple&, ArgTuple&, const hetcompute::pattern::tuner&) {}
        };

        // If valid cpu kernel is not found in the given kernel tuple, do nothing
        template <size_t Dims, typename KernelTuple, typename ArgTuple>
        struct execute_on_cpu<invalid_pos, Dims, KernelTuple, ArgTuple, false>
        {
            static void kernel_launch(const hetcompute::range<Dims>&, KernelTuple&, ArgTuple&, const hetcompute::pattern::tuner&) {}
        };

    };  // namespace internal
};  // namespace hetcompute
