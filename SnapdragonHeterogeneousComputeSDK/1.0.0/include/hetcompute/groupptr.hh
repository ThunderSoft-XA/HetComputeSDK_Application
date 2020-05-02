#pragma once

#include <string>
#include <hetcompute/group.hh>
// TODO Uncomment once patterns are migrated
//#include <hetcompute/tuner.hh>
#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/util/hetcomputeptrs.hh>
#include <hetcompute/internal/task/group_impl.hh>

namespace hetcompute
{
    /// @cond PRIVATE
    // Forward declarations
    // --------------------------------------------------------------------
    class group_ptr;

    group_ptr create_group();
    group_ptr create_group(std::string const& name);

    namespace internal
    {
        group* c_ptr(::hetcompute::group_ptr& g);
        group* c_ptr(::hetcompute::group_ptr const& g);

        namespace testing
        {
            class group_tester;
        }; // namespace hetcompute::internal::testing

        namespace pattern
        {
            class group_ptr_shim;
        }; // namespace hetcompute::internal::pattern

    }; // namespace hetcompute::internal

    // --------------------------------------------------------------------
    /// @endcond

    /** @addtogroup groupclass_doc
    @{
    */

    /**
     * @brief Smart pointer to a group object.
     *
     * Smart pointer to a group object, similar to <code>std::shared_ptr</code>.
     *
     */
    class group_ptr
    {
        /** @cond PRIVATE */
        friend ::hetcompute::internal::group* ::hetcompute::internal::c_ptr(::hetcompute::group_ptr& g);
        friend ::hetcompute::internal::group* ::hetcompute::internal::c_ptr(::hetcompute::group_ptr const& g);

        friend group_ptr create_group(std::string const&);
        friend group_ptr create_group(const char*);
        friend group_ptr create_group();
        friend group_ptr intersect(group_ptr const& a, group_ptr const& b);

        friend class ::hetcompute::internal::testing::group_tester;
        friend class ::hetcompute::internal::pattern::group_ptr_shim;
        friend class ::hetcompute::group;
        /** @endcond */

    public:
        /**
         * @brief Default constructor. Constructs a <code>group_ptr</code>
         * with no <code>group</code>.
         *
         * Constructs a <code>group_ptr</code> that manages no <code>group</code>.
         *  <code>group_ptr::get</code> returns <code>nullptr</code>.
         */
        group_ptr() : _shared_ptr(nullptr) {}

        /**
         * @brief Default constructor. Constructs a <code>group_ptr</code>
         * with no <code>group</code>.
         *
         * Constructs a <code>group_ptr</code> that manages no <code>group</code>.
         * <code>group_ptr::get</code> returns <code>nullptr</code>.
         */
        /* implicit */ group_ptr(std::nullptr_t) : _shared_ptr(nullptr) {}

        /**
         * @brief Copy constructor. Constructs a <code>group_ptr</code> that
         * manages the same group as <code>other</code>.
         *
         * Constructs a <code>group_ptr</code> object that manages the same group
         * as <code>other</code>.  If <code>other</code> points to
         * <code>nullptr</code>, the newly built object points to
         * <code>nullptr</code> as well.
         *
         * @param other Group pointer to copy.
         *
         */
        group_ptr(group_ptr const& other) : _shared_ptr(other._shared_ptr) {}

        /**
         * @brief Move constructor. Move-constructs a <code>group_ptr</code> that
         * manages the same group as <code>other</code>.
         *
         * Constructs a <code>group_ptr</code> object that manages the same
         * group as <code>other</code> and resets <code>other</code>. If
         * <code>other</code> points to <code>nullptr</code>, the newly
         * built object points to <code>nullptr</code> as well.
         *
         * @param other Group pointer to move from.
         *
         */
        group_ptr(group_ptr&& other) : _shared_ptr(std::move(other._shared_ptr)) {}

        /**
         * @brief Assignment operator. Assigns the group managed by <code>other</code>
         * to <code>*this</code>.
         *
         * Assigns the group managed by <code>other</code> to <code>*this</code>.
         * If, before the assignment, <code>*this</code> was the last
         * <code>group_ptr</code> pointing to a group <code>g</code>, then the
         * assignment will cause <code>g</code> to be destroyed. If <code>other</code>
         * manages no object, <code>*this</code> will not manage an object either after
         * the assignment.
         *
         * @param other Group pointer to copy.
         *
         * @return <code>*this</code>.
         */
        group_ptr& operator=(group_ptr const& other)
        {
            _shared_ptr = other._shared_ptr;
            return *this;
        }

        /**
         * @brief Assignment operator. Resets <code>*this</code>.
         *
         * Resets <code>*this</code> so that it manages no object. If,
         * before the assignment, <code>*this</code> was the last
         * <code>group_ptr</code> pointing to a group <code>g</code>, then
         * the assignment will cause <code>g</code> to be destroyed. If
         * <code>other</code> manages no object, <code>*this</code> will not
         * manage an object either after the assignment.
         *
         * @return <code>*this</code>.
         *
         */
        group_ptr& operator=(std::nullptr_t)
        {
            _shared_ptr = nullptr;
            return *this;
        }

        /**
         * @brief Move-assignment operator. Move-assigns the group managed
         * by <code>other</code> to <code>*this</code>.
         *
         * Move-assigns the group managed by <code>other</code> to
         * <code>*this</code>. <code>other</code> will manage no group
         * after the assignment.
         *
         * If, before the assignment, <code>*this</code> was the last
         * <code>group_ptr</code> pointing to a group <code>g</code>, then
         * the assignment will cause <code>g</code> to be destroyed. If
         * <code>other</code> manages no object, <code>*this</code> will not
         * manage an object either after the assignment.
         *
         * @param other Group pointer to move from.
         *
         * @return <code>*this</code>.
         */
        group_ptr& operator=(group_ptr&& other)
        {
            _shared_ptr = (std::move(other._shared_ptr));
            return *this;
        }

        /**
         * @brief Exchanges managed groups between <code>*this</code> and
         * <code>other</code>.
         *
         *  Exchanges managed groups between <code>*this</code> and
         * <code>other</code>.
         *
         * @param other Group pointer to exchange with.
         */
        void swap(group_ptr& other) { std::swap(_shared_ptr, other._shared_ptr); }

        /**
         * @brief Dereference operator. Returns pointer to the managed group.
         *
         * Returns pointer to the managed group. Do not call this member
         * function if <code>*this</code> does not manage a group. This would
         * cause a fatal error.
         *
         *
         * @return Pointer to managed group object.
         */
        group* operator->() const
        {
            HETCOMPUTE_INTERNAL_ASSERT(_shared_ptr != nullptr, "Invalid group ptr");
            auto g = get_raw_ptr();
            HETCOMPUTE_INTERNAL_ASSERT(g != nullptr, "Invalid null pointer.");
            return g->get_facade<group>();
        }

        /**
         * @brief Returns pointer to managed group.
         *
         * Returns pointer to the managed group. Remember that the lifetime
         * of the group is defined by the lifetime of the
         * <code>group_ptr</code> objects managing it. If all
         * <code>group_ptr</code> objects managing a group <code>g</code>
         * go out of scope, all <code>group*</code> pointing to
         * <code>g</code> may be invalid.
         *
         *  @return Pointer to managed group object.
         */
        group* get() const
        {
            auto t = get_raw_ptr();
            if (t == nullptr)
                return nullptr;
            return t->get_facade<group>();
        }

        /**
         * @brief Resets pointer to managed group.
         *
         * Resets pointer to managed group. If, <code>*this</code> was the last
         * <code>group_ptr</code> pointing to a group <code>g</code>, then
         * <code>reset()</code> cause <code>g</code> to be destroyed.
         *
         * @throw api_exception If group pointer is <code>nullptr</code>.
         *
         */
        void reset() { _shared_ptr.reset(); }

        /**
         * @brief Checks whether pointer is not <code>nullptr</code>.
         *
         * Checks whether <code>*this</code> manages a group.
         *
         *  @return
         *    <code>true</code> -- The pointer is not <code>nullptr</code> (<code>*this</code> manages a group).\n
         *    <code>false</code> -- The pointer is <code>nullptr</code> (<code>*this</code> does not manage a group).
         */
        explicit operator bool() const { return _shared_ptr != nullptr; }

        /**
         * @brief Returns the number of <code>group_ptr</code> objects
         * managing the same object (including <code>*this</code>).
         *
         * Returns the number of <code>group_ptr</code> objects managing the
         * same object (including <code>*this</code>). Notice that the HETCOMPUTE
         * runtime keeps one internal <code>group_ptr</code> to a group
         * if the group contains one or more tasks. This is to prevent
         * a group from disappearing while it has tasks.
         *
         * @includelineno samples/src/group_ptr1.cc
         *
         * @par Output
         * @code
         * After construction: g.use_count() = 1
         * After copy-construction: g2.use_count() = 2
         * Task in g running. g.use_count() = 3
         * After calling g.get(). g.use_count() = 3
         * After g->wait_for: g.use_count() = 2
         * @endcode
         *
         * @return Total number of <code>group_ptr</code>
         */
        size_t use_count() const { return _shared_ptr.use_count(); }

        /**
         * @brief Checks whether <code>*this</code> is the
         * only<code>group_ptr</code> managing the same <code>group</code>
         * object.
         *
         * Checks whether <code>*this</code> is the
         * only<code>group_ptr</code> managing the same <code>group</code>
         * object. If <code>*this</code> does not manage any
         * <code>group</code>, <code>unique()</code> returns
         * <code>false</code>.
         *
         * It is equivalent to checking whether <code>use_count</code> is
         * <code>1</code>, except that it is more efficient.
         *
         *
         *  @return
         *    <code>true</code> -- The pointer is the only <code>group_ptr</code> managing the group.
         *    <code>false</code> -- The pointer is not the only <code>group_ptr</code> managing the group
         *    or <code>*this</code> is <code>nullptr</code>.
         */
        bool unique() const { return _shared_ptr.use_count() == 1; }

        // Default destructor... empty destructor
        ~group_ptr() {}

        /** @cond PRIVATE */

    protected:
        explicit group_ptr(internal::group* g) : _shared_ptr(g) {}

        group_ptr(internal::group* g, internal::group_shared_ptr::ref_policy policy) : _shared_ptr(g, policy) {}

        explicit group_ptr(internal::group_shared_ptr&& g)
            : _shared_ptr(g.reset_but_not_unref(), internal::group_shared_ptr::ref_policy::no_initial_ref)
        {
        }

        internal::group* get_raw_ptr() const { return ::hetcompute::internal::c_ptr(_shared_ptr); }

        // TODO: we add this function because we cannot get rid of
        // attrs in pattern calls and have to use legacy::launch method
        // which only accepts group_shared_ptr in pre-API20.
        // Once we can handle with_attrs() in API20 launch or get
        // rid of attrs completely, we should remove this protected method
        // as well.
        internal::group_shared_ptr get_shared_ptr() const { return _shared_ptr; }
        /** @endcond */

    private:
        internal::group_shared_ptr _shared_ptr;

        static_assert(sizeof(group) == sizeof(internal::group::self_ptr), "Invalid group size.");

    };  // class group_ptr

    /**
     *  @brief Compares group <code>g</code> to <code>nullptr</code>.
     */
    inline bool operator==(group_ptr const& g, std::nullptr_t) { return !g; }

    /**
     *  @brief Compares <code>nullptr</code> to group <code>g</code>.
     */
    inline bool operator==(std::nullptr_t, group_ptr const& g) { return !g; }

    /**
     *  @brief Compares group <code>g</code> to <code>nullptr</code>.
     */
    inline bool operator!=(group_ptr const& g, std::nullptr_t) { return static_cast<bool>(g); }

    /**
     *  @brief Compares <code>nullptr</code> to group <code>g</code>.
     */
    inline bool operator!=(std::nullptr_t, group_ptr const& g) { return static_cast<bool>(g); }

    /**
     *  @brief Compares group <code>a</code> to group <code>b</code>.
     */
    inline bool operator==(group_ptr const& a, group_ptr const& b) { return hetcompute::internal::c_ptr(a) == hetcompute::internal::c_ptr(b); }

    /**
     *  @brief Compares group <code>a</code> to group <code>b</code>.
     */
    inline bool operator!=(group_ptr const& a, group_ptr const& b) { return hetcompute::internal::c_ptr(a) != hetcompute::internal::c_ptr(b); }

    /**
     * @brief Creates a named group and returns a <code>group_ptr</code>
     * that points to the group.
     *
     * Creates a named group and returns a <code>group_ptr</code> that
     * points to it. Named groups can facilitate debugging of complex
     * applications. Keep in mind, that Qualcomm HetCompute will make a copy of
     * <code>name</code>, which may cause a slight overhead if you repeatedly
     * create and destroy groups.
     *
     * <code>name</code> does not have to be unique. Qualcomm HetCompute does not ensure
     * it, so two or more groups can share the same name.
     *
     * @param name Group name.
     *
     *  @return
     *   group_ptr -- Pointer to the new group.
     *
     *  @par Example
     *  @includelineno samples/src/group_create1.cc
     *
     *  @sa <code>hetcompute::create_group()</code>
     *  @sa <code>hetcompute::create_group(std::string const&)</code>
     */

    inline group_ptr create_group(const char* name) { return group_ptr(::hetcompute::internal::group_factory::create_leaf_group(name)); }

    /**
     * @brief Creates a named group and returns a <code>group_ptr</code> that
     * points to the group.
     *
     * Creates a named group and returns a <code>group_ptr</code> that
     * points to it. Named groups can facilitate debugging of complex
     * applications. Keep in mind, that Qualcomm HetCompute will make a copy of
     * <code>name</code>, which may cause a slight overhead if you repeatedly
     * create and destroy groups.
     *
     * <code>name</code> does not have to be unique. Qualcomm HetCompute does not ensure
     * it, so two or more groups can share the same name.
     *
     * @param name Group name.
     *
     *  @return
     *   group_ptr -- Pointer to the new group.
     *
     *  @par Example
     *  @includelineno samples/src/group_create1.cc
     *
     *    @sa <code>hetcompute::create_group()</code>
     *    @sa <code>hetcompute::create_group(const char*)</code>
     */
    inline group_ptr create_group(std::string const& name)
    {
        return group_ptr(::hetcompute::internal::group_factory::create_leaf_group(name));
    }

    /**
     * @brief Creates a group and returns a <code>group_ptr</code> that points
     * to the group.
     *
     * Creates a group and returns a <code>group_ptr</code> that points to it.
     *
     *  @return group_ptr Pointer to the new group.
     *
     *  @par Example
     *  @includelineno samples/src/group_create1.cc
     *
     *    @sa <code>hetcompute::create_group()</code>
     *    @sa <code>hetcompute::create_group(const char*)</code>
     */
    inline group_ptr create_group() { return group_ptr(::hetcompute::internal::group_factory::create_leaf_group()); }

    /**
     * @brief Returns a pointer to a group that represents the intersection of
     * two groups.
     *
     * Returns a pointer to a group that represents the intersection of
     * two groups. Some applications require that tasks join more than
     * one group. It is possible, though not recommended for performance
     * reasons, to use <code>hetcompute::group::launch(hetcompute::task_ptr<>
     * const&)</code> or <code>hetcompute::group::add(hetcompute::task_ptr<>
     * const&)</code> repeatedly to add a task to several groups. Instead,
     * use <code>hetcompute::intersect(group_ptr const&, group_ptr
     * const&)</code> to create a new group that represents the
     * intersection of all the groups where the tasks need to
     * launch. Again, this method is more performant than repeatedly
     * launching the same task into different groups.
     *
     * Launching a task into the intersection group also simultaneously
     * launches it into all the groups that are part of the intersection.
     *
     * Consecutive calls to <code>hetcompute::intersect</code> with the same groups'
     * pointer as arguments, return a pointer to the same group.
     *
     * Group intersection is a commutative operation.
     *
     * You can use the <code>&</code> operator instead of
     * <code>hetcompute::group::intersect</code>.
     *
     * @param a Group pointer to the first group.
     * @param b Group pointer to the second group.
     *
     * @return group_ptr -- Group pointer that points to a group that
     *  represents the intersection of <code>a</code> and <code>b</code>.
     *
     *  @par Example
     *  @includelineno samples/src/group_intersection1.cc
     *
     *  @par
     *  The example above shows an application with three groups:
     *  <code>g1</code>, <code>g2</code>, and their intersection
     *  <code>g12</code>. We launch thousands of tasks on both
     *  <code>g1</code> and <code>g2</code>. We then wait for
     *  <code>g12</code> (line 23), but <code>g12->wait_for()</code>
     *  returns immediately because <code>g12</code> is empty. This is
     *  because at this point no task belongs to both
     *  <code>g1</code> and <code>g2</code>.  We then launch a task into
     *  <code>g12</code> (line 29). <code>g1->wait_for()</code> and
     *  <code>g2->wait_for()</code> return only after the task in
     *  <code>g12</code> completes execution because it belongs to
     *  <code>g1</code>, <code>g2</code>, and <code>g12</code>.
     *
     *  @sa <code>hetcompute::operator&(hetcompute::group_ptr const&,  hetcompute::group_ptr const&)</code>
     */
    group_ptr intersect(group_ptr const& a, group_ptr const& b);

    /**
     * @brief Returns a pointer to a group that represents the intersection of
     * two groups.
     *
     * @param a Group pointer to the first group to intersect.
     * @param b Group pointer to the second group to intersect.
     *
     * @return group_ptr -- Group pointer that points to a group that
     *  represents the intersection of <code>a</code> and <code>b</code>.
     *
     * @sa <code>hetcompute::intersect(hetcompute::group_ptr const&, hetcompute::group_ptr const&)</code>.
     */
    inline group_ptr operator&(group_ptr const& a, group_ptr const& b) { return ::hetcompute::intersect(a, b); }

    /**
     * @cond PRIVATE
     * @brief Specifies that the task invoking this function should be
     * deemed to finish only after tasks in group <code>g</code> finish.
     *
     * Specifies that the task invoking this function should be deemed to
     * finish only after tasks in <code>g</code> finish. This method returns
     * immediately.
     *
     * If the invoking task is multi-threaded, the programmer must ensure that
     * concurrent calls to <code>finish_after</code> from within the task are
     * properly synchronized.
     *
     * @param g Pointer to group.
     *
     * @throws api_exception If invoked from outside a task or from within
     * a <code>hetcompute::pfor_each</code> or if <code>g</code> points to
     * <code>nullptr</code>
     * @note If exceptions are disabled by the application, the API terminates
     *       in the above listed error conditions.
     * @endcond PRIVATE
     */
    void finish_after(group* g);

    /**
     * @brief Specifies that the task invoking this function should be
     * deemed to finish only after tasks in group <code>g</code> finish.
     *
     * Specifies that the task invoking this function should be deemed to
     * finish only after tasks in <code>g</code> finish. This method returns
     * immediately.
     *
     * If the invoking task is multi-threaded, the programmer must ensure that
     * concurrent calls to <code>finish_after</code> from within the task are
     * properly synchronized.
     *
     * @param g Group pointer.
     *
     * @throws api_exception If invoked from outside a task or from within
     * a <code>hetcompute::pfor_each</code> or if <code>g</code> points to
     * <code>nullptr</code>
     * @note If exceptions are disabled by the application, the API terminates
     *       in the above listed error conditions.
     */
    void finish_after(group_ptr const& g);

    /** @} */ /* end_addtogroup groupclass_doc */

    /// @cond PRIVATE
    /// Hide this from doxygen
    namespace internal
    {
        inline ::hetcompute::internal::group* c_ptr(::hetcompute::group_ptr& g) { return g.get_raw_ptr(); }

        inline ::hetcompute::internal::group* c_ptr(::hetcompute::group_ptr const& g) { return g.get_raw_ptr(); }

        /// Launch list of kernels into group gptr
        /// @param gptr group-pointer to group into which to launch kernels
        /// @param kernels std::list of kernels of the same type to launch
        ///
        /// @note Works only with lvalue kernel list
        template <typename Code>
        void launch(hetcompute::group_ptr& gptr, std::list<Code>& kernels)
        {
            auto sz = kernels.size();
            if (sz == 0)
            {
                return;
            }

            auto g = c_ptr(gptr);
            HETCOMPUTE_INTERNAL_ASSERT(g != nullptr, "Unexpected null group.");

            using decayed_type = typename std::decay<Code>::type;

            static_assert(!::hetcompute::internal::is_hetcompute_task_ptr<decayed_type>::value, "can only launch multiple kernels, not tasks");
            using launch_policy = hetcompute::internal::group_launch::launch_code<Code>;

            for (auto k = kernels.begin(), e = kernels.end(); k != e; k++)
            {
                launch_policy::launch_impl(false, g, *k, std::false_type());
            }
            hetcompute::internal::notify_all(sz);
        }

        void spin_wait_for(hetcompute::group_ptr& gptr);

    }; // namespace hetcompute::internal
    /// @endcond

};  // namespace hetcomppute
