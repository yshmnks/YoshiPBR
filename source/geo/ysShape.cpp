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