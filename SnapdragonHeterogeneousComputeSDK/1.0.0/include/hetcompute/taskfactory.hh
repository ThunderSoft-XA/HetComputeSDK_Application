#pragma once

// Include user-visible headers first
#include <hetcompute/cpukernel.hh>
#include <hetcompute/gpukernel.hh>
#include <hetcompute/groupptr.hh>
#include <hetcompute/dspkernel.hh>
#include <hetcompute/taskptr.hh>

// Include internal headers next
#include <hetcompute/internal/task/operators.hh>
#include <hetcompute/internal/util/templatemagic.hh>

namespace hetcompute
{
    /** @addtogroup taskclass_doc
    @{ */

    /**
     * Helper struct for specifying do_not_collapse task.
     */
    struct do_not_collapse_t
    {
    };

    /**
     * Object of type do_not_collapse_t.
     */
    const do_not_collapse_t do_not_collapse{};

    /**
     * Collapsed task type.
     */
    template <typename Fn>
    using collapsed_task_type = typename ::hetcompute::internal::task_factory<Fn>::collapsed_task_type;

    /**
     * Non-collapsed task type.
     */
    template <typename Fn>
    using non_collapsed_task_type = typename ::hetcompute::internal::task_factory<Fn>::non_collapsed_task_type;

    /**
     * Create a value task.
     *
     * The task needs to be launched and will return immediately.
     *
     * @tparam ReturnType Return type of the task.
     * @tparam Args...    Parameter types of the task.
     *
     * @param args     Arguments used to construct an object of type
     *                    <code>ReturnType</code>.
     *
     * @return <code>hetcompute::task_ptr<ReturnType></code> whose return value is
     *         an object of type  <code>ReturnType</code> constructed with
     *         <code>args...</code>.
     *
     * @includelineno samples/src/create_value_task.cc
     */
    template <typename ReturnType, typename... Args>
    ::hetcompute::task_ptr<ReturnType> create_value_task(Args&&... args)
    {
        auto t = ::hetcompute::internal::cputask_factory::create_value_task<ReturnType>(std::forward<Args>(args)...);
        return ::hetcompute::task_ptr<ReturnType>(t, ::hetcompute::internal::task_shared_ptr::ref_policy::no_initial_ref);
    }

    /**
     * Create a collapsed task out of <code>Code</code> and (optionally) bind all
     * arguments.
     *
     * @tparam Code Type of the work for this task.
     * @tparam Args...  Parameter types of the task.
     *
     * @param code  The work for the task. <code>code</code> can be Qualcomm HetCompute kernels
     *              (CPU, GPU, or DSP), a lambda expression, a function object, or a
     *              function pointer.
     * @param args  Argument used to bind to the task (only supported by CPU tasks).
     *              If left empty, no arguments will be bound to the task.
     *
     * @return <code>hetcompute::task_ptr<ReturnType(Params...)></code>, the task_ptr with
     *         full function signature.
     *
     * @includelineno samples/src/create_collapsed_task.cc
     *
     * @sa <code>hetcompute::task<ReturnType(Args...)>::bind_all</code>
     */
    template <typename Code, typename... Args>
    collapsed_task_type<Code> create_task(Code&& code, Args&&... args)
    {
        using factory     = ::hetcompute::internal::task_factory<Code>;
        auto raw_task_ptr = factory::create_collapsed_task(std::forward<Code>(code), std::forward<Args>(args)...);
        return collapsed_task_type<Code>(raw_task_ptr, ::hetcompute::internal::task_shared_ptr::ref_policy::no_initial_ref);
    }

    // Helper function for poly kernel create_task
    namespace internal
    {
        template <typename... Args>
        void create_poly_task_impl(group*, task*, std::tuple<>&&, Args&&...)
        {
        }

        // Helper function for poly kernel create_task
        template <typename Code1, typename... Codes, typename... Args>
        void create_poly_task_impl(group* g, task* t, std::tuple<Code1, Codes...>&& codes, Args&&... args)
        {
            using factory1     = task_factory<Code1>;
            auto raw_task_ptr1 = factory1::create_collapsed_task(std::forward<Code1>(std::get<0>(codes)), std::forward<Args>(args)...);
            t->add_alternative(raw_task_ptr1);
            raw_task_ptr1->join_group(g, false);
            create_poly_task_impl(g, t, tuple_rest(codes), std::forward<Args>(args)...);
        }

        template <typename Code1, typename... Codes, typename... Args>
        task* create_poly_task(std::tuple<Code1, Codes...>&& codes, Args&&... args)
        {
            using factory1 = task_factory<Code1>;

            auto raw_task_ptr1 = factory1::create_collapsed_task(std::forward<Code1>(std::get<0>(codes)), std::forward<Args>(args)...);

            // Only group to which alternative tasks will belong
            auto g     = hetcompute::create_group();
            auto g_ptr = c_ptr(g);

            create_poly_task_impl(g_ptr, raw_task_ptr1, tuple_rest(codes), std::forward<Args>(args)...);

            // The user-facing task, raw_task_ptr1, will be set as the representative
            // task of the group, and transitively, as the representative task of the
            // alternatives. Consequently, any cancelation of the user-facing task will
            // automatically be seen by any dispatched alternative task.
            g_ptr->set_representative_task(raw_task_ptr1);

            return raw_task_ptr1;
        }

        // un-collapsed variants of the above functions
        template <typename... Args>
        void create_non_collapsed_poly_task_impl(group*, task*, std::tuple<>&&, Args&&...)
        {
        }

        // Helper function for poly kernel create_task
        template <typename Code1, typename... Codes, typename... Args>
        void create_non_collapsed_poly_task_impl(group* g, task* t, std::tuple<Code1, Codes...>&& codes, Args&&... args)
        {
            using factory1     = task_factory<Code1>;
            auto raw_task_ptr1 = factory1::create_non_collapsed_task(std::forward<Code1>(std::get<0>(codes)), std::forward<Args>(args)...);
            t->add_alternative(raw_task_ptr1);
            raw_task_ptr1->join_group(g, false);
            create_non_collapsed_poly_task_impl(g, t, tuple_rest(codes), std::forward<Args>(args)...);
        }

        template <typename Code1, typename... Codes, typename... Args>
        task* create_non_collapsed_poly_task(std::tuple<Code1, Codes...>&& codes, Args&&... args)
        {
            using factory1 = task_factory<Code1>;

            auto raw_task_ptr1 = factory1::create_non_collapsed_task(std::forward<Code1>(std::get<0>(codes)), std::forward<Args>(args)...);

            // Only group to which alternative tasks will belong
            auto g     = hetcompute::create_group();
            auto g_ptr = c_ptr(g);

            create_non_collapsed_poly_task_impl(g_ptr, raw_task_ptr1, tuple_rest(codes), std::forward<Args>(args)...);

            // The user-facing task, raw_task_ptr1, will be set as the representative
            // task of the group, and transitively, as the representative task of the
            // alternatives. Consequently, any cancelation of the user-facing task will
            // automatically be seen by any dispatched alternative task.
            g_ptr->set_representative_task(raw_task_ptr1);

            return raw_task_ptr1;
        }
    }; // namespace internal

    namespace beta
    {
        /**
         * Create task from a poly kernel.
         *
         * All kernels must implement the same interface.
         *
         * @param codes tuple of kernels (cpu/gpu/hexagon) of which exactly one will be executed
         * @param args arguments to the task
         * @return <code>hetcompute::task_ptr<ReturnType(Params...)></code>, the task_ptr with
         *         full function signature.
         *
         * @includelineno samples/src/polykernel.cc
         */
        template <typename Code1, typename... Codes, typename... Args>
        collapsed_task_type<Code1> create_task(std::tuple<Code1, Codes...>&& codes, Args&&... args)
        {
            auto raw_task_ptr1 = ::hetcompute::internal::create_poly_task(std::move(codes), std::forward<Args>(args)...);
            return collapsed_task_type<Code1>(raw_task_ptr1, ::hetcompute::internal::task_shared_ptr::ref_policy::no_initial_ref);
        }
    }; // namespace beta

    /**
     * Create a non-collapsed task out of <code>Code</code> and (optionally) bind all
     * arguments.
     *
     * @tparam Code Type of the work for this task.
     * @tparam Args...  Parameter types of the task.
     *
     * @param code  The work for the task. <code>code</code> can be Qualcomm HetCompute kernels
     *              (CPU, GPU, or DSP), a lambda expression, a function object, or
     *              a function pointer.
     * @param args  Argument used to bind to the task (only supported by CPU tasks).
     *              If left empty, no arguments will be bound to the task.
     *
     * @return <code>hetcompute::task_ptr<ReturnType(Params...)></code>, the task_ptr with
     *         full function signature.
     *
     * @includelineno samples/src/create_noncollapsed_task.cc
     *
     * @sa <code>hetcompute::task<ReturnType(Args...)>::bind_all</code>
     */
    template <typename Code, typename... Args>
    non_collapsed_task_type<Code> create_task(do_not_collapse_t, Code&& code, Args&&... args)
    {
        using factory     = ::hetcompute::internal::task_factory<Code>;
        auto raw_task_ptr = factory::create_non_collapsed_task(std::forward<Code>(code), std::forward<Args>(args)...);
        return non_collapsed_task_type<Code>(raw_task_ptr, ::hetcompute::internal::task_shared_ptr::ref_policy::no_initial_ref);
    }

    namespace beta
    {
        /**
         * Create a non-collapsed task from a poly kernel.
         *
         * All kernels must implement the same interface.
         *
         * @tparam Code1 Type of the first kernel for this task.
         * @tparam Codes Types of the rest of the kernels for this task.
         * @tparam Args...  Parameter types of the task.
         *
         * @param codes tuple of kernels (cpu/gpu/hexagon) of which exactly one will be executed
         * @param args arguments to the task
         * @return <code>hetcompute::task_ptr<ReturnType(Params...)></code>, the task_ptr with
         *         full function signature.
         */
        template <typename Code1, typename... Codes, typename... Args>
        non_collapsed_task_type<Code1> create_task(do_not_collapse_t, std::tuple<Code1, Codes...>&& codes, Args&&... args)
        {
            auto raw_task_ptr1 = ::hetcompute::internal::create_non_collapsed_poly_task(std::move(codes), std::forward<Args>(args)...);
            return non_collapsed_task_type<Code1>(raw_task_ptr1, ::hetcompute::internal::task_shared_ptr::ref_policy::no_initial_ref);
        }
    }; // namespace beta

    /**
     * Create a collapsed task out of <code>Code</code>, bind all arguments, if any
     * exist (mandatory), and launch the task.
     *
     * @tparam Code Type of the work for this task.
     * @tparam Args... Parameter types of the task.
     *
     * @param code  The work for the task. <code>code</code> can be Qualcomm HetCompute kernels
     *              (CPU, GPU, or DSP), a lambda expression, a function object,
     *              or a function pointer.
     * @param args  Argument used to bind to the task (only supported by CPU
     *              tasks). If left empty, no arguments will be bound to the task.
     *
     * @return <code>hetcompute::task_ptr<ReturnType(Params...)></code>, the task_ptr with
     *         full function signature.
     *
     * @includelineno samples/src/launch_collapsed_task.cc
     *
     * @sa <code>hetcompute::create_task(Code&&, Args&&...)</code>
     */
    template <typename Code, typename... Args>
    collapsed_task_type<Code> launch(Code&& code, Args&&... args)
    {
        using factory     = ::hetcompute::internal::task_factory<Code>;
        auto raw_task_ptr = factory::create_collapsed_task(std::forward<Code>(code), std::forward<Args>(args)...);
        raw_task_ptr->launch(nullptr, nullptr);
        return collapsed_task_type<Code>(raw_task_ptr, ::hetcompute::internal::task_shared_ptr::ref_policy::no_initial_ref);
    }

    namespace beta
    {
        /**
         * Create a collapsed task from a poly-kernel, bind all arguments, if any exist
         * (mandatory), and launch the task.
         *
         * All kernels must implement the same interface.
         *
         * @tparam Code1 Type of the first kernel for this task.
         * @tparam Codes Types of the rest of the kernels for this task.
         * @tparam Args...  Parameter types of the task.
         *
         * @param codes tuple of kernels (cpu/gpu/hexagon) of which exactly one will be executed
         * @param args arguments to the task
         * @return <code>hetcompute::task_ptr<ReturnType(Params...)></code>, the task_ptr with
         *         full function signature.
         */
        template <typename Code1, typename... Codes, typename... Args>
        collapsed_task_type<Code1> launch(std::tuple<Code1, Codes...>&& codes, Args&&... args)
        {
            auto raw_task_ptr1 = ::hetcompute::internal::create_poly_task(std::move(codes), std::forward<Args>(args)...);
            raw_task_ptr1->launch(nullptr, nullptr);
            return collapsed_task_type<Code1>(raw_task_ptr1, ::hetcompute::internal::task_shared_ptr::ref_policy::no_initial_ref);
        }
    }; // namespace beta

    /**
     * Create a non-collapsed task out of <code>Code</code>, bind all arguments, if
     * any exist (mandatory), and launch the task.
     *
     * @tparam Code Type of work for this task.
     * @tparam Args...  Parameter types of the task.
     *
     * @param code  The work for the task. <code>code</code> can be Qualcomm HetCompute kernels
     *              (CPU, GPU, or DSP), a lambda expression, a function object, or
     *              a function pointer.
     * @param args  Argument used to bind to the task (only supported by CPU tasks).
     *              If left empty, no arguments will be bound to the task.
     *
     * @return <code>hetcompute::task_ptr<ReturnType(Params...)></code>, the task_ptr with
     *         full function signature.
     *
     * @includelineno samples/src/launch_noncollapsed_task.cc
     *
     * @sa <code>hetcompute::create_task(do_not_collapse_t, Code&&, Args&&...)</code>
     */
    template <typename Code, typename... Args>
    non_collapsed_task_type<Code> launch(do_not_collapse_t, Code&& code, Args&&... args)
    {
        using factory     = ::hetcompute::internal::task_factory<Code>;
        auto raw_task_ptr = factory::create_non_collapsed_task(std::forward<Code>(code), std::forward<Args>(args)...);
        raw_task_ptr->launch(nullptr, nullptr);
        return non_collapsed_task_type<Code>(raw_task_ptr, ::hetcompute::internal::task_shared_ptr::ref_policy::no_initial_ref);
    }

    namespace beta
    {
        /**
         * Create a non-collapsed task from a poly-kernel, bind all arguments, if any
         * exist (mandatory), and launch the task.
         *
         * All kernels must implement the same interface.
         *
         * @tparam Code1 Type of the first kernel for this task.
         * @tparam Codes Types of the rest of the kernels for this task.
         * @tparam Args...  Parameter types of the task.
         *
         * @param codes tuple of kernels (cpu/gpu/hexagon) of which exactly one will be executed
         * @param args arguments to the task
         * @return <code>hetcompute::task_ptr<ReturnType(Params...)></code>, the task_ptr with
         *         full function signature.
         */
        template <typename Code1, typename... Codes, typename... Args>
        non_collapsed_task_type<Code1> launch(do_not_collapse_t, std::tuple<Code1, Codes...>&& codes, Args&&... args)
        {
            auto raw_task_ptr1 = ::hetcompute::internal::create_non_collapsed_poly_task(std::move(codes), std::forward<Args>(args)...);
            raw_task_ptr1->launch(nullptr, nullptr);
            return non_collapsed_task_type<Code1>(raw_task_ptr1, ::hetcompute::internal::task_shared_ptr::ref_policy::no_initial_ref);
        }
    };        // namespace beta
    /** @} */ /* end_addtogroup taskclass_doc */

    // Operators for task_ptr<ReturnType>
    //
    //  unary arithmetic and bitwise operator                      -- yes
    //  Binary arithmetic and bitwise operator                     -- yes
    //  Binary compound arithmetic and bitwise assignment operator -- yes
    //  Comparison and logical operator                            -- no (ambiguous)
    //
    // unary operators for task_ptr<ReturnType>
    HETCOMPUTE_UNARY_TASKPTR_OP(+)
    HETCOMPUTE_UNARY_TASKPTR_OP(-)
    HETCOMPUTE_UNARY_TASKPTR_OP(~)

    // binary operators for task_ptr<ReturnType>
    // task_ptr op task_ptr
    HETCOMPUTE_BINARY_TASKPTR_OP_2PTRS(+)
    HETCOMPUTE_BINARY_TASKPTR_OP_2PTRS(-)
    HETCOMPUTE_BINARY_TASKPTR_OP_2PTRS(*)
    HETCOMPUTE_BINARY_TASKPTR_OP_2PTRS(/)
    HETCOMPUTE_BINARY_TASKPTR_OP_2PTRS(%)
    HETCOMPUTE_BINARY_TASKPTR_OP_2PTRS(&)
    HETCOMPUTE_BINARY_TASKPTR_OP_2PTRS(|)
    HETCOMPUTE_BINARY_TASKPTR_OP_2PTRS (^)

    // task_ptr op value
    HETCOMPUTE_BINARY_TASKPTR_OP_PTR_VALUE(+)
    HETCOMPUTE_BINARY_TASKPTR_OP_PTR_VALUE(-)
    HETCOMPUTE_BINARY_TASKPTR_OP_PTR_VALUE(*)
    HETCOMPUTE_BINARY_TASKPTR_OP_PTR_VALUE(/)
    HETCOMPUTE_BINARY_TASKPTR_OP_PTR_VALUE(%)
    HETCOMPUTE_BINARY_TASKPTR_OP_PTR_VALUE(&)
    HETCOMPUTE_BINARY_TASKPTR_OP_PTR_VALUE(|)
    HETCOMPUTE_BINARY_TASKPTR_OP_PTR_VALUE (^)

    // value op task_ptr
    HETCOMPUTE_BINARY_TASKPTR_OP_VALUE_PTR(+)
    HETCOMPUTE_BINARY_TASKPTR_OP_VALUE_PTR(-)
    HETCOMPUTE_BINARY_TASKPTR_OP_VALUE_PTR(*)
    HETCOMPUTE_BINARY_TASKPTR_OP_VALUE_PTR(/)
    HETCOMPUTE_BINARY_TASKPTR_OP_VALUE_PTR(%)
    HETCOMPUTE_BINARY_TASKPTR_OP_VALUE_PTR(&)
    HETCOMPUTE_BINARY_TASKPTR_OP_VALUE_PTR(|)
    HETCOMPUTE_BINARY_TASKPTR_OP_VALUE_PTR (^)

    // compound assignment operators for task_ptr<ReturnType>
    HETCOMPUTE_TASKPTR_COMPOUND_ASSIGNMENT_OPERATOR(+)
    HETCOMPUTE_TASKPTR_COMPOUND_ASSIGNMENT_OPERATOR(-)
    HETCOMPUTE_TASKPTR_COMPOUND_ASSIGNMENT_OPERATOR(*)
    HETCOMPUTE_TASKPTR_COMPOUND_ASSIGNMENT_OPERATOR(/)
    HETCOMPUTE_TASKPTR_COMPOUND_ASSIGNMENT_OPERATOR(%)
    HETCOMPUTE_TASKPTR_COMPOUND_ASSIGNMENT_OPERATOR(&)
    HETCOMPUTE_TASKPTR_COMPOUND_ASSIGNMENT_OPERATOR(|)
    HETCOMPUTE_TASKPTR_COMPOUND_ASSIGNMENT_OPERATOR (^)
}; // namespace hetcompute
