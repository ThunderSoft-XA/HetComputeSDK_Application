#pragma once

#include <hetcompute/internal/util/sparse_bitmap.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace group_misc
        {
            typedef sparse_bitmap  group_signature;
            static group_signature empty_bitmap = sparse_bitmap(0);

        }; // namespace hetcompute::internal::group_misc
    };     // namespace hetcompute::internal
};         // namespace hetcompute
