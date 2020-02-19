#pragma once

#include <assert.h>
#include <float.h>

#define ysAssert(x)		\
    if (!(x))			\
    {					\
        __debugbreak();	\
    }

#define ysNew new
#define ysDelete(x) delete x
#define ysDeleteArray(x) delete[] x
#define ysMemCpy(dst, src, byteCount) memcpy(dst, src, byteCount)
#define ysMalloc(byteCount) _aligned_malloc(byteCount, 16)
#define ysMallocAlign(byteCount, byteAlignment) _aligned_malloc(byteCount, byteAlignment)
#define ysFree(x) _aligned_free(x)

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

#define	ys_maxFloat		FLT_MAX
#define	ys_epsilon		FLT_EPSILON
#define ys_pi			3.14159265359f

#define ys_nullIndex    (-1)