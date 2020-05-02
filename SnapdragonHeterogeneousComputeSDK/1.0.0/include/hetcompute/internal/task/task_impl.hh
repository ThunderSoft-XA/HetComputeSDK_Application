#pragma once

// This file is needed to refactor out finish_after_impl() from internal/task/task.hh.

#include <hetcompute/internal/legacy/task.hh>

namespace hetcompute
{
    namespace internal
    {
        template <typename StubTaskFn>
        void task::finish_after_impl(task* pred, task* succ, StubTaskFn&& fn)
        {
            finish_after_state_ptr& tfa = succ->get_finish_after_state();
            if (!succ->should_finish_after(tfa))
            {
                // Create stub task and add pred as predecessor
                auto c = create_stub_task(fn, pred);
                succ->set_finish_after_state(tfa, c);
            }
            else
            {
                auto fa = tfa.get();
                HETCOMPUTE_INTERNAL_ASSERT(fa->_finish_after_stub_task != nullptr, "must have already invoked set_finish_after_state");
                task::add_task_dependence(pred, c_ptr(fa->_finish_after_stub_task));
            }
        }

    }; // namespace internal
};     // namespace hetcompute
