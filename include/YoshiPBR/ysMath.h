#pragma once
#pragma warning(disable : 4201)

#include "YoshiPBR/ysTypes.h"

#include <emmintrin.h>
#include <xmmintrin.h>
#include <math.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysVec4
{
    union
    {
        struct
        {
            float x, y, z, w;
        };
        __m128 simd;
    };
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysTransform
{
    ysVec4 p;
    ysVec4 q;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysMtx44
{
    // columns
    ysVec4 cx, cy, cz, cw;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysFloat3
{
    union
    {
        struct
        {
            ys_float32 x, y, z;
        };

        struct
        {
            ys_float32 r, g, b;
        };
    };
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysFloat4
{
    union
    {
        struct
        {
            ys_float32 x, y, z, w;
        };

        struct
        {
            ys_float32 r, g, b, a;
        };
    };
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Useful constants
extern const ysVec4 ysQuat_identity;
extern const ysVec4 ysVec4_zero;
extern const ysVec4 ysVec4_half;
extern const ysVec4 ysVec4_one;
extern const ysVec4 ysVec4_two;
extern const ysVec4 ysVec4_pi;
extern const ysVec4 ysVec4_2pi;
extern const ysVec4 ysVec4_maxFloat;
extern const ysVec4 ysVec4_unitX;
extern const ysVec4 ysVec4_unitY;
extern const ysVec4 ysVec4_unitZ;
extern const ysVec4 ysVec4_unitW;
extern const ysTransform ysTransform_identity;
extern const ysMtx44 ysMtx44_zero;
extern const ysMtx44 ysMtx44_identity;

ys_float32 ysSafeReciprocal(ys_float32);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The unspecified channels are set to 0
ysVec4 ysVecSet(ys_float32 x, ys_float32 y, ys_float32 z, ys_float32 w);
ysVec4 ysVecSet(ys_float32 x, ys_float32 y, ys_float32 z);
ysVec4 ysVecSet(ys_float32 x, ys_float32 y);
ysVec4 ysVecSet(ys_float32 x);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// All channels are set to the specified value
ysVec4 ysSplat(ys_float32);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Component splatters
ysVec4 ysSplatX(const ysVec4&);
ysVec4 ysSplatY(const ysVec4&);
ysVec4 ysSplatZ(const ysVec4&);
ysVec4 ysSplatW(const ysVec4&);

template <typename T>
void ysSwap(T& a, T& b)
{
    T tmp = a;
    a = b;
    b = tmp;
}
template <typename T>
T ysMin(const T& a, const T& b)
{
    return (a < b) ? a : b;
}
template <typename T>
T ysMax(const T& a, const T& b)
{
    return (a > b) ? a : b;
}
template <typename T>
T ysClamp(const T& unclamped, const T& min, const T& max)
{
    return ysMin(ysMax(min, unclamped), max);
}
template <typename T>
T ysAbs(const T& a)
{
    return ysMax(a, -a);
}
ysVec4 ysMin(const ysVec4&, const ysVec4&);
ysVec4 ysMax(const ysVec4&, const ysVec4&);
ysVec4 ysAbs(const ysVec4&);

bool ysAllLT3(const ysVec4&, const ysVec4&);
bool ysAllLE3(const ysVec4&, const ysVec4&);
bool ysAllEQ3(const ysVec4&, const ysVec4&);
bool ysAllGE3(const ysVec4&, const ysVec4&);
bool ysAllGT3(const ysVec4&, const ysVec4&);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Operator overloads
ysVec4 operator-(const ysVec4&);
ysVec4 operator+(const ysVec4&, const ysVec4&);
ysVec4 operator-(const ysVec4&, const ysVec4&);
ysVec4 operator*(const ysVec4&, const ysVec4&);
ysVec4 operator/(const ysVec4&, const ysVec4&);
ysVec4& operator+=(ysVec4&, const ysVec4&);
ysVec4& operator-=(ysVec4&, const ysVec4&);
ysVec4& operator*=(ysVec4&, const ysVec4&);
ysVec4& operator/=(ysVec4&, const ysVec4&);
bool operator==(const ysVec4&, const ysVec4&);
bool operator!=(const ysVec4&, const ysVec4&);

ysVec4 ysSplatDot4(const ysVec4&, const ysVec4&);
ysVec4 ysSplatDot3(const ysVec4&, const ysVec4&);
ys_float32 ysDot4(const ysVec4&, const ysVec4&);
ys_float32 ysDot3(const ysVec4&, const ysVec4&);
ys_float32 ysLengthSqr4(const ysVec4&);
ys_float32 ysLengthSqr3(const ysVec4&);
ys_float32 ysLength4(const ysVec4&);
ys_float32 ysLength3(const ysVec4&);
bool ysIsSafeToNormalize4(const ysVec4&); // We are pretty strict. Squared norm must be larger than eps^2
bool ysIsSafeToNormalize3(const ysVec4&);
bool ysIsApproximatelyNormalized4(const ysVec4&);
bool ysIsApproximatelyNormalized3(const ysVec4&);
ysVec4 ysNormalize4(const ysVec4&);
ysVec4 ysNormalize3(const ysVec4&);
ysVec4 ysCross(const ysVec4&, const ysVec4&);

ysVec4 ysConjugate(const ysVec4& q);

ysVec4 ysQuatFromAxisAngle(const ysVec4& axis, float angle);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Compute the quaternion that aligns src to dst
ysVec4 ysQuatFromUnitVectors(const ysVec4& src, const ysVec4& dst);

ysMtx44 ysMtxFromQuat(const ysVec4& q);
ysMtx44 ysMtxFromTransform(const ysTransform&);

ysTransform ysInvert(const ysTransform&);

ysVec4 ysRotate(const ysVec4& q, const ysVec4& v);

ysVec4 ysMulQQ(const ysVec4& q2, const ysVec4& q1);
ysTransform ysMul(const ysTransform& xf2, const ysTransform& xf1);
ysVec4 ysMul(const ysTransform& xf, const ysVec4& p);
ysVec4 ysMul33(const ysMtx44&, const ysVec4&);
ysVec4 ysMulT33(const ysMtx44&, const ysVec4&);

ys_float32 ysRandom(ys_float32 min, ys_float32 max);
ysVec4 ysRandom3(const ysVec4& min, const ysVec4& max);
ysVec4 ysRandomDir();
ysVec4 ysRandomQuat();