#include "ysScene.h"
#include "YoshiPBR/ysDebugDraw.h"
#include "light/ysLight.h"
#include "light/ysLightPoint.h"
#include "mat/ysMaterial.h"
#include "mat/ysMaterialStandard.h"
#include "YoshiPBR/ysLock.h"
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
    m_emissiveShapeIndices = nullptr;
    m_shapeCount = 0;
    m_triangleCount = 0;
    m_materialCount = 0;
    m_materialStandardCount = 0;
    m_lightCount = 0;
    m_lightPointCount = 0;
    m_emissiveShapeCount = 0;
    m_renders.Create();
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
        dst->m_albedoDiffuse = ysMax(src->m_albedoDiffuse, ysVec4_zero);
        dst->m_albedoSpecular = ysMax(src->m_albedoSpecular, ysVec4_zero);
        dst->m_emissiveDiffuse = ysMax(src->m_emissiveDiffuse, ysVec4_zero);
        dst->m_albedoDiffuse.w = 0.0f;
        dst->m_albedoSpecular.w = 0.0f;
        dst->m_emissiveDiffuse.w = 0.0f;

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

    // Add emissive shapes to the special BVH
    m_emissiveShapeCount = 0;
    for (ys_int32 i = 0; i < m_shapeCount; ++i)
    {
        const ysShape* shape = m_shapes + i;
        const ysMaterial* baseMaterial = m_materials + shape->m_materialId.m_index;
        if (baseMaterial->m_type == ysMaterial::Type::e_standard)
        {
            const ysMaterialStandard* material = m_materialStandards + baseMaterial->m_typeIndex;
            if (material->m_emissiveDiffuse == ysVec4_zero)
            {
                continue;
            }
            m_emissiveShapeCount++;
        }
    }
    m_emissiveShapeIndices = static_cast<ys_int32*>(ysMalloc(sizeof(ys_int32) * m_emissiveShapeCount));

    ys_int32 emissiveShapeIdx = 0;
    for (ys_int32 i = 0; i < m_shapeCount; ++i)
    {
        const ysShape* shape = m_shapes + i;
        const ysMaterial* baseMaterial = m_materials + shape->m_materialId.m_index;
        if (baseMaterial->m_type == ysMaterial::Type::e_standard)
        {
            const ysMaterialStandard* material = m_materialStandards + baseMaterial->m_typeIndex;
            if (material->m_emissiveDiffuse == ysVec4_zero)
            {
                continue;
            }
            m_emissiveShapeIndices[emissiveShapeIdx++] = i;
        }
    }

    const ys_int32 expectedMaxRenderConcurrency = 8;
    m_renders.Create(expectedMaxRenderConcurrency);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene::Destroy()
{
    ysAssert(m_renders.GetCount() == 0);

    m_bvh.Destroy();
    ysSafeFree(m_shapes);
    ysSafeFree(m_triangles);
    ysSafeFree(m_materials);
    ysSafeFree(m_materialStandards);
    ysSafeFree(m_lights);
    ysSafeFree(m_lightPoints);
    ysSafeFree(m_emissiveShapeIndices);
    m_shapeCount = 0;
    m_triangleCount = 0;
    m_materialCount = 0;
    m_materialStandardCount = 0;
    m_lightCount = 0;
    m_lightPointCount = 0;
    m_emissiveShapeCount = 0;
    m_renders.Destroy();
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
ysVec4 ysScene::SampleRadiance(const ysSurfaceData& surfaceData, ys_int32 bounceCount, ys_int32 maxBounceCount,
    bool sampleLight, bool sampleBRDF) const
{
    ysVec4 emittedRadiance = surfaceData.m_material->EvaluateEmittedRadiance
    (
        this, surfaceData.m_incomingDirectionWS, surfaceData.m_normalWS, surfaceData.m_tangentWS
    );

    if (bounceCount == maxBounceCount)
    {
        return emittedRadiance;
    }

    ysVec4 biTangentWS = ysCross(surfaceData.m_normalWS, surfaceData.m_tangentWS);

    ysMtx44 surfaceFrameWS;
    surfaceFrameWS.cx = surfaceData.m_tangentWS;
    surfaceFrameWS.cy = biTangentWS;
    surfaceFrameWS.cz = surfaceData.m_normalWS;

    ysVec4 incomingDirectionLS = ysMulT33(surfaceFrameWS, surfaceData.m_incomingDirectionWS);

    // The space of directions to sample point lights is infinitesimal and therefore DISJOINT from the space of directions to sample
    // surfaces in general (including area lights, which are simpy emissive surfaces). Hence, we assign our point light samples the full
    // weight of 1 in the context of multiple importance sampling (MIS); similarly, the result from surface sampling gets weight 1.
    ysVec4 pointLitRadiance = ysVec4_zero;
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
        pointLitRadiance += projIrradiance * brdf;
    }

    ysVec4 surfaceLitRadiance = ysVec4_zero;
    {
        // For MIS, we adopt the recommended approach in Chapter 8 of Veach's thesis: the standard balance heuristic with added power
        // heuristic (exponent = 2) to boost low variance strategies. We use a single sample per strategy, with strategies as follows:
        // - Pick a direction on the hemisphere at the current location (this corresponds to a single strategy).
        // - Pick a point on the surface of each emissive shape that, barring occlusion by other scene geometry, is visible from the current
        //   point. This corresponds to up to m_emissiveShapeCount strategies. As each shape is DISJOINT from the others, we assign them all
        //   weight 1 IF the aforementioned sample by direction (ignoring all occlusion) does not intersect any of these shapes... otherwise
        //   we employ the powered-balance-heuristic to weight the estimators between point/direction sampling.

        // Sample the hemisphere (ideally according to the BRDF*cosine)
        ysVec4 surfaceLitRadiance_dir = ysVec4_zero;

        ysVec4 outgoingDirectionWS;
        ys_float32 pAngle_dir;
        if (sampleBRDF)
        {
            ysVec4 outgoingDirectionLS;
            surfaceData.m_material->GenerateRandomDirection(this, &outgoingDirectionLS, &pAngle_dir, incomingDirectionLS);
            ysAssert(pAngle_dir >= 0.0f);
            outgoingDirectionWS = ysMul33(surfaceFrameWS, outgoingDirectionLS);
            if (pAngle_dir >= ys_epsilon) // Prevent division by zero
            {
                ysSceneRayCastInput rci;
                rci.m_maxLambda = ys_maxFloat;
                rci.m_direction = outgoingDirectionWS;
                rci.m_origin = surfaceData.m_posWS + outgoingDirectionWS * ysSplat(0.001f); // Hack. Push it out a little to collision with the source shape.

                ysSceneRayCastOutput rco;
                bool hit = RayCastClosest(&rco, rci);
                if (hit)
                {
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

                    ysVec4 irradiance = SampleRadiance(otherSurface, bounceCount + 1, maxBounceCount, sampleLight, sampleBRDF);
                    ysAssert(ysAllGE3(irradiance, ysVec4_zero));
                    surfaceLitRadiance_dir = brdf * irradiance * cosTheta / ysSplat(pAngle_dir);
                }
            }
        }

        bool dirPtSamplingAreDisjoint = true;
        ys_float32 weight_dir = 0.0f;

        const ysVec4& x_src = surfaceData.m_posWS;
        const ysVec4& n_src = surfaceData.m_normalWS;
        if (sampleLight)
        {
            for (ys_int32 i = 0; i < m_emissiveShapeCount; ++i)
            {
                // Sample each emissive shape

                const ys_int32& shapeIdx_src = m_emissiveShapeIndices[i];
                ysShape* shape_src = m_shapes + shapeIdx_src;
                ysSurfacePoint frame_src;
                ys_float32 pArea_pt;
                bool success = shape_src->GenerateRandomVisibleSurfacePoint(this, &frame_src, &pArea_pt, x_src);
                if (success == false)
                {
                    continue;
                }
                ysAssert(pArea_pt > 0.0f);
                const ysVec4& x_dst = frame_src.m_point;
                const ysVec4& n_dst = frame_src.m_normal;
                ysVec4 v_src_dst = x_dst - x_src;
                if (ysIsSafeToNormalize3(v_src_dst) == false)
                {
                    continue;
                }
                ys_float32 rr = ysLengthSqr3(v_src_dst);
                ysVec4 w_src_dst = ysNormalize3(v_src_dst);
                ysVec4 w_dst_src = -w_src_dst;
                ys_float32 cos_dst = ysDot3(w_dst_src, n_dst);
                ysAssert(cos_dst > 0.0f);
                if (cos_dst < ys_epsilon)
                {
                    // Prevent division by zero
                    continue;
                }
                // Convert probability density from 'per area' to 'per solid angle'
                ys_float32 pAngle_pt = pArea_pt * rr / cos_dst;
                if (pAngle_pt < ys_epsilon)
                {
                    // Prevent division by zero
                    continue;
                }

                ys_float32 weight_pt = 1.0f;
                if (sampleBRDF)
                {
                    ysRayCastInput rci;
                    rci.m_maxLambda = ys_maxFloat;
                    rci.m_direction = outgoingDirectionWS;
                    rci.m_origin = x_dst + outgoingDirectionWS * ysSplat(0.001f);

                    ysRayCastOutput rco;
                    bool samplingTechniquesOverlap = shape_src->RayCast(this, &rco, rci);
                    if (samplingTechniquesOverlap)
                    {
                        ysVec4 wLS_src_dst = ysMulT33(surfaceFrameWS, w_src_dst);
                        ys_float32 pAngleTmp_dir = surfaceData.m_material->ProbabilityDensityForGeneratedDirection(this, wLS_src_dst, incomingDirectionLS);
                        ys_float32 pAngleSqr_pt = pAngle_pt * pAngle_pt; // power heuristic with exponent 2
                        //weight_pt = pAngleSqr_pt / (pAngleSqr_pt + pAngleTmp_dir * pAngleTmp_dir);
                        weight_pt = pAngle_pt / (pAngle_pt + pAngleTmp_dir);

                        ys_float32 pAreaTmp_pt = shape_src->ProbabilityDensityForGeneratedPoint(this, rco.m_hitPoint, x_dst);
                        ysVec4 vTmp = x_dst - rco.m_hitPoint;
                        ysVec4 wTmp = ysNormalize3(vTmp);
                        ys_float32 cosTmp = ysDot3(wTmp, rco.m_hitNormal);
                        //ysAssert(cosTmp >= 0.0f);
                        if (cosTmp > ys_epsilon)
                        {
                            ys_float32 pAngleTmp_pt = pAreaTmp_pt * ysLengthSqr3(vTmp) / cosTmp;
                            ys_float32 pAngleSqr_dir = pAngle_dir * pAngle_dir;
                            //weight_dir += pAngleSqr_dir / (pAngleSqr_dir + pAngleTmp_pt * pAngleTmp_pt);
                            weight_dir += pAngle_dir / (pAngle_dir + pAngleTmp_pt);
                        }

                        dirPtSamplingAreDisjoint = false;
                    }
                }

                ys_float32 cos_src = ysDot3(w_src_dst, n_src);
                if (cos_src <= 0.0f)
                {
                    continue;
                }

                ysSceneRayCastInput srci;
                srci.m_maxLambda = 1.0f;
                srci.m_direction = v_src_dst;
                srci.m_origin = x_src + w_src_dst * ysSplat(0.001f); // Hack. Push it out a little to collision with the source shape.

                ysSceneRayCastOutput srco;
                bool occluded = RayCastClosest(&srco, srci);
                if (occluded && srco.m_shapeId.m_index != shapeIdx_src)
                {
                    continue;
                }
                ysVec4 wLS_src_dst = ysMulT33(surfaceFrameWS, w_src_dst);
                ysVec4 brdf = surfaceData.m_material->EvaluateBRDF(this, incomingDirectionLS, wLS_src_dst);
                ysAssert(ysAllGE3(brdf, ysVec4_zero));

                ysSurfaceData otherSurface;
                otherSurface.m_shape = shape_src;
                otherSurface.m_material = m_materials + shape_src->m_materialId.m_index;
                otherSurface.m_posWS = x_src;
                otherSurface.m_normalWS = n_src;
                otherSurface.m_tangentWS = frame_src.m_tangent;
                otherSurface.m_incomingDirectionWS = w_dst_src;

                ysVec4 irradiance = SampleRadiance(otherSurface, bounceCount + 1, maxBounceCount, sampleLight, sampleBRDF);
                ysAssert(ysAllGE3(irradiance, ysVec4_zero));
                surfaceLitRadiance += ysSplat(weight_pt) * (brdf * irradiance * ysSplat(cos_dst / pAngle_pt));
            }
        }

        if (dirPtSamplingAreDisjoint)
        {
            ysAssert(weight_dir == 0.0f);
            weight_dir = 1.0f;
        }
        surfaceLitRadiance += ysSplat(weight_dir) * surfaceLitRadiance_dir;
    }

    return emittedRadiance + pointLitRadiance + surfaceLitRadiance;
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

    bool sampleLight = input.m_sampleLight;
    bool sampleBRDF = input.m_sampleBRDF;
    if (sampleLight == false && sampleBRDF == false)
    {
        sampleLight = true;
        sampleBRDF = true;
    }

    const ys_float32 aspectRatio = ys_float32(input.m_pixelCountX) / ys_float32(input.m_pixelCountY);
    // These are one-sided heights and widths
    const ys_float32 height = tanf(input.m_fovY);
    const ys_float32 width = height * aspectRatio;
    const ys_float32 pixelHeight = height / ys_float32(input.m_pixelCountY);
    const ys_float32 pixelWidth = width / ys_float32(input.m_pixelCountX);

    ys_int32 pixelIdx = 0;
    for (ys_int32 i = 0; i < input.m_pixelCountY && target->m_state != ysRender::State::e_terminated; ++i)
    {
        ys_float32 yFraction = 1.0f - 2.0f * ys_float32(i + 1) / ys_float32(input.m_pixelCountY);
        ys_float32 yMid = height * yFraction;
        for (ys_int32 j = 0; j < input.m_pixelCountX && target->m_state != ysRender::State::e_terminated; ++j)
        {
            ys_float32 xFraction = 2.0f * ys_float32(j + 1) / ys_float32(input.m_pixelCountX) - 1.0f;
            ys_float32 xMid = width * xFraction;

            {
                ysScopedLock(&target->m_interruptLock);

                ysRender::Pixel* pixel = target->m_pixels + pixelIdx;
                pixel->m_value = ysVec4_zero;

                for (ys_int32 sampleIdx = 0; sampleIdx < input.m_samplesPerPixel; ++sampleIdx)
                {
                    ys_float32 x = xMid + ysRandom(-pixelWidth, pixelWidth);
                    ys_float32 y = yMid + ysRandom(-pixelHeight, pixelHeight);

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
                                const ysShape* shape = m_shapes + rco.m_shapeId.m_index;
                                const ysMaterial* material = m_materials + shape->m_materialId.m_index;

                                ysSurfaceData surfaceData;
                                surfaceData.m_shape = shape;
                                surfaceData.m_material = material;
                                surfaceData.m_posWS = rco.m_hitPoint;
                                surfaceData.m_normalWS = rco.m_hitNormal;
                                surfaceData.m_tangentWS = rco.m_hitTangent;
                                surfaceData.m_incomingDirectionWS = -pixelDirWS;

                                radiance += SampleRadiance(surfaceData, 0, input.m_maxBounceCount, sampleLight, sampleBRDF);
                            }
                            pixel->m_value += radiance;
                            break;
                        }
                        case ysSceneRenderInput::RenderMode::e_normals:
                        {
                            ysVec4 normalColor = hit ? (rco.m_hitNormal + ysVec4_one) * ysVec4_half : ysVec4_zero;
                            pixel->m_value += normalColor;
                            break;
                        }
                        case ysSceneRenderInput::RenderMode::e_depth:
                        {
                            ys_float32 depth = hit ? rco.m_lambda * ysLength3(pixelDirWS) : -1.0f;
                            pixel->m_value += ysSplat(depth);
                            break;
                        }
                    }
                }
                pixel->m_value *= ysSplat(samplesPerPixelInv);
                pixel->m_isNull = false;
            }

            pixelIdx++;
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
    render.Terminate();
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