#pragma once

#ifdef HETCOMPUTE_HAVE_GPU

#include <hetcompute/internal/legacy/attrobjs.hh>
#include <hetcompute/internal/legacy/types.hh>

#include <hetcompute/internal/device/clkernel.hh>
#include <hetcompute/internal/device/glkernel.hh>

namespace hetcompute
{
    namespace internal
    {
        // Release functor for gpukernel objects.
        template <typename GPUKernelType>
        struct gpukernel_deleter
        {
            static void release(GPUKernelType* k)
            {
#ifdef HETCOMPUTE_HAVE_OPENCL
                delete k->get_cl_kernel();
#endif // HETCOMPUTE_HAVE_OPENCL
#ifdef HETCOMPUTE_HAVE_GLES
                delete k->get_gl_kernel();
#endif // HETCOMPUTE_HAVE_GLES
                delete k;
            }
        };

        namespace legacy
        {
            // hetcompute::create_kernel returns a shared_ptr to this class.
            template <typename... Params>
            class gpukernel : public hetcompute::internal::ref_counted_object<gpukernel<Params...>,
                                                                              hetcompute::internal::hetcomputeptrs::default_logger,
                                                                              hetcompute::internal::gpukernel_deleter<gpukernel<Params...>>>,
                              public internal::legacy::gpu_kernel_base
            {
            private:
#ifdef HETCOMPUTE_HAVE_OPENCL
                hetcompute::internal::clkernel* _cl_kernel;
#endif // HETCOMPUTE_HAVE_OPENCL

#ifdef HETCOMPUTE_HAVE_GLES
                hetcompute::internal::glkernel* _gl_kernel;
#endif // HETCOMPUTE_HAVE_GLES

                HETCOMPUTE_DELETE_METHOD(gpukernel(gpukernel const&));
                HETCOMPUTE_DELETE_METHOD(gpukernel(gpukernel&&));
                HETCOMPUTE_DELETE_METHOD(gpukernel& operator=(gpukernel const&));
                HETCOMPUTE_DELETE_METHOD(gpukernel& operator=(gpukernel&&));

            public:
                typedef std::tuple<Params...> parameters;

#ifdef HETCOMPUTE_HAVE_OPENCL
                // constructor for CL Kernel
                gpukernel(const std::string& kernel_str, const std::string& kernel_name, const std::string& build_options)
                    : _cl_kernel(nullptr)
#ifdef HETCOMPUTE_HAVE_GLES
                      ,
                      _gl_kernel(nullptr)
#endif // HETCOMPUTE_HAVE_GLES
                {
                    // get devices.
                    HETCOMPUTE_INTERNAL_ASSERT(!hetcompute::internal::s_dev_ptrs->empty(), "No GPU devices found on the platform");
                    _cl_kernel =
                        new hetcompute::internal::clkernel((*hetcompute::internal::s_dev_ptrs)[0], kernel_str, kernel_name, build_options);
                    HETCOMPUTE_DLOG("kernel_name: %s, %p, %s", kernel_name.c_str(), _cl_kernel, build_options.c_str());
                }

                // constructor for CL Kernel binary
                gpukernel(void const* kernel_bin, size_t kernel_len, const std::string& kernel_name, const std::string& build_options)
                    : _cl_kernel(nullptr)
#ifdef HETCOMPUTE_HAVE_GLES
                      ,
                      _gl_kernel(nullptr)
#endif // HETCOMPUTE_HAVE_GLES
                {
                    // get devices.
                    HETCOMPUTE_INTERNAL_ASSERT(!hetcompute::internal::s_dev_ptrs->empty(), "No GPU devices found on the platform");
                    _cl_kernel = new hetcompute::internal::clkernel((*hetcompute::internal::s_dev_ptrs)[0],
                                                                    kernel_bin,
                                                                    kernel_len,
                                                                    kernel_name,
                                                                    build_options);
                    HETCOMPUTE_DLOG("kernel_name: %s, %p, %s", kernel_name.c_str(), _cl_kernel, build_options.c_str());
                }

#ifdef HETCOMPUTE_HAVE_GLES
                bool is_cl() const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_cl_kernel == nullptr || _gl_kernel == nullptr, "Kernel must be only one of CL or GLES");
                    return _cl_kernel != nullptr;
                }
#else  // HETCOMPUTE_HAVE_GLES
                bool is_cl() const { return true; }
#endif // HETCOMPUTE_HAVE_GLES

                hetcompute::internal::clkernel* get_cl_kernel() { return _cl_kernel; }
#endif // HETCOMPUTE_HAVE_OPENCL

#ifdef HETCOMPUTE_HAVE_GLES
                hetcompute::internal::glkernel* get_gl_kernel() { return _gl_kernel; }

#ifdef HETCOMPUTE_HAVE_OPENCL
                bool is_gl() const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_cl_kernel == nullptr || _gl_kernel == nullptr, "Kernel must be only one of CL or GLES");
                    return _gl_kernel != nullptr;
                }
#else  // HETCOMPUTE_HAVE_OPENCL
                bool is_gl() const { return true; }
#endif // HETCOMPUTE_HAVE_OPENCL

                // constructor for GL Kernel
                explicit gpukernel(const std::string& kernel_str)
                    :
#ifdef HETCOMPUTE_HAVE_OPENCL
                      _cl_kernel(nullptr),
#endif // HETCOMPUTE_HAVE_OPENCL
                      _gl_kernel(nullptr)
                {
                    _gl_kernel = new hetcompute::internal::glkernel(kernel_str);
                }

#endif // HETCOMPUTE_HAVE_GLES

#ifdef HETCOMPUTE_HAVE_OPENCL
                std::pair<void const*, size_t> get_cl_kernel_binary() const
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_cl_kernel != nullptr, "null cl kernel");
                    return _cl_kernel->get_cl_kernel_binary();
                }
#endif // HETCOMPUTE_HAVE_OPENCL

                inline std::mutex& access_dispatch_mutex()
                {
                    HETCOMPUTE_INTERNAL_ASSERT(false
#ifdef HETCOMPUTE_HAVE_OPENCL
                                                   ||
                                                   is_cl()
#endif // HETCOMPUTE_HAVE_OPENCL
#ifdef HETCOMPUTE_HAVE_GLES
                                                   ||
                                                   is_gl()
#endif // HETCOMPUTE_HAVE_GLES
                                                   ,
                                               "Invalid GPU kernel type: must be cl or gl");

#ifdef HETCOMPUTE_HAVE_OPENCL
#ifdef HETCOMPUTE_HAVE_GLES
                    if (is_cl())
#endif // HETCOMPUTE_HAVE_GLES
                        return get_cl_kernel()->access_dispatch_mutex();
#endif // HETCOMPUTE_HAVE_OPENCL

#ifdef HETCOMPUTE_HAVE_GLES
                    return get_gl_kernel()->access_dispatch_mutex();
#endif // HETCOMPUTE_HAVE_GLES
                }
            };

#ifdef HETCOMPUTE_HAVE_OPENCL
            // Creates a Kernel object with CL kernel as backend.
            template <typename... Params>
            kernel_ptr<Params...>
            create_kernel(const std::string& kernel_str, const std::string& kernel_name, const std::string build_options = std::string(""))
            {
                auto k_ptr = new gpukernel<Params...>(kernel_str, kernel_name, build_options);
                HETCOMPUTE_DLOG("Creating hetcompute::gpukernel: %s", kernel_name.c_str());
                return kernel_ptr<Params...>(k_ptr, kernel_ptr<Params...>::ref_policy::do_initial_ref);
            }

            // Creates a Kernel object with CL kernel as backend with an option to retrieve optimal_local_size of the kernel.
            template <typename... Params>
            kernel_ptr<Params...> create_kernel(const std::string& kernel_str,
                                                const std::string& kernel_name,
                                                size_t&            opt_local_size,
                                                const std::string  build_options = std::string(""))
            {
                auto k_ptr     = new gpukernel<Params...>(kernel_str, kernel_name, build_options);
                opt_local_size = k_ptr->get_cl_kernel()->get_optimal_local_size();
                HETCOMPUTE_DLOG("Creating hetcompute::gpukernel: %s", kernel_name.c_str());
                return kernel_ptr<Params...>(k_ptr, kernel_ptr<Params...>::ref_policy::do_initial_ref);
            }

            // Creates a Kernel object with CL kernel binary as backend.
            template <typename... Params>
            kernel_ptr<Params...> create_kernel(void const*        kernel_bin,
                                                size_t             kernel_len,
                                                const std::string& kernel_name,
                                                const std::string  build_options = std::string(""))
            {
                auto k_ptr = new gpukernel<Params...>(kernel_bin, kernel_len, kernel_name, build_options);
                HETCOMPUTE_DLOG("Creating hetcompute::gpukernel: %s", kernel_name.c_str());
                return kernel_ptr<Params...>(k_ptr, kernel_ptr<Params...>::ref_policy::do_initial_ref);
            }

            // Creates a Kernel object with CL kernel binary as backend with an option to retrieve optimal_local_size of the kernel.
            template <typename... Params>
            kernel_ptr<Params...> create_kernel(void const*        kernel_bin,
                                                size_t             kernel_len,
                                                const std::string& kernel_name,
                                                size_t&            opt_local_size,
                                                const std::string  build_options = std::string(""))
            {
                auto k_ptr     = new gpukernel<Params...>(kernel_bin, kernel_len, kernel_name, build_options);
                opt_local_size = k_ptr->get_cl_kernel()->get_optimal_local_size();
                HETCOMPUTE_DLOG("Creating hetcompute::gpukernel: %s", kernel_name.c_str());
                return kernel_ptr<Params...>(k_ptr, kernel_ptr<Params...>::ref_policy::do_initial_ref);
            }
#endif // HETCOMPUTE_HAVE_OPENCL

#ifdef HETCOMPUTE_HAVE_GLES
            // Creates a Kernel object with GL kernel as backend.
            template <typename... Params>
            kernel_ptr<Params...> create_kernel(const std::string& gl_kernel_str)
            {
                auto k_ptr = new gpukernel<Params...>(gl_kernel_str);
                HETCOMPUTE_DLOG("Creating hetcompute::glkernel");
                return kernel_ptr<Params...>(k_ptr, kernel_ptr<Params...>::ref_policy::do_initial_ref);
            }
#endif     // HETCOMPUTE_HAVE_GLES
        }; // namespace hetcompute::internal::legacy

    }; // namespace hetcompute::internal

}; // namespace hetcompute

#endif // HETCOMPUTE_HAVE_GPU
