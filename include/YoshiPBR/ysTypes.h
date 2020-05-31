#pragma once

#include <assert.h>
#include <float.h>
#include <memory>

#define YS_REF(x) (void)x

#if defined(_DEBUG)
#define ysDEBUG_BUILD (1)
#else
#define ysDEBUG_BUILD (0)
#endif

#define ysAssert(x)		\
    if (!(x))			\
    {					\
        __debugbreak();	\
    }

#if ysDEBUG_BUILD
    #define ysAssertDebug(x) ysAssert(x)
#else
    #define ysAssertDebug(x)
#endif

#define ysAssertCompile(x) static_assert(x, #x)

// TODO: How to do aligned new? Use placement new?
#define ysNew new
#define ysDelete(x) delete x
#define ysDeleteArray(x) delete[] x
#define ysMemCpy(dst, src, byteCount) memcpy(dst, src, byteCount)
#define ysMemSet(dst, byteValue, byteCount) memset(dst, byteValue, byteCount)
#define ysMemCmp(mem1, mem2, byteCount) memcmp(mem1, mem2, byteCount)
#define ysMalloc(byteCount) _aligned_malloc(byteCount, 16)
#define ysMallocAlign(byteCount, byteAlignment) _aligned_malloc(byteCount, byteAlignment)
#define ysFree(x) _aligned_free(x)
#define ysSafeFree(x) \
    _aligned_free(x); \
    x = nullptr;

template <typename T>
void ysSwap(T& a, T& b)
{
    T tmp = a;
    a = b;
    b = tmp;
}

typedef signed char	ys_int8;
typedef signed short ys_int16;
typedef signed int ys_int32;
typedef signed long long ys_int64;
typedef unsigned char ys_uint8;
typedef unsigned short ys_uint16;
typedef unsigned int ys_uint32;
typedef signed long long ys_uint64;
typedef float ys_float32;
typedef double ys_float64;

#define	ys_maxFloat     FLT_MAX
#define	ys_epsilon      FLT_EPSILON
#define ys_zeroSafe     FLT_EPSILON // General purpose threshold to prevent division by zero. TODO: How small should we allow this to go?
#define ys_pi           3.14159265359f
#define ys_2pi          6.28318530717f

#define ys_nullIndex    (-1)