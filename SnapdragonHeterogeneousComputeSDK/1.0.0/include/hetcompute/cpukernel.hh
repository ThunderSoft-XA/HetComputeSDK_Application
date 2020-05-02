#pragma once

// Include internal headers
#include <hetcompute/internal/legacy/attr.hh>
#include <hetcompute/internal/task/cpukernel.hh>

namespace hetcompute
{
    namespace internal
    {
        // Forward declarations so we can friend them
        template <typename X, typename Y>
        struct task_factory_dispatch;

        template <typename X, typename Y, typename Z>
        struct task_factory;

        struct cpu_kernel_caller;

    }; // namespace hetcompute::internal

    /**
     * @addtogroup kernelclass_doc
     * @{
     */

    /**
     * @brief A wrapper around a function object
     *
     * A <code>cpu_kernel</code> object contains CPU executable code. It can be
     * used to create tasks. When such a task runs, it executes the function
     * object in its <code>cpu_kernel</code>.
     *
     * @sa cpu_kernel<FReturnType(FArgs...)>
     */
    template <typename Fn>
    class cpu_kernel : protected ::hetcompute::internal::cpu_kernel<Fn>
    {
        using parent = ::hetcompute::internal::cpu_kernel<Fn>;

        template <typename X, typename Y>
        friend struct ::hetcompute::internal::task_factory_dispatch;

        template <typename X, typename Y, typename Z>
        friend struct ::hetcompute::internal::task_factory;

        friend struct ::hetcompute::internal::cpu_kernel_caller;

        static_assert(std::is_copy_constructible<Fn>::value, "CPU kernels must be copy constructible.");

        static_assert(std::is_move_constructible<Fn>::value, "CPU kernels must be move constructible.");

    public:
        using size_type   = typename parent::size_type;
        using return_type = typename parent::return_type;
        using args_tuple  = typename parent::args_tuple;

        using collapsed_task_type     = typename parent::collapsed_task_type;
        using non_collapsed_task_type = typename parent::non_collapsed_task_type;

        static HETCOMPUTE_CONSTEXPR_CONST size_type arity = parent::arity;

        /**
         * @brief Constructor
         *
         * @param fn An lvalue function object.
         */
        explicit cpu_kernel(Fn const& fn) : parent(fn) {}

        /**
         * @brief Constructor
         *
         * @param fn An rvalue function object.
         */
        explicit cpu_kernel(Fn&& fn) : parent(std::forward<Fn>(fn)) {}

        /**
         * @brief Copy constructor
         *
         * @param other Another <code>cpu_kernel</code> object.
         */
        cpu_kernel(cpu_kernel const& other) : parent(other) {}

        /**
         * @brief Move constructor
         *
         * @param other Another <code>cpu_kernel</code> object.
         */
        cpu_kernel(cpu_kernel&& other) : parent(std::move(other)) {}

        /**
         * @brief Copy assignment
         */
        cpu_kernel& operator=(cpu_kernel const& other) { return static_cast<cpu_kernel&>(parent::operator=(other)); }

        /**
         * @brief Move assignment
         */
        cpu_kernel& operator=(cpu_kernel&& other) { return static_cast<cpu_kernel&>(parent::operator=(std::move(other))); }

        /**
         * @brief Set this <code>cpu_kernel</code> object as blocking
         */
        cpu_kernel& set_blocking()
        {
            parent::set_attr(::hetcompute::internal::legacy::attr::blocking);
            return *this;
        }

        /**
         * @brief Returns whether this <code>cpu_kernel</code> object is blocking
         */
        bool is_blocking() const { return parent::has_attr(::hetcompute::internal::legacy::attr::blocking); }

        /**
         * @brief Set this <code>cpu_kernel</code> object as meant for a big core in a
         *        big.LITTLE SoC
         */
        cpu_kernel& set_big()
        {
            parent::set_attr(::hetcompute::internal::legacy::attr::big);
            return *this;
        }

        /**
         * @brief Returns whether a <code>cpu_kernel</code> object is meant for a big
         *        core in a big.LITTLE SoC
         */
        bool is_big() const { return parent::has_attr(::hetcompute::internal::legacy::attr::big); }

        /**
         * @brief Set this <code>cpu_kernel</code> object as meant for a LITTLE core
         *        in a big.LITTLE SoC
         */
        cpu_kernel& set_little()
        {
            parent::set_attr(::hetcompute::internal::legacy::attr::little);
            return *this;
        }

        /**
         * @brief Returns whether a <code>cpu_kernel</code> object is meant for a
         *        LITTLE core in a big.LITTLE SoC
         */
        bool is_little() const { return parent::has_attr(::hetcompute::internal::legacy::attr::little); }

        /**
         * @brief Destructor
         */
        ~cpu_kernel() {}
    };

    template <typename Fn>
    HETCOMPUTE_CONSTEXPR_CONST typename cpu_kernel<Fn>::size_type cpu_kernel<Fn>::arity;

    /**
     * @brief A wrapper around a function
     *
     * A <code>cpu_kernel</code> object contains CPU executable code. It can be
     * used to create tasks. When such a task runs, it executes the function
     * in its <code>cpu_kernel</code>.
     *
     * @sa cpu_kernel<FReturnType(FArgs...)>
     */
    template <typename FReturnType, typename... FArgs>
    class cpu_kernel<FReturnType(FArgs...)> : protected ::hetcompute::internal::cpu_kernel<FReturnType (*)(FArgs...)>
    {
        using parent = ::hetcompute::internal::cpu_kernel<FReturnType (*)(FArgs...)>;

        template <typename X, typename Y>
        friend struct ::hetcompute::internal::task_factory_dispatch;

        template <typename X, typename Y, typename Z>
        friend struct ::hetcompute::internal::task_factory;

        friend struct ::hetcompute::internal::cpu_kernel_caller;

    public:
        using size_type   = typename parent::size_type;
        using return_type = typename parent::return_type;
        using args_tuple  = typename parent::args_tuple;

        using collapsed_task_type     = typename parent::collapsed_task_type;
        using non_collapsed_task_type = typename parent::non_collapsed_task_type;

        static HETCOMPUTE_CONSTEXPR_CONST size_type arity = parent::arity;

        /**
         * @brief Constructor
         *
         * @param fn A function name or function pointer.
         */
        explicit cpu_kernel(FReturnType (*fn)(FArgs...)) : parent(std::forward<FReturnType (*)(FArgs...)>(fn)) {}

        /**
         * @brief Copy constructor
         *
         * @param other Another <code>cpu_kernel</code> object.
         */
        cpu_kernel(cpu_kernel const& other) : parent(other) {}

        /**
         * @brief Move constructor
         *
         * @param other Another <code>cpu_kernel</code> object.
         */
        cpu_kernel(cpu_kernel&& other) : parent(other) {}

        /**
         * @brief Copy assignment
         */
        cpu_kernel& operator=(cpu_kernel const& other) { return static_cast<cpu_kernel&>(parent::operator=(other)); }

        /**
         * @brief Move assignment
         */
        cpu_kernel& operator=(cpu_kernel&& other) { return static_cast<cpu_kernel&>(parent::operator=(std::move(other))); }

        /**
         * @brief Set a <code>cpu_kernel</code> object as blocking
         */
        cpu_kernel& set_blocking()
        {
            parent::set_attr(::hetcompute::internal::legacy::attr::blocking);
            return *this;
        }

        /**
         * @brief Returns whether a <code>cpu_kernel</code> object is blocking
         */
        bool is_blocking() const { return parent::has_attr(::hetcompute::internal::legacy::attr::blocking); }

        /**
         * @brief Set this <code>cpu_kernel</code> object as meant for a big core in a
         *        big.LITTLE SoC
         */
        cpu_kernel& set_big()
        {
            parent::set_attr(::hetcompute::internal::legacy::attr::big);
            return *this;
        }

        /**
         * @brief Returns whether a <code>cpu_kernel</code> object is meant for a big
         *        core in a big.LITTLE SoC
         */
        bool is_big() const { return parent::has_attr(::hetcompute::internal::legacy::attr::big); }

        /**
         * @brief Set this <code>cpu_kernel</code> object as meant for a LITTLE core
         *        in a big.LITTLE SoC
         */
        cpu_kernel& set_little()
        {
            parent::set_attr(::hetcompute::internal::legacy::attr::little);
            return *this;
        }

        /**
         * @brief Returns whether a <code>cpu_kernel</code> object is meant for a
         *        LITTLE core in a big.LITTLE SoC
         */
        bool is_little() const { return parent::has_attr(::hetcompute::internal::legacy::attr::little); }

        /**
         * @brief Destructor
         */
        ~cpu_kernel() {}
    };

    template <typename FReturnType, typename... FArgs>
    HETCOMPUTE_CONSTEXPR_CONST
        typename hetcompute::cpu_kernel<FReturnType(FArgs...)>::size_type hetcompute::cpu_kernel<FReturnType(FArgs...)>::arity;

    /**
     * @brief Create a <code>cpu_kernel</code> object from a function
     *
     * Create a <code>cpu_kernel</code> object that executes a given
     * function. This kernel object can then be used to create a task.
     *
     * @param fn A function name or function pointer.
     * @return cpu_kernel A kernel object that executes the given function.
     *
     * @sa create_cpu_kernel(Fn&& fn)
     */
    template <typename FReturnType, typename... FArgs>
    hetcompute::cpu_kernel<FReturnType(FArgs...)> create_cpu_kernel(FReturnType (*fn)(FArgs...))
    {
        return hetcompute::cpu_kernel<FReturnType(FArgs...)>(fn);
    }

    /**
     * @brief Create a <code>cpu_kernel</code> object from a function object
     *
     * Create a <code>cpu_kernel</code> object that executes a given function
     * object. A function object (also called a functor) is any object with the
     * <code>()</code> operator defined, such as a lambda expression.
     * This kernel object can then be used to create a task.
     *
     * @param fn A function object such as a lambda expression.
     * @return cpu_kernel A kernel object that executes the given function object.
     *
     * @sa create_cpu_kernel(FReturnType(*fn)(FArgs...))
     */
    template <typename Fn>
    hetcompute::cpu_kernel<typename std::remove_reference<Fn>::type> create_cpu_kernel(Fn&& fn)
    {
        using no_ref = typename std::remove_reference<Fn>::type;
        return hetcompute::cpu_kernel<no_ref>(std::forward<Fn>(fn));
    }

    /**
 * @}
 */ /* end_addtogroup kernelclass_doc */

}; // namespace hetcompute
