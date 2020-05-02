#pragma once

#include <hetcompute/internal/util/hetcomputeptrs.hh>

namespace hetcompute
{
    namespace internal
    {
        // forward declaration
        class task;

        // A std::shared_ptr-like managed pointer to a task object. We
        // need this because we sometimes require a shared pointer
        // to a task and we can't use user facing pointers because of
        // circular dependencies.
        using task_shared_ptr = hetcompute_shared_ptr<task>;

    }; // namespace hetcompute::internal
};     // namespace hetcompute
