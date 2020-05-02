#pragma once

#include <mutex>

namespace hetcompute
{
    namespace internal
    {
        typedef std::mutex hetcompute_lock;

    }; // namespace hetcompute::internal
};     // namespace hetcompute
