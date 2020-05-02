#pragma once

// Macro to suppress warnings about unused variables
#define HETCOMPUTE_UNUSED(x) ((void)x)

// Macros to make the VS release numbers more memorable
#ifdef _MSC_VER
    #define HETCOMPUTE_VS2015 1900
    #define HETCOMPUTE_VS2013 1800
    #define HETCOMPUTE_VS2012 1700
#endif // _MSC_VER

// Macro to define the proper incantation of inline for C files
#ifndef HETCOMPUTE_INLINE
    #ifdef _MSC_VER
        #define HETCOMPUTE_INLINE __inline
    #else
        #define HETCOMPUTE_INLINE inline
    #endif // _MSC_VER
#endif

// Macro to define the proper constexpr for different compilers
#ifndef HETCOMPUTE_CONSTEXPR
    #ifdef _MSC_VER
        #define HETCOMPUTE_CONSTEXPR
    #else
        #define HETCOMPUTE_CONSTEXPR constexpr
    #endif // _MSC_VER
#endif // HETCOMPUTE_CONSTEXPR

// and the replacement with const
#ifndef HETCOMPUTE_CONSTEXPR_CONST
    #ifdef _MSC_VER
        #define HETCOMPUTE_CONSTEXPR_CONST const
    #else
        #define HETCOMPUTE_CONSTEXPR_CONST constexpr
    #endif // _MSC_VER
#endif // HETCOMPUTE_CONSTEXPR_CONST

// Macros to implement the alignment stuff
#ifdef _MSC_VER
    #define HETCOMPUTE_ALIGN(size) __declspec(align(size))
    #define HETCOMPUTE_ALIGNED(type, name, size) __declspec(align(size)) type name
#else
    #define HETCOMPUTE_ALIGN(size) __attribute__((aligned(size)))
    #define HETCOMPUTE_ALIGNED(type, name, size) type name __attribute__((aligned(size)))
#endif

// Macro to provide an interface for GCC-specific attributes, but that
// can be skipped on different compilers such as VS2012 that do not
// support these.
#ifdef _MSC_VER
    #define HETCOMPUTE_GCC_ATTRIBUTE(args)
#else
    #define HETCOMPUTE_GCC_ATTRIBUTE(args) __attribute__(args)
#endif

#ifdef __GNUC__
    #define HETCOMPUTE_DEPRECATED(decl) decl __attribute__((deprecated))
#elif defined(_MSC_VER)
    #define HETCOMPUTE_DEPRECATED(decl) __declspec(deprecated) decl
#else
    #warning No HETCOMPUTE_DEPRECATED implementation for this compiler
    #define HETCOMPUTE_DEPRECATED(decl) decl
#endif

// VS2012 does not support the noexcept keyword, so use a macro to
// provide different implementations. VS2015 does.
#if defined(_MSC_VER) && (_MSC_VER < HETCOMPUTE_VS2015)
    #define HETCOMPUTE_NOEXCEPT
#else
    #define HETCOMPUTE_NOEXCEPT noexcept
#endif

// VS2012 does not support explicitly deleted methods, so use a macro
// to provide different implementations. VS2015 does.
#if defined(_MSC_VER) && (_MSC_VER < HETCOMPUTE_VS2015)
    // We do not provide an implementation here. This is ok, and if
    // someone tries to call it, they will get a linker error.  You cannot
    // put an implementation here, because some constructors need to
    // specify initializers.
    #define HETCOMPUTE_DELETE_METHOD(...) __VA_ARGS__
#else
    // Use C++11 features to do this more cleanly
    #define HETCOMPUTE_DELETE_METHOD(...) __VA_ARGS__ = delete
#endif

// VS2012 does not support explicit default constructors, so use a
// macro to provide different implementations. VS2015 does.
#if defined(_MSC_VER) && (_MSC_VER < HETCOMPUTE_VS2015)
    #define HETCOMPUTE_DEFAULT_METHOD(...) __VA_ARGS__ {}
#else
    #define HETCOMPUTE_DEFAULT_METHOD(...) __VA_ARGS__ = default
#endif
