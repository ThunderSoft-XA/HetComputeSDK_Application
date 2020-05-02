#pragma once

#include <string>

namespace hetcompute
{
    namespace internal
    {
        namespace pointkernel
        {
            template <typename... L>
            struct arg_list_gen
            {
                template <typename M, typename... N>
                struct append
                {
                    typedef arg_list_gen<M, N..., L...> type;
                };
            };

            template <typename T>
            struct is_range : std::integral_constant<bool, false>
            {
            };

            template <>
            struct is_range<hetcompute::range<1>> : std::integral_constant<bool, true>
            {
            };

            template <>
            struct is_range<hetcompute::range<2>> : std::integral_constant<bool, true>
            {
            };

            template <>
            struct is_range<hetcompute::range<3>> : std::integral_constant<bool, true>
            {
            };

            // Struct to generate a list of arguments corresponding to the input parameters to the cpu kernel.
            struct cpu_arg_list
            {
                typedef arg_list_gen<> type;
            };

            // Expand hetcompute::range<> into (hetcompute::range<>, hetcompute::pattern::tuner) for cpu kernel
            template <bool isRange, typename... Args>
            struct cpu_expand_range;

            template <typename... Args>
            struct cpu_expand_range<false, Args...>
            {
                typedef typename cpu_arg_list::type::template append<Args...>::type type;
            };

            template <typename... Args>
            struct cpu_expand_range<true, hetcompute::range<1>, Args...>
            {
                typedef typename cpu_arg_list::type::template append<hetcompute::range<1>, hetcompute::pattern::tuner, Args...>::type type;
            };

            template <typename... Args>
            struct cpu_expand_range<true, hetcompute::range<2>, Args...>
            {
                typedef typename cpu_arg_list::type::template append<hetcompute::range<2>, hetcompute::pattern::tuner, Args...>::type type;
            };

            template <typename... Args>
            struct cpu_expand_range<true, hetcompute::range<3>, Args...>
            {
                typedef typename cpu_arg_list::type::template append<hetcompute::range<3>, hetcompute::pattern::tuner, Args...>::type type;
            };

            template <typename F, typename... Args>
            struct make_cpu_kernel_args
            {
                typedef typename cpu_expand_range<is_range<F>::value, F, Args...>::type type;
            };

            // Generate the cpu kernel type by stripping out the variadic argument list, and directly passing it to hetcompute::cpu_kernel.
            template <typename... Args>
            struct cpu_kernel_generator;

            template <typename ReturnType, typename... Args>
            struct cpu_kernel_generator<ReturnType, arg_list_gen<Args...>>
            {
                typedef hetcompute::cpu_kernel<ReturnType(Args...)> type;
            };

            // Generate the cpu kernel signature type
            template <typename... Args>
            struct cpu_kernel_signature_generator;

            template <typename ReturnType, typename... Args>
            struct cpu_kernel_signature_generator<ReturnType, arg_list_gen<Args...>>
            {
                typedef ReturnType (*type)(Args...);
            };

            // Struct to generate a list of arguments corresponding to the input parameters to the gpu kernel.
            template <bool is_pointer, bool is_integer, typename... Args>
            struct gpu_arg_list;

            template <>
            struct gpu_arg_list<false, false>
            {
                typedef arg_list_gen<> type;
            };

            template <typename First>
            struct gpu_arg_list<false, false, First>
            {
                typedef typename gpu_arg_list<false, false>::type::template append<First>::type type;
            };

            template <typename First, typename Second>
            struct gpu_arg_list<true, true, First, Second>
            {
                typedef typename gpu_arg_list<false, false>::type::
                    template append<typename hetcompute::buffer_ptr<typename std::remove_pointer<First>::type>, Second>::type type;
            };

            template <typename First, typename Second>
            struct gpu_arg_list<true, false, First, Second>
            {
                typedef typename gpu_arg_list<false, false>::type::template append<First, Second>::type type;
            };

            template <typename First, typename Second>
            struct gpu_arg_list<false, true, First, Second>
            {
                typedef typename gpu_arg_list<false, false>::type::template append<First, Second>::type type;
            };

            template <typename First, typename Second>
            struct gpu_arg_list<false, false, First, Second>
            {
                typedef typename gpu_arg_list<false, false>::type::template append<First, Second>::type type;
            };

            template <typename First, typename Second, typename Third, typename... Args>
            struct gpu_arg_list<false, false, First, Second, Third, Args...>
            {
                typedef typename gpu_arg_list<std::is_pointer<Second>::value, std::is_integral<Third>::value, Second, Third, Args...>::
                    type::template append<First>::type type;
            };

            template <typename First, typename Second, typename Third, typename... Args>
            struct gpu_arg_list<true, false, First, Second, Third, Args...>
            {
                typedef typename gpu_arg_list<std::is_pointer<Second>::value, std::is_integral<Third>::value, Second, Third, Args...>::
                    type::template append<First>::type type;
            };

            template <typename First, typename Second, typename Third, typename... Args>
            struct gpu_arg_list<false, true, First, Second, Third, Args...>
            {
                typedef typename gpu_arg_list<std::is_pointer<Second>::value, std::is_integral<Third>::value, Second, Third, Args...>::
                    type::template append<First>::type type;
            };

            template <typename First, typename Second, typename Third, typename... Args>
            struct gpu_arg_list<true, true, First, Second, Third, Args...>
            {
                typedef typename gpu_arg_list<std::is_pointer<Second>::value, std::is_integral<Third>::value, Second, Third, Args...>::
                    type::template append<typename hetcompute::buffer_ptr<typename std::remove_pointer<First>::type>>::type type;
            };

            // Discard hetcompute::range<> for gpu kernel
            template <bool isRange, typename... Args>
            struct gpu_discard_range;

            // Catch for when we have < 2 input arguments and one of them is the range
            // Here we only have the range, and no other arguments to the kernel
            template <typename Range>
            struct gpu_discard_range<true, Range>
            {
                typedef typename gpu_arg_list<false, false>::type type;
            };

            // Here we only have a range, and one other argument to the kernel.
            // Cannot be a buffer, as the pointer type would need to be followed by an integral type, corresponding
            // to the number of elements in the buffer.
            template <typename Range, typename First>
            struct gpu_discard_range<true, Range, First>
            {
                typedef typename gpu_arg_list<false, false, First>::type type;
            };

            template <typename First, typename Second, typename... Args>
            struct gpu_discard_range<false, First, Second, Args...>
            {
                typedef
                    typename gpu_arg_list<std::is_pointer<First>::value, std::is_integral<Second>::value, First, Second, Args...>::type type;
            };

            template <typename Range, typename First, typename Second, typename... Args>
            struct gpu_discard_range<true, Range, First, Second, Args...>
            {
                typedef
                    typename gpu_arg_list<std::is_pointer<First>::value, std::is_integral<Second>::value, First, Second, Args...>::type type;
            };

            template <typename F, typename... Args>
            struct replace_pointer_size_pair_with_bufferptr
            {
                typedef typename gpu_discard_range<is_range<F>::value, F, Args...>::type type;
            };

            // Generate the gpu kernel type
            template <typename... Args>
            struct gpu_kernel_generator;

            template <typename... Args>
            struct gpu_kernel_generator<arg_list_gen<Args...>>
            {
                typedef hetcompute::gpu_kernel<Args...> type;
            };

            template <bool is_output_buffer, typename... Args>
            struct output_buffer_type_extractor;

            // First argument is an output buffer ptr.
            template <typename T, typename... Args>
            struct output_buffer_type_extractor<true, T, Args...>
            {
                typedef T type;
            };

            // First argument is not an output buffer ptr.
            template <typename F, typename S, typename... Args>
            struct output_buffer_type_extractor<false, F, S, Args...>
            {
                typedef
                    typename output_buffer_type_extractor<::hetcompute::internal::pattern::utility::is_output_buffer_ptr<S>::value, S, Args...>::type
                        type;
            };

            // Generate the gpu kernel type
            template <typename... Args>
            struct get_output_buffer_type;

            template <typename F, typename... Args>
            struct get_output_buffer_type<arg_list_gen<F, Args...>>
            {
                typedef
                    typename output_buffer_type_extractor<::hetcompute::internal::pattern::utility::is_output_buffer_ptr<F>::value, F, Args...>::type
                        type;
            };

            struct dsp_arg_list
            {
                typedef arg_list_gen<> type;
            };

            // The dsp compiler does not like size_t, so we use int in the range expansion
            // Expand hetcompute::range<N> into (int, int...) for dsp kernel
            template <bool isRange, typename... Args>
            struct dsp_expand_range;

            template <typename... Args>
            struct dsp_expand_range<false, Args...>
            {
                typedef typename dsp_arg_list::type::template append<Args...>::type type;
            };

            template <typename... Args>
            struct dsp_expand_range<true, hetcompute::range<1>, Args...>
            {
                typedef typename dsp_arg_list::type::template append<int, int, Args...>::type type;
            };

            template <typename... Args>
            struct dsp_expand_range<true, hetcompute::range<2>, Args...>
            {
                typedef typename dsp_arg_list::type::template append<int, int, int, int, Args...>::type type;
            };

            template <typename... Args>
            struct dsp_expand_range<true, hetcompute::range<3>, Args...>
            {
                typedef typename dsp_arg_list::type::template append<int, int, int, int, int, int, Args...>::type type;
            };

            template <typename F, typename... Args>
            struct make_dsp_kernel_args
            {
                typedef typename dsp_expand_range<is_range<F>::value, F, Args...>::type type;
            };

            // Generate the dsp kernel signature
            template <typename... Args>
            struct dsp_kernel_signature_generator;

            template <typename... Args>
            struct dsp_kernel_signature_generator<arg_list_gen<Args...>>
            {
                typedef int (*type)(Args...);
            };

            // Generate the dsp kernel type
            template <typename... Args>
            struct dsp_kernel_generator;

            template <typename... Args>
            struct dsp_kernel_generator<arg_list_gen<Args...>>
            {
                typedef ::hetcompute::dsp_kernel<int (*)(Args...)> type;
            };

            // Generate the dsp buffer type
            template <typename... Args>
            struct get_dsp_output_buffer_type
            {
                typedef typename get_output_buffer_type<typename replace_pointer_size_pair_with_bufferptr<Args...>::type>::type type;
            };

#ifdef HETCOMPUTE_HAVE_OPENCL
            template <typename T = void>
            static std::string stringize(std::string type, std::string name)
            {
                std::string final_signature_string("");
                if (type.find("*") != std::string::npos)
                {
                    // pointer type. Use __global address space qualifier.
                    final_signature_string += "__global " + type;
                }
                else
                {
                    // Not a pointer type. Use default __private address space qualifier.
                    final_signature_string += "__private " + type;
                }
                final_signature_string += " " + name + ")";
                return final_signature_string.c_str();
            }

            template <typename... Args>
            static std::string stringize(std::string type, std::string name, std::string type1, std::string name1, Args... args)
            {
                std::string final_signature_string("");
                if (type.find("*") != std::string::npos)
                {
                    // pointer type. Use __global address space qualifier.
                    final_signature_string += "__global " + type;
                }
                else
                {
                    // Not a pointer type. Use default __private address space qualifier.
                    final_signature_string += "__private " + type;
                }
                final_signature_string += " " + name + ", ";
                std::string temp = final_signature_string + stringize(type1, name1, args...);
                return temp.c_str();
            }

            template <typename... Args>
            std::string gpu_gen_signature(Args... args)
            {
                std::string temp = "(" + stringize(args...);
                return temp;
            }
#endif     // HETCOMPUTE_HAVE_OPENCL
        }; // namespace pointkernel
    }; // namespace internal
}; // namespace hetcompute
