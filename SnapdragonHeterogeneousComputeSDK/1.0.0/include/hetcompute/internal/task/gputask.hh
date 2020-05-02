/** @file gputask.hh */
#pragma once

#ifdef HETCOMPUTE_HAVE_GPU

#include <hetcompute/buffer.hh>
#include <hetcompute/internal/legacy/gpukernel.hh>
#include <hetcompute/internal/buffer/arenaaccess.hh>
#include <hetcompute/internal/buffer/bufferpolicy.hh>
#include <hetcompute/internal/device/cldevice.hh>
#include <hetcompute/internal/device/clevent.hh>
#include <hetcompute/internal/task/task.hh>
#include <hetcompute/internal/util/strprintf.hh>

namespace hetcompute
{
    template <typename T>
    struct buffer_attrs
    {
        typedef T type;
    };

    /// Used as a template parameter to hetcompute::gpu_kernel to indicate a locally allocated parameter.
    /// During task creation or launch, the corresponding argument takes the size of the local allocation
    /// in number of elements of type T.
    template <typename T>
    class local
    {
    };

    namespace internal
    {
        // forward decl
        class task_bundle_dispatch;

        /// gputask_base: base class of gputask
        /// 1. Allows processing of gputask arguments from within completion_callback:
        ///     + via on_finish() - release_arguments()
        /// 2. Provides functionality for the bundle-dispatch of multiple gputasks:
        ///     + bundle dispatch can pre-acquire buffers from each gputask.
        ///     + bundle dispatch can disable buffer acquires during gputask execution.
        class gputask_base : public task
        {
        protected:
            using dyn_sized_bas = buffer_acquire_set<0, false>;

            // Set to true (with task lock held) if task has finished do_execute(), else
            // false. Since task lock has to be held while setting _executed to true, it
            // need not be an atomic -- the true setting will be visible to any other
            // thread that invokes has_executed() with the task lock held.
            bool _executed;

            /// = false, default semantics, gputask execution acquires and releases the gputask's buffers.
            /// = true, GPU-bundle-dispatch will collectively acquire buffers for all the bundled tasks
            ///         prior to executing the bundled tasks, and will collectively release buffers afterwards.
            bool _does_bundle_dispatch;

            /// = true, default, gpu task is executing for the first time
            /// = false, gpu task launch has already been attempted at least once
            bool _first_execution;

            // gpu task execution time (if profile is set through the hetcompute tuner object).
            uint64_t _exec_time;

            virtual void release_arguments() = 0;

        public:
            explicit gputask_base(legacy::task_attrs a)
                : task(nullptr, a), _executed(false), _does_bundle_dispatch(false), _first_execution(true), _exec_time(0)
            {
            }

            /// Used to setup bundle dispatch
            void configure_for_bundle_dispatch() { _does_bundle_dispatch = true; }

            /// Used by bundle dispatch to collect all buffer arguments
            virtual void update_buffer_acquire_set(dyn_sized_bas& bas) = 0;

            /**
             * Used to check whether gpu task has been sent to the OpenCL runtime
             * @return true if task has executed, else false
             */
            bool has_executed() const { return _executed; }

            /**
             * Identify the type of kernel in the task by the executor device it will use.
             * @return The executor device used by the kernel inside this gputask.
             */
            virtual executor_device get_executor_device() const = 0;

            /**
             * Executes the gputask
             * @param tbd Execute task as part of bundle tbd; may be nullptr, and if so
             *            means task is not being dispatched in a bundle.
             * @returns true if task actually executed, false otherwise (may return false
             *          to indicate buffers were not successfully acquired)
             */
            virtual bool do_execute(task_bundle_dispatch* tbd = nullptr) = 0;

            /**
             * Query gpu task execution time (if profile is enabled through hetcompute tuner)
             * @returns gpu task execution time
             */
            uint64_t get_exec_time() const { return _exec_time; }

            /**
             * Set gpu task execution time (if profile is enabled through hetcompute tuner)
             * @param elapsed_time gpu task execution time.
             */
            void set_exec_time(uint64_t elapsed_time) { _exec_time = elapsed_time; }

            /// Called by completion callback
            void on_finish()
            {
                // Timing the task if profiling is enabled
                if (_exec_time != 0)
                {
                    uint64_t elapsed_time = hetcompute_get_time_now() - _exec_time;
                    set_exec_time(elapsed_time);
                }

                if (!_does_bundle_dispatch)
                {
                    release_arguments();
                }
            }
        }; // class gputask_base

        namespace gpu_kernel_dispatch
        {
            template <typename T>
            struct cl_arg_setter
            {
                template <typename Kernel>
                static void set(Kernel* kernel, size_t index, T& arg)
                {
                    HETCOMPUTE_DLOG("Setting kernel argument[%zu]: value type", index);
                    kernel->set_arg(index, arg);
                }
            }; // struct cl_arg_setter

            struct cl_arg_local_alloc_setter
            {
                template <typename Kernel>
                static void set(Kernel* kernel, size_t index, size_t num_bytes_to_local_alloc)
                {
                    HETCOMPUTE_DLOG("Setting kernel argument[%zu]: local_alloc %zu bytes", index, num_bytes_to_local_alloc);
                    kernel->set_arg_local_alloc(index, num_bytes_to_local_alloc);
                }
            }; // struct cl_arg_local_alloc_setter

            // The following types sole purpose is to make the template code
            // much more readable.
            struct input_output_param
            {
            };

            struct input_param
            {
            };

            struct output_param
            {
            };

            struct const_buffer
            {
            };

            struct non_const_buffer
            {
            };

            struct cl_arg_api_buffer_setter
            {
#ifdef HETCOMPUTE_HAVE_OPENCL
                template <typename BufferPtr, typename BufferAcquireSet>
                static void set(::hetcompute::internal::legacy::device_ptr const& device,
                                internal::clkernel*                               kernel,
                                size_t                                            i,
                                BufferPtr&                                        b,
                                input_param,
                                BufferAcquireSet& bas)
                {
                    HETCOMPUTE_DLOG("Setting kernel argument[%zu]: hetcompute::buffer_ptr in", i);

                    HETCOMPUTE_API_ASSERT(b != nullptr, "Need non-null buffer_ptr to execute with gpu kernel");

                    // TODO: When we have multiple gpu devices present we may also need to pass
                    // the device to request_acquire_action()
                    HETCOMPUTE_UNUSED(device);

                    auto acquired_arena = bas.find_acquired_arena(b);

#ifdef HETCOMPUTE_CL_TO_CL2
                    auto svm_ptr = arena_storage_accessor::access_cl2_arena_for_gputask(acquired_arena);
                    kernel->set_arg_svm(i, svm_ptr);
#else
                    auto ocl_buffer = arena_storage_accessor::access_cl_arena_for_gputask(acquired_arena);
                    kernel->set_arg(i, ocl_buffer);
#endif /// HETCOMPUTE_CL_TO_CL2
                }
#endif // HETCOMPUTE_HAVE_OPENCL

#ifdef HETCOMPUTE_HAVE_GLES
                template <typename BufferPtr, typename BufferAcquireSet>
                static void set(::hetcompute::internal::legacy::device_ptr const& device,
                                internal::glkernel*                               kernel,
                                size_t                                            i,
                                BufferPtr&                                        b,
                                input_param,
                                BufferAcquireSet& bas)
                {
                    HETCOMPUTE_DLOG("Setting kernel argument[%zu]: hetcompute::buffer_ptr in", i);

                    HETCOMPUTE_API_ASSERT(b != nullptr, "Need non-null buffer_ptr to execute with gpu kernel");

                    // TODO: When we have multiple gpu devices present we may also need to pass
                    // the device to request_acquire_action()
                    HETCOMPUTE_UNUSED(device);

                    auto acquired_arena = bas.find_acquired_arena(b);

                    auto id = arena_storage_accessor::access_gl_arena_for_gputask(acquired_arena);
                    kernel->set_arg(i, id);
                }
#endif // HETCOMPUTE_HAVE_GLES

#ifdef HETCOMPUTE_HAVE_OPENCL
                template <typename BufferPtr, typename BufferAcquireSet>
                static void set(::hetcompute::internal::legacy::device_ptr const& device,
                                internal::clkernel*                               kernel,
                                size_t                                            i,
                                BufferPtr&                                        b,
                                output_param,
                                BufferAcquireSet& bas)
                {
                    HETCOMPUTE_DLOG("Setting kernel argument[%zu]: hetcompute::buffer_ptr out", i);

                    // TODO: When we have multiple gpu devices present we may also need to pass
                    // the device to request_acquire_action()
                    HETCOMPUTE_UNUSED(device);

                    auto acquired_arena = bas.find_acquired_arena(b);

#ifdef HETCOMPUTE_CL_TO_CL2
                    auto svm_ptr = arena_storage_accessor::access_cl2_arena_for_gputask(acquired_arena);
                    kernel->set_arg_svm(i, svm_ptr);
#else
                    auto ocl_buffer = arena_storage_accessor::access_cl_arena_for_gputask(acquired_arena);
                    kernel->set_arg(i, ocl_buffer);
#endif // HETCOMPUTE_CL_TO_CL2
                }
#endif // HETCOMPUTE_HAVE_OPENCL

#ifdef HETCOMPUTE_HAVE_GLES
                template <typename BufferPtr, typename BufferAcquireSet>
                static void set(::hetcompute::internal::legacy::device_ptr const& device,
                                internal::glkernel*                               kernel,
                                size_t                                            i,
                                BufferPtr&                                        b,
                                output_param,
                                BufferAcquireSet& bas)
                {
                    HETCOMPUTE_DLOG("Setting kernel argument[%zu]: hetcompute::buffer_ptr out", i);

                    // TODO: When we have multiple gpu devices present we may also need to pass
                    // the device to request_acquire_action()
                    HETCOMPUTE_UNUSED(device);

                    auto acquired_arena = bas.find_acquired_arena(b);

                    auto id = arena_storage_accessor::access_gl_arena_for_gputask(acquired_arena);
                    kernel->set_arg(i, id);
                }
#endif // HETCOMPUTE_HAVE_GLES

#ifdef HETCOMPUTE_HAVE_OPENCL
                template <typename BufferPtr, typename BufferAcquireSet>
                static void set(::hetcompute::internal::legacy::device_ptr const& device,
                                internal::clkernel*                               kernel,
                                size_t                                            i,
                                BufferPtr&                                        b,
                                input_output_param,
                                BufferAcquireSet& bas)
                {
                    HETCOMPUTE_DLOG("Setting kernel argument[%zu]: hetcompute::buffer_ptr inout", i);

                    // TODO: When we have multiple gpu devices present we may also need to pass
                    // the device to request_acquire_action()
                    HETCOMPUTE_UNUSED(device);

                    auto acquired_arena = bas.find_acquired_arena(b);

#ifdef HETCOMPUTE_CL_TO_CL2
                    auto svm_ptr = arena_storage_accessor::access_cl2_arena_for_gputask(acquired_arena);
                    kernel->set_arg_svm(i, svm_ptr);
#else
                    auto ocl_buffer = arena_storage_accessor::access_cl_arena_for_gputask(acquired_arena);
                    kernel->set_arg(i, ocl_buffer);
#endif // HETCOMPUTE_CL_TO_CL2
                }
#endif // HETCOMPUTE_HAVE_OPENCL

#ifdef HETCOMPUTE_HAVE_GLES
                template <typename BufferPtr, typename BufferAcquireSet>
                static void set(::hetcompute::internal::legacy::device_ptr const& device,
                                internal::glkernel*                               kernel,
                                size_t                                            i,
                                BufferPtr&                                        b,
                                input_output_param,
                                BufferAcquireSet& bas)
                {
                    HETCOMPUTE_DLOG("Setting kernel argument[%zu]: hetcompute::buffer_ptr inout", i);

                    // TODO: When we have multiple gpu devices present we may also need to pass
                    // the device to request_acquire_action()
                    HETCOMPUTE_UNUSED(device);

                    auto acquired_arena = bas.find_acquired_arena(b);

                    auto id = arena_storage_accessor::access_gl_arena_for_gputask(acquired_arena);
                    kernel->set_arg(i, id);
                }
#endif         // HETCOMPUTE_HAVE_GLES
            }; // struct cl_arg_api_buffer_setter

            struct cl_arg_texture_setter
            {
#ifdef HETCOMPUTE_HAVE_OPENCL
                static void set(::hetcompute::internal::legacy::device_ptr const&,
                                internal::clkernel*                              kernel,
                                size_t                                           i,
                                hetcompute::graphics::internal::base_texture_cl* tex_cl)
                {
                    HETCOMPUTE_INTERNAL_ASSERT(tex_cl != nullptr, "Null texture_cl");
                    HETCOMPUTE_DLOG("Setting kernel argument[%zu]: texture type. Parameter: in_out", i);
                    kernel->set_arg(i, tex_cl);
                }
#endif // HETCOMPUTE_HAVE_OPENCL

#if defined(HETCOMPUTE_HAVE_OPENCL) && defined(HETCOMPUTE_HAVE_GLES)
                static void set(::hetcompute::internal::legacy::device_ptr const&,
                                internal::glkernel*,
                                size_t,
                                hetcompute::graphics::internal::base_texture_cl*)
                {
                    HETCOMPUTE_UNIMPLEMENTED("Textures not implemented in GL compute path");
                }
#endif // defined(HETCOMPUTE_HAVE_OPENCL) && defined(HETCOMPUTE_HAVE_GLES)

            }; // struct cl_arg_texture_setter

            struct cl_arg_sampler_setter
            {
#ifdef HETCOMPUTE_HAVE_OPENCL
                template <typename SamplerPtr>
                static void
                set(::hetcompute::internal::legacy::device_ptr const&, internal::clkernel* kernel, size_t i, SamplerPtr& sampler_ptr)
                {
                    HETCOMPUTE_DLOG("Setting kernel argument[%zu]: sampler type. Parameter: in_out", i);
                    kernel->set_arg(i, sampler_ptr);
                }
#endif // HETCOMPUTE_HAVE_OPENCL

#ifdef HETCOMPUTE_HAVE_GLES
                template <typename SamplerPtr>
                static void set(::hetcompute::internal::legacy::device_ptr const&, internal::glkernel*, size_t, SamplerPtr&)
                {
                    HETCOMPUTE_UNIMPLEMENTED("Samplers not implemented in GL compute path");
                }
#endif         // HETCOMPUTE_HAVE_GLES
            }; // struct cl_arg_sampler_setter

            HETCOMPUTE_GCC_IGNORE_BEGIN("-Weffc++");

            // The following templates check whether T is a GPU local allocation parameter.
            template <typename T>
            struct is_local_alloc : public std::false_type
            {
                typedef void data_type;
            };

            template <typename U>
            struct is_local_alloc<hetcompute::local<U>> : public std::true_type
            {
                typedef U data_type;
            };

            // The following templates check whether T is a texture_ptr.
            template <typename T>
            struct is_texture_ptr : public std::false_type
            {
            };

#ifdef HETCOMPUTE_HAVE_OPENCL
            template <hetcompute::graphics::image_format ImageFormat, int Dims>
            struct is_texture_ptr<hetcompute::graphics::texture_ptr<ImageFormat, Dims>> : public std::true_type
            {
            };
#endif // HETCOMPUTE_HAVE_OPENCL

            // The following templates check whether T is a sampler_ptr.
            template <typename T>
            struct is_sampler_ptr : public std::false_type
            {
            };

#ifdef HETCOMPUTE_HAVE_OPENCL
            template <hetcompute::graphics::addressing_mode addrMode, hetcompute::graphics::filter_mode filterMode>
            struct is_sampler_ptr<hetcompute::graphics::sampler_ptr<addrMode, filterMode>> : public std::true_type
            {
            };
#endif // HETCOMPUTE_HAVE_OPENCL

            template <typename T>
            struct is_normal_arg : public std::conditional<!is_api20_buffer_ptr<T>::value && !is_local_alloc<T>::value &&
                                                               !is_texture_ptr<T>::value && !is_sampler_ptr<T>::value,
                                                           std::true_type,
                                                           std::false_type>::type
            {
            };

            HETCOMPUTE_GCC_IGNORE_END("-Weffc++");

            // The following templates define the kernel argument traits.
            template <typename T>
            struct kernel_arg_ptr_traits;

            template <typename T>
            struct kernel_arg_ptr_traits<::hetcompute::buffer_ptr<T>>
            {
                using naked_type = typename std::remove_cv<T>::type;
                using is_const   = typename std::is_const<T>;
            };

            // The following templates define the kernel parameter traits
            template <typename T>
            struct kernel_param_ptr_traits;

            template <typename T>
            struct kernel_param_ptr_traits<hetcompute::buffer_ptr<T>>
            {
                using naked_type = typename std::remove_cv<T>::type;
                using is_const   = std::is_const<T>;
                using direction  = typename std::conditional<is_const::value, input_param, input_output_param>::type;
            };

            template <typename T>
            struct kernel_param_ptr_traits<hetcompute::in<hetcompute::buffer_ptr<T>>>
            {
                using naked_type = typename std::remove_cv<T>::type;
                using is_const   = std::is_const<T>;
                using direction  = input_param;
            };

            template <typename T>
            struct kernel_param_ptr_traits<hetcompute::out<hetcompute::buffer_ptr<T>>>
            {
                using naked_type = typename std::remove_cv<T>::type;
                using is_const   = std::is_const<T>;
                using direction  = output_param;
            };

            template <typename T>
            struct kernel_param_ptr_traits<hetcompute::inout<hetcompute::buffer_ptr<T>>>
            {
                using naked_type = typename std::remove_cv<T>::type;
                using is_const   = std::is_const<T>;
                using direction  = input_output_param;
            };

#ifdef HETCOMPUTE_HAVE_OPENCL
            /// Parses argument if it is not of buffer_ptr type.
            /// @param index Argument index
            /// @param arg Reference to argument
            template <typename Param, typename Arg, typename Kernel>
            void
            parse_normal_arg(::hetcompute::internal::legacy::device_ptr const&, Kernel* kernel, size_t index, Arg& arg, bool do_not_dispatch)
            {
                // Make sure that we weren't expecting a buffer_ptr
                static_assert(!is_api20_buffer_ptr<Param>::value, "Expected buffer_ptr argument");
                static_assert(!is_texture_ptr<Param>::value, "Expected texture_ptr or buffer_ptr argument");
                static_assert(std::is_same<Param, Arg>::value, "Mismatch in type of normal argument");
                if (do_not_dispatch == false)
                {
                    cl_arg_setter<Arg>::set(kernel, index, arg);
                }
            }
#endif // HETCOMPUTE_HAVE_OPENCL

            /// Parses argument when Param = hetcompute::local<T>.
            /// @param index Argument index
            /// @param arg Reference to argument
            template <typename Param, typename Arg, typename Kernel>
            void
            parse_local_alloc_arg(::hetcompute::internal::legacy::device_ptr const&, Kernel* kernel, size_t index, Arg& arg, bool do_not_dispatch)
            {
                static_assert(is_local_alloc<Param>::value, "Expected a hetcompute::local parameter");
                size_t sizeof_T = sizeof(typename is_local_alloc<Param>::data_type);

                size_t num_bytes_to_local_alloc = sizeof_T * arg;
                if (do_not_dispatch == false)
                {
                    cl_arg_local_alloc_setter::set(kernel, index, num_bytes_to_local_alloc);
                }
            }

            /// Param is an api_buffer_ptr.
            /// arg is a hetcompute::buffer_ptr.
            template <typename Param, typename Arg, typename BufferAcquireSet, typename Kernel>
            void parse_api_buffer_ptr_arg_dispatch(std::true_type,
                                                   ::hetcompute::internal::legacy::device_ptr const& device,
                                                   Kernel*                                         kernel,
                                                   size_t                                          index,
                                                   Arg&                                            arg,
                                                   BufferAcquireSet&                               bas,
                                                   bool                                            do_not_dispatch)
            {
                static_assert(hetcompute::internal::is_api20_buffer_ptr<Param>::value, "Param must be a hetcompute::buffer_ptr");

                typedef kernel_param_ptr_traits<Param> param_traits;
                using direction = typename param_traits::direction;

                typedef kernel_arg_ptr_traits<Arg> arg_traits;

                static_assert(std::is_same<typename param_traits::naked_type, typename arg_traits::naked_type>::value == true,
                              "Incompatible hetcompute::buffer_ptr data types");

                static_assert(param_traits::is_const::value == false || std::is_same<direction, input_param>::value == true,
                              "buffer_ptr of const data type may only be declared as input to kernel");

                static_assert(arg_traits::is_const::value == false || std::is_same<direction, input_param>::value == true,
                              "A buffer_ptr<const T> argument may only be passed as a kernel input");

                if (do_not_dispatch == true)
                {
                    // construct buffer acquire set, do not dispatch yet
                    auto acquire_action =
                        (std::is_same<direction, input_param>::value == true ?
                             bufferpolicy::acquire_r :
                             (std::is_same<direction, output_param>::value == true ? bufferpolicy::acquire_w : bufferpolicy::acquire_rw));
                    bas.add(arg, acquire_action);
                }
                else
                {
                    // dispatch a previously acquired buffer argument to the kernel
                    cl_arg_api_buffer_setter::set(device, kernel, index, arg, direction(), bas);
                }
            }

#ifdef HETCOMPUTE_HAVE_OPENCL
            /// Param is a texture_ptr.
            /// arg is a hetcompute::buffer_ptr.
            template <typename Param, typename Arg, typename BufferAcquireSet, typename Kernel>
            void parse_api_buffer_ptr_arg_dispatch(std::false_type,
                                                   ::hetcompute::internal::legacy::device_ptr const& device,
                                                   Kernel*                                         kernel,
                                                   size_t                                          index,
                                                   Arg&                                            arg,
                                                   BufferAcquireSet&                               bas,
                                                   bool                                            do_not_dispatch)
            {
                static_assert(is_texture_ptr<Param>::value, "Param must be a hetcompute::texture_ptr");

                if (do_not_dispatch == true)
                {
                    // construct buffer acquire set, do not dispatch yet

                    // textures are currently always rw -- until kernel sig starts to specify direction
                    auto acquire_action = bufferpolicy::acquire_rw;

                    auto& bb              = reinterpret_cast<hetcompute::internal::buffer_ptr_base&>(arg);
                    auto  buf_as_tex_info = buffer_accessor::get_buffer_as_texture_info(bb);

                    using tpinfo = hetcompute::graphics::internal::texture_ptr_info<Param>;
                    HETCOMPUTE_API_ASSERT(buf_as_tex_info.get_used_as_texture(), "buffer_ptr is not setup to be treated as texture");
                    HETCOMPUTE_INTERNAL_ASSERT(buf_as_tex_info.get_dims() > 0 && buf_as_tex_info.get_dims() <= 3,
                                             "buffer_ptr treated as texture of invalid dimension");
                    HETCOMPUTE_API_ASSERT(buf_as_tex_info.get_dims() == tpinfo::dims,
                                        "buffer_ptr is setup to be interpreted as %d dimensional, but kernel requires %d dimensions",
                                        buf_as_tex_info.get_dims(),
                                        tpinfo::dims);

                    auto bufstate = c_ptr(buffer_accessor::get_bufstate(bb));
                    HETCOMPUTE_INTERNAL_ASSERT(bufstate != nullptr, "Null bufstate");
                    auto preexisting_tex_a = bufstate->get(TEXTURE_ARENA);
                    if (preexisting_tex_a == nullptr || !arena_storage_accessor::texture_arena_has_format(preexisting_tex_a))
                    {
                        // No texture of appropriate format exists in buffer. Send in a new texture to be saved in the texture_arena.
                        HETCOMPUTE_DLOG("Allocating new texture to save in texture_arena of bufstate=%p", bufstate);
                        bb.allocate_host_accessible_data(true);
                        bool has_ion_ptr = false;
#ifdef HETCOMPUTE_HAVE_QTI_DSP
                        auto preexisting_ion_a = bufstate->get(ION_ARENA);
                        has_ion_ptr = (preexisting_ion_a != nullptr && arg.host_data() == arena_storage_accessor::get_ptr(preexisting_ion_a));
#endif // HETCOMPUTE_HAVE_QTI_DSP
                        auto tex_cl = new hetcompute::graphics::internal::texture_cl<
                            tpinfo::img_format,
                            tpinfo::dims>(hetcompute::graphics::internal::image_size_converter<tpinfo::dims, 3>::convert(
                                              buf_as_tex_info.get_img_size()),
                                          false,
                                          arg.host_data(),
                                          has_ion_ptr,
                                          false);
                        HETCOMPUTE_INTERNAL_ASSERT(tex_cl != nullptr,
                                                 "internal texture_cl creation failed for bufferstate=%p",
                                                 ::hetcompute::internal::c_ptr(buffer_accessor::get_bufstate(bb)));
                        HETCOMPUTE_DLOG("Created tex_cl=%p for bufstate=%p",
                                      tex_cl,
                                      ::hetcompute::internal::c_ptr(buffer_accessor::get_bufstate(bb)));
                        buf_as_tex_info.access_tex_cl() = tex_cl;
                    }

                    bas.add(arg, acquire_action, buf_as_tex_info);
                }
                else
                {
                    // dispatch a previously acquired buffer argument *as texture* to the kernel

                    auto acquired_arena = bas.find_acquired_arena(arg);

                    auto tex_cl = arena_storage_accessor::access_texture_arena_for_gputask(acquired_arena);
                    cl_arg_texture_setter::set(device, kernel, index, tex_cl);
                }
            }
#endif // HETCOMPUTE_HAVE_OPENCL

            /// Parses argument if arg is a hetcompute::buffer_ptr
            /// @param index Argument index
            /// @param arg Reference to argument
            template <typename Param, typename Arg, typename BufferAcquireSet, typename Kernel>
            void parse_api_buffer_ptr_arg(::hetcompute::internal::legacy::device_ptr const& device,
                                          Kernel*                                         kernel,
                                          size_t                                          index,
                                          Arg&                                            arg,
                                          BufferAcquireSet&                               bas,
                                          bool                                            do_not_dispatch)
            {
                static_assert(hetcompute::internal::is_api20_buffer_ptr<Param>::value || is_texture_ptr<Param>::value,
                              "Unexpected hetcompute::buffer_ptr argument");

                parse_api_buffer_ptr_arg_dispatch<Param, Arg, BufferAcquireSet>(hetcompute::internal::is_api20_buffer_ptr<Param>(), // Param is
                                                                                                                                // buffer or
                                                                                                                                // texture?
                                                                                device,
                                                                                kernel,
                                                                                index,
                                                                                arg,
                                                                                bas,
                                                                                do_not_dispatch);
            }

#ifdef HETCOMPUTE_HAVE_OPENCL
            /// Parses argument if arg is a texture_ptr
            /// @param index Argument index
            /// @param arg Reference to argument
            template <typename Param, typename Arg, typename Kernel>
            void parse_texture_ptr_arg(::hetcompute::internal::legacy::device_ptr const& device,
                                       Kernel*                                         kernel,
                                       size_t                                          index,
                                       Arg&                                            arg,
                                       bool                                            do_not_dispatch)
            {
                // Check first whether the parameter is also a texture_ptr
                static_assert(is_texture_ptr<Param>::value, "Unexpected texture_ptr argument");

                static_assert(std::is_same<Param, Arg>::value, "Incompatible texture_ptr types");

                if (do_not_dispatch == false)
                {
                    cl_arg_texture_setter::set(device, kernel, index, internal::c_ptr(arg));
                }
            }
#endif // HETCOMPUTE_HAVE_OPENCL

#ifdef HETCOMPUTE_HAVE_OPENCL
            /// Parses argument if arg is a sampler_ptr
            /// @param index Argument index
            /// @param arg Reference to argument
            template <typename Param, typename Arg, typename Kernel>
            void parse_sampler_ptr_arg(::hetcompute::internal::legacy::device_ptr const& device,
                                       Kernel*                                         kernel,
                                       size_t                                          index,
                                       Arg&                                            arg,
                                       bool                                            do_not_dispatch)
            {
                // sampler_ptr type check
                static_assert(is_sampler_ptr<Param>::value, "Expected sampler_ptr argument");

                static_assert(std::is_same<Param, Arg>::value, "Incompatible sampler_ptr types");

                if (do_not_dispatch == false)
                {
                    cl_arg_sampler_setter::set(device, kernel, index, arg);
                }
            }
#endif // HETCOMPUTE_HAVE_OPENCL

            enum class dispatch_type
            {
                normal,
                local_alloc,
                api_buffer,
                texture,
                sampler
            };

            template <bool is_arg_a_local_alloc, bool is_arg_an_api_buffer_ptr, bool is_arg_a_texture_ptr, bool is_arg_a_sampler_ptr>
            struct translate_flags_to_dispatch_type;

            template <>
            struct translate_flags_to_dispatch_type<false, false, false, false>
            {
                static auto const value = dispatch_type::normal;
            };

            template <>
            struct translate_flags_to_dispatch_type<true, false, false, false>
            {
                static auto const value = dispatch_type::local_alloc;
            };

            template <>
            struct translate_flags_to_dispatch_type<false, true, false, false>
            {
                static auto const value = dispatch_type::api_buffer;
            };

            template <>
            struct translate_flags_to_dispatch_type<false, false, true, false>
            {
                static auto const value = dispatch_type::texture;
            };

            template <>
            struct translate_flags_to_dispatch_type<false, false, false, true>
            {
                static auto const value = dispatch_type::sampler;
            };

#ifdef HETCOMPUTE_HAVE_OPENCL
            // Dispatcher methods: selectively dispatch on DispatchType argument.
            template <typename Param, typename Arg, dispatch_type DispatchType, typename BufferAcquireSet, typename Kernel>
            typename std::enable_if<DispatchType == dispatch_type::normal, void>::type
            parse_arg_dispatch(::hetcompute::internal::legacy::device_ptr const& device,
                               Kernel*                                         kernel,
                               size_t                                          index,
                               Arg&                                            arg,
                               BufferAcquireSet&,
                               bool do_not_dispatch)
            {
                parse_normal_arg<Param, Arg>(device, kernel, index, arg, do_not_dispatch);
            }
#endif // HETCOMPUTE_HAVE_OPENCL

#ifdef HETCOMPUTE_HAVE_OPENCL
            template <typename Param, typename Arg, dispatch_type DispatchType, typename BufferAcquireSet, typename Kernel>
            typename std::enable_if<DispatchType == dispatch_type::local_alloc, void>::type
            parse_arg_dispatch(::hetcompute::internal::legacy::device_ptr const& device,
                               Kernel*                                         kernel,
                               size_t                                          index,
                               Arg&                                            arg,
                               BufferAcquireSet&,
                               bool do_not_dispatch)
            {
                parse_local_alloc_arg<Param, Arg>(device, kernel, index, arg, do_not_dispatch);
            }
#endif // HETCOMPUTE_HAVE_OPENCL

            template <typename Param, typename Arg, dispatch_type DispatchType, typename BufferAcquireSet, typename Kernel>
            typename std::enable_if<DispatchType == dispatch_type::api_buffer, void>::type
            parse_arg_dispatch(::hetcompute::internal::legacy::device_ptr const& device,
                               Kernel*                                         kernel,
                               size_t                                          index,
                               Arg&                                            arg,
                               BufferAcquireSet&                               bas,
                               bool                                            do_not_dispatch)
            {
                parse_api_buffer_ptr_arg<Param, Arg>(device, kernel, index, arg, bas, do_not_dispatch);
            }

#ifdef HETCOMPUTE_HAVE_OPENCL
            template <typename Param, typename Arg, dispatch_type DispatchType, typename BufferAcquireSet, typename Kernel>
            typename std::enable_if<DispatchType == dispatch_type::texture, void>::type
            parse_arg_dispatch(::hetcompute::internal::legacy::device_ptr const& device,
                               Kernel*                                         kernel,
                               size_t                                          index,
                               Arg&                                            arg,
                               BufferAcquireSet&,
                               bool do_not_dispatch)
            {
                parse_texture_ptr_arg<Param, Arg>(device, kernel, index, arg, do_not_dispatch);
            }
#endif // HETCOMPUTE_HAVE_OPENCL

#ifdef HETCOMPUTE_HAVE_OPENCL
            template <typename Param, typename Arg, dispatch_type DispatchType, typename BufferAcquireSet, typename Kernel>
            typename std::enable_if<DispatchType == dispatch_type::sampler, void>::type
            parse_arg_dispatch(::hetcompute::internal::legacy::device_ptr const& device,
                               Kernel*                                         kernel,
                               size_t                                          index,
                               Arg&                                            arg,
                               BufferAcquireSet&,
                               bool do_not_dispatch)
            {
                parse_sampler_ptr_arg<Param, Arg>(device, kernel, index, arg, do_not_dispatch);
            }
#endif // HETCOMPUTE_HAVE_OPENCL

            template <typename Params, typename Args, typename NormalArgs, size_t index = 0, typename BufferAcquireSet, typename Kernel>
            typename std::enable_if<index == std::tuple_size<Args>::value, void>::type
            prepare_args_pass(::hetcompute::internal::legacy::device_ptr const&, Kernel*, Args&&, NormalArgs&, BufferAcquireSet&, bool)
            {
            }

            /// Param is not a normal arg
            template <typename Norm, typename Arg, bool IsParamNormal>
            struct select_normal_arg
            {
                static Arg& get(Norm&, Arg& arg) { return arg; }
            };

            /// Param is normal arg
            template <typename Norm, typename Arg>
            struct select_normal_arg<Norm, Arg, true>
            {
                static Norm& get(Norm& norm, Arg&) { return norm; }
            };

            template <typename Params, typename Args, typename NormalArgs, size_t index = 0, typename BufferAcquireSet, typename Kernel>
                typename std::enable_if <
                index<std::tuple_size<Args>::value, void>::type prepare_args_pass(::hetcompute::internal::legacy::device_ptr const& device,
                                                                                  Kernel*                                           kernel,
                                                                                  Args&&                                            args,
                                                                                  NormalArgs&       normal_args,
                                                                                  BufferAcquireSet& bas,
                                                                                  bool              do_not_dispatch)
            {
                // Fail if the number of paraments is different than the number of arguments.
                static_assert(std::tuple_size<Args>::value == std::tuple_size<Params>::value,
                              "Number of parameters is different to number of arguments");

                typedef typename std::tuple_element<index, Params>::type     param_type;
                typedef typename std::tuple_element<index, Args>::type       arg_type;
                typedef typename std::tuple_element<index, NormalArgs>::type norm_type;

                typedef typename is_local_alloc<param_type>::type                        is_param_a_local_alloc;
                typedef typename hetcompute::internal::is_api20_buffer_ptr<arg_type>::type is_arg_an_api_buffer_ptr;
                typedef typename is_texture_ptr<arg_type>::type                          is_arg_a_texture_ptr;
                typedef typename is_sampler_ptr<arg_type>::type                          is_arg_a_sampler_ptr;

                auto constexpr is_norm   = is_normal_arg<param_type>::value;
                using param_type_to_pass = typename std::conditional<is_norm, typename std::remove_cv<param_type>::type, param_type>::type;
                using arg_type_to_pass   = typename std::conditional<is_norm, norm_type, arg_type>::type;
                auto& arg_to_pass = select_normal_arg<norm_type, arg_type, is_norm>::get(std::get<index>(normal_args), std::get<index>(args));

                parse_arg_dispatch<param_type_to_pass,
                                   arg_type_to_pass,
                                   translate_flags_to_dispatch_type<is_param_a_local_alloc::value,
                                                                    is_arg_an_api_buffer_ptr::value,
                                                                    is_arg_a_texture_ptr::value,
                                                                    is_arg_a_sampler_ptr::value>::value>(device,
                                                                                                         kernel,
                                                                                                         index,
                                                                                                         arg_to_pass,
                                                                                                         bas,
                                                                                                         do_not_dispatch);

                prepare_args_pass<Params, Args, NormalArgs, index + 1>(device, kernel, std::forward<Args>(args), normal_args, bas, do_not_dispatch);
            }

            /// prepare_args validates task argument-types Args against the GPU
            /// kernel signature Params, atomically acquires the buffer arguments,
            /// and dispatches all the arguments to the OpenCL runtime.
            ///
            /// @param device The GPU device to dispatch arguments to
            /// @param kernel The GPU kernel corresponding to Params
            /// @param args   The task parameters
            /// @param bas    The buffer-acquire-set used to atomically acquire all
            ///               the buffer arguments in args
            /// @param requestor A unique ID for the current GPU task, the task* by convention.
            ///
            /// @param add_buffers
            ///               =true  ==> set up buffer acquire set
            ///               =false ==> do not set up buffer acquire set (has already been set up)
            ///
            /// @param acquire_buffers
            ///               =true  ==> add all buffers in args to bas, and then
            ///                    block until the buffers are atomically acquired.
            ///               =false ==> skip updating bas and the atomic acquire of buffers
            ///
            /// @param dispatch_args
            ///               = true  ==> dispatch args to the OpenCL runtime.
            ///               = false ==> do not dispatch args to the OpenCL runtime.
            ///
            /// @returns false if a buffer conflict occurred, else true
            template <typename Params, typename Args, typename NormalArgs, typename BufferAcquireSet, typename PreacquiredArenas, typename Kernel>
            bool prepare_args(::hetcompute::internal::legacy::device_ptr const& device,
                              Kernel*                                         k_ptr,
                              Args&&                                          args,
                              NormalArgs&                                     normal_args,
                              BufferAcquireSet&                               bas,
                              PreacquiredArenas const*                        p_preacquired_arenas,
                              void const*                                     requestor,
                              bool                                            add_buffers     = true,
                              bool                                            acquire_buffers = true,
                              bool                                            dispatch_args   = true)
            {
                HETCOMPUTE_INTERNAL_ASSERT(k_ptr != nullptr, "null kernel");

                if (add_buffers)
                {
                    // Pass 1: setup _buffer_acquire_set, do not dispatch args to kernel
                    if (0)
                    {
                    }
#ifdef HETCOMPUTE_HAVE_OPENCL
                    else if (k_ptr->is_cl())
                    {
                        prepare_args_pass<Params, Args>(device, k_ptr->get_cl_kernel(), std::forward<Args>(args), normal_args, bas, true);
                    }
#endif // HETCOMPUTE_HAVE_OPENCL
#ifdef HETCOMPUTE_HAVE_GLES
                    else if (k_ptr->is_gl())
                    {
                        prepare_args_pass<Params, Args>(device, k_ptr->get_gl_kernel(), std::forward<Args>(args), normal_args, bas, true);
                    }
#endif // HETCOMPUTE_HAVE_GLES
                    else
                    {
                        HETCOMPUTE_FATAL("Unsupported type of GPU kernel");
                    }
                }

                if (acquire_buffers)
                {
                    hetcompute::internal::executor_device ed = hetcompute::internal::executor_device::unspecified;
#ifdef HETCOMPUTE_HAVE_OPENCL
                    if (k_ptr->is_cl())
                    {
                        ed = hetcompute::internal::executor_device::gpucl;
                    }
#endif // HETCOMPUTE_HAVE_OPENCL
#ifdef HETCOMPUTE_HAVE_GLES
                    if (k_ptr->is_gl())
                    {
                        ed = hetcompute::internal::executor_device::gpugl;
                    }
#endif // HETCOMPUTE_HAVE_GLES

                    // attempt to acquire all the buffers, otherwise report conflict
                    bas.acquire_buffers(requestor,
                                        { ed },
                                        true,
                                        (p_preacquired_arenas != nullptr && p_preacquired_arenas->has_any() ? p_preacquired_arenas : nullptr));
                    if (!bas.acquired())
                    {
                        return false; // conflict found during buffer acquisition
                    }
                }

                if (dispatch_args)
                {
                    // Pass 2: dispatch args to kernel
                    if (0)
                    {
                    }
#ifdef HETCOMPUTE_HAVE_OPENCL
                    else if (k_ptr->is_cl())
                    {
                        prepare_args_pass<Params, Args>(device, k_ptr->get_cl_kernel(), std::forward<Args>(args), normal_args, bas, false);
                    }
#endif // HETCOMPUTE_HAVE_OPENCL
#ifdef HETCOMPUTE_HAVE_GLES
                    else if (k_ptr->is_gl())
                    {
                        prepare_args_pass<Params, Args>(device, k_ptr->get_gl_kernel(), std::forward<Args>(args), normal_args, bas, false);
                    }
#endif // HETCOMPUTE_HAVE_GLES
                    else
                    {
                        HETCOMPUTE_FATAL("Unsupported type of GPU kernel");
                    }
                }

                return true;
            }
        }; // namespace gpu_kernel_dispatch

#ifdef HETCOMPUTE_HAVE_OPENCL
        void CL_CALLBACK task_bundle_completion_callback(cl_event event, cl_int, void* user_data);
#endif // HETCOMPUTE_HAVE_OPENCL

        /// Used by the scheduler to dispatch a sequence of gputasks as a bundle.
        /// The superset of buffers used by the gputask-sequence is acquired collectively
        /// before the first gputask in the sequence is executed, and released collectively
        /// after the last gputask completes.
        ///
        /// Bundle dispatch requires the following semantics:
        ///   - scheduler must add gputasks in sequence to a bundle via add()
        ///   - scheduler must not invoke gputask::do_execute()
        ///   - instead, the scheduler must invoke execute_all()
        class task_bundle_dispatch
        {
            /// Tasks for bundle dispatch: will be executed in order by execute_all().
            std::vector<gputask_base*> _tasks;

            // Tracks scheduling depth. Used to determine when to call execute_all(), and
            // to ensure that the bundle is not executed more than once due to a flaw in
            // the scheduling algorithm.
            unsigned int _depth;

            using dyn_sized_bas = buffer_acquire_set<0, false>;
            using dyn_sized_pa  = preacquired_arenas<false>;

            dyn_sized_bas _bas;
            dyn_sized_pa  _preacquired_arenas;

            void add(task* t, bool first = false)
            {
                HETCOMPUTE_ILOG("task_bundle_dispatch() %p: adding gputask %p", this, t);
                HETCOMPUTE_INTERNAL_ASSERT(t != nullptr, "Cannot add a null task to task_bundle_dispatch");
                auto gt = static_cast<gputask_base*>(t);
                HETCOMPUTE_INTERNAL_ASSERT(gt != nullptr, "task_bundle_dispatch: received a non-GPU task");
                if (!first)
                {
                    _tasks.push_back(gt);
                }
                else
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_tasks[0] == nullptr, "first element must be written exactly once");
                    _tasks[0] = gt;
                }
                gt->configure_for_bundle_dispatch();

                HETCOMPUTE_ILOG("task_bundle_dispatch() %p: adding gputask %p", this, t);
                gt->update_buffer_acquire_set(_bas);
            }

            /**
             * add root task of bundle
             * @param t root task
             */
            void add_root(task* t) { add(t, true); }

        public:
            task_bundle_dispatch() : _tasks(), _depth(0), _bas(), _preacquired_arenas()
            {
                _tasks.reserve(8);
                _tasks.push_back(nullptr); // reserve first element -- will be updated later
                                           // with task that triggered bundling
            }

            /**
             * Add task to bundle. Must not be the root task.
             * @param t task to add
             */
            void add_non_root(task* t) { add(t, false); }

            /**
             * Execute all tasks in bundle including the specified root task
             * @param root_task root task of bundle that initiated bundling
             */
            void execute_all(task* root_task)
            {
                HETCOMPUTE_INTERNAL_ASSERT(_tasks.size() > 0,
                                         "task_bundle_dispatch should be called on bundles of"
                                         "at least two tasks (including the root_task)");
                HETCOMPUTE_INTERNAL_ASSERT(_depth == 0, "Can call execute_all only at scheduling depth of zero");

                add_root(root_task);

                HETCOMPUTE_ILOG("task_bundle_dispatch %p: execute_all()", this);

                HETCOMPUTE_INTERNAL_ASSERT(root_task != nullptr, "root task is null");
                void const* requestor = root_task; // use the root task as the unique requestor ID

                // FIXME: Assumes that bundle consists only of gputasks.
                //  get_executor_device() should be moved to class task to enable bundles of other types.
                auto gpu_root_task = static_cast<gputask_base*>(root_task);
                HETCOMPUTE_INTERNAL_ASSERT(gpu_root_task != nullptr, "Current limitation: root task must be a gputask.");
                auto ed = gpu_root_task->get_executor_device();

                // block until all the buffers are acquired
                _bas.blocking_acquire_buffers(requestor, { ed }, _preacquired_arenas.has_any() ? &_preacquired_arenas : nullptr);
                for (auto& gt : _tasks)
                {
                    HETCOMPUTE_INTERNAL_ASSERT(gt != nullptr, "Null gputask found during bundle dispatch");
                    HETCOMPUTE_INTERNAL_ASSERT(gt->get_executor_device() == ed, "Mismatch in executor device type in bundle");
                    gt->execute_sync(this);
                }
            }

            std::vector<gputask_base*> const& get_tasks() { return _tasks; }

            /**
             * Depth is used to track when to invoke execute_all() on the bundle.
             * It is invoked when the recursive task graph traversal unwinds back to
             * depth zero.
             */
            void increment_depth() { _depth++; }

            void decrement_depth() { _depth--; }

            unsigned int get_depth() const { return _depth; }

            /**
             * Returns whether more than one task exists in the bundle.
             * @return true if more than one task exists in the bundle, false otherwise
             */
            bool contains_many() const { return _tasks.size() > 1; }

            dyn_sized_bas& get_buffer_acquire_set() { return _bas; }

            dyn_sized_pa& get_preacquired_arenas() { return _preacquired_arenas; }

            void register_preacquired_arena(bufferstate* bufstate, arena* preacquired_arena)
            {
                _preacquired_arenas.register_preacquired_arena(bufstate, preacquired_arena);
            }
        };  // class task_bundle_dispatch

#ifdef HETCOMPUTE_HAVE_OPENCL
        void CL_CALLBACK completion_callback(cl_event event, cl_int, void* user_data);
#endif // HETCOMPUTE_HAVE_OPENCL

        template <typename TupledParams, size_t index = std::tuple_size<TupledParams>::value, typename... ExtractedKernelArgs>
        struct kernel_ptr_for_tupled_params
        {
            using type = typename kernel_ptr_for_tupled_params<TupledParams,
                                                               index - 1,
                                                               typename std::tuple_element<index - 1, TupledParams>::type,
                                                               ExtractedKernelArgs...>::type;
        };

        template <typename TupledParams, typename... ExtractedKernelArgs>
        struct kernel_ptr_for_tupled_params<TupledParams, 0, ExtractedKernelArgs...>
        {
            using type = ::hetcompute::internal::legacy::kernel_ptr<ExtractedKernelArgs...>;
        };

        /// normal_args_tuple_type<Params>::type will be a std::tuple
        /// where the i'th element has type std::tuple_element<i, Params>::type if it is
        /// a normal arg, and has type char if not a normal arg. We use char instead of void
        /// b/c you cannot have reference to object of type tuple<..., void, ...>.
        ///
        /// Useful for making a local copy of all the normal arg values, so the normal
        /// arg Lvalues are guaranteed to stay allocated till the task launches.
        template <typename Params>
        struct normal_args_tuple_type;

        template <typename... Ts>
        struct normal_args_tuple_type<std::tuple<Ts...>>
        {
            using type = std::tuple<
                typename std::conditional<gpu_kernel_dispatch::is_normal_arg<Ts>::value, typename std::remove_cv<Ts>::type, char>::type...>;
        };

        /// Copies a single element at index Index from Params to normal_args,
        /// but only if the element is a normal arg.
        template <typename Params, typename Args, size_t Index, bool IsNormal>
        struct normal_args_tuple_single_copier
        {
            normal_args_tuple_single_copier(Args&, typename normal_args_tuple_type<Params>::type&)
            {
                // do nothing when not a normal arg
            }
        };

        template <typename Params, typename Args, size_t Index>
        struct normal_args_tuple_single_copier<Params, Args, Index, true>
        {
            normal_args_tuple_single_copier(Args& args, typename normal_args_tuple_type<Params>::type& normal_args)
            {
                std::get<Index>(normal_args) = std::get<Index>(args);
            }
        };

        /// Copies the normal args from tuple params to tuple normal_args
        template <typename Params, typename Args, size_t Index>
        struct normal_args_tuple_copier
        {
            using is_normal = gpu_kernel_dispatch::is_normal_arg<typename std::tuple_element<Index - 1, Params>::type>;

            normal_args_tuple_copier(Args& args, typename normal_args_tuple_type<Params>::type& normal_args)
            {
                normal_args_tuple_single_copier<Params, Args, Index - 1, is_normal::value>(args, normal_args);
                normal_args_tuple_copier<Params, Args, Index - 1>(args, normal_args);
            }
        };

        template <typename Params, typename Args>
        struct normal_args_tuple_copier<Params, Args, 0>
        {
            normal_args_tuple_copier(Args&, typename normal_args_tuple_type<Params>::type&) {}
        };

        /// Container to hold normal args Lvalues during task execution
        template <typename Params, typename Args>
        struct normal_args_container
        {
            using type = typename normal_args_tuple_type<Params>::type;
            type _tp;

            explicit normal_args_container(Args& args) : _tp()
            {
                normal_args_tuple_copier<Params, Args, std::tuple_size<Params>::value>(args, _tp);
            }
        };

        HETCOMPUTE_GCC_IGNORE_BEGIN("-Weffc++");
        struct gpu_launch_info
        {
            bool dummy;
#ifdef HETCOMPUTE_HAVE_OPENCL
            clevent   _cl_completion_event;
            cldevice* _d_ptr;
#endif // HETCOMPUTE_HAVE_OPENCL
#ifdef HETCOMPUTE_HAVE_GLES
            GLsync _gl_fence;
#endif // HETCOMPUTE_HAVE_GLES

            explicit gpu_launch_info(::hetcompute::internal::legacy::device_ptr const& device)
                : dummy(true)
#ifdef HETCOMPUTE_HAVE_OPENCL
                  ,
                  _cl_completion_event(),
                  _d_ptr(internal::c_ptr(device))
#endif // HETCOMPUTE_HAVE_OPENCL
#ifdef HETCOMPUTE_HAVE_GLES
                  ,
                  _gl_fence()
#endif // HETCOMPUTE_HAVE_GLES
            {
                HETCOMPUTE_UNUSED(device);
            }
        };  // class gpu_launch_info
        HETCOMPUTE_GCC_IGNORE_END("-Weffc++");

        template <size_t Dims, typename Fn, typename Params, typename Args>
        class gputask : public gputask_base
        {
            static constexpr size_t num_buffer_args = num_buffer_ptrs_in_tuple<Args>::value;

        private:
            gpu_launch_info                                     _gli;
            Args                                                _kernel_args;
            typename kernel_ptr_for_tupled_params<Params>::type _kernel_ptr;
            normal_args_container<Params, Args>                 _normal_args_container;
            ::hetcompute::range<Dims>                             _global_range;
            ::hetcompute::range<Dims>                             _local_range;
            ::hetcompute::internal::legacy::device_ptr const&     _device;
            Fn&                                                 _fn; // cpu version; deprecated
            buffer_acquire_set<num_buffer_args>                 _buffer_acquire_set;
            preacquired_arenas<true, num_buffer_args>           _preacquired_arenas;

            HETCOMPUTE_DELETE_METHOD(gputask(gputask const&));
            HETCOMPUTE_DELETE_METHOD(gputask(gputask&&));
            HETCOMPUTE_DELETE_METHOD(gputask& operator=(gputask const&));
            HETCOMPUTE_DELETE_METHOD(gputask& operator=(gputask&&));

            virtual executor_device get_executor_device() const;
            virtual bool            do_execute(task_bundle_dispatch* tbd = nullptr);

        public:
            template <typename Kernel>
            gputask(::hetcompute::internal::legacy::device_ptr const& device,
                    const ::hetcompute::range<Dims>&                  r,
                    const ::hetcompute::range<Dims>&                  l,
                    Fn&                                             f,
                    legacy::task_attrs                              a,
                    Kernel                                          kernel,
                    Args&                                           kernel_args)
                : gputask_base(a),
                  _gli(device),
                  _kernel_args(kernel_args),
                  _kernel_ptr(kernel),
                  _normal_args_container(kernel_args),
                  _global_range(r),
                  _local_range(l),
                  _device(device),
                  _fn(f),
                  _buffer_acquire_set(),
                  _preacquired_arenas()
            {
            }

            // refer to class internal::task
            void unsafe_enable_non_locking_buffer_acquire() { _buffer_acquire_set.enable_non_locking_buffer_acquire(); }

            // refer to class internal::task
            void unsafe_register_preacquired_arena(bufferstate* bufstate, arena* preacquired_arena)
            {
                _preacquired_arenas.register_preacquired_arena(bufstate, preacquired_arena);
            }

            virtual void update_buffer_acquire_set(dyn_sized_bas& bas);

            virtual void release_arguments();
        };

        template <size_t Dims, typename Fn, typename Params, typename Args>
        void gputask<Dims, Fn, Params, Args>::update_buffer_acquire_set(dyn_sized_bas& bas)
        {
            auto k_ptr = internal::c_ptr(_kernel_ptr);
            HETCOMPUTE_INTERNAL_ASSERT((k_ptr != nullptr), "null kernel");

            // kernel is a dummy argument as the "update bas" pass will never use it
#ifdef HETCOMPUTE_HAVE_OPENCL
            clkernel* kernel = nullptr;
#else  // HETCOMPUTE_HAVE_OPENCL
            glkernel* kernel = nullptr;
#endif // HETCOMPUTE_HAVE_OPENCL

            // First pass only: will update bas
            gpu_kernel_dispatch::prepare_args_pass<Params, Args>(_device,
                                                                 kernel,
                                                                 std::forward<Args>(_kernel_args),
                                                                 _normal_args_container._tp,
                                                                 bas, // NOTE: instead of _buffer_acquire_set
                                                                 true);
        }

        template <size_t Dims, typename Fn, typename Params, typename Args>
        executor_device gputask<Dims, Fn, Params, Args>::get_executor_device() const
        {
            auto k_ptr = internal::c_ptr(_kernel_ptr);
            HETCOMPUTE_INTERNAL_ASSERT((k_ptr != nullptr), "null kernel");
#ifdef HETCOMPUTE_HAVE_OPENCL
            if (k_ptr->is_cl())
                return executor_device::gpucl;
#endif // HETCOMPUTE_HAVE_OPENCL
#ifdef HETCOMPUTE_HAVE_GLES
            if (k_ptr->is_gl())
                return executor_device::gpugl;
#endif // HETCOMPUTE_HAVE_GLES

            HETCOMPUTE_UNREACHABLE("Invalid kernel");
            return executor_device::unspecified;
        }

        /**
         *  Captures an execution request via some programmatic construct, i.e.,
         *  an async task or a blocking invocation of code.
         *
         *  is_blocking() == true  ==> not a task
         *  is_blocking() == false ==> a task
         *
         *  The executor construct could be part of a bundle. If so, the captured information
         *  indicates if the construct is the last in to execute in the bundle so that appropriate
         *  clean up can be done after it completes execution.
         *
         *  is_bundled() indicates if the executor construct is part of a bundle.
         *
         *  get_task_bundle() provides the task bundle when the executor construct is a task
         *  (i.e., when is_blocking() == false).
         *
         *  A requestor id (a task pointer if async, otherwise a unique integer) is used to
         *  uniquely identify an executor construct among all concurrent executor constructs
         *  that might access a  buffer. The requestor must not be dereferenced as a pointer
         *  unless is_blocking() == false, indicating that the requestor is a task pointer.
         */
        class executor_construct
        {
        public:
            explicit executor_construct(gputask_base* requestor_task, task_bundle_dispatch* tbd = nullptr)
                : _requestor(requestor_task),
                  _is_blocking(false),
                  _is_bundled(tbd != nullptr),
                  _is_last_in_bundle(tbd != nullptr && tbd->get_tasks().back() == requestor_task),
                  _tbd(tbd)
            {
                HETCOMPUTE_INTERNAL_ASSERT(requestor_task != nullptr, "Invalid task");
                HETCOMPUTE_INTERNAL_ASSERT(is_bundled() || !is_last_in_bundle(), "is_last_in_bundle cannot be true when not bundled");
            }

            executor_construct(void* blocking_requestor_id, bool requestor_is_bundled, bool requestor_is_last_in_bundle)
                : _requestor(blocking_requestor_id),
                  _is_blocking(true),
                  _is_bundled(requestor_is_bundled),
                  _is_last_in_bundle(requestor_is_last_in_bundle),
                  _tbd(nullptr)
            {
                HETCOMPUTE_INTERNAL_ASSERT(blocking_requestor_id != nullptr, "Invalid blocking requestor id -- needs to be unique");
                HETCOMPUTE_INTERNAL_ASSERT(is_bundled() || !is_last_in_bundle(), "is_last_in_bundle cannot be true when not bundled");
            }

            void* get_requestor() const { return _requestor; }

            bool is_blocking() const { return _is_blocking; }

            bool is_bundled() const { return _is_bundled; }

            bool is_last_in_bundle() const { return _is_last_in_bundle; }

            task_bundle_dispatch* get_task_bundle() const
            {
                HETCOMPUTE_INTERNAL_ASSERT(!is_blocking(), "Construct is not a task");
                return _tbd;
            }

            gputask_base* get_requestor_task() const
            {
                HETCOMPUTE_INTERNAL_ASSERT(!is_blocking(), "Not a task");
                return static_cast<gputask_base*>(_requestor);
            }

            std::string to_string() const
            {
                return hetcompute::internal::strprintf("(requestor=%p %s %s %s tbd=%p)",
                                                     _requestor,
                                                     (_is_blocking ? "Blocking" : "NonBlocking"),
                                                     (_is_bundled ? "Bundled" : "UnBundled"),
                                                     (_is_last_in_bundle ? "Last" : "NotLast"),
                                                     _tbd);
            }

        private:
            void* _requestor;
            bool  _is_blocking;
            bool  _is_bundled;
            bool  _is_last_in_bundle;

            // != nullptr only if _requestor is a task (i.e., _launch_type == async)
            task_bundle_dispatch* _tbd;
        };  // class executor_construct

#ifdef HETCOMPUTE_HAVE_OPENCL
        HETCOMPUTE_GCC_IGNORE_BEGIN("-Wshadow");
        template <typename Range>
        void launch_cl_kernel(cldevice*                 d_ptr,
                              executor_construct const& exec_cons,
                              clkernel*                 cl_kernel,
                              Range&                    global_range,
                              Range&                    local_range,
                              clevent&                  cl_completion_event)
        {
            HETCOMPUTE_INTERNAL_ASSERT(cl_kernel != nullptr, "Invalid cl_kernel");

            cl_completion_event = d_ptr->launch_kernel(cl_kernel, global_range, local_range);

            if (exec_cons.is_blocking())
            {
                return; // no need to set callback, the calling scope must block
            }

            HETCOMPUTE_INTERNAL_ASSERT(!exec_cons.is_blocking(), "Must be a task to be launched async");
            auto gputask = exec_cons.get_requestor();
            HETCOMPUTE_INTERNAL_ASSERT(gputask != nullptr, "non-blocking execution requires valid gputask");

            auto tbd = exec_cons.get_task_bundle();
            HETCOMPUTE_INTERNAL_ASSERT(tbd != nullptr || !exec_cons.is_bundled(), "Invalid bundle for a bundled task");

            // Set up completion callback for a non-bundled task or for the last task of a bundle
            if (!exec_cons.is_bundled() || exec_cons.is_last_in_bundle())
            {
                auto cc       = exec_cons.is_bundled() ? &task_bundle_completion_callback : &completion_callback;
                auto userdata = exec_cons.is_bundled() ? static_cast<void*>(tbd) : gputask;

                cl_completion_event.get_impl().setCallback(CL_COMPLETE, cc, userdata);
            }
        }
        HETCOMPUTE_GCC_IGNORE_END("-Wshadow");
#endif // HETCOMPUTE_HAVE_OPENCL

#ifdef HETCOMPUTE_HAVE_GLES
        template <typename GLKernel, typename Range, typename GLFence>
        void
        launch_gl_kernel(executor_construct const& exec_cons, GLKernel* gl_kernel, Range& global_range, Range& local_range, GLFence& gl_fence)
        {
            HETCOMPUTE_INTERNAL_ASSERT(gl_kernel != nullptr, "Invalid gl_kernel");
            HETCOMPUTE_API_ASSERT(!exec_cons.is_bundled(), "GL dispatch does not as yet support bundle dispatch");

            gl_fence = gl_kernel->launch(global_range, local_range);

            // fence object is signalled from GL runtimei.
            GLenum error = glClientWaitSync(gl_fence, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
            if (GL_TIMEOUT_EXPIRED == error)
            {
                HETCOMPUTE_DLOG("GL_TIMEOUT_EXPIRED");
            }
            else if ((GL_ALREADY_SIGNALED == error) || (GL_CONDITION_SATISFIED == error))
            {
                if (exec_cons.is_blocking())
                {
                    // nothing to do, already blocked on kernel completion
                }
                else
                {
                    HETCOMPUTE_DLOG("--- GL stub task ---");
                    auto gputask = exec_cons.get_requestor_task();
                    gputask->on_finish();
                    gputask->finish(false, nullptr, true, true);
                }
            }
            else if (GL_WAIT_FAILED == error)
            {
                HETCOMPUTE_DLOG("GL_WAIT_FAILED");
            }
        }
#endif // HETCOMPUTE_HAVE_GLES

        /**
         *  Supports blocking or async invocation of a GPU kernel.
         *
         *  @tparam Dims       Dimension of global and local ranges (Dims = 1,2 or 3)
         *  @tparam Params     A tuple of the argument-types used to declare the GPU kernel
         *  @tparam Args       A tuple of the argument-types passed to execute the kernel (excluding the ranges)
         *  @tparam KernePtr   Internal kernel pointer (could CL, GL, etc.)
         *  @tparam NormalArgs A tuple to hold the normal args passed to the kernel, with "char" used as a
         *                     placeholder when the corresponding arg in Args is not a normal arg. Useful for
         *                     automatic type conversion from the corresponding arg in Args to the corresponding
         *                     requirement param in Params.
         *  @tparam BAS        The types of the buffer-acquire-set to be used
         *  @tparam PreacquiredArenas
         *                     The data structure used to hold the preacquired arenas for buffers in the
         *                     buffer-acquire-set
         *
         *  @param tbd         == nullptr => no bundle dispatch, execute gt normally.
         *                     Otherwise, gives the bundle containing gt, so gt may be launched as part of the tbd
         *  @param k_ptr       The kernel being launched (CL, GL, etc)
         *  @param gt          != nullptr => a task is being launched.
         *                     == nullptr => execution without a task (e.g., a blocking invocation of a GPU kernel)
         *  @param gli         Low-level device-specific properties needed to launch the kernel
         *  @param device      The GPU device to launch the kernel on
         *  @param kernel_args Tuple of args passed to launch the kernel (excluding the range arguments)
         *  @param normal_args Tuple with entries corresponding to kernel_args, where an entry is either of the
         *                     normal arg type in the corresponding entry of Params, or is char as a dummy placeholder
         *                     when the corresponding entry of Params is not a normal arg
         *  @param bas         The buffer-acquire-set to populate and/or dispatch from.
         *  @param p_preacquired_arenas
         *                     If != nullptr, provides pre-acquired arenas to use for specific buffers in bas,
         *                     instead of having the bas request the buffer to provide an arena for the GPU to access.
         *  @param first_execution
         *                     State to track whether this is the first attempt to execute a GPU construct (task/blocking).
         *                     On subsequent attempts some steps, such as the addition of buffers to bas, are skipped.
         *                     Must be =true on the first attempt to execute the GPU construct. Will be set to false
         *                     if the attempt failed. On subsequent attempts, must be =false.
         *
         */
        template <size_t Dims, typename Params, typename Args, typename KernelPtr, typename NormalArgs, typename BAS, typename PreacquiredArenas>
        bool gpu_do_execute(KernelPtr                                       k_ptr,
                            executor_construct const&                       exec_cons,
                            gpu_launch_info&                                gli,
                            ::hetcompute::internal::legacy::device_ptr const& device,
                            ::hetcompute::range<Dims> const&                  global_range,
                            ::hetcompute::range<Dims> const&                  local_range,
                            Args&&                                          kernel_args,
                            NormalArgs&                                     normal_args,
                            BAS&                                            bas,
                            PreacquiredArenas const*                        p_preacquired_arenas,
                            bool                                            add_buffers,
                            bool                                            perform_launch)
        {
            HETCOMPUTE_INTERNAL_ASSERT(exec_cons.get_requestor() != nullptr, "Invalid requestor in executor construct");

#ifdef HETCOMPUTE_HAVE_OPENCL

#ifdef HETCOMPUTE_HAVE_GLES
            char const* kernel_type = (k_ptr->is_cl() ? "OpenCL" : k_ptr->is_gl() ? "OpenGLES" : "INVALID");
#else  // HETCOMPUTE_HAVE_GLES
            char const* kernel_type = "OpenCL";
#endif // HETCOMPUTE_HAVE_GLES

#else  // HETCOMPUTE_HAVE_OPENCL
            char const* kernel_type = "OpenGLES";
#endif // HETCOMPUTE_HAVE_OPENCL
            HETCOMPUTE_UNUSED(kernel_type);

            char last_in_bundle = exec_cons.is_last_in_bundle() ? 'Y' : 'N';
            HETCOMPUTE_UNUSED(last_in_bundle);

            if (exec_cons.is_blocking())
            {
                HETCOMPUTE_DLOG("executing %s executor id %p bundled=%c last_in_bundle=%c",
                              kernel_type,
                              exec_cons.get_requestor(),
                              exec_cons.is_bundled() ? 'Y' : 'N',
                              last_in_bundle);
            }
            else
            { // a task
                if (exec_cons.is_bundled())
                {
                    HETCOMPUTE_DLOG("enqueuing %s task %p for bundle %p last_in_bundle=%c",
                                  kernel_type,
                                  exec_cons.get_requestor_task(),
                                  exec_cons.get_task_bundle(),
                                  last_in_bundle);
                }
                else
                {
                    HETCOMPUTE_DLOG("enqueuing %s task %p", kernel_type, exec_cons.get_requestor_task());
                }
            }

#ifdef HETCOMPUTE_HAVE_OPENCL
            HETCOMPUTE_INTERNAL_ASSERT((gli._d_ptr != nullptr), "null device");
            if (k_ptr->is_cl())
            {
                HETCOMPUTE_DLOG("cl kernel: %p", k_ptr->get_cl_kernel());
            }
#endif // HETCOMPUTE_HAVE_OPENCL
#ifdef HETCOMPUTE_HAVE_GLES
            if (k_ptr->is_gl())
            {
                HETCOMPUTE_DLOG("gl kernel: %p", k_ptr->get_gl_kernel());
            }
#endif // HETCOMPUTE_HAVE_GLES

            // Create a critical section w.r.t. launch of the same kernel from
            // a concurrent thread.
            // Ensures that all the kernel arguments and the kernel itself are
            // enqueued atomically to the OpenCL command queue in the presence of
            // a concurrent thread attempting to launch the same kernel.
            std::unique_lock<std::mutex> ul(k_ptr->access_dispatch_mutex(), std::defer_lock);
            if (perform_launch) // do not need lock on kernel if only adding buffers to bas
            {
                ul.lock();
            }

            void const* requestor = exec_cons.get_requestor();
            bool        conflict  = false;
            if (!exec_cons.is_bundled())
            {
                conflict = !gpu_kernel_dispatch::prepare_args<Params, Args>(device,
                                                                            k_ptr,
                                                                            std::forward<Args>(kernel_args),
                                                                            normal_args,
                                                                            bas,
                                                                            p_preacquired_arenas,
                                                                            requestor,
                                                                            add_buffers,
                                                                            perform_launch,  // acquire buffers
                                                                            perform_launch); // dispatch args
            }
            else if (exec_cons.is_blocking())
            { // blocking executor bundle
                preacquired_arenas_base* paa = nullptr;
                conflict =
                    !gpu_kernel_dispatch::prepare_args<Params, Args>(device,
                                                                     k_ptr,
                                                                     std::forward<Args>(kernel_args),
                                                                     normal_args,
                                                                     bas,
                                                                     paa,
                                                                     requestor,
                                                                     add_buffers,
                                                                     false, // buffers would be acquired outside for the entire bundle
                                                                     perform_launch);
                HETCOMPUTE_INTERNAL_ASSERT(!conflict, "No conflict should be found as no buffer acquires would be attempted.");
            }
            else
            { // a task bundle
                HETCOMPUTE_INTERNAL_ASSERT(!add_buffers, "Re-attemping add buffers: Task bundle dispatch would have already added buffers.");
                HETCOMPUTE_INTERNAL_ASSERT(exec_cons.get_task_bundle()->get_buffer_acquire_set().acquired(),
                                         "Task bundle dispatch should have already acquired buffers.");
                auto tbd = exec_cons.get_task_bundle();
                conflict = !gpu_kernel_dispatch::prepare_args<Params, Args>(device,
                                                                            k_ptr,
                                                                            std::forward<Args>(kernel_args),
                                                                            normal_args,
                                                                            tbd->get_buffer_acquire_set(),
                                                                            &(tbd->get_preacquired_arenas()),
                                                                            requestor,
                                                                            false,           // buffers already added by task bundle
                                                                            false,           // buffers already acquired by task bundle
                                                                            perform_launch); // dispatch args
                HETCOMPUTE_INTERNAL_ASSERT(!conflict, "No conflict should be found as no buffer acquires would be attempted.");
            }

            if (conflict)
            {
                return false;
            }

            if (!perform_launch)
            {
                return true;
            }

            // Launch kernel via underlying runtime API
            if (0)
            {
            }
#ifdef HETCOMPUTE_HAVE_OPENCL
            else if (k_ptr->is_cl())
            {
                launch_cl_kernel(gli._d_ptr, exec_cons, k_ptr->get_cl_kernel(), global_range, local_range, gli._cl_completion_event);

                // Perform completion synchronization for an unbundled blocking executor
                // or for the last blocking executor in a bundle.
                if (exec_cons.is_blocking())
                {
                    if (!exec_cons.is_bundled() || exec_cons.is_last_in_bundle())
                    {
                        bas.release_buffers(requestor);
                        gli._d_ptr->get_cmd_queue().finish();
                    }
                }
            }
#endif // HETCOMPUTE_HAVE_OPENCL
#ifdef HETCOMPUTE_HAVE_GLES
            else if (k_ptr->is_gl())
            {
                launch_gl_kernel(exec_cons, k_ptr->get_gl_kernel(), global_range, local_range,
                                 gli._gl_fence); // always blocking
            }
#endif // HETCOMPUTE_HAVE_GLES
            else
            {
                HETCOMPUTE_FATAL("Unsupported type of GPU kernel");
            }

            return true;
        }

        template <size_t Dims, typename Fn, typename Params, typename Args>
        bool gputask<Dims, Fn, Params, Args>::do_execute(task_bundle_dispatch* tbd)
        {
            auto k_ptr = internal::c_ptr(_kernel_ptr);

            HETCOMPUTE_INTERNAL_ASSERT((k_ptr != nullptr), "null kernel");

            HETCOMPUTE_INTERNAL_ASSERT(get_snapshot().is_running(), "Can't execute task %p: %s", this, to_string().c_str());

            auto is_bundle_dispatch = tbd != nullptr;
            bool launched =
                gpu_do_execute<Dims, Params, Args>(k_ptr,
                                                   executor_construct(this, tbd),
                                                   _gli,
                                                   _device,
                                                   _global_range,
                                                   _local_range,
                                                   std::forward<Args>(_kernel_args),
                                                   _normal_args_container._tp,
                                                   _buffer_acquire_set,
                                                   &_preacquired_arenas,
                                                   !is_bundle_dispatch && _first_execution, // acquire buffers on first execution attempt
                                                   true);                                   // perform launch
            _first_execution = false;

            if (!launched)
            {
                return false;
            }

            // mark the gpu task as having executed; specifically, as having been sent
            // to the OpenCL or OpenGLES runtime.
            auto guard = get_lock_guard();
            _executed  = true;

            return true;
        }

        template <size_t Dims, typename Fn, typename Params, typename Args>
        void gputask<Dims, Fn, Params, Args>::release_arguments()
        {
            HETCOMPUTE_DLOG("release_arguments");
            _buffer_acquire_set.release_buffers(this);
        }

    }; // namespace internal
};     // namespace hetcompute

#endif // HETCOMPUTE_HAVE_GPU
