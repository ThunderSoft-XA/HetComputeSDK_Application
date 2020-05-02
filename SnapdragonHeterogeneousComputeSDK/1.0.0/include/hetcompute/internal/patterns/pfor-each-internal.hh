#pragma once

#include <hetcompute/internal/patterns/cpu_pfor_each.hh>
#include <hetcompute/internal/patterns/gpu_pfor_each.hh>
#include <hetcompute/internal/patterns/hetero/pfor_each_helper.hh>
#include <hetcompute/internal/pointkernel/pointkernel-internal.hh>

namespace hetcompute
{
    namespace pattern
    {
        template <typename T1, typename T2>
        class pfor;
    }; // namespace pattern

    namespace internal
    {
        // Template to dispatch pfor based on iterator type
        template <class InputIterator, typename UnaryFn, typename T>
        struct pfor_each_dispatcher;

        // GPU version of pfor.
        template <typename UnaryFn>
        struct pfor_each_dispatcher<size_t, UnaryFn, std::true_type>
        {
            // Routes it to size_t verion on gpu
            static void dispatch(group* g, size_t first, size_t last, UnaryFn&& fn, const size_t, const hetcompute::pattern::tuner&)
            {
                pfor_each_gpu(g, first, last, std::forward<UnaryFn>(fn));
            }

            // Routes it to hetcompute::range<Dims> version on gpu.
            // FIXME: with hetero pfor_each, the gpu path might no longer needed.
            template <size_t Dims>
            static void dispatch(group* g, const hetcompute::range<Dims>& r, UnaryFn&& fn, const hetcompute::pattern::tuner&)
            {
                for (auto i : r.stride())
                    HETCOMPUTE_API_ASSERT((i == 1), "GPU ranges must be unit stride");

                hetcompute::range<Dims> l;
                pfor_each_gpu(g, r, l, std::forward<UnaryFn>(fn));
            }
        };

        // if index is of type integral, pass index with correct offset
        template <typename UnaryFn, typename IterType>
        void func_impl(const size_t& idx, IterType first, UnaryFn&& fn, std::true_type)
        {
            fn(static_cast<IterType>(idx) + first);
        }

        // if index is of type pointer/RAiterator, cannot perform static_cast
        template <typename UnaryFn, class IterType>
        void func_impl(const size_t& idx, IterType first, UnaryFn&& fn, std::false_type)
        {
            fn(idx + first);
        }

        // CPU version for non_sizet inputIterator
        template <class InputIterator, typename UnaryFn>
        struct pfor_each_dispatcher<InputIterator, UnaryFn, std::false_type>
        {
            static void
            dispatch(group* g, InputIterator first, InputIterator last, UnaryFn&& fn, const size_t stride, const hetcompute::pattern::tuner& t)
            {
                HETCOMPUTE_API_ASSERT(hetcompute::internal::callable_object_is_mutable<UnaryFn>::value == false,
                                      "Mutable functor is not allowed in hetcompute patterns!");

                if (t.is_serial())
                {
                    for (InputIterator idx = first; idx < last; idx += stride)
                    {
                        fn(idx);
                    }
                    return;
                }

                if (t.is_static())
                {
                    pfor_each_static(g, first, last, std::forward<UnaryFn>(fn), stride, t);
                    return;
                }

                const auto fn_transform = [fn, first](size_t i) { func_impl(i, first, fn, std::is_integral<InputIterator>()); };

                pfor_each_dynamic(g, size_t(0), size_t(last - first), fn_transform, stride, t);
            }
        };

        // CPU version for size_t
        template <typename UnaryFn>
        struct pfor_each_dispatcher<size_t, UnaryFn, std::false_type>
        {
            // Routes it to size_t version on cpu
            static void dispatch(group* g, size_t first, size_t last, UnaryFn&& fn, const size_t stride, hetcompute::pattern::tuner const& t)
            {
                HETCOMPUTE_API_ASSERT(hetcompute::internal::callable_object_is_mutable<UnaryFn>::value == false,
                                      "Mutable functor is not allowed in hetcompute patterns!");

                if (t.is_serial())
                {
                    for (size_t idx = first; idx < last; idx += stride)
                    {
                        fn(idx);
                    }
                }

                else if (t.is_static())
                {
                    pfor_each_static(g, first, last, std::forward<UnaryFn>(fn), stride, t);
                }

                else
                {
                    pfor_each_dynamic(g, first, last, std::forward<UnaryFn>(fn), stride, t);
                }
            }

            template <size_t Dims>
            static void dispatch(group* g, const hetcompute::range<Dims>& r, UnaryFn&& fn, const hetcompute::pattern::tuner& t)
            {
                pfor_each_range<Dims, UnaryFn>::pfor_each_range_impl(g, r, std::forward<UnaryFn>(fn), t);
            }
        };

        // implementation details of user-facing interfaces
        template <class InputIterator, typename UnaryFn>
        void pfor_each_internal(group*                            g,
                                InputIterator                     first,
                                InputIterator                     last,
                                UnaryFn&&                         fn,
                                const size_t                      stride,
                                const hetcompute::pattern::tuner& t = hetcompute::pattern::tuner())
        {
            // pfor_each dispatcher routes it to different versions.
            internal::pfor_each_dispatcher<InputIterator, UnaryFn, typename std::is_base_of<legacy::body_with_attrs_base_gpu, UnaryFn>::type>::
                dispatch(g, first, last, std::forward<UnaryFn>(fn), stride, t);
        }

        template <size_t Dims, typename UnaryFn>
        void pfor_each_internal(group* g, const hetcompute::range<Dims>& r, UnaryFn&& fn, const hetcompute::pattern::tuner& t)
        {
            // pfor_each dispatcher routes it to different hetcompute::range<Dims> version.
            internal::pfor_each_dispatcher<size_t, UnaryFn, typename std::is_base_of<legacy::body_with_attrs_base_gpu, UnaryFn>::type>::
                dispatch(g, r, std::forward<UnaryFn>(fn), t);
        }

        template <class InputIterator, typename UnaryFn>
        void pfor_each(group_ptr                         group,
                       InputIterator                     first,
                       InputIterator                     last,
                       UnaryFn&&                         fn,
                       const size_t                      stride = 1,
                       const hetcompute::pattern::tuner& t      = hetcompute::pattern::tuner())
        {
            auto gptr = c_ptr(group);
            pfor_each_internal(gptr, first, last, std::forward<UnaryFn>(fn), stride, t);
        }

        // Disable implicit type conversion from 0 to nullptr
        // Example: pfor_each(0, 100, 2, [](){});
        template <class InputIterator, typename UnaryFn>
        void pfor_each(int, InputIterator, InputIterator, UnaryFn) = delete;

        template <size_t Dims, typename UnaryFn>
        void pfor_each(group_ptr group, const hetcompute::range<Dims>& r, UnaryFn&& fn, const hetcompute::pattern::tuner& t)
        {
            auto gptr = c_ptr(group);
            pfor_each_internal(gptr, r, std::forward<UnaryFn>(fn), t);
        }

        // Compared to the external pfor_each_async API, put fn first so that can be
        // called with ...args from multiple places
        template <class InputIterator, typename UnaryFn>
        hetcompute::task_ptr<void()> pfor_each_async(UnaryFn                           fn,
                                                     InputIterator                     first,
                                                     InputIterator                     last,
                                                     const size_t                      stride = 1,
                                                     const hetcompute::pattern::tuner& tuner  = hetcompute::pattern::tuner())
        {
            auto g = create_group();
            auto t = hetcompute::create_task([g, first, last, fn, stride, tuner] { internal::pfor_each(g, first, last, fn, stride, tuner); });
            auto gptr = internal::c_ptr(g);
            gptr->set_representative_task(internal::c_ptr(t));
            return t;
        }

        template <size_t Dims, typename UnaryFn>
        hetcompute::task_ptr<void()>
        pfor_each_async(UnaryFn fn, const hetcompute::range<Dims>& r, const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
        {
            auto g    = create_group();
            auto t    = hetcompute::create_task([g, r, fn, tuner] { internal::pfor_each(g, r, fn, tuner); });
            auto gptr = internal::c_ptr(g);
            gptr->set_representative_task(internal::c_ptr(t));
            return t;
        }

        /**
        /// Internal implementation of heterogeneous pfor_each
        /// Partition range based on hints of hetcompute::pattern::tuner and execute
        /// them on heterogeneous processing elements on the SoC using programmer-provided
        /// kernels. Privatized buffers for each device is created. Once the execution
        /// is completed, results are merged back.

         * @param range   Range object (1D, 2D or 3D) representing the iteration space.
         * @param klist   List of kernels. Each kernel belongs to any of the following
                          category: CPU, OPENCL, or Hexagon. Kernels can be zipped with
                          any order and combination. If multiple kernels of the same
                          category are defined, a compiler error will be generated.
         * @param tuner   Qualcomm HETCOMPUTE pattern tuner object used to specify the
                          allocation of iteration space to be computed by each
                          heterogeneous kernel given out in klist.
         * @param buf_tup Tuple of local output buffers for the gpu and dsp kernels.
                          The first element is the gpu local buffer. The second element of the tuple
                          is a vector of output buffers for the dsp kernel, where the size of the
                          vector is equal to the number of dsp threads.
         * @param args    Variadic arguments pass to kernels.
         */

        template <size_t Dims, typename KernelTuple, typename ArgTuple, typename KernelFirst, typename... KernelRest, typename... Args, typename Boolean, typename Buf_Tuple>
        void pfor_each(hetcompute::pattern::pfor<KernelTuple, ArgTuple>* const p,
                       const hetcompute::range<Dims>&                          r,
                       std::tuple<KernelFirst, KernelRest...>&                 klist,
                       hetcompute::pattern::tuner&                             tuner,
                       const Boolean                                           called_with_pointkernel,
                       Buf_Tuple&&                                             buf_tup,
                       Args&&... args)
        {
            HETCOMPUTE_UNUSED(buf_tup);
            bool have_gpu = false;
            bool have_dsp = false;

#if defined(HETCOMPUTE_HAVE_OPENCL)
            have_gpu = true;
#endif // defined(HETCOMPUTE_HAVE_OPENCL)

#if defined(HETCOMPUTE_HAVE_QTI_DSP)
            have_dsp = true;
#endif // defined(HETCOMPUTE_HAVE_QTI_DSP)

            size_t first = r.begin(0);
            size_t last  = r.end(0);

            auto arg_list = std::make_tuple(std::forward<Args>(args)...);

            using kernel_type = std::tuple<KernelFirst, KernelRest...>;
            using arg_type    = decltype(arg_list);
            using idx_type    = size_t;
            using pct_type    = float;
            using time_type   = uint64_t;

            time_type cpu_exec_begin = 0, cpu_exec_end = 0;
            time_type cpu_exec_time = 0;

            // compile time boolean to indicate whether this function was called using kernels
            // generated from a pointkernel.
            HETCOMPUTE_CONSTEXPR_CONST bool is_called_with_pointkernel = called_with_pointkernel;

            // Extract output buffer & Check
            HETCOMPUTE_CONSTEXPR_CONST idx_type out_idx = pattern::utility::extract_output_buf<sizeof...(Args), decltype(arg_list)>::value;
            auto                                output  = std::get<out_idx>(arg_list);

            using buf_type = typename pattern::utility::buffer_data_type<decltype(output)>::data_type;
// FIXME: GL GPU kernels are not supported for now.
#if defined(HETCOMPUTE_HAVE_OPENCL) || defined(HETCOMPUTE_HAVE_QTI_DSP)
            using bufptr_type = hetcompute::buffer_ptr<buf_type>;
#endif

            // Check kernel number
            size_t klist_size = std::tuple_size<kernel_type>::value;
            HETCOMPUTE_API_ASSERT(klist_size > 0 && klist_size <= 3, "Wrong number of kernels provided!");

            // Execute using cpu kernel if tuner is set to serial
            if (tuner.is_serial())
            {
                const size_t ck_idx = hetcompute::internal::pattern::utility::cpu_kernel_pos<kernel_type>::pos;
                HETCOMPUTE_API_ASSERT(ck_idx != invalid_pos, "Serial tuner requires a valid CPU kernel.");
                execute_on_cpu<ck_idx, Dims, kernel_type, arg_type, is_called_with_pointkernel>::kernel_launch(r, klist, arg_list, tuner);
                return;
            }

#if defined(HETCOMPUTE_HAVE_OPENCL) || defined(HETCOMPUTE_HAVE_QTI_DSP)
            idx_type size = r.linearized_distance();
            if (size != output.size())
            {
                HETCOMPUTE_FATAL("Iteration space in heterogeneous pfor_each must match output buffer size!");
            }
            idx_type nd_offset = size / (last - first);
#endif

            // we need stride of the first dimension for the transformed cpu kernel
            // and index calculation
            idx_type stride = r.stride(0);

            HETCOMPUTE_CONSTEXPR_CONST size_t ck_idx = pattern::utility::cpu_kernel_pos<kernel_type>::pos;
            HETCOMPUTE_CONSTEXPR_CONST size_t gk_idx = pattern::utility::gpu_kernel_pos<kernel_type>::pos;
            HETCOMPUTE_CONSTEXPR_CONST size_t hk_idx = pattern::utility::dsp_kernel_pos<kernel_type>::pos;

            if (tuner.has_profile())
            {
                HETCOMPUTE_API_ASSERT(ck_idx != invalid_pos, "Auto tuning requires a valid CPU kernel to be profiled as baseline.");

                HETCOMPUTE_API_ASSERT(last - first >= 3, "Auto tuning is impossible due to insufficient number of iterations.");

                if (ck_idx != invalid_pos && gk_idx != invalid_pos && hk_idx != invalid_pos)
                {
                    tuner.set_cpu_load(33).set_gpu_load(33).set_dsp_load(34);
                }
                else if (ck_idx != invalid_pos && gk_idx != invalid_pos)
                {
                    tuner.set_cpu_load(50).set_gpu_load(50);
                }
                else if (ck_idx != invalid_pos && hk_idx != invalid_pos)
                {
                    tuner.set_cpu_load(50).set_dsp_load(50);
                }
                else
                {
                    tuner.set_cpu_load(100);
                }
            }

            const idx_type cpu_load = tuner.get_cpu_load();
            const idx_type gpu_load = tuner.get_gpu_load();
            const idx_type dsp_load = tuner.get_dsp_load();

            HETCOMPUTE_API_ASSERT(cpu_load + gpu_load + dsp_load == 100, "Incorrect load setting across devices!");
            HETCOMPUTE_API_ASSERT(!(cpu_load > 0 && ck_idx == invalid_pos), "CPU: kernel and tuner load mismatch!");
            HETCOMPUTE_API_ASSERT(!(gpu_load > 0 && gk_idx == invalid_pos), "GPU: kernel and tuner load mismatch!");
            HETCOMPUTE_API_ASSERT(!(dsp_load > 0 && hk_idx == invalid_pos), "Hexagon: Kernel and tuner load mismatch!");
            HETCOMPUTE_API_ASSERT(!(gpu_load > 0 && !have_gpu), "Must use HETCOMPUTE_HAVE_OPENCL to dispatch to GPU!");
            HETCOMPUTE_API_ASSERT(!(dsp_load > 0 && !have_dsp), "Must use HETCOMPUTE_HAVE_QTI_DSP to dispatch to DSP!");

            // enable device only when used
            hetcompute::internal::executor_device_bitset eds;

            // FIXME: this statement should be conditional on cpu load.
            // Currently override_device_sets has to be subset of exector_device,
            // plus our use case always involve buffers declared on cpu.
            // In the future, we should make this conditional on cpu load by
            // removing the restriction of override_device_sets.
            eds.add(executor_device::cpu);
            if (gpu_load > 0)
                // FIXME: use get_executor_device from gpu kernel (for gl).
                eds.add(executor_device::gpucl);
            if (dsp_load > 0)
                eds.add(executor_device::dsp);

                // Acquire all buffer arguments to avoid potential race conditions
                // for input buffers in between sub task launched inside pfor.
#if defined(HETCOMPUTE_HAVE_OPENCL) && defined(HETCOMPUTE_HAVE_QTI_DSP)
            static HETCOMPUTE_CONSTEXPR_CONST size_t num_devices = 3;
#elif defined(HETCOMPUTE_HAVE_OPENCL) || defined(HETCOMPUTE_HAVE_QTI_DSP)
            static HETCOMPUTE_CONSTEXPR_CONST size_t num_devices = 2;
#else
            static HETCOMPUTE_CONSTEXPR_CONST size_t num_devices = 1;
#endif
            HETCOMPUTE_CONSTEXPR_CONST size_t                 num_buffer = pattern::utility::get_num_buffer<arg_type>::value;
            buffer_acquire_set<num_buffer, true, num_devices> bas;
            pattern::utility::add_buffer_in_args<sizeof...(Args), num_buffer, decltype(arg_list), decltype(bas)>::add_buffer(bas, arg_list);

            // A unique ID for bas
            size_t uid = 0;

            auto& output_base_ptr = reinterpret_cast<hetcompute::internal::buffer_ptr_base&>(std::get<out_idx>(arg_list));
            auto  output_bs       = hetcompute::internal::c_ptr(hetcompute::internal::buffer_accessor::get_bufstate(output_base_ptr));

            // global output buffer only acquired by cpu, other devices acquire local buffer.
            hetcompute::internal::override_device_sets<true, 1> ods;
            ods.register_override_device_set(output_bs, { hetcompute::internal::executor_device::cpu });

            bas.acquire_buffers(&uid, eds, false, nullptr, &ods);
            HETCOMPUTE_API_ASSERT(bas.acquired(), "Buffer acquire failure!");

            // calculate computational range for each device
            pct_type gpu_fraction = pct_type(gpu_load) / 100.00;
            pct_type dsp_fraction = pct_type(dsp_load) / 100.00;

            auto gpu_num_iters = idx_type((last - first) * gpu_fraction);
            auto dsp_num_iters = idx_type((last - first) * dsp_fraction);

            // parallelize range of the outer dimension
            // Note that all ranges are end exclusive: [begin, end)
            idx_type gpu_begin = first;
            idx_type gpu_end   = gpu_begin + gpu_num_iters;

            // make sure dsp begin index is of integral multiple of stride
            idx_type dsp_begin = gpu_end % stride == 0 ? gpu_end : normalize_idx(gpu_end, stride);
            idx_type dsp_end   = dsp_begin + dsp_num_iters;

            // make sure cpu begin index is of integral multiple of stride
            // cpu deals with remaining work
            idx_type cpu_begin = dsp_end % stride == 0 ? dsp_end : normalize_idx(dsp_end, stride);
            idx_type cpu_end   = last;
            // Note that cpu_num_iters can be negative as we don't check
            // if cpu_begin is less than last
            int cpu_num_iters = cpu_end - cpu_begin;

            auto* output_arena = bas.find_acquired_arena(output, executor_device::cpu);
            HETCOMPUTE_INTERNAL_ASSERT(output_arena != nullptr, "Invalid arena pointer!");

            auto* optr = static_cast<buf_type*>(arena_storage_accessor::get_ptr(output_arena));
            HETCOMPUTE_INTERNAL_ASSERT(optr != nullptr, "Unexpected null storage in acquired arena!");

#if defined(HETCOMPUTE_HAVE_OPENCL)
            bufptr_type gpu_buffer = std::get<0>(buf_tup);
            ;
            buf_type* gpu_buffer_ptr        = nullptr;
            idx_type  gpu_num_bytes_to_copy = 0;
            if (gpu_num_iters > 0)
            {
                gpu_num_bytes_to_copy = gpu_num_iters * nd_offset * sizeof(buf_type);

                if (!range_check<Dims>::all_single_stride(r))
                {
                    gpu_buffer.acquire_wi();
                    HETCOMPUTE_API_ASSERT(gpu_buffer.host_data() != nullptr, "gpu privatized buffer is not host accessible!");
                    gpu_buffer_ptr = static_cast<buf_type*>(gpu_buffer.host_data());
                    memcpy(gpu_buffer_ptr, optr, gpu_num_bytes_to_copy);
                    gpu_buffer.release();
                }
            }
#endif

#if defined(HETCOMPUTE_HAVE_QTI_DSP)
            std::vector<bufptr_type> dsp_buffer_vec = std::get<1>(buf_tup);
            ;

            // begin/end pair for each dsp thread
            std::vector<buf_type*>                 dsp_buffer_ptr(get_num_dsp_threads());
            std::vector<std::pair<size_t, size_t>> dsp_info_vec;
#endif

            auto g = hetcompute::create_group();

            // First launch gpu kernel
#if defined(HETCOMPUTE_HAVE_OPENCL)
            // we need to hold a task pointer to acquire profile information.
            hetcompute::task_ptr<void(void)> gpu_task;
            if (gpu_num_iters > 0)
            {
                hetcompute::range<Dims> gpu_range = create_gpu_range<Dims>::create(r, gpu_begin, gpu_end);
                execute_on_gpu<gk_idx, out_idx, Dims, kernel_type, arg_type, buf_type, is_called_with_pointkernel>::kernel_launch(gpu_task,
                                                                                                                                  gpu_range,
                                                                                                                                  g,
                                                                                                                                  klist,
                                                                                                                                  arg_list,
                                                                                                                                  gpu_buffer,
                                                                                                                                  tuner);
            }
#endif // defined(HETCOMPUTE_HAVE_OPENCL)

            // Create a copy of arg_list that we pass for the dsp_task. This ensures that the buffer_ptrs remain valid
            // for the duration of the task.
            // This copy is necessary as the cpu kernel directly operates on arglist.
            auto arg_list_for_dsp_task = arg_list;
            // Launch dsp kernel
#if defined(HETCOMPUTE_HAVE_QTI_DSP)
            // we need to hold a task pointer to acquire profile information.
            hetcompute::task_ptr<void(void)> dsp_task;
            if (dsp_num_iters > 0)
            {
                idx_type block = dsp_num_iters / get_num_dsp_threads();
                idx_type extra = dsp_num_iters % get_num_dsp_threads();

                size_t iter_cnt = tuner.is_serial() ? 1 : get_num_dsp_threads();
                for (size_t i = 0; i < iter_cnt; ++i)
                {
                    idx_type begin = dsp_begin + i * block;
                    idx_type end = (i == get_num_dsp_threads() - 1) ? (dsp_begin + (i + 1) * block + extra) : (dsp_begin + (i + 1) * block);
                    // info: dsp_offset[i], dsp_num_bytes_to_copy[i]
                    dsp_info_vec.push_back(std::make_pair((begin * nd_offset), ((end - begin) * nd_offset * sizeof(buf_type))));

                    if (!range_check<Dims>::all_single_stride(r))
                    {
                        dsp_buffer_vec[i].acquire_wi();
                        HETCOMPUTE_API_ASSERT(dsp_buffer_vec[i].host_data() != nullptr, "dsp privatized buffer is not host accessible!");
                        dsp_buffer_ptr[i] = static_cast<buf_type*>(dsp_buffer_vec[i].host_data());
                        memcpy(dsp_buffer_ptr[i] + dsp_info_vec[i].first, optr + dsp_info_vec[i].first, dsp_info_vec[i].second);
                        dsp_buffer_vec[i].release();
                    }
                    hetcompute::range<Dims> dsp_range = create_range<Dims>::create(r, begin, end);

                    execute_on_dsp<hk_idx, out_idx, Dims, kernel_type, arg_type, buf_type>::kernel_launch(dsp_task,
                                                                                                          dsp_range,
                                                                                                          g,
                                                                                                          klist,
                                                                                                          arg_list_for_dsp_task,
                                                                                                          dsp_buffer_vec[i],
                                                                                                          tuner);
                }
            }
#endif // defined(HETCOMPUTE_HAVE_QTI_DSP)

            // Launch cpu kernel
            if (cpu_num_iters > 0)
            {
                hetcompute::range<Dims> cpu_range = create_range<Dims>::create(r, cpu_begin, cpu_end);
                if (tuner.has_profile())
                {
                    cpu_exec_begin = hetcompute_get_time_now();
                }
                execute_on_cpu<ck_idx, Dims, kernel_type, arg_type, is_called_with_pointkernel>::kernel_launch(cpu_range,
                                                                                                               klist,
                                                                                                               arg_list,
                                                                                                               tuner);
                if (tuner.has_profile())
                {
                    cpu_exec_end  = hetcompute_get_time_now();
                    cpu_exec_time = cpu_exec_end - cpu_exec_begin;
                    p->set_cpu_task_time(cpu_exec_time / 1000);
                }
            }

            // synchronize here
            g->wait_for();

            // merge results back to host
#if defined(HETCOMPUTE_HAVE_OPENCL)
            if (gpu_num_iters > 0)
            {
                // explicit sync with host
                gpu_buffer.acquire_ro();
                HETCOMPUTE_API_ASSERT(gpu_buffer.host_data() != nullptr, "gpu privatized buffer is not host accessible!");
                gpu_buffer_ptr = static_cast<buf_type*>(gpu_buffer.host_data());
                memcpy(optr, gpu_buffer_ptr, gpu_num_bytes_to_copy);
                gpu_buffer.release();
            }
#endif // defined(HETCOMPUTE_HAVE_OPENCL)

#if defined(HETCOMPUTE_HAVE_QTI_DSP)
            if (dsp_num_iters > 0)
            {
                // merge dsp results
                size_t iter_cnt = tuner.is_serial() ? 1 : get_num_dsp_threads();

                for (size_t i = 0; i < iter_cnt; ++i)
                {
                    // explicit sync with host
                    dsp_buffer_vec[i].acquire_ro();
                    HETCOMPUTE_API_ASSERT(dsp_buffer_vec[i].host_data() != nullptr, "dsp privatized buffer is not host accessible!");
                    dsp_buffer_ptr[i] = static_cast<buf_type*>(dsp_buffer_vec[i].host_data());
                    memcpy(optr + dsp_info_vec[i].first, dsp_buffer_ptr[i] + dsp_info_vec[i].first, dsp_info_vec[i].second);
                    dsp_buffer_vec[i].release();
                }
            }
#endif // defined(HETCOMPUTE_HAVE_QTI_DSP)

            bas.release_buffers(&uid);

            /// begin of profiling section
#if defined(HETCOMPUTE_HAVE_OPENCL)
            uint64_t gpu_exec_time = 0;
            if (gpu_task != nullptr)
            {
                gpu_exec_time = static_cast<gputask_base*>(c_ptr(gpu_task))->get_exec_time();
                p->set_gpu_task_time(gpu_exec_time / 1000);
            }
#endif // defined(HETCOMPUTE_HAVE_OPENCL)

#if defined(HETCOMPUTE_HAVE_QTI_DSP)
            uint64_t dsp_exec_time = 0;
            if (dsp_task != nullptr)
            {
                dsp_exec_time = static_cast<dsptask_return_layer*>(c_ptr(dsp_task))->get_exec_time();
                p->set_dsp_task_time(dsp_exec_time / 1000);
            }
#endif // defined(HETCOMPUTE_HAVE_QTI_DSP)

            // CPU exec time serves as the normalized profile for all devices (1.0)
            // gpu and dsp profiles are saved as fractions of cpu execution time.
            if (tuner.has_profile())
            {
                // turn all time measurements to microseconds
                uint64_t tscale            = 1e3;
                double   profile_threshold = 10.0;
                double   epsilon           = 1e-3;
                HETCOMPUTE_UNUSED(epsilon);

                HETCOMPUTE_API_ASSERT(cpu_num_iters > 0, "Profile: Unexpected number of CPU iterations!");
                double cput = static_cast<double>(cpu_exec_time / tscale);
                if (cput < profile_threshold)
                {
                    HETCOMPUTE_ILOG("Warning: CPU profile in tiny granularity, abort profiling...");
                    return;
                }

#if defined(HETCOMPUTE_HAVE_OPENCL)
                if (gpu_load > 0 && gk_idx != invalid_pos)
                {
                    double gpu_relative_time          = static_cast<double>(gpu_exec_time) / static_cast<double>(cpu_exec_time);
                    double throughput_coefficient_gpu = 0;
                    HETCOMPUTE_API_ASSERT(gpu_num_iters > 0, "Profile: Unexpected number of GPU iterations!");
                    throughput_coefficient_gpu = static_cast<double>(cpu_num_iters) / static_cast<double>(gpu_num_iters);
                    if (gpu_relative_time < epsilon)
                    {
                        HETCOMPUTE_ILOG("Warning: GPU profile in tiny granularity, abort profiling...");
                    }
                    else
                    {
                        p->set_gpu_profile(gpu_relative_time * throughput_coefficient_gpu);
                    }
                }
#endif // defined(HETCOMPUTE_HAVE_OPENCL)

#if defined(HETCOMPUTE_HAVE_QTI_DSP)
                if (dsp_load > 0 && hk_idx != invalid_pos)
                {
                    double dsp_relative_time          = static_cast<double>(dsp_exec_time) / static_cast<double>(cpu_exec_time);
                    double throughput_coefficient_dsp = 0;
                    HETCOMPUTE_API_ASSERT(dsp_num_iters > 0, "Profile: Unexpected number of DSP iterations!");
                    throughput_coefficient_dsp = static_cast<double>(cpu_num_iters) / static_cast<double>(dsp_num_iters);
                    if (dsp_relative_time < epsilon)
                    {
                        HETCOMPUTE_ILOG("Warning: Hexagon profile in tiny granularity, abort profiling...");
                    }
                    else
                    {
                        p->set_dsp_profile(dsp_relative_time * throughput_coefficient_dsp);
                    }
                }
#endif // defined(HETCOMPUTE_HAVE_QTI_DSP)

                p->add_run();
            }
            /// end of profiling section
        }

        // pfor_each helper methods that help with the creation of the local output buffers.
        // A single gpu local buffer, and a vector of dsp local buffers are created, and then
        // passed to the internal::pfor_each function.
        // In the following case, where a tuple of kernels is passed in, new local buffers are created
        // each time.
        template <typename KernelTuple, typename ArgTuple, size_t Dims, size_t... Indices>
        void pfor_each_run_helper(hetcompute::pattern::pfor<KernelTuple, ArgTuple>* const p,
                                  const hetcompute::range<Dims>&                          r,
                                  KernelTuple&                                            klist,
                                  hetcompute::pattern::tuner&                             t,
                                  hetcompute::internal::integer_list_gen<Indices...>,
                                  ArgTuple& atpl)
        {
            // Check if there are multiple output buffers
            auto num_output_buf = hetcompute::internal::pattern::utility::num_output_buffer_in_tuple<ArgTuple>::value;
            HETCOMPUTE_API_ASSERT(num_output_buf == 1, "Requires one and only one output buffer in parameter list.");

            // Extract output buffer & Check
            HETCOMPUTE_CONSTEXPR_CONST size_t out_idx =
                hetcompute::internal::pattern::utility::extract_output_buf<std::tuple_size<ArgTuple>::value, ArgTuple>::value;
            HETCOMPUTE_API_ASSERT(out_idx != hetcompute::internal::pattern::utility::invalid_pos,
                                  "Requires at least one output buffer in the argument list!");
            auto output = std::get<out_idx>(atpl);

#if defined(HETCOMPUTE_HAVE_OPENCL) || defined(HETCOMPUTE_HAVE_QTI_DSP)
            using buf_type = typename hetcompute::internal::pattern::utility::buffer_data_type<decltype(output)>::data_type;
#endif
            size_t buffer_size = r.linearized_distance();
            HETCOMPUTE_UNUSED(buffer_size);
#ifdef HETCOMPUTE_HAVE_OPENCL
            auto gpu_local_buffer = hetcompute::create_buffer<buf_type>(buffer_size);
#endif
#ifdef HETCOMPUTE_HAVE_QTI_DSP
            std::vector<hetcompute::buffer_ptr<buf_type>> dsp_buffer_vec(get_num_dsp_threads());
            for (size_t i = 0; i < get_num_dsp_threads(); i++)
            {
                dsp_buffer_vec[i] = hetcompute::create_buffer<buf_type>(buffer_size, { hetcompute::dsp });
            }
#endif

            auto local_buffer_tuple = std::make_tuple(
#if defined(HETCOMPUTE_HAVE_OPENCL) && defined(HETCOMPUTE_HAVE_QTI_DSP)
                gpu_local_buffer, dsp_buffer_vec
#elif defined(HETCOMPUTE_HAVE_OPENCL)
                gpu_local_buffer
#elif defined(HETCOMPUTE_HAVE_QTI_DSP)
                dsp_buffer_vec
#else
            // create empty tuple
#endif
            );

            hetcompute::internal::pfor_each(p,
                                            r,
                                            klist,
                                            t,
                                            hetcompute::internal::buffer::utility::constant<bool, false>(),
                                            std::move(local_buffer_tuple),
                                            std::get<Indices - 1>(std::forward<ArgTuple>(atpl))...);
        }

        // In the following case, when the function is called with a pointkernel object, then the local buffer
        // within the pointkernel object is examined to determine whether it can be reused. If it has not yet been
        // allocated, or is smaller than necessary, then a new buffer is allocated. These newly allocated buffers are
        // then stored in the pointkernel object for reuse.
        template <typename RT, typename... PKArgs, typename ArgTuple, size_t Dims, size_t... Indices>
        void pfor_each_run_helper(hetcompute::pattern::pfor<pointkernel::pointkernel<RT, PKArgs...>, ArgTuple>* const p,
                                  const hetcompute::range<Dims>&                                                      r,
                                  pointkernel::pointkernel<RT, PKArgs...>&                                            pk,
                                  hetcompute::pattern::tuner&                                                         t,
                                  hetcompute::internal::integer_list_gen<Indices...>,
                                  ArgTuple& atpl)
        {
            // Check if there are multiple output buffers
            auto num_output_buf = hetcompute::internal::pattern::utility::num_output_buffer_in_tuple<ArgTuple>::value;
            HETCOMPUTE_API_ASSERT(num_output_buf == 1, "Requires one and only one output buffer in parameter list.");

            auto klist = std::make_tuple(pk._cpu_kernel
#ifdef HETCOMPUTE_HAVE_OPENCL
                                         ,
                                         pk._gpu_kernel
#endif
#ifdef HETCOMPUTE_HAVE_QTI_DSP
                                         ,
                                         pk._dsp_kernel
#endif
            );
            // size of temporary output buffers
            size_t buffer_size = r.linearized_distance();
            HETCOMPUTE_UNUSED(buffer_size);
#ifdef HETCOMPUTE_HAVE_OPENCL
            if (t.get_gpu_load() > 0)
            {
                auto gpu_buffer            = pk._gpu_local_buffer;
                using gpu_buffer_base_type = typename hetcompute::internal::buffer::utility::buffertraits<decltype(gpu_buffer)>::element_type;
                if (gpu_buffer == nullptr || gpu_buffer.size() < buffer_size)
                {
                    // gpu local buffer has not yet been allocated or is smaller than necessary.
                    // Allocate a new buffer
                    gpu_buffer = hetcompute::create_buffer<gpu_buffer_base_type>(buffer_size);
                    // store the newly allocated buffer in the pointkernel object.
                    pk._gpu_local_buffer = gpu_buffer;
                }
            }
#endif
#ifdef HETCOMPUTE_HAVE_QTI_DSP
            if (t.get_dsp_load() > 0)
            {
                auto dsp_buffer_vec        = pk._dsp_local_buffer_vec;
                using dsp_buffer_base_type = typename hetcompute::internal::buffer::utility::buffertraits<
                    typename std::remove_reference<decltype(dsp_buffer_vec[0])>::type>::element_type;
                for (size_t i = 0; i < dsp_buffer_vec.size(); i++)
                {
                    auto dsp_buffer = dsp_buffer_vec[i];
                    if (dsp_buffer == nullptr || dsp_buffer.size() < buffer_size)
                    {
                        // dsp local buffer has not yet been allocated or is smaller than necessary.
                        // Allocate a new buffer
                        dsp_buffer                  = hetcompute::create_buffer<dsp_buffer_base_type>(buffer_size, { hetcompute::dsp });
                        pk._dsp_local_buffer_vec[i] = dsp_buffer;
                    }
                }
            }
#endif

            auto local_buffer_tuple = std::make_tuple(
#if defined(HETCOMPUTE_HAVE_OPENCL) && defined(HETCOMPUTE_HAVE_QTI_DSP)
                pk._gpu_local_buffer, pk._dsp_local_buffer_vec
#elif defined(HETCOMPUTE_HAVE_OPENCL)
                pk._gpu_local_buffer
#elif defined(HETCOMPUTE_HAVE_QTI_DSP)
                pk._dsp_local_buffer_vec
#else
            // create empty tuple
#endif
            );
            hetcompute::internal::pfor_each(p,
                                            r,
                                            klist,
                                            t,
                                            hetcompute::internal::buffer::utility::constant<bool, true>(),
                                            std::move(local_buffer_tuple),
                                            std::get<Indices - 1>(std::forward<ArgTuple>(atpl))...);
        }

    }; // namespace internal
};     // namespace hetcompute
