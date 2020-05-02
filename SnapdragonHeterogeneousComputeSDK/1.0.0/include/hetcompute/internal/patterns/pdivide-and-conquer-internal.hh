#pragma once

#include <hetcompute/internal/patterns/common.hh>

namespace hetcompute
{
    namespace internal
    {
        /// Node in the pdivide-and-conquer work tree
        /// Captures a Problem and stores computed Solution
        template <typename Problem, typename Solution>
        struct dnc_node
        {
            explicit dnc_node(Problem p) : _problem(p), _solution(), _subproblems() {}

            // The following two methods are recommended if Problem/Solution is a pointer
            // type.
            dnc_node(const dnc_node& other)
            {
                _problem     = other._problem;
                _solution    = other._solution;
                _subproblems = other._subproblems;
            }

            dnc_node& operator=(const dnc_node& other)
            {
                if (this != &other)
                {
                    _problem     = other._problem;
                    _solution    = other._solution;
                    _subproblems = other._subproblems;
                }
                return *this;
            }

            Problem  _problem;
            Solution _solution;

            // Any subproblems into which _problem is split
            std::vector<std::shared_ptr<dnc_node>> _subproblems;
        };

        /// Workhorse of the pdivide_and_conquer pattern.
        ///
        /// is_base_case, base, split, and merge are lambdas used in the
        /// divide-and-conquer algorithm. See the pdivide_and_conquer documentation for
        /// what each means.
        ///
        /// @param g         Launch any internal tasks into this group if not null
        /// @param c         Task representing continuation of work tree node 'n'
        /// @param n         Work tree node associated with problem
        /// @param is_base_n True if 'n' is a base case problem, false otherwise
        template <typename Problem, typename Solution, typename Fn1, typename Fn2, typename Fn3, typename Fn4>
        void pdivide_and_conquer_internal(group_ptr                    g,
                                          task_ptr<>                   c,
                                          dnc_node<Problem, Solution>* n,
                                          Fn1&&                        is_base_case,
                                          Fn2&&                        base,
                                          Fn3&&                        split,
                                          Fn4&&                        merge,
                                          bool                         is_base_n)
        {
            HETCOMPUTE_INTERNAL_ASSERT(n->_subproblems.empty(), "work node should have no subproblems just yet");

            if (is_base_n)
            {
                HETCOMPUTE_INTERNAL_ASSERT(c == nullptr, "if base, continuation should be null");
                n->_solution = base(n->_problem);
            }
            else
            {
                for (auto i : split(n->_problem))
                {
                    // Lifetime of node_i is the same as the lifetime of its parent, n.
                    // Use smart pointer so that node is automatically deallocated in
                    // exceptional circumstances / early returns.
                    auto node_i = std::make_shared<dnc_node<Problem, Solution>>(i);
                    n->_subproblems.push_back(node_i);
                    // Create child task's continuation here itself.
                    // This is so that the current task's continuation may have the right
                    // dependencies on either the children tasks or the children tasks'
                    // continuations.
                    auto is_base_i  = is_base_case(i);
                    auto cont_child = is_base_i ? nullptr : hetcompute::create_task([=]() {
                        // This continuation will not be invoked if there isn't atleast one
                        // subproblem.
                        HETCOMPUTE_INTERNAL_ASSERT(!node_i->_subproblems.empty(), "need atleast one subproblem to merge");
                        std::vector<Solution> sols(node_i->_subproblems.size());
                        std::transform(node_i->_subproblems.begin(),
                                       node_i->_subproblems.end(),
                                       sols.begin(),
                                       [](std::shared_ptr<dnc_node<Problem, Solution>> m) { return m->_solution; });
                        node_i->_solution = merge(i, sols);
                    });

                    // Create child task
                    auto task_child = hetcompute::create_task([=] {
                        // Pass node_i's raw pointer to function because function argument's
                        // lifetime is exceeded by lifetime of node_i itself. Therefore, we
                        // don't need to track references in this function call.
                        internal::pdivide_and_conquer_internal(g, cont_child, node_i.get(), is_base_case, base, split, merge, is_base_i);
                    });
                    internal::c_ptr(task_child)->launch(internal::c_ptr(g), nullptr);

                    // If child task is a base case, current task's continuation depends on
                    // the child task; else, current task's continuation depends on the child
                    // task's continuation.
                    auto c_pred = is_base_i ? task_child : cont_child;

                    c_pred->then(c);
                }
                internal::c_ptr(c)->launch(internal::c_ptr(g), nullptr);
            }
        }

        /// Specialize for no "Solution"
        template <typename Problem, typename Fn1, typename Fn2, typename Fn3, typename Fn4>
        void
        pdivide_and_conquer_internal(group_ptr g, task_ptr<> c, Problem p, Fn1&& is_base_case, Fn2&& base, Fn3&& split, Fn4&& merge, bool is_base_p)
        {
            if (is_base_p)
            {
                HETCOMPUTE_INTERNAL_ASSERT(c == nullptr, "if base, continuation should be null");
                base(p);
            }
            else
            {
                for (auto i : split(p))
                {
                    // Create child task's continuation here itself.
                    // This is so that the current task's continuation may have the right
                    // dependencies on either the children tasks or the children tasks'
                    // continuations.
                    auto is_base_i  = is_base_case(i);
                    auto cont_child = is_base_i ? nullptr : hetcompute::create_task([=] { merge(i); });

                    // Create child task
                    auto task_child = hetcompute::create_task(
                        [=] { internal::pdivide_and_conquer_internal(g, cont_child, i, is_base_case, base, split, merge, is_base_i); });
                    internal::c_ptr(task_child)->launch(internal::c_ptr(g), nullptr);

                    // If child task is a base case, current task's continuation depends on
                    // the child task; else, current task's continuation depends on the child
                    // task's continuation.
                    auto c_pred = is_base_i ? task_child : cont_child;

                    c_pred->then(c);
                }
                internal::c_ptr(c)->launch(internal::c_ptr(g), nullptr);
            }
        }

        /// Specialize for no "Solution" and no "merge"
        ///
        /// In contrast to the other pdivide_and_conquer implementations, the lack of a
        /// solution and merge step simplifies implementation. A tree of tasks is
        /// created, with all tasks launched into a single group, and the group is
        /// waited upon.
        template <typename Problem, typename Fn1, typename Fn2, typename Fn3>
        void pdivide_and_conquer_internal(group_ptr g, Problem p, Fn1&& is_base_case, Fn2&& base, Fn3&& split, bool is_base_p)
        {
            HETCOMPUTE_INTERNAL_ASSERT(g != nullptr, "group must not be null");
            if (is_base_p)
            {
                base(p);
            }
            else
            {
                for (auto i : split(p))
                {
                    auto is_base_i = is_base_case(i);
                    auto f         = [=] { internal::pdivide_and_conquer_internal(g, i, is_base_case, base, split, is_base_i); };
                    // If there are tasks in the local queue, it means other threads are
                    // likely busy doing work. Because if they were not busy, then they would
                    // have stolen the tasks from the local queue. Therefore, if there are
                    // tasks in the local queue, do not create a new task. Instead, execute
                    // the function directly. In divide-and-conquer, there will be
                    // opportunities later to create tasks if other threads are idle.
                    auto load = internal::get_load_on_current_thread();
                    if (load == 0)
                    {
                        // Launch child task
                        g->launch(f);
                    }
                    else
                    {
                        // Execute inline
                        f();
                    }
                }
            }
        }

        template <typename Problem, typename Solution, typename Fn1, typename Fn2, typename Fn3, typename Fn4>
        hetcompute::task_ptr<Solution> pdivide_and_conquer(group_ptr g, Problem p, Fn1&& is_base, Fn2&& base, Fn3&& split, Fn4&& merge)
        {
            if (is_base(p))
            {
                return hetcompute::create_value_task<Solution>(base(p));
            }
            else
            {
                // Lifetime of n is this lexical scope.
                // Use smart pointer so that node is automatically deallocated in
                // exceptional circumstances / early returns.
                // Lifetime of nodes corresponding to subproblems in n->_subproblems
                // corresponds to the lifetime of n. This means that the tree of nodes will
                // be deallocated only at the end of this lexical scope! In reality, for
                // any node ni, ni->_subproblems needs to be live only until the
                // 'merge' task associated with ni executes. However, orchestrating this in
                // the presence of tasks executing lambdas is hard -- there is a tension
                // between the need for dynamic transfer of exclusive ownership and the
                // existing facility for static transfer of exclusive ownership via
                // std::move in lexical scopes.
                auto n    = std::make_shared<internal::dnc_node<Problem, Solution>>(p);
                auto cont = hetcompute::create_task([n, p, merge] {
                    // n->_subproblems could be empty if split did not create any subproblems
                    std::vector<Solution> sols(n->_subproblems.size());
                    std::transform(n->_subproblems.begin(),
                                   n->_subproblems.end(),
                                   sols.begin(),
                                   [](std::shared_ptr<internal::dnc_node<Problem, Solution>> m) { return m->_solution; });
                    n->_solution = merge(p, sols);
                    return n->_solution;
                });
                // Pass raw pointer to n to function because function argument's lifetime
                // is exceeded by lifetime of n itself. Therefore, we don't need to track
                // references in this function call.
                internal::pdivide_and_conquer_internal(g, cont, n.get(), is_base, base, split, merge, false /* not base case */);
                return cont;
            }
        }

        template <typename Problem, typename Fn1, typename Fn2, typename Fn3, typename Fn4>
        void pdivide_and_conquer(group_ptr g, Problem p, Fn1&& is_base, Fn2&& base, Fn3&& split, Fn4&& merge, bool async = false)
        {
            HETCOMPUTE_INTERNAL_ASSERT(g == nullptr || async == true, "g should be valid only for async invocations");
            if (is_base(p))
            {
                base(p);
            }
            else
            {
                auto c = hetcompute::create_task([merge, p] { merge(p); });
                internal::pdivide_and_conquer_internal(g, c, p, is_base, base, split, merge, false /* not base case */);
                if (async)
                    g->finish_after();
                else
                    c->wait_for();
            }
        }

        template <typename Problem, typename Fn1, typename Fn2, typename Fn3>
        void pdivide_and_conquer(group_ptr g, Problem p, Fn1&& is_base, Fn2&& base, Fn3&& split, bool async = false)
        {
            HETCOMPUTE_INTERNAL_ASSERT(g == nullptr || async == true, "g should be valid only for async invocations");
            if (is_base(p))
            {
                base(p);
            }
            else
            {
                if (async || g == nullptr)
                    g = hetcompute::create_group();
                internal::pdivide_and_conquer_internal(g, p, is_base, base, split, false /* not base case */);
                if (async)
                    g->finish_after();
                else
                    g->wait_for();
            }
        }

        template <typename Problem, typename Solution, typename Fn1, typename Fn2, typename Fn3, typename Fn4>
        hetcompute::task_ptr<Solution> pdivide_and_conquer_async(Fn1&& is_base, Fn2&& base, Fn3&& split, Fn4&& merge, Problem p)
        {
            auto g    = create_group();
            auto t    = hetcompute::create_task([g, p, is_base, base, split, merge] {
                return hetcompute::internal::pdivide_and_conquer<Problem, Solution>(g, p, is_base, base, split, merge);
            });
            auto gptr = internal::c_ptr(g);
            gptr->set_representative_task(internal::c_ptr(t));
            return t;
        }

        template <typename Problem, typename Fn1, typename Fn2, typename Fn3, typename Fn4>
        hetcompute::task_ptr<> pdivide_and_conquer_async(Fn1&& is_base, Fn2&& base, Fn3&& split, Fn4&& merge, Problem p)
        {
            auto g = hetcompute::create_group();
            auto t = hetcompute::create_task(
                [g, p, is_base, base, split, merge] { hetcompute::internal::pdivide_and_conquer(g, p, is_base, base, split, merge, true); });
            auto gptr = internal::c_ptr(g);
            gptr->set_representative_task(internal::c_ptr(t));
            return t;
        }

        template <typename Problem, typename Fn1, typename Fn2, typename Fn3>
        hetcompute::task_ptr<> pdivide_and_conquer_async(Fn1&& is_base, Fn2&& base, Fn3&& split, Problem p)
        {
            auto g = hetcompute::create_group();
            auto t = hetcompute::create_task(
                [g, p, is_base, base, split] { hetcompute::internal::pdivide_and_conquer(g, p, is_base, base, split, true); });
            auto gptr = internal::c_ptr(g);
            gptr->set_representative_task(internal::c_ptr(t));
            return t;
        }

    };  // namespace internal
};  // namespace hetcompute
