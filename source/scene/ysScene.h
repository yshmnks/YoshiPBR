#pragma once

#include "YoshiPBR/ysBVH.h"
#include "YoshiPBR/ysPool.h"
#include "YoshiPBR/ysTypes.h"

#include "scene/ysRender.h"

#define YOSHIPBR_MAX_SCENE_COUNT (1)

struct ysDrawInputGeo;
struct ysLight;
struct ysLightPoint;
struct ysEmissiveMaterial;
struct ysEmissiveMaterialUniform;
struct ysMaterial;
struct ysMaterialMirror;
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

    ysVec4 SampleRadiance(const ysSurfaceData&, ys_int32 bounceCount, ys_int32 maxBounceCount, bool sampleLight) const;

    struct GenerateSubpathInput;
    struct GenerateSubpathOutput;
    void GenerateSubpaths(GenerateSubpathOutput*, const GenerateSubpathInput&) const;
    ysVec4 EvaluateTruncatedSubpaths(const GenerateSubpathOutput&, ys_int32 truncatedEyeSubpathVertexCount, ys_int32 truncatedLightSubpathVertexCount) const;
    ysVec4 SampleRadiance_Bi(const GenerateSubpathInput&) const;

    ysVec4 RenderPixel(const ysSceneRenderInput& input, const ysVec4& pixelDirLS) const;
    void DoRenderWork(ysRender* target) const;
    void Render(ysSceneRenderOutput* output, const ysSceneRenderInput& input) const;

    ysVec4 DebugRenderPixel(const ysSceneRenderInput& input, ys_float32 pixelX, ys_float32 pixelY) const;

    void DebugDrawGeo(const ysDrawInputGeo&) const;
    void DebugDrawLights(const ysDrawInputLights&) const;

    ysBVH m_bvh;

    ////////////
    // Shapes //
    ////////////

    ysShape* m_shapes;
    ys_int32 m_shapeCount;

    ysTriangle* m_triangles;
    ys_int32 m_triangleCount;

    //////////////////////////
    // Reflective Materials //
    //////////////////////////

    ysMaterial* m_materials;
    ys_int32 m_materialCount;

    ysMaterialStandard* m_materialStandards;
    ys_int32 m_materialStandardCount;

    ysMaterialMirror* m_materialMirrors;
    ys_int32 m_materialMirrorCount;

    ////////////////////////
    // Emissive Materials //
    ////////////////////////

    ysEmissiveMaterial* m_emissiveMaterials;
    ys_int32 m_emissiveMaterialCount;

    ysEmissiveMaterialUniform* m_emissiveMaterialUniforms;
    ys_int32 m_emissiveMaterialUniformCount;

    ////////////////////////////////////////////////
    // Lights with infinitesimal or infinite area //
    ////////////////////////////////////////////////

    ysLight* m_lights;
    ys_int32 m_lightCount;

    ysLightPoint* m_lightPoints;
    ys_int32 m_lightPointCount;

    // These are for quickly iterating over all area light sources
    ys_int32* m_emissiveShapeIndices;
    ys_int32 m_emissiveShapeCount;
    
    ysPool<ysRender> m_renders;
};