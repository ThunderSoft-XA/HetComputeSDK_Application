#pragma once
// Define a set macros that are used for compilers compatibility

#ifdef __GNUC__
    #define HETCOMPUTE_GCC_PRAGMA(x) _Pragma(#x)
    #define HETCOMPUTE_GCC_PRAGMA_DIAG(x) HETCOMPUTE_GCC_PRAGMA(GCC diagnostic x)
    #define HETCOMPUTE_GCC_IGNORE_BEGIN(x) HETCOMPUTE_GCC_PRAGMA_DIAG(push)         \
                                         HETCOMPUTE_GCC_PRAGMA_DIAG(ignored x)
    #define HETCOMPUTE_GCC_IGNORE_END(x) HETCOMPUTE_GCC_PRAGMA_DIAG(pop)

    #ifdef __clang__
        #define HETCOMPUTE_CLANG_PRAGMA_DIAG(x) HETCOMPUTE_GCC_PRAGMA(clang diagnostic x)
        #define HETCOMPUTE_CLANG_IGNORE_BEGIN(x) HETCOMPUTE_CLANG_PRAGMA_DIAG(push)         \
                                               HETCOMPUTE_CLANG_PRAGMA_DIAG(ignored x)
        #define HETCOMPUTE_CLANG_IGNORE_END(x) HETCOMPUTE_CLANG_PRAGMA_DIAG(pop)
    #else
        #define HETCOMPUTE_CLANG_IGNORE_BEGIN(x)
        #define HETCOMPUTE_CLANG_IGNORE_END(x)
    #endif // __clang__
#else
    #define HETCOMPUTE_GCC_IGNORE_BEGIN(x)
    #define HETCOMPUTE_GCC_IGNORE_END(x)
    #define HETCOMPUTE_CLANG_IGNORE_BEGIN(x)
    #define HETCOMPUTE_CLANG_IGNORE_END(x)
#endif // __GNUC__

#ifdef _MSC_VER
    #define HETCOMPUTE_MSC_PRAGMA(x) __pragma(x)
    #define HETCOMPUTE_MSC_PRAGMA_WARNING(x) HETCOMPUTE_MSC_PRAGMA(warning(x))
    #define HETCOMPUTE_MSC_IGNORE_BEGIN(x) HETCOMPUTE_MSC_PRAGMA_WARNING(push)          \
                                         HETCOMPUTE_MSC_PRAGMA_WARNING(disable : x)
    #define HETCOMPUTE_MSC_IGNORE_END(x) HETCOMPUTE_MSC_PRAGMA_WARNING(pop)
#else
    #define HETCOMPUTE_MSC_IGNORE_BEGIN(x)
    #define HETCOMPUTE_MSC_IGNORE_END(x)
#endif

// Macros to control symbol visibility
#if defined(__clang__) /* clang must be first since it also defines __GNUC__*/
    // for now, it defines the same rules as GCC
    #define HETCOMPUTE_PUBLIC __attribute__ ((visibility ("default")))
    #define HETCOMPUTE_PUBLIC_START  HETCOMPUTE_GCC_PRAGMA(GCC visibility push(default))
    #define HETCOMPUTE_PUBLIC_STOP   HETCOMPUTE_GCC_PRAGMA(GCC visibility pop)
    #define HETCOMPUTE_PRIVATE __attribute__ ((visibility ("hidden")))
    #define HETCOMPUTE_PRIVATE_START HETCOMPUTE_GCC_PRAGMA(GCC visibility push(hidden))
    #define HETCOMPUTE_PRIVATE_STOP  HETCOMPUTE_GCC_PRAGMA(GCC visibility pop)
#elif defined(__GNUC__)
    #define HETCOMPUTE_PUBLIC  __attribute__ ((visibility ("default")))
    #define HETCOMPUTE_PUBLIC_START  HETCOMPUTE_GCC_PRAGMA(GCC visibility push(default))
    #define HETCOMPUTE_PUBLIC_STOP   HETCOMPUTE_GCC_PRAGMA(GCC visibility pop)
    #define HETCOMPUTE_PRIVATE __attribute__ ((visibility ("hidden")))
    #define HETCOMPUTE_PRIVATE_START HETCOMPUTE_GCC_PRAGMA(GCC visibility push(hidden))
    #define HETCOMPUTE_PRIVATE_STOP  HETCOMPUTE_GCC_PRAGMA(GCC visibility pop)
#elif defined(_MSC_VER)
    #ifdef HETCOMPUTE_DLL_EXPORT
        /* We are building the hetcompute library */
        #define HETCOMPUTE_PUBLIC __declspec(dllexport)
    #else
        /* We are using this library */
        #define HETCOMPUTE_PUBLIC __declspec(dllimport)
    #endif // HETCOMPUTE_DLL_EXPORT
    #define HETCOMPUTE_PRIVATE
#else
    #error "Unknown compiler. Please use GCC, Clang, or Visual Studio."
#endif
