#pragma once

#include "YoshiPBR/ysBVH.h"
#include "YoshiPBR/ysPool.h"
#include "YoshiPBR/ysTypes.h"

#include "scene/ysRender.h"

#define YOSHIPBR_MAX_SCENE_COUNT (1)

struct ysDrawInputGeo;
struct ysLight;
struct ysLightPoint;
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
    static ysScene* s_scenes[YOSHIPBR_MAX_SCENE_COUNT];

    struct ysSurfaceData;

    void Reset();
    void Create(const ysSceneDef&);
    void Destroy();

    bool RayCastClosest(ysSceneRayCastOutput*, const ysSceneRayCastInput&) const;

    ysVec4 SampleRadiance(const ysSurfaceData&, ys_int32 bounceCount, ys_int32 maxBounceCount, bool sampleLight, bool sampleBRDF) const;

    void DoRenderWork(ysRender* target) const;
    void Render(ysSceneRenderOutput* output, const ysSceneRenderInput& input) const;

    void DebugDrawGeo(const ysDrawInputGeo&) const;
    void DebugDrawLights(const ysDrawInputLights&) const;

    ysBVH m_bvh;

    ysShape* m_shapes;
    ys_int32 m_shapeCount;

    ysTriangle* m_triangles;
    ys_int32 m_triangleCount;

    ysMaterial* m_materials;
    ys_int32 m_materialCount;

    ysMaterialStandard* m_materialStandards;
    ys_int32 m_materialStandardCount;

    ysLight* m_lights;
    ys_int32 m_lightCount;

    ysLightPoint* m_lightPoints;
    ys_int32 m_lightPointCount;

    ys_int32* m_emissiveShapeIndices;
    ys_int32 m_emissiveShapeCount;
    
    ysPool<ysRender> m_renders;
};