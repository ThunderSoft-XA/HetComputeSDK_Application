#pragma once

// Defines helper functions for parallel programming patterns

#include <hetcompute/runtime.hh>

#include <hetcompute/internal/legacy/group.hh>
#include <hetcompute/internal/legacy/task.hh>
#include <hetcompute/internal/patterns/policy-adaptive-stealer.hh>
#include <hetcompute/internal/util/distance.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace pattern
        {
            class group_ptr_shim
            {
            public:
                static ::hetcompute::group_ptr shared_to_group_ptr_cast(::hetcompute::internal::group_shared_ptr&& g)
                {
                    HETCOMPUTE_API_ASSERT(g != nullptr, "Unexpected null ptr!");
                    return ::hetcompute::group_ptr(std::move(g));
                }

                static ::hetcompute::internal::group_shared_ptr group_to_shared_ptr_cast(::hetcompute::group_ptr g)
                {
                    HETCOMPUTE_API_ASSERT(g != nullptr, "Unexpected null ptr!");
                    return g.get_shared_ptr();
                }
            };  // class group_ptr_shim
        }; // namespace pattern

        template <class T>
        T static_chunk_size(T count, const size_t nctx)
        {
            // Inexpensive way to create fine-grained tasks; the real solution is
            // outlined below (adaptable or user-configurable chunking)
            T const GRANULARITY_MULTIPLIER = 4;
            return std::max(T(1), count / (GRANULARITY_MULTIPLIER * static_cast<T>(nctx)));
        }

        // We probably want to get rid of this func once cleanup is finalized.
        template <typename NullaryFn>
        void launch_or_exec(bool not_last, internal::group_shared_ptr& g, NullaryFn&& fn)
        {
            if (not_last)
            {
                auto attrs = legacy::create_task_attrs(attr::pfor);
                legacy::launch(g, legacy::with_attrs(attrs, fn));
            }
            else
            {
                auto attrs  = legacy::create_task_attrs(attr::pfor, attr::inlined);
                auto master = legacy::create_task(legacy::with_attrs(attrs, fn));
                c_ptr(master)->execute_inline_task(c_ptr(g));
            }
        }

        // A template argument is needed here to pass
        // to the stealer_task_body, which needs to work
        // on tree nodes with intermediate value of type T.
        // Policy is an enum class defined in policy_adaptive.hh
        template <typename T, typename AdaptiveStrategy, Policy P>
        void execute_master_task(hetcompute_shared_ptr<AdaptiveStrategy> strategy_ptr)
        {
            auto g            = strategy_ptr->get_group();
            auto master_attrs = legacy::create_task_attrs(attr::pfor, attr::inlined);
            auto helper_attrs = legacy::create_task_attrs(attr::pfor);

            auto master = legacy::create_task(legacy::with_attrs(master_attrs, [strategy_ptr]() mutable {
                stealer_wrapper<T, AdaptiveStrategy, P>::stealer_task_body(strategy_ptr, 0);
            }));

            size_t max_tasks = strategy_ptr->get_max_tasks();

            auto gs = pattern::group_ptr_shim::group_to_shared_ptr_cast(g);
            // launch helper tasks without notifying
            for (size_t i = 1; i < max_tasks; ++i)
            {
                legacy::launch(gs,
                               legacy::with_attrs(helper_attrs,
                                                  [strategy_ptr, i]() mutable {
                                                      stealer_wrapper<T, AdaptiveStrategy, P>::stealer_task_body(strategy_ptr, i);
                                                  }),
                               false);
            }

            hetcompute::internal::notify_all(max_tasks - 1);
            c_ptr(master)->execute_inline_task(c_ptr(g));
        }

    }; // namespace internal
};     // namespace hetcompute
