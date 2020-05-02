#pragma once

#if defined(HETCOMPUTE_HAVE_QTI_DSP)

#include <hetcompute/internal/task/dspkernel.hh>
#include <hetcompute/internal/task/internal_taskfactory.hh>

namespace hetcompute
{
    /** @addtogroup kernelclass_doc
        @{ */

    template <typename Fn>
    class dsp_kernel;

    /**
     *  For this DSP kernel, the template signature corresponds to
     *  the DSP kernel's parameter list.
     *
     *  @tparam Args Arguments of the DSP function run by the kernel.
     *
     *  @sa create_dsp_kernel() for creating a <tt>dsp_kernel</tt>.
     */

    template <typename... Args>
    class dsp_kernel<int (*)(Args...)> : public ::hetcompute::internal::dsp_kernel<Args...>
    {
        using parent = ::hetcompute::internal::dsp_kernel<Args...>;

        template <typename X, typename Y>
        friend struct ::hetcompute::internal::task_factory_dispatch;

        template <typename X, typename Y, typename Z>
        friend struct ::hetcompute::internal::task_factory;

    public:
        using size_type   = typename parent::size_type;
        using return_type = typename parent::return_type;
        using args_tuple  = typename parent::args_tuple;

        using fn_type = typename parent::dsp_code_type;

        using collapsed_task_type     = typename parent::collapsed_task_type;
        using non_collapsed_task_type = typename parent::non_collapsed_task_type;

        static HETCOMPUTE_CONSTEXPR_CONST size_type arity = parent::arity;

        static_assert(std::is_same<return_type, int>::value == true,
                      "Functions for "
                      "Hexagon kernels must return int");

        /**
         * Constructor
         *
         * @param fn The dsp function to be called.
         */
        explicit dsp_kernel(fn_type const& fn) : parent(fn) {}

        /**
         * Constructor
         *
         * @param fn The DSP function to be called.
         */
        explicit dsp_kernel(fn_type&& fn) : parent(std::forward<fn_type>(fn)) {}

        /*
         * Copy constructor.
         */
        dsp_kernel(dsp_kernel const& other) : parent(other) {}

        /**
         * Move constructor.
         */
        dsp_kernel(dsp_kernel&& other) : parent(std::move(other)) {}

        /**
         * Equality operator.
         */
        dsp_kernel& operator=(dsp_kernel const& other) { return static_cast<dsp_kernel&>(parent::operator=(other)); }

        /**
         * Inequality operator.
         */
        dsp_kernel& operator=(dsp_kernel&& other) { return static_cast<dsp_kernel&>(parent::operator=(std::move(other))); }

        /**
         * Destructor.
         */
        ~dsp_kernel() {}
    }; // dsp_kernel<int(*)(Args...)>

    // static member definition for dsp_kernel<Fn>::arity
    template <typename... Args>
    HETCOMPUTE_CONSTEXPR_CONST typename dsp_kernel<int (*)(Args...)>::size_type dsp_kernel<int (*)(Args...)>::arity;

    /**
     * This template creates a DSP kernel executable by Qualcomm HetCompute SDK.
     * The template signature corresponds to the DSP kernel parameter list.
     *
     * The kernel code is specified as a C language function. The function returns
     * an int that corresponds to the status. When the function returns something
     * other than 0, the Qualcomm HetCompute runtime will trigger a hetcompute::dsp_exception().
     *
     * @tparam dsp_function The DSP function pointer.
     *
     * @snippet samples/src/hexagon_is_prime.cc EXAMPLE
     */
    template <typename... Args>
    hetcompute::dsp_kernel<int (*)(Args...)> create_dsp_kernel(int (*fn)(Args...))
    {
        return hetcompute::dsp_kernel<int (*)(Args...)>(fn);
    }

    /** @} */ /* end_addtogroup kernelclass_doc */

}; // namespace hetcompute

#endif // defined(HETCOMPUTE_HAVE_QTI_DSP)
