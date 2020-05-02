#pragma once

#include <string>
#include <hetcompute/internal/pointkernel/pk_util.hh>

namespace hetcompute
{
    namespace pattern
    {
        template <typename T1, typename T2>
        class pfor;
    }; // namespace pattern

    namespace internal
    {
        namespace pointkernel
        {
            // forward declaration
            template <typename RT, typename... Args>
            class pointkernel;
        };
        // forward declaration
        template <typename ReturnType, typename... Args, typename ArgTuple, size_t Dims, size_t... Indices>
        void pfor_each_run_helper(const hetcompute::range<Dims>&                   r,
                                  pointkernel::pointkernel<ReturnType, Args...>& pk,
                                  const hetcompute::pattern::tuner&                t,
                                  hetcompute::internal::integer_list_gen<Indices...>,
                                  ArgTuple& atpl);

        namespace testing
        {
            // Forward declaration
            class pk_accessor;
        }; // namespace testing

#if defined(HETCOMPUTE_HAVE_QTI_DSP)
        // TODO: change to g_num_dsp_exec_ctx once we fix fast-rpc problem in dsp
        inline static constexpr size_t get_num_dsp_threads() { return 1; }
#endif // defined(HETCOMPUTE_HAVE_QTI_DSP)

        namespace pointkernel
        {
            template <typename ReturnType, typename... Args>
            class pointkernel
            {
                using cpu_kernel_type = typename cpu_kernel_generator<ReturnType, typename make_cpu_kernel_args<Args...>::type>::type;
                using cpu_kernel_signature_type =
                    typename cpu_kernel_signature_generator<ReturnType, typename make_cpu_kernel_args<Args...>::type>::type;

#if defined(HETCOMPUTE_HAVE_OPENCL)
                using gpu_kernel_args        = typename replace_pointer_size_pair_with_bufferptr<Args...>::type;
                using gpu_kernel_type        = typename gpu_kernel_generator<gpu_kernel_args>::type;
                using gpu_output_buffer_type = typename get_output_buffer_type<gpu_kernel_args>::type;
#endif

#if defined(HETCOMPUTE_HAVE_QTI_DSP)
                using dsp_kernel_args        = typename make_dsp_kernel_args<Args...>::type;
                using dsp_kernel_signature   = typename dsp_kernel_signature_generator<dsp_kernel_args>::type;
                using dsp_kernel_type        = typename dsp_kernel_generator<dsp_kernel_args>::type;
                using dsp_output_buffer_type = typename get_dsp_output_buffer_type<Args...>::type;
#endif

            public:
                pointkernel()
                    : _cpu_kernel()
#if defined(HETCOMPUTE_HAVE_OPENCL)
                      ,
                      _gpu_kernel_name(),
                      _gpu_kernel_string()
#endif
#if defined(HETCOMPUTE_HAVE_QTI_DSP)
                      ,
                      _dsp_kernel()
#endif
                {
                }

                pointkernel(cpu_kernel_signature_type fn
#if defined(HETCOMPUTE_HAVE_OPENCL)
                            ,
                            std::string kernel_name,
                            std::string kernel_string
#endif
#if defined(HETCOMPUTE_HAVE_QTI_DSP)
                            ,
                            dsp_kernel_signature dsp_fn
#endif
                            )
                    : _cpu_kernel(hetcompute::create_cpu_kernel<>(fn))
#if defined(HETCOMPUTE_HAVE_OPENCL)
                      ,
                      _gpu_kernel(kernel_string,
                                  kernel_name
                // Define the floating point variants of the point kernel math
                // functions for OpenCL. Should be a separate static const string, but
                // due to C++ template-class restrictions, it is inconvenient to make
                // it so.
#ifdef __aarch64__
                                  ,
                                  "-Dcbrtf=cbrt "
                                  "-Dcosf=cos "
                                  "-Dcoshf=cosh "
                                  "-Dcospif=cospi "
                                  "-Derfcf=erfc "
                                  "-Derff=erf "
                                  "-Dexpf=exp "
                                  "-Dexp2f=exp2 "
                                  "-Dexpm1f=expm1 "
                                  "-Dfabsf=fabs "
                                  "-Dfdimf=fdim "
                                  "-Dfloorf=floor "
                                  "-Dfmaf=fma "
                                  "-Dfmaxf=fmax "
                                  "-Dfminf=fmin "
                                  "-Dfmodf=fmod "
                                  "-Dfractf=fract "
                                  "-Dfrexpf=frexp "
                                  "-Dhypotf=hypot "
                                  "-Dldexpf=ldexp "
                                  "-Dlgammaf=lgamma "
                                  "-Dlogf=logf "
                                  "-Dlog2f=log2 "
                                  "-Dlog10f=log10 "
                                  "-Dlog1pf=log1p "
                                  "-Dlogbf=logb "
                                  "-Dmadf=mad "
                                  "-Dmaxmagf=maxmag "
                                  "-Dminmagf=minmag "
                                  "-Dmodff=modf "
                                  "-Dnextafterf=nextafter "
                                  "-Dpowf=pow "
                                  "-Dpownf=pown "
                                  "-Dpowrf=powr "
                                  "-Dremainderf=remainder "
                                  "-Dremquof=remquo "
                                  "-Drintf=rint "
                                  "-Drootnf=rootn "
                                  "-Droundf=round "
                                  "-Dsqrtf=sqrt "
                                  "-Drsqrtf=rsqrt "
                                  "-Dsinf=sin "
                                  "-Dsincosf=sincos "
                                  "-Dsinhf=sinh "
                                  "-Dsinpif=sinpi "
                                  "-Dtanf=tan "
                                  "-Dtanhf=tanh "
                                  "-Dtanpif=tanpi "
                                  "-Dtgammaf=tgamma "
                                  "-Dtruncf=trunc "
#endif
                                  ),
                      _gpu_kernel_name(kernel_name),
                      _gpu_kernel_string(kernel_string),
                      _gpu_local_buffer()
#endif
#if defined(HETCOMPUTE_HAVE_QTI_DSP)
                      ,
                      _dsp_kernel(hetcompute::create_dsp_kernel<>(dsp_fn)),
                      _dsp_local_buffer_vec(get_num_dsp_threads())
#endif
                {
                }

#if defined(HETCOMPUTE_HAVE_OPENCL)
                gpu_output_buffer_type get_gpu_local_buffer() { return _gpu_local_buffer; }
                void                   set_gpu_local_buffer(gpu_output_buffer_type buf) { _gpu_local_buffer = buf; }
#endif

                // Use default copy constructor, move constructor, copy assignment and move assignment operators.
                HETCOMPUTE_DEFAULT_METHOD(pointkernel(pointkernel const&));
                HETCOMPUTE_DEFAULT_METHOD(pointkernel(pointkernel&&));
                HETCOMPUTE_DEFAULT_METHOD(pointkernel& operator=(pointkernel const&));
                HETCOMPUTE_DEFAULT_METHOD(pointkernel& operator=(pointkernel&&));

                friend class ::hetcompute::internal::testing::pk_accessor;

                template <typename RT, typename... PKArgs, typename ArgTuple, size_t Dims, size_t... Indices>
                friend void hetcompute::internal::pfor_each_run_helper(
                    hetcompute::pattern::pfor<hetcompute::internal::pointkernel::pointkernel<RT, PKArgs...>, ArgTuple>* const p,
                    const hetcompute::range<Dims>&                                                                          r,
                    hetcompute::internal::pointkernel::pointkernel<RT, PKArgs...>&                                          pk,
                    hetcompute::pattern::tuner&                                                                             t,
                    hetcompute::internal::integer_list_gen<Indices...>,
                    ArgTuple& atpl);

            private:
                cpu_kernel_type _cpu_kernel;
#if defined(HETCOMPUTE_HAVE_OPENCL)
                gpu_kernel_type        _gpu_kernel;
                std::string            _gpu_kernel_name;
                std::string            _gpu_kernel_string;
                gpu_output_buffer_type _gpu_local_buffer;

#endif

#if defined(HETCOMPUTE_HAVE_QTI_DSP)
                dsp_kernel_type                     _dsp_kernel;
                std::vector<dsp_output_buffer_type> _dsp_local_buffer_vec;
#endif
            };  // class pointkernel

        }; // namespace pointkernel
    }; // namespace internal
}; // namespace hetcompute
