#pragma once

#include <hetcompute/memregion.hh>
#include <hetcompute/internal/buffer/memregion-internal.hh>

namespace hetcompute
{
    namespace internal
    {
        class memregion_base_accessor
        {
        public:
            static internal::internal_memregion* get_internal_mr(memregion const& mr)
            {
                return mr._int_mr;
            }
        };

    }; // namespace internal
};     // namespace hetcompute
