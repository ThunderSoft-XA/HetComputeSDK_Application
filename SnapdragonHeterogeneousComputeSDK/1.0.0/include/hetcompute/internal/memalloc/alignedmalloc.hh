#pragma once

namespace hetcompute
{
    namespace internal
    {
        void* hetcompute_aligned_malloc(size_t alignment, size_t size);
        void hetcompute_aligned_free(void* ptr);
    }; // namespace internal
};     // namespace hetcompute
