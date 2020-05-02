/** @file cldevice.hh */
#pragma once

#ifdef HETCOMPUTE_HAVE_OPENCL

#include <hetcompute/internal/device/gpuopencl.hh>
#include <hetcompute/internal/attr.hh>
#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/graphics/egl.hh>
#include <hetcompute/internal/legacy/attr.hh>
#include <hetcompute/internal/legacy/types.hh>
#include <hetcompute/range.hh>

namespace hetcompute
{
    namespace internal
    {
        class clkernel;
        class clevent;

        // GPU devices on the platform.
        extern std::vector<legacy::device_ptr>* s_dev_ptrs;

        struct cldevice_deleter
        {
            static void release(cldevice* g);
        };

        inline legacy::device_ptr
        get_default_cldevice()
        {
            HETCOMPUTE_INTERNAL_ASSERT(s_dev_ptrs != nullptr, "cldevices not setup");
            HETCOMPUTE_INTERNAL_ASSERT(s_dev_ptrs->size() >= 1, "there must be at least one cldevice set up");
            return s_dev_ptrs->at(0);
        }

        // gpu device currently based on opencl backend.
        // reference counted and client would get a legacy::device_ptr.
        class cldevice : public ref_counted_object<cldevice, hetcomputeptrs::default_logger, cldevice_deleter>
        {
        private:
            cl::Platform     _ocl_platform;
            cl::Device       _ocl_device;
            cl::Context      _ocl_context;
            cl::CommandQueue _ocl_cmd_queue;
            cl::NDRange      _ocl_global;
            cl::NDRange      _ocl_local;
            cl::NDRange      _ocl_offset;

            cl_context_properties create_context_properties();

        public:
            cl::Context get_context()
            {
                return _ocl_context;
            }

            cl::CommandQueue get_cmd_queue()
            {
                return _ocl_cmd_queue;
            }

            void copy_to_device_blocking(const cl::Buffer buffer, const void* ptr, size_t size);
            void copy_from_device_blocking(const cl::Buffer buffer, void* ptr, size_t size);
            void copy_to_device_async(const cl::Buffer buffer, const void* ptr, size_t size, cl::Event& write_event);
            void copy_from_device_async(const cl::Buffer buffer, void* ptr, size_t size, cl::Event& read_event);

            void* map_async(const cl::Buffer buffer, size_t size, cl::Event& read_event);
            void unmap_async(const cl::Buffer buffer, void* mapped_ptr, cl::Event& write_event);

            cldevice(const cl::Platform& ocl_platform, const cl::Device& ocl_device)
                : _ocl_platform(ocl_platform),
                  _ocl_device(ocl_device),
                  _ocl_context(),
                  _ocl_cmd_queue(),
                  _ocl_global(),
                  _ocl_local(),
                  _ocl_offset()
            {
#ifdef HETCOMPUTE_HAVE_GLES
                HETCOMPUTE_GCC_IGNORE_BEGIN("-Wold-style-cast")
                cl_context_properties ctx_props[] = { CL_GL_CONTEXT_KHR,
                                                      (cl_context_properties)egl::get_instance().get_context(),
                                                      CL_EGL_DISPLAY_KHR,
                                                      (cl_context_properties)egl::get_instance().get_display(),
                                                      CL_CONTEXT_PLATFORM,
                                                      (cl_context_properties)(_ocl_platform)(),
                                                      0 };
                HETCOMPUTE_GCC_IGNORE_END("-Wold-style-cast")
#endif // HETCOMPUTE_HAVE_GLES

                // create context
                cl_int status;

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                try
                {
                    _ocl_context = cl::Context(ocl_device,
#ifdef HETCOMPUTE_HAVE_GLES
                                               ctx_props,
#else  // HETCOMPUTE_HAVE_GLES
                                           nullptr,
#endif // HETCOMPUTE_HAVE_GLES
                                               nullptr,
                                               nullptr,
                                               &status);
                }
                catch (cl::Error& err)
                {
                    HETCOMPUTE_FATAL("cl::Context()->%s", get_cl_error_string(err.err()));
                }
#else
                _ocl_context = cl::Context(ocl_device,
#ifdef HETCOMPUTE_HAVE_GLES
                                           ctx_props,
#else  // HETCOMPUTE_HAVE_GLES
                                           nullptr,
#endif // HETCOMPUTE_HAVE_GLES
                                           nullptr,
                                           nullptr,
                                           &status);
                if (status != CL_SUCCESS)
                {
                    HETCOMPUTE_FATAL("cl::Context()->%s", get_cl_error_string(status));
                }
#endif

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                // create command queue.
                try
                {
#ifdef HETCOMPUTE_HAVE_OPENCL_PROFILING
                    _ocl_cmd_queue = cl::CommandQueue(_ocl_context, _ocl_device, CL_QUEUE_PROFILING_ENABLE, &status);
#else  // HETCOMPUTE_HAVE_OPENCL_PROFILING
                    _ocl_cmd_queue = cl::CommandQueue(_ocl_context, _ocl_device, 0, &status);
#endif // HETCOMPUTE_HAVE_OPENCL_PROFILING
                }
                catch (cl::Error& err)
                {
                    HETCOMPUTE_FATAL("cl::CommandQueue()->%s", get_cl_error_string(err.err()));
                }
                catch (...)
                {
                    HETCOMPUTE_FATAL("Unknown error in cl::CommandQueue()");
                }
#else

#ifdef HETCOMPUTE_HAVE_OPENCL_PROFILING
                _ocl_cmd_queue = cl::CommandQueue(_ocl_context, _ocl_device, CL_QUEUE_PROFILING_ENABLE, &status);
#else  // HETCOMPUTE_HAVE_OPENCL_PROFILING
                _ocl_cmd_queue = cl::CommandQueue(_ocl_context, _ocl_device, 0, &status);
#endif // HETCOMPUTE_HAVE_OPENCL_PROFILING

                if (status != CL_SUCCESS)
                {
                    HETCOMPUTE_FATAL("cl::CommandQueue()->%s", get_cl_error_string(status));
                }
#endif
            }

            explicit cldevice(const cl::Device& ocl_device) : cldevice(cl::Platform::getDefault(), ocl_device)
            {
            }

            cldevice() : cldevice(cl::Platform::getDefault(), cl::Device::getDefault())
            {
            }

            template<size_t Dims>
            void build_launch_args(const ::hetcompute::range<Dims>& r, const ::hetcompute::range<Dims>& lr=::hetcompute::range<Dims>());

            template<size_t Dims>
            void check_launch_args(size_t work_group_size, const ::hetcompute::range<Dims>& r);

            template<size_t Dims>
            clevent launch_kernel(const clkernel* kernel,
                                  const ::hetcompute::range<Dims>& r,
                                  const ::hetcompute::range<Dims>& lr= ::hetcompute::range<Dims>(),
                                  HETCOMPUTE_CL_VECTOR_CLASS<cl::Event>* events=nullptr);

            inline cl::Device get_impl()
            {
                return _ocl_device;
            }

            void flush()
            {
                cl_int status = _ocl_cmd_queue.finish();
                HETCOMPUTE_DLOG("cl::CommandQueue::finish()->%d", status);
                HETCOMPUTE_INTERNAL_ASSERT(status == CL_SUCCESS, "cl::CommandQueue()::finish()->%d", status);
            }

            // function parameter used only for template argument deduction.
            // get(hetcompute::device_attr::endian_little)
            // Attribute will be deduced as internal::device_attr_endian_little.
            // it would use the specialization
            // internal::device_attr<device_attr_endian_little>
            template <typename Attribute>
            auto get(Attribute) -> typename internal::device_attr<Attribute>::type
            {
                return _ocl_device.getInfo<internal::device_attr<Attribute>::cl_name>();
            }

            friend class clkernel;
            friend struct access_cldevice;
            friend struct cldevice_deleter;

        }; // class cldevice

        inline void cldevice_deleter::release(cldevice* d)
        {
            /// All the pending commands to the device are flushed before
            /// returning from operator ()
            cl_int status = d->_ocl_cmd_queue.finish();
            HETCOMPUTE_DLOG("cl::CommandQueue::finish()->%d", status);
            HETCOMPUTE_INTERNAL_ASSERT(status == CL_SUCCESS, "cl::CommandQueue::finish()->%d", status);
            if (status == CL_SUCCESS)
            {
                delete d;
            }
        }

        struct access_cldevice
        {
            static cl::Device& get_cl_device(legacy::device_ptr& p)
            {
                auto praw = c_ptr(p.get_raw_ptr());
                HETCOMPUTE_INTERNAL_ASSERT(praw != nullptr, "device_ptr is not set");
                return praw->_ocl_device;
            }

            static cl::Context& get_cl_context(legacy::device_ptr& p)
            {
                auto praw = c_ptr(p.get_raw_ptr());
                HETCOMPUTE_INTERNAL_ASSERT(praw != nullptr, "device_ptr is not set");
                return praw->_ocl_context;
            }

            static cl::CommandQueue& get_cl_cmd_queue(legacy::device_ptr& p)
            {
                auto praw = c_ptr(p.get_raw_ptr());
                HETCOMPUTE_INTERNAL_ASSERT(praw != nullptr, "device_ptr is not set");
                return praw->_ocl_cmd_queue;
            }
        };

        void get_devices(device_attr_device_type_gpu, std::vector<legacy::device_ptr>* devices);

    };  // namespace internal
};  // namespace hetcompute


#endif // HETCOMPUTE_HAVE_OPENCL

