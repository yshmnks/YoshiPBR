#include "YoshiPBR/ysScene.h"
#include "YoshiPBR/ysDebugDraw.h"
#include "light/ysLight.h"
#include "light/ysLightPoint.h"
#include "mat/ysMaterial.h"
#include "mat/ysMaterialStandard.h"
#include "YoshiPBR/ysRay.h"
#include "YoshiPBR/ysShape.h"
#include "YoshiPBR/ysStructures.h"
#include "YoshiPBR/ysTriangle.h"

ysScene* ysScene::s_scenes[YOSHIPBR_MAX_SCENE_COUNT] = { nullptr };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene::Reset()
{
    m_bvh.Reset();
    m_shapes = nullptr;
    m_triangles = nullptr;
    m_materials = nullptr;
    m_materialStandards = nullptr;
    m_lights = nullptr;
    m_lightPoints = nullptr;
    m_shapeCount = 0;
    m_triangleCount = 0;
    m_materialCount = 0;
    m_materialStandardCount = 0;
    m_lightCount = 0;
    m_lightPointCount = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene::Create(const ysSceneDef& def)
{
    m_shapeCount = def.m_triangleCount;
    m_shapes = static_cast<ysShape*>(ysMalloc(sizeof(ysShape) * m_shapeCount));

    m_triangleCount = def.m_triangleCount;
    m_triangles = static_cast<ysTriangle*>(ysMalloc(sizeof(ysTriangle) * m_triangleCount));

    m_materialCount = def.m_materialStandardCount;
    m_materials = static_cast<ysMaterial*>(ysMalloc(sizeof(ysMaterial) * m_materialCount));

    m_materialStandardCount = def.m_materialStandardCount;
    m_materialStandards = static_cast<ysMaterialStandard*>(ysMalloc(sizeof(ysMaterialStandard) * m_materialStandardCount));

    m_lightCount = def.m_lightPointCount;
    m_lights = static_cast<ysLight*>(ysMalloc(sizeof(ysLight) * m_lightCount));

    m_lightPointCount = def.m_lightPointCount;
    m_lightPoints = static_cast<ysLightPoint*>(ysMalloc(sizeof(ysLightPoint) * m_lightPointCount));

    ys_int32 materialStandardStartIdx = 0;

    ////////////
    // Shapes //
    ////////////

    ys_int32 shapeIdx = 0;

    ysAABB* aabbs = static_cast<ysAABB*>(ysMalloc(sizeof(ysAABB) * m_shapeCount));
    ysShapeId* shapeIds = static_cast<ysShapeId*>(ysMalloc(sizeof(ysShapeId) * m_shapeCount));

    for (ys_int32 i = 0; i < m_triangleCount; ++i, ++shapeIdx)
    {
        ysTriangle* dstTriangle = m_triangles + i;
        const ysInputTriangle* srcTriangle = def.m_triangles + i;
        dstTriangle->m_v[0] = srcTriangle->m_vertices[0];
        dstTriangle->m_v[1] = srcTriangle->m_vertices[1];
        dstTriangle->m_v[2] = srcTriangle->m_vertices[2];
        ysVec4 ab = dstTriangle->m_v[1] - dstTriangle->m_v[0];
        ysVec4 ac = dstTriangle->m_v[2] - dstTriangle->m_v[0];
        ysVec4 ab_x_ac = ysCross(ab, ac);
        dstTriangle->m_n = ysIsSafeToNormalize3(ab_x_ac) ? ysNormalize3(ab_x_ac) : ysVec4_zero;
        dstTriangle->m_t = ysIsSafeToNormalize3(ab) ? ysNormalize3(ab) : ysVec4_zero; // TODO...
        dstTriangle->m_twoSided = srcTriangle->m_twoSided;

        ysShape* shape = m_shapes + shapeIdx;
        shape->m_type = ysShape::Type::e_triangle;
        shape->m_typeIndex = i;

        switch (srcTriangle->m_materialType)            
        {
            case ysMaterialType::e_standard:
                shape->m_materialId.m_index = materialStandardStartIdx + srcTriangle->m_materialTypeIndex;
                break;
            default:
                ysAssert(false);
                break;
        }

        aabbs[shapeIdx] = dstTriangle->ComputeAABB();
        shapeIds[shapeIdx].m_index = shapeIdx;
    }

    ysAssert(shapeIdx == m_shapeCount);

    m_bvh.Create(aabbs, shapeIds, m_shapeCount);
    ysFree(shapeIds);
    ysFree(aabbs);

    ///////////////
    // Materials //
    ///////////////

    ys_int32 materialIdx = 0;

    for (ys_int32 i = 0; i < m_materialStandardCount; ++i, ++materialIdx)
    {
        ysMaterialStandard* dst = m_materialStandards + i;
        const ysMaterialStandardDef* src = def.m_materialStandards + i;
        dst->m_albedoDiffuse = src->m_albedoDiffuse;
        dst->m_albedoSpecular = src->m_albedoSpecular;
        dst->m_emissiveDiffuse = src->m_emissiveDiffuse;

        ysMaterial* material = m_materials + materialIdx;
        material->m_type = ysMaterial::Type::e_standard;
        material->m_typeIndex = i;
    }

    ysAssert(materialIdx == m_materialCount);

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
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene::Destroy()
{
    m_bvh.Destroy();
    ysFree(m_shapes);
    ysFree(m_triangles);
    ysFree(m_materials);
    ysFree(m_materialStandards);
    ysFree(m_lights);
    ysFree(m_lightPoints);
    m_shapeCount = 0;
    m_triangleCount = 0;
    m_materialCount = 0;
    m_materialStandardCount = 0;
    m_lightCount = 0;
    m_lightPointCount = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ysScene::RayCastClosest(ysSceneRayCastOutput* output, const ysSceneRayCastInput& input) const
{
    return m_bvh.RayCastClosest(this, output, input);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysScene::ysSurfaceData
{
    const ysShape* m_shape;
    const ysMaterial* m_material;
    ysVec4 m_posWS;
    ysVec4 m_normalWS;
    ysVec4 m_tangentWS;
    ysVec4 m_incomingDirectionWS;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysVec4 ysScene::SampleRadiance(const ysSurfaceData& surfaceData, ys_int32 bounceCount, ys_int32 maxBounceCount) const
{
    ysVec4 biTangentWS = ysCross(surfaceData.m_normalWS, surfaceData.m_tangentWS);

    ysMtx44 surfaceFrameWS;
    surfaceFrameWS.cx = surfaceData.m_tangentWS;
    surfaceFrameWS.cy = biTangentWS;
    surfaceFrameWS.cz = surfaceData.m_normalWS;

    ysVec4 incomingDirectionLS = ysMulT33(surfaceFrameWS, surfaceData.m_incomingDirectionWS);

    ysVec4 radiance = ysVec4_zero;

    for (ys_int32 i = 0; i < m_lightPointCount; ++i)
    {
        const ysLightPoint* light = m_lightPoints + i;
        ysVec4 surfaceToLightWS = light->m_position - surfaceData.m_posWS;
        ys_float32 rr = ysLengthSqr3(surfaceToLightWS);
        if (ysIsSafeToNormalize3(surfaceToLightWS) == false)
        {
            continue;
        }
        ysVec4 nSurfaceToLightWS = ysNormalize3(surfaceToLightWS);
        ys_float32 cosTheta = ysDot3(nSurfaceToLightWS, surfaceData.m_normalWS);
        if (cosTheta <= 0.0f)
        {
            continue;
        }

        ysSceneRayCastInput rci;
        rci.m_maxLambda = 1.0f;
        rci.m_direction = surfaceToLightWS;
        rci.m_origin = surfaceData.m_posWS + nSurfaceToLightWS * ysSplat(0.001f); // Hack. Push it out a little to collision with the source shape.

        ysSceneRayCastOutput rco;
        bool occluded = RayCastClosest(&rco, rci);
        if (occluded)
        {
            continue;
        }
        ysVec4 projIrradiance = light->m_radiantIntensity * ysSplat(cosTheta) / ysSplat(rr);
        ysVec4 nSurfaceToLightLS = ysMulT33(surfaceFrameWS, nSurfaceToLightWS);
        ysVec4 brdf = surfaceData.m_material->EvaluateBRDF(this, incomingDirectionLS, nSurfaceToLightLS);
        radiance = radiance + projIrradiance * brdf;
    }

    if (bounceCount == maxBounceCount)
    {
        return radiance;
    }

    ysVec4 indirectRadiance = ysVec4_zero;

    const ys_int32 sampleDirCount = 16;
    for (ys_int32 i = 0; i < sampleDirCount; ++i)
    {
        ysVec4 outgoingDirectionLS;
        ys_float32 probDens;
        surfaceData.m_material->GenerateRandomDirection(this, &outgoingDirectionLS, &probDens, incomingDirectionLS);
        ysAssert(probDens >= 0.0f);
        if (probDens < ys_epsilon)
        {
            continue;
        }
        ysVec4 outgoingDirectionWS = ysMul33(surfaceFrameWS, outgoingDirectionLS);

        ysSceneRayCastInput rci;
        rci.m_maxLambda = ys_maxFloat;
        rci.m_direction = outgoingDirectionWS;
        rci.m_origin = surfaceData.m_posWS + outgoingDirectionWS * ysSplat(0.001f); // Hack. Push it out a little to collision with the source shape.

        ysSceneRayCastOutput rco;
        bool hit = RayCastClosest(&rco, rci);
        if (hit == false)
        {
            continue;
        }

        ysVec4 brdf = surfaceData.m_material->EvaluateBRDF(this, incomingDirectionLS, outgoingDirectionLS);
        ysVec4 cosTheta = ysSplatDot3(outgoingDirectionWS, surfaceData.m_normalWS);
        ysAssert(ysAllGE3(brdf, ysVec4_zero));

        ysSurfaceData otherSurface;
        otherSurface.m_shape = m_shapes + rco.m_shapeId.m_index;
        otherSurface.m_material = m_materials + otherSurface.m_shape->m_materialId.m_index;
        otherSurface.m_posWS = rco.m_hitPoint;
        otherSurface.m_normalWS = rco.m_hitNormal;
        otherSurface.m_tangentWS = rco.m_hitTangent;
        otherSurface.m_incomingDirectionWS = -outgoingDirectionWS;

        ysVec4 irradiance = SampleRadiance(otherSurface, bounceCount + 1, maxBounceCount);
        ysAssert(ysAllGE3(irradiance, ysVec4_zero));
        indirectRadiance = indirectRadiance + brdf * irradiance * cosTheta / ysSplat(probDens);
    }

    ys_float32 invSampleCount = 1.0f / sampleDirCount;
    radiance = radiance + indirectRadiance * ysSplat(invSampleCount);
    return radiance;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene::Render(ysSceneRenderOutput* output, const ysSceneRenderInput& input) const
{
    output->m_pixels.SetCount(input.m_pixelCountX * input.m_pixelCountY);

    const ys_float32 aspectRatio = ys_float32(input.m_pixelCountX) / ys_float32(input.m_pixelCountY);
    const ys_float32 height = tanf(input.m_fovY);
    const ys_float32 width = height * aspectRatio;

    ys_int32 pixelIdx = 0;
    for (ys_int32 i = 0; i < input.m_pixelCountY; ++i)
    {
        ys_float32 yFraction = 1.0f - 2.0f * ys_float32(i + 1) / ys_float32(input.m_pixelCountY);
        ys_float32 y = height * yFraction;
        for (ys_int32 j = 0; j < input.m_pixelCountX; ++j)
        {
            ys_float32 xFraction = 2.0f * ys_float32(j + 1) / ys_float32(input.m_pixelCountX) - 1.0f;
            ys_float32 x = width * xFraction;

            ysVec4 pixelDirLS = ysVecSet(x, y, -1.0f, 0.0f);
            ysVec4 pixelDirWS = ysRotate(input.m_eye.q, pixelDirLS);

            ysSceneRayCastInput rci;
            rci.m_maxLambda = ys_maxFloat;
            rci.m_direction = pixelDirWS;
            rci.m_origin = input.m_eye.p;

            ysSceneRayCastOutput rco;
            bool hit = RayCastClosest(&rco, rci);

            switch (input.m_renderMode)
            {
                case ysSceneRenderInput::RenderMode::e_regular:
                {
                    ysVec4 radiance = ysVec4_zero;
                    if (hit)
                    {
                        ysSurfaceData surfaceData;
                        surfaceData.m_shape = m_shapes + rco.m_shapeId.m_index;
                        surfaceData.m_material = m_materials + surfaceData.m_shape->m_materialId.m_index;
                        surfaceData.m_posWS = rco.m_hitPoint;
                        surfaceData.m_normalWS = rco.m_hitNormal;
                        surfaceData.m_tangentWS = rco.m_hitTangent;
                        surfaceData.m_incomingDirectionWS = -pixelDirWS;
                        radiance = SampleRadiance(surfaceData, 0, input.m_maxBounceCount);
                    }
                    output->m_pixels[pixelIdx].r = radiance.x;
                    output->m_pixels[pixelIdx].g = radiance.y;
                    output->m_pixels[pixelIdx].b = radiance.z;
                    break;
                }
                case ysSceneRenderInput::RenderMode::e_normals:
                {
                    ysVec4 normalColor = hit ? (rco.m_hitNormal + ysVec4_one) * ysVec4_half : ysVec4_zero;
                    output->m_pixels[pixelIdx].r = normalColor.x;
                    output->m_pixels[pixelIdx].g = normalColor.y;
                    output->m_pixels[pixelIdx].b = normalColor.z;
                    break;
                }
                case ysSceneRenderInput::RenderMode::e_depth:
                {
                    ys_float32 depth = hit ? rco.m_lambda * ysLength3(pixelDirWS) : -1.0f;
                    output->m_pixels[pixelIdx].r = depth;
                    output->m_pixels[pixelIdx].g = depth;
                    output->m_pixels[pixelIdx].b = depth;
                    break;
                }
            }
            pixelIdx++;
        }
    }

    // Tone Mapping
    switch (input.m_renderMode)
    {
        case ysSceneRenderInput::RenderMode::e_regular:
        {
            break;
        }
        case ysSceneRenderInput::RenderMode::e_normals:
        {
            break;
        }
        case ysSceneRenderInput::RenderMode::e_depth:
        {
            ys_float32 minDepth = ys_maxFloat;
            ys_float32 maxDepth = 0.0f;
            for (ys_int32 i = 0; i < input.m_pixelCountX * input.m_pixelCountY; ++i)
            {
                ys_float32 depth = output->m_pixels[i].r;
                if (depth < 0.0f)
                {
                    continue;
                }
                minDepth = ysMin(minDepth, depth);
                maxDepth = ysMax(maxDepth, depth);
            }
            for (ys_int32 i = 0; i < input.m_pixelCountX * input.m_pixelCountY; ++i)
            {
                ys_float32 depth = output->m_pixels[i].r;
                if (depth < 0.0f)
                {
                    output->m_pixels[i].r = 1.0f;
                    output->m_pixels[i].g = 0.0f;
                    output->m_pixels[i].b = 0.0f;
                }
                else
                {
                    ys_float32 normalizedDepth = (depth - minDepth) / (maxDepth - minDepth);
                    output->m_pixels[i].r = normalizedDepth;
                    output->m_pixels[i].g = normalizedDepth;
                    output->m_pixels[i].b = normalizedDepth;
                }
            }
            break;
        }
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