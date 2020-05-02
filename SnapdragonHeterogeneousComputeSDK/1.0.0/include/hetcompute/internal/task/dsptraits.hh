/** @file dsptraits.hh */
#pragma once

#if defined(HETCOMPUTE_HAVE_QTI_DSP)

#include <utility>

#include <hetcompute/internal/buffer/bufferpolicy.hh>
#include <hetcompute/internal/task/task.hh>

namespace hetcompute
{
    template <typename Fn>
    class dsp_kernel;

    namespace internal
    {
        //
        // Given a hexagon DSP function, Fn = int(Params...) this file
        // matches dsp Fn Params... against task arguments, Args...
        // The matching includes buffer expansion into pointer plus size
        // and the number of arguments is also verified
        //

        //
        // Capture the dsp function parameters
        //
        template <typename... Params>
        struct FnParams;

        //
        // Specialized version that forces the dsp function to return int as
        // required from the IDL and inserts the parameters into a tuple
        //
        template <typename... Params>
        struct FnParams<int (*)(Params...)>
        {
            using tuple_params = std::tuple<Params...>;
        };

        // The following templates check whether T is a buffer_ptr
        template <typename T>
        struct is_buffer_ptr;

        // If the argument is not a buffer, return false
        template <typename T>
        struct is_buffer_ptr : public std::false_type
        {
        };

        // Return true when the argument is a hetcompute::buffer
        template <typename T>
        struct is_buffer_ptr<::hetcompute::buffer_ptr<T>> : public std::true_type
        {
        };

        // Helper code to verify the matching. When the hetcompute argument is a buffer, we need
        // two IDL arguments
        // TP refers to the tuple of parameters
        template <typename TP, size_t index, typename Arg>
        struct match_buffer_against_two_Params;

        // Perform conversion for a buffer
        // TP refers to the tuple of parameters
        template <typename TP, size_t index, typename T>
        struct match_buffer_against_two_Params<TP, index, hetcompute::buffer_ptr<T>>
        {
            static_assert(index + 1 < std::tuple_size<TP>::value, "Extra parameters found");
            static_assert(std::is_same<typename std::tuple_element<index, TP>::type, T>::value, "Incorrect buffer type");
            // require to have the size after the buffer
            static_assert(std::is_integral<typename std::tuple_element<index + 1, TP>::type>::value, "Incorrect integral type after buffer");

            static constexpr auto next_index = index + 2;
        };

        // Matching function for the single, non-buffer, case
        template <typename TP, size_t index, typename Arg>
        struct match_single_Arg_against_single_Param
        {
            static constexpr auto next_index = index + 1;
        };

        // Ensure there are not extra arguments after the matching
        template <typename TP, size_t index>
        struct check_no_remaining_params
        {
            static_assert(std::tuple_size<TP>::value == index, "Extra parameters found");
        };

        // Helper function that will match the arguments and verify that their number is the same;
        // the number of parameters in the Tuple of Parameters, TP, that comes from the hexagon dsp function pointer has to match
        // the number if Args that the user has provided
        template <typename TP, size_t index, typename Arg, typename... RemainingArgs>
        struct compare_Args_against_Params
        {
            typedef typename std::conditional<is_buffer_ptr<Arg>::value,
                                              match_buffer_against_two_Params<TP, index, Arg>,
                                              match_single_Arg_against_single_Param<TP, index, Arg>>::type arg_checker;

            typedef typename std::conditional<(sizeof...(RemainingArgs) > 0),
                                              compare_Args_against_Params<TP, arg_checker::next_index, RemainingArgs...>,
                                              check_no_remaining_params<TP, arg_checker::next_index>>::type rest_args;
        };

        // Checker to verify that the arguments from the task match those from the dsp kernel
        // If the task argument is a buffer, the check will consume two dsp kernel arguments, one for
        // the address and another for the size.
        // The static check will also fails if the number of arguments differ
        template <typename Fn, typename... Args>
        struct check;

        template <typename Fn>
        struct check<::hetcompute::dsp_kernel<Fn>>
        {
        };

        // Specialized version of the checker for dsp kernels
        template <typename Fn, typename... Args>
        struct check<::hetcompute::dsp_kernel<Fn>, Args...>
        {
            using tuple_params = typename FnParams<Fn>::tuple_params;

            using checker = compare_Args_against_Params<tuple_params, 0, Args...>;
        };

        // Sequence of integer numbers to convert the hetcompute invocation arguments into dsp IDL arguments.
        template <std::uint32_t... numbers>
        struct integer_sequence
        {
            static constexpr size_t get_size() { return sizeof...(numbers); }
        };

        /// The following structs creates an integer sequence from 0 to Top-1.
        template <std::uint32_t Top, std::uint32_t... Sequence>
        struct create_integer_sequence : create_integer_sequence<Top - 1, Top - 1, Sequence...>
        {
        };

        template <std::uint32_t... Sequence>
        struct create_integer_sequence<0, Sequence...>
        {
            using type = integer_sequence<Sequence...>;
        };

        // Helper functions to make the invocation of the dsp function with the converted arguments.
        // It should be called only from dsptask::execute()
        template <typename F, typename ArgsTuple, uint32_t... Number>
        int apply_impl(F& f, ArgsTuple& args, integer_sequence<Number...>)
        {
            return f(std::get<Number>(args)...);
        }

        template <typename F, typename ArgsTuple>
        int apply(F& f, ArgsTuple& args)
        {
            return apply_impl(f, args, typename create_integer_sequence<std::tuple_size<ArgsTuple>::value>::type());
        }

        // helper struct to remove references
        template <typename T>
        struct decay_and_unref
        {
            using type = typename std::remove_reference<typename std::decay<T>::type>::type;
        };

        //
        // Helper functions to fill the buffer acquire set
        // TP represents the tuple of arguments
        // The function of the dsp kernel, Fn, is required to know whether the buffers are read-only
        template <typename BufferAcquireSet, typename TP, size_t tp_index, typename Fn, size_t fn_index, bool TP_index_is_buffer_ptr>
        struct add_buffer_to_acquire_set;

        // Specialization for non buffer parameter
        // Do not add any element into the set because the parameter is not a buffer
        template <typename BufferAcquireSet, typename TP, size_t tp_index, typename Fn, size_t fn_index>
        struct add_buffer_to_acquire_set<BufferAcquireSet, TP, tp_index, Fn, fn_index, false>
        {
            add_buffer_to_acquire_set(BufferAcquireSet&, TP&) {}
        };

        // Specialization for buffer parameter
        // Params always refer to the tuple of parameters of the task
        // Arguments always refer to the dsp kernel function arguments
        template <typename BufferAcquireSet, typename TP, size_t tp_index, typename Fn, size_t fn_index>
        struct add_buffer_to_acquire_set<BufferAcquireSet, TP, tp_index, Fn, fn_index, true>
        {
            add_buffer_to_acquire_set(BufferAcquireSet& bas, TP& tp)
            {
                // get the buffer from the tuple
                using param_type = typename std::tuple_element<tp_index, TP>::type;
                param_type param = std::get<tp_index>(tp);

                // find if the buffer is read-only
                using fn_arg_type                      = typename std::tuple_element<fn_index, typename FnParams<Fn>::tuple_params>::type;
                static constexpr auto buffer_read_only = std::is_const<typename std::remove_pointer<fn_arg_type>::type>::value;
                auto                  acquire_mode =
                    buffer_read_only ? hetcompute::internal::bufferpolicy::acquire_r : hetcompute::internal::bufferpolicy::acquire_rw;

                bas.add(param, acquire_mode);
            }
        };

        // Parse the parameter list and add the buffers to a buffer acquire set
        // TP represents the tuple of arguments
        // tp_index represents the position of the current tuple parameter
        template <typename BufferAcquireSet, typename TP, size_t tp_index, typename Fn, size_t fn_index, bool is_finished>
        struct parse_and_add_buffers_to_acquire_set;

        template <typename BufferAcquireSet, typename TP, size_t tp_index, typename Fn, size_t fn_index>
        struct parse_and_add_buffers_to_acquire_set<BufferAcquireSet, TP, tp_index, Fn, fn_index, false>
        {
            using tuple_type = typename std::tuple_element<tp_index, TP>::type;
            using param_type = typename decay_and_unref<tuple_type>::type;

            static constexpr auto is_current_param_a_buffer_ptr = is_api20_buffer_ptr<param_type>::value;

            static constexpr size_t increment = is_current_param_a_buffer_ptr ? 2 : 1;
            using current_param_type = add_buffer_to_acquire_set<BufferAcquireSet, TP, tp_index, Fn, fn_index, is_current_param_a_buffer_ptr>;

            current_param_type _current_param;

            parse_and_add_buffers_to_acquire_set<BufferAcquireSet, TP, tp_index + 1, Fn, fn_index + increment, tp_index + 1 >= std::tuple_size<TP>::value>
                _rest_params;

            parse_and_add_buffers_to_acquire_set(BufferAcquireSet& bas, TP& tp) : _current_param(bas, tp), _rest_params(bas, tp) {}
        };

        template <typename BufferAcquireSet, typename TP, size_t tp_index, typename Fn, size_t fn_index>
        struct parse_and_add_buffers_to_acquire_set<BufferAcquireSet, TP, tp_index, Fn, fn_index, true>
        {
            parse_and_add_buffers_to_acquire_set(BufferAcquireSet&, TP&) {}
        };

        // Helper functions to convert the arguments
        // Convert a single argument from the hetcompute tuple of arguments TP
        //  to the dsp side.
        // If the argument is a hetcompute::buffer, it is converted to the dsp format.
        /// If the TP argument is not a buffer, it is passed through without any change.
        template <typename Fn, size_t fn_index, typename TP, size_t index, typename BufferAcquireSet, bool TP_index_is_buffer_ptr>
        struct translate_single_TP_arg_to_dsp_tuple;

        /// Non buffer specialization. Pass arguments as they are.
        /// Do not perform any change on them
        template <typename Fn, size_t fn_index, typename TP, size_t index, typename BufferAcquireSet>
        struct translate_single_TP_arg_to_dsp_tuple<Fn, fn_index, TP, index, BufferAcquireSet, false>
        {
            using arg_type = typename std::tuple_element<index, TP>::type;
            std::tuple<arg_type> _htp_at_index;

            translate_single_TP_arg_to_dsp_tuple(TP& tp, BufferAcquireSet&) : _htp_at_index(std::get<index>(tp)) {}
        };

        /// Type trait to get the buffer element type in hetcompute buffers
        template <typename BufferPtr>
        struct buffertraits;

        template <typename T>
        struct buffertraits<::hetcompute::buffer_ptr<T>>
        {
            using element_type = T;
            using api20        = std::true_type;
        };

        //
        // Templates for buffer specialization. Convert a hetcompute buffer into its initial address and size
        // The construction of this class guarantees that the data is ready for consumption
        // executing the buffer sync() method.

        template <typename Fn, size_t fn_index, typename TP, size_t index, typename BufferAcquireSet>
        struct translate_single_TP_arg_to_dsp_tuple<Fn, fn_index, TP, index, BufferAcquireSet, true>
        {
            // type of the buffer
            using buffer_type = typename std::decay<typename std::tuple_element<index, TP>::type>::type;
            // type of the elements stored by the buffer
            using element_type = typename buffertraits<buffer_type>::element_type;
            std::tuple<element_type*, size_t> _htp_at_index;

            // check if the buffer is read-only or not
            // The IDL adds const to the in sequences, so if the dsp kernel function has a buffer
            // with const we assume it's read only and, hence, acquire_r for the buffer
            using fn_arg_type = typename std::tuple_element<fn_index, typename FnParams<Fn>::tuple_params>::type;
            // We need to remove the pointer because otherwise is_const returns false

            template <typename T>
            element_type* get_data(::hetcompute::buffer_ptr<T> b, BufferAcquireSet& bas)
            {
                // this functions is called after the blocking call to acquire the buffers, so it is
                // guaranteed that all buffers are already acquired at this point. Otherwise, we need to abort
                // the execution because the pointer we are going to acquire may not be valid
                HETCOMPUTE_INTERNAL_ASSERT(bas.acquired(), "Buffers should be already acquired");

                // get the arena
                auto acquired_arena = bas.find_acquired_arena(b);

                auto ion_ptr = arena_storage_accessor::access_ion_arena_for_dsptask(acquired_arena);
                return static_cast<element_type*>(ion_ptr);
            }

            template <typename T>
            size_t get_size(::hetcompute::buffer_ptr<T> b)
            {
                return b.size();
            }

            translate_single_TP_arg_to_dsp_tuple(TP& tp, BufferAcquireSet& bas)
                : _htp_at_index(std::make_tuple(get_data(std::get<index>(tp), bas), get_size(std::get<index>(tp))))
            {
            }
        };

        // Perform the buffer conversion for the argument tuple
        // We also use the dsp kernel function to detect if the argument is const in
        // the function signature in which case we assume the buffer is read only
        template <typename Fn, size_t fn_index, typename TP, size_t index, typename BufferAcquireSet, bool is_finished>
        struct translate_TP_args_to_dsp;

        /// Conversion of a single element of the arguments and continue the conversion for the next
        /// argument in the tuple
        template <typename Fn, size_t fn_index, typename TP, size_t index, typename BufferAcquireSet>
        struct translate_TP_args_to_dsp<Fn, fn_index, TP, index, BufferAcquireSet, false>
        {
            using tuple_type = typename std::tuple_element<index, TP>::type;
            // type of the current argument
            using element_type = typename decay_and_unref<tuple_type>::type;

            // check whether the current argument is a buffer
            static bool constexpr is_index_arg_a_buffer_ptr = ::hetcompute::internal::is_buffer_ptr<element_type>::value;

            // translate the current argument
            translate_single_TP_arg_to_dsp_tuple<Fn, fn_index, TP, index, BufferAcquireSet, is_index_arg_a_buffer_ptr> _dsp_container_for_index;
            // translate the next arguments
            using rest_type = translate_TP_args_to_dsp<Fn,
                                                           is_index_arg_a_buffer_ptr ? fn_index + 2 : fn_index + 1,
                                                           TP,
                                                           index + 1,
                                                           BufferAcquireSet,
                                                           index + 1 >= std::tuple_size<TP>::value>;
            rest_type _rest;

            using dsp_tuple_till_index = decltype(std::tuple_cat(_dsp_container_for_index._htp_at_index, _rest._htp_till_index));

            // contains a tuple of dsp arguments converted from tp<index .. end>
            // this tuple is built as the argument list is traversed
            dsp_tuple_till_index _htp_till_index;

            translate_TP_args_to_dsp(TP& tp, BufferAcquireSet& bas)
                : _dsp_container_for_index(tp, bas),
                  _rest(tp, bas),
                  _htp_till_index(std::tuple_cat(_dsp_container_for_index._htp_at_index, _rest._htp_till_index))
            {
            }
        };

        /// Terminate the conversion and return and empty list when there are not more
        /// arguments to process
        template <typename Fn, size_t fn_index, typename TP, size_t index, typename BufferAcquireSet>
        struct translate_TP_args_to_dsp<Fn, fn_index, TP, index, BufferAcquireSet, true>
        {
            std::tuple<> _htp_till_index;

            translate_TP_args_to_dsp(TP&, BufferAcquireSet&) : _htp_till_index() {}
        };

    }; // namespace internal
};     // namespace hetcompute

#endif // defined(HETCOMPUTE_HAVE_QTI_DSP)
