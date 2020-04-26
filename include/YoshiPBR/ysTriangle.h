#pragma once

#include "YoshiPBR/ysAABB.h"

struct ysRayCastInput;
struct ysRayCastOutput;
struct ysSurfacePoint;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysTriangle
{
    ysAABB ComputeAABB() const;
    bool RayCast(ysRayCastOutput*, const ysRayCastInput&) const;
    void GenerateRandomSurfacePoint(ysSurfacePoint* point, ys_float32* probabilityDensity) const;
    bool GenerateRandomVisibleSurfacePoint(ysSurfacePoint* point, ys_float32* probabilityDensity, const ysVec4& vantagePoint) const;
    ys_float32 ProbabilityDensityForGeneratedPoint(const ysVec4& point, const ysVec4& vantagePoint) const;

    ysVec4 m_v[3]; // vertices
    ysVec4 m_n;    // normal
    ysVec4 m_t;    // tangent
    bool m_twoSided;
};