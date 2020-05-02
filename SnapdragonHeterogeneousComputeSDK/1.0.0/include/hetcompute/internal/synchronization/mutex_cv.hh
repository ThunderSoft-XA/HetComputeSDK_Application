#pragma once

#include <utility>
#include <mutex>
#include <condition_variable>

namespace hetcompute
{
    namespace internal
    {
        typedef std::pair<std::mutex, std::condition_variable> paired_mutex_cv;
    }; // namespace internal
};     // namespace hetcompute
