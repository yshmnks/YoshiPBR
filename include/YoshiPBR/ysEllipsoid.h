#pragma once

#include "YoshiPBR/ysAABB.h"

struct ysRayCastInput;
struct ysRayCastOutput;
struct ysSurfacePoint;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysEllipsoid
{
    ysAABB ComputeAABB() const;
    bool RayCast(ysRayCastOutput*, const ysRayCastInput&) const;
    void GenerateRandomSurfacePoint(ysSurfacePoint* point, ys_float32* probabilityDensity) const;
    ys_float32 ProbabilityDensityForGeneratedPoint(const ysVec4& point) const;

    ysTransform m_xf;
    ysVec4 m_s;
    ysVec4 m_sInv;
};