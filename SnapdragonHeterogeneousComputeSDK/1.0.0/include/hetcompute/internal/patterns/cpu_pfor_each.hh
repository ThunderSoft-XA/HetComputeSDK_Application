#pragma once

#include <hetcompute/range.hh>
#include <hetcompute/internal/patterns/common.hh>

namespace hetcompute
{
    namespace internal
    {
        template <size_t Dims, typename UnaryFn>
        struct pfor_each_range;

        template <typename UnaryFn>
        struct pfor_each_range<1, UnaryFn>
        {
            static void pfor_each_range_impl(group* g, const hetcompute::range<1>& r, UnaryFn fn, const hetcompute::pattern::tuner& tuner)
            {
                pfor_each_internal(g,
                                   r.begin(0),
                                   r.end(0),
                                   [&r, fn](size_t i) {
                                       index<1> idx(i);
                                       fn(idx);
                                   },
                                   r.stride(0),
                                   tuner);
            }
        };

        template <typename UnaryFn>
        struct pfor_each_range<2, UnaryFn>
        {
            static void pfor_each_range_impl(group* g, const hetcompute::range<2>& r, UnaryFn fn, const hetcompute::pattern::tuner& tuner)
            {
                pfor_each_internal(g,
                                   r.begin(0),
                                   r.end(0),
                                   [&r, fn](size_t i) {
                                       for (size_t j = r.begin(1); j < r.end(1); j += r.stride(1))
                                       {
                                           index<2> idx(i, j);
                                           fn(idx);
                                       }
                                   },
                                   r.stride(0),
                                   tuner);
            }
        };

        template <typename UnaryFn>
        struct pfor_each_range<3, UnaryFn>
        {
            static void pfor_each_range_impl(group* g, const hetcompute::range<3>& r, UnaryFn fn, const hetcompute::pattern::tuner& tuner)
            {
                pfor_each_internal(g,
                                   r.begin(0),
                                   r.end(0),
                                   [&r, fn](size_t i) {
                                       for (size_t j = r.begin(1); j < r.end(1); j += r.stride(1))
                                       {
                                           for (size_t k = r.begin(2); k < r.end(2); k += r.stride(2))
                                           {
                                               index<3> idx(i, j, k);
                                               fn(idx);
                                           }
                                       }
                                   },
                                   r.stride(0),
                                   tuner);
            }
        };

        template <class InputIterator, typename UnaryFn>
        void pfor_each_static(group*                          group,
                              InputIterator                   first,
                              InputIterator                   last,
                              UnaryFn&&                       fn,
                              const size_t                    stride,
                              hetcompute::pattern::tuner const& tuner)
        {
            if (first >= last)
            {
                return;
            }

            auto g_internal = create_group();
            auto g          = g_internal;

            if (group != nullptr)
            {
                g = pattern::group_ptr_shim::shared_to_group_ptr_cast(group_intersect::intersect_impl(c_ptr(g), group));
            }

            typedef typename internal::distance_helper<InputIterator>::_result_type working_type;

            // Create GRANULARITY_MULTIPLIER * num_exec_ctx chunks for parallel execution
            // The number of workers equal to num_exec_ctx
            // TODO: Add tuner option.
            const working_type GRANULARITY_MULTIPLIER = 4;

            // The following condition check has no problem, however, if one
            // defines first = X, and last = X + C in the code, the compiler
            // will try to optimize it away by assuming the condition is always
            // false. Unfortunately, this is treated as an error when -Wstrict-overflow
            // is enabled. Hence, we have the HETCOMPUTE_GCC ignore guard in unittests.

            const working_type dist       = internal::distance(first, last);
            auto               num_chunks = GRANULARITY_MULTIPLIER * static_cast<working_type>(tuner.get_doc());
            working_type       blksz      = std::max(working_type(1), dist / num_chunks);
            num_chunks                    = dist % blksz == 0 ? dist / blksz : dist / blksz + 1;

            HETCOMPUTE_INTERNAL_ASSERT(blksz > 0, "block size should be greater than zero");

            std::atomic<working_type> work_id(0);

// Visual Studio screws up with ordinary lambda so we need to wrap it.
#ifdef _MSC_VER
            std::function<void(InputIterator)> poj        = fn;
            auto                               chunk_body = [first, last, blksz, num_chunks, stride, &work_id, poj]
#else
            auto chunk_body = [first, last, blksz, num_chunks, stride, &work_id, fn]
#endif // _MSC_VER
            {
                while (1)
                {
                    auto prev = work_id.fetch_add(1, hetcompute::mem_order_relaxed);
                    if (prev < num_chunks)
                    {
                        InputIterator lb      = first + prev * blksz;
                        working_type  safeofs = std::min(working_type(internal::distance(lb, last)), blksz);
                        InputIterator rb      = lb + safeofs;
                        InputIterator it      = (lb - first) % stride == 0 ? lb : first + (prev * blksz / stride + 1) * stride;
                        while (it < rb)
                        {
#ifdef _MSC_VER
                            poj(it);
#else
                            fn(it);
#endif // _MSC_VER
                            it += working_type(stride);
                        }
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
        }

        // using adaptive stealer to dynamically balancing workload
        // over the entire iteration range.
        // Works for iterator of type size_t
        template <typename Body>
        void pfor_each_dynamic(group* group, size_t first, size_t last, Body&& body, const size_t stride, const hetcompute::pattern::tuner& tuner)
        {
            if (first >= last)
            {
                return;
            }

            // Wrap the body so that we get a nice interface to it
            typedef ::hetcompute::internal::legacy::body_wrapper<Body> pfor_body_wrapper;
            pfor_body_wrapper                                        wrapper(std::forward<Body>(body));

            // ptransform has a binary variant
            HETCOMPUTE_INTERNAL_ASSERT(pfor_body_wrapper::get_arity() == 1 || pfor_body_wrapper::get_arity() == 2,
                                     "invalid number of arguments in body");

            // Let's get the body attributes, and add pfor and inlined to them
            auto body_attrs = wrapper.get_attrs();
            body_attrs      = legacy::add_attr(body_attrs, attr::pfor);
            body_attrs      = legacy::add_attr(body_attrs, attr::inlined);

            // We get a reference to the wrapper
            auto&                                                      fn = wrapper.get_fn();
            typedef typename std::remove_reference<decltype(fn)>::type UnaryFn;

            // blk_size defines the minimal granularity of iterations to advance.
            size_t blk_size = tuner.get_chunk_size();

            // We first check whether this is a nested pfor or not.
            // It is, we treat it differently depending on whether
            // the user passed a group pointer to it or not:
            //   - if she didn't, then we execute the pfor as a regular for.
            //   - if she did, we create a task that executes it as a regular
            //     for, but we execute it inline.
            //
            auto t = current_task();
            if (t && t->is_pfor())
            {
                if (group == nullptr)
                {
                    for (size_t i = first; i < last; i += stride)
                    {
                        fn(i);
                    }
                    return;
                }

                HETCOMPUTE_MSC_IGNORE_BEGIN(4702)
                auto inlined = ::hetcompute::internal::legacy::create_task(legacy::with_attrs(body_attrs, [first, last, &fn, stride] {
                    for (size_t i = first; i < last; i += stride)
                    {
                        fn(i);
                    }
                }));
                HETCOMPUTE_MSC_IGNORE_END(4702)
                c_ptr(inlined)->execute_inline_task(group);
                return;
            };

            // Create the group that will take care of the parallel for
            auto g_internal = create_group();
            auto g          = g_internal;
            if (group != nullptr)
            {
                g = pattern::group_ptr_shim::shared_to_group_ptr_cast(group_intersect::intersect_impl(c_ptr(g), group));
            }

            typedef adaptive_steal_strategy<void, size_t, UnaryFn, UnaryFn, Policy::MAP> strategy_type;

            // Block size needs to be aligned with the stride
            // Such an alignment will guarantee the correctness of incrementing index
            // and maintain the performance implication of stealing frequency
            size_t num_stride = blk_size / stride;
            blk_size          = blk_size % stride == 0 ? num_stride * stride : (num_stride + 1) * stride;

            // adaptive_pfor is in fact the storage with a work steal tree,
            // the group pointer, and the lambda.
            hetcompute_shared_ptr<strategy_type> strategy_ptr(
                new strategy_type(g, first, last - 1, std::forward<UnaryFn>(fn), body_attrs, blk_size, stride, tuner));

            size_t max_tasks = strategy_ptr->get_max_tasks();
            if (max_tasks > 1 && max_tasks < (last - first))
            {
                strategy_ptr->static_split(max_tasks);
            }

            // We are the ones that found the parallel for, so we start
            // stealing from the pfor tree inline. If the pfor is short
            // enough, we might not even spawn many new tasks.
            execute_master_task<void, strategy_type, Policy::MAP>(strategy_ptr);

            spin_wait_for(g);

#ifdef HETCOMPUTE_ADAPTIVE_PFOR_DEBUG
            print_tree<void>(strategy_ptr->get_tree());
#endif // HETCOMPUTE_ADAPTIVE_PFOR_DEBUG
        }
    };  // namespace internal
};  // namespace hetcompute
