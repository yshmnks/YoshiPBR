#pragma once

#include "YoshiPBR/ysTypes.h"
#include "YoshiPBR/ysBVH.h"

struct ysShape;
struct ysTriangle;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysScene
{
    ysBVH m_bvh;

    ysShape* m_shapes;
    ys_int32 m_shapeCount;

    ysTriangle* m_triangles;
    ys_int32 m_triangleCount;
};