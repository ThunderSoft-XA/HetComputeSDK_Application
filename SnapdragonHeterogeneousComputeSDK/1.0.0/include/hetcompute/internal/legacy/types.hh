#pragma once

#include <hetcompute/internal/util/hetcomputeptrs.hh>

namespace hetcompute
{
    namespace internal
    {
        class group;
        class dspbuffer;

#ifdef HETCOMPUTE_HAVE_OPENCL
        class cldevice;
#endif // HETCOMPUTE_HAVE_OPENCL

        namespace legacy
        {
/** @cond IGNORE_SHARED_PTR */
#ifdef HETCOMPUTE_HAVE_GPU

            template <typename T>
            class buffer;

            template <typename... Kargs>
            class gpukernel;

#ifdef HETCOMPUTE_HAVE_OPENCL
            // An std::shared_ptr-like managed pointer to an OpenCL device object.
            typedef internal::hetcompute_shared_ptr<internal::cldevice> device_ptr;
#else  // HETCOMPUTE_HAVE_OPENCL
            // An object type passed around in the OpenGLES dispatch layers without being accessed.
            typedef struct dummy_device_ptr
            {
            } device_ptr;
#endif // HETCOMPUTE_HAVE_OPENCL

            template <typename... Kargs>
            using kernel_ptr = internal::hetcompute_shared_ptr<::hetcompute::internal::legacy::gpukernel<Kargs...>>;

#endif // HETCOMPUTE_HAVE_GPU

#ifdef HETCOMPUTE_HAVE_QTI_DSP
            typedef internal::hetcompute_shared_ptr<internal::dspbuffer> dspbuffer_ptr;
#endif // HETCOMPUTE_HAVE_QTI_DSP

            /** @endcond */
        }; // namespace hetcompute::internal::legacy
    };     // namespace hetcompute::internal
};         // namespace hetcompute
