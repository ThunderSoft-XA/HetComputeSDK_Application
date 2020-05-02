#pragma once

/// Implementations of Compare-And-Swap, Atomic Load and Atomic Store

#include <atomic>

#include <hetcompute/internal/util/debug.hh>

#if !defined(_MSC_VER)
    #define HETCOMPUTE_SIZEOF_MWORD __SIZEOF_SIZE_T__
#else
    #if _M_X64
        #define HETCOMPUTE_SIZEOF_MWORD 8
    #else
        #define HETCOMPUTE_SIZEOF_MWORD 4
    #endif /// _M_X64
#endif /// !defined(_MSC_VER)

#define HETCOMPUTE_SIZEOF_DMWORD (HETCOMPUTE_SIZEOF_MWORD * 2)

namespace hetcompute
{
    namespace internal
    {
        template <size_t N, typename T>
        class atomicops
        {
        public:
            static inline T fetch_add(T* dest, const T& incr, std::memory_order m = std::memory_order_seq_cst)
            {
                HETCOMPUTE_INTERNAL_ASSERT(m == std::memory_order_seq_cst || m == std::memory_order_acq_rel || m == std::memory_order_relaxed,
                                         "Invalid memory order %u",
                                         m);
                return std::atomic_fetch_add_explicit(reinterpret_cast<std::atomic<T>*>(dest), incr, m);
            }
            static inline T fetch_sub(T* dest, const T& incr, std::memory_order m = std::memory_order_seq_cst)
            {
                HETCOMPUTE_INTERNAL_ASSERT(m == std::memory_order_seq_cst || m == std::memory_order_acq_rel || m == std::memory_order_relaxed,
                                         "Invalid memory order %u",
                                         m);
                return std::atomic_fetch_sub_explicit(reinterpret_cast<std::atomic<T>*>(dest), incr, m);
            }
            static inline T fetch_and(T* dest, const T& incr, std::memory_order m = std::memory_order_seq_cst)
            {
                HETCOMPUTE_INTERNAL_ASSERT(m == std::memory_order_seq_cst || m == std::memory_order_acq_rel || m == std::memory_order_relaxed,
                                         "Invalid memory order %u",
                                         m);
                return std::atomic_fetch_and_explicit(reinterpret_cast<std::atomic<T>*>(dest), incr, m);
            }
            static inline T fetch_or(T* dest, const T& incr, std::memory_order m = std::memory_order_seq_cst)
            {
                HETCOMPUTE_INTERNAL_ASSERT(m == std::memory_order_seq_cst || m == std::memory_order_acq_rel || m == std::memory_order_relaxed,
                                         "Invalid memory order %u",
                                         m);
                return std::atomic_fetch_or_explicit(reinterpret_cast<std::atomic<T>*>(dest), incr, m);
            }
            static inline T fetch_xor(T* dest, const T& incr, std::memory_order m = std::memory_order_seq_cst)
            {
                HETCOMPUTE_INTERNAL_ASSERT(m == std::memory_order_seq_cst || m == std::memory_order_acq_rel || m == std::memory_order_relaxed,
                                         "Invalid memory order %u",
                                         m);
                return std::atomic_fetch_xor_explicit(reinterpret_cast<std::atomic<T>*>(dest), incr, m);
            }
        };

        static inline void hetcompute_atomic_thread_fence(std::memory_order m)
        {
            return std::atomic_thread_fence(m);
        }

    }; // namespace internal

}; // namespace hetcompute

#if defined(i386) || defined(__i386) || defined(__i386__) || defined(__x86_64__)
#define HETCOMPUTE_HAS_ATOMIC_DMWORD 1

namespace hetcompute
{
    namespace internal
    {
#ifdef __x86_64__
    #if defined(__clang__) && __SIZEOF_INT128__ < 16
        typedef __uint128_t hetcompute_dmword_t;
    #else
        typedef unsigned __int128 hetcompute_dmword_t;
    #endif
#else
        typedef uint64_t hetcompute_dmword_t;
#endif
    }; // namespace internal
};     // namespace hetcompute

#elif defined(__ARM_ARCH_7A__)
namespace hetcompute
{
    namespace internal
    {
#ifdef __LP64__
        // we don't have code for this yet
        #define HETCOMPUTE_HAS_ATOMIC_DMWORD 0
        typedef unsigned __int128 hetcompute_dmword_t;
#else
        #define HETCOMPUTE_HAS_ATOMIC_DMWORD 1
        typedef uint64_t hetcompute_dmword_t;
#endif
    }; // namespace internal
};     // namespace hetcompute

#elif defined(__aarch64__) // ARMv8
namespace hetcompute
{
    namespace internal
    {
#define HETCOMPUTE_HAS_ATOMIC_DMWORD 1
#if HETCOMPUTE_SIZEOF_MWORD == 8
        typedef unsigned __int128 hetcompute_dmword_t;
#else
        typedef uint64_t hetcompute_dmword_t;
#endif
    }; // namespace internal
};     // namespace hetcompute

#elif defined(__GNUC__)
#if __LP64__
    #define HETCOMPUTE_HAS_ATOMIC_DMWORD __GLIBCXX_USE_INT128
#else
    #define HETCOMPUTE_HAS_ATOMIC_DMWORD 1
#endif

namespace hetcompute
{
    namespace internal
    {
#if __GLIBCXX_USE_INT128
        typedef unsigned __int128 hetcompute_dmword_t;
#else
        typedef uint64_t hetcompute_dmword_t;
#endif
    }; // namespace internal
};     // namespace hetcompute

#elif defined(_M_IX86) || defined(_M_X64)
#define HETCOMPUTE_HAS_ATOMIC_DMWORD 1
namespace hetcompute
{
    namespace internal
    {
        // This is ULONGLONG even for 16 byte types since users will want to
        // split this up into bitfields.  The use is responsible to make sure
        // the overall type is aligned
        typedef ULONGLONG hetcompute_dmword_t;
    }; // namespace internal
};     // namespace hetcompute
#else
    #error No implementation for atomic operations available for this architecture.
#endif
