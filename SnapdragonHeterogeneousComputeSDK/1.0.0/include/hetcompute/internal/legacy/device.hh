#pragma once

#ifdef HETCOMPUTE_HAVE_OPENCL

#include <hetcompute/internal/device/cldevice.hh>
#include <hetcompute/internal/legacy/types.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace legacy
        {
            // A wrapper around internal::get_devices, used by runtime. Can be easily replaced with internal version.
            template <typename Attribute>
            inline void get_devices(Attribute d_type, std::vector<device_ptr>* devices)
            {
                HETCOMPUTE_INTERNAL_ASSERT(devices, "null vector passed to get_devices");
                internal::get_devices(d_type, devices);
            }

            // A wrapper to get device-specific attribute. Also used by runtime and can be easily replaced.
            template <typename Attribute>
            auto get_device_attribute(device_ptr device, Attribute attr) -> typename internal::device_attr<Attribute>::type
            {
                auto d_ptr = internal::c_ptr(device);
                HETCOMPUTE_API_ASSERT(d_ptr, "null device_ptr");
                return d_ptr->get(std::forward<Attribute>(attr));
            }

        }; // namespace hetcompute::internal::legacy

    }; // namespace hetcompute::internal

}; // namespace hetcompute

#endif // HETCOMPUTE_HAVE_OPENCL
