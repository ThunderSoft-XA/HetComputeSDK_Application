#pragma once

#ifdef HETCOMPUTE_HAVE_OPENCL

#include <hetcompute/range.hh>

namespace hetcompute
{
    namespace internal
    {
        // BWAG - Body with attributes GPU
        template <typename BWAG>
        void pfor_each_gpu(group* group, size_t first, size_t last, BWAG bwag)
        {
            if (first >= last)
            {
                return;
            }

            auto g_ptr = c_ptr(group);
            if (g_ptr && g_ptr->is_canceled())
            {
                return;
            }

            hetcompute::range<1> r(first, last);
            hetcompute::range<1> l;
            pfor_each_gpu(r, l, bwag);
        }

        template <size_t Dims, typename BWAG>
        void pfor_each_gpu(const hetcompute::range<Dims>& r, const hetcompute::range<Dims>& l, BWAG bwag)
        {
            typedef typename BWAG::kernel_parameters k_params;
            typedef typename BWAG::kernel_arguments  k_args;

            // get devices.
            auto      d_ptr  = c_ptr((*internal::s_dev_ptrs)[0]);
            auto      k_ptr  = c_ptr(bwag.get_gpu_kernel());
            clkernel* kernel = k_ptr->get_cl_kernel();

            // create a guaranteed unique requestor id
            char  dummy;
            void* requestor = &dummy;

            buffer_acquire_set<num_buffer_ptrs_in_tuple<k_args>::value> bas;

            gpu_kernel_dispatch::prepare_args<k_params, k_args>((*s_dev_ptrs)[0],
                                                                k_ptr,
                                                                std::forward<k_args>(bwag.get_cl_kernel_args()),
                                                                bas,
                                                                requestor);

            // launch kernel.
            clevent event = d_ptr->launch_kernel(kernel, r, l);
            event.wait();

            bas.release_buffers(requestor);

#ifdef HETCOMPUTE_HAVE_OPENCL_PROFILING
            HETCOMPUTE_ILOG("cl::CommandQueue::enqueueNDRangeKernel() time = %0.6f us", event.get_time_ns() / 1000.0);
#endif // HETCOMPUTE_HAVE_OPENCL_PROFILING
        }

    }; // namespace internal
};     // namespace hetcompute

#endif // HETCOMPUTE_HAVE_OPENCL
