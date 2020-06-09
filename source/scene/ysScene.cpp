#include "ysScene.h"
#include "YoshiPBR/ysDebugDraw.h"
#include "light/ysLight.h"
#include "light/ysLightPoint.h"
#include "mat/emissive/ysEmissiveMaterial.h"
#include "mat/emissive/ysEmissiveMaterialUniform.h"
#include "mat/reflective/ysMaterial.h"
#include "mat/reflective/ysMaterialMirror.h"
#include "mat/reflective/ysMaterialStandard.h"
#include "threading/ysJobSystem.h"
#include "threading/ysParallelAlgorithms.h"
#include "YoshiPBR/ysEllipsoid.h"
#include "YoshiPBR/ysRay.h"
#include "YoshiPBR/ysShape.h"
#include "YoshiPBR/ysStructures.h"
#include "YoshiPBR/ysThreading.h"
#include "YoshiPBR/ysTriangle.h"

// TODO: As soon as markup any of the code with Tracy macros, Tracy initialization barfs complaining about my CPU
//       (i7-6700) missing the RDTSC instruction. This happens even for Tracy's provided example client applications.
#include "tracy/Tracy.hpp"

ysScene* ysScene::s_scenes[YOSHIPBR_MAX_SCENE_COUNT] = { nullptr };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static ysShapeId sShapeIdFromPtr(const ysScene* scene, const ysShape* shape)
{
    ysAssert(shape != nullptr);
    ysShapeId id;
    id.m_index = ys_int32(shape - scene->m_shapes);
    ysAssert(0 <= id.m_index && id.m_index < scene->m_shapeCount);
    return id;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct CollectClosestReflectiveData
{
    const ysScene* m_scene;
    ysShapeId m_ignoreShapeId;

    bool m_hit;
    ysSceneRayCastOutput m_output;
};

static ysRayCastFlowControlCode sCollectClosestReflective(const ysSceneRayCastOutput& hitOutput, void* voidData)
{
    CollectClosestReflectiveData* data = static_cast<CollectClosestReflectiveData*>(voidData);
    if (hitOutput.m_shapeId == data->m_ignoreShapeId)
    {
        return ysRayCastFlowControlCode::e_continue;
    }
    const ysShape* shape = data->m_scene->m_shapes + hitOutput.m_shapeId.m_index;
    bool isReflective = (shape->m_materialId != ys_nullMaterialId);
    if (isReflective)
    {
        data->m_hit = true;
        data->m_output = hitOutput;
        return ysRayCastFlowControlCode::e_clip;
    }
    return ysRayCastFlowControlCode::e_continue;
}

static bool sRayCastClosestReflective(const ysScene* scene, ysSceneRayCastOutput* output, const ysSceneRayCastInput& input, const ysShapeId& ignoreShapeId)
{
    CollectClosestReflectiveData data;
    data.m_scene = scene;
    data.m_ignoreShapeId = ignoreShapeId;
    data.m_hit = false;
    scene->m_bvh.RayCast(scene, input, &data, sCollectClosestReflective);
    *output = data.m_output;
    return data.m_hit;
}

static bool sRayCastClosestReflective(const ysScene* scene, ysSceneRayCastOutput* output, const ysSceneRayCastInput& input, const ysShape* ignoreShape)
{
    return sRayCastClosestReflective(scene, output, input, sShapeIdFromPtr(scene, ignoreShape));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SingleReflectiveMultipleEmissives
{
    bool m_hitReflective;
    ysSceneRayCastOutput m_reflectiveOutput;

    static const ys_int32 s_maxEmissiveCount = 16;
    ysSceneRayCastOutput m_emissiveOutputs[s_maxEmissiveCount];
    ys_int32 m_emissiveCount;
};

struct CollectEmissivesAndClosestReflectiveData
{
    const ysScene* m_scene;
    ysShapeId m_ignoreShapeId;

    SingleReflectiveMultipleEmissives* m_output;
};

static ysRayCastFlowControlCode sCollectEmissivesAndClosestReflective(const ysSceneRayCastOutput& hitOutput, void* voidData)
{
    CollectEmissivesAndClosestReflectiveData* data = static_cast<CollectEmissivesAndClosestReflectiveData*>(voidData);
    if (hitOutput.m_shapeId == data->m_ignoreShapeId)
    {
        return ysRayCastFlowControlCode::e_continue;
    }
    SingleReflectiveMultipleEmissives* srme = data->m_output;
    const ysShape* shape = data->m_scene->m_shapes + hitOutput.m_shapeId.m_index;
    bool isReflective = (shape->m_materialId != ys_nullMaterialId);
    if (isReflective)
    {
        srme->m_hitReflective = true;
        srme->m_reflectiveOutput = hitOutput;
        return ysRayCastFlowControlCode::e_clip;
    }
    bool isEmissive = (shape->m_emissiveMaterialId != ys_nullEmissiveMaterialId);
    if (isEmissive)
    {
        srme->m_emissiveOutputs[srme->m_emissiveCount++] = hitOutput;
    }
    return ysRayCastFlowControlCode::e_continue;
}

static void sRayCastClosestReflective_CollectEmissives
(
    const ysScene* scene,
    SingleReflectiveMultipleEmissives* output,
    const ysSceneRayCastInput& input,
    const ysShapeId& ignoreShapeId
)
{
    output->m_hitReflective = false;
    output->m_emissiveCount = 0;

    CollectEmissivesAndClosestReflectiveData data;
    data.m_scene = scene;
    data.m_ignoreShapeId = ignoreShapeId;
    data.m_output = output;

    scene->m_bvh.RayCast(scene, input, &data, sCollectEmissivesAndClosestReflective);

    if (output->m_hitReflective == false)
    {
        ysAssert(output->m_emissiveCount == 0);
        return;
    }
    
    ys_int32 n = 0;
    for (ys_int32 i = 0; i < output->m_emissiveCount; ++i)
    {
        if (output->m_emissiveOutputs[i].m_lambda < output->m_reflectiveOutput.m_lambda)
        {
            output->m_emissiveOutputs[n] = output->m_emissiveOutputs[i];
            n++;
        }
    }
    output->m_emissiveCount = n;
}

static void sRayCastClosestReflective_CollectEmissives(const ysScene* scene, SingleReflectiveMultipleEmissives* output, const ysSceneRayCastInput& input, const ysShape* ingoreShape)
{
    sRayCastClosestReflective_CollectEmissives(scene, output, input, sShapeIdFromPtr(scene, ingoreShape));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static ysVec4 sAccumulateDirectRadiance(const ysScene* scene, const ysVec4& dirWS, const ysSceneRayCastOutput* emissiveHits, ys_int32 emissiveHitCount)
{
    ysVec4 radiance = ysVec4_zero;
    for (ys_int32 i = 0; i < emissiveHitCount; ++i)
    {
        const ysSceneRayCastOutput& o = emissiveHits[i];
        const ysShape* s = scene->m_shapes + o.m_shapeId.m_index;
        ysAssert(s->m_materialId == ys_nullMaterialId);
        ysAssert(s->m_emissiveMaterialId != ys_nullEmissiveMaterialId);
        const ysEmissiveMaterial* e = scene->m_emissiveMaterials + s->m_emissiveMaterialId.m_index;
        ysMtx44 R;
        R.cx = o.m_hitTangent;
        R.cy = ysCross(o.m_hitNormal, o.m_hitTangent);
        R.cz = o.m_hitNormal;
        ysRadiance L = e->EvaluateRadiance(scene, ysMulT33(R, dirWS));
        if (L.m_isFinite)
        {
            radiance += L.m_value;
        }
    }
    return radiance;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene::Reset()
{
    m_bvh.Reset();
    m_shapes = nullptr;
    m_ellipsoids = nullptr;
    m_triangles = nullptr;
    m_materials = nullptr;
    m_materialStandards = nullptr;
    m_materialMirrors = nullptr;
    m_emissiveMaterials = nullptr;
    m_emissiveMaterialUniforms = nullptr;
    m_lights = nullptr;
    m_lightPoints = nullptr;
    m_emissiveShapeIndices = nullptr;
    m_shapeCount = 0;
    m_ellipsoidCount = 0;
    m_triangleCount = 0;
    m_materialCount = 0;
    m_materialStandardCount = 0;
    m_materialMirrorCount = 0;
    m_emissiveMaterialCount = 0;
    m_emissiveMaterialUniformCount = 0;
    m_lightCount = 0;
    m_lightPointCount = 0;
    m_emissiveShapeCount = 0;
    m_renders.Create();
    m_jobSystem = nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene::Create(const ysSceneDef& def)
{
    {
        m_shapeCount = def.m_ellipsoidCount + def.m_triangleCount;
        m_shapes = static_cast<ysShape*>(ysMalloc(sizeof(ysShape) * m_shapeCount));

        m_ellipsoidCount = def.m_ellipsoidCount;
        m_ellipsoids = static_cast<ysEllipsoid*>(ysMalloc(sizeof(ysEllipsoid) * m_ellipsoidCount));

        m_triangleCount = def.m_triangleCount;
        m_triangles = static_cast<ysTriangle*>(ysMalloc(sizeof(ysTriangle) * m_triangleCount));
    }

    {
        m_materialCount = def.m_materialStandardCount + def.m_materialMirrorCount;
        m_materials = static_cast<ysMaterial*>(ysMalloc(sizeof(ysMaterial) * m_materialCount));

        m_materialStandardCount = def.m_materialStandardCount;
        m_materialStandards = static_cast<ysMaterialStandard*>(ysMalloc(sizeof(ysMaterialStandard) * m_materialStandardCount));

        m_materialMirrorCount = def.m_materialMirrorCount;
        m_materialMirrors = static_cast<ysMaterialMirror*>(ysMalloc(sizeof(ysMaterialMirror) * m_materialMirrorCount));
    }

    {
        m_emissiveMaterialCount = def.m_emissiveMaterialUniformCount;
        m_emissiveMaterials = static_cast<ysEmissiveMaterial*>(ysMalloc(sizeof(ysEmissiveMaterial) * m_emissiveMaterialCount));

        m_emissiveMaterialUniformCount = def.m_emissiveMaterialUniformCount;
        m_emissiveMaterialUniforms = static_cast<ysEmissiveMaterialUniform*>(ysMalloc(sizeof(ysEmissiveMaterialUniform) * m_emissiveMaterialUniformCount));
    }

    {
        m_lightCount = def.m_lightPointCount;
        m_lights = static_cast<ysLight*>(ysMalloc(sizeof(ysLight) * m_lightCount));

        m_lightPointCount = def.m_lightPointCount;
        m_lightPoints = static_cast<ysLightPoint*>(ysMalloc(sizeof(ysLightPoint) * m_lightPointCount));
    }

    ys_int32 materialStandardStartIdx = 0;
    ys_int32 materialMirrorStartIdx = m_materialStandardCount;

    ys_int32 emissiveMaterialUniformStartIdx = 0;

    ////////////
    // Shapes //
    ////////////

    auto SetShapeMaterialIds = [&](ysShape* shape, const ysShapeDef* def)
    {
        switch (def->m_materialType)
        {
            case ysMaterialType::e_none:
                shape->m_materialId = ys_nullMaterialId;
                break;
            case ysMaterialType::e_standard:
                shape->m_materialId.m_index = materialStandardStartIdx + def->m_materialTypeIndex;
                break;
            case ysMaterialType::e_mirror:
                shape->m_materialId.m_index = materialMirrorStartIdx + def->m_materialTypeIndex;
                break;
            default:
                ysAssert(false);
                break;
        }

        switch (def->m_emissiveMaterialType)
        {
            case ysEmissiveMaterialType::e_none:
                shape->m_emissiveMaterialId = ys_nullEmissiveMaterialId;
                break;
            case ysEmissiveMaterialType::e_uniform:
                shape->m_emissiveMaterialId.m_index = emissiveMaterialUniformStartIdx + def->m_emissiveMaterialTypeIndex;
                break;
            default:
                ysAssert(false);
                break;
        }
    };

    ys_int32 shapeIdx = 0;

    ysAABB* aabbs = static_cast<ysAABB*>(ysMalloc(sizeof(ysAABB) * m_shapeCount));
    ysShapeId* shapeIds = static_cast<ysShapeId*>(ysMalloc(sizeof(ysShapeId) * m_shapeCount));

    for (ys_int32 i = 0; i < m_ellipsoidCount; ++i, ++shapeIdx)
    {
        ysEllipsoid* dst = m_ellipsoids + i;
        const ysEllipsoidDef& src = def.m_ellipsoids[i];
        dst->m_xf = src.m_transform;
        dst->m_s = src.m_radii;
        dst->m_sInv.x = ysSafeReciprocal(dst->m_s.x);
        dst->m_sInv.y = ysSafeReciprocal(dst->m_s.y);
        dst->m_sInv.z = ysSafeReciprocal(dst->m_s.z);

        ysShape* shape = m_shapes + shapeIdx;
        shape->m_type = ysShape::Type::e_ellipsoid;
        shape->m_typeIndex = i;

        SetShapeMaterialIds(shape, &src);

        aabbs[shapeIdx] = dst->ComputeAABB();
        shapeIds[shapeIdx].m_index = shapeIdx;
    }

    for (ys_int32 i = 0; i < m_triangleCount; ++i, ++shapeIdx)
    {
        ysTriangle* dst = m_triangles + i;
        const ysInputTriangle& src = def.m_triangles[i];
        dst->m_v[0] = src.m_vertices[0];
        dst->m_v[1] = src.m_vertices[1];
        dst->m_v[2] = src.m_vertices[2];
        ysVec4 ab = dst->m_v[1] - dst->m_v[0];
        ysVec4 ac = dst->m_v[2] - dst->m_v[0];
        ysVec4 ab_x_ac = ysCross(ab, ac);
        dst->m_n = ysIsSafeToNormalize3(ab_x_ac) ? ysNormalize3(ab_x_ac) : ysVec4_zero;
        dst->m_t = ysIsSafeToNormalize3(ab) ? ysNormalize3(ab) : ysVec4_zero; // TODO...
        dst->m_twoSided = src.m_twoSided;

        ysShape* shape = m_shapes + shapeIdx;
        shape->m_type = ysShape::Type::e_triangle;
        shape->m_typeIndex = i;
        
        SetShapeMaterialIds(shape, &src);

        aabbs[shapeIdx] = dst->ComputeAABB();
        shapeIds[shapeIdx].m_index = shapeIdx;
    }

    ysAssert(shapeIdx == m_shapeCount);

    m_bvh.Create(aabbs, shapeIds, m_shapeCount);
    ysFree(shapeIds);
    ysFree(aabbs);

    //////////////////////////
    // Reflective Materials //
    //////////////////////////
    {
        ys_int32 materialIdx = 0;

        for (ys_int32 i = 0; i < m_materialStandardCount; ++i, ++materialIdx)
        {
            ysMaterialStandard* dst = m_materialStandards + i;
            const ysMaterialStandardDef* src = def.m_materialStandards + i;
            dst->m_albedoDiffuse = ysMax(src->m_albedoDiffuse, ysVec4_zero);
            dst->m_albedoSpecular = ysMax(src->m_albedoSpecular, ysVec4_zero);
            dst->m_albedoDiffuse.w = 0.0f;
            dst->m_albedoSpecular.w = 0.0f;

            ysMaterial* material = m_materials + materialIdx;
            material->m_type = ysMaterial::Type::e_standard;
            material->m_typeIndex = i;
        }

        for (ys_int32 i = 0; i < m_materialMirrorCount; ++i, ++materialIdx)
        {
            ysMaterialMirror* dst = m_materialMirrors + i;
            const ysMaterialMirrorDef* src = def.m_materialMirrors + i;
            YS_REF(dst);
            YS_REF(src);

            ysMaterial* material = m_materials + materialIdx;
            material->m_type = ysMaterial::Type::e_mirror;
            material->m_typeIndex = i;
        }

        ysAssert(materialIdx == m_materialCount);
    }

    ////////////////////////
    // Emissive Materials //
    ////////////////////////
    {
        ys_int32 emissiveMaterialIdx = 0;

        for (ys_int32 i = 0; i < m_emissiveMaterialUniformCount; ++i, ++emissiveMaterialIdx)
        {
            ysEmissiveMaterialUniform* dst = m_emissiveMaterialUniforms + i;
            const ysEmissiveMaterialUniformDef* src = def.m_emissiveMaterialUniforms + i;
            dst->m_radiance = ysMax(src->m_radiance, ysVec4_zero);
            dst->m_radiance.w = 0.0f;

            ysEmissiveMaterial* emissiveMaterial = m_emissiveMaterials + emissiveMaterialIdx;
            emissiveMaterial->m_type = ysEmissiveMaterial::Type::e_uniform;
            emissiveMaterial->m_typeIndex = i;
        }

        ysAssert(emissiveMaterialIdx == m_emissiveMaterialCount);
    }

    ////////////
    // Lights //
    ////////////

    ys_int32 lightIdx = 0;

    ysVec4 invSphereRadians = ysSplat(0.25f / ys_pi);
    for (ys_int32 i = 0; i < m_lightPointCount; ++i, ++lightIdx)
    {
        ysLightPoint* dst = m_lightPoints + i;
        const ysLightPointDef* src = def.m_lightPoints + i;
        dst->m_position = src->m_position;
        dst->m_radiantIntensity = src->m_wattage * invSphereRadians;

        ysLight* light = m_lights + lightIdx;
        light->m_type = ysLight::Type::e_point;
        light->m_typeIndex = i;
    }

    ysAssert(lightIdx == m_lightCount);

    // Add emissive shapes to the special list so that we can randomly choose an area light to sample
    // TODO: Might also be good to have separate BVHs for reflective and emissive shapes.
    m_emissiveShapeCount = 0;
    for (ys_int32 i = 0; i < m_shapeCount; ++i)
    {
        const ysShape* shape = m_shapes + i;
        if (shape->m_emissiveMaterialId != ys_nullEmissiveMaterialId)
        {
            m_emissiveShapeCount++;
        }
    }
    m_emissiveShapeIndices = static_cast<ys_int32*>(ysMalloc(sizeof(ys_int32) * m_emissiveShapeCount));

    ys_int32 emissiveShapeIdx = 0;
    for (ys_int32 i = 0; i < m_shapeCount; ++i)
    {
        const ysShape* shape = m_shapes + i;
        if (shape->m_emissiveMaterialId != ys_nullEmissiveMaterialId)
        {
            m_emissiveShapeIndices[emissiveShapeIdx++] = i;
        }
    }

    const ys_int32 expectedMaxRenderConcurrency = 8;
    m_renders.Create(expectedMaxRenderConcurrency);

    ys_uint32 hardwareConcurrency = std::thread::hardware_concurrency();

    ysJobSystemDef jobSysDef;
    jobSysDef.m_workerCount = hardwareConcurrency - 1;
    m_jobSystem = ysJobSystem_Create(jobSysDef);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene::Destroy()
{
    ysAssert(m_renders.GetCount() == 0);

    m_bvh.Destroy();
    ysSafeFree(m_shapes);
    ysSafeFree(m_ellipsoids);
    ysSafeFree(m_triangles);
    ysSafeFree(m_materials);
    ysSafeFree(m_materialStandards);
    ysSafeFree(m_materialMirrors);
    ysSafeFree(m_emissiveMaterials);
    ysSafeFree(m_emissiveMaterialUniforms);
    ysSafeFree(m_lights);
    ysSafeFree(m_lightPoints);
    ysSafeFree(m_emissiveShapeIndices);
    m_shapeCount = 0;
    m_ellipsoidCount = 0;
    m_triangleCount = 0;
    m_materialCount = 0;
    m_materialStandardCount = 0;
    m_materialMirrorCount = 0;
    m_emissiveMaterialCount = 0;
    m_emissiveMaterialUniformCount = 0;
    m_lightCount = 0;
    m_lightPointCount = 0;
    m_emissiveShapeCount = 0;
    m_renders.Destroy();
    ysJobSystem_Destroy(m_jobSystem);
    m_jobSystem = nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysScene::ysSurfaceData
{
    void SetShape(const ysScene* scene, const ysShapeId& shapeId)
    {
        m_shape = scene->m_shapes + shapeId.m_index;
        m_material = (m_shape->m_materialId == ys_nullMaterialId)
            ? nullptr
            : scene->m_materials + m_shape->m_materialId.m_index;
        m_emissive = (m_shape->m_emissiveMaterialId == ys_nullEmissiveMaterialId)
            ? nullptr
            : scene->m_emissiveMaterials + m_shape->m_emissiveMaterialId.m_index;
    }

    const ysShape* m_shape;
    const ysMaterial* m_material;
    const ysEmissiveMaterial* m_emissive;
    ysVec4 m_posWS;
    ysVec4 m_normalWS;
    ysVec4 m_tangentWS;
    ysVec4 m_incomingDirectionWS;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysVec4 ysScene::SampleRadiance(const ysSurfaceData& surfaceData, ys_int32 bounceCount, ys_int32 maxBounceCount,
    bool sampleLight) const
{
    // Our labeling scheme is based on the path taken by photons: surface 0 --> surface 1 --> surface 2
    // In this context, surface 1 is the current one, and surface 0 is the new randomly sampled surface.
    const ysVec4& x1 = surfaceData.m_posWS;
    const ysVec4& n1 = surfaceData.m_normalWS;
    const ysVec4& t1 = surfaceData.m_tangentWS;
    const ysVec4& w12 = surfaceData.m_incomingDirectionWS;
    const ysShape* shape1 = surfaceData.m_shape;
    const ysMaterial* mat1 = surfaceData.m_material;

    ysMtx44 R1; // The frame at surface 1
    {
        R1.cx = t1;
        R1.cy = ysCross(n1, t1);
        R1.cz = n1;
    }
    ysVec4 w12_LS1 = ysMulT33(R1, w12); // direction 1->2 expressed in the frame of surface 1 (LS1: "in the local space of surface 1").

    ysVec4 emittedRadiance = ysVec4_zero;
    if (surfaceData.m_emissive != nullptr)
    {
        // In principle, sampling directionally-specular emission is inconceivable for unidirectional path tracing.
        ysRadiance emitL = surfaceData.m_emissive->EvaluateRadiance(this, w12_LS1);
        emittedRadiance = emitL.m_isFinite ? emitL.m_value : ysVec4_zero;
    }

    if (bounceCount == maxBounceCount)
    {
        return emittedRadiance;
    }

    // The space of directions to sample point lights is infinitesimal and therefore DISJOINT from the space of directions to sample
    // surfaces in general (including area lights, which are simpy emissive surfaces). Hence, we assign our point light samples the full
    // weight of 1 in the context of multiple importance sampling (MIS).
    ysVec4 pointLitRadiance = ysVec4_zero;
    for (ys_int32 i = 0; i < m_lightPointCount; ++i)
    {
        const ysLightPoint* light = m_lightPoints + i;
        ysVec4 v10 = light->m_position - x1;
        if (ysIsSafeToNormalize3(v10) == false)
        {
            continue;
        }

        ysVec4 w10 = ysNormalize3(v10);
        ys_float32 cos10_1 = ysDot3(w10, n1);
        if (cos10_1 <= 0.0f)
        {
            continue;
        }

        ysSceneRayCastInput srci;
        srci.m_maxLambda = 1.0f;
        srci.m_direction = v10;
        srci.m_origin = x1;

        ysSceneRayCastOutput srco;
        bool occluded = sRayCastClosestReflective(this, &srco, srci, shape1);
        if (occluded)
        {
            continue;
        }

        ys_float32 rr10 = ysLengthSqr3(v10);
        ysVec4 projIrradiance = light->m_radiantIntensity * ysSplat(cos10_1) / ysSplat(rr10);
        ysVec4 w10_LS1 = ysMulT33(R1, w10);
        ysBSDF brdf = surfaceData.m_material->EvaluateBRDF(this, w10_LS1, w12_LS1);
        // In principle, impossible for unidirectional path tracing to sample specular reflection of a point light.
        pointLitRadiance += brdf.m_isFinite ? projIrradiance * brdf.m_value : ysVec4_zero;
    }

    ysVec4 surfaceLitRadiance = ysVec4_zero;
    {
        // For MIS, we adopt the recommended approach in Chapter 8 of Veach's thesis: the standard balance heuristic with added power
        // heuristic (exponent = 2) to boost low variance strategies. We use a single sample per strategy, with strategies as follows:
        // - Pick a direction by sampling the hemisphere, ideally according to the BRDF. (This corresponds to a single strategy)
        // - Pick a direction by selecting point on the surface of each emissive shape that, barring occlusion by other scene
        //   geometry, is visible from the current point. (This corresponds to m_emissiveShapeCount strategies)
        // Each such direction could conceivably have been generated by another strategy. For instance, a given direction may point towards
        // multiple light sources. It is under these circumstances that MIS must be employed.

        // Sample the hemisphere (ideally according to the BRDF*cosine)
        {
            ysVec4 w10_LS1;
            ysBSDF f012;
            ysDirectionalProbabilityDensity p = mat1->GenerateRandomDirection(this, &w10_LS1, w12_LS1, &f012);
            ysVec4 w10 = ysMul33(R1, w10_LS1);
        
            ysSceneRayCastInput srci;
            srci.m_maxLambda = ys_maxFloat;
            srci.m_direction = w10;
            srci.m_origin = x1;
        
            SingleReflectiveMultipleEmissives srme;
            sRayCastClosestReflective_CollectEmissives(this, &srme, srci, shape1);
            if (srme.m_hitReflective || srme.m_emissiveCount > 0)
            {
                ysVec4 radiance = sAccumulateDirectRadiance(this, -w10, srme.m_emissiveOutputs, srme.m_emissiveCount);
                if (srme.m_hitReflective)
                {
                    const ysSceneRayCastOutput &opt = srme.m_reflectiveOutput;

                    ysSurfaceData surface0;
                    surface0.SetShape(this, opt.m_shapeId);
                    surface0.m_posWS = opt.m_hitPoint;
                    surface0.m_normalWS = opt.m_hitNormal;
                    surface0.m_tangentWS = opt.m_hitTangent;
                    surface0.m_incomingDirectionWS = -w10;

                    ysVec4 incomingRadiance = SampleRadiance(surface0, bounceCount + 1, maxBounceCount, sampleLight);
                    ysAssert(ysAllGE3(incomingRadiance, ysVec4_zero));
                    radiance += f012.m_value * incomingRadiance / ysSplat(p.m_perProjectedSolidAngle.m_value);
                }
        
                ys_float32 weight = 1.0f;
                if (sampleLight && p.m_perSolidAngle.m_isFinite)
                {
                    ys_float32 numerator = p.m_perSolidAngle.m_value * p.m_perSolidAngle.m_value;
                    ys_float32 denominator = numerator;
                    for (ys_int32 i = 0; i < m_emissiveShapeCount; ++i)
                    {
                        const ysShape* emissiveShape = m_shapes + m_emissiveShapeIndices[i];

                        ysRayCastInput rci;
                        rci.m_maxLambda = ys_maxFloat;
                        rci.m_direction = w10;
                        rci.m_origin = x1;

                        ysRayCastOutput rco;
                        bool samplingTechniquesOverlap = emissiveShape->RayCast(this, &rco, rci);
                        if (samplingTechniquesOverlap)
                        {
                            ys_float32 pAreaTmp = emissiveShape->ProbabilityDensityForGeneratedPoint(this, rco.m_hitPoint);
                            ys_float32 rr = rco.m_lambda * rco.m_lambda;
                            ys_float32 c = ysDot3(-w10, rco.m_hitNormal);
                            ysAssert(c >= 0.0f);
                            if (c > ys_zeroSafe)
                            {
                                ys_float32 pAngleTmp = pAreaTmp * rr / c;
                                denominator += pAngleTmp * pAngleTmp;
                            }
                        }
                    }
                    weight = numerator * ysSafeReciprocal(denominator);
                }
        
                surfaceLitRadiance += ysSplat(weight) * radiance;
            }
        }
        
        if (sampleLight)
        {
            // Sample each emissive shape
            for (ys_int32 i = 0; i < m_emissiveShapeCount; ++i)
            {
                // NOTE: The shape that the generated direction first intersects may actually be something other than this emissive shape.
                //       So it's a bit inconsistent to refer to this emissive shape as shape '0,' but we do so for convenience.
                const ysShape* shape0 = m_shapes + m_emissiveShapeIndices[i];
                if (shape0 == shape1)
                {
                    continue;
                }
                ysSurfacePoint frame0;
                ys_float32 pArea;
                shape0->GenerateRandomSurfacePoint(this, &frame0, &pArea);
                ysAssert(pArea > 0.0f);
                const ysVec4& x0 = frame0.m_point;
                const ysVec4& n0 = frame0.m_normal;
                ysVec4 v01 = x1 - x0;
                if (ysIsSafeToNormalize3(v01) == false)
                {
                    continue;
                }
                ys_float32 rr01 = ysLengthSqr3(v01);
                ysVec4 w01 = ysNormalize3(v01);
                ysVec4 v10 = -v01;
                ysVec4 w10 = -w01;
                ysVec4 w10_LS1 = ysMulT33(R1, w10);
                ys_float32 cos01_0 = ysDot3(w01, n0);
                if (cos01_0 < ys_zeroSafe)
                {
                    // The point is on a surface facing away
                    continue;
                }
                ys_float32 cos10_1 = ysDot3(w10, n1);
                if (cos10_1 <= 0.0f)
                {
                    continue;
                }
                // Convert probability density from 'per area' to 'per solid angle'
                ys_float32 pAngle = pArea * rr01 / cos01_0;
                if (pAngle < ys_epsilon)
                {
                    // Prevent division by zero
                    continue;
                }

                ysSceneRayCastInput srci;
                srci.m_maxLambda = ys_maxFloat;
                srci.m_direction = v10;
                srci.m_origin = x1;

                SingleReflectiveMultipleEmissives srme;
                sRayCastClosestReflective_CollectEmissives(this, &srme, srci, shape1);
                if (srme.m_hitReflective || srme.m_emissiveCount > 0)
                {
                    ysVec4 radiance = sAccumulateDirectRadiance(this, w01, srme.m_emissiveOutputs, srme.m_emissiveCount);
                
                    if (srme.m_hitReflective)
                    {
                        const ysSceneRayCastOutput &opt = srme.m_reflectiveOutput;

                        ysSurfaceData surface0;
                        surface0.SetShape(this, opt.m_shapeId);
                        surface0.m_posWS = opt.m_hitPoint;
                        surface0.m_normalWS = opt.m_hitNormal;
                        surface0.m_tangentWS = opt.m_hitTangent;
                        surface0.m_incomingDirectionWS = w01;

                        ysVec4 incomingRadiance = SampleRadiance(surface0, bounceCount + 1, maxBounceCount, sampleLight);
                        ysAssert(ysAllGE3(incomingRadiance, ysVec4_zero));
                        ysBSDF brdf = mat1->EvaluateBRDF(this, w10_LS1, w12_LS1);
                        if (brdf.m_isFinite == false)
                        {
                            // In principle, impossible for unidirectional path tracing to sample specular reflection of a point on an area light.
                            continue;
                        }
                        radiance += brdf.m_value * incomingRadiance * ysSplat(cos10_1 / pAngle);
                    }

                    ys_float32 weight;
                    {
                        ys_float32 numerator = pAngle * pAngle;
                        ys_float32 denominator = numerator;
                        for (ys_int32 j = 0; j < m_emissiveShapeCount; ++j)
                        {
                            if (j == i)
                            {
                                continue;
                            }
                            const ysShape* emissiveShape = m_shapes + m_emissiveShapeIndices[j];
                            if (emissiveShape == shape1)
                            {
                                continue;
                            }

                            ysRayCastInput rci;
                            rci.m_maxLambda = ys_maxFloat;
                            rci.m_direction = w10;
                            rci.m_origin = x1;

                            ysRayCastOutput rco;
                            bool samplingTechniquesOverlap = emissiveShape->RayCast(this, &rco, rci);
                            if (samplingTechniquesOverlap)
                            {
                                ys_float32 pAreaTmp = emissiveShape->ProbabilityDensityForGeneratedPoint(this, rco.m_hitPoint);
                                ys_float32 rr = rco.m_lambda * rco.m_lambda;
                                ys_float32 c = ysDot3(w01, rco.m_hitNormal);
                                ysAssert(c >= 0.0f);
                                if (c > ys_epsilon) // Prevent division by zero
                                {
                                    ys_float32 pAngleTmp = pAreaTmp * rr / c;
                                    denominator += pAngleTmp * pAngleTmp;
                                }
                            }
                        }

                        ysDirectionalProbabilityDensity p = mat1->ProbabilityDensityForGeneratedIncomingDirection(this, w10_LS1, w12_LS1);
                        if (p.m_perSolidAngle.m_isFinite)
                        {
                            // In principle, impossible for area light sampling to overlap with sampling singular directional distribution.
                            denominator += p.m_perSolidAngle.m_value * p.m_perSolidAngle.m_value;
                        }

                        weight = numerator / denominator;
                    }

                    surfaceLitRadiance += ysSplat(weight) * radiance;
                }
            }
        }
    }

    return emittedRadiance + pointLitRadiance + surfaceLitRadiance;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct PathVertex
{
    void SetShape(const ysScene* scene, const ysShapeId& shapeId)
    {
        m_shape = scene->m_shapes + shapeId.m_index;
        m_material = (m_shape->m_materialId == ys_nullMaterialId)
            ? nullptr
            : scene->m_materials + m_shape->m_materialId.m_index;
        m_emissive = (m_shape->m_emissiveMaterialId == ys_nullEmissiveMaterialId)
            ? nullptr
            : scene->m_emissiveMaterials + m_shape->m_emissiveMaterialId.m_index;
    }

    const ysShape* m_shape;
    const ysMaterial* m_material;
    const ysEmissiveMaterial* m_emissive;
    ysVec4 m_posWS;
    ysVec4 m_normalWS;
    ysVec4 m_tangentWS;

    // 0: probability per projected solid angle of generating the direction to backtrack this subpath.
    // 1: probability per projected solid angle of generating the direction to move forward on this subpath.
    // ... If not finite, the corresponding probability per projected solid angle is delta(w-w0), and m_p[i] is expected to 1.0
    ysDirectionalProbabilityDensity m_p[2];

    // conversion factor from per-projected-solid-angle to per-area probabilities. Corresponds to the m_p[1].
    // Note: The conversion factor for m_p[0] lives on the previous vertex.
    ys_float32 m_projToArea1;

    // BRDF for scattering at this point... or if this is the vertex on the light/sensor, the directional distribution of emitted radiance/
    // importance (directional distribution of radiance is the the radiance divided by the integral of radiance over projected solid angle)
    ysBSDF m_f;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct InputEyePathVertex
{
    void InitializePathVertex(PathVertex* pv) const
    {
        pv->m_shape = m_shape;
        pv->m_material = m_material;
        pv->m_emissive = m_emissive;
        pv->m_posWS = m_posWS;
        pv->m_normalWS = m_normalWS;
        pv->m_tangentWS = m_tangentWS;
    }

    const ysShape* m_shape;
    const ysMaterial* m_material;
    const ysEmissiveMaterial* m_emissive;
    ysVec4 m_posWS;
    ysVec4 m_normalWS;
    ysVec4 m_tangentWS;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysScene::GenerateSubpathInput
{
    ys_int32 minLightPathVertexCount;
    ys_int32 maxLightPathVertexCount;

    // We haven't yet formalized the notion of importance emitter
    // e.g. For a pinhole camera, each pixel's importance is W(x, w) = DiracDelta(x - x_pinhole) * W'(w) where W' is only nonzero over the
    //      solid angle subtended by the pixel.
    // Therefore, the bidirectional method cannot directly evaluate the integral over importance; that responsibility currently falls on the
    // caller of the method. More concretely, if the light path intersects the camera, we have no way to determine which pixel was hit.
    // Ultimately, as the camera model is unknown to the method, the caller must generate the t=0,1 eye path vertices and probabilities.
    // Note: Even if the camera model is known, unless the camera is included as part of the scene collision the caller will still have to
    //       generate the t=0 eye path vertex. The t=1 vertex is no longer essential, however.
    InputEyePathVertex eyePathVertex0;
    InputEyePathVertex eyePathVertex1;

    // The "important exitance" (i.e. importance summed over all projected solid angles at our chosen point on the sensor) divided by the
    // probability per area of selecting our chosen point on the sensor.
    ysVec4 WSpatialOverPSpatial0;
    // WDirectional: Directional distribution of importance from the point on the sensor (vertex 0) to the first point in the scene (vertex 1).
    // PDirectional: The probability per projected solid angle (relative to the sensor) for generating vertex 1 given vertex 0.
    ysVec4 WDirectionalOverPDirectional01;

    // Remarks:
    // - For a finite sensor, the two parameters above can each be broken down into numerator and denominator. However, we consume the
    //   precomputed ratio because for some camera models, while the ratio is well-defined, individually the numerator and denominator can
    //   be unbounded (delta functions).
    // - Let's consider the most common pinhole camera, which measures radiance in each pixel. The directional distribution of importance
    //   (i.e importance divided by the important exitance) in this case is the inverse pixel projected solid angle; hence, the numerator is
    //   just the unit-scaled delta function. The denominator, being a probability density, is also a unit-scaled delta function. So...
    //       WSpatialOverPSpatial0 = 1
    //   Computing WDirectionalOverPDirectional1 is a bit more nuanced; each pixel spans a finite range of directions and so unlike for the
    //   inifinitesimal pinhole, there are various ways to generate vertex1. It is still useful to consider a specific example, however.
    //   Suppose vertex1 is generated by treating the pixel as a finite rect some distance from the pinhole and randomly sampling points on
    //   this rect. If the dimensions of the pixel rect are small relative the distance from pinhole (usually a reasonable assumption), then
    //   the projected solid angle measure is constant across the pixel and PDirectional=1/wProj_pixel. As aforementtioned,
    //   WDirectional=1/wProj_pixel. So...
    //      WDirectionalOverPDirectional1 = 1

    ys_int32 minEyePathVertexCount;
    ys_int32 maxEyePathVertexCount;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysScene::GenerateSubpathOutput
{
    static const ys_int32 s_nLCeil = 16;
    static const ys_int32 s_nECeil = 16;
    PathVertex m_y[s_nLCeil]; // light-path vertices
    PathVertex m_z[s_nECeil]; //   eye-path vertices
    ys_int32 m_nL;
    ys_int32 m_nE;
    ys_float32 m_dPdA_L0;
    ys_float32 m_dPdA_W0;
    bool m_dPdA_L0_finite;
    bool m_dPdA_W0_finite;
    ysVec4 m_estimatorFactor_L0; // The emitted irradiance divided by the per-area-probability of generating vertex 0 of the light subpath
    ysVec4 m_estimatorFactor_W0; // ........... importance .................................................................  eye  .......

    // The index of the first vertex on each subpath that could possibly be culled by Russian Roulette; the first few vertices are typically
    // not considered for RR termination since they contribute the most to the image function. Note that the following indices may exceed
    // the actual number of vertices generated (m_nL/E) as the subpath may exit the scene or strike "Vantablack."
    ys_int32 m_russianRouletteBeginL;
    ys_int32 m_russianRouletteBeginE;
};

static const ys_float32 s_minRussianRouletteContinuationProbability = 0.001f;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static ysVec4 sDivide(const ysBSDF& f, const ysProbabilityDensity& p)
{
    ysAssert(f.m_isFinite || p.m_isFinite == false);
    ysAssert(p.m_value > 0.0f);
    return (f.m_isFinite == p.m_isFinite) ? f.m_value / ysSplat(p.m_value) : ysVec4_zero;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene::GenerateSubpaths(GenerateSubpathOutput* output, const GenerateSubpathInput& input) const
{
    const ys_float32 divZeroThresh = ys_epsilon; // Prevent unsafe division.

    PathVertex* y = output->m_y;
    PathVertex* z = output->m_z;

    const ys_int32 nLMin = input.minLightPathVertexCount;
    const ys_int32 nLMax = input.maxLightPathVertexCount;
    const ys_int32 nEMin = input.minEyePathVertexCount;
    const ys_int32 nEMax = input.maxEyePathVertexCount;
    ysAssert(nLMin <= nLMax && nEMin <= nEMax);
    ysAssert(nLMax <= GenerateSubpathOutput::s_nLCeil && nEMax <= GenerateSubpathOutput::s_nECeil);
    // Eye path must contain at least two vertices AND they must be pregenerated
    ysAssert(nLMin >= 0 && nEMin >= 2);

    ys_int32 nL;
    ys_int32 nE;

    // Probability to generate the first point on the Light. Don't forget to account for random light selection!
    ys_float32 probArea_L0 = -1.0f;
    bool probArea_L0_finite = false;
    ys_float32 probArea_W0 = 1.0f;      // TODO
    bool probArea_W0_finite = false;    // TODO

    ysVec4 LSpatialOverPSpatial0 = -ysVec4_zero;
    ysVec4 WSpatialOverPSpatial0 = input.WSpatialOverPSpatial0;

    ////////////////////////////////
    // Generate the LIGHT subpath //
    ////////////////////////////////
    nL = 0;
    while (true) // This while loop is just a convenient trick to terminate the path due to things like Russian Roulette. We should never actually reenter.
    {
        ysAssert(nL == 0); // No reentry
        if (nL >= nLMax)
        {
            break;
        }

        ysVec4 emittedIrradiance;
        {
            // Vertex 0: Pick a point on the light

            // TODO: Sample according to wattage
            ys_int32 emissiveShapeIdxIdx = rand() % m_emissiveShapeCount;
            ys_int32 emissiveShapeIdx = m_emissiveShapeIndices[emissiveShapeIdxIdx];
            const ysShape* emissiveShape = m_shapes + emissiveShapeIdx;
            ysSurfacePoint sp;
            emissiveShape->GenerateRandomSurfacePoint(this, &sp, &probArea_L0);
            probArea_L0 /= ys_float32(m_emissiveShapeCount); // Note this division!
            probArea_L0_finite = true; // TODO: Point lights

            const ysEmissiveMaterial* emissiveMaterial = m_emissiveMaterials + emissiveShape->m_emissiveMaterialId.m_index;
            emittedIrradiance = emissiveMaterial->EvaluateIrradiance(this).m_value;
            LSpatialOverPSpatial0 = emittedIrradiance / ysSplat(probArea_L0);

            y[nL].m_material = m_materials + emissiveShape->m_materialId.m_index;
            y[nL].m_emissive = emissiveMaterial;
            y[nL].m_shape = emissiveShape;
            y[nL].m_posWS = sp.m_point;
            y[nL].m_normalWS = sp.m_normal;
            y[nL].m_tangentWS = sp.m_tangent;
            y[nL].m_p[0].SetInvalid();
            y[nL].m_p[1].SetInvalid();
            y[nL].m_projToArea1 = -1.0f;
            y[nL].m_f.SetInvalid();

            nL++;
        }

        if (nL >= nLMax)
        {
            break;
        }

        {
            // Vertex 1: This is a bit different from Vertices 2...s. Namely, the usual BSDF term is the directional distribution of the emitted radiance.

            PathVertex* y1 = y + (nL - 1);
            PathVertex* y2 = y + (nL - 0);

            ysMtx44 R1; // The frame at vertex 1
            {
                R1.cx = y1->m_tangentWS;
                R1.cy = ysCross(y1->m_normalWS, y1->m_tangentWS);
                R1.cz = y1->m_normalWS;
            }

            ysVec4 u12_LS1;
            ysRadiance emittedRadiance;
            ysDirectionalProbabilityDensity p12 = y1->m_emissive->GenerateRandomDirection(this, &u12_LS1, &emittedRadiance);
            ysAssert(u12_LS1.z >= 0.0f);
            if (u12_LS1.z < divZeroThresh)
            {
                break;
            }
            ysVec4 u12 = ysMul33(R1, u12_LS1);

            ysBSDF Ldirectional;
            Ldirectional.m_value = emittedRadiance.m_value / emittedIrradiance;
            Ldirectional.m_isFinite = emittedRadiance.m_isFinite;

            if (nL >= nLMin)
            {
                // We've already generated the minimum number of vertices requested. Do Russian Roulette termination.
                ysVec4 fp = sDivide(Ldirectional, p12.m_perProjectedSolidAngle);
                ys_float32 q = ysClamp(ysMax(ysMax(fp.x, fp.y), fp.z), s_minRussianRouletteContinuationProbability, 1.0f);
                ys_float32 r = ysRandom(0.0f, 1.0f);
                bool absorbed = (r > q);
                if (absorbed)
                {
                    break;
                }
            }

            y1->m_f = Ldirectional;

            ysSceneRayCastInput srci;
            srci.m_maxLambda = ys_maxFloat;
            srci.m_direction = u12;
            srci.m_origin = y1->m_posWS;

            ysSceneRayCastOutput srco;
            bool hit = sRayCastClosestReflective(this, &srco, srci, y1->m_shape);
            if (hit == false)
            {
                break;
            }
            ysMtx44 R2; // The frame at vertex 2
            {
                R2.cx = srco.m_hitTangent;
                R2.cy = ysCross(srco.m_hitNormal, srco.m_hitTangent);
                R2.cz = srco.m_hitNormal;
            }
            ysVec4 u21_LS2 = ysMulT33(R2, -u12);
            ysAssert(u21_LS2.z > 0.0f);
            if (u21_LS2.z < divZeroThresh)
            {
                break;
            }
            ysAssert(srco.m_lambda >= 0.0f);
            ys_float32 d12Sqr = srco.m_lambda * srco.m_lambda;
            if (d12Sqr < divZeroThresh)
            {
                break;
            }
            y1->m_p[1] = p12;
            y1->m_projToArea1 = u12_LS1.z * u21_LS2.z / d12Sqr;

            y2->SetShape(this, srco.m_shapeId);
            y2->m_posWS = srco.m_hitPoint;
            y2->m_normalWS = srco.m_hitNormal;
            y2->m_tangentWS = srco.m_hitTangent;
            // Set some garbage values. They will be fixed when the next vertex is generated or when we join the light and eye paths
            y2->m_p[0].SetInvalid();
            y2->m_p[1].SetInvalid();
            y2->m_projToArea1 = -1.0f;
            y2->m_f.SetInvalid();

            nL++;
        }

        while (nL < nLMax)
        {
            const PathVertex* y0 = y + (nL - 2);
            PathVertex* y1 = y + (nL - 1);
            PathVertex* y2 = y + (nL - 0); // This is the potential new vertex

            ysMtx44 R1; // The frame at vertex 1
            {
                R1.cx = y1->m_tangentWS;
                R1.cy = ysCross(y1->m_normalWS, y1->m_tangentWS);
                R1.cz = y1->m_normalWS;
            }
            ysVec4 v10 = y0->m_posWS - y1->m_posWS;
            ysVec4 v10_LS1 = ysMulT33(R1, v10);
            ysVec4 u10_LS1 = ysNormalize3(v10_LS1);
            ysAssert(u10_LS1.z > divZeroThresh * 0.999f); // This should have been checked when generating the previous vertex

            ysVec4 u12_LS1;
            ysBSDF f012;
            ysDirectionalProbabilityDensity p210;
            ysDirectionalProbabilityDensity p012 = y1->m_material->GenerateRandomDirection(this, u10_LS1, &u12_LS1, &f012, &p210);
            ysAssert(u12_LS1.z >= 0.0f);
            if (u12_LS1.z < divZeroThresh)
            {
                break;
            }
            ysVec4 u12 = ysMul33(R1, u12_LS1);

            if (nL >= nLMin)
            {
                ysVec4 fp = sDivide(f012, p012.m_perProjectedSolidAngle);
                ys_float32 q = ysClamp(ysMax(ysMax(fp.x, fp.y), fp.z), s_minRussianRouletteContinuationProbability, 1.0f);
                ys_float32 r = ysRandom(0.0f, 1.0f);
                bool absorbed = (r > q);
                if (absorbed)
                {
                    break;
                }
            }

            ysSceneRayCastInput srci;
            srci.m_maxLambda = ys_maxFloat;
            srci.m_direction = u12;
            srci.m_origin = y1->m_posWS;

            ysSceneRayCastOutput srco;
            bool hit = sRayCastClosestReflective(this, &srco, srci, y1->m_shape);
            if (hit == false)
            {
                break;
            }

            ysMtx44 R2; // The frame at vertex 2
            {
                R2.cx = srco.m_hitTangent;
                R2.cy = ysCross(srco.m_hitNormal, srco.m_hitTangent);
                R2.cz = srco.m_hitNormal;
            }
            ysVec4 u21_LS2 = ysMulT33(R2, -u12);
            ysAssert(u21_LS2.z > 0.0f);
            if (u21_LS2.z < divZeroThresh)
            {
                break;
            }

            ysAssert(srco.m_lambda >= 0.0f);
            ys_float32 d12Sqr = srco.m_lambda * srco.m_lambda;
            if (d12Sqr < divZeroThresh)
            {
                break;
            }

            y1->m_p[0] = p210;
            y1->m_p[1] = p012;
            y1->m_projToArea1 = u12_LS1.z * u21_LS2.z / d12Sqr;
            y1->m_f = f012;

            y2->SetShape(this, srco.m_shapeId);
            y2->m_posWS = srco.m_hitPoint;
            y2->m_normalWS = srco.m_hitNormal;
            y2->m_tangentWS = srco.m_hitTangent;
            y2->m_p[0].SetInvalid();
            y2->m_p[1].SetInvalid();
            y2->m_projToArea1 = -1.0f;
            y2->m_f.SetInvalid();

            nL++;
        }

        break;
    }

    //////////////////////////////
    // Generate the EYE subpath //
    //////////////////////////////
    {
        input.eyePathVertex0.InitializePathVertex(z + 0);
        input.eyePathVertex1.InitializePathVertex(z + 1);
        z[0].m_p[0].SetInvalid();
        z[0].m_p[1].SetInvalid();
        z[0].m_p[1].m_perProjectedSolidAngle.m_value = 1.0f;
        z[0].m_p[1].m_perProjectedSolidAngle.m_isFinite = false;
        z[0].m_p[1].m_perSolidAngle.m_value = 1.0f; // Treat the sensor on the eye that is sensitive to this pixel as oriented directly towards the pixel.
        z[0].m_p[1].m_perSolidAngle.m_isFinite = false;
        z[1].m_p[0].SetInvalid();
        z[1].m_p[1].SetInvalid();
        z[1].m_projToArea1 = -1.0f;
        z[1].m_f.SetInvalid();

        ysVec4 v10 = z[0].m_posWS - z[1].m_posWS;
        ysVec4 u10 = ysNormalize3(v10);
        ys_float32 d10Sqr = ysLengthSqr3(v10);
        ys_float32 c = ysDot3(u10, z[1].m_normalWS);
        ysAssert(c >= 0.0f && d10Sqr > divZeroThresh);
        z[0].m_projToArea1 = c / d10Sqr;
        z[0].m_f.m_value = input.WDirectionalOverPDirectional01;
        z[0].m_f.m_isFinite = false;
    }
    nE = 2;
    while (nE < nEMax)
    {
        const PathVertex* z0 = z + (nE - 2);
        PathVertex* z1 = z + (nE - 1);
        PathVertex* z2 = z + (nE - 0);

        ysMtx44 R1;
        {
            R1.cx = z1->m_tangentWS;
            R1.cy = ysCross(z1->m_normalWS, z1->m_tangentWS);
            R1.cz = z1->m_normalWS;
        }
        ysVec4 v10 = z0->m_posWS - z1->m_posWS;
        ysVec4 v10_LS1 = ysMulT33(R1, v10);
        ysVec4 u10_LS1 = ysNormalize3(v10_LS1);
        ysAssert(u10_LS1.z > divZeroThresh * 0.999f);

        ysVec4 u12_LS1;
        ysBSDF f210;
        ysDirectionalProbabilityDensity p210;
        ysDirectionalProbabilityDensity p012 = z1->m_material->GenerateRandomDirection(this, &u12_LS1, u10_LS1, &f210, &p210);
        ysAssert(u12_LS1.z >= 0.0f);
        if (u12_LS1.z < divZeroThresh)
        {
            break;
        }
        ysVec4 u12 = ysMul33(R1, u12_LS1);

        if (nE >= nEMin)
        {
            ysVec4 fp = sDivide(f210, p012.m_perProjectedSolidAngle);
            ys_float32 q = ysClamp(ysMax(ysMax(fp.x, fp.y), fp.z), s_minRussianRouletteContinuationProbability, 1.0f);
            ys_float32 r = ysRandom(0.0f, 1.0f);
            bool absorbed = (r > q);
            if (absorbed)
            {
                break;
            }
        }

        ysSceneRayCastInput srci;
        srci.m_maxLambda = ys_maxFloat;
        srci.m_direction = u12;
        srci.m_origin = z1->m_posWS;

        ysSceneRayCastOutput srco;
        bool hit = sRayCastClosestReflective(this, &srco, srci, z1->m_shape);
        if (hit == false)
        {
            break;
        }

        ysMtx44 R2;
        {
            R2.cx = srco.m_hitTangent;
            R2.cy = ysCross(srco.m_hitNormal, srco.m_hitTangent);
            R2.cz = srco.m_hitNormal;
        }
        ysVec4 u21_LS2 = ysMulT33(R2, -u12);
        ysAssert(u21_LS2.z > 0.0f);
        if (u21_LS2.z < divZeroThresh)
        {
            break;
        }

        ysAssert(srco.m_lambda >= 0.0f);
        ys_float32 d12Sqr = srco.m_lambda * srco.m_lambda;
        if (d12Sqr < divZeroThresh)
        {
            break;
        }

        z1->m_p[0] = p210;
        z1->m_p[1] = p012;
        z1->m_projToArea1 = u12_LS1.z * u21_LS2.z / d12Sqr;
        z1->m_f = f210;

        z2->SetShape(this, srco.m_shapeId);
        z2->m_posWS = srco.m_hitPoint;
        z2->m_normalWS = srco.m_hitNormal;
        z2->m_tangentWS = srco.m_hitTangent;
        z2->m_p[0].SetInvalid();
        z2->m_p[1].SetInvalid();
        z2->m_projToArea1 = -1.0f;
        z2->m_f.SetInvalid();

        nE++;
    }

    output->m_nL = nL;
    output->m_nE = nE;
    output->m_dPdA_L0 = probArea_L0;
    output->m_dPdA_W0 = probArea_W0;
    output->m_dPdA_L0_finite = probArea_L0_finite;
    output->m_dPdA_W0_finite = probArea_W0_finite;
    output->m_estimatorFactor_L0 = LSpatialOverPSpatial0;
    output->m_estimatorFactor_W0 = WSpatialOverPSpatial0;
    output->m_russianRouletteBeginL = input.minLightPathVertexCount;
    output->m_russianRouletteBeginE = input.minEyePathVertexCount;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysVec4 ysScene::EvaluateTruncatedSubpaths(const GenerateSubpathOutput& subpaths, ys_int32 s, ys_int32 t) const
{
    const ys_float32 rayCastNudge = 0.001f; // Hack. Push ray cast origins out a little to avoid collision with the source shape.
    const ys_float32 divZeroThresh = ys_epsilon; // Prevent unsafe division.

    ysAssert(0 <= s && s <= subpaths.m_nL);
    ysAssert(0 <= t && t <= subpaths.m_nE);
    ysAssert(s + t >= 2);
    ysAssert(t >= 2); // Need this assert only because we don't model the camera as part of the scene
    PathVertex yz[GenerateSubpathOutput::s_nLCeil + GenerateSubpathOutput::s_nECeil];
    PathVertex* y = yz + 0;
    PathVertex* z = yz + s;
    ysMemCpy(y, subpaths.m_y, sizeof(PathVertex) * s);
    ysMemCpy(z, subpaths.m_z, sizeof(PathVertex) * t);

    const ysVec4& LSpatialOverPSpatial0 = subpaths.m_estimatorFactor_L0;
    const ysVec4& WSpatialOverPSpatial0 = subpaths.m_estimatorFactor_W0;
    ys_float32 probArea_L0 = subpaths.m_dPdA_L0;
    ys_float32 probArea_W0 = subpaths.m_dPdA_W0;
    bool probAreaFinite_L0 = subpaths.m_dPdA_L0_finite;
    bool probAreaFinite_W0 = subpaths.m_dPdA_W0_finite;

    // The emitted radiance and importance at vertex 0. These are only used if one of the subpaths has length 0.
    // If both subpaths are non-empty, then these are already captured by the emitted irradiance and the "pseudo-BSDF" at vertex 0.
    ysVec4 L01 = -ysVec4_one;
    ysVec4 W01 = -ysVec4_one;
    YS_REF(W01);

    /////////////////////////////////
    // Join light and eye subpaths //
    /////////////////////////////////
    ysAssert(s >= 0 && t >= 2);
    if (s > 0 && t > 0)
    {
        PathVertex* x1 = y + (s - 1); // The last vertex on the light path
        PathVertex* x2 = z + (t - 1); // The last vertex on the  eye  path
        ysVec4 v12 = x2->m_posWS - x1->m_posWS;
        ys_float32 d12Sqr = ysLengthSqr3(v12);
        if (d12Sqr < divZeroThresh)
        {
            return ysVec4_zero;
        }

        ysMtx44 R1;
        {
            R1.cx = x1->m_tangentWS;
            R1.cy = ysCross(x1->m_normalWS, x1->m_tangentWS);
            R1.cz = x1->m_normalWS;
        }

        ysMtx44 R2;
        {
            R2.cx = x2->m_tangentWS;
            R2.cy = ysCross(x2->m_normalWS, x2->m_tangentWS);
            R2.cz = x2->m_normalWS;
        }

        ysVec4 u12 = ysNormalize3(v12);
        ysVec4 u12_LS1 = ysMulT33(R1, u12);
        ysVec4 u21_LS2 = ysMulT33(R2, -u12);
        if (u12_LS1.z < divZeroThresh || u21_LS2.z < divZeroThresh)
        {
            return ysVec4_zero;
        }

        ysSceneRayCastInput srci;
        srci.m_origin = x1->m_posWS;
        srci.m_direction = v12;
        srci.m_maxLambda = 1.0f;

        ysSceneRayCastOutput srco;
        bool hit = sRayCastClosestReflective(this, &srco, srci, x1->m_shape);
        if (hit)
        {
            ysShape* hitShape = m_shapes + srco.m_shapeId.m_index;
            bool occluded = (hitShape != x2->m_shape);
            if (occluded)
            {
                return ysVec4_zero;
            }
        }

        ys_float32 g = u12_LS1.z * u21_LS2.z / d12Sqr;
        x1->m_projToArea1 = g;
        x2->m_projToArea1 = g;

        if (s == 1)
        {
            ysAssert(x1->m_emissive != nullptr);
            ysIrradiance LSpatial = x1->m_emissive->EvaluateIrradiance(this);
            ysRadiance L = x1->m_emissive->EvaluateRadiance(this, u12_LS1);
            x1->m_p[1] = x1->m_emissive->ProbabilityDensityForGeneratedDirection(this, u12_LS1);
            x1->m_f.m_value = L.m_value / LSpatial.m_value;
            x1->m_f.m_isFinite = L.m_isFinite;
        }
        else
        {
            ysAssert(s > 1);
            PathVertex* x0 = y + (s - 2);
            ysVec4 v10 = x0->m_posWS - x1->m_posWS;
            ysVec4 u10 = ysNormalize3(v10);
            ysVec4 u10_LS1 = ysMulT33(R1, u10);
            x1->m_p[0] = x1->m_material->ProbabilityDensityForGeneratedIncomingDirection(this, u10_LS1, u12_LS1);
            x1->m_p[1] = x1->m_material->ProbabilityDensityForGeneratedOutgoingDirection(this, u10_LS1, u12_LS1);
            x1->m_f = x1->m_material->EvaluateBRDF(this, u10_LS1, u12_LS1);
        }

        PathVertex* x3 = z + (t - 2);
        ysVec4 v23 = x3->m_posWS - x2->m_posWS;
        ysVec4 u23 = ysNormalize3(v23);
        ysVec4 u23_LS2 = ysMulT33(R2, u23);
        x2->m_p[0] = x2->m_material->ProbabilityDensityForGeneratedOutgoingDirection(this, u21_LS2, u23_LS2);
        x2->m_p[1] = x2->m_material->ProbabilityDensityForGeneratedIncomingDirection(this, u21_LS2, u23_LS2);
        x2->m_f = x2->m_material->EvaluateBRDF(this, u21_LS2, u23_LS2);

        if (x1->m_f.m_isFinite == false || x2->m_f.m_isFinite == false)
        {
            // Joining light and eye subpaths at specular vertices with non-vanishing BSDFs has probability zero. So even if it happens
            // numerically, pretend it ain't so. The theoretical impossibility is reflected by the fact that the estimator diverges if
            // either of the BSDFs at the junction is unbounded.
            return ysVec4_zero;
        }

        if (x1->m_p[1].m_perProjectedSolidAngle.m_isFinite == false ||
            x2->m_p[1].m_perProjectedSolidAngle.m_isFinite == false ||
            x1->m_p[1].m_perProjectedSolidAngle.m_value < ys_zeroSafe ||
            x2->m_p[1].m_perProjectedSolidAngle.m_value < ys_zeroSafe)
        {
            return ysVec4_zero;
        }
    }
    else if (t > 0)
    {
        ysAssert(t >= 2);

        PathVertex* x2 = z + (t - 1);
        if (x2->m_emissive == nullptr)
        {
            return ysVec4_zero;
        }
        const PathVertex* x3 = z + (t - 2);
        probArea_L0 = x2->m_shape->ProbabilityDensityForGeneratedPoint(this, x2->m_posWS);
        ysAssert(probArea_L0 > ys_zeroSafe);
        probArea_L0 /= (ys_float32)m_emissiveShapeCount;
        probAreaFinite_L0 = true;
        ysMtx44 R2;
        {
            R2.cx = x2->m_tangentWS;
            R2.cy = ysCross(x2->m_normalWS, x2->m_tangentWS);
            R2.cz = x2->m_normalWS;
        };
        ysVec4 v23 = x3->m_posWS - x2->m_posWS;
        ysVec4 u23 = ysNormalize3(v23);
        ysVec4 u23_LS2 = ysMulT33(R2, u23);
        x2->m_p[0] = x2->m_emissive->ProbabilityDensityForGeneratedDirection(this, u23_LS2);
        ysIrradiance LSpatial = x2->m_emissive->EvaluateIrradiance(this);
        ysRadiance L = x2->m_emissive->EvaluateRadiance(this, u23_LS2);
        if (L.m_isFinite == false)
        {
            // Sampling the directionally-specular emission has zero probability in principle.
            return ysVec4_zero;
        }
        L01 = L.m_value;
        x2->m_f.m_value = L.m_value / LSpatial.m_value;
        x2->m_f.m_isFinite = L.m_isFinite;
    }
    else
    {
        ysAssert(s >= 2);

        // This requires the camera to be finite and modeled as part of the scene
        ysAssert(false);
        return ysVec4_zero;
    }

#if 1
    ////////////////
    // Validation //
    ////////////////
    {
    //ysAssert(probAreaFinite_L0 || probArea_L0 == 1.0f); // TODO: Need to take into account random light selection
    ysAssert(probAreaFinite_W0 || probArea_W0 == 1.0f);

    if (s > 0)
    {
        // Note that after Russian Roulette is taken into account, the delta function may be squashed such that "m_value <= 1.0f"
        ysAssert(
            y[0].m_p[1].m_perProjectedSolidAngle.m_isFinite ||
            y[0].m_p[1].m_perProjectedSolidAngle.m_value == 1.0f);
    }

    if (t > 0)
    {
        ysAssert(
            z[0].m_p[1].m_perProjectedSolidAngle.m_isFinite ||
            z[0].m_p[1].m_perProjectedSolidAngle.m_value == 1.0f);
    }

    if (s > 0 && t > 0)
    {
        ysAssert(
            y[s - 1].m_p[1].m_perProjectedSolidAngle.m_isFinite ||
            y[s - 1].m_p[1].m_perProjectedSolidAngle.m_value == 1.0f);
        ysAssert(
            z[t - 1].m_p[1].m_perProjectedSolidAngle.m_isFinite ||
            z[t - 1].m_p[1].m_perProjectedSolidAngle.m_value == 1.0f);
    }

    for (ys_int32 i = 1; i < s - 1; ++i)
    {
        const ysDirectionalProbabilityDensity& p10 = y[i].m_p[0];
        const ysDirectionalProbabilityDensity& p01 = y[i].m_p[1];
        YS_REF(p10);
        YS_REF(p01);
        ysAssert(p10.m_perProjectedSolidAngle.m_isFinite || p10.m_perProjectedSolidAngle.m_value == 1.0f);
        ysAssert(p01.m_perProjectedSolidAngle.m_isFinite || p01.m_perProjectedSolidAngle.m_value == 1.0f);
    }

    for (ys_int32 i = 1; i < t - 1; ++i)
    {
        const ysDirectionalProbabilityDensity& p10 = z[i].m_p[0];
        const ysDirectionalProbabilityDensity& p01 = z[i].m_p[1];
        YS_REF(p10);
        YS_REF(p01);
        ysAssert(p10.m_perProjectedSolidAngle.m_isFinite || p10.m_perProjectedSolidAngle.m_value == 1.0f);
        ysAssert(p01.m_perProjectedSolidAngle.m_isFinite || p01.m_perProjectedSolidAngle.m_value == 1.0f);
    }
    }
#endif

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Bake Russian Roulette continuation probability into the edges                                           //
    // WARNING: The formula for termination probablity must match the one actually used for subpath generation //
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    {
        ys_int32 rrBeginL = ysMax(0, subpaths.m_russianRouletteBeginL - 1);
        ys_int32 rrBeginE = ysMax(0, subpaths.m_russianRouletteBeginE - 1);
    
        {
            ys_int32 idx = 0;
            for (ys_int32 i = 0; i < (t == 0 ? s - 1 : s); ++i, ++idx)
            {
                if (idx < rrBeginL)
                {
                    continue;
                }
                PathVertex* pv = y + i;
                ysVec4 fp = sDivide(pv->m_f, pv->m_p[1].m_perProjectedSolidAngle);
                ys_float32 q = ysClamp(ysMax(ysMax(fp.x, fp.y), fp.z), s_minRussianRouletteContinuationProbability, 1.0f);
                pv->m_p[1].m_perSolidAngle.m_value *= q;
                pv->m_p[1].m_perProjectedSolidAngle.m_value *= q;
            }
    
            for (ys_int32 i = t - 1; i > 0; --i, ++idx)
            {
                if (idx < rrBeginL)
                {
                    continue;
                }
                PathVertex* pv = z + i;
                ysVec4 fp = sDivide(pv->m_f, pv->m_p[0].m_perProjectedSolidAngle);
                ys_float32 q = ysClamp(ysMax(ysMax(fp.x, fp.y), fp.z), s_minRussianRouletteContinuationProbability, 1.0f);
                pv->m_p[0].m_perSolidAngle.m_value *= q;
                pv->m_p[0].m_perProjectedSolidAngle.m_value *= q;
            }
        }
    
        {
            ys_int32 idx = 0;
            for (ys_int32 i = 0; i < (s == 0 ? t - 1 : t); ++i, ++idx)
            {
                if (idx < rrBeginE)
                {
                    continue;
                }
                PathVertex* pv = z + i;
                ysVec4 fp = sDivide(pv->m_f, pv->m_p[1].m_perProjectedSolidAngle);
                ys_float32 q = ysClamp(ysMax(ysMax(fp.x, fp.y), fp.z), s_minRussianRouletteContinuationProbability, 1.0f);
                pv->m_p[1].m_perSolidAngle.m_value *= q;
                pv->m_p[1].m_perProjectedSolidAngle.m_value *= q;
            }
    
            for (ys_int32 i = s - 1; i > 0; --i, ++idx)
            {
                if (idx < rrBeginE)
                {
                    continue;
                }
                PathVertex* pv = y + i;
                ysVec4 fp = sDivide(pv->m_f, pv->m_p[0].m_perProjectedSolidAngle);
                ys_float32 q = ysClamp(ysMax(ysMax(fp.x, fp.y), fp.z), s_minRussianRouletteContinuationProbability, 1.0f);
                pv->m_p[0].m_perSolidAngle.m_value *= q;
                pv->m_p[0].m_perProjectedSolidAngle.m_value *= q;
            }
        }
    }

    ////////////////////////////////////
    // Compute (unweighted) estimator //
    ////////////////////////////////////
    ysVec4 estimator;
    {
        ysVec4 estimatorL = ysVec4_one;
        if (s > 0)
        {
            estimatorL *= LSpatialOverPSpatial0;
            for (ys_int32 i = 0; i < s - 1; ++i)
            {
                if (y[i].m_f.m_isFinite == y[i].m_p[1].m_perProjectedSolidAngle.m_isFinite)
                {
                    estimatorL *= y[i].m_f.m_value / ysSplat(y[i].m_p[1].m_perProjectedSolidAngle.m_value);
                }
                else
                {
                    // This edge was generated by sampling the BSDF (it is NOT the edge connecting the subpaths). Therefore, if the BSDF is
                    // specular, then the probability distribution with which we generated the in/out directions should also exibit
                    // singularities. Even if we somehow happen to sample the specular feature of the BSDF with poorly matched distribution,
                    // we cannot divide by zero here, so just return zero.
                    ysAssert(false);
                    return ysVec4_zero;
                }
            }
        }

        ysVec4 estimatorE = ysVec4_one;
        if (t > 0)
        {
            estimatorE *= WSpatialOverPSpatial0;
            for (ys_int32 i = 0; i < t - 1; ++i)
            {
                if (z[i].m_f.m_isFinite == z[i].m_p[1].m_perProjectedSolidAngle.m_isFinite)
                {
                    estimatorE *= z[i].m_f.m_value / ysSplat(z[i].m_p[1].m_perProjectedSolidAngle.m_value);
                }
                else
                {
                    ysAssert(false);
                    return ysVec4_zero;
                }
            }
        }

        ysVec4 estimatorJoin;
        ysAssert(s > 0 || t > 0);
        if (s == 0)
        {
            ysAssert(t >= 2);
            ysAssert(ysAllGE3(L01, ysVec4_zero));
            estimatorJoin = L01;
        }
        else if (t == 0)
        {
            ysAssert(s >= 2);
            ysAssert(ysAllGE3(W01, ysVec4_zero));
            estimatorJoin = W01;
        }
        else
        {
            ysAssert(s > 0 && t > 0);
            ysAssert(y[s - 1].m_projToArea1 == z[t - 1].m_projToArea1);
            ysAssert(y[s - 1].m_f.m_isFinite && z[t - 1].m_f.m_isFinite);
            ysVec4 g = ysSplat(y[s - 1].m_projToArea1);
            estimatorJoin = y[s - 1].m_f.m_value * g * z[t - 1].m_f.m_value;
        }

        estimator = estimatorL * estimatorJoin * estimatorE;
        ysAssert(ysAllGE3(estimator, ysVec4_zero));
    }

    //////////////////////////////
    // Compute estimator weight //
    //////////////////////////////
    ys_float32 weight;
    {
        // pA and pB are the per-area-probabilities of generating xA[0] and xB[0]
        auto ComputeSubpathWeightDenominator = [](ys_float32* denom, bool* denomIsFinite,
            const PathVertex* xA, ys_int32 nA, ys_float32 pA, bool pAIsFinite,
            const PathVertex* xB, ys_int32 nB, ys_float32 pB, bool pBIsFinite)
        {
            if (nA == 0)
            {
                *denom = 0.0f;
                *denomIsFinite = true;
                return;
            }

            if (nA == 1)
            {
                const PathVertex& xBLast = xB[nB - 1];
                if (xBLast.m_p[1].m_perProjectedSolidAngle.m_isFinite == pAIsFinite)
                {
                    ys_float32 pRatio = (xBLast.m_p[1].m_perProjectedSolidAngle.m_value * xBLast.m_projToArea1) / pA;
                    *denom = pRatio * pRatio; // Balance heuristic with exponent 2
                    *denomIsFinite = true;
                }
                else if (pAIsFinite)
                {
                    // To be precise, 'denom' should be the coefficient of the delta function.
                    // For our use case, it suffices to set a garbage value.
                    *denom = -1.0f;
                    *denomIsFinite = false;
                }
                else
                {
                    *denom = 0.0f;
                    *denomIsFinite = true;
                }
                return;
            }

            // Traverse the path backwards, accumulating (P[i]/P[nA-1])^2 from i=nA-2 to i=-1
            // ... where P[i] is the full path probability assuming xA[i] is the last vertex on subpath A.
            // P[i]/P[nA-1] = P[i]/P[i+1] * ... * P[nA-3]/P[nA-2] * P[nA-2]/P[nA-1]

            ys_float32 accum;

            ys_float32 pRatio;
            ys_int32 pRatioIsZeroCount;
            {
                const bool& p01Finite = xA[nA - 2].m_p[1].m_perProjectedSolidAngle.m_isFinite;
                const ys_float32& p01 = xA[nA - 2].m_p[1].m_perProjectedSolidAngle.m_value * xA[nA - 2].m_projToArea1;

                bool p21Finite;
                ys_float32 p21;
                if (nB > 0)
                {
                    p21Finite = xB[nB - 1].m_p[1].m_perProjectedSolidAngle.m_isFinite;
                    p21 = xB[nB - 1].m_p[1].m_perProjectedSolidAngle.m_value * xB[nB - 1].m_projToArea1;
                }
                else
                {
                    p21Finite = pBIsFinite;
                    p21 = pB;
                }

                if (p21Finite == false && p01Finite == true)
                {
                    *denom = -1.0f;
                    *denomIsFinite = false;
                    return;
                }

                pRatio = p21 / p01;
                if (p21Finite == p01Finite)
                {
                    pRatioIsZeroCount = 0;
                    accum = pRatio * pRatio;
                }
                else
                {
                    pRatioIsZeroCount = 1;
                    accum = 0.0f;
                }
            }

            for (ys_int32 i = nA - 2; i >= 1; --i)
            {
                const bool& p01Finite = xA[i - 1].m_p[1].m_perProjectedSolidAngle.m_isFinite;
                const bool& p21Finite = xA[i + 1].m_p[0].m_perProjectedSolidAngle.m_isFinite;

                if (p21Finite == false && p01Finite == true)
                {
                    pRatioIsZeroCount--;
                }
                else if (p21Finite == true && p01Finite == false)
                {
                    pRatioIsZeroCount++;
                }

                if (pRatioIsZeroCount < 0)
                {
                    *denom = -1.0f;
                    *denomIsFinite = false;
                    return;
                }

                // p01: The probability to generate xA[i-1]->xA[i] as part of subpath A
                // p21: The probability to generate xA[i+1]->xA[i] as part of subpath B
                ys_float32 p01 = xA[i - 1].m_p[1].m_perProjectedSolidAngle.m_value * xA[i - 1].m_projToArea1;
                ys_float32 p21 = xA[i + 1].m_p[0].m_perProjectedSolidAngle.m_value * xA[i].m_projToArea1;
                pRatio *= p21 / p01;
                accum += (pRatioIsZeroCount == 0) ? pRatio * pRatio : 0.0f;
            }

            {
                const bool& p01Finite = pAIsFinite;
                const bool& p21Finite = xA[1].m_p[0].m_perProjectedSolidAngle.m_isFinite;

                if (p21Finite == false && p01Finite == true)
                {
                    pRatioIsZeroCount--;
                }
                else if (p21Finite == true && p01Finite == false)
                {
                    pRatioIsZeroCount++;
                }

                if (pRatioIsZeroCount < 0)
                {
                    *denom = -1.0f;
                    *denomIsFinite = false;
                    return;
                }

                ys_float32 p01 = pA;
                ys_float32 p21 = xA[1].m_p[0].m_perProjectedSolidAngle.m_value * xA[0].m_projToArea1;
                pRatio *= p21 / p01;
                accum += (pRatioIsZeroCount == 0) ? pRatio * pRatio : 0.0f;
            }

            *denom = accum;
            *denomIsFinite = true;
        };

        ys_float32 weightDenomL;
        bool weightDenomIsFiniteL;
        ComputeSubpathWeightDenominator(&weightDenomL, &weightDenomIsFiniteL,
            y, s, probArea_L0, probAreaFinite_L0,
            z, t, probArea_W0, probAreaFinite_W0);
        if (weightDenomIsFiniteL == false)
        {
            return ysVec4_zero;
        }
        ysAssert(weightDenomL >= 0.0f);

        ys_float32 weightDenomE;
        bool weightDenomIsFiniteE;
        ComputeSubpathWeightDenominator(&weightDenomE, &weightDenomIsFiniteE,
            z, t, probArea_W0, probAreaFinite_W0,
            y, s, probArea_L0, probAreaFinite_L0);
        if (weightDenomIsFiniteE == false)
        {
            return ysVec4_zero;
        }
        ysAssert(weightDenomE >= 0.0f);

        ys_float32 weightInv = 1.0f + (weightDenomL + weightDenomE);
        weight = 1.0f / weightInv;
    }

    ysVec4 weightedEstimator = ysSplat(weight) * estimator;
    return weightedEstimator;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysVec4 ysScene::SampleRadiance_Bi(const GenerateSubpathInput& input) const
{
    GenerateSubpathOutput subpaths;
    GenerateSubpaths(&subpaths, input);

    if (subpaths.m_nL + subpaths.m_nE < 2)
    {
        return ysVec4_zero;
    }

    ysVec4 radiance = ysVec4_zero;
    for (ys_int32 s = 0; s <= subpaths.m_nL; ++s)
    {
        ys_int32 tBegin = ysMax(0, 2 - s);
        tBegin = 2; // Require at least two eye vertices for now
        for (ys_int32 t = tBegin; t <= subpaths.m_nE; ++t)
        {
            radiance += EvaluateTruncatedSubpaths(subpaths, s, t);
        }
    }

    return radiance;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysVec4 ysScene::RenderPixel(const ysSceneRenderInput& input, const ysVec4& pixelDirLS) const
{
    ysVec4 pixelDirWS = ysRotate(input.m_eye.q, pixelDirLS);

    ysSceneRayCastInput rci;
    rci.m_maxLambda = ys_maxFloat;
    rci.m_direction = pixelDirWS;
    rci.m_origin = input.m_eye.p;

    ysVec4 pixelValue = ysVec4_zero;
    switch (input.m_renderMode)
    {
        case ysSceneRenderInput::RenderMode::e_regular:
        {
            SingleReflectiveMultipleEmissives srme;
            sRayCastClosestReflective_CollectEmissives(this, &srme, rci, ys_nullShapeId);

            ysVec4 nPixelDirWS = ysNormalize3(pixelDirWS);

            ysVec4 radiance = sAccumulateDirectRadiance(this, -nPixelDirWS, srme.m_emissiveOutputs, srme.m_emissiveCount);

            if (srme.m_hitReflective)
            {
                const ysSceneRayCastOutput& mainOutput = srme.m_reflectiveOutput;

                const ysShape* shape = m_shapes + mainOutput.m_shapeId.m_index;
                const ysMaterial* material = m_materials + shape->m_materialId.m_index;
                const ysEmissiveMaterial* emissive = (shape->m_emissiveMaterialId == ys_nullEmissiveMaterialId)
                    ? nullptr
                    : m_emissiveMaterials + shape->m_emissiveMaterialId.m_index;

                ysAssert(input.m_giInput != nullptr);
                switch (input.m_giInput->m_type)
                {

                    case ysGlobalIlluminationInput::Type::e_uniDirectional:
                    {
                        const ysGlobalIlluminationInput_UniDirectional* giInput
                            = static_cast<const ysGlobalIlluminationInput_UniDirectional*>(input.m_giInput);

                        ysSurfaceData surfaceData;
                        surfaceData.m_shape = shape;
                        surfaceData.m_material = material;
                        surfaceData.m_emissive = emissive;
                        surfaceData.m_posWS = mainOutput.m_hitPoint;
                        surfaceData.m_normalWS = mainOutput.m_hitNormal;
                        surfaceData.m_tangentWS = mainOutput.m_hitTangent;
                        surfaceData.m_incomingDirectionWS = -nPixelDirWS;

                        // To be precise, what we should actually accumulate here is...
                        //     radiance += Irradiance / wproj_pixel = (SampleRadiance / (dP/dwproj)) / wproj_pixel
                        // ... where dP/dwproj is the probability per projected solid angle of sampling this point on the pixel
                        //           wproj_pixel is the projected solid angle subtended by the pixel as seen by the eye.
                        // Now for our specific implementation, we sample uniformly across the pixel's area such that dP/dwproj = wproj_pixel
                        // (here we have assumed that the pixel subtends an infinitesimal solid angle)
                        // Therefore, we merely need to accumulate radiance += SampleRadiance
                        radiance += SampleRadiance(surfaceData, 0, giInput->m_maxBounceCount, giInput->m_sampleLight);
                        break;
                    }
                    case ysGlobalIlluminationInput::Type::e_biDirectional:
                    {
                        const ysGlobalIlluminationInput_BiDirectional* giInput
                            = static_cast<const ysGlobalIlluminationInput_BiDirectional*>(input.m_giInput);

                        GenerateSubpathInput args;
                        args.minLightPathVertexCount = ysMin(1, giInput->m_maxLightSubpathVertexCount);
                        args.maxLightPathVertexCount = giInput->m_maxLightSubpathVertexCount;
                        args.minEyePathVertexCount = 2;
                        args.maxEyePathVertexCount = ysMax(2, giInput->m_maxEyeSubpathVertexCount);

                        args.eyePathVertex0.m_shape = nullptr;
                        args.eyePathVertex0.m_material = nullptr;
                        args.eyePathVertex0.m_posWS = input.m_eye.p;
                        args.eyePathVertex0.m_normalWS = ysVec4_zero;
                        args.eyePathVertex0.m_tangentWS = ysVec4_zero;

                        args.eyePathVertex1.m_shape = shape;
                        args.eyePathVertex1.m_material = material;
                        args.eyePathVertex1.m_emissive = emissive;
                        args.eyePathVertex1.m_posWS = mainOutput.m_hitPoint;
                        args.eyePathVertex1.m_normalWS = mainOutput.m_hitNormal;
                        args.eyePathVertex1.m_tangentWS = mainOutput.m_hitTangent;

                        args.WSpatialOverPSpatial0 = ysVec4_one;
                        args.WDirectionalOverPDirectional01 = ysVec4_one;

                        radiance += SampleRadiance_Bi(args);

                        break;
                    }
                    default:
                    {
                        ysAssert(false);
                        break;
                    }
                }
            }
            pixelValue = radiance;
            break;
        }
        case ysSceneRenderInput::RenderMode::e_compare:
        {
            // Comparing GI methods is a bit of an outlier since we allow different samples per pixel for the two methods being compared.
            // This function should be called with RenderMode::e_regular and the comparison performed by the caller.
            ysAssert(false);
            break;
        }
        case ysSceneRenderInput::RenderMode::e_normals:
        {
            ysSceneRayCastOutput rco;
            bool hit = m_bvh.RayCastClosest(this, &rco, rci);
            ysVec4 normalColor = hit ? (rco.m_hitNormal + ysVec4_one) * ysVec4_half : ysVec4_zero;
            pixelValue = normalColor;
            break;
        }
        case ysSceneRenderInput::RenderMode::e_depth:
        {
            ysSceneRayCastOutput rco;
            bool hit = m_bvh.RayCastClosest(this, &rco, rci);
            ys_float32 depth = hit ? rco.m_lambda * ysLength3(pixelDirWS) : -1.0f;
            pixelValue = ysSplat(depth);
            break;
        }
    }

    return pixelValue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SharedData
{
    const ysScene* scene;
    ysRender* target;
    ys_float32 samplesPerPixelInv;
    ys_float32 samplesPerPixelCompareInv;
    ys_float32 height;
    ys_float32 width;
    ys_float32 pixelHeight;
    ys_float32 pixelWidth;
};

struct PixelCoordinates
{
    ys_int32 i;
    ys_int32 j;
};

static void ASDF(PixelCoordinates& px, SharedData* sd)
{
    ys_int32 i = px.i;
    ys_int32 j = px.j;

    const ysScene* scene = sd->scene;
    ysRender* target = sd->target;
    const ysSceneRenderInput& input = target->m_input;
    ys_float32 samplesPerPixelInv = sd->samplesPerPixelInv;
    ys_float32 samplesPerPixelCompareInv = sd->samplesPerPixelCompareInv;
    ys_float32 height = sd->height;
    ys_float32 width = sd->width;
    ys_float32 pixelHeight = sd->pixelHeight;
    ys_float32 pixelWidth = sd->pixelWidth;

    ys_float32 yFraction = 1.0f - 2.0f * ys_float32(i + 1) / ys_float32(input.m_pixelCountY);
    ys_float32 yMid = height * yFraction;
    ys_float32 xFraction = 2.0f * ys_float32(j + 1) / ys_float32(input.m_pixelCountX) - 1.0f;
    ys_float32 xMid = width * xFraction;

    ys_int32 pixelIdx = input.m_pixelCountX * i + j;
    ysRender::Pixel* pixel = target->m_pixels + pixelIdx;

    if (input.m_renderMode == ysSceneRenderInput::RenderMode::e_compare)
    {
        ysSceneRenderInput tmpInput = input;
        tmpInput.m_renderMode = ysSceneRenderInput::RenderMode::e_regular;

        ysVec4 pixelValueA = ysVec4_zero;
        for (ys_int32 sampleIdx = 0; sampleIdx < tmpInput.m_samplesPerPixel; ++sampleIdx)
        {
            ys_float32 x = xMid + ysRandom(-pixelWidth, pixelWidth);
            ys_float32 y = yMid + ysRandom(-pixelHeight, pixelHeight);
            ysVec4 pixelDirLS = ysVecSet(x, y, -1.0f, 0.0f);
            ysVec4 deltaValue = scene->RenderPixel(tmpInput, pixelDirLS);
            pixelValueA += deltaValue;
        }
        pixelValueA *= ysSplat(samplesPerPixelInv);

        ysSwap(tmpInput.m_giInput, tmpInput.m_giInputCompare);
        ysSwap(tmpInput.m_samplesPerPixel, tmpInput.m_samplesPerPixelCompare);

        ysVec4 pixelValueB = ysVec4_zero;
        for (ys_int32 sampleIdx = 0; sampleIdx < tmpInput.m_samplesPerPixel; ++sampleIdx)
        {
            ys_float32 x = xMid + ysRandom(-pixelWidth, pixelWidth);
            ys_float32 y = yMid + ysRandom(-pixelHeight, pixelHeight);
            ysVec4 pixelDirLS = ysVecSet(x, y, -1.0f, 0.0f);
            ysVec4 deltaValue = scene->RenderPixel(tmpInput, pixelDirLS);
            pixelValueB += deltaValue;
        }
        pixelValueB *= ysSplat(samplesPerPixelCompareInv);

        pixel->m_value = (pixelValueB - pixelValueA) + ysVec4_half;
    }
    else
    {
        pixel->m_value = ysVec4_zero;
        for (ys_int32 sampleIdx = 0; sampleIdx < input.m_samplesPerPixel; ++sampleIdx)
        {
            ys_float32 x = xMid + ysRandom(-pixelWidth, pixelWidth);
            ys_float32 y = yMid + ysRandom(-pixelHeight, pixelHeight);
            ysVec4 pixelDirLS = ysVecSet(x, y, -1.0f, 0.0f);
            ysVec4 deltaValue = scene->RenderPixel(input, pixelDirLS);
            pixel->m_value += deltaValue;
        }
        pixel->m_value *= ysSplat(samplesPerPixelInv);
    }
    pixel->m_isNull = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene::DoRenderWork(ysRender* target) const
{
    const ysSceneRenderInput& input = target->m_input;
    if (input.m_samplesPerPixel <= 0)
    {
        return;
    }
    ys_float32 samplesPerPixelInv = 1.0f / (ys_float32)input.m_samplesPerPixel;

    ys_float32 samplesPerPixelCompareInv = 0.0f;
    if (input.m_renderMode == ysSceneRenderInput::RenderMode::e_compare && input.m_samplesPerPixelCompare <= 0)
    {
        return;
    }
    else
    {
        samplesPerPixelCompareInv = 1.0f / (ys_float32)input.m_samplesPerPixelCompare;
    }

    const ys_float32 aspectRatio = ys_float32(input.m_pixelCountX) / ys_float32(input.m_pixelCountY);
    // These are one-sided heights and widths
    const ys_float32 height = tanf(input.m_fovY);
    const ys_float32 width = height * aspectRatio;
    const ys_float32 pixelHeight = height / ys_float32(input.m_pixelCountY);
    const ys_float32 pixelWidth = width / ys_float32(input.m_pixelCountX);

    const ys_int32 k_pixelBatchCount = 256;
    static bool asdf = false;
    //if (m_jobSystem == nullptr)
    if (asdf)
    {
        ys_int32 pixelIdx0 = 0;
        ys_int32 pixelIdx = 0;
        for (ys_int32 i = 0; i < input.m_pixelCountY && target->m_state != ysRender::State::e_terminated; ++i)
        {
            ys_float32 yFraction = 1.0f - 2.0f * ys_float32(i + 1) / ys_float32(input.m_pixelCountY);
            ys_float32 yMid = height * yFraction;
            for (ys_int32 j = 0; j < input.m_pixelCountX && target->m_state != ysRender::State::e_terminated; ++j)
            {
                ys_float32 xFraction = 2.0f * ys_float32(j + 1) / ys_float32(input.m_pixelCountX) - 1.0f;
                ys_float32 xMid = width * xFraction;

                ysRender::Pixel* pixel = target->m_pixels + pixelIdx;

                if (input.m_renderMode == ysSceneRenderInput::RenderMode::e_compare)
                {
                    ysSceneRenderInput tmpInput = input;
                    tmpInput.m_renderMode = ysSceneRenderInput::RenderMode::e_regular;

                    ysVec4 pixelValueA = ysVec4_zero;
                    for (ys_int32 sampleIdx = 0; sampleIdx < tmpInput.m_samplesPerPixel; ++sampleIdx)
                    {
                        ys_float32 x = xMid + ysRandom(-pixelWidth, pixelWidth);
                        ys_float32 y = yMid + ysRandom(-pixelHeight, pixelHeight);
                        ysVec4 pixelDirLS = ysVecSet(x, y, -1.0f, 0.0f);
                        ysVec4 deltaValue = RenderPixel(tmpInput, pixelDirLS);
                        pixelValueA += deltaValue;
                    }
                    pixelValueA *= ysSplat(samplesPerPixelInv);

                    ysSwap(tmpInput.m_giInput, tmpInput.m_giInputCompare);
                    ysSwap(tmpInput.m_samplesPerPixel, tmpInput.m_samplesPerPixelCompare);

                    ysVec4 pixelValueB = ysVec4_zero;
                    for (ys_int32 sampleIdx = 0; sampleIdx < tmpInput.m_samplesPerPixel; ++sampleIdx)
                    {
                        ys_float32 x = xMid + ysRandom(-pixelWidth, pixelWidth);
                        ys_float32 y = yMid + ysRandom(-pixelHeight, pixelHeight);
                        ysVec4 pixelDirLS = ysVecSet(x, y, -1.0f, 0.0f);
                        ysVec4 deltaValue = RenderPixel(tmpInput, pixelDirLS);
                        pixelValueB += deltaValue;
                    }
                    pixelValueB *= ysSplat(samplesPerPixelCompareInv);

                    pixel->m_value = (pixelValueB - pixelValueA) + ysVec4_half;
                }
                else
                {
                    pixel->m_value = ysVec4_zero;
                    for (ys_int32 sampleIdx = 0; sampleIdx < input.m_samplesPerPixel; ++sampleIdx)
                    {
                        ys_float32 x = xMid + ysRandom(-pixelWidth, pixelWidth);
                        ys_float32 y = yMid + ysRandom(-pixelHeight, pixelHeight);
                        ysVec4 pixelDirLS = ysVecSet(x, y, -1.0f, 0.0f);
                        ysVec4 deltaValue = RenderPixel(input, pixelDirLS);
                        pixel->m_value += deltaValue;
                    }
                    pixel->m_value *= ysSplat(samplesPerPixelInv);
                }

                pixel->m_isNull = false;

                pixelIdx++;

                if (pixelIdx - pixelIdx0 == k_pixelBatchCount)
                {
                    ysScopedLock lock(&target->m_interruptLock);
                    for (ys_int32 k = pixelIdx0; k < pixelIdx; ++k)
                    {
                        target->m_exposedPixels[k] = target->m_pixels[k];
                    }
                    pixelIdx0 = pixelIdx;
                }
            }
        }

        if (pixelIdx - pixelIdx0 > 0)
        {
            ysScopedLock lock(&target->m_interruptLock);
            for (ys_int32 k = pixelIdx0; k < pixelIdx; ++k)
            {
                target->m_exposedPixels[k] = target->m_pixels[k];
            }
        }
    }
    else
    {
        SharedData sharedData;
        sharedData.scene = this;
        sharedData.target = target;
        sharedData.samplesPerPixelInv = samplesPerPixelInv;
        sharedData.samplesPerPixelCompareInv = samplesPerPixelCompareInv;
        sharedData.height = height;
        sharedData.width = width;
        sharedData.pixelHeight = pixelHeight;
        sharedData.pixelWidth = pixelWidth;
        PixelCoordinates pixelBatch[k_pixelBatchCount];

        ys_int32 n = 0;
        for (ys_int32 i = 0; i < input.m_pixelCountY && target->m_state != ysRender::State::e_terminated; ++i)
        {
            for (ys_int32 j = 0; j < input.m_pixelCountX && target->m_state != ysRender::State::e_terminated; ++j)
            {
                PixelCoordinates* dst = pixelBatch + n;
                dst->i = i;
                dst->j = j;
                n++;

                if (n == k_pixelBatchCount)
                {
                    ysParallelFor(m_jobSystem, pixelBatch, n, &sharedData, 2, ASDF);
                    n = 0;

                    ysScopedLock lock(&target->m_interruptLock);
                    for (ys_int32 k = 0; k < k_pixelBatchCount; ++k)
                    {
                        const PixelCoordinates& src = pixelBatch[k];
                        ys_int32 pixelIdx = input.m_pixelCountX * src.i + src.j;
                        target->m_exposedPixels[pixelIdx] = target->m_pixels[pixelIdx];
                    }
                }
            }
        }

        if (n > 0)
        {
            ysParallelFor(m_jobSystem, pixelBatch, n, &sharedData, 2, ASDF);

            ysScopedLock lock(&target->m_interruptLock);
            for (ys_int32 k = 0; k < k_pixelBatchCount; ++k)
            {
                const PixelCoordinates& src = pixelBatch[k];
                ys_int32 pixelIdx = input.m_pixelCountX * src.i + src.j;
                target->m_exposedPixels[pixelIdx] = target->m_pixels[pixelIdx];
            }
        }

    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene::Render(ysSceneRenderOutput* output, const ysSceneRenderInput& input) const
{
    ysRender render;
    render.Create(this, input);
    render.DoWork();
    render.GetOutputFinal(output);
    render.Destroy();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysVec4 ysScene::DebugRenderPixel(const ysSceneRenderInput& input, ys_float32 pixelX, ys_float32 pixelY) const
{
    if (input.m_samplesPerPixel <= 0)
    {
        return ysVec4_zero;
    }
    ys_float32 samplesPerPixelInv = 1.0f / (ys_float32)input.m_samplesPerPixel;

    ys_float32 samplesPerPixelCompareInv = 0.0f;
    if (input.m_renderMode == ysSceneRenderInput::RenderMode::e_compare && input.m_samplesPerPixelCompare <= 0)
    {
        return ysVec4_zero;
    }
    else
    {
        samplesPerPixelCompareInv = 1.0f / (ys_float32)input.m_samplesPerPixelCompare;
    }

    const ys_float32 aspectRatio = ys_float32(input.m_pixelCountX) / ys_float32(input.m_pixelCountY);
    const ys_float32 height = tanf(input.m_fovY);
    const ys_float32 width = height * aspectRatio;
    const ys_float32 pixelHeight = height / ys_float32(input.m_pixelCountY);
    const ys_float32 pixelWidth = width / ys_float32(input.m_pixelCountX);
    ys_float32 yFraction = 1.0f - 2.0f * (pixelY + 1.0f) / ys_float32(input.m_pixelCountY);
    ys_float32 xFraction = 2.0f * (pixelX + 1.0f) / ys_float32(input.m_pixelCountX) - 1.0f;
    ys_float32 yMid = height * yFraction;
    ys_float32 xMid = width * xFraction;

    if (input.m_renderMode == ysSceneRenderInput::RenderMode::e_compare)
    {
        ysSceneRenderInput tmpInput = input;
        tmpInput.m_renderMode = ysSceneRenderInput::RenderMode::e_regular;

        ysVec4 pixelValueA = ysVec4_zero;
        for (ys_int32 sampleIdx = 0; sampleIdx < tmpInput.m_samplesPerPixel; ++sampleIdx)
        {
            ys_float32 x = xMid + ysRandom(-pixelWidth, pixelWidth);
            ys_float32 y = yMid + ysRandom(-pixelHeight, pixelHeight);
            ysVec4 pixelDirLS = ysVecSet(x, y, -1.0f, 0.0f);
            ysVec4 deltaValue = RenderPixel(tmpInput, pixelDirLS);
            pixelValueA += deltaValue;
        }
        pixelValueA *= ysSplat(samplesPerPixelInv);

        ysSwap(tmpInput.m_giInput, tmpInput.m_giInputCompare);
        ysSwap(tmpInput.m_samplesPerPixel, tmpInput.m_samplesPerPixelCompare);

        ysVec4 pixelValueB = ysVec4_zero;
        for (ys_int32 sampleIdx = 0; sampleIdx < tmpInput.m_samplesPerPixel; ++sampleIdx)
        {
            ys_float32 x = xMid + ysRandom(-pixelWidth, pixelWidth);
            ys_float32 y = yMid + ysRandom(-pixelHeight, pixelHeight);
            ysVec4 pixelDirLS = ysVecSet(x, y, -1.0f, 0.0f);
            ysVec4 deltaValue = RenderPixel(tmpInput, pixelDirLS);
            pixelValueB += deltaValue;
        }
        pixelValueB *= ysSplat(samplesPerPixelCompareInv);

        ysVec4 diff = (pixelValueB - pixelValueA);
        ysVec4 avgInv;
        {
            ysVec4 avg = (pixelValueB - pixelValueA) * ysVec4_half;
            avgInv.x = ysSafeReciprocal(avg.x);
            avgInv.y = ysSafeReciprocal(avg.y);
            avgInv.z = ysSafeReciprocal(avg.z);
            avgInv.w = ysSafeReciprocal(avg.w);
        }
        ysVec4 pixelValue = (diff * avgInv) + ysVec4_half;
        return pixelValue;
    }
    else
    {
        ysVec4 pixelValue = ysVec4_zero;
        for (ys_int32 sampleIdx = 0; sampleIdx < input.m_samplesPerPixel; ++sampleIdx)
        {
            ys_float32 x = xMid + ysRandom(-pixelWidth, pixelWidth);
            ys_float32 y = yMid + ysRandom(-pixelHeight, pixelHeight);
            ysVec4 pixelDirLS = ysVecSet(x, y, -1.0f, 0.0f);
            ysVec4 deltaValue = RenderPixel(input, pixelDirLS);
            pixelValue += deltaValue;
        }
        pixelValue *= ysSplat(samplesPerPixelInv);
        return pixelValue;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene::DebugDrawGeo(const ysDrawInputGeo& input) const
{
    Color colors[1];
    colors[0] = Color(1.0f, 0.0f, 0.0f);
    for (ys_int32 i = 0; i < m_triangleCount; ++i)
    {
        const ysTriangle* triangle = m_triangles + i;
        input.debugDraw->DrawTriangleList(triangle->m_v, colors, 1);
        if (triangle->m_twoSided)
        {
            ysVec4 cba[3] = { triangle->m_v[2], triangle->m_v[1], triangle->m_v[0] };
            input.debugDraw->DrawTriangleList(cba, colors, 1);
        }
        ysVec4 segments[3][2];
        segments[0][0] = triangle->m_v[0];
        segments[0][1] = triangle->m_v[1];
        segments[1][0] = triangle->m_v[1];
        segments[1][1] = triangle->m_v[2];
        segments[2][0] = triangle->m_v[2];
        segments[2][1] = triangle->m_v[0];
        Color c[3];
        c[0] = Color(1.0f, 1.0f, 1.0f);
        c[1] = c[0];
        c[2] = c[1];
        input.debugDraw->DrawSegmentList(&segments[0][0], c, 3);
    }

    for (ys_int32 i = 0; i < m_ellipsoidCount; ++i)
    {
        const ysEllipsoid& ellipsoid = m_ellipsoids[i];
        input.debugDraw->DrawEllipsoid(ellipsoid.m_s, ellipsoid.m_xf, Color(0.0f, 1.0f, 0.0f));
        input.debugDraw->DrawWireEllipsoid(ellipsoid.m_s, ellipsoid.m_xf, Color(1.0f, 1.0f, 1.0f));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene::DebugDrawLights(const ysDrawInputLights& input) const
{
    Color wireFrameColor = Color(1.0f, 1.0f, 1.0f);
    for (ys_int32 i = 0; i < m_lightPointCount; ++i)
    {
        const ysLightPoint* light = m_lightPoints + i;
        ysTransform xf;
        xf.q = ysQuat_identity;
        xf.p = light->m_position;
        ysVec4 halfDims = ysSplat(0.25f);
        input.debugDraw->DrawWireEllipsoid(halfDims, xf, wireFrameColor);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysSceneId ysScene_Create(const ysSceneDef& def)
{
    ys_int32 freeSceneIdx = 0;
    while (freeSceneIdx < YOSHIPBR_MAX_SCENE_COUNT)
    {
        if (ysScene::s_scenes[freeSceneIdx] == nullptr)
        {
            break;
        }
        freeSceneIdx++;
    }

    if (freeSceneIdx == YOSHIPBR_MAX_SCENE_COUNT)
    {
        return ys_nullSceneId;
    }

    ysScene::s_scenes[freeSceneIdx] = static_cast<ysScene*>(ysMalloc(sizeof(ysScene)));
    ysScene::s_scenes[freeSceneIdx]->Create(def);

    ysSceneId id;
    id.m_index = freeSceneIdx;
    return id;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene_Destroy(ysSceneId id)
{
    ysAssert(ysScene::s_scenes[id.m_index] != nullptr);
    ysScene::s_scenes[id.m_index]->Destroy();
    ysFree(ysScene::s_scenes[id.m_index]);
    ysScene::s_scenes[id.m_index] = nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene_Render(ysSceneId id, ysSceneRenderOutput* output, const ysSceneRenderInput& input)
{
    ysAssert(ysScene::s_scenes[id.m_index] != nullptr);
    ysScene::s_scenes[id.m_index]->Render(output, input);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysVec4 ysScene_DebugRenderPixel(ysSceneId id, const ysSceneRenderInput& input, const ys_float32 pixelX, const ys_float32 pixelY)
{
    ysAssert(ysScene::s_scenes[id.m_index] != nullptr);
    return ysScene::s_scenes[id.m_index]->DebugRenderPixel(input, pixelX, pixelY);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysRenderId ysScene_CreateRender(ysSceneId id, const ysSceneRenderInput& input)
{
    ysAssert(ysScene::s_scenes[id.m_index] != nullptr);
    ysScene* scene = ysScene::s_scenes[id.m_index];

    ys_int32 renderIdx = scene->m_renders.Allocate();
    scene->m_renders[renderIdx].Create(scene, input);

    ysRenderId renderId;
    renderId.m_sceneIdx = id.m_index;
    renderId.m_index = renderIdx;
    return renderId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene_DestroyRender(ysRenderId id)
{
    ysAssert(ysScene::s_scenes[id.m_sceneIdx] != nullptr);
    ysScene* scene = ysScene::s_scenes[id.m_sceneIdx];

    ysAssert(scene->m_renders[id.m_index].m_poolIndex == id.m_index);
    ysRender& render = scene->m_renders[id.m_index];
    render.Terminate(scene);
    render.Destroy();
    scene->m_renders.Free(id.m_index);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ys_int32 ysScene_GetBVHDepth(ysSceneId id)
{
    ysAssert(ysScene::s_scenes[id.m_index] != nullptr);
    return ysScene::s_scenes[id.m_index]->m_bvh.m_depth;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene_DebugDrawBVH(ysSceneId id, const ysDrawInputBVH& input)
{
    ysAssert(ysScene::s_scenes[id.m_index] != nullptr);
    ysScene::s_scenes[id.m_index]->m_bvh.DebugDraw(input);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene_DebugDrawGeo(ysSceneId id, const ysDrawInputGeo& input)
{
    ysAssert(ysScene::s_scenes[id.m_index] != nullptr);
    ysScene::s_scenes[id.m_index]->DebugDrawGeo(input);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene_DebugDrawLights(ysSceneId id, const ysDrawInputLights& input)
{
    ysAssert(ysScene::s_scenes[id.m_index] != nullptr);
    ysScene::s_scenes[id.m_index]->DebugDrawLights(input);
}