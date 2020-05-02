#pragma once

// Define a series of macros that are used for debugging and error reporting.
// We define our own macros with different names than what Android uses, so
// that we can easily intercept the calls and do anything we might need.
//    HETCOMPUTE_DLOG - debugging log
//    HETCOMPUTE_ILOG - information log
//    HETCOMPUTE_WLOG - warning log
//    HETCOMPUTE_FATAL - error log, with exit(1) terminating the application
//    HETCOMPUTE_ELOG - error log
//    HETCOMPUTE_LLOG - same as always log, but will not print thread prefix info
//   Do not include an ending \n char, since that is provided automatically

#include <hetcompute/internal/compat/compiler_compat.h>
#include <hetcompute/internal/util/internal_exceptions.hh>
#include <hetcompute/internal/util/memorder.hh>

#ifdef __ANDROID__
// On Android, since they have non-standard includes, we ignore a bunch of
// compiler warnings for gcc versions > 4.8, e.g. -Wshadow and -Wold-style-cast
HETCOMPUTE_GCC_IGNORE_BEGIN("-Wshadow")
HETCOMPUTE_GCC_IGNORE_BEGIN("-Wold-style-cast")
#endif // __ANDROID__

#include <atomic>
#include <cstring>

#ifdef __ANDROID__
#include <android/log.h>
#endif

#include <hetcompute/internal/compat/compat.h>
#include <hetcompute/internal/util/platform_tid.h>

#ifdef __ANDROID__
HETCOMPUTE_GCC_IGNORE_END("-Wold-style-cast")
HETCOMPUTE_GCC_IGNORE_END("-Wshadow")
#endif // __ANDROID__

#include <hetcompute/internal/util/macros.hh>
#include <hetcompute/internal/util/tls.hh>

#ifndef _MSC_VER
// HetCompute Debug breakpoint used with gdb. Example: gdb> break hetcompute_breakpoint
extern "C" void hetcompute_breakpoint() __attribute__((noinline));
#else
HETCOMPUTE_INLINE void
hetcompute_breakpoint()
{
}
#endif

namespace hetcompute
{
    namespace internal
    {
        using ::hetcompute_exit;
        using ::hetcompute_android_logprintf;

        // use regular ints to make the printing code as portable as possible.
        // use %8x for printing alignment, but if the system has a 64-bit int
        // then this will eventually exceed that, which is not a problem.
        extern std::atomic<unsigned int> hetcompute_log_counter;

    }; // namespace internal

}; // namespace hetcompute

// Note that fetch_add(1,hetcompute::mem_order_relaxed) is safe to use since
// there are no other variables involved, and the compiler still uses
// ldrex/strex on ARM for example. But we can avoid the extra barriers.
#ifdef __ANDROID__
// Macros for Android platform
#ifdef __aarch64__
#define HETCOMPUTE_FMT_TID "lx"
#else // __aarch64__
#define HETCOMPUTE_FMT_TID "x"
#endif // __aarch64__

#define HETCOMPUTE_DLOG(format, ...)                                                                                                           \
    ::hetcompute::internal::hetcompute_android_logprintf(ANDROID_LOG_DEBUG,                                                                      \
                                                     "\033[32mD %8x t%" HETCOMPUTE_FMT_TID " %s:%d " format "\033[0m",                         \
                                                     ::hetcompute::internal::hetcompute_log_counter.fetch_add(1, ::hetcompute::mem_order_relaxed), \
                                                     hetcompute_internal_get_platform_thread_id(),                                             \
                                                     __FILE__,                                                                               \
                                                     __LINE__,                                                                               \
                                                     ##__VA_ARGS__)

#define HETCOMPUTE_DLOGN(format, ...)                                                                                                          \
    ::hetcompute::internal::hetcompute_android_logprintf(ANDROID_LOG_DEBUG,                                                                      \
                                                     "\033[32mD %8x t%" HETCOMPUTE_FMT_TID " %s:%d " format "\033[0m",                         \
                                                     ::hetcompute::internal::hetcompute_log_counter.fetch_add(1, ::hetcompute::mem_order_relaxed), \
                                                     hetcompute_internal_get_platform_thread_id(),                                             \
                                                     __FILE__,                                                                               \
                                                     __LINE__,                                                                               \
                                                     ##__VA_ARGS__)

#define HETCOMPUTE_ILOG(format, ...)                                                                                                           \
    ::hetcompute::internal::hetcompute_android_logprintf(ANDROID_LOG_INFO,                                                                       \
                                                     "\033[33mI %8x t%" HETCOMPUTE_FMT_TID " %s:%d " format "\033[0m",                         \
                                                     ::hetcompute::internal::hetcompute_log_counter.fetch_add(1, ::hetcompute::mem_order_relaxed), \
                                                     hetcompute_internal_get_platform_thread_id(),                                             \
                                                     __FILE__,                                                                               \
                                                     __LINE__,                                                                               \
                                                     ##__VA_ARGS__)

#define HETCOMPUTE_WLOG(format, ...)                                                                                                           \
    ::hetcompute::internal::hetcompute_android_logprintf(ANDROID_LOG_WARN,                                                                       \
                                                     "\033[35mW %8x t%" HETCOMPUTE_FMT_TID " %s:%d " format "\033[0m",                         \
                                                     ::hetcompute::internal::hetcompute_log_counter.fetch_add(1, ::hetcompute::mem_order_relaxed), \
                                                     hetcompute_internal_get_platform_thread_id(),                                             \
                                                     __FILE__,                                                                               \
                                                     __LINE__,                                                                               \
                                                     ##__VA_ARGS__)

#define HETCOMPUTE_EXIT_FATAL(format, ...)                                                                                                   \
    ::hetcompute::internal::hetcompute_android_logprintf(ANDROID_LOG_ERROR,                                                                                 \
                                                         "\033[31mFATAL %8x t%" HETCOMPUTE_FMT_TID " %s:%d %s() " format,                                     \
                                                         ::hetcompute::internal::hetcompute_log_counter.fetch_add(1,::hetcompute::mem_order_relaxed),             \
                                                         hetcompute_internal_get_platform_thread_id(), __FILE__, __LINE__, __FUNCTION__ , ## __VA_ARGS__),    \
        ::hetcompute::internal::hetcompute_android_logprintf (ANDROID_LOG_ERROR,                                                                                \
                                                          "t%" HETCOMPUTE_FMT_TID " %s:%d **********",                                                        \
                                                          hetcompute_internal_get_platform_thread_id(), __FILE__, __LINE__),                                  \
        ::hetcompute::internal::hetcompute_android_logprintf (ANDROID_LOG_ERROR,                                                                                \
                                                          "t%" HETCOMPUTE_FMT_TID " %s:%d - Terminating with exit(1)",                                        \
                                                           hetcompute_internal_get_platform_thread_id(), __FILE__, __LINE__),                                 \
        ::hetcompute::internal::hetcompute_android_logprintf (ANDROID_LOG_ERROR,                                                                                \
                                                          "t%" HETCOMPUTE_FMT_TID " %s:%d **********\033[0m",                                                 \
                                                          hetcompute_internal_get_platform_thread_id(), __FILE__, __LINE__),                                  \
        hetcompute_exit(1)

#define HETCOMPUTE_ELOG(format, ...)                                                                                                           \
    ::hetcompute::internal::hetcompute_android_logprintf(ANDROID_LOG_ERROR,                                                                       \
                                                     "\033[36mA %8x t%" HETCOMPUTE_FMT_TID " %s:%d " format "\033[0m",                         \
                                                     ::hetcompute::internal::hetcompute_log_counter.fetch_add(1, ::hetcompute::mem_order_relaxed), \
                                                     hetcompute_internal_get_platform_thread_id(),                                             \
                                                     __FILE__,                                                                               \
                                                     __LINE__,                                                                               \
                                                     ##__VA_ARGS__)

#define HETCOMPUTE_LLOG(format, ...) ::hetcompute::internal::hetcompute_android_logprintf(ANDROID_LOG_INFO, format "\n", ##__VA_ARGS__)

#else

// Macros for Linux/OSX platforms
#if defined(PRIxPTR)
#define HETCOMPUTE_FMT_TID PRIxPTR
#elif defined(i386) || defined(__i386) || defined(__i386__) || defined(__ARM_ARCH_7A__)
#define HETCOMPUTE_FMT_TID "x"
#else
#define HETCOMPUTE_FMT_TID "lx"
#endif

#define HETCOMPUTE_LOG_FPRINTF(stream, fmt, ...) fprintf(stream, fmt, ##__VA_ARGS__)

#define HETCOMPUTE_DLOG(format, ...)                                                                                                         \
    HETCOMPUTE_LOG_FPRINTF(stderr,                                                                                                           \
                         "\033[32mD %8x t%" HETCOMPUTE_FMT_TID " %s:%d " format "\033[0m\n",                                                 \
                         hetcompute::internal::hetcompute_log_counter.fetch_add(1, hetcompute::mem_order_relaxed),                               \
                         hetcompute_internal_get_platform_thread_id(),                                                                       \
                         __FILE__,                                                                                                         \
                         __LINE__,                                                                                                         \
                         ##__VA_ARGS__)

#define HETCOMPUTE_DLOGN(format, ...)                                                                                                        \
    HETCOMPUTE_LOG_FPRINTF(stderr,                                                                                                           \
                         "\033[32mD %8x t%" HETCOMPUTE_FMT_TID " %s:%d " format "\033[0m",                                                   \
                         hetcompute::internal::hetcompute_log_counter.fetch_add(1, hetcompute::mem_order_relaxed),                               \
                         hetcompute_internal_get_platform_thread_id(),                                                                       \
                         __FILE__,                                                                                                         \
                         __LINE__,                                                                                                         \
                         ##__VA_ARGS__)

#define HETCOMPUTE_ILOG(format, ...)                                                                                                         \
    HETCOMPUTE_LOG_FPRINTF(stderr,                                                                                                           \
                         "\033[33mI %8x t%" HETCOMPUTE_FMT_TID " %s:%d " format "\033[0m\n",                                                 \
                         hetcompute::internal::hetcompute_log_counter.fetch_add(1, hetcompute::mem_order_relaxed),                               \
                         hetcompute_internal_get_platform_thread_id(),                                                                       \
                         __FILE__,                                                                                                         \
                         __LINE__,                                                                                                         \
                         ##__VA_ARGS__)

#define HETCOMPUTE_WLOG(format, ...)                                                                                                         \
    HETCOMPUTE_LOG_FPRINTF(stderr,                                                                                                           \
                         "\033[35mW %8x t%" HETCOMPUTE_FMT_TID " %s:%d " format "\033[0m\n",                                                 \
                         hetcompute::internal::hetcompute_log_counter.fetch_add(1, hetcompute::mem_order_relaxed),                               \
                         hetcompute_internal_get_platform_thread_id(),                                                                       \
                         __FILE__,                                                                                                         \
                         __LINE__,                                                                                                         \
                         ##__VA_ARGS__)

#define HETCOMPUTE_EXIT_FATAL(format, ...)                                                                                                   \
    HETCOMPUTE_LOG_FPRINTF(stderr,                                                                                                           \
                         "\033[31mFATAL %8x t%" HETCOMPUTE_FMT_TID " %s:%d %s() " format "\n",                                               \
                         hetcompute::internal::hetcompute_log_counter.fetch_add(1, hetcompute::mem_order_relaxed),                               \
                         hetcompute_internal_get_platform_thread_id(),                                                                       \
                         __FILE__,                                                                                                         \
                         __LINE__,                                                                                                         \
                         __FUNCTION__,                                                                                                     \
                         ##__VA_ARGS__)                                                                                                    \
    ,                                                                                                                                      \
        HETCOMPUTE_LOG_FPRINTF(stderr,                                                                                                       \
                             "t%" HETCOMPUTE_FMT_TID " %s:%d - Terminating with exit(1)\033[0m\n",                                           \
                             hetcompute_internal_get_platform_thread_id(),                                                                   \
                             __FILE__,                                                                                                     \
                             __LINE__),                                                                                                    \
        hetcompute_exit(1)

#define HETCOMPUTE_ELOG(format, ...)                                                                                                         \
    HETCOMPUTE_LOG_FPRINTF(stderr,                                                                                                           \
                         "\033[36mA %8x t%" HETCOMPUTE_FMT_TID " %s:%d " format "\033[0m\n",                                                 \
                         hetcompute::internal::hetcompute_log_counter.fetch_add(1, hetcompute::mem_order_relaxed),                               \
                         hetcompute_internal_get_platform_thread_id(),                                                                       \
                         __FILE__,                                                                                                         \
                         __LINE__,                                                                                                         \
                         ##__VA_ARGS__)

#define HETCOMPUTE_LLOG(format, ...) HETCOMPUTE_LOG_FPRINTF(stderr, format "\n", ##__VA_ARGS__)
#endif

// Various error handler macros. These do not throw exceptions and cannot be caught.
#define HETCOMPUTE_UNIMPLEMENTED(format, ...)                                                                                                \
    HETCOMPUTE_EXIT_FATAL("Unimplemented code in function '%s' at %s:%d " format, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#define HETCOMPUTE_UNREACHABLE(format, ...)                                                                                                  \
    HETCOMPUTE_EXIT_FATAL("Unreachable code triggered in function '%s' at %s:%d " format, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)

// HETCOMPUTE_FATAL (immediate exit, does not throw an exception, always enabled)
// This macro is called when we have a fatal error to report, which should
// never be ignored. HETCOMPUTE_FATAL does not work like an assert; you need to
// write the if statement yourself, so that it is obvious that this code
// is always checked. All the other asserts can be optionally disabled.
// HETCOMPUTE_FATAL should be used for out-of-memory, file-not-found, etc., that
// is not a bug in the code.
#ifndef HETCOMPUTE_THROW_ON_API_ASSERT
#define HETCOMPUTE_FATAL(format, ...) HETCOMPUTE_EXIT_FATAL(format, ##__VA_ARGS__)
#else
#define HETCOMPUTE_FATAL(format, ...)                                                                                                            \
    do                                                                                                                                         \
    {                                                                                                                                          \
        throw ::hetcompute::internal::fatal_exception(::hetcompute::internal::strprintf(format, ##__VA_ARGS__), __FILE__, __LINE__, __FUNCTION__); \
    } while (false)
#endif // HETCOMPUTE_THROW_ON_API_ASSERT

// HETCOMPUTE_API_ASSERT
//  - Causes immediate exit
//  - Always enabled
#ifdef HETCOMPUTE_THROW_ON_API_ASSERT
#define HETCOMPUTE_API_ASSERT(cond, format, ...)                                                                                             \
    do                                                                                                                                     \
    {                                                                                                                                      \
        if (!(cond))                                                                                                                       \
        {                                                                                                                                  \
            throw ::hetcompute::internal::api_assert_exception(::hetcompute::internal::strprintf(format, ##__VA_ARGS__),                       \
                                                             __FILE__,                                                                     \
                                                             __LINE__,                                                                     \
                                                             __FUNCTION__);                                                                \
        }                                                                                                                                  \
    } while (false)

#else

#define HETCOMPUTE_API_ASSERT_COND(cond) #cond
#define HETCOMPUTE_API_ASSERT(cond, format, ...)                                                                                             \
    do                                                                                                                                     \
    {                                                                                                                                      \
        if (!(cond))                                                                                                                       \
        {                                                                                                                                  \
            HETCOMPUTE_EXIT_FATAL("API assert [%s] - " format, HETCOMPUTE_API_ASSERT_COND(cond), ##__VA_ARGS__);                               \
        }                                                                                                                                  \
    } while (false)
#endif // HETCOMPUTE_THROW_ON_API_ASSERT

// HETCOMPUTE_INTERNAL_ASSERT
//  - Causes immediate exit
//  - Disabled in NDEBUG, otherwise enabled by default.
//  - Override with HETCOMPUTE_ENABLE_CHECK_INTERNAL or HETCOMPUTE_DISABLE_CHECK_INTERNAL

// Used for checking internal data structures and ensures the integrity
// of the library. This assert generally indicates a bug in the library
// itself and should be reported to the developers.
#ifdef HETCOMPUTE_DEBUG
#define HETCOMPUTE_CHECK_INTERNAL
#endif

#ifdef HETCOMPUTE_DISABLE_CHECK_INTERNAL
#undef HETCOMPUTE_CHECK_INTERNAL
#endif // HETCOMPUTE_DISABLE_CHECK_INTERNAL

#ifdef HETCOMPUTE_ENABLE_CHECK_INTERNAL
#ifndef HETCOMPUTE_CHECK_INTERNAL
#define HETCOMPUTE_CHECK_INTERNAL
#endif
#endif // HETCOMPUTE_ENABLE_CHECK_INTERNAL

#ifdef HETCOMPUTE_CHECK_INTERNAL
#ifndef HETCOMPUTE_THROW_ON_API_ASSERT
#define HETCOMPUTE_INTERNAL_ASSERT_COND(cond) #cond
#define HETCOMPUTE_INTERNAL_ASSERT(cond, format, ...)                                                                                        \
    do                                                                                                                                     \
    {                                                                                                                                      \
        if (!(cond))                                                                                                                       \
        {                                                                                                                                  \
            HETCOMPUTE_EXIT_FATAL("Internal assert failed [%s] - " format, HETCOMPUTE_INTERNAL_ASSERT_COND(cond), ##__VA_ARGS__);              \
        }                                                                                                                                  \
    } while (false)

#else // HETCOMPUTE_THROW_ON_API_ASSERT
#define HETCOMPUTE_INTERNAL_ASSERT(cond, format, ...)                                                                                        \
    do                                                                                                                                     \
    {                                                                                                                                      \
        if (!(cond))                                                                                                                       \
        {                                                                                                                                  \
            throw ::hetcompute::internal::assert_exception(::hetcompute::internal::strprintf(format, ##__VA_ARGS__),                           \
                                                         __FILE__,                                                                         \
                                                         __LINE__,                                                                         \
                                                         __FUNCTION__);                                                                    \
        }                                                                                                                                  \
    } while (false)
#endif // HETCOMPUTE_THROW_ON_API_ASSERT

#else // !HETCOMPUTE_CHECK_INTERNAL
#define HETCOMPUTE_INTERNAL_ASSERT(cond, format, ...)                                                                                        \
    do                                                                                                                                     \
    {                                                                                                                                      \
        if (false)                                                                                                                         \
        {                                                                                                                                  \
            if (cond)                                                                                                                      \
            {                                                                                                                              \
            }                                                                                                                              \
                                                                                                                                           \
            if (format)                                                                                                                    \
            {                                                                                                                              \
            }                                                                                                                              \
        }                                                                                                                                  \
    } while (false)
#endif // HETCOMPUTE_CHECK_INTERNAL

// Control how logging is performed based on the compilation flags
#ifndef HETCOMPUTE_DEBUG
// Disable some logging when HETCOMPUTE_DEBUG is disabled
#ifndef HETCOMPUTE_NOLOG
#define HETCOMPUTE_NOLOG
#endif
#endif // HETCOMPUTE_DEBUG

#ifdef HETCOMPUTE_NOLOG
#undef HETCOMPUTE_DLOG
#undef HETCOMPUTE_DLOGN

#define HETCOMPUTE_DLOG(format, ...)                                                                                                         \
    do                                                                                                                                     \
    {                                                                                                                                      \
    } while (false)

#define HETCOMPUTE_DLOGN(format, ...)                                                                                                        \
    do                                                                                                                                     \
    {                                                                                                                                      \
    } while (false)

#endif // HETCOMPUTE_NOLOG
