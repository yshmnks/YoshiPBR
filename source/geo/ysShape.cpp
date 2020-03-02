#include "YoshiPBR/ysShape.h"
#include "YoshiPBR/ysScene.h"
#include "YoshiPBR/ysTriangle.h"

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
void ysShape::ConvertFromWSToLS(const ysScene* scene, ysVec4* posLS, ysVec4* dirLS, const ysVec4& posWS, const ysVec4& dirWS) const
{
    switch (m_type)
    {
        case Type::e_triangle:
        {
            const ysTriangle& triangle = scene->m_triangles[m_typeIndex];
            triangle.ConvertFromWSToLS(posLS, dirLS, posWS, dirWS);
            break;
        }
        default:
            ysAssert(false);
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysShape::ConvertFromLSToWS(const ysScene* scene, ysVec4* posWS, ysVec4* dirWS, const ysVec4& posLS, const ysVec4& dirLS) const
{
    switch (m_type)
    {
        case Type::e_triangle:
        {
            const ysTriangle& triangle = scene->m_triangles[m_typeIndex];
            triangle.ConvertFromWSToLS(posWS, dirWS, posLS, dirLS);
            break;
        }
        default:
            ysAssert(false);
            break;
    }
}