#pragma once

#include "YoshiPBR/ysTypes.h"
#include "YoshiPBR/ysBVH.h"

struct ysDrawInputGeo;
struct ysMaterial;
struct ysMaterialStandard;
struct ysRayCastInput;
struct ysRayCastOutput;
struct ysSceneDef;
struct ysSceneRayCastInput;
struct ysSceneRayCastOutput;
struct ysSceneRenderInput;
struct ysSceneRenderOutput;
struct ysRay;
struct ysShape;
struct ysTriangle;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysScene
{
    void Reset();
    void Create(const ysSceneDef*);
    void Destroy();

    bool RayCastClosest(ysSceneRayCastOutput*, const ysSceneRayCastInput&) const;

    ysVec4 SampleRadiance(const ysRay&, ys_int32 maxBounceCount) const;

    void Render(ysSceneRenderOutput* output, const ysSceneRenderInput* input) const;

    void DebugDrawGeo(const ysDrawInputGeo*) const;

    ysBVH m_bvh;

    ysShape* m_shapes;
    ys_int32 m_shapeCount;

    ysTriangle* m_triangles;
    ys_int32 m_triangleCount;

    ysMaterial* m_materials;
    ys_int32 m_materialCount;

    ysMaterialStandard* m_materialStandards;
    ys_int32 m_materialStandardCount;
};