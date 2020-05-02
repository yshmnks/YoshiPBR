#pragma once

#include "YoshiPBR/ysAABB.h"
#include "YoshiPBR/ysStructures.h"

struct ysRayCastInput;
struct ysRayCastOutput;
struct ysScene;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysSurfacePoint
{
    ysVec4 m_point;
    ysVec4 m_normal;
    ysVec4 m_tangent;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysShape
{
    enum Type
    {
        e_triangle,
    };

    ysAABB ComputeAABB(const ysScene* scene) const;
    bool RayCast(const ysScene* scene, ysRayCastOutput*, const ysRayCastInput&) const;
    void GenerateRandomSurfacePoint(const ysScene*, ysSurfacePoint* point, ys_float32* probabilityDensity) const;
    bool GenerateRandomVisibleSurfacePoint(const ysScene*, ysSurfacePoint* point, ys_float32* probabilityDensity, const ysVec4& vantagePoint) const;
    ys_float32 ProbabilityDensityForGeneratedPoint(const ysScene*, const ysVec4& point, const ysVec4& vantagePoint) const;

    Type m_type;
    ys_int32 m_typeIndex;

    ysMaterialId m_materialId;
};