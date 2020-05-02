/** @file gpukernel.hh */
#pragma once

#include <hetcompute/internal/task/gpukernel.hh>
#include <hetcompute/kernel.hh>

namespace hetcompute
{
    /** @addtogroup kernelclass_doc
        @{ */

    /**
     * @brief Used as a template parameter to hetcompute::gpu_kernel
     *        to indicate a locally allocated parameter.
     *
     * Used as a template parameter to hetcompute::gpu_kernel to indicate a
     * locally allocated parameter. Corresponds to a __local parameter of
     * an OpenCL kernel.  During task creation or launch, the corresponding
     * argument takes the size of the local allocation in number of elements
     * of type T.
     *
     * @sa hetcompute::gpu_kernel
     *
     * Example:
     *
     * @code
     * const char* kernel_string = "
     * __kernel void k(__global int *a,
     *                 __global int *b,
     *                 __local  int *c)
     * {
     *     ...
     * }";
     *
     * hetcompute::gpu_kernel<hetcompute::buffer_ptr<int>,
     *                      hetcompute::buffer_ptr<int>,
     *                      hetcompute::local<int>> gk(kernel_string, "k");
     *
     * // pass __local size in number of elements (not number of bytes as for OpenCL)
     * int number_of_ints = number_of_bytes / sizeof(int);
     * auto t = hetcompute::create_task(gk, r, buf_a, buf_b, number_of_ints);
     *
     * @endcode
     */
    template<typename T>
    class local;

    /** @} */ /* end_addtogroup kernelclass_doc */

    namespace beta
    {
        /** @addtogroup kernelclass_doc
            @{ */

        /**
         * @brief Type used for declaring the constant <tt>hetcompute::cl</tt>.
         *
         * @sa hetcompute::gpu_kernel
         */
        class cl_t
        {
        };

        /**
         * @brief Type used for declaring the constant <tt>hetcompute::gl</tt>.
         *
         * @sa hetcompute::gpu_kernel
         */
        class gl_t
        {
        };

        /**
         * @brief Used to explicitly indicate creation of an OpenCL kernel.
         *
         * @sa hetcompute::gpu_kernel
         */
        cl_t const cl{};

        /**
         * @brief Used to explicitly indicate creation of an OpenGL ES compute kernel.
         *
         * @sa hetcompute::gpu_kernel
         */
        gl_t const gl{};

        /** @} */ /* end_addtogroup kernelclass_doc */

    };  // namespace beta

    /** @addtogroup kernelclass_doc
        @{ */

#ifdef HETCOMPUTE_HAVE_GPU
    /**
     *  @brief A wrapper around OpenCL C kernels and OpenGL ES compute shaders
     *  for GPU compute.
     *
     *  A wrapper around OpenCL C kernels and OpenGL ES compute shaders
     *  for GPU compute.
     *  The template signature corresponds to the GPU kernel parameter list.
     *
     *  @sa hetcompute::create_gpu_kernel for creating a <tt>gpu_kernel</tt>.
     */
    template <typename... Args>
    class gpu_kernel : private internal::gpu_kernel_implementation<Args...>
    {
        using parent = internal::gpu_kernel_implementation<Args...>;

        enum
        {
            num_kernel_args = sizeof...(Args)
        };

        enum kernel_type
        {
            CL,
            GL
        };

    public:
#ifdef HETCOMPUTE_HAVE_OPENCL
        /**
         *  @brief Constructor, implicit for OpenCL kernel.
         *
         *  Constructor, implicit for OpenCL kernel.
         *
         *  @param cl_kernel_str    The OpenCL C kernel code as a string.
         *  @param cl_kernel_name   The name of the kernel function to be called.
         *  @param cl_build_options Build options to pass to OpenCL (optional).
         */
        gpu_kernel(std::string const& cl_kernel_str, std::string const& cl_kernel_name, std::string const& cl_build_options = "")
            : parent(cl_kernel_str, cl_kernel_name, cl_build_options), _kernel_type(CL)
        {
        }

        /**
         *  @brief Constructor, explicit for OpenCL kernel.
         *
         *  Constructor, explicit for OpenCL kernel.
         *
         *  @arg                    Pass <tt>hetcompute::beta::cl</tt> to explicitly select
         *                          this OpenCL kernel constructor.
         *  @param cl_kernel_str    The OpenCL C kernel code as a string.
         *  @param cl_kernel_name   The name of the kernel function to be called.
         *  @param cl_build_options Build options to pass to OpenCL (optional).
         */
        gpu_kernel(beta::cl_t const&,
                   std::string const& cl_kernel_str,
                   std::string const& cl_kernel_name,
                   std::string const& cl_build_options = "")
            : parent(cl_kernel_str, cl_kernel_name, cl_build_options), _kernel_type(CL)
        {
        }
#endif // HETCOMPUTE_HAVE_OPENCL

#ifdef HETCOMPUTE_HAVE_GLES
        /**
         *  @brief Constructor, explicit for OpenGL ES compute kernel.
         *
         *  Constructor, explicit for OpenGL ES compute kernel.
         *
         *  @arg                    Pass <tt>hetcompute::beta::gl</tt> to explicitly select
         *                          this OpenGL ES kernel constructor.
         *  @param gl_kernel_str    The OpenGL ES shader code as a string.
         */
        gpu_kernel(beta::gl_t const&, std::string const& gl_kernel_str) : parent(gl_kernel_str), _kernel_type(GL) {}
#endif // HETCOMPUTE_HAVE_GLES

#ifdef HETCOMPUTE_HAVE_OPENCL
        /**
         *  @brief Constructor, implicit for precompiled OpenCL kernel.
         *
         *  Constructor, implicit for precompiled OpenCL kernel.
         *
         *  @param cl_kernel_bin    The pointer to a precompiled OpenCL C kernel binary.
         *  @param cl_kernel_len    The length of the precompiled kernel in bytes.
         *  @param cl_kernel_name   The name of the kernel function to be called.
         *  @param cl_build_options Build options to pass to OpenCL (optional).
         */
        gpu_kernel(void const* cl_kernel_bin, size_t cl_kernel_len, std::string const& cl_kernel_name, std::string const& cl_build_options = "")
            : parent(cl_kernel_bin, cl_kernel_len, cl_kernel_name, cl_build_options), _kernel_type(CL)
        {
        }

        /**
         *  @brief Constructor, explicit for precompiled OpenCL kernel.
         *
         *  Constructor, explicit for precompiled OpenCL kernel.
         *
         *  @arg                    Pass <tt>hetcompute::beta::cl</tt> to explicitly select
         *                          this OpenCL kernel constructor.
         *  @param cl_kernel_bin    The pointer to a precompiled OpenCL C kernel binary.
         *  @param cl_kernel_len    The length of the precompiled kernel in bytes.
         *  @param cl_kernel_name   The name of the kernel function to be called.
         *  @param cl_build_options Build options to pass to OpenCL (optional).
         */
        gpu_kernel(beta::cl_t const&,
                   void const*        cl_kernel_bin,
                   size_t             cl_kernel_len,
                   std::string const& cl_kernel_name,
                   std::string const& cl_build_options = "")
            : parent(cl_kernel_bin, cl_kernel_len, cl_kernel_name, cl_build_options), _kernel_type(CL)
        {
        }

        /**
         *  @brief Extracts the CL binary. Error if invoked on a non-OpenCL gpukernel.
         *
         *  Extracts the CL binary. Error if invoked on a non-OpenCL gpukernel.
         *
         *  @return std::pair consisting of a pointer to an allocated buffer holding
         *          the CL binary and the size of the allocated buffer (sized to hold
         *          the binary) in bytes.
         *
         *  @note   Each invocation of this function internally allocates a new buffer
         *          of an appropriate size using <code>new[]</code>. The user code is
         *          responsible for deleting the buffer after use by calling
         *          <code>delete[]</code>.
         */
        std::pair<void const*, size_t> get_cl_kernel_binary() const
        {
            HETCOMPUTE_API_ASSERT(is_cl(), "CL kernel binary info requested for a non-OpenCL kernel");
            return parent::get_cl_kernel_binary();
        }
#endif // HETCOMPUTE_HAVE_OPENCL

        HETCOMPUTE_DEFAULT_METHOD(gpu_kernel(gpu_kernel const&));
        HETCOMPUTE_DEFAULT_METHOD(gpu_kernel& operator=(gpu_kernel const&));
        HETCOMPUTE_DEFAULT_METHOD(gpu_kernel(gpu_kernel&&));
        HETCOMPUTE_DEFAULT_METHOD(gpu_kernel& operator=(gpu_kernel&&));

        /*
         *  Destructor. gpu_kernel is not ref-counted.
         */
        ~gpu_kernel() {}

        /**
         *  @brief Identifies if kernel type is OpenCL.
         *
         *  Identifies if kernel type is OpenCL.
         *
         *  @return true if this is an OpenCL kernel, false otherwise.
         */
        bool is_cl() const { return _kernel_type == CL; }

        /**
         *  @brief Identifies if kernel type is OpenGL ES.
         *
         *  Identifies if kernel type is OpenGL ES.
         *
         *  @return true if this is an OpenGL ES kernel, false otherwise.
         */
        bool is_gl() const { return _kernel_type == GL; }

    private:
        kernel_type const _kernel_type;

        template <typename Code, class Enable>
        friend struct hetcompute::internal::task_factory_dispatch;

        template <typename GPUKernel, typename... CallArgs>
        friend void hetcompute::internal::execute_gpu(GPUKernel const& gk, CallArgs&&... args);

        template <typename GPUKernel, size_t Dims, typename... CallArgs>
        friend struct hetcompute::internal::executor;
    };

#endif // HETCOMPUTE_HAVE_GPU

#ifdef HETCOMPUTE_HAVE_OPENCL
    /**
     *  Creates a GPU kernel executable by HETCOMPUTE, implicitly for OpenCL.
     *  The template signature corresponds to the GPU kernel parameter list.
     *
     *  The kernel code is specified as a string of OpenCL C code.
     *
     *  Equivalent to calling the <tt>hetcompute::gpu_kernel</tt> constructor
     *  directly.
     *
     *  @param cl_kernel_str    The OpenCL C kernel code as a string.
     *  @param cl_kernel_name   The name of the kernel function to be called.
     *  @param cl_build_options The build options to pass to OpenCL (optional).
     *
     *  @return A <tt>gpu_kernel</tt> object.
     */
    template <typename... Args>
    gpu_kernel<Args...>
    create_gpu_kernel(std::string const& cl_kernel_str, std::string const& cl_kernel_name, std::string const& cl_build_options = "")
    {
        return gpu_kernel<Args...>(cl_kernel_str, cl_kernel_name, cl_build_options);
    }
#endif // HETCOMPUTE_HAVE_OPENCL

    /** @} */ /* end_addtogroup kernelclass_doc */

    namespace beta
    {
        /** @addtogroup kernelclass_doc
            @{ */

#ifdef HETCOMPUTE_HAVE_OPENCL
        /**
         *  Creates a GPU kernel executable by HETCOMPUTE, explictly for OpenCL.
         *  The template signature corresponds to the GPU kernel parameter list.
         *
         *  The kernel code is specified as a string of OpenCL C code.
         *
         *  Equivalent to calling the <tt>hetcompute::gpu_kernel</tt> constructor
         *  directly.
         *
         *  @arg         Pass <tt>hetcompute::beta::cl</tt> to explicitly select OpenCL.
         *  @param cl_kernel_str    The OpenCL C kernel code as a string.
         *  @param cl_kernel_name   The name of the kernel function to be called.
         *  @param cl_build_options The build options to pass to OpenCL (optional).
         *
         *  @return A <tt>gpu_kernel</tt> object.
         */
        template <typename... Args>
        gpu_kernel<Args...> create_gpu_kernel(beta::cl_t const&,
                                              std::string const& cl_kernel_str,
                                              std::string const& cl_kernel_name,
                                              std::string const& cl_build_options = "")
        {
            return gpu_kernel<Args...>(beta::cl, cl_kernel_str, cl_kernel_name, cl_build_options);
        }
#endif  // HETCOMPUTE_HAVE_OPENCL

#ifdef HETCOMPUTE_HAVE_GLES
        /**
         *  Creates a GPU kernel executable by HETCOMPUTE, explictly for OpenGL ES.
         *  The template signature corresponds to the GPU kernel parameter list.
         *
         *  The OpenGL ES shader code is specified as a string.
         *
         *  Equivalent to calling the <tt>hetcompute::gpu_kernel</tt> constructor
         *  directly.
         *
         *  @arg         Pass <tt>hetcompute::beta::gl</tt> to explicitly select OpenGL.
         *  @param gl_kernel_str    The OpenGL ES shader code as a string.
         *
         *  @return A <tt>gpu_kernel</tt> object.
         */
        template <typename... Args>
        gpu_kernel<Args...> create_gpu_kernel(beta::gl_t const&, std::string const& gl_kernel_str)
        {
            return gpu_kernel<Args...>(beta::gl, gl_kernel_str);
        }
#endif // HETCOMPUTE_HAVE_GLES

        /** @} */ /* end_addtogroup kernelclass_doc */

    };  // namespace beta

    /** @addtogroup kernelclass_doc
    @{ */

#ifdef HETCOMPUTE_HAVE_OPENCL
    /**
     *  Creates a GPU kernel executable by HETCOMPUTE, implicitly for OpenCL,
     *  using a precompiled OpenCL kernel.
     *  The template signature corresponds to the GPU kernel parameter list.
     *
     *  The kernel code is specified as a prebuilt OpenCL kernel binary.
     *
     *  Equivalent to calling the <tt>hetcompute::gpu_kernel</tt> constructor
     *  directly.
     *
     *  @param cl_kernel_bin    The pointer to a precompiled OpenCL C kernel binary.
     *  @param cl_kernel_len    The length of the precompiled kernel in bytes.
     *  @param cl_kernel_name   The name of the kernel function to be called.
     *  @param cl_build_options The build options to pass to OpenCL (optional).
     *
     *  @return A <tt>gpu_kernel</tt> object.
     */
    template <typename... Args>
    gpu_kernel<Args...> create_gpu_kernel(void const*        cl_kernel_bin,
                                          size_t             cl_kernel_len,
                                          std::string const& cl_kernel_name,
                                          std::string const& cl_build_options = "")
    {
        return gpu_kernel<Args...>(cl_kernel_bin, cl_kernel_len, cl_kernel_name, cl_build_options);
    }
#endif // HETCOMPUTE_HAVE_OPENCL

    /** @} */ /* end_addtogroup kernelclass_doc */

    namespace beta {

        /** @addtogroup kernelclass_doc
        @{ */

#ifdef HETCOMPUTE_HAVE_OPENCL
        /**
         *  Creates a GPU kernel executable by HETCOMPUTE, explicitly for OpenCL,
         *  using a precompiled OpenCL kernel.
         *  The template signature corresponds to the GPU kernel parameter list.
         *
         *  The kernel code is specified as a prebuilt OpenCL kernel binary.
         *
         *  Equivalent to calling the <tt>hetcompute::gpu_kernel</tt> constructor
         *  directly.
         *
         *  @arg         Pass <tt>hetcompute::beta::cl</tt> to explicitly select OpenCL.
         *  @param cl_kernel_bin    The pointer to a precompiled OpenCL C kernel binary.
         *  @param cl_kernel_len    The length of the precompiled kernel in bytes.
         *  @param cl_kernel_name   The name of the kernel function to be called.
         *  @param cl_build_options The build options to pass to OpenCL (optional).
         *
         *  @return A <tt>gpu_kernel</tt> object.
         */
        template <typename... Args>
        gpu_kernel<Args...> create_gpu_kernel(beta::cl_t const&,
                                              void const*        cl_kernel_bin,
                                              size_t             cl_kernel_len,
                                              std::string const& cl_kernel_name,
                                              std::string const& cl_build_options = "")
        {
            return gpu_kernel<Args...>(beta::cl, cl_kernel_bin, cl_kernel_len, cl_kernel_name, cl_build_options);
        }
#endif // HETCOMPUTE_HAVE_OPENCL

#ifdef HETCOMPUTE_HAVE_GLES
        template <typename... Args>
        gpu_kernel<Args...> create_gpu_kernel(beta::gl_t const&, void const* gl_kernel_bin, size_t gl_kernel_len)
        {
            HETCOMPUTE_UNIMPLEMENTED("HetCompute does not support creation of precompiled OpenGL ES kernels.");
            return gpu_kernel<Args...>(beta::gl, gl_kernel_bin, gl_kernel_len);
        }
#endif // HETCOMPUTE_HAVE_GLES

#ifdef HETCOMPUTE_HAVE_GPU
        /**
         *  @brief Utility template to get the tuple type for GPU pipeline stages.
         *
         *  Utility template to get the tuple type of
         *  hetcompute::range, and GPU kernel argument types.
         *  Use case: get the return type of the before synchronization lambda for
         *  a gpu pipeline stage.
         *
         *  The wrapped type can be accessed trough call_tuple<...>::type.
         *
         *  @tparam Dim dimension of hetcompute::range.
         *  @tparam Args... GPU kernel argument type list.
         *  @sa template<typename... Args> void add_gpu_stage(Args&&... args)
         */
        template <size_t Dim, typename... Args>
        struct call_tuple<Dim, gpu_kernel<Args...>>
        {
            using type = std::tuple<hetcompute::range<Dim>, typename internal::strip_buffer_dir<Args>::type...>;
        };
#endif // HETCOMPUTE_HAVE_GPU

        /** @} */ /* end_addtogroup kernelclass_doc */

    }; // namespace beta
};  // namespace hetcompute
