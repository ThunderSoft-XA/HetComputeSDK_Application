#pragma once

#include <hetcompute/internal/patterns/common.hh>

namespace hetcompute
{
    namespace internal
    {
        // Conduct pre-order traversal on the adaptive stealing tree and join
        // the results together.
        // Pre-order traversal is necessary for non-commutative reduction operators.
        //
        // Since the granuality of minimum work is large by default, there is no
        // need to conduct parallel merge. A serial merge procedure will suffice.
        template <typename T, typename Fn>
        void top_down_merge(typename ws_node<T>::node_type* root, Fn&& fn)
        {
            auto lnode = root->get_left();
            auto rnode = root->get_right();

            // work steal tree is a full binary tree
            if (lnode == nullptr && rnode == nullptr)
                return;

            top_down_merge<T, Fn>(lnode, std::forward<Fn>(fn));
            top_down_merge<T, Fn>(rnode, std::forward<Fn>(fn));

            lnode->set_value(fn(lnode->peek_value(), rnode->peek_value()));
            root->set_value(fn(root->peek_value(), lnode->peek_value()));
        }

        // Primary template for preduce dispatcher
        template <typename T, class InputIterator, typename Reduce, typename Join, bool IsIntegral>
        struct preduce_dispatcher;

        // Iterator is of integral type, dispatch to the adaptive stealing backend
        template <typename T, class InputIterator, typename Reduce, typename Join>
        struct preduce_dispatcher<T, InputIterator, Reduce, Join, true>
        {
            static T dispatch(group*                          g,
                              InputIterator                   first,
                              InputIterator                   last,
                              const T&                        identity,
                              Reduce&&                        reduce,
                              Join&&                          join,
                              const hetcompute::pattern::tuner& t)
            {
                return preduce_dynamic(g, first, last, identity, std::forward<Reduce>(reduce), std::forward<Join>(join), t);
            }
        };

        // Otherwise, convert to integral version
        template <typename T, class InputIterator, typename Reduce, typename Join>
        struct preduce_dispatcher<T, InputIterator, Reduce, Join, false>
        {
            static T dispatch(group*        g,
                              InputIterator first,
                              InputIterator last,
                              const T&      identity,
                              Reduce&&,
                              Join&&                          join,
                              const hetcompute::pattern::tuner& t)
            {
                auto fn = [first, join](size_t i, size_t j, T& init) {
                    for (size_t k = i; k < j; ++k)
                    {
                        init = join(init, *(first + k));
                    }
                };

                return preduce_dynamic(g, size_t(0), size_t(last - first), identity, std::forward<decltype(fn)>(fn), std::forward<Join>(join), t);
            }
        };

        // Adaptive stealing backend
        template <typename T, class InputIterator, typename Reduce, typename Join>
        T preduce_dynamic(group*                          group,
                          InputIterator                   first,
                          InputIterator                   last,
                          const T&                        identity,
                          Reduce&&                        reduce,
                          Join&&                          join,
                          const hetcompute::pattern::tuner& tuner)
        {
            if (first >= last)
            {
                return identity;
            }

            // We don't provide index normalization for the reduce pattern
            HETCOMPUTE_API_ASSERT(first >= 0, "Unexpected negative index");

            typedef hetcompute::internal::legacy::body_wrapper<Reduce> reduce_body_type;
            typedef hetcompute::internal::legacy::body_wrapper<Join>   join_body_type;
            reduce_body_type                                         reduce_wrapper(std::forward<Reduce>(reduce));

            auto body_attrs = reduce_wrapper.get_attrs();
            body_attrs      = legacy::add_attr(body_attrs, hetcompute::internal::attr::inlined);
            // Make sure the reduce function passed in has the right form
            HETCOMPUTE_INTERNAL_ASSERT(reduce_body_type::get_arity() == 3, "Invalid number of arguments in Reduce function");
            HETCOMPUTE_INTERNAL_ASSERT(join_body_type::get_arity() == 2, "Invalid number of arguments in Join function");

            // minimum work granularity defined here
            const size_t blk_size = tuner.get_chunk_size();

            auto g_internal = create_group();
            auto g          = g_internal;
            if (group != nullptr)
            {
                g = pattern::group_ptr_shim::shared_to_group_ptr_cast(group_intersect::intersect_impl(c_ptr(g), group));
            }

            typedef adaptive_steal_strategy<T, size_t, Reduce, Reduce, Policy::REDUCE> strategy_type;
            hetcompute_shared_ptr<strategy_type>                                         strategy_ptr(
                new strategy_type(g, first, last - 1, identity, std::forward<Reduce>(reduce), body_attrs, blk_size, tuner));
            // In general we do not need optimization for preparing pre-established work
            // stealing tree due to the poor ratio of performance gain and overhead for
            // most reductions.

            execute_master_task<T, strategy_type, Policy::REDUCE>(strategy_ptr);

            spin_wait_for(g);

            auto root = strategy_ptr->get_root();
            HETCOMPUTE_API_ASSERT(root != nullptr, "Unexpected nullptr!");
            top_down_merge<T, Join>(root, std::forward<Join>(join));

            auto result = std::move(root->peek_value());
            return result;
        }

        // Static chunking backend
        template <typename T, class InputIterator, typename Reduce, typename Join>
        T preduce_static(group*                          group,
                         InputIterator                   first,
                         InputIterator                   last,
                         const T&                        identity,
                         Reduce&&                        reduce,
                         Join&&                          join,
                         const hetcompute::pattern::tuner& tuner)
        {
            if (first >= last)
            {
                return identity;
            }

            auto g_internal = create_group();
            auto g          = g_internal;

            if (group != nullptr)
            {
                g = pattern::group_ptr_shim::shared_to_group_ptr_cast(group_intersect::intersect_impl(c_ptr(g), group));
            }

            typedef typename internal::distance_helper<InputIterator>::_result_type working_type;

            const working_type GRANULARITY_MULTIPLIER = 4;

            const working_type dist       = internal::distance(first, last);
            auto               num_chunks = GRANULARITY_MULTIPLIER * static_cast<working_type>(tuner.get_doc());
            working_type       blksz      = std::max(working_type(1), dist / num_chunks);
            num_chunks                    = dist % blksz == 0 ? dist / blksz : dist / blksz + 1;
            size_t n                      = size_t(num_chunks);
            HETCOMPUTE_INTERNAL_ASSERT(blksz > 0, "block size should be greater than zero");

            // Intermediate vector to hold temporary results of subranges.
            std::vector<T> temp_vec(n);

            std::atomic<size_t> work_id(0);

            auto chunk_body = [&temp_vec, first, last, &identity, blksz, n, &work_id, reduce]() mutable {
                while (1)
                {
                    auto prev = work_id.fetch_add(1, hetcompute::mem_order_relaxed);

                    if (prev < n)
                    {
                        InputIterator lb      = first + prev * blksz;
                        working_type  safeofs = std::min(working_type(internal::distance(lb, last)), blksz);
                        InputIterator rb      = lb + safeofs;
                        temp_vec[prev]        = std::move(identity);
                        reduce(lb, rb, temp_vec[prev]);
                    }
                    else
                    {
                        break;
                    }
                }
            };

            for (size_t i = 0; i < tuner.get_doc() - 1; ++i)
            {
                g->launch(chunk_body);
            }
            chunk_body();

            spin_wait_for(g);

            // Join results
            auto   result = std::move(temp_vec[0]);
            size_t i      = 1;
            while (i < n)
            {
                result = join(result, temp_vec[i]);
                i++;
            }

            return result;
        }

        // implementation details of user-facing interfaces
        template <typename T, class InputIterator, typename Reduce, typename Join>
        T preduce_internal(group*                          g,
                           InputIterator                   first,
                           InputIterator                   last,
                           const T&                        identity,
                           Reduce&&                        reduce,
                           Join&&                          join,
                           const hetcompute::pattern::tuner& t)
        {
            if (t.is_serial())
            {
                T result = std::move(identity);
                reduce(first, last, result);
                return result;
            }
            else if (t.is_static())
            {
                return preduce_static<T, InputIterator, Reduce, Join>(g,
                                                                      first,
                                                                      last,
                                                                      identity,
                                                                      std::forward<Reduce>(reduce),
                                                                      std::forward<Join>(join),
                                                                      t);
            }
            else
            {
                // preduce dispatcher dispatches function call to different versions
                // according to the type of iterator
                return preduce_dispatcher<T, InputIterator, Reduce, Join, std::is_integral<InputIterator>::value>::
                    dispatch(g, first, last, identity, std::forward<Reduce>(reduce), std::forward<Join>(join), t);
            }
        }

        template <typename T, class InputIterator, typename Reduce, typename Join>
        T preduce(group_ptr                       group,
                  InputIterator                   first,
                  InputIterator                   last,
                  const T&                        identity,
                  Reduce&&                        reduce,
                  Join&&                          join,
                  const hetcompute::pattern::tuner& t)
        {
            auto gptr = c_ptr(group);
            return internal::preduce_internal(gptr, first, last, identity, std::forward<Reduce>(reduce), std::forward<Join>(join), t);
        }

        template <typename T, typename Container, typename Join>
        T preduce(group_ptr group, Container& c, const T& identity, Join&& join, const hetcompute::pattern::tuner& t)
        {
            // TODO: We might need to check if c is indexable using [ ]
            // before transformation
            auto fn = [c, join](size_t i, size_t j, T& init) {
                for (size_t k = i; k < j; ++k)
                {
                    init = join(init, c[k]);
                }
            };

            return preduce(group, size_t(0), c.size(), identity, std::forward<decltype(fn)>(fn), std::forward<Join>(join), t);
        }

        template <typename T, typename Join>
        T preduce(group_ptr group, T* first, T* last, const T& identity, Join&& join, const hetcompute::pattern::tuner& t)
        {
            auto fn = [first, join](size_t i, size_t j, T& init) {
                for (size_t k = i; k < j; ++k)
                    init = join(init, *(first + k));
            };

            return preduce(group, size_t(0), size_t(last - first), identity, std::forward<decltype(fn)>(fn), std::forward<Join>(join), t);
        }

        template <typename T, class Iterator, typename Join>
        T preduce(group_ptr group, Iterator first, Iterator last, const T& identity, Join&& join, const hetcompute::pattern::tuner& t)
        {
            auto fn = [first, join](size_t i, size_t j, T& init) {
                for (size_t k = i; k < j; ++k)
                    init = join(init, *(first + k));
            };

            return preduce(group, size_t(0), size_t(last - first), identity, std::forward<decltype(fn)>(fn), std::forward<Join>(join), t);
        }

        template <typename T, class InputIterator, typename Reduce, typename Join>
        hetcompute::task_ptr<T> preduce_async(Reduce&&                        reduce,
                                            Join&&                          join,
                                            InputIterator                   first,
                                            InputIterator                   last,
                                            const T&                        identity,
                                            const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
        {
            auto g    = create_group();
            auto t    = hetcompute::create_task([g, first, last, identity, reduce, join, tuner] {
                auto v = internal::preduce(g, first, last, identity, reduce, join, tuner);
                return v;
            });
            auto gptr = internal::c_ptr(g);
            gptr->set_representative_task(internal::c_ptr(t));
            return t;
        }

        template <typename T, typename Container, typename Join>
        hetcompute::task_ptr<T>
        preduce_async(Join&& join, Container& c, const T& identity, const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
        {
            auto g    = create_group();
            auto t    = hetcompute::create_task([g, c, identity, join, tuner] {
                auto v = internal::preduce(g, c, identity, join, tuner);
                return v;
            });
            auto gptr = internal::c_ptr(g);
            gptr->set_representative_task(internal::c_ptr(t));
            return t;
        }

        template <typename T, typename Join>
        hetcompute::task_ptr<T>
        preduce_async(Join&& join, T* first, T* last, const T& identity, const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
        {
            auto g    = create_group();
            auto t    = hetcompute::create_task([g, first, last, identity, join, tuner] {
                auto v = internal::preduce(g, first, last, identity, std::forward<Join>(join), tuner);
                return v;
            });
            auto gptr = internal::c_ptr(g);
            gptr->set_representative_task(internal::c_ptr(t));
            return t;
        }

        template <typename T, typename Iterator, typename Join>
        hetcompute::task_ptr<T> preduce_async(Join&&                          join,
                                            Iterator                        first,
                                            Iterator                        last,
                                            const T&                        identity,
                                            const hetcompute::pattern::tuner& tuner = hetcompute::pattern::tuner())
        {
            auto g    = create_group();
            auto t    = hetcompute::create_task([g, first, last, identity, join, tuner] {
                auto v = internal::preduce(g, first, last, identity, std::forward<Join>(join), tuner);
                return v;
            });
            auto gptr = internal::c_ptr(g);
            gptr->set_representative_task(internal::c_ptr(t));
            return t;
        }

    };  // namespace internal
};  // namespace hetcompute
