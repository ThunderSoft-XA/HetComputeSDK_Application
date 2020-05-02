/** @file clevent.hh */
#pragma once

#ifdef HETCOMPUTE_HAVE_OPENCL

#include <hetcompute/internal/device/gpuopencl.hh>

namespace hetcompute
{
    namespace internal
    {
        class clevent
        {
        private:
            cl::Event _ocl_event;

        public:
            clevent() : _ocl_event()
            {
            }

            explicit clevent(const cl::Event& event) : _ocl_event(event)
            {
            }

            inline cl::Event get_impl()
            {
                return _ocl_event;
            }

            inline void wait()
            {
                _ocl_event.wait();
            }

            double get_time_ns()
            {
                cl_ulong time_start, time_end;
                _ocl_event.getProfilingInfo(CL_PROFILING_COMMAND_START, &time_start);
                _ocl_event.getProfilingInfo(CL_PROFILING_COMMAND_END, &time_end);
                HETCOMPUTE_INTERNAL_ASSERT((time_end >= time_start), "negative execution time for opencl command");
                return (time_end - time_start);
            }
        };

    };  // namespace internal
};  // namespace hetcompute

#endif // HETCOMPUTE_HAVE_OPENCL
