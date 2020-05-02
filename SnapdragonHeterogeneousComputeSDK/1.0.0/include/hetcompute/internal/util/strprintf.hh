#pragma once

#include <string>
#include <hetcompute/internal/util/macros.hh>

namespace hetcompute
{
    namespace internal
    {
        HETCOMPUTE_GCC_ATTRIBUTE((format(printf, 1, 2)))
        std::string strprintf(const char*, ...);
    }; // namespace internal
};     // namespace hetcompute
