#pragma once

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
// Useful constants
extern const ysVec4 ysQuat_identity;
extern const ysVec4 ysVec4_zero;
extern const ysVec4 ysVec4_half;
extern const ysVec4 ysVec4_one;
extern const ysVec4 ysVec4_two;
extern const ysVec4 ysVec4_unitX;
extern const ysVec4 ysVec4_unitY;
extern const ysVec4 ysVec4_unitZ;
extern const ysVec4 ysVec4_unitW;
extern const ysTransform ysTransform_identity;
extern const ysMtx44 ysMtx44_zero;
extern const ysMtx44 ysMtx44_identity;

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Operator overloads
ysVec4 operator-(const ysVec4&);
ysVec4 operator+(const ysVec4&, const ysVec4&);
ysVec4 operator-(const ysVec4&, const ysVec4&);
ysVec4 operator*(const ysVec4&, const ysVec4&);
ysVec4 operator/(const ysVec4&, const ysVec4&);

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