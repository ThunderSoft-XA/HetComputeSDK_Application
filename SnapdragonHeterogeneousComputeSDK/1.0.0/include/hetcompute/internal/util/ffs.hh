#pragma once

#include <stdint.h>

namespace hetcompute
{
    namespace internal
    {
        inline int hetcompute_ffs(uint32_t x)
        {
            if (x == 0)
            {
                return 0;
            }

#ifdef _MSC_VER
            unsigned long index;
            if (_BitScanForward(&index, x))
            {
                return index + 1;
            }
            return 0;
#else
            return __builtin_ffs(x);
#endif
        }
    }; // namespace internal
};     // namespace hetcompute
