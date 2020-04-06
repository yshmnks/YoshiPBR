#include "YoshiPBR/ysShape.h"
#include "YoshiPBR/ysTriangle.h"
#include "scene/ysScene.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysAABB ysShape::ComputeAABB(const ysScene* scene) const
{
    switch (m_type)
    {
        case Type::e_triangle:
        {
            const ysTriangle& triangle = scene->m_triangles[m_typeIndex];
            return triangle.ComputeAABB();
        }
        default:
        {
            ysAssert(false);
            ysAABB aabb;
            aabb.SetInvalid();
            return aabb;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ysShape::RayCast(const ysScene* scene, ysRayCastOutput* output, const ysRayCastInput& input) const
{
    switch (m_type)
    {
        case Type::e_triangle:
        {
            const ysTriangle& triangle = scene->m_triangles[m_typeIndex];
            return triangle.RayCast(output, input);
        }
        default:
            ysAssert(false);
            return false;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ysShape::GenerateRandomVisibleSurfacePoint(const ysScene* scene, ysSurfacePoint* point, ys_float32* probabilityDensity, const ysVec4& vantagePoint) const
{
    switch (m_type)
    {
        case Type::e_triangle:
        {
            const ysTriangle& triangle = scene->m_triangles[m_typeIndex];
            return triangle.GenerateRandomVisibleSurfacePoint(point, probabilityDensity, vantagePoint);
        }
        default:
            ysAssert(false);
            return false;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ys_float32 ysShape::ProbabilityDensityForGeneratedPoint(const ysScene* scene, const ysVec4& point, const ysVec4& vantagePoint) const
{
    ys_float32 probDens;
    switch (m_type)
    {
        case Type::e_triangle:
        {
            const ysTriangle& triangle = scene->m_triangles[m_typeIndex];
            probDens = triangle.ProbabilityDensityForGeneratedPoint(point, vantagePoint);
            break;
        }
        default:
        {
            ysAssert(false);
            probDens = 0.0f;
            break;
        }
    }
    ysAssert(probDens >= 0.0f);
    return probDens;
}