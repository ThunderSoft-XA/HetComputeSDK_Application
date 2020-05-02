/** @file clkernel.hh */
#pragma once

#ifdef HETCOMPUTE_HAVE_OPENCL

#include <mutex>

// Include user-visible headers first
#include <hetcompute/texture.hh>

#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/device/cldevice.hh>
#include <hetcompute/internal/device/gpuopencl.hh>

namespace hetcompute
{
    namespace internal
    {
        // GPU devices on the platform.
        extern std::vector<legacy::device_ptr>* s_dev_ptrs;

        class clkernel
        {
        private:
            cl::Program _ocl_program;
            cl::Kernel  _ocl_kernel;
            size_t      _opt_local_size;
            std::mutex  _dispatch_mutex;

        public:
            clkernel(legacy::device_ptr const& device, const std::string& task_str, const std::string& task_name, const std::string& build_options)
                : _ocl_program(), _ocl_kernel(), _opt_local_size(0), _dispatch_mutex()
            {
                auto d_ptr = internal::c_ptr(device);
                HETCOMPUTE_INTERNAL_ASSERT((d_ptr != nullptr), "null device ptr");

                //create program.
                cl_int status;

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                try
                {
#endif
                    _ocl_program = cl::Program(d_ptr->get_context(), task_str, false, &status);

                    // build program.
                    HETCOMPUTE_CL_VECTOR_CLASS<cl::Device> devices;
                    devices.push_back(d_ptr->get_impl());
                    status = _ocl_program.build(devices, build_options.c_str());

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                }
                catch (cl::Error& err)
                {
                    HETCOMPUTE_CL_STRING_CLASS build_log;
                    _ocl_program.getBuildInfo(d_ptr->get_impl(), CL_PROGRAM_BUILD_LOG, &build_log);
                    HETCOMPUTE_FATAL("cl::Program::build()->%s\n build_log: %s", get_cl_error_string(err.err()), build_log.c_str());
                }
                catch (...)
                {
                    HETCOMPUTE_FATAL("Unknown error in cl::Program::build()");
                }
#else
                if (status != CL_SUCCESS)
                {
                    HETCOMPUTE_CL_STRING_CLASS build_log;
                    _ocl_program.getBuildInfo(d_ptr->get_impl(), CL_PROGRAM_BUILD_LOG, &build_log);
                    HETCOMPUTE_FATAL("cl::Program::build()->%s\n build_log: %s", get_cl_error_string(status), build_log.c_str());
                }
#endif

                // create kernel

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                try
                {
#endif
                    _ocl_kernel     = cl::Kernel(_ocl_program, task_name.c_str(), &status);
                    _opt_local_size = _ocl_kernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(d_ptr->get_impl(), &status);

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                }
                catch (cl::Error& err)
                {
                    HETCOMPUTE_FATAL("cl::Kernel()->%s", get_cl_error_string(err.err()));
                }
                catch (...)
                {
                    HETCOMPUTE_FATAL("Unknown error in cl::Kernel()");
                }
#else
                if (status != CL_SUCCESS)
                {
                    HETCOMPUTE_FATAL("cl::Kernel()->%s", get_cl_error_string(status));
                }
#endif
            }

            clkernel(legacy::device_ptr const& device,
                     void const*               kernel_bin,
                     size_t                    kernel_size,
                     const std::string&        kernel_name,
                     const std::string&        build_options)
                : _ocl_program(), _ocl_kernel(), _opt_local_size(0), _dispatch_mutex()
            {
                auto d_ptr = internal::c_ptr(device);
                HETCOMPUTE_INTERNAL_ASSERT((d_ptr != nullptr), "null device ptr");
                HETCOMPUTE_CL_VECTOR_CLASS<cl::Device> devices;
                cl_int                               status;

                // create program.
#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                try
                {
#endif
                    devices.push_back(d_ptr->get_impl());
                    HETCOMPUTE_CL_VECTOR_CLASS<std::pair<void const*, size_t>> binaries;
                    binaries.push_back(std::make_pair(kernel_bin, kernel_size));
                    _ocl_program = cl::Program(d_ptr->get_context(), devices, binaries, nullptr, &status);
#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                }
                catch (cl::Error& err)
                {
                    HETCOMPUTE_FATAL("cl::Program(binary %p, length %zu)->%s", kernel_bin, kernel_size, get_cl_error_string(err.err()));
                }
                catch (...)
                {
                    HETCOMPUTE_FATAL("Unknown error in creating a program");
                }
#else
                if (status != CL_SUCCESS)
                {
                    HETCOMPUTE_FATAL("cl::Program(binary %p, length %zu)->%s", kernel_bin, kernel_size, get_cl_error_string(status));   
                }
#endif

                // build program.

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                try
                {
#endif
                    status = _ocl_program.build(devices, build_options.c_str());

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                }
                catch (cl::Error& err)
                {
                    HETCOMPUTE_CL_STRING_CLASS build_log;
                    _ocl_program.getBuildInfo(d_ptr->get_impl(), CL_PROGRAM_BUILD_LOG, &build_log);
                    HETCOMPUTE_FATAL("cl::Program::build()->%s\n build_log: %s", get_cl_error_string(err.err()), build_log.c_str());
                }
                catch (...)
                {
                    HETCOMPUTE_FATAL("Unknown error in cl::Program::build()");
                }
#else
                if (status != CL_SUCCESS)
                {
                    HETCOMPUTE_CL_STRING_CLASS build_log;
                    _ocl_program.getBuildInfo(d_ptr->get_impl(), CL_PROGRAM_BUILD_LOG, &build_log);
                    HETCOMPUTE_FATAL("cl::Program::build()->%s\n build_log: %s", get_cl_error_string(status), build_log.c_str());
                }
#endif


                // create kernel
#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                try
                {
#endif
                    _ocl_kernel     = cl::Kernel(_ocl_program, kernel_name.c_str(), &status);
                    _opt_local_size = _ocl_kernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(d_ptr->get_impl(), &status);

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                }
                catch (cl::Error& err)
                {
                    HETCOMPUTE_FATAL("cl::Kernel()->%s", get_cl_error_string(err.err()));
                }
                catch (...)
                {
                    HETCOMPUTE_FATAL("Unknown error in cl::Kernel()");
                }
#else
                if (status != CL_SUCCESS)
                {
                    HETCOMPUTE_FATAL("cl::Kernel()->%s", get_cl_error_string(status));
                }
#endif
            }

            inline size_t get_optimal_local_size() const
            {
                return _opt_local_size;
            }

            inline const cl::Kernel get_impl() const
            {
                return _ocl_kernel;
            }

            std::pair<void const*, size_t> get_cl_kernel_binary() const
            {
                void*  bin_ptr;
                size_t bin_size;

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                try
                {
#endif
                    cl_uint ndev;
                    _ocl_program.getInfo(CL_PROGRAM_NUM_DEVICES, &ndev);

                    if (ndev == 0)
                    {
                        HETCOMPUTE_FATAL("No OpenCL devices found");
                    }

                    auto sizes = new size_t[ndev];
                    _ocl_program.getInfo(CL_PROGRAM_BINARY_SIZES, sizes);
                    bin_size = sizes[0];

                    auto binaries = new unsigned char*[ndev];

                    // We only use the binary from the first device and ignore the rest.
                    binaries[0] = new unsigned char[bin_size];
                    for (size_t i   = 1; i < ndev; i++)
                    {
                        binaries[i] = nullptr;
                    }
                    _ocl_program.getInfo(CL_PROGRAM_BINARIES, binaries);
                    bin_ptr = binaries[0];

                    delete[] sizes;
                    delete[] binaries;
#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                }
                catch (cl::Error& err)
                {
                    HETCOMPUTE_FATAL("cl::Program::getInfo()->%s", get_cl_error_string(err.err()));
                }
                catch (...)
                {
                    HETCOMPUTE_FATAL("Unknown error in cl::Program::getInfo()");
                }
#endif
                return std::make_pair(bin_ptr, bin_size);
            }

            inline std::mutex& access_dispatch_mutex()
            {
                return _dispatch_mutex;
            }

            // handle texture types.
            inline void set_arg(size_t arg_index, hetcompute::graphics::internal::base_texture_cl* tex_cl)
            {
                HETCOMPUTE_INTERNAL_ASSERT(tex_cl != nullptr, "null texture_cl");

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                try
                {
#endif
                    // set kernel arg
                    cl_int status = _ocl_kernel.setArg(arg_index, tex_cl->_img);
                    HETCOMPUTE_UNUSED(status);
                    HETCOMPUTE_DLOG("cl::Kernel::setArg(%zu, texture %p)->%d", arg_index, tex_cl, status);

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                }
                catch (cl::Error& err)
                {
                    HETCOMPUTE_FATAL("cl::Kernel::setArg(%zu, texture %p)->%s", arg_index, tex_cl, get_cl_error_string(err.err()));
                }
#else
                if (status != CL_SUCCESS)
                {
                    HETCOMPUTE_FATAL("cl::Kernel::setArg(%zu, texture %p)->%s", arg_index, tex_cl, get_cl_error_string(status));
                }
#endif
            }

            // handle sampler types.
            template <hetcompute::graphics::addressing_mode addrMode, hetcompute::graphics::filter_mode filterMode>
            inline void set_arg(size_t arg_index, hetcompute::graphics::sampler_ptr<addrMode, filterMode> const& sampler_ptr)
            {
                auto const& psampler = c_ptr(sampler_ptr);
                HETCOMPUTE_INTERNAL_ASSERT((psampler != nullptr), "null sampler_cl");

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                try
                {
#endif
                    // set kernel arg
                    cl_int status = clSetKernelArg(_ocl_kernel(), arg_index, sizeof(cl_sampler), static_cast<void*>(&(psampler->_sampler)));
                    if (status != CL_SUCCESS)
                        HETCOMPUTE_FATAL("clSetKernelArg(%zu, sampler %p)->%d", arg_index, psampler, status);

                    HETCOMPUTE_DLOG("cl::Kernel::setArg(%zu, sampler %p)->%d", arg_index, psampler, status);
#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                }
                catch (cl::Error& err)
                {
                    HETCOMPUTE_FATAL("cl::Kernel::setArg(%zu, sampler %p)->%s", arg_index, psampler, get_cl_error_string(err.err()));
                }
#else
                if (status != CL_SUCCESS)
                {
                    HETCOMPUTE_FATAL("cl::Kernel::setArg(%zu, sampler %p)->%s", arg_index, psampler, get_cl_error_string(status));
                }
#endif
            }

            // handle cl::Buffer types.
            inline void set_arg(size_t arg_index, cl::Buffer const& cl_buffer)
            {
#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                try
                {
#endif
                    // set kernel arg
                    cl_int status = _ocl_kernel.setArg(arg_index, cl_buffer);
                    HETCOMPUTE_UNUSED(status);
                    HETCOMPUTE_DLOG("cl::Kernel::setArg(%zu, %p)->%d", arg_index, &cl_buffer, status);
#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                }
                catch (cl::Error& err)
                {
                    HETCOMPUTE_FATAL("cl::Kernel::setArg(%zu, %p)->%s", arg_index, &cl_buffer, get_cl_error_string(err.err()));
                }
#else
                if (status != CL_SUCCESS)
                {
                    HETCOMPUTE_FATAL("cl::Kernel::setArg(%zu, %p)->%s", arg_index, &cl_buffer, get_cl_error_string(status));
                }
#endif
            }

            // handle value types.
            template <typename T>
            void set_arg(size_t arg_index, T value)
            {
#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                try
                {
#endif                    
                    cl_int status = _ocl_kernel.setArg(arg_index, sizeof(T), &value);
                    HETCOMPUTE_UNUSED(status);
                    HETCOMPUTE_DLOG("cl::Kernel::setArg(%zu, %zu, %p)->%d", arg_index, sizeof(T), &value, status);
#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                }
                catch (cl::Error& err)
                {
                    HETCOMPUTE_FATAL("cl::Kernel::setArg(%zu, %zu, %p)->%s", arg_index, sizeof(T), &value, get_cl_error_string(err.err()));
                }
#else
                if (status != CL_SUCCESS)
                {
                    HETCOMPUTE_FATAL("cl::Kernel::setArg(%zu, %zu, %p)->%s", arg_index, sizeof(T), &value, get_cl_error_string(status));
                }
#endif
            }

            // handle local_alloc.
            inline void set_arg_local_alloc(size_t arg_index, size_t num_bytes_to_local_alloc)
            {
#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                try
                {
#endif
                    // set kernel arg
                    cl_int status = _ocl_kernel.setArg(arg_index, num_bytes_to_local_alloc, nullptr);
                    HETCOMPUTE_UNUSED(status);
                    HETCOMPUTE_DLOG("cl::Kernel::setArg(%zu, %zu bytes)->%d", arg_index, num_bytes_to_local_alloc, status);
#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                }
                catch (cl::Error& err)
                {
                    HETCOMPUTE_FATAL("cl::Kernel::setArg(%zu, %zu bytes)->%s",
                                   arg_index,
                                   num_bytes_to_local_alloc,
                                   get_cl_error_string(err.err()));
                }
#else
                if (status != CL_SUCCESS)
                {
                    HETCOMPUTE_FATAL("cl::Kernel::setArg(%zu, %zu bytes)->%s",
                                   arg_index,
                                   num_bytes_to_local_alloc,
                                   get_cl_error_string(status));
                }
#endif
            }


            // Each element from a tuple is extracted and an index is
            // incremented, set_args(with empty body) is only enabled
            // when index is equal to length of Kargs, this terminates
            // the recursion.
            template <size_t index = 0, typename... Kargs>
            typename std::enable_if<(index == sizeof...(Kargs)), void>::type set_args(const std::tuple<Kargs...>&)
            {
            }

            template <size_t index = 0, typename... Kargs>
            typename std::enable_if<(index < sizeof...(Kargs)), void>::type set_args(const std::tuple<Kargs...>& t)
            {
                set_arg(index, std::get<index>(t));
                set_args<index + 1, Kargs...>(t);
            }

        };  // class clkernel
    };  // namespace internal
};  // namespace hetcompute

#endif // HETCOMPUTE_HAVE_OPENCL
