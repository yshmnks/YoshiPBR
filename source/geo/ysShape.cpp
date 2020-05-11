#include "YoshiPBR/ysShape.h"
#include "YoshiPBR/ysEllipsoid.h"
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
        case Type::e_ellipsoid:
        {
            const ysEllipsoid& ellipsoid = scene->m_ellipsoids[m_typeIndex];
            return ellipsoid.ComputeAABB();
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
        case Type::e_ellipsoid:
        {
            const ysEllipsoid& ellipsoid = scene->m_ellipsoids[m_typeIndex];
            return ellipsoid.RayCast(output, input);
        }
        default:
            ysAssert(false);
            return false;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysShape::GenerateRandomSurfacePoint(const ysScene* scene, ysSurfacePoint* point, ys_float32* probabilityDensity) const
{
    switch (m_type)
    {
        case Type::e_triangle:
        {
            const ysTriangle& triangle = scene->m_triangles[m_typeIndex];
            triangle.GenerateRandomSurfacePoint(point, probabilityDensity);
            break;
        }
        case Type::e_ellipsoid:
        {
            const ysEllipsoid& ellipsoid = scene->m_ellipsoids[m_typeIndex];
            ellipsoid.GenerateRandomSurfacePoint(point, probabilityDensity);
            break;
        }
        default:
            ysAssert(false);
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ys_float32 ysShape::ProbabilityDensityForGeneratedPoint(const ysScene* scene, const ysVec4& point) const
{
    ys_float32 probDens;
    switch (m_type)
    {
        case Type::e_triangle:
        {
            const ysTriangle& triangle = scene->m_triangles[m_typeIndex];
            probDens = triangle.ProbabilityDensityForGeneratedPoint(point);
            break;
        }
        case Type::e_ellipsoid:
        {
            const ysEllipsoid& ellipsoid = scene->m_ellipsoids[m_typeIndex];
            probDens = ellipsoid.ProbabilityDensityForGeneratedPoint(point);
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