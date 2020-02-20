#pragma once

#include "YoshiPBR/ysTypes.h"
#include "YoshiPBR/ysBVH.h"

struct ysDrawInputGeo;
struct ysSceneDef;
struct ysShape;
struct ysTriangle;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysScene
{
    void Reset();
    void Create(const ysSceneDef*);
    void Destroy();

    void DebugDrawGeo(const ysDrawInputGeo*) const;

    ysBVH m_bvh;

    ysShape* m_shapes;
    ys_int32 m_shapeCount;

    ysTriangle* m_triangles;
    ys_int32 m_triangleCount;
};