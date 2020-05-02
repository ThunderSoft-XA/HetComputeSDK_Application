/** @file taskptr.hh */
#pragma once

// Include user-visible headers first
#include <hetcompute/exceptions.hh>
#include <hetcompute/task.hh>

// Include internal headers next
#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/task/task_shared_ptr.hh>

namespace hetcompute
{
    /** @cond PRIVATE */
    namespace internal
    {
        namespace testing
        {
            class task_tests;
        };   // namespace testing
    };  // namespace internal

    // Forward declarations
    // --------------------------------------------------------------------
    template <typename... Stuff>
    class task_ptr;

    template <typename Fn>
    class cpu_body;

    template <typename ReturnType, typename... Args>
    ::hetcompute::task_ptr<ReturnType> create_value_task(Args&&... args);

    // --------------------------------------------------------------------
    /** @endcond */

    /** @addtogroup taskclass_doc
    @{ */

    /**
     * @brief Smart pointer to a task object without function information.
     *
     * Smart pointer to a task object, i.e., <code>hetcompute::task<></code>,
     * similar to <code>std::shared_ptr</code>.
    */
    template<>
    class task_ptr<>
    {
        /** @cond PRIVATE */
        friend ::hetcompute::internal::task* ::hetcompute::internal::c_ptr(::hetcompute::task_ptr<>& t);
        friend ::hetcompute::internal::task* ::hetcompute::internal::c_ptr(::hetcompute::task_ptr<> const& t);
        friend class ::hetcompute::internal::testing::task_tests;
        /** @endcond */

    public:
        /**
         *  Task Object type
         */
        using task_type = task<>;

        /**
         * @brief Default constructor. Constructs a <code>task_ptr<></code>
         * with no <code>task<></code>.
         *
         * Constructs a <code>task_ptr<></code> that manages no <code>task<></code>.
         *  <code>task_ptr<>::get</code> returns <code>nullptr</code>.
        */
        task_ptr() : _shared_ptr(nullptr)
        {
        }

        /**
         * @brief Default constructor. Constructs a <code>task_ptr<></code>
         * with no <code>task<></code>.
         *
         * Constructs a <code>task_ptr<></code> that manages no <code>task<></code>.
         *  <code>task_ptr<>::get</code> returns <code>nullptr</code>.
        */
        task_ptr(std::nullptr_t) : _shared_ptr(nullptr)
        {
        }

        /**
         * @brief Copy constructor. Constructs a <code>task_ptr<></code> that
         * manages the same task as <code>other</code>.
         *
         * Constructs a <code>task_ptr<></code> object that manages the same task<>
         * as <code>other</code>. If <code>other</code> points to
         * <code>nullptr</code>, the newly built object also points to
         * <code>nullptr</code>.
         *
         * @param other Task pointer to copy.
         *
        */
        task_ptr(task_ptr const& other) : _shared_ptr(other._shared_ptr)
        {
        }

        /**
         * @brief Move constructor. Move-constructs a <code>task_ptr<></code> that
         * manages the same task as <code>other</code>.
         *
         * Constructs a <code>task_ptr<></code> object that manages the same
         * task as <code>other</code> and resets <code>other</code>. If
         * <code>other</code> points to <code>nullptr</code>, the newly
         * built object also points to <code>nullptr</code>.
         *
         * @param other Task pointer to move from.
         *
        */
        task_ptr(task_ptr&& other) : _shared_ptr(std::move(other._shared_ptr))
        {
        }

        /**
         * @brief Assignment operator. Assigns the task managed by <code>other</code>
         * to <code>*this</code>.
         *
         * Assigns the task managed by <code>other</code> to <code>*this</code>. If,
         * before the assignment, <code>*this</code> was the last <code>task_ptr<></code>
         * pointing to a task <code>t</code>, then the assignment will cause
         * <code>t</code> to be destroyed. If <code>other</code> manages no
         * object, <code>*this</code> will also not manage an object after
         * the assignment.
         *
         * @param other Task pointer to copy.
         *
         * @return <code>*this</code>.
        */
        task_ptr& operator=(task_ptr const& other)
        {
            _shared_ptr = other._shared_ptr;
            return *this;
        }

        /**
         * @brief Assignment operator. Resets <code>*this</code>.
         *
         * Resets <code>*this</code> so that it manages no object. If,
         * before the assignment, <code>*this</code> was the last
         * <code>task_ptr<></code> pointing to a task <code>t</code>, then
         * the assignment will cause <code>t</code> to be destroyed.
         * <code>*this</code> will also not manage an object after the assignment.
         *
         * @return <code>*this</code>.
        */
        task_ptr& operator=(std::nullptr_t)
        {
            _shared_ptr = nullptr;
            return *this;
        }

        /**
         * @brief Move-assignment operator. Move-assigns the task managed
         * by <code>other</code> to <code>*this</code>.
         *
         * Move-assigns the task managed by <code>other</code> to
         * <code>*this</code>. <code>other</code> will manage no task
         * after the assignment.
         *
         * If, before the assignment, <code>*this</code> was the last
         * <code>task_ptr<></code> pointing to a task <code>t</code>, then
         * the assignment will cause <code>t</code> to be destroyed. If
         * <code>other</code> manages no object, <code>*this</code> will also
         * not manage an object after the assignment.
         *
         * @param other Task pointer to move from.
         *
         * @return <code>*this</code>.
        */
        task_ptr& operator=(task_ptr&& other)
        {
            _shared_ptr = (std::move(other._shared_ptr));
            return *this;
        }

        /**
         * @brief Exchanges managed tasks between <code>*this</code> and
         * <code>other</code>.
         *
         * Exchanges managed tasks between <code>*this</code> and
         * <code>other</code>.
         *
         * @param other Task pointer to exchange with.
        */
        void swap(task_ptr& other)
        {
            std::swap(_shared_ptr, other._shared_ptr);
        }

        /**
         * @brief Dereference operator. Returns pointer to managed task.
         *
         * Returns pointer to managed task. Do not call this member
         * function if <code>*this</code> manages no task.
         *
         * @return Pointer to managed task object.
        */
        task_type* operator->() const
        {
            auto t = get_raw_ptr();
            HETCOMPUTE_INTERNAL_ASSERT(t != nullptr, "null task pointer.");
            return t->get_facade<task_type>();
        }

        /**
         * @brief Returns the pointer to the managed task.
         *
         * Returns pointer to the managed task. Remember that the lifetime
         * of the task is defined by the lifetime of the
         * <code>task_ptr<></code> objects managing it. If all
         * <code>task_ptr<></code> objects managing a task <code>t</code>
         * go out of scope, all <code>task<>*</code> pointing to
         * <code>t</code> may be invalid.
         *
         * @return Pointer to managed task object.
        */
        task_type* get() const
        {
            auto t = get_raw_ptr();
            if (t == nullptr)
            {
                return nullptr;
            }
            return t->get_facade<task_type>();
        }

        /**
         * @brief Resets the pointer to the managed task.
         *
         * Resets pointer to managed task. If, <code>*this</code> was the last
         * <code>task_ptr<></code> pointing to a task <code>t</code>, then
         * <code>reset()</code> cause <code>g</code> to be destroyed.
         *
         *
         * @return Pointer to managed task object.
        */
        void reset()
        {
            _shared_ptr.reset();
        }

        /**
         * @brief Checks whether pointer is not <code>nullptr</code>.
         *
         * Checks whether <code>*this</code> manages a task.
         *
         * @return
         *    <code>true</code> -- The pointer is not <code>nullptr</code> (<code>*this</code> manages a task).\n
         *    <code>false</code> -- The pointer is <code>nullptr</code> (<code>*this</code> does not manage a task).
        */
        explicit operator bool() const
        {
            return _shared_ptr != nullptr;
        }

        /**
         * @brief Returns the number of <code>task_ptr<></code> objects
         * managing the same object (including <code>*this</code>).
         *
         * Returns the number of <code>task_ptr<></code> objects
         * managing the same object (including <code>*this</code>).
         *
         * @includelineno samples/src/task_ptr1.cc
         *
         * @par Output
         * @code
         * After construction: t.use_count() = 1
         * After copy-construction: t2.use_count() = 2
         * After calling t.get(). t.use_count() = 2
         * After t->wait_for: t.use_count() = 2
         * @endcode
         *
         * @return Total number of <code>task_ptr<></code> points to the same task.
        */
        size_t use_count() const
        {
            return _shared_ptr.use_count();
        }

        /**
         * @brief Returns if this is the only <code>task_ptr</code>
         *        managing the underlying task
         *
         * Returns if this is the only <code>task_ptr</code> object
         * managing the underlying task.
         *
         * @return A boolean indicating if <code>task_ptr</code> uniquely
         *         manages the underlying task
        */
        bool unique() const
        {
            return _shared_ptr.use_count() == 1;
        }

        /**
         *  Default destructor.
         */
        ~task_ptr()
        {
        }

        /** @cond PRIVATE */

    protected:
        explicit task_ptr(::hetcompute::internal::task* t) : _shared_ptr(t)
        {
        }

        task_ptr(::hetcompute::internal::task* t, ::hetcompute::internal::task_shared_ptr::ref_policy policy) : _shared_ptr(t, policy)
        {
        }

        ::hetcompute::internal::task* get_raw_ptr() const
        {
            return ::hetcompute::internal::c_ptr(_shared_ptr);
        }

        /** @endcond */

    private:
        ::hetcompute::internal::hetcompute_shared_ptr<::hetcompute::internal::task> _shared_ptr;

        // static_assert(sizeof(task_type) == sizeof(::hetcompute::internal::task::self_ptr), "Cant allocate task.");
    };

    /**
     * @brief Smart pointer to a task object with function with void return type.
     *
     * Smart pointer to a task object, i.e., <code>hetcompute::task<void></code>,
     * similar to <code>std::shared_ptr</code>.
     */
    template <>
    class task_ptr<void> : public task_ptr<>
    {
        /** @cond PRIVATE */
        using parent = task_ptr<>;
        /** @endcond */

    public:
        /**
         * Task object type.
        */
        using task_type = task<void>;

        /**
         * @brief Default constructor. Constructs a <code>task_ptr<void></code>
         * with no <code>task<void></code>.
         *
         * Constructs a <code>task_ptr<void></code> that manages no <code>task<void></code>.
         * <code>task_ptr<void>::get</code> returns <code>nullptr</code>.
        */
        task_ptr() : parent() {}

        /**
         * @brief Default constructor. Constructs a <code>task_ptr<void></code>
         * with no <code>task<void></code>.
         *
         * Constructs a <code>task_ptr<void></code> that manages no <code>task<void></code>.
         * <code>task_ptr<void>::get</code> returns <code>nullptr</code>.
        */
        /* implicit */ task_ptr(std::nullptr_t) : parent(nullptr) {}

        /**
         * @brief Copy constructor. Constructs a <code>task_ptr<void></code> that
         * manages the same task as <code>other</code>.
         *
         * Constructs a <code>task_ptr<void></code> object that manages the same task<void>
         * as <code>other</code>. If <code>other</code> points to <code>nullptr</code>,
         * the newly built object also points to <code>nullptr</code>.
         *
         * @param other Task pointer to copy.
         *
        */
        task_ptr(task_ptr<void> const& other) : parent(other) {}

        /**
         * @brief Move constructor. Move-constructs a <code>task_ptr<void></code> that
         * manages the same task as <code>other</code>.
         *
         * Constructs a <code>task_ptr<void></code> object that manages the same
         * task as <code>other</code> and resets <code>other</code>. If
         * <code>other</code> points to <code>nullptr</code>, the newly
         * built object also points to <code>nullptr</code>.
         *
         * @param other Task pointer to move from.
         *
        */
        task_ptr(task_ptr<void>&& other) : parent(std::move(other)) {}

        /**
         * @brief Assignment operator. Assigns the task managed by <code>other</code>
         * to <code>*this</code>.
         *
         * Assigns the task managed by <code>other</code> to <code>*this</code>. If,
         * before the assignment, <code>*this</code> was the last
         * <code>task_ptr<void></code> pointing to a task <code>t</code>, then the
         * assignment will cause <code>t</code> to be destroyed. If <code>other</code>
         * manages no object, <code>*this</code> will also not manage an object after
         * the assignment.
         *
         * @param other Task pointer to copy.
         *
         * @return <code>*this</code>.
        */
        task_ptr& operator=(task_ptr<void> const& other)
        {
            parent::operator=(other);
            return *this;
        }

        /**
         * @brief Assignment operator. Resets <code>*this</code>.
         *
         * Resets <code>*this</code> so that it manages no object. If,
         * before the assignment, <code>*this</code> was the last
         * <code>task_ptr<void></code> pointing to a task <code>t</code>, then
         * the assignment will cause <code>t</code> to be destroyed. If
         * <code>other</code> manages no object, <code>*this</code> will also
         * not manage an object after the assignment.
         *
         * @return <code>*this</code>.
         */
        task_ptr& operator=(task_ptr<void>&& other)
        {
            parent::operator=(std::move(other));
            return *this;
        }

        /**
         *  Destructor.
        */
        ~task_ptr() {}

        /**
         * @brief Exchanges managed tasks between <code>*this</code> and
         * <code>other</code>.
         *
         * Exchanges managed tasks between <code>*this</code> and
         * <code>other</code>.
         *
         * @param other Task pointer to exchange with.
         */
        void swap(task_ptr<void>& other) { parent::swap(other); }

        /**
         * @brief Dereference operator. Returns pointer to managed task.
         *
         * Returns pointer to managed task. Do not call this member
         * function if <code>*this</code> manages no task.
         *
         * @return Pointer to managed task object.
         */
        task_type* operator->() const
        {
            auto t = get_raw_ptr();
            HETCOMPUTE_INTERNAL_ASSERT(t != nullptr, "null task pointer.");
            return t->get_facade<task_type>();
        }

        /**
         * @brief Returns pointer to managed task.
         *
         * Returns pointer to the managed task. Remember that the lifetime
         * of the task is defined by the lifetime of the
         * <code>task_ptr<void></code> objects managing it. If all
         * <code>task_ptr<void></code> objects managing a task <code>t</code>
         * go out of scope, all <code>task<void>*</code> pointing to
         * <code>t</code> may be invalid.
         *
         * @return Pointer to managed task object.
         */
        task_type* get() const
        {
            auto t = get_raw_ptr();
            if (t == nullptr)
                return nullptr;
            return t->get_facade<task_type>();
        }

    protected:
        task_ptr(::hetcompute::internal::task* t, ::hetcompute::internal::task_shared_ptr::ref_policy policy) : parent(t, policy) {}
    };  // task_ptr<void>

    /**
     * @brief Smart pointer to a task object with function with non-void return type.
     *
     * Smart pointer to a task object, i.e., <code>hetcompute::task<ReturnType></code>,
     * similar to <code>std::shared_ptr</code>.
     *
     * @tparam ReturnType Return type of the task function
    */
    template<typename ReturnType>
    class task_ptr<ReturnType> : public task_ptr<>
    {
        /** @cond PRIVATE */
        static_assert(std::is_reference<ReturnType>::value == false,
                      "This version of Qualcomm Technologies Inc. HetCompute does not support references as return types of tasks. "
                      "Check the Qualcomm Technologies Inc. HetCompute manual.");

        using parent = task_ptr<>;

        /** @endcond */

    public:
        /**
         * Return type of the task function.
         */
        using return_type = ReturnType;

        /**
         * Task object type.
         */
        using task_type = task<return_type>;

        /**
         * @brief Default constructor. Constructs a <code>task_ptr<ReturnType></code>
         * with no <code>task<ReturnType></code>.
         *
         * Constructs a <code>task_ptr<ReturnType></code> that manages no
         * <code>task<ReturnType></code>. <code>task_ptr<ReturnType>::get</code>
         * returns <code>nullptr</code>.
         */
        task_ptr() : parent(nullptr) {}

        /**
         * @brief Default constructor. Constructs a <code>task_ptr<ReturnType></code>
         * with no <code>task<ReturnType></code>.
         *
         * Constructs a <code>task_ptr<ReturnType></code> that manages no
         * <code>task<ReturnType></code>. <code>task_ptr<ReturnType>::get</code>
         * returns <code>nullptr</code>.
         */
        /* implicit */ task_ptr(std::nullptr_t) : parent(nullptr) {}

        /**
         * @brief Copy constructor. Constructs a <code>task_ptr<ReturnType></code> that
         * manages the same task as <code>other</code>.
         *
         * Constructs a <code>task_ptr<ReturnType></code> object that manages the same
         * task<ReturnType> as <code>other</code>. If <code>other</code> points to
         * <code>nullptr</code>, the newly built object also points to
         * <code>nullptr</code>.
         *
         * @param other Task pointer to copy.
         *
         */
        task_ptr(task_ptr<return_type> const& other) : parent(other) {}

        /**
         * @brief Move constructor. Move-constructs a <code>task_ptr<ReturnType></code> that
         * manages the same task as <code>other</code>.
         *
         * Constructs a <code>task_ptr<ReturnType></code> object that manages the same
         * task as <code>other</code> and resets <code>other</code>. If
         * <code>other</code> points to <code>nullptr</code>, the newly
         * built object also points to <code>nullptr</code>.
         *
         * @param other Task pointer to move from.
         *
         */
        task_ptr(task_ptr<return_type>&& other) : parent(std::move(other)) {}

        /**
         * @brief Assignment operator. Assigns the task managed by <code>other</code>
         * to <code>*this</code>.
         *
         * Assigns the task managed by <code>other</code> to <code>*this</code>.
         * If, before the assignment, <code>*this</code> was the last
         * <code>task_ptr<ReturnType></code> pointing to a task <code>t</code>,
         * then the assignment will cause <code>t</code> to be destroyed. If
         * <code>other</code> manages no object, <code>*this</code> will also not
         * manage an object after the assignment.
         *
         * @param other Task pointer to copy.
         *
         * @return <code>*this</code>.
         */
        task_ptr& operator=(task_ptr<return_type> const& other)
        {
            parent::operator=(other);
            return *this;
        }

        /**
         * @brief Assignment operator. Resets <code>*this</code>.
         *
         * Resets <code>*this</code> so that it manages no object. If,
         * before the assignment, <code>*this</code> was the last
         * <code>task_ptr<ReturnType></code> pointing to a task <code>t</code>, then
         * the assignment will cause <code>t</code> to be destroyed. If
         * <code>other</code> manages no object, <code>*this</code> will also
         * not manage an object after the assignment.
         *
         * @return <code>*this</code>.
         */
        task_ptr& operator=(task_ptr<return_type>&& other)
        {
            parent::operator=(std::move(other));
            return *this;
        }

        /**
         * @brief Compound assignment operator += with value operand
         *
         * Compound assignment operator += with value operand
         *
         * Create a new task whose return value will be the result of this operator
         * applied to the return value of the original task pointed by this task_ptr
         * and the operand on the right side of the operator.
         *
         * The new task will be data dependent on the original task and the operand on
         * the right side if it is also a task.
         *
         * The new task will be launching automatically by the runtime once the data
         * is ready.
         *
         * The old task pointed by this will be dereferrenced, and this task_ptr will
         * point to the newly created task with the result value.
         *
         * @note1 The operator should be applicable onto the return value of the
         * current task and the operand (return value is considered here if the operand
         * is also a task).
         *
         * @param op Operand on the right side of this operator.
         *
         * @return A new task whose return value is the result of this operator
         *         and can be pointed to by this shared pointer (same type of return
         *         value).
         *
         * @includelineno samples/src/algebraic_compound_assignment_op.cc
        */
        template <typename T>
        task_ptr& operator+=(T&& op);

        /**
         * @brief Compound assignment operator -= with value operand.
         *
         * Compound assignment operator -= with value operand.
         *
         * @sa template<typename T> task_ptr& operator+=(T&& op2)
        */
        template <typename T>
        task_ptr& operator-=(T&& op);

        /**
         * @brief Compound assignment operator *= with value operand.
         *
         * Compound assignment operator *= with value operand.
         *
         * @sa template<typename T> task_ptr& operator+=(T&& op2)
         */
        template <typename T>
        task_ptr& operator*=(T&& op);

        /**
         * @brief Compound assignment operator /= with value operand.
         *
         * Compound assignment operator /= with value operand.
         *
         * @sa template<typename T> task_ptr& operator+=(T&& op2)
         */
        template <typename T>
        task_ptr& operator/=(T&& op);

        /**
         * @brief Compound assignment operator %= with value operand.
         *
         * Compound assignment operator %= with value operand.
         *
         * @sa template<typename T> task_ptr& operator+=(T&& op2)
         */
        template <typename T>
        task_ptr& operator%=(T&& op);

        /**
         * @brief Compound assignment operator &= with value operand.
         *
         * Compound assignment operator &= with value operand.
         *
         * @sa template<typename T> task_ptr& operator+=(T&& op2)
         */
        template <typename T>
        task_ptr& operator&=(T&& op);

        /**
         * @brief Compound assignment operator |= with value operand.
         *
         * Compound assignment operator |= with value operand.
         *
         * @sa template<typename T> task_ptr& operator+=(T&& op2)
         */
        template <typename T>
        task_ptr& operator|=(T&& op);

        /**
         * @brief Compound assignment operator ^= with value operand.
         *
         * Compound assignment operator ^= with value operand.
         *
         * @sa template<typename T> task_ptr& operator+=(T&& op2)
         */
        template <typename T>
        task_ptr& operator^=(T&& op);

        /**
         * @brief Destructor.
         *
         * Destructor.
         */
        ~task_ptr() {}

        /**
         * @brief Exchanges managed tasks between <code>*this</code> and
         * <code>other</code>.
         *
         * Exchanges managed tasks between <code>*this</code> and
         * <code>other</code>.
         *
         * @param other Task pointer to exchange with.
         */
        void swap(task_ptr<return_type>& other) { parent::swap(other); }

        /**
         * @brief Dereference operator. Returns pointer to managed task.
         *
         * Returns pointer to managed task. Do not call this member
         * function if <code>*this</code> manages no task.
         *
         * @throw api_exception If task pointer is <code>nullptr</code>.
         * @note  If exceptions are disabled in application, terminates the app
         *        if task pointer is <code>nullptr</code>
         *
         * @return Pointer to managed task object.
         */
        task_type* operator->() const
        {
            auto t = get_raw_ptr();
            HETCOMPUTE_INTERNAL_ASSERT(t != nullptr, "null task pointer.");
            return t->template get_facade<task_type>();
        }

        /**
         * @brief Returns pointer to managed task.
         *
         * Returns pointer to the managed task. Remember that the lifetime
         * of the task is defined by the lifetime of the
         * <code>task_ptr<ReturnType></code> objects managing it. If all
         * <code>task_ptr<ReturnType></code> objects managing a task <code>t</code>
         * go out of scope, all <code>task<ReturnType>*</code> pointing to
         * <code>t</code> may be invalid.
         *
         * @return Pointer to managed task object.
         */
        task_type* get() const
        {
            auto t = get_raw_ptr();
            if (t == nullptr)
            {
                return nullptr;
            }
            return t->template get_facade<task_type>();
        }

        /** @cond PRIVATE */
    protected:
        task_ptr<return_type>(::hetcompute::internal::task* t, ::hetcompute::internal::task_shared_ptr::ref_policy policy) : parent(t, policy)
        {
        }

        template <typename R, typename... As>
        friend ::hetcompute::task_ptr<R> create_value_task(As&&... args);
        /** @endcond */

    };  // class task_ptr<return_type>

    /**
     * @brief Smart pointer to a task object with full-function signature (return
     * type + parameter list).
     *
     * Smart pointer to a task object, i.e., <code>hetcompute::task<ReturnType(Args...)></code>,
     * similar to <code>std::shared_ptr</code>.
     *
     * @tparam ReturnType Return type of the task function.
     * @tparam Args...    The type for the task parameters.
    */
    template <typename ReturnType, typename... Args>
    class task_ptr<ReturnType(Args...)> : public task_ptr<ReturnType>
    {
        /** @cond PRIVATE */
        using parent = task_ptr<ReturnType>;
        /** @endcond */

    public:
        /**
         * Task object type.
         */
        using task_type = task<ReturnType(Args...)>;

        /**
         *  Unsigned integral type.
         */
        using size_type = typename task_type::size_type;

        /**
         * Return type of the task function.
         */
        using return_type = typename task_type::return_type;

        /**
         * Tuple for the task object parameter types.
         */
        using args_tuple = typename task_type::args_tuple;

        /**
         * Number of parameters.
         */
        static HETCOMPUTE_CONSTEXPR_CONST size_type arity = task_type::arity;

        /** @cond PRIVATE*/
        /**
         *  @brief Helper structure to get the type of the task argument.
         *
         *  Helper structure to get the type of the task argument.
         *
         *  @tparam ArgIndex The index of the task argument.
         */
        template <size_type ArgIndex>
        struct argument
        {
            static_assert(ArgIndex < sizeof...(Args), "Out of range argument indexing.");
            using type = typename std::tuple_element<ArgIndex, args_tuple>::type;
        };
        /** @endcond */

        /**
         * @brief Default constructor. Constructs a <code>task_ptr<ReturnType></code>
         * with no <code>task<ReturnType></code>.
         *
         * Constructs a <code>task_ptr<ReturnType></code> that manages no
         * <code>task<ReturnType></code>. <code>task_ptr<ReturnType>::get</code>
         * returns <code>nullptr</code>.
         */
        task_ptr() : parent() {}

        /**
         * @brief Default constructor. Constructs a <code>task_ptr<ReturnType></code>
         * with no <code>task<ReturnType></code>.
         *
         * Constructs a <code>task_ptr<ReturnType></code> that manages no
         * <code>task<ReturnType></code>. <code>task_ptr<ReturnType>::get</code>
         * returns <code>nullptr</code>.
         */
        /* implicit */ task_ptr(std::nullptr_t) : parent(nullptr) {}

        /**
         * @brief Copy constructor. Constructs a <code>task_ptr<ReturnType></code> that
         * manages the same task as <code>other</code>.
         *
         * Constructs a <code>task_ptr<ReturnType></code> object that manages the same
         * task<ReturnType> as <code>other</code>. If <code>other</code> points to
         * <code>nullptr</code>, the newly built object also points to
         * <code>nullptr</code>.
         *
         * @param other Task pointer to copy.
         *
         */
        task_ptr(task_ptr<ReturnType(Args...)> const& other) : parent(other) {}

        /**
         * @brief Move constructor. Move-constructs a <code>task_ptr<ReturnType></code> that
         * manages the same task as <code>other</code>.
         *
         * Constructs a <code>task_ptr<ReturnType></code> object that manages the same
         * task as <code>other</code> and resets <code>other</code>. If
         * <code>other</code> points to <code>nullptr</code>, the newly
         * built object also points to <code>nullptr</code>.
         *
         * @param other Task pointer to move from.
         *
         */
        task_ptr(task_ptr<ReturnType(Args...)>&& other) : parent(std::move(other)) {}

        /**
         * @brief Assignment operator. Assigns the task managed by <code>other</code>
         * to <code>*this</code>.
         *
         * Assigns the task managed by <code>other</code> to <code>*this</code>. If,
         * before the assignment, <code>*this</code> was the last
         * <code>task_ptr<ReturnType></code> pointing to a task <code>t</code>, then
         * the assignment will cause <code>t</code> to be destroyed. If
         * <code>other</code> manages no object, <code>*this</code> will also not
         * manage an object after the assignment.
         *
         * @param other Task pointer to copy.
         *
         * @return <code>*this</code>.
         */
        task_ptr& operator=(task_ptr<ReturnType(Args...)> const& other)
        {
            parent::operator=(other);
            return *this;
        }

        /**
         * @brief Assignment operator. Resets <code>*this</code>.
         *
         * Resets <code>*this</code> so that it manages no object. If, before the
         * assignment, <code>*this</code> was the last <code>task_ptr<ReturnType></code>
         * pointing to a task <code>t</code>, then the assignment will cause <code>t</code>
         * to be destroyed. If <code>other</code> manages no object, <code>*this</code>
         * will also not manage an object after the assignment.
         *
         * @return <code>*this</code>.
         */
        task_ptr& operator=(task_ptr<ReturnType(Args...)>&& other)
        {
            parent::operator=(std::move(other));
            return *this;
        }

        /**
         *  @brief Destructor.
         *
         *  Destructor.
         */
        ~task_ptr() {}

        /**
         * @brief Exchanges managed tasks between <code>*this</code> and
         * <code>other</code>.
         *
         * Exchanges managed tasks between <code>*this</code> and
         * <code>other</code>.
         *
         * @param other Task pointer to exchange with.
         */
        void swap(task_ptr<ReturnType(Args...)>& other) { parent::swap(other); }

        /**
         * @brief Dereference operator. Returns pointer to managed task.
         *
         * Returns pointer to managed task. Do not call this member
         * function if <code>*this</code> manages no task.
         *
         * @return Pointer to managed task object.
         */
        task_type* operator->() const
        {
            auto t = task_ptr<>::get_raw_ptr();
            HETCOMPUTE_INTERNAL_ASSERT(t != nullptr, "null task pointer.");
            return t->template get_facade<task_type>();
        }

        /**
         * @brief Returns pointer to managed task.
         *
         * Returns pointer to the managed task. Remember that the lifetime
         * of the task is defined by the lifetime of the
         * <code>task_ptr<ReturnType></code> objects managing it. If all
         * <code>task_ptr<ReturnType></code> objects managing a task <code>t</code>
         * go out of scope, all <code>task<ReturnType>*</code> pointing to
         * <code>t</code> may be invalid.
         *
         * @return Pointer to managed task object.
         */
        task_type* get() const
        {
            auto t = task_ptr<>::get_raw_ptr();
            if (t == nullptr)
            {
                return nullptr;
            }
            return t->template get_facade<task_type>();
        }

        /** @cond PRIVATE */
        task_ptr(::hetcompute::internal::task* t, ::hetcompute::internal::task_shared_ptr::ref_policy policy) : parent(t, policy) {}

    protected:
        static_assert(arity < ::hetcompute::internal::set_arg_tracker::max_arity::value,
                      "Task has too many arguments. Refer to the Qualcomm Technologies Inc. HetCompute manual.");

        template <class Fn>
        friend class cpu_body;
        /** @endcond */

    }; // class task_ptr<ReturnType(Args..)>

    /** @cond PRIVATE */
    template <typename ReturnType, typename... Args>
    HETCOMPUTE_CONSTEXPR_CONST typename task_ptr<ReturnType(Args...)>::size_type task_ptr<ReturnType(Args...)>::arity;
    /** @endcond */

    /**
     *  @brief Compare tasks.
     *
     *  Compares task <code>t</code> to <code>nullptr</code>.
     *
     *  @return
     *    <code>true</code> --  The pointer is not <code>nullptr</code> (<code>*this</code> manages a task).\n
     *    <code>false</code> -- The pointer is <code>nullptr</code> (<code>*this</code> does not manage a task).
     */
    inline bool operator==(task_ptr<> const& t, std::nullptr_t) { return !t; }

    /**
     *  @brief Compare tasks.
     *
     *  Compares task <code>t</code> to <code>nullptr</code>.
     *
     *  @return
     *    <code>true</code> --  The pointer is not <code>nullptr</code> (<code>*this</code> manages a task).\n
     *    <code>false</code> -- The pointer is <code>nullptr</code> (<code>*this</code> does not manage a task).
     */
    inline bool operator==(std::nullptr_t, task_ptr<> const& t) { return !t; }

    /**
     *  @brief Compare tasks.
     *
     *  Compares task <code>t</code> to <code>nullptr</code>.
     *
     *  @return
     *    <code>true</code> -- The pointer is <code>nullptr</code> (<code>*this</code> does not manage a task).
     *    <code>false</code> -- The pointer is not <code>nullptr</code> (<code>*this</code> manages a task).\n
     */
    inline bool operator!=(::hetcompute::task_ptr<> const& t, std::nullptr_t) { return static_cast<bool>(t); }

    /**
     *  @brief Compare tasks.
     *
     *  Compares task <code>t</code> to <code>nullptr</code>.
     *
     *  @return
     *    <code>true</code> -- The pointer is <code>nullptr</code> (<code>*this</code> does not manage a task).
     *    <code>false</code> -- The pointer is not <code>nullptr</code> (<code>*this</code> manages a task).\n
     */
    inline bool operator!=(std::nullptr_t, ::hetcompute::task_ptr<> const& t) { return static_cast<bool>(t); }

    /**
     *  @brief Compare tasks.
     *
     *  Compares task <code>a</code> to task <code>b</code>.
     *
     *  @return
     *    <code>true</code> -- Task <code>a</code> is the same as task <code>b</code>.
     *    <code>false</code> -- Task <code>a</code> is not the same as task <code>b</code>.
     */
    inline bool operator==(::hetcompute::task_ptr<> const& a, ::hetcompute::task_ptr<> const& b)
    {
        return hetcompute::internal::c_ptr(a) == hetcompute::internal::c_ptr(b);
    }

    /**
     *  @brief Compare tasks.
     *
     *  Compares task <code>a</code> to task <code>b</code>.
     *
     *  @return
     *    <code>true</code> -- Task <code>a</code> is not the same as task <code>b</code>.
     *    <code>false</code> -- Task <code>a</code> is the same as task <code>b</code>.
     */
    inline bool operator!=(::hetcompute::task_ptr<> const& a, ::hetcompute::task_ptr<> const& b) { return !(a == b); }
    /**
     *  @brief Compare tasks.
     *
     *  Set control dependency from <code>pred</code> to task <code>succ</code>.
     *
     *  @sa <code>hetcompute::task<>::then</code>
     */
    inline ::hetcompute::task_ptr<>& operator>>(::hetcompute::task_ptr<>& pred, ::hetcompute::task_ptr<>& succ)
    {
        pred->then(succ);
        return succ;
    }

    /**
     * @brief Specifies that the task invoking this function should be deemed to finish
     * only after the task finishes.
     *
     * Specifies that the task invoking this function should be deemed to finish
     * only after the task finishes. This method returns immediately.
     *
     * If the invoking task is multi-threaded, the programmer must ensure that
     * concurrent calls to finish_after from within the task are
     * properly synchronized.
     *
     * Do not call this function if <code>task</code> is <code>nullptr</code>.
     * It would cause a fatal error.
     *
     * @param task Task after which invoking task is deemed to finish.
     *             Can't be <code>nullptr</code>
     *
     * @throws api_exception If invoked from outside a task or from within a
     *                       hetcompute::pfor_each or if 'task' points to
     *                       null.
     */
    inline void finish_after(::hetcompute::task<>* task)
    {
        HETCOMPUTE_API_ASSERT(task != nullptr, "null task_ptr");
        auto t_ptr = internal::c_ptr(task);
        auto t     = internal::current_task();
        HETCOMPUTE_API_THROW(t != nullptr, "finish_after must be called from within a task");
        HETCOMPUTE_API_THROW(!t->is_pfor(), "finish_after cannot be called from within a pfor_each");
        t->finish_after(t_ptr);
    }

    inline void finish_after(::hetcompute::task_ptr<> const& task)
    {
        auto t_ptr = internal::c_ptr(task);
        HETCOMPUTE_API_ASSERT(t_ptr != nullptr, "null task_ptr");
        auto t = internal::current_task();
        HETCOMPUTE_API_THROW(t != nullptr, "finish_after must be called from within a task");
        HETCOMPUTE_API_THROW(!t->is_pfor(), "finish_after cannot be called from within a pfor_each");
        t->finish_after(t_ptr);
    }

    /// @cond PRIVATE
    /// Hide this from doxygen
    namespace internal
    {
        inline ::hetcompute::internal::task* c_ptr(::hetcompute::task_ptr<>& t)
        {
            return t.get_raw_ptr();
        }

        inline ::hetcompute::internal::task* c_ptr(::hetcompute::task_ptr<> const& t)
        {
            return t.get_raw_ptr();
        }
    }; //namespace hetcompute::internal
    /// @endcond

#ifdef ONLY_FOR_DOXYGEN
    /**
     * @brief Algebraic unary operator - for task.
     *
     * Algebraic unary operator - for task.
     *
     * Create a new task whose return value will be the result of this operator
     * applied to the return value of task t.
     *
     * The new task will be data dependent on task t.
     *
     * The new task will be launching automatically by the runtime once the data is ready.
     *
     * @note1 the operator should be appliable onto the return value of task t
     *
     * @param t Task operand (should have return value).
     *
     * @return A new task whose return value is the result of this operator.
     *
     * @includelineno samples/src/algebraic_op_unary_taskptr.cc
     */
    template <typename T>
    inline ::hetcompute::task_ptr<typename ::hetcompute::task_ptr<T>::return_type> operator-(const ::hetcompute::task_ptr<T>& t);

    /**
     * @brief Algebraic unary operator + for task.
     *
     * Algebraic unary operator + for task.
     *
     * @sa template<typename T>
     *     inline ::hetcompute::task_ptr<typename ::hetcompute::task_ptr<T>::return_type>
     *     operator-(const ::hetcompute::task_ptr<T>& t);
     */
    template <typename T>
    inline ::hetcompute::task_ptr<typename ::hetcompute::task_ptr<T>::return_type> operator+(const ::hetcompute::task_ptr<T>& t);

    /**
     * @brief Algebraic unary operator ~ for task.
     *
     * Algebraic unary operator ~ for task.
     *
     * @sa template<typename T>
     *     inline ::hetcompute::task_ptr<typename ::hetcompute::task_ptr<T>::return_type>
     *     operator-(const ::hetcompute::task_ptr<T>& t);
     */
    template <typename T>
    inline ::hetcompute::task_ptr<typename ::hetcompute::task_ptr<T>::return_type> operator~(const ::hetcompute::task_ptr<T>& t);

    // binary task operators
    /**
     * @brief Algebraic binary operator + for tasks.
     *
     * Algebraic binary operator + for tasks.
     *
     * Create a new task whose return value will be the result of this operator
     * applied to the return values of task t1 and task t2.
     *
     * The new task will be data dependent on task t1 and t2.
     *
     * The new task will be launching automatically by the runtime once the data are ready.
     *
     * @note1 the operator should be applicable onto the return values of task t1 and task t2.
     *
     * @param t1 First task operand (should have return value).
     *
     * @param t2 Second task operand (should have return value).
     *
     * @return A new task whose return value is the result of this operator.
     *
     * @includelineno samples/src/algebraic_op_two_taskptrs.cc
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<typename ::hetcompute::task_ptr<T1>::return_type>() +
                                         std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
    operator+(const ::hetcompute::task_ptr<T1>& t1, const ::hetcompute::task_ptr<T2>& t2);

    /**
     * @brief Algebraic binary operator + for tasks.
     *
     * Algebraic binary operator + for task.
     *
     * Create a new task whose return value will be the result of this operator
     * applied to the return value of task t1 and operand op2.
     *
     * The new task will be data dependent on task t1.
     *
     * The new task will be launching automatically by the runtime once the data is ready.
     *
     * @note1 the operator should be applicable onto the return value of task t1 and operand op2.
     *
     * @param t1 Task operand (should have return value).
     *
     * @param op2 Value operand.
     *
     * @return A new task whose return value is the result of this operator.
     *
     * @includelineno samples/src/algebraic_op_taskptr_value.cc
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<typename ::hetcompute::task_ptr<T1>::return_type>() + std::declval<T2>())>
    operator+(const ::hetcompute::task_ptr<T1>& t1, T2&& op2);

    /**
     * @brief Algebraic binary operator + for tasks.
     *
     * Algebraic binary operator + for tasks.
     *
     * Create a new task whose return value will be the result of this operator
     * applied to the return value of operand op1 and task t2.
     *
     * The new task will be data dependent on task t2.
     *
     * The new task will be launching automatically by the runtime once the data is ready.
     *
     * @note1 the operator should be applicable onto the return value of operand op1 and task t2.
     *
     * @param op1 Value operand.
     *
     * @param t2 Task operand (should have return value).
     *
     * @return A new task whose return value is the result of this operator.
     *
     * @includelineno samples/src/algebraic_op_value_taskptr.cc
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<T1>() + std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
    operator+(T1&& op1, const ::hetcompute::task_ptr<T2>& t2);

    /**
     * @brief Algebraic binary operator - for tasks.
     *
     * Algebraic binary operator - for tasks.
     *
     * @sa template<typename T1, typename T2>
     *     inline ::hetcompute::task_ptr<decltype(std::declval<typename ::hetcompute::task_ptr<T1>::return_type>()
     *                                      +
     *                                      std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
     *     operator+(const ::hetcompute::task_ptr<T1>& t1, const ::hetcompute::task_ptr<T2>& t2)
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<typename ::hetcompute::task_ptr<T1>::return_type>() -
                                         std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
    operator-(const ::hetcompute::task_ptr<T1>& t1, const ::hetcompute::task_ptr<T2>& t2);

    /**
     * @brief Algebraic binary operator - for tasks.
     *
     * Algebraic binary operator - for tasks.
     *
     * @sa template<typename T1, typename T2>
     *     inline ::hetcompute::task_ptr<decltype(std::declval<T1>()
     *                                      +
     *                                      std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
     *     operator+(T1&& op1, const ::hetcompute::task_ptr<T2>& t2)
     *
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<typename ::hetcompute::task_ptr<T1>::return_type>() - std::declval<T2>())>
    operator-(const ::hetcompute::task_ptr<T1>& t1, T2&& op2);

    /**
     * @brief Algebraic binary operator - for tasks.
     *
     * Algebraic binary operator - for tasks.
     *
     * @sa template<typename T1, typename T2>
     *     inline ::hetcompute::task_ptr<decltype(std::declval<T1>()
     *                                      +
     *                                      std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
     *     operator+(T1&& op1, const ::hetcompute::task_ptr<T2>& t2)
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<T1>() - std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
    operator-(T1&& op1, const ::hetcompute::task_ptr<T2>& t2);

    /**
     * @brief Algebraic binary operator * for tasks.
     *
     * Algebraic binary operator * for tasks.
     *
     * @sa template<typename T1, typename T2>
     *     inline ::hetcompute::task_ptr<decltype(std::declval<typename ::hetcompute::task_ptr<T1>::return_type>()
     *                                      +
     *                                      std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
     *     operator+(const ::hetcompute::task_ptr<T1>& t1, const ::hetcompute::task_ptr<T2>& t2)
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<typename ::hetcompute::task_ptr<T1>::return_type>() *
                                         std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
    operator*(const ::hetcompute::task_ptr<T1>& t1, const ::hetcompute::task_ptr<T2>& t2);

    /**
     * @brief Algebraic binary operator * for tasks.
     *
     * Algebraic binary operator * for tasks.
     *
     * @sa template<typename T1, typename T2>
     *     inline ::hetcompute::task_ptr<decltype(std::declval<T1>()
     *                                      +
     *                                      std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
     *     operator+(T1&& op1, const ::hetcompute::task_ptr<T2>& t2)
     *
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<typename ::hetcompute::task_ptr<T1>::return_type>() * std::declval<T2>())>
    operator*(const ::hetcompute::task_ptr<T1>& t1, T2&& op2);

    /**
     * @brief Algebraic binary operator * for tasks.
     *
     * Algebraic binary operator * for tasks.
     *
     * @sa template<typename T1, typename T2>
     *     inline ::hetcompute::task_ptr<decltype(std::declval<T1>()
     *                                      +
     *                                      std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
     *     operator+(T1&& op1, const ::hetcompute::task_ptr<T2>& t2)
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<T1>() * std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
    operator*(T1&& op1, const ::hetcompute::task_ptr<T2>& t2);

    /**
     * @brief Algebraic binary operator / for tasks.
     *
     * Algebraic binary operator / for tasks.
     *
     * @sa template<typename T1, typename T2>
     *     inline ::hetcompute::task_ptr<decltype(std::declval<typename ::hetcompute::task_ptr<T1>::return_type>()
     *                                      +
     *                                      std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
     *     operator+(const ::hetcompute::task_ptr<T1>& t1, const ::hetcompute::task_ptr<T2>& t2)
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<typename ::hetcompute::task_ptr<T1>::return_type>() /
                                         std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
    operator/(const ::hetcompute::task_ptr<T1>& t1, const ::hetcompute::task_ptr<T2>& t2);

    /**
     * @brief Algebraic binary operator / for tasks.
     *
     * Algebraic binary operator / for tasks.
     *
     * @sa template<typename T1, typename T2>
     *     inline ::hetcompute::task_ptr<decltype(std::declval<T1>()
     *                                      +
     *                                      std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
     *     operator+(T1&& op1, const ::hetcompute::task_ptr<T2>& t2)
     *
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<typename ::hetcompute::task_ptr<T1>::return_type>() / std::declval<T2>())>
    operator/(const ::hetcompute::task_ptr<T1>& t1, T2&& op2);

    /**
     * @brief Algebraic binary operator / for tasks.
     *
     * Algebraic binary operator / for tasks.
     *
     * @sa template<typename T1, typename T2>
     *     inline ::hetcompute::task_ptr<decltype(std::declval<T1>()
     *                                      +
     *                                      std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
     *     operator+(T1&& op1, const ::hetcompute::task_ptr<T2>& t2)
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<T1>() / std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
    operator/(T1&& op1, const ::hetcompute::task_ptr<T2>& t2);

    /**
     * @brief Algebraic binary operator % for tasks.
     *
     * Algebraic binary operator % for tasks.
     *
     * @sa template<typename T1, typename T2>
     *     inline ::hetcompute::task_ptr<decltype(std::declval<typename ::hetcompute::task_ptr<T1>::return_type>()
     *                                      +
     *                                      std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
     *     operator+(const ::hetcompute::task_ptr<T1>& t1, const ::hetcompute::task_ptr<T2>& t2)
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<typename ::hetcompute::task_ptr<T1>::return_type>() %
                                         std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
    operator%(const ::hetcompute::task_ptr<T1>& t1, const ::hetcompute::task_ptr<T2>& t2);

    /**
     * @brief Algebraic binary operator % for tasks.
     *
     * Algebraic binary operator % for tasks.
     *
     * @sa template<typename T1, typename T2>
     *     inline ::hetcompute::task_ptr<decltype(std::declval<T1>()
     *                                      +
     *                                      std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
     *     operator+(T1&& op1, const ::hetcompute::task_ptr<T2>& t2)
     *
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<typename ::hetcompute::task_ptr<T1>::return_type>() % std::declval<T2>())>
    operator%(const ::hetcompute::task_ptr<T1>& t1, T2&& op2);

    /**
     * @brief Algebraic binary operator % for tasks.
     *
     * Algebraic binary operator % for tasks.
     *
     * @sa template<typename T1, typename T2>
     *     inline ::hetcompute::task_ptr<decltype(std::declval<T1>()
     *                                      +
     *                                      std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
     *     operator+(T1&& op1, const ::hetcompute::task_ptr<T2>& t2)
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<T1>() % std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
    operator%(T1&& op1, const ::hetcompute::task_ptr<T2>& t2);

    /**
     * @brief Algebraic binary operator & for tasks.
     *
     * Algebraic binary operator & for tasks.
     *
     * @sa template<typename T1, typename T2>
     *     inline ::hetcompute::task_ptr<decltype(std::declval<typename ::hetcompute::task_ptr<T1>::return_type>()
     *                                      +
     *                                      std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
     *     operator+(const ::hetcompute::task_ptr<T1>& t1, const ::hetcompute::task_ptr<T2>& t2)
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<typename ::hetcompute::task_ptr<T1>::return_type>() &
                                         std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
    operator&(const ::hetcompute::task_ptr<T1>& t1, const ::hetcompute::task_ptr<T2>& t2);

    /**
     * @brief Algebraic binary operator & for tasks.
     *
     * Algebraic binary operator & for tasks.
     *
     * @sa template<typename T1, typename T2>
     *     inline ::hetcompute::task_ptr<decltype(std::declval<T1>()
     *                                      +
     *                                      std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
     *     operator+(T1&& op1, const ::hetcompute::task_ptr<T2>& t2)
     *
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<typename ::hetcompute::task_ptr<T1>::return_type>() & std::declval<T2>())>
    operator&(const ::hetcompute::task_ptr<T1>& t1, T2&& op2);

    /**
     * @brief Algebraic binary operator & for tasks.
     *
     * Algebraic binary operator & for tasks.
     *
     * @sa template<typename T1, typename T2>
     *     inline ::hetcompute::task_ptr<decltype(std::declval<T1>()
     *                                      +
     *                                      std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
     *     operator+(T1&& op1, const ::hetcompute::task_ptr<T2>& t2)
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<T1>() & std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
    operator&(T1&& op1, const ::hetcompute::task_ptr<T2>& t2);

    /**
     * @brief Algebraic binary operator ^ for tasks.
     *
     * Algebraic binary operator ^ for tasks.
     *
     * @sa template<typename T1, typename T2>
     *     inline ::hetcompute::task_ptr<decltype(std::declval<typename ::hetcompute::task_ptr<T1>::return_type>()
     *                                      +
     *                                      std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
     *     operator+(const ::hetcompute::task_ptr<T1>& t1, const ::hetcompute::task_ptr<T2>& t2)
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<typename ::hetcompute::task_ptr<T1>::return_type>() ^
                                         std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
    operator^(const ::hetcompute::task_ptr<T1>& t1, const ::hetcompute::task_ptr<T2>& t2);

    /**
     * @brief Algebraic binary operator ^ for tasks.
     *
     * Algebraic binary operator ^ for tasks.
     *
     * @sa template<typename T1, typename T2>
     *     inline ::hetcompute::task_ptr<decltype(std::declval<T1>()
     *                                      +
     *                                      std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
     *     operator+(T1&& op1, const ::hetcompute::task_ptr<T2>& t2)
     *
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<typename ::hetcompute::task_ptr<T1>::return_type>() ^ std::declval<T2>())>
    operator^(const ::hetcompute::task_ptr<T1>& t1, T2&& op2);

    /**
     * @brief Algebraic binary operator ^ for tasks.
     *
     * Algebraic binary operator ^ for tasks.
     *
     * @sa template<typename T1, typename T2>
     *     inline ::hetcompute::task_ptr<decltype(std::declval<T1>()
     *                                      +
     *                                      std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
     *     operator+(T1&& op1, const ::hetcompute::task_ptr<T2>& t2)
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<T1>() ^ std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
    operator^(T1&& op1, const ::hetcompute::task_ptr<T2>& t2);

    /**
     * @brief Algebraic binary operator | for tasks.
     *
     * Algebraic binary operator | for tasks.
     *
     * @sa template<typename T1, typename T2>
     *     inline ::hetcompute::task_ptr<decltype(std::declval<typename ::hetcompute::task_ptr<T1>::return_type>()
     *                                      +
     *                                      std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
     *     operator+(const ::hetcompute::task_ptr<T1>& t1, const ::hetcompute::task_ptr<T2>& t2)
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<typename ::hetcompute::task_ptr<T1>::return_type>() |
                                         std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
    operator|(const ::hetcompute::task_ptr<T1>& t1, const ::hetcompute::task_ptr<T2>& t2);

    /**
     * @brief Algebraic binary operator | for tasks.
     *
     * Algebraic binary operator | for tasks.
     *
     * @sa template<typename T1, typename T2>
     *     inline ::hetcompute::task_ptr<decltype(std::declval<T1>()
     *                             +
     *                             std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
     *     operator+(T1&& op1, const ::hetcompute::task_ptr<T2>& t2)
     *
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<typename ::hetcompute::task_ptr<T1>::return_type>() | std::declval<T2>())>
    operator|(const ::hetcompute::task_ptr<T1>& t1, T2&& op2);

    /**
     * @brief Algebraic binary operator | for tasks.
     *
     * Algebraic binary operator | for tasks.
     *
     * @sa template<typename T1, typename T2>
     *     inline ::hetcompute::task_ptr<decltype(std::declval<T1>()
     *                             +
     *                             std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
     *     operator+(T1&& op1, const ::hetcompute::task_ptr<T2>& t2)
     */
    template <typename T1, typename T2>
    inline ::hetcompute::task_ptr<decltype(std::declval<T1>() | std::declval<typename ::hetcompute::task_ptr<T2>::return_type>())>
    operator|(T1&& op1, const ::hetcompute::task_ptr<T2>& t2);

#endif //#ifdef ONLY_FOR_DOXYGEN

    /** @} */ /* end_addtogroup taskclass_doc */

};  // namespace hetcompute


