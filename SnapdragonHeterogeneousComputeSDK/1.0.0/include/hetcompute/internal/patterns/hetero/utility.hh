#pragma once

#include <tuple>
#include <hetcompute/internal/task/functiontraits.hh>
#include <hetcompute/internal/util/templatemagic.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace pattern
        {
            namespace utility
            {
                constexpr size_t invalid_pos = std::numeric_limits<size_t>::max();

                /**
                 * Kernel type check
                 * Same as is_taskable in taskfactory.hh
                 */

                template <typename F, class Enable = void>
                struct is_cpu_kernel : std::integral_constant<bool, false>
                {
                };

                template <typename F>
                struct is_cpu_kernel<F, typename std::enable_if<!std::is_same<typename function_traits<F>::kind, user_code_types::invalid>::value>::type>
                    : std::integral_constant<bool, true>
                {
                };

                template <typename F>
                struct is_cpu_kernel<::hetcompute::cpu_kernel<F>, void> : std::integral_constant<bool, true>
                {
                };

                template <typename F>
                struct is_hetcompute_cpu_kernel : std::integral_constant<bool, false>
                {
                };

                template <typename F>
                struct is_hetcompute_cpu_kernel<::hetcompute::cpu_kernel<F>> : std::integral_constant<bool, true>
                {
                };

                template <typename F, class Enable = void>
                struct is_gpu_kernel : std::integral_constant<bool, false>
                {
                };

                template <typename... KernelArgs>
                struct is_gpu_kernel<::hetcompute::gpu_kernel<KernelArgs...>, void> : std::integral_constant<bool, true>
                {
                };

                template <typename F, class Enable = void>
                struct is_dsp_kernel : std::integral_constant<bool, false>
                {
                };

                template <typename F>
                struct is_dsp_kernel<::hetcompute::dsp_kernel<F>, void> : std::integral_constant<bool, true>
                {
                };

                /**
                 * Search cpu/gpu/dsp kernel in given tuple, and return pos in tuple
                 * If pos is -1, then no kernel of corresponding type is found
                 * If there are more than one kernel with the same time, the implementation
                 * will yield a compilation error with error message
                 */

                // Returns cpu kernel position in the given tuple
                template <size_t pos, typename Tuple>
                struct search_cpu_kernel
                {
                    using prior = search_cpu_kernel<pos - 1, Tuple>;
                    template <size_t Pos>
                    using ktype = typename std::tuple_element<Pos, Tuple>::type;

                    static HETCOMPUTE_CONSTEXPR_CONST size_t prior_cpu_pos   = prior::cpu_pos;
                    static HETCOMPUTE_CONSTEXPR_CONST size_t current_cpu_pos = is_cpu_kernel<ktype<pos>>::value ? pos : invalid_pos;

                    static_assert(prior::cpu_pos == invalid_pos || current_cpu_pos == invalid_pos, "Multiple cpu kernel defined!");

                    static HETCOMPUTE_CONSTEXPR_CONST size_t cpu_pos = prior_cpu_pos != invalid_pos ? prior_cpu_pos : current_cpu_pos;
                };

                template <typename Tuple>
                struct search_cpu_kernel<invalid_pos, Tuple>
                {
                    static HETCOMPUTE_CONSTEXPR_CONST size_t cpu_pos = invalid_pos;
                };

                template <typename Tuple>
                struct cpu_kernel_pos
                {
                    using result = search_cpu_kernel<std::tuple_size<Tuple>::value - 1, Tuple>;

                    static HETCOMPUTE_CONSTEXPR_CONST size_t pos = result::cpu_pos;
                };

                // Returns gpu kernel position in the given tuple
                template <size_t pos, typename Tuple>
                struct search_gpu_kernel
                {
                    using prior = search_gpu_kernel<pos - 1, Tuple>;
                    template <size_t Pos>
                    using ktype = typename std::tuple_element<Pos, Tuple>::type;

                    static HETCOMPUTE_CONSTEXPR_CONST size_t prior_gpu_pos   = prior::gpu_pos;
                    static HETCOMPUTE_CONSTEXPR_CONST size_t current_gpu_pos = is_gpu_kernel<ktype<pos>>::value ? pos : invalid_pos;

                    static_assert(prior::gpu_pos == invalid_pos || current_gpu_pos == invalid_pos, "Multiple gpu kernel defined!");

                    static HETCOMPUTE_CONSTEXPR_CONST size_t gpu_pos = prior_gpu_pos != invalid_pos ? prior_gpu_pos : current_gpu_pos;
                };

                template <typename Tuple>
                struct search_gpu_kernel<invalid_pos, Tuple>
                {
                    static HETCOMPUTE_CONSTEXPR_CONST size_t gpu_pos = invalid_pos;
                };

                template <typename Tuple>
                struct gpu_kernel_pos
                {
                    using result = search_gpu_kernel<std::tuple_size<Tuple>::value - 1, Tuple>;

                    static HETCOMPUTE_CONSTEXPR_CONST size_t pos = result::gpu_pos;
                };


                // Returns dsp kernel position in the given tuple
                template <size_t pos, typename Tuple>
                struct search_hex_kernel
                {
                    using prior = search_hex_kernel<pos - 1, Tuple>;
                    template <size_t Pos>
                    using ktype = typename std::tuple_element<Pos, Tuple>::type;

                    static HETCOMPUTE_CONSTEXPR_CONST size_t prior_hex_pos   = prior::hex_pos;
                    static HETCOMPUTE_CONSTEXPR_CONST size_t current_hex_pos = is_dsp_kernel<ktype<pos>>::value ? pos : invalid_pos;

                    static_assert(prior::hex_pos == invalid_pos || current_hex_pos == invalid_pos, "Multiple dsp kernel defined!");

                    static HETCOMPUTE_CONSTEXPR_CONST size_t hex_pos = prior_hex_pos != invalid_pos ? prior_hex_pos : current_hex_pos;
                };

                template <typename Tuple>
                struct search_hex_kernel<invalid_pos, Tuple>
                {
                    static HETCOMPUTE_CONSTEXPR_CONST size_t hex_pos = invalid_pos;
                };

                template <typename Tuple>
                struct dsp_kernel_pos
                {
                    using result = search_hex_kernel<std::tuple_size<Tuple>::value - 1, Tuple>;

                    static HETCOMPUTE_CONSTEXPR_CONST size_t pos = result::hex_pos;
                };

                /**
                 * Buffer type check
                 * input: tuple<Args...>, check the following conditions
                 *  - all buffers have equal size
                 *  - multiple output buffers are not supported
                 */

                template <typename T>
                struct is_input_buffer_ptr;

                template <typename T>
                struct is_input_buffer_ptr : public std::false_type
                {
                };

                template <typename T>
                struct is_input_buffer_ptr<::hetcompute::buffer_ptr<const T>> : public std::true_type
                {
                };

                template <typename T>
                struct is_input_buffer_ptr<::hetcompute::in<::hetcompute::buffer_ptr<T>>> : public std::true_type
                {
                };

                template <typename T>
                struct is_output_buffer_ptr;

                template <typename T>
                struct is_output_buffer_ptr : public std::false_type
                {
                };

                template <typename T>
                struct is_output_buffer_ptr<::hetcompute::buffer_ptr<const T>> : public std::false_type
                {
                };

                template <typename T>
                struct is_output_buffer_ptr<::hetcompute::buffer_ptr<T>> : public std::true_type
                {
                };

                template <typename T>
                struct is_output_buffer_ptr<::hetcompute::out<::hetcompute::buffer_ptr<T>>> : public std::true_type
                {
                };

                template <typename T>
                struct is_output_buffer_ptr<::hetcompute::inout<::hetcompute::buffer_ptr<T>>> : public std::true_type
                {
                };

                template <typename Tuple, size_t index>
                struct num_input_buffer_helper
                {
                    enum
                    {
                        value = (is_input_buffer_ptr<typename std::decay<typename std::tuple_element<index - 1, Tuple>::type>::type>::value ?
                                     1 :
                                     0) +
                                num_input_buffer_helper<Tuple, index - 1>::value
                    };
                };

                template <typename Tuple>
                struct num_input_buffer_helper<Tuple, 0>
                {
                    enum
                    {
                        value = 0
                    };
                };

                template <typename Tuple>
                struct num_input_buffer_in_tuple
                {
                    enum
                    {
                        value = num_input_buffer_helper<Tuple, std::tuple_size<Tuple>::value>::value
                    };
                };

                template <typename Tuple, size_t index>
                struct num_output_buffer_helper
                {
                    enum
                    {
                        value = (is_output_buffer_ptr<typename std::decay<typename std::tuple_element<index - 1, Tuple>::type>::type>::value ?
                                     1 :
                                     0) +
                                num_output_buffer_helper<Tuple, index - 1>::value
                    };
                };

                template <typename Tuple>
                struct num_output_buffer_helper<Tuple, 0>
                {
                    enum
                    {
                        value = 0
                    };
                };

                template <typename Tuple>
                struct num_output_buffer_in_tuple
                {
                    enum
                    {
                        value = num_output_buffer_helper<Tuple, std::tuple_size<Tuple>::value>::value
                    };
                };

                template <typename Tuple>
                struct get_num_buffer
                {
                    enum
                    {
                        value = num_output_buffer_in_tuple<Tuple>::value + num_input_buffer_in_tuple<Tuple>::value
                    };
                };

                template <typename Tuple, size_t index>
                int get_buffer_size(
                    Tuple& t,
                    typename std::enable_if<!is_input_buffer_ptr<typename std::tuple_element<index - 1, Tuple>::type>::value &&
                                            !is_output_buffer_ptr<typename std::tuple_element<index - 1, Tuple>::type>::value>::type* = nullptr)
                {
                    HETCOMPUTE_UNUSED(t);
                    return -1;
                };

                template <typename Tuple, size_t index>
                int get_buffer_size(
                    Tuple& t,
                    typename std::enable_if<is_input_buffer_ptr<typename std::tuple_element<index - 1, Tuple>::type>::value ||
                                            is_output_buffer_ptr<typename std::tuple_element<index - 1, Tuple>::type>::value>::type* = nullptr)
                {
                    return std::get<index - 1>(t).size();
                };

                template <size_t n, typename Tuple>
                struct buffer_size_check_helper
                {
                    static int check_size(Tuple& t)
                    {
                        int sz      = get_buffer_size<Tuple, n>(t);
                        int prev_sz = buffer_size_check_helper<n - 1, Tuple>::check_size(t);
                        HETCOMPUTE_API_THROW(sz == -1 || prev_sz == -1 || sz == prev_sz, "Buffer size mismatch!");
                        return prev_sz;
                    }
                };

                template <typename Tuple>
                struct buffer_size_check_helper<1, Tuple>
                {
                    static int check_size(Tuple& t)
                    {
                        int sz = get_buffer_size<Tuple, 1>(t);
                        return sz;
                    }
                };

                template <typename Tuple>
                void buffer_size_check(Tuple& t)
                {
                    buffer_size_check_helper<std::tuple_size<Tuple>::value, Tuple>::check_size(t);
                }

                // Extract variadic arg unpacked from a tuple
                template <size_t... Indices, typename Fn, typename Tuple>
                void tuple_unpack(Fn&& fn, size_t first, size_t stride, size_t last, Tuple&& t, integer_list_gen<Indices...>)
                {
                    fn(first, stride, last, std::get<Indices - 1>(std::forward<Tuple>(t))...);
                }

                template <size_t... Indices, typename Fn, typename Tuple>
                void tuple_unpack_dsp(hetcompute::group_ptr& g,
                                      Fn&&                 fn,
                                      size_t               first,
                                      size_t               stride,
                                      size_t               last,
                                      Tuple&&              t,
                                      integer_list_gen<Indices...>)
                {
                    g->launch(fn, first, stride, last, std::get<Indices - 1>(std::forward<Tuple>(t))...);
                }

                // Wrapper func to call tuple extracted parameters
                template <typename Fn, typename... Args>
                void fn_call_wrapper(Fn&& fn, size_t first, size_t stride, size_t last, std::tuple<Args...>& t)
                {
                    tuple_unpack(std::forward<Fn>(fn), first, stride, last, t, typename integer_list<sizeof...(Args)>::type());
                }

                // Find and replace output buffer parameter in arg pack
                template <typename ArgType, typename T>
                ArgType arg_transform(
                    ArgType&&                arg,
                    hetcompute::buffer_ptr<T>& out_buf,
                    typename std::enable_if<!is_output_buffer_ptr<typename std::remove_reference<ArgType>::type>::value>::type* = nullptr)
                {
                    HETCOMPUTE_UNUSED(out_buf);
                    return std::forward<ArgType>(arg);
                }

                template <typename ArgType, typename T>
                hetcompute::buffer_ptr<T> arg_transform(
                    ArgType&&                arg,
                    hetcompute::buffer_ptr<T>& out_buf,
                    typename std::enable_if<is_output_buffer_ptr<typename std::remove_reference<ArgType>::type>::value>::type* = nullptr)
                {
                    HETCOMPUTE_UNUSED(arg);
                    return out_buf;
                }

                // Extract index of output buffer from tuple
                template <size_t index, typename Tuple>
                struct extract_output_buf
                {
                    static constexpr size_t value =
                        is_output_buffer_ptr<typename std::decay<typename std::tuple_element<index - 1, Tuple>::type>::type>::value ?
                            index - 1 :
                            extract_output_buf<index - 1, Tuple>::value;
                };

                template <typename Tuple>
                struct extract_output_buf<0, Tuple>
                {
                    static constexpr size_t value = invalid_pos;
                };

                // find output buffer_ptr in arg_list and replace it with local buffer ptr
                // DEPRECATED: since std::get<> returns a reference, one can directly set tuple
                // element given tuple is non-const.
                template <typename bufptr_type, typename Tuple>
                Tuple search_and_replace(Tuple& param_tuple, bufptr_type bptr)
                {
                    HETCOMPUTE_CONSTEXPR_CONST size_t tuple_size = std::tuple_size<Tuple>::value;
                    HETCOMPUTE_CONSTEXPR_CONST size_t pos        = extract_output_buf<tuple_size, Tuple>::value;

                    auto param_tuple_head = ::hetcompute::internal::tuple_head(param_tuple, typename integer_list<pos>::type());
                    auto param_tuple_tail =
                        ::hetcompute::internal::tuple_subset(param_tuple, typename integer_list_offset<tuple_size - pos - 1, pos>::type());

                    return std::tuple_cat(param_tuple_head, std::forward_as_tuple(bptr), param_tuple_tail);
                }

                // append argument list with additional

                // Conditional add buffer pointer encountered in args
                template <typename T, typename BAS>
                void bas_add(BAS& bas, T& ptr, typename std::enable_if<is_input_buffer_ptr<T>::value>::type* = nullptr)
                {
                    bas.add(ptr, bufferpolicy::action_t::acquire_r);
                }

                template <typename T, typename BAS>
                void bas_add(BAS& bas, T& ptr, typename std::enable_if<is_output_buffer_ptr<T>::value>::type* = nullptr)
                {
                    bas.add(ptr, bufferpolicy::action_t::acquire_rw);
                }

                template <typename T, typename BAS>
                void bas_add(BAS&,
                             T&,
                             typename std::enable_if<!is_input_buffer_ptr<T>::value && !is_output_buffer_ptr<T>::value>::type* = nullptr)
                {
                }

                // Iterate over args and add buffer_ptr to buffer_acquire_set if necessary
                template <size_t pos, size_t N, typename Tuple, typename BAS>
                struct add_buffer_in_args
                {
                    static void add_buffer(BAS& bas, Tuple& tp)
                    {
                        bas_add(bas, std::get<pos - 1>(tp));
                        add_buffer_in_args<pos - 1, N, Tuple, BAS>::add_buffer(bas, tp);
                    }
                };

                template <size_t N, typename Tuple, typename BAS>
                struct add_buffer_in_args<0, N, Tuple, BAS>
                {
                    static void add_buffer(BAS&, Tuple&) {}
                };

                // Get data type for buffer pointer
                template <typename Buffer>
                struct buffer_data_type
                {
                };

                template <typename T>
                struct buffer_data_type<hetcompute::buffer_ptr<T>>
                {
                    using data_type = T;
                };

// Conditional process input buffer encountered in args for gpu tasks
#if defined(HETCOMPUTE_HAVE_OPENCL)
                template <typename T>
                void add_cl_arena(hetcompute::internal::task* gpu_task,
                                  T&                        arg,
                                  typename std::enable_if<is_input_buffer_ptr<T>::value>::type* = nullptr)
                {
                    auto& arg_base_ptr = reinterpret_cast<hetcompute::internal::buffer_ptr_base&>(arg);
                    auto  arg_bs       = hetcompute::internal::c_ptr(hetcompute::internal::buffer_accessor::get_bufstate(arg_base_ptr));

#ifdef HETCOMPUTE_CL_TO_CL2 // use CL2 arena (SVM) for OpenCL
                    auto gpu_arena = arg_bs->get(hetcompute::internal::CL2_ARENA);
#else
                    auto gpu_arena = arg_bs->get(hetcompute::internal::CL_ARENA);
#endif // HETCOMPUTE_CL_TO_CL2

                    HETCOMPUTE_INTERNAL_ASSERT(gpu_arena != nullptr, "Unexpected nullptr for gpu arena!");

#ifdef HETCOMPUTE_CL_TO_CL2
                    hetcompute::internal::arena_storage_accessor::access_cl2_arena_for_gputask(gpu_arena);
#else
                    hetcompute::internal::arena_storage_accessor::access_cl_arena_for_gputask(gpu_arena);
#endif

                    // Register input buffers for hetero task.
                    // Since thread safety is not necessary for accessing input buffers,
                    // we can save a pair of acquire/release within the hetero task.
                    gpu_task->unsafe_register_preacquired_arena(arg_bs, gpu_arena);
                }

                template <typename T>
                void add_cl_arena(hetcompute::internal::task*, T&, typename std::enable_if<!is_input_buffer_ptr<T>::value>::type* = nullptr)
                {
                }

                // Iterate over args and add input buffer index position to input_idx bitset
                template <size_t pos, typename Tuple>
                struct extract_input_arenas_for_gpu
                {
                    static void preacquire_input_arenas(hetcompute::internal::task* gpu_task, Tuple& tp)
                    {
                        add_cl_arena(gpu_task, std::get<pos - 1>(tp));
                        extract_input_arenas_for_gpu<pos - 1, Tuple>::preacquire_input_arenas(gpu_task, tp);
                    }
                };

                template <typename Tuple>
                struct extract_input_arenas_for_gpu<0, Tuple>
                {
                    static void preacquire_input_arenas(hetcompute::internal::task*, Tuple&) {}
                };
#endif // defined(HETCOMPUTE_HAVE_OPENCL)

#if defined(HETCOMPUTE_HAVE_QTI_DSP)
                // Conditional process input buffer encountered in args for dsp tasks
                template <typename T>
                void add_ion_arena(hetcompute::internal::task* dsp_task,
                                   T&                        arg,
                                   typename std::enable_if<is_input_buffer_ptr<T>::value>::type* = nullptr)
                {
                    auto& arg_base_ptr = reinterpret_cast<hetcompute::internal::buffer_ptr_base&>(arg);
                    auto  arg_bs       = hetcompute::internal::c_ptr(hetcompute::internal::buffer_accessor::get_bufstate(arg_base_ptr));

                    auto dsp_arena = arg_bs->get(hetcompute::internal::ION_ARENA);
                    HETCOMPUTE_INTERNAL_ASSERT(dsp_arena != nullptr, "Unexpected nullptr for ion arena!");
                    hetcompute::internal::arena_storage_accessor::access_ion_arena_for_dsptask(dsp_arena);

                    // Register input buffers for hetero task.
                    // Since thread safety is not necessary for accessing input buffers,
                    // we can save a pair of acquire/release within the hetero task.
                    dsp_task->unsafe_register_preacquired_arena(arg_bs, dsp_arena);
                }

                template <typename T>
                void add_ion_arena(hetcompute::internal::task*, T&, typename std::enable_if<!is_input_buffer_ptr<T>::value>::type* = nullptr)
                {
                }

                // Iterate over args and add input buffer index position to input_idx bitset
                template <size_t pos, typename Tuple>
                struct extract_input_arenas_for_dsp
                {
                    static void preacquire_input_arenas(hetcompute::internal::task* dsp_task, Tuple& tp)
                    {
                        add_ion_arena(dsp_task, std::get<pos - 1>(tp));
                        extract_input_arenas_for_dsp<pos - 1, Tuple>::preacquire_input_arenas(dsp_task, tp);
                    }
                };

                template <typename Tuple>
                struct extract_input_arenas_for_dsp<0, Tuple>
                {
                    static void preacquire_input_arenas(hetcompute::internal::task*, Tuple&) {}
                };
#endif // defined(HETCOMPUTE_HAVE_QTI_DSP)

            };  // namespac utility
        };  // namespace pattern
    };  // namespace internal
};  // namespace hetcompute
