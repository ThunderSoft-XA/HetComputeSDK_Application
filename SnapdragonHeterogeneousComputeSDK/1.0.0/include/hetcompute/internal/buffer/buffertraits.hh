#pragma once

namespace hetcompute
{
    namespace internal
    {
        namespace buffer
        {
            namespace utility
            {
                /// Type trait to get the buffer element type in hetcompute buffers
                template <typename BufferPtr>
                struct buffertraits;

                template<typename T>
                struct buffertraits<::hetcompute::buffer_ptr<T>>
                {
                    using element_type = T;
                    using api20 = std::true_type;
                };  // struct buffertraits

                // The following templates check whether T is a buffer_ptr
                template <typename T>
                struct is_buffer_ptr;

                template <typename T>
                struct is_buffer_ptr : public std::false_type
                {
                }; // struct is_buffer_ptr

                // Return true when the argument is a hetcompute::buffer
                template <typename T>
                struct is_buffer_ptr<::hetcompute::buffer_ptr<T>> : public std::true_type
                {
                };  // struct is_buffer_ptr

                template <typename TP, size_t index, bool TP_index_is_buffer_ptr>
                struct expand_buffer_to_pointer_size_pair_for_cpu;

                /// Non buffer specialization. Pass arguments as they are.
                /// Do not perform any change on them
                template <typename TP, size_t index>
                struct expand_buffer_to_pointer_size_pair_for_cpu<TP, index, false>
                {
                    using arg_type = typename std::tuple_element<index, TP>::type;
                    std::tuple<arg_type> _tp_at_index;

                    explicit expand_buffer_to_pointer_size_pair_for_cpu(TP& tp) : _tp_at_index(std::get<index>(tp))
                    {
                    }
                };  // struct expand_buffer_to_pointer_size_pair_for_cpu

                // The following methods are invoked when calling heterogeneous pfor_each using kernels generated from
                // a pointkernel.
                // Templates for buffer specialization. Convert a hetcompute buffer into its initial address and size
                // The construction of this class guarantees that the data is ready for consumption.

                template <typename TP, size_t index>
                struct expand_buffer_to_pointer_size_pair_for_cpu<TP, index, true>
                {
                    // type of the buffer
                    using buffer_type = typename std::decay<typename std::tuple_element<index, TP>::type>::type;
                    // type of the elements stored by the buffer
                    using element_type = typename buffertraits<buffer_type>::element_type;
                    std::tuple<element_type*, size_t> _tp_at_index;

                    template <typename T>
                    element_type* get_data(::hetcompute::buffer_ptr<T> b)
                    {
                        auto bptr = static_cast<element_type*>(b.host_data());
                        if (bptr == nullptr)
                        {
                            b.acquire_ro();
                            b.release();
                            bptr = static_cast<element_type*>(b.host_data());
                        }
                        return bptr;
                    }

                    template <typename T>
                    size_t get_size(::hetcompute::buffer_ptr<T> b)
                    {
                        return b.size();
                    }

                    explicit expand_buffer_to_pointer_size_pair_for_cpu(TP& tp)
                        : _tp_at_index(std::make_tuple(get_data(std::get<index>(tp)), get_size(std::get<index>(tp))))
                    {
                    }
                };  // struct expand_buffer_to_pointer_size_pair_for_cpu

                template <typename... Args>
                struct type_list;

                template <typename... Args>
                struct type_list<hetcompute::gpu_kernel<Args...>>
                {
                    template <std::size_t N>
                    using type = typename std::tuple_element<N, std::tuple<Args...>>::type;
                };  // struct type_list

                template <typename T, T V>
                struct constant
                {
                    constexpr /* implicit */ operator T() const { return V; }
                };  // struct constant

                // Perform the buffer conversion for the argument tuple
                template <typename TP, size_t index, bool is_finished>
                struct expand_buffers_in_args_for_cpu;

                /// Conversion of a single element of the arguments and continue the conversion for the next
                /// argument in the tuple
                template <typename TP, size_t index>
                struct expand_buffers_in_args_for_cpu<TP, index, false>
                {
                    using tuple_type = typename std::tuple_element<index, TP>::type;
                    // type of the current argument
                    using element_type = typename std::decay<tuple_type>::type;

                    // check whether the current argument is a buffer
                    static bool constexpr is_index_arg_a_buffer_ptr = is_buffer_ptr<element_type>::value;

                    // translate the current argument
                    expand_buffer_to_pointer_size_pair_for_cpu<TP, index, is_index_arg_a_buffer_ptr> _cpu_container_for_index;
                    // translate the next arguments
                    using rest_type = expand_buffers_in_args_for_cpu<TP, index + 1, index + 1 >= std::tuple_size<TP>::value>;
                    rest_type _rest;

                    using cpu_tuple_till_index = decltype(std::tuple_cat(_cpu_container_for_index._tp_at_index, _rest._expanded_args_list));

                    // contains a tuple of cpu kernel arguments converted from tp<index .. end>
                    // this tuple is built as the argument list is traversed
                    cpu_tuple_till_index _expanded_args_list;

                    explicit expand_buffers_in_args_for_cpu(TP& tp)
                        : _cpu_container_for_index(tp),
                          _rest(tp),
                          _expanded_args_list(std::tuple_cat(_cpu_container_for_index._tp_at_index, _rest._expanded_args_list))
                    {
                    }
                };  // expand_buffers_in_args_for_cpu

                /// Terminate the conversion and return and empty list when there are not more
                /// arguments to process
                template <typename TP, size_t index>
                struct expand_buffers_in_args_for_cpu<TP, index, true>
                {
                    std::tuple<> _expanded_args_list;

                    explicit expand_buffers_in_args_for_cpu(TP&) : _expanded_args_list() {}
                };  // expand_buffers_in_args_for_cpu

                template <typename TP, size_t index, bool TP_index_is_buffer_ptr>
                struct expand_buffer_to_buffer_size_pair_for_gpu;

                /// Non buffer specialization. Pass arguments as they are.
                /// Do not perform any change on them
                template <typename TP, size_t index>
                struct expand_buffer_to_buffer_size_pair_for_gpu<TP, index, false>
                {
                    using arg_type = typename std::tuple_element<index, TP>::type;
                    std::tuple<arg_type> _tp_at_index;

                    explicit expand_buffer_to_buffer_size_pair_for_gpu(TP& tp) : _tp_at_index(std::get<index>(tp)) {}
                };  // struct expand_buffer_to_buffer_size_pair_for_gpu

                //
                // Templates for buffer specialization. Convert a hetcompute buffer into buffer_ptr and size
                // The construction of this class guarantees that the data is ready for consumption.

                template <typename TP, size_t index>
                struct expand_buffer_to_buffer_size_pair_for_gpu<TP, index, true>
                {
                    using tuple_type = typename std::tuple_element<index, TP>::type;
                    // type of the buffer
                    std::tuple<tuple_type, size_t> _tp_at_index;

                    template <typename T>
                    size_t get_size(::hetcompute::buffer_ptr<T> b)
                    {
                        return b.size();
                    }

                    explicit expand_buffer_to_buffer_size_pair_for_gpu(TP& tp)
                        : _tp_at_index(std::make_tuple(std::get<index>(tp), get_size(std::get<index>(tp))))
                    {
                    }
                };  // struct expand_buffer_to_buffer_size_pair_for_gpu

                // Perform the buffer conversion for the argument tuple
                template <typename TP, size_t index, bool is_finished>
                struct expand_buffers_in_args_for_gpu;

                /// Conversion of a single element of the arguments and continue the conversion for the next
                /// argument in the tuple
                template <typename TP, size_t index>
                struct expand_buffers_in_args_for_gpu<TP, index, false>
                {
                    using tuple_type = typename std::tuple_element<index, TP>::type;
                    // type of the current argument
                    using element_type = typename std::decay<tuple_type>::type;

                    // check whether the current argument is a buffer
                    static bool constexpr is_index_arg_a_buffer_ptr = is_buffer_ptr<element_type>::value;

                    // translate the current argument
                    expand_buffer_to_buffer_size_pair_for_gpu<TP, index, is_index_arg_a_buffer_ptr> _gpu_container_for_index;
                    // translate the next arguments
                    using rest_type = expand_buffers_in_args_for_gpu<TP, index + 1, index + 1 >= std::tuple_size<TP>::value>;
                    rest_type _rest;

                    using gpu_tuple_till_index = decltype(std::tuple_cat(_gpu_container_for_index._tp_at_index, _rest._expanded_args_list));

                    // contains a tuple of gpu kernel arguments converted from tp<index .. end>
                    // this tuple is built as the argument list is traversed
                    gpu_tuple_till_index _expanded_args_list;

                    explicit expand_buffers_in_args_for_gpu(TP& tp)
                        : _gpu_container_for_index(tp),
                          _rest(tp),
                          _expanded_args_list(std::tuple_cat(_gpu_container_for_index._tp_at_index, _rest._expanded_args_list))
                    {
                    }
                };  // struct expand_buffers_in_args_for_gpu

                /// Terminate the conversion and return an empty list when there are no more
                /// arguments to process
                template <typename TP, size_t index>
                struct expand_buffers_in_args_for_gpu<TP, index, true>
                {
                    std::tuple<> _expanded_args_list;

                    explicit expand_buffers_in_args_for_gpu(TP&) : _expanded_args_list() {}
                };  // struct expand_buffers_in_args_for_gpu

            };  // namespace utility
        };  // namespace buffer
    };  // namespace internal
};  // namespace hetcompute
