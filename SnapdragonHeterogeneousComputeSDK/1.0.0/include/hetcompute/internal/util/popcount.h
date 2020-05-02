#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#include <stdlib.h>
#include <stdint.h>
#include <hetcompute/internal/compat/compiler_compat.h>

// Macro to define the proper incantation of inline for C files
#ifndef HETCOMPUTE_INLINE
#ifdef _MSC_VER
#define HETCOMPUTE_INLINE __inline
#else
#define HETCOMPUTE_INLINE inline
#endif // _MSC_VER
#endif

///////////////////////////////////////////////////////////////////////////////

// http://openvswitch.org/pipermail/dev/2012-July/019297.html
// Alternative: HACKMEM 169
static HETCOMPUTE_INLINE int
hetcompute_popcount(uint32_t x)
{
#ifdef HAVE___BUILTIN_POPCOUNT
    return __builtin_popcount(x);
#else
    #define HETCOMPUTE_INIT1(X)                                                                                                                  \
        ((((X) & (1 << 0)) != 0) + (((X) & (1 << 1)) != 0) + (((X) & (1 << 2)) != 0) + (((X) & (1 << 3)) != 0) + (((X) & (1 << 4)) != 0) +     \
        (((X) & (1 << 5)) != 0) + (((X) & (1 << 6)) != 0) + (((X) & (1 << 7)) != 0))
    #define HETCOMPUTE_INIT2(X) HETCOMPUTE_INIT1(X), HETCOMPUTE_INIT1((X) + 1)
    #define HETCOMPUTE_INIT4(X) HETCOMPUTE_INIT2(X), HETCOMPUTE_INIT2((X) + 2)
    #define HETCOMPUTE_INIT8(X) HETCOMPUTE_INIT4(X), HETCOMPUTE_INIT4((X) + 4)
    #define HETCOMPUTE_INIT16(X) HETCOMPUTE_INIT8(X), HETCOMPUTE_INIT8((X) + 8)
    #define HETCOMPUTE_INIT32(X) HETCOMPUTE_INIT16(X), HETCOMPUTE_INIT16((X) + 16)
    #define HETCOMPUTE_INIT64(X) HETCOMPUTE_INIT32(X), HETCOMPUTE_INIT32((X) + 32)

    static const uint8_t popcount8[256] = { HETCOMPUTE_INIT64(0), HETCOMPUTE_INIT64(64), HETCOMPUTE_INIT64(128), HETCOMPUTE_INIT64(192) };
    return (popcount8[x & 0xff] + popcount8[(x >> 8) & 0xff] + popcount8[(x >> 16) & 0xff] + popcount8[x >> 24]);
#endif
}

///////////////////////////////////////////////////////////////////////////////

HETCOMPUTE_CLANG_IGNORE_BEGIN("-Wold-style-cast");

static HETCOMPUTE_INLINE int
hetcompute_popcountl(unsigned long int x)
{
#ifdef HAVE___BUILTIN_POPCOUNTL
    return __builtin_popcountl(x);
#else
    int cnt = hetcompute_popcount((uint32_t)(x));
    if (sizeof x > 4)
    {
        cnt += hetcompute_popcount((uint32_t)(x >> 31 >> 1));
    }
    return cnt;
#endif
}

///////////////////////////////////////////////////////////////////////////////

static HETCOMPUTE_INLINE int
hetcompute_popcountw(size_t x)
{
    int cnt = hetcompute_popcount((uint32_t)(x));
    if (sizeof x > 4)
    {
        cnt += hetcompute_popcount((uint32_t)(x >> 31 >> 1));
    }
    return cnt;
}

HETCOMPUTE_CLANG_IGNORE_END("-Wold-style-cast");

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
