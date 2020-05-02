/** @file pipelineutility.hh */
#pragma once

#include <hetcompute/internal/buffer/bufferpolicy.hh>

namespace hetcompute
{
    namespace pattern
    {
        class tuner;
    } // namespace pattern

    namespace internal
    {
        namespace pipeline_utility
        {
            /**
             * enum for template magic to add stages with variadic parameters
             */
            enum stage_param_list_type
            {
                hetcompute_iteration_lag,
                hetcompute_iteration_rate,
                hetcompute_sliding_window_size,
                hetcompute_stage_pattern_tuner
            };

            /**
             * enum for template magic to launch pipeline with variadic parameters
             */
            enum launch_param_list_type
            {
                hetcompute_num_iterations,
                hetcompute_launch_pattern_tuner
            };

            /**
             * Get the greatest common divisor (GCD) of two numbers
             * @param size_t a first number
             * @param size_t b second number
             * @return size_t GCD of a and b
             */
            static inline size_t get_gcd(size_t a, size_t b)
            {
                while (a * b != 0)
                {
                    if (a >= b)
                    {
                        a = a % b;
                    }
                    else
                    {
                        b = b % a;
                    }
                }
                return (a + b);
            }

            /**
             * Get the last iteration id for the successor stage
             * @param rate_curr
             * @param rate_succ
             * @param curr_iter
             * @param size_t succ_iter
             */
            static inline size_t get_succ_iter(size_t rate_curr, size_t rate_succ, size_t curr_iter)
            {
                return curr_iter * rate_succ / rate_curr;
            }

            /**
             * Get the iteration id for the predecessor stage (who launches curr_iter)
             * @param rate_pred
             * @param rate_curr
             * @param curr_iter
             * @param pred_t pred_iter
             */
            static inline size_t get_pred_iter(size_t rate_pred, size_t rate_curr, size_t curr_iter)
            {
                return (curr_iter * rate_pred + rate_curr - 1) / rate_curr;
            }

            /**
             * helper to get the 2nd (arg1) argument type of the lambda
             */
            template <typename T, size_t arity>
            struct get_body_arg1_type
            {
                using type = void;
            };

            template <typename T>
            struct get_body_arg1_type<T, 2>
            {
                using type = typename internal::function_traits<T>::template arg_type<1>;
            };

            template <size_t arity>
            struct get_body_arg1_type<void, arity>
            {
                using type = void;
            };

            /**
             * helper to check if the 2nd argument of a cpu stage lambda is based on stage_input
             */
            template <typename T, size_t arity, typename B>
            struct is_body_arg1_based_on_b;

            template <typename T, typename B>
            struct is_body_arg1_based_on_b<T, 2, B>
            {
                using arg1_type       = typename internal::function_traits<T>::template arg_type<1>;
                using arg1_type_noref = typename std::remove_reference<arg1_type>::type;

                static_assert(
                    std::is_reference<arg1_type>::value,
                    "the second argument of a cpu stage lambda/function shuold always be a reference to hetcompute::stage_input<T>.");

                static constexpr bool value = std::is_base_of<B, arg1_type_noref>::value ? true : false;
            };

            template <typename T, typename B>
            struct is_body_arg1_based_on_b<T, 1, B>
            {
                static constexpr bool value = true;
            };

            template <size_t arity, typename B>
            struct is_body_arg1_based_on_b<void, arity, B>
            {
                static constexpr bool value = true;
            };

            /**
             * helper to check if a type is a std::tuple or not
             */
            template <typename... Args>
            struct is_std_tuple
            {
                static constexpr bool value = false;
            };

            template <typename... Args>
            struct is_std_tuple<std::tuple<Args...>>
            {
                static constexpr bool value = true;
            };

            /**
             * helper struct to extract the type of a stage_input
             */
            template <typename T>
            struct strip_stage_input_type
            {
                using type = T;
            };

            template <typename T>
            struct strip_stage_input_type<hetcompute::stage_input<T>&>
            {
                using type = T;
            };

            /**
             * helper struct to check whether a type is a parallel_stage
             */
            template <typename T>
            struct is_parallel_stage
            {
                static constexpr bool value = false;
            };

            template <>
            struct is_parallel_stage<hetcompute::parallel_stage>
            {
                static constexpr bool value = true;
            };

            /**
             * helper struct to check whether a type is a serial_stage
             */
            template <typename T>
            struct is_serial_stage
            {
                static constexpr bool value = false;
            };

            template <>
            struct is_serial_stage<hetcompute::serial_stage>
            {
                static constexpr bool value = true;
            };

            /**
             * helper struct to check whether a type is a iteration_lag
             */
            template <typename T>
            struct is_iteration_lag
            {
                static constexpr bool value = false;
            };

            template <>
            struct is_iteration_lag<hetcompute::iteration_lag>
            {
                static constexpr bool value = true;
            };

            /**
             * helper struct to check whether a type is a iteration_rate
             */
            template <typename T>
            struct is_iteration_rate
            {
                static constexpr bool value = false;
            };

            template <>
            struct is_iteration_rate<hetcompute::iteration_rate>
            {
                static constexpr bool value = true;
            };

            /**
             * helper struct to check whether a type is a sliding_window_size
             */
            template <typename T>
            struct is_sliding_window_size
            {
                static constexpr bool value = false;
            };

            template <>
            struct is_sliding_window_size<hetcompute::sliding_window_size>
            {
                static constexpr bool value = true;
            };

            /**
             * helper struct to check whether a type is a pattern tuner
             */
            template <typename T>
            struct is_pattern_tuner
            {
                static constexpr bool value = false;
            };

            template <>
            struct is_pattern_tuner<hetcompute::pattern::tuner>
            {
                static constexpr bool value = true;
            };

            /**
             * Checks whether T is an callable object for hetcompute pipeline
             * match lambda, functor or other types except functions
             */
            template <typename T>
            struct is_hetcompute_pipeline_callable
            {
            private:
                template <typename U>
                static auto check(int) -> decltype(&U::operator(), std::true_type());

                template <typename>
                static std::false_type check(...);

                using enable_if_type = decltype(check<T>(0));

            public:
                using type = typename enable_if_type::type;
                enum
                {
                    value = type::value
                };
            }; // is_hetcompute_pipeline_callable

            // Matches function references
            template <typename FReturnType, typename... FArgs>
            struct is_hetcompute_pipeline_callable<FReturnType (&)(FArgs...)>
            {
                static constexpr bool value = true;
            };

            // Matches function pointers
            template <typename FReturnType, typename... FArgs>
            struct is_hetcompute_pipeline_callable<FReturnType (*)(FArgs...)>
            {
                static constexpr bool value = true;
            };

            /**
             * helper structure to get the type of a tuple element
             * type = void when index is -1
             */
            template <int index, typename TP>
            struct tuple_element_type_helper
            {
                using type = typename std::tuple_element<index, TP>::type;
            };

            template <typename TP>
            struct tuple_element_type_helper<-1, TP>
            {
                using type = void;
            };

            /**
             * helper for parsing the type of the pipeline stage (parallel or serial)
             * pindex is the index of the parallel stage in the tuple
             * sindex is the index of the serial stage in the tuple
             */
            template <int index, typename TP>
            struct stage_type_parser
            {
                using prior = stage_type_parser<index - 1, TP>;
                using type =
                    typename std::remove_cv<typename std::remove_reference<typename std::tuple_element<index, TP>::type>::type>::type;

                static_assert(prior::pindex == -1 || !is_parallel_stage<type>::value,
                              "Multiple parallel_stage definitions found in the pipeline");

                static_assert(prior::sindex == -1 || !is_serial_stage<type>::value,
                              "Multiple serial_stage definitions found in the pipeline");

                static constexpr int pindex = (prior::pindex != -1 ? prior::pindex : is_parallel_stage<type>::value ? index : -1);

                static constexpr int sindex = (prior::sindex != -1 ? prior::sindex : is_serial_stage<type>::value ? index : -1);
            };

            template <typename TP>
            struct stage_type_parser<-1, TP>
            {
                static constexpr int pindex = -1;
                static constexpr int sindex = -1;
            };

            template <typename... Args>
            struct check_stage_type
            {
                using TP     = std::tuple<Args...>;
                using result = stage_type_parser<std::tuple_size<TP>::value - 1, TP>;

                static_assert(result::pindex != -1 || result::sindex != -1,
                              "a pipeline stage should be specified to be either serial or parallel.");

                static_assert(result::pindex == -1 || result::sindex == -1,
                              "a pipeline stage cannot be serial and parallel at the same time.");

                static constexpr bool has_parallel_stage = result::pindex == -1 ? false : true;
                static constexpr int  pindex             = result::pindex;
                static constexpr int  sindex             = result::sindex;
                static constexpr int  index              = has_parallel_stage ? pindex : sindex;

                using type = typename std::conditional<has_parallel_stage, hetcompute::parallel_stage, hetcompute::serial_stage>::type;
            };

            /**
             * helper for parsing the callable functor (for the cpu stage)
             * type the type of the lambda
             * index the index of the lambda in the tuple
             */
            template <int index, typename TP, int offset>
            struct callable_parser
            {
                using prior      = callable_parser<index - 1, TP, offset - 1>;
                using type       = typename std::tuple_element<index, TP>::type;
                using unref_type = typename std::remove_reference<typename std::tuple_element<index, TP>::type>::type;

                static constexpr bool is_callable =
                    is_hetcompute_pipeline_callable<type>::value || is_hetcompute_pipeline_callable<unref_type>::value;

                static_assert(prior::callable_index == -1 || !is_callable, "Multiple lambdas found for the pipeline cpu stage");

                static constexpr int callable_index = (prior::callable_index != -1 ? prior::callable_index : is_callable ? index : -1);
            };

            template <int index, typename TP>
            struct callable_parser<index, TP, -1>
            {
                static constexpr int callable_index = -1;
            };

            template <typename... Args>
            struct extract_callable_lambda
            {
                using TP     = std::tuple<Args...>;
                using result = callable_parser<std::tuple_size<TP>::value - 1, TP, std::tuple_size<TP>::value - 1>;

                static constexpr int index = result::callable_index;
                using type                 = typename tuple_element_type_helper<index, TP>::type;
            };

            /**
             * check the number of stage type, lag, iterations and sliding window size specified in add_stage_args
             */
            template <int index, typename TP>
            struct parse_add_stage_params_appearance
            {
                using prior = parse_add_stage_params_appearance<index - 1, TP>;
                using type =
                    typename std::remove_cv<typename std::remove_reference<typename std::tuple_element<index, TP>::type>::type>::type;

                static constexpr size_t parallel_stage_num =
                    is_parallel_stage<type>::value ? prior::parallel_stage_num + 1 : prior::parallel_stage_num;
                static_assert(parallel_stage_num < 2, "Multiple parallel_stage defined when adding the pipeline stage");

                static constexpr size_t serial_stage_num =
                    is_serial_stage<type>::value ? prior::serial_stage_num + 1 : prior::serial_stage_num;
                static_assert(serial_stage_num < 2, "Multiple serial_stage defined when adding the pipeline stage");

                static constexpr size_t iteration_lag_num =
                    is_iteration_lag<type>::value ? prior::iteration_lag_num + 1 : prior::iteration_lag_num;
                static_assert(iteration_lag_num < 2, "Multiple iteration_lag defined when adding the pipeline stage");

                static constexpr size_t iteration_rate_num =
                    is_iteration_rate<type>::value ? prior::iteration_rate_num + 1 : prior::iteration_rate_num;
                static_assert(iteration_rate_num < 2, "Multiple iteration_rate defined when adding the pipeline stage");

                static constexpr size_t sliding_window_size_num =
                    is_sliding_window_size<type>::value ? prior::sliding_window_size_num + 1 : prior::sliding_window_size_num;
                static_assert(sliding_window_size_num < 2, "Multiple sliding_window_size defined when adding the pipeline stage");

                static constexpr size_t pattern_tuner_num =
                    is_pattern_tuner<type>::value ? prior::pattern_tuner_num + 1 : prior::pattern_tuner_num;
                static_assert(pattern_tuner_num < 2, "Multiple pattern tuners defined when adding the pipeline stage");

                static constexpr int iteration_lag_index = is_iteration_lag<type>::value ? index : prior::iteration_lag_index;

                static constexpr int iteration_rate_index = is_iteration_rate<type>::value ? index : prior::iteration_rate_index;

                static constexpr int sliding_window_size_index =
                    is_sliding_window_size<type>::value ? index : prior::sliding_window_size_index;

                static constexpr int pattern_tuner_index = is_pattern_tuner<type>::value ? index : prior::pattern_tuner_index;
            };

            template <typename TP>
            struct parse_add_stage_params_appearance<-1, TP>
            {
                static constexpr size_t parallel_stage_num      = 0;
                static constexpr size_t serial_stage_num        = 0;
                static constexpr size_t iteration_lag_num       = 0;
                static constexpr size_t iteration_rate_num      = 0;
                static constexpr size_t sliding_window_size_num = 0;
                static constexpr size_t pattern_tuner_num       = 0;

                static constexpr int iteration_lag_index       = -1;
                static constexpr int iteration_rate_index      = -1;
                static constexpr int sliding_window_size_index = -1;
                static constexpr int pattern_tuner_index       = -1;
            };

            template <typename ContextRefType, typename... Args>
            struct check_add_cpu_stage_params
            {
                static constexpr bool value = true;
                using TP                    = std::tuple<Args...>;
                using result                = parse_add_stage_params_appearance<std::tuple_size<TP>::value - 1, TP>;

                using Body                      = typename extract_callable_lambda<Args...>::type;
                static constexpr int body_index = extract_callable_lambda<Args...>::index;
                static_assert(body_index != -1, "a cpu stage should have its function defined");

                using StageType                  = typename check_stage_type<Args...>::type;
                static constexpr int stage_index = check_stage_type<Args...>::index;
                static_assert(stage_index != -1, "a cpu stage should specify its type, i.e. serial or parallel");

                using arg0_type = typename function_traits<Body>::template arg_type<0>;
                static_assert(std::is_same<arg0_type, ContextRefType>::value,
                              "The 1st param for a stage function should be the pipeline context ref.");
                static_assert(is_body_arg1_based_on_b<Body, function_traits<Body>::arity::value, hetcompute::stage_input_base>::value,
                              "The 2nd param for a cpu stage function should be a reference to type stage_input<xxx>");

                static_assert(result::parallel_stage_num == 1 || result::serial_stage_num == 1,
                              "a cpu stage should specify its type, i.e. serial or parallel");
            };

            /**
             * check the number of stage type, lag, iterations and sliding window size specified in add_stage_args
             */
            template <int index, typename TP>
            struct parse_launch_params_appearance
            {
                using prior = parse_launch_params_appearance<index - 1, TP>;
                using type =
                    typename std::remove_cv<typename std::remove_reference<typename std::tuple_element<index, TP>::type>::type>::type;
                static constexpr size_t pattern_tuner_num =
                    is_pattern_tuner<type>::value ? prior::pattern_tuner_num + 1 : prior::pattern_tuner_num;
                static_assert(pattern_tuner_num < 2, "Multiple pattern tuners defined when adding the pipeline stage");

                static constexpr int pattern_tuner_index = is_pattern_tuner<type>::value ? index : prior::pattern_tuner_index;
            };

            template <typename TP>
            struct parse_launch_params_appearance<-1, TP>
            {
                static constexpr size_t pattern_tuner_num = 0;

                static constexpr int pattern_tuner_index = -1;
            };

            template <typename... Args>
            struct check_launch_params
            {
                static constexpr bool value = true;
                using TP                    = std::tuple<Args...>;
                using result                = parse_launch_params_appearance<std::tuple_size<TP>::value - 1, TP>;
            };

            /**
             * helper to get the reference type of a type
             * but get int* for type void
             */
            template <typename T>
            struct type_helper
            {
                using org_type =
                    typename std::conditional<std::is_same<T, void*>::value, void*, typename std::remove_reference<T>::type>::type;

                using ref_type = typename std::conditional<std::is_same<T, void*>::value, void*, org_type&>::type;

                using rref_type = typename std::conditional<std::is_same<T, void*>::value, void*, org_type&&>::type;
            };

            /**
             * helper to get the ringbuffer type for a specific input element type
             * return stagebuffer if the element type is void
             */
            template <typename T>
            struct get_ringbuffer_type
            {
                using type =
                    typename std::conditional<std::is_same<T, void>::value, hetcompute::internal::stagebuffer, hetcompute::internal::ringbuffer<T>>::type;
            };

            /**
             * helper to get the dynamicbuffer type for a specific input element type
             * return stagebuffer if the element type is void
             */
            template <typename T>
            struct get_dynamicbuffer_type
            {
                using type = typename std::
                    conditional<std::is_same<T, void>::value, hetcompute::internal::stagebuffer, hetcompute::internal::dynamicbuffer<T>>::type;
            };

            /**
             * help to extract tuple element and return nullptr when index == -1
             */
            template <int index, typename TP>
            struct get_tuple_element_helper
            {
                static typename std::tuple_element<index, TP>::type get(TP& t)
                {
                    using elem_type = typename std::tuple_element<index, TP>::type;
                    return std::forward<elem_type>(std::get<index>(t));
                }
            };

            template <typename TP>
            struct get_tuple_element_helper<-1, TP>
            {
                static int* get(TP&) { return nullptr; }
            };

            /**
             * helper to copy the value from tuple TP1<index1> when index1 is not -1;
             * otherwise copy the value from tupel TP2<index2> (needs implicit copy constructor)
             */
            template <typename RT, typename TP1, typename TP2, int index1, int index2>
            struct mux_param_value
            {
                static RT get(TP1& tp1, TP2&)
                {
                    using elem_type =
                        typename std::remove_cv<typename std::remove_reference<typename std::tuple_element<index1, TP1>::type>::type>::type;
                    static_assert(std::is_same<elem_type, RT>::value, "return type does not match");
                    return std::get<index1>(tp1);
                }
            };

            template <typename RT, typename TP1, typename TP2, int index2>
            struct mux_param_value<RT, TP1, TP2, -1, index2>
            {
                static RT get(TP1&, TP2& tp2)
                {
                    using elem_type =
                        typename std::remove_cv<typename std::remove_reference<typename std::tuple_element<index2, TP2>::type>::type>::type;
                    static_assert(std::is_same<elem_type, RT>::value, "return type does not match");
                    return std::get<index2>(tp2);
                }
            };

            /**
             * helper to extract the type of the CPU body
             * have some drama with getting (&) instead of (*) for function bodies
             * while using && for the parameters of add_stage
             */
            template <typename T, int id>
            struct get_cpu_body_type
            {
                static_assert(id != -1,
                              "a CPU stage should always have one body,"
                              "i.e. lambda/callable object /function ptr");
                using type = typename ::std::tuple_element<id, T>::type;
            };

            /**
             * helper template struct to workaround
             * lambda capture error for variadic parameters
             * by wrapping them as an std::tuple and expand one by one to apply
             */
            template <size_t N>
            struct apply_launch
            {
                template <typename CPINST, typename LAUNCHTYPE, typename TUNER, typename TUPLE, typename... T>
                static void apply(CPINST* cpinst, LAUNCHTYPE& launch_type, TUNER const& tuner, TUPLE& tp, T&... t)
                {
                    apply_launch<N - 1>::apply(cpinst, launch_type, tuner, tp, std::get<N - 1>(tp), t...);
                }
            };

            template <>
            struct apply_launch<0>
            {
                template <typename CPINST, typename LAUNCHTYPE, typename TUNER, typename TUPLE, typename... T>
                static void apply(CPINST* cpinst, LAUNCHTYPE& launch_type, TUNER const& tuner, TUPLE&, T&... t)
                {
                    cpinst->launch(t..., launch_type, tuner);
                }
            };

            /**
             * helper struct to check whether a type is a gpu kernel
             */
            template <typename... Args>
            struct is_gpu_kernel
            {
                static constexpr bool value = false;
            };

            template <typename... Args>
            struct is_gpu_kernel<hetcompute::gpu_kernel<Args...>&>
            {
                static constexpr bool value = true;
            };

            template <typename... Args>
            struct is_gpu_kernel<hetcompute::gpu_kernel<Args...>>
            {
                static constexpr bool value = true;
            };

            /**
             * helper for parsing the existence of gpu kernel
             * index is the index of the gpu kernel in the tuple
             */
            template <int index, typename TP>
            struct gpu_kernel_parser
            {
                using prior = gpu_kernel_parser<index - 1, TP>;
                using type  = typename std::tuple_element<index, TP>::type;

                static_assert(prior::gk_index == -1 || !is_gpu_kernel<type>::value, "Multiple gpu kernel found for the pipeline gpu stage");

                static constexpr int gk_index = (prior::gk_index != -1 ? prior::gk_index : is_gpu_kernel<type>::value ? index : -1);
            };

            template <typename TP>
            struct gpu_kernel_parser<-1, TP>
            {
                static constexpr int gk_index = -1;
            };

            template <typename... Args>
            struct check_gpu_kernel
            {
                using TP     = std::tuple<Args...>;
                using result = gpu_kernel_parser<std::tuple_size<TP>::value - 1, TP>;

                static constexpr bool has_gpu_kernel = result::gk_index == -1 ? false : true;
                static constexpr int  index          = result::gk_index;

                using type = typename tuple_element_type_helper<index, TP>::type;
            };

#ifndef HETCOMPUTE_HAVE_RTTI
            template <typename T>
            struct sizeof_type
            {
                static constexpr size_t size = sizeof(T);
            };

            template <>
            struct sizeof_type<void>
            {
                static constexpr size_t size = 0;
            };
#endif

#ifdef HETCOMPUTE_HAVE_GPU
            /**
             * helper to get the return tuple type of a before body
             * return std::tuple<> if the body returns void
             */
            template <typename T>
            struct get_return_tuple_type
            {
                using type = typename std::conditional<std::is_same<T, void>::value, std::tuple<>, T>::type;
            };

            template <typename... Args>
            struct is_hetcompute_pipeline_callable<hetcompute::gpu_kernel<Args...>&>
            {
                static constexpr bool value = false;
            };

            template <typename... Args>
            struct is_hetcompute_pipeline_callable<hetcompute::gpu_kernel<Args...>>
            {
                static constexpr bool value = false;
            };

            /**
             * helper to check if a type is a hetcompute::range or not
             */
            HETCOMPUTE_GCC_IGNORE_BEGIN("-Weffc++");
            template <typename T>
            struct is_hetcompute_range : std::false_type
            {
                static constexpr bool value = false;
            };

            template <size_t Dims>
            struct is_hetcompute_range<hetcompute::range<Dims>> : std::true_type
            {
                static constexpr bool value = true;
            };
            HETCOMPUTE_GCC_IGNORE_END("-Weffc++");

            /**
             * helper to remove the first element type from a tuple
             */
            template <typename Arg, typename... Args>
            struct remove_first_tuple_element_type;

            template <typename Arg, typename... Args>
            struct remove_first_tuple_element_type<std::tuple<Arg, Args...>>
            {
                using type = std::tuple<Args...>;
            };

            template <typename Arg, typename... Args>
            struct remove_first_tuple_element_type<std::tuple<Arg, Args...>&>
            {
                using type = std::tuple<Args...>&;
            };

            // utilities for gpu stages
            /**
             * template helper for stripping buffer directions for variadic template params
             */
            template <typename... BufDirTypes>
            struct strip_buffer_dir_params;

            template <typename... BufDirTypes>
            struct strip_buffer_dir_params<std::tuple<BufDirTypes...>>
            {
                using stripped_tuple_type = std::tuple<typename strip_buffer_dir<BufDirTypes>::type...>;
            };

            /**
             * helper to get the argument type of the after lambda
             */
            template <typename T, size_t arity>
            struct get_af_arg1_type
            {
                using type = void;
            };

            template <typename T>
            struct get_af_arg1_type<T, 2>
            {
                using type = typename internal::function_traits<T>::template arg_type<1>;
            };

            template <size_t arity>
            struct get_af_arg1_type<void, arity>
            {
                using type = void;
            };

            /**
             * helper to extract the type of the CPU body
             * have some drama with getting (&) instead of (*) for function bodies
             * while using && for the parameters of add_stage
             */
            template <typename T, int id>
            struct get_body_type
            {
                using type = typename ::std::tuple_element<id, T>::type;
            };

            template <typename T>
            struct get_body_type<T, -1>
            {
                using type = void;
            };

            /**
             * Helper for parsing the before functor (provided before the gpu kernel)
             */
            template <typename... Args>
            struct extract_before_lambda
            {
                using TP = std::tuple<Args...>;

                static_assert(check_gpu_kernel<Args...>::has_gpu_kernel, "extracting before lambda from a non-gpu stage");

                using result = callable_parser<check_gpu_kernel<Args...>::index - 1, TP, check_gpu_kernel<Args...>::index - 1>;

                // The index of the before lambda in the tuple
                static constexpr int index = result::callable_index;

                // The type of the before lambda
                // std::tuple_size<TP>::value if not found
                using type = typename tuple_element_type_helper<index, TP>::type;
            };

            /**
             * helper for parsing the after functor (provided after the gpu kernel)
             */
            template <typename... Args>
            struct extract_after_lambda
            {
                using TP = std::tuple<Args...>;

                static_assert(check_gpu_kernel<Args...>::has_gpu_kernel, "extracting after lambda from a non-gpu stage");

                using result =
                    callable_parser<std::tuple_size<TP>::value - 1, TP, std::tuple_size<TP>::value - check_gpu_kernel<Args...>::index - 1>;

                // The index of the after lambda in the tuple
                static constexpr int index = result::callable_index;

                // The type of the after lambda
                // std::tuple_size<TP>::value if not found
                using type = typename tuple_element_type_helper<index, TP>::type;
            };

            /**
             * helper to get the Args from a gpu kernel
             */
            template <typename... Args>
            struct get_gpu_kernel_args_tuple;

            template <typename... Args>
            struct get_gpu_kernel_args_tuple<hetcompute::gpu_kernel<Args...>>
            {
                using type = typename std::tuple<Args...>;
            };

            template <typename... Args>
            struct get_gpu_kernel_args_tuple<hetcompute::gpu_kernel<Args...>&>
            {
                using type = typename std::tuple<Args...>;
            };

            template <typename ContextRefType, typename InputTupleType, typename... Args>
            struct check_add_gpu_stage_params
            {
                static constexpr bool value = true;
                using TP                    = std::tuple<Args...>;
                using result                = parse_add_stage_params_appearance<std::tuple_size<TP>::value - 1, TP>;

                static constexpr int gkbody_index = check_gpu_kernel<Args...>::index;
                using GKBody                      = typename get_body_type<InputTupleType, gkbody_index>::type;
                using GKParamsTupleType           = typename get_gpu_kernel_args_tuple<GKBody>::type;
                static_assert(gkbody_index != -1, "gpu stage should have a gpu kernel");

                static constexpr int bbody_index = extract_before_lambda<Args...>::index;
                using BeforeBody                 = typename get_body_type<InputTupleType, bbody_index>::type;

                static constexpr int abody_index = extract_after_lambda<Args...>::index;
                using AfterBodyVoid              = typename get_body_type<InputTupleType, abody_index>::type;

                using AfterBody = typename std::conditional<std::is_same<AfterBodyVoid, void>::value, void*, AfterBodyVoid>::type;

                static_assert(!std::is_same<BeforeBody, void>::value, "a gpu stage should have a before lambda");

                using before_arg0_type = typename internal::function_traits<BeforeBody>::template arg_type<0>;
                static_assert(std::is_same<before_arg0_type, ContextRefType>::value,
                              "The 1st param for the before functor of a gpu stage should be the pipeline context ref.");

                // Provide a fake type if AfterBody is not provide.
                using UniAfterBody = typename std::conditional<std::is_same<AfterBodyVoid, void>::value, void (&)(void*), AfterBody>::type;

                using after_arg0_type = typename internal::function_traits<UniAfterBody>::template arg_type<0>;

                static_assert(std::is_same<after_arg0_type, ContextRefType>::value || std::is_same<after_arg0_type, void*>::value,
                              "The 1st param for the after functor of a gpu stage should be the pipeline context ref.");

                using before_return_type = typename internal::function_traits<BeforeBody>::return_type;
                static_assert(is_std_tuple<before_return_type>::value, "before lamdba should return a std::tuple");
                static_assert(std::tuple_size<before_return_type>::value >= 2,
                              "before lambda's return tuple should have at least two elements, i.e. a hetcompute::range, and one input for "
                              "the gpu kernel");

                using before_return_type_first_element = typename std::tuple_element<0, before_return_type>::type;
                static_assert(is_hetcompute_range<before_return_type_first_element>::value,
                              "before lambda's return tuple should be a std::tuple whose first element is a hetcompute::range");

                using before_return_type_wo_first_element = typename remove_first_tuple_element_type<before_return_type>::type;
                using gk_params_tuple_type                = typename strip_buffer_dir_params<GKParamsTupleType>::stripped_tuple_type;
                static_assert(std::is_same<before_return_type_wo_first_element, gk_params_tuple_type>::value,
                              "The return type of the before functor of a gpu stage should be an std::tuple of the gpu kernel parameters.");

                using StageType                  = typename check_stage_type<Args...>::type;
                static constexpr int stage_index = check_stage_type<Args...>::index;
                static_assert(stage_index != -1, "a gpu stage should specify its type, i.e. serial or parallel");

                static_assert(result::parallel_stage_num == 1 || result::serial_stage_num == 1,
                              "a gpu stage should specify its type, i.e. serial or parallel");
            };
#endif // HETCOMPUTE_HAVE_GPU

        }; // namespace pipeline_utility
    };     // namespace internal
};         // namespace hetcompute
