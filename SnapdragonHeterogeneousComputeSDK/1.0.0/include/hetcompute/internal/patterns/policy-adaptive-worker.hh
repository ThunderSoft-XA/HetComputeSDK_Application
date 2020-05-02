#pragma once

#include <hetcompute/internal/patterns/policy-adaptive.hh>

namespace hetcompute
{
    namespace internal
    {
        // Work on a worksteal tree node until stealing is detected or finish.
        // This API is the evil twin brother of try_steal. If progress is modified
        // by the stealer task, the owner will start looking for work from the
        // stolen node instead of from the root of the tree.
        // Return true if complete the range and false if detected stolen.
        //
        // Dispatch to different versions of implementations in line with
        // separate patterns

        // Ignore pragma outside the function to comply with MSVS warning rules
        // http://msdn.microsoft.com/en-us/library/vstudio/2c8f766e%28v=vs.100%29.aspx

        // Primary template for the worker class
        template <typename T, typename Fn, Policy P>
        struct worker_wrapper;

        // Specialize for preduce
        template <typename T, typename Fn>
        struct worker_wrapper<T, Fn, Policy::REDUCE>
        {
            static bool work_on(typename ws_node<T>::node_type* node, Fn&& fn)
            {
                HETCOMPUTE_INTERNAL_ASSERT(node != nullptr, "Unexpectedly work on null pointer");
                HETCOMPUTE_INTERNAL_ASSERT(node->is_unclaimed(hetcompute::mem_order_relaxed) == false,
                                         "Invalid node state %zu",
                                         node->get_progress(hetcompute::mem_order_relaxed));

                typename ws_node<T>::size_type first = node->get_first();
                typename ws_node<T>::size_type last  = node->get_last();

                if (first > last)
                {
                    node->set_completed();
                    return true;
                }

                auto                           blk_size = node->get_tree()->get_blk_size();
                auto                           i        = first;
                typename ws_node<T>::size_type right_bound;

                // if it's root, or the first left child in pre-built tree, we must
                // finish the range [first, first + _blk_size)
                // before checking for stealing
                //
                if (first == node->get_tree()->range_start())
                {
                    right_bound = i + blk_size;
                    right_bound = right_bound > last ? last + 1 : right_bound;
                    fn(i, right_bound, node->peek_value());
                    i = right_bound;
                    if (right_bound == last + 1)
                    {
                        return true;
                    }
                }

                // work on assigned iterations and check for stealing after
                // a batch is done.
                do
                {
                    // process blk_size iterations in a batch
                    right_bound = i + blk_size;
                    right_bound = right_bound > last ? last + 1 : right_bound;

                    auto j = i;
                    fn(j, right_bound, node->peek_value());
                    // Increase progress atomically
                    auto prev = node->inc_progress(blk_size, hetcompute::mem_order_relaxed);

                    // If the previous value is not what we expect, that
                    // means that someone has stolen the node from us.
                    if (i != prev)
                    {
                        HETCOMPUTE_INTERNAL_ASSERT(node->is_stolen(blk_size, hetcompute::mem_order_relaxed),
                                                 "Invalid node state %zu",
                                                 node->get_progress(hetcompute::mem_order_relaxed));
                        return false;
                    }

                    // inc_progress calls fetch_add, which
                    // returns the previous value of progress,
                    // so we have to increase i accordingly
                    i = prev + blk_size;

                } while (i <= last);

                node->set_completed();
                return true;
            }
        }; // struct worker_wrapper


        // Specialize for pfor_each
        // The specialization has to take care of processing stride
        template <typename Fn>
        struct worker_wrapper<void, Fn, Policy::MAP>
        {
            static bool work_on(typename ws_node<void>::node_type* node, Fn&& fn)
            {
                HETCOMPUTE_INTERNAL_ASSERT(node != nullptr, "Unexpectedly work on null pointer");
                HETCOMPUTE_INTERNAL_ASSERT(node->is_unclaimed(hetcompute::mem_order_relaxed) == false,
                                         "Invalid node state %zu",
                                         node->get_progress(hetcompute::mem_order_relaxed));

                typename ws_node<void>::size_type first = node->get_first();
                typename ws_node<void>::size_type last  = node->get_last();

                if (first > last)
                {
                    node->set_completed();
                    return true;
                }

                auto blk_size = node->get_tree()->get_blk_size();
                auto stride   = node->get_tree()->get_stride();

                auto                              i = first;
                typename ws_node<void>::size_type right_bound;

                // if it's root, or the first left child in pre-built tree, we must
                // finish the range [first, first + _blk_size)
                // before checking for stealing
                //
                if (first == node->get_tree()->range_start())
                {
                    right_bound = i + blk_size - 1;
                    right_bound = right_bound > last ? last : right_bound;
                    while (i <= right_bound)
                    {
                        fn(i);
                        i += stride;
                    }
                    if (right_bound == last)
                    {
                        return true;
                    }
                }

                // work on assigned iterations and check for stealing after
                // a batch is done.
                do
                {
                    // process blk_size iterations in a batch
                    right_bound = i + blk_size - 1;
                    right_bound = right_bound > last ? last : right_bound;

                    auto j = i;
                    while (j <= right_bound)
                    {
                        fn(j);
                        j += stride;
                    }
                    // Increase progress atomically
                    auto prev = node->inc_progress(blk_size, hetcompute::mem_order_relaxed);

                    // If the previous value is not what we expect, that
                    // means that someone has stolen the node from us.
                    if (i != prev)
                    {
                        HETCOMPUTE_INTERNAL_ASSERT(node->is_stolen(blk_size, hetcompute::mem_order_relaxed),
                                                 "Invalid node state %zu",
                                                 node->get_progress(hetcompute::mem_order_relaxed));
                        return false;
                    }

                    // inc_progress calls fetch_add, which
                    // returns the previous value of progress,
                    // so we have to increase i accordingly
                    i = prev + blk_size;

                } while (i <= last);

                node->set_completed();
                return true;
            }
        };  // struct worker_wrapper

        // Specialize for pscan
        // Note that we should skip processing the first element of each local range
        template <typename T, typename Fn>
        struct worker_wrapper<T, Fn, Policy::SCAN>
        {
            static bool work_on(typename ws_node<T>::node_type* node, Fn&& fn)
            {
                HETCOMPUTE_INTERNAL_ASSERT(node != nullptr, "Unexpectedly work on null pointer");
                HETCOMPUTE_INTERNAL_ASSERT(node->is_unclaimed(hetcompute::mem_order_relaxed) == false,
                                         "Invalid node state %zu",
                                         node->get_progress(hetcompute::mem_order_relaxed));

                typename ws_node<T>::size_type first = node->get_first();
                typename ws_node<T>::size_type last  = node->get_last();

                if (first > last)
                {
                    node->set_completed();
                    return true;
                }

                // index i records processing progress
                // invariant: the algorithm will process the batch [i, right_bound)
                //            atomically. For the first batch, the first element
                //            is ignored.

                auto                              blk_size = node->get_tree()->get_blk_size();
                typename ws_node<void>::size_type rb;

                auto i           = first + 1;
                auto lb          = first;
                auto range_start = node->get_tree()->range_start();

                // work on assigned iterations and check for stealing after
                // a batch is done.
                while (i <= last)
                {
                    rb = lb + blk_size;
                    rb = rb >= last ? last + 1 : rb;

                    for (auto j = i; j < rb; ++j)
                    {
                        fn(j - 1, j);
                    }

                    if (lb != range_start)
                    {
                        // Increase progress atomically
                        auto prev = node->inc_progress(blk_size, hetcompute::mem_order_relaxed);

                        // If the previous value is not what we expect, it
                        // means that someone has stolen the node from us.
                        if (lb != prev)
                        {
                            HETCOMPUTE_INTERNAL_ASSERT(node->is_stolen(blk_size, hetcompute::mem_order_relaxed),
                                                     "Invalid node state %zu, %zu-%zu, lb: %zu, prev: %zu",
                                                     node->get_progress(hetcompute::mem_order_relaxed),
                                                     first,
                                                     last,
                                                     lb,
                                                     prev);
                            return false;
                        }
                    }

                    // inc_progress calls fetch_add, which
                    // returns the previous value of progress,
                    // so we have to increase i accordingly
                    i  = rb;
                    lb = i;
                }

                node->set_completed();
                return true;
            }
        };  // struct worker_wrapper

    };  // namespace internal
};  // namespace hetcompute
