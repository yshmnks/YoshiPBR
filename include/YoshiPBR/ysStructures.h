#pragma once

#include "YoshiPBR/ysMath.h"

struct ysDebugDraw;

struct ysMaterialId
{
    ys_int32 m_index;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysInputTriangle
{
    // A free-standing triangle
    ysVec4 vertices[3];
    ysMaterialId materialId;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysSceneDef
{
    ysSceneDef()
    {
        m_triangles = nullptr;
        m_triangleCount = 0;
    }

    const ysInputTriangle* m_triangles;
    ys_int32 m_triangleCount;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysDrawInputBVH
{
    ysDrawInputBVH()
    {
        debugDraw = nullptr;
        depth = ys_nullIndex;
    }

    ysDebugDraw* debugDraw;

    // If null, all nodes will be drawn. (depth=0 at the root and increases descending into the children)
    ys_int32 depth;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysDrawInputGeo
{
    ysDrawInputGeo()
    {
        debugDraw = nullptr;
    }

    ysDebugDraw* debugDraw;
};