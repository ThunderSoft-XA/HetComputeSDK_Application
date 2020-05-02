#pragma once

#include <hetcompute/internal/util/hetcomputeptrs.hh>

namespace hetcompute
{
    namespace internal
    {
        // forward declaration
        class group;

        // A std::shared_ptr-like managed pointer to a group object. We
        // need this because the pointers to the parent groups must
        // be shared pointers.
        using group_shared_ptr = hetcompute_shared_ptr<group>;
    }; // namespace hetcompute::internal
}; // namespace hetcompute
