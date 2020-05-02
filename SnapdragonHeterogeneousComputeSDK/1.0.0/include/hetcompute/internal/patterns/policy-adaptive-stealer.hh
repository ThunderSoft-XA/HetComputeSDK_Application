#pragma once

#include <hetcompute/internal/patterns/policy-adaptive-worker.hh>

namespace hetcompute
{
    namespace internal
    {
        // Primary template for stealer task body
        template <typename T, typename Strategy, Policy P>
        struct stealer_wrapper;

        // Specialize for preduce
        template <typename T, typename Strategy>
        struct stealer_wrapper<T, Strategy, Policy::REDUCE>
        {
            static void stealer_task_body(hetcompute_shared_ptr<Strategy> stg_ptr, size_t task_id)
            {
                HETCOMPUTE_INTERNAL_ASSERT(current_task() != nullptr, "current task pointer unexpectedly invalid");

                auto  root      = stg_ptr->get_root();
                auto  curr_node = root;
                auto& fn        = stg_ptr->get_fn();
                auto  blk_size  = stg_ptr->get_blk_size();

                // tree is pre-built.
                if (stg_ptr->is_prealloc() && task_id < stg_ptr->get_prealloc_leaf())
                {
                    curr_node = stg_ptr->find_work_prealloc(task_id);
                }
                else
                {
                    // when task_id is 0, it always claims root
                    if (task_id != 0)
                    {
                        curr_node = stg_ptr->find_work_intree(root, blk_size, stg_ptr->get_identity());
                    }
                }

                if (curr_node == nullptr)
                {
                    return;
                }

                do
                {
#ifdef HETCOMPUTE_ADAPTIVE_PFOR_DEBUG
                    // save task id for graphviz output
                    curr_node->set_worker_id(task_id);
#endif // HETCOMPUTE_ADAPTIVE_PFOR_DEBUG
                    bool work_complete = worker_wrapper<T, decltype(fn), Policy::REDUCE>::work_on(curr_node, fn);
                    if (work_complete)
                    {
                        curr_node = stg_ptr->find_work_intree(root, blk_size, stg_ptr->get_identity());
                    }
                    // if work not complete, meaning stealing happened during
                    // work on, we search for work from current node instead of root
                    else
                    {
                        curr_node = stg_ptr->find_work_intree(curr_node, blk_size, stg_ptr->get_identity());
                    }
                } while (curr_node != nullptr);
            }
        };

        // Specialize for pscan
        // The task body for pscan includes three stages
        //
        // --Stage One:   Each stealer tries to own or steal a local range and performs
        //                the scan operation on the local range (first, last]. Similar
        //                to other patterns.
        // --Stage Two:   The master task (the one executed on the thread calling
        //                hetcompute::pscan) will traverse the work stealing
        //                tree in a pre-order fashion. When traversing
        //                from node A to B, it will add A's last element (
        //                local sum) to B's last element, and save A's last element
        //                in B for stage three. Other tasks wait for A's completion
        //                before proceeding to Stage Three.
        // --Stage Three: In parallel, each tasks traverses the tree and locates the node
        //                it worked on at Stage One (by saving the worker_id at
        //                each node). The saved footprints might (1) eliminate working
        //                contentions; and (2) achieve better locality. After locating
        //                the node, each worker will perform the scan operation on
        //                the rest of the range (first, last - 1] with the local
        //                sum (passed by pre-order parent) saved in Stage Two.
        template <typename T, typename Strategy>
        struct stealer_wrapper<T, Strategy, Policy::SCAN>
        {
            // Local sum update performed by the master task at Stage Two
            // This routine is executed in serial
            // A recursive version is employed deliberately for better readability
            template <typename InputIterator, typename BinaryFn>
            static void update_local_sum(typename ws_node<T>::node_type* n, const InputIterator base, BinaryFn&& fn)
            {
                if (n == nullptr)
                {
                    return;
                }

                auto first = n->get_first();
                auto last  = n->get_last();
                T    val;

                // We don't need to update local scan on the leftmost branch
                if (first != n->get_tree()->range_start())
                {
                    val = *(base + first - 1);

                    n->set_value(val);
                    if (n->get_left() != 0)
                    {
                        auto offset = n->get_split_idx();
                        if (offset >= first)
                        {
                            *(base + offset) = fn(val, *(base + offset));
                        }
                    }
                    else
                    {
                        *(base + last) = fn(val, *(base + last));
                    }
                }

                // update local sum recursively
                update_local_sum<InputIterator, BinaryFn>(n->get_left(), base, fn);
                update_local_sum<InputIterator, BinaryFn>(n->get_right(), base, fn);
            }

            // Local scan performed by all tasks at Stage Three
            // A recursive version is employed deliverately for better readability
            template <typename InputIterator, typename BinaryFn>
            static void scan_node(typename ws_node<T>::node_type* n, size_t tid, const InputIterator base, BinaryFn&& fn)
            {
                // skip updating the leftmost branch
                if (n == nullptr)
                {
                    return;
                }

                auto first = n->get_first();
                auto last  = n->get_last();

                // Only do this for non left-most branch
                if (first != 0)
                {
                    // find if this node was previously assigned to the working task
                    // at Stage One. Note that we use the leftmost 4 bits in
                    // left_traversal to record the footprints
                    if (n->get_task_id() == static_cast<uint16_t>(tid))
                    {
                        // Leaf Node, update all the elements except for the last one
                        // (updated in second stage)
                        if (n->get_left() == nullptr)
                        {
                            for (auto idx = last - 1; idx >= first; --idx)
                            {
                                *(base + idx) = fn(n->peek_value(), *(base + idx));
                            }
                        }

                        // Non-leaft node, update range (first, split index - 1]
                        else
                        {
                            auto offset = n->get_split_idx();
                            if ((offset - 1) >= first)
                            {
                                for (auto idx = offset - 1; idx >= first; --idx)
                                {
                                    *(base + idx) = fn(n->peek_value(), *(base + idx));
                                }
                            }
                        }
                    }
                }

                // scan node recursively
                scan_node<InputIterator, BinaryFn>(n->get_left(), tid, base, fn);
                scan_node<InputIterator, BinaryFn>(n->get_right(), tid, base, fn);
            }

            static void stealer_task_body(hetcompute_shared_ptr<Strategy> stg_ptr, size_t task_id)
            {
                HETCOMPUTE_INTERNAL_ASSERT(current_task() != nullptr, "current task pointer unexpectedly invalid");

                auto  root      = stg_ptr->get_root();
                auto  curr_node = root;
                auto& fn        = stg_ptr->get_fn();
                auto& bf        = stg_ptr->get_binary_fn();
                auto  blk_size  = stg_ptr->get_blk_size();
                auto  base      = stg_ptr->get_base();

                // tree is pre-built.
                if (stg_ptr->is_prealloc() && task_id < stg_ptr->get_prealloc_leaf())
                {
                    curr_node = stg_ptr->find_work_prealloc(task_id);
                }
                else
                {
                    // when task_id is 0, it always claims root
                    if (task_id != 0)
                    {
                        curr_node = stg_ptr->find_work_intree(task_id, root, blk_size);
                    }
                }

                if (curr_node == nullptr)
                {
                    // nothing to work on, the task is finished
                    stg_ptr->inc_task_counter();
                    return;
                }

                do
                {
#ifdef HETCOMPUTE_ADAPTIVE_PFOR_DEBUG
                    // save task id for graphviz output
                    curr_node->set_worker_id(task_id);
#endif // HETCOMPUTE_ADAPTIVE_PFOR_DEBUG
                    bool work_complete = worker_wrapper<T, decltype(fn), Policy::SCAN>::work_on(curr_node, fn);
                    if (work_complete)
                    {
                        curr_node = stg_ptr->find_work_intree(task_id, root, blk_size);
                    }
                    // if work not complete, meaning stealing happened during
                    // work on, we search for work from current node instead of root
                    else
                    {
                        curr_node = stg_ptr->find_work_intree(task_id, curr_node, blk_size);
                    }
                } while (curr_node != nullptr);

                // notify one task is finished
                stg_ptr->inc_task_counter();

                if (task_id == 0)
                {
                    // main task spin waits here until all helper tasks finish stage one
                    while (stg_ptr->get_task_counter() != stg_ptr->get_max_tasks())
                    {
                    }

                    // Stage 2, the main thread task is in charge of prepare local sums
                    // for the whole tree to enable data-parallel update in the 3rd stage.
                    update_local_sum(root, base, bf);
                    stg_ptr->set_local_sum_updated(hetcompute::mem_order_release);
                }

                // spin wait here until main task finishes stage 2
                while (!stg_ptr->is_local_sum_updated(hetcompute::mem_order_acquire))
                {
                }

                // Stage 3, all tasks start working on the previously assigned nodes
                // in parallel
                scan_node(root, task_id, base, bf);
            }
        };

        // Specialize for pfor_each
        template <typename Strategy>
        struct stealer_wrapper<void, Strategy, Policy::MAP>
        {
            static void stealer_task_body(hetcompute_shared_ptr<Strategy> stg_ptr, size_t task_id)
            {
                HETCOMPUTE_INTERNAL_ASSERT(current_task() != nullptr, "current task pointer unexpectedly invalid");

                auto  root      = stg_ptr->get_root();
                auto  curr_node = root;
                auto& fn        = stg_ptr->get_fn();
                auto  blk_size  = stg_ptr->get_blk_size();

                // tree is pre-built.
                if (stg_ptr->is_prealloc() && task_id < stg_ptr->get_prealloc_leaf())
                {
                    curr_node = stg_ptr->find_work_prealloc(task_id);
                }
                else
                {
                    // when task_id is 0, it always claims root
                    if (task_id != 0)
                    {
                        curr_node = stg_ptr->find_work_intree(root, blk_size);
                    }
                }

                if (curr_node == nullptr)
                    return;

                do
                {
#ifdef HETCOMPUTE_ADAPTIVE_PFOR_DEBUG
                    // save task id for graphviz output
                    curr_node->set_worker_id(task_id);
#endif // HETCOMPUTE_ADAPTIVE_PFOR_DEBUG
                    bool work_complete = worker_wrapper<void, decltype(fn), Policy::MAP>::work_on(curr_node, fn);

                    if (work_complete)
                    {
                        curr_node = stg_ptr->find_work_intree(root, blk_size);
                    }
                    // if work not complete, meaning stealing happened during
                    // work on, we search for work from current node instead of root
                    else
                    {
                        curr_node = stg_ptr->find_work_intree(curr_node, blk_size);
                    }
                } while (curr_node != nullptr);
            }
        };

    };  // namespace internal
};  // namespace hetcompute
