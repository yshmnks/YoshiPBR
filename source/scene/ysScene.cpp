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

    ysVec4 emittedRadiance = mat1->EvaluateEmittedRadiance(this, w12, n1, t1);

    if (bounceCount == maxBounceCount)
    {
        return emittedRadiance;
    }

    ysVec4 b1 = ysCross(n1, t1); // bitangent

    ysMtx44 R1; // The frame at surface 1
    R1.cx = t1;
    R1.cy = b1;
    R1.cz = n1;

    ysVec4 w12_LS1 = ysMulT33(R1, w12); // direction 1->2 expressed in the frame of surface 1 (LS1: "in the local space of surface 1").

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
        srci.m_origin = x1 + w10 * ysSplat(0.001f); // Hack. Push it out a little to collision with the source shape.

        ysSceneRayCastOutput srco;
        bool occluded = RayCastClosest(&srco, srci);
        if (occluded)
        {
            continue;
        }

        ys_float32 rr10 = ysLengthSqr3(v10);
        ysVec4 projIrradiance = light->m_radiantIntensity * ysSplat(cos10_1) / ysSplat(rr10);
        ysVec4 w10_LS1 = ysMulT33(R1, w10);
        ysVec4 brdf = surfaceData.m_material->EvaluateBRDF(this, w10_LS1, w12_LS1);
        pointLitRadiance += projIrradiance * brdf;
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
            ys_float32 pAngle;
            mat1->GenerateRandomDirection(this, &w10_LS1, &pAngle, w12_LS1);
            ysAssert(pAngle >= 0.0f);
            ysVec4 w10 = ysMul33(R1, w10_LS1);
            if (pAngle >= ys_epsilon) // Prevent division by zero
            {
                ysSceneRayCastInput srci;
                srci.m_maxLambda = ys_maxFloat;
                srci.m_direction = w10;
                srci.m_origin = x1 + w10 * ysSplat(0.001f); // Hack. Push it out a little to collision with the source shape.

                ysSceneRayCastOutput srco;
                bool hit = RayCastClosest(&srco, srci);
                if (hit)
                {
                    ysSurfaceData surface0;
                    surface0.m_shape = m_shapes + srco.m_shapeId.m_index;
                    surface0.m_material = m_materials + surface0.m_shape->m_materialId.m_index;
                    surface0.m_posWS = srco.m_hitPoint;
                    surface0.m_normalWS = srco.m_hitNormal;
                    surface0.m_tangentWS = srco.m_hitTangent;
                    surface0.m_incomingDirectionWS = -w10;

                    ysVec4 irradiance = SampleRadiance(surface0, bounceCount + 1, maxBounceCount, sampleLight);
                    ysAssert(ysAllGE3(irradiance, ysVec4_zero));
                    ysVec4 brdf = mat1->EvaluateBRDF(this, w10_LS1, w12_LS1);
                    ysAssert(ysAllGE3(brdf, ysVec4_zero));
                    ys_float32 cos10_1 = ysDot3(w10, n1);
                    ysAssert(cos10_1 > 0.0f);
                    ysVec4 radiance = brdf * irradiance * ysSplat(cos10_1 / pAngle);

                    ys_float32 weight = 1.0f;
                    if (sampleLight)
                    {
                        ys_float32 numerator = pAngle * pAngle;
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
                                ys_float32 pAreaTmp = emissiveShape->ProbabilityDensityForGeneratedPoint(this, rco.m_hitPoint, x1);
                                ys_float32 rr = rco.m_lambda * rco.m_lambda;
                                ys_float32 c = ysDot3(-w10, rco.m_hitNormal);
                                ysAssert(c >= 0.0f);
                                if (c > ys_epsilon) // Prevent division by zero
                                {
                                    ys_float32 pAngleTmp = pAreaTmp * rr / c;
                                    denominator += pAngleTmp * pAngleTmp;
                                }
                            }
                        }
                        weight = numerator / denominator;
                    }

                    surfaceLitRadiance += ysSplat(weight) * radiance;
                }
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
                bool success = shape0->GenerateRandomVisibleSurfacePoint(this, &frame0, &pArea, x1);
                if (success == false)
                {
                    continue;
                }
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
                ysAssert(cos01_0 > 0.0f);
                if (cos01_0 < ys_epsilon)
                {
                    // Prevent division by zero
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
                srci.m_maxLambda = 1.0f;
                srci.m_direction = v10;
                srci.m_origin = x1 + w10 * ysSplat(0.001f); // Hack. Push it out a little to collision with the source shape.

                ysSceneRayCastOutput srco;
                bool hit = RayCastClosest(&srco, srci);

                ysSurfaceData surface0;
                if (hit)
                {
                    surface0.m_shape = m_shapes + srco.m_shapeId.m_index;
                    surface0.m_material = m_materials + surface0.m_shape->m_materialId.m_index;
                    surface0.m_posWS = srco.m_hitPoint;
                    surface0.m_normalWS = srco.m_hitNormal;
                    surface0.m_tangentWS = srco.m_hitTangent;
                    surface0.m_incomingDirectionWS = w01;
                }
                else
                {
                    // Numerically, the ray cast could miss... in principle, it cannot (recall that we generated the cast direction by
                    // sampling the light's surface). At the very least, the cast should hit the light.
                    surface0.m_shape = shape0;
                    surface0.m_material = m_materials + shape0->m_materialId.m_index;
                    surface0.m_posWS = x0;
                    surface0.m_normalWS = n0;
                    surface0.m_tangentWS = frame0.m_tangent;
                    surface0.m_incomingDirectionWS = w01;
                }

                ysVec4 irradiance = SampleRadiance(surface0, bounceCount + 1, maxBounceCount, sampleLight);
                ysAssert(ysAllGE3(irradiance, ysVec4_zero));
                ysVec4 brdf = mat1->EvaluateBRDF(this, w10_LS1, w12_LS1);
                ysAssert(ysAllGE3(brdf, ysVec4_zero));
                ysVec4 radiance = brdf * irradiance * ysSplat(cos10_1 / pAngle);

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
                            ys_float32 pAreaTmp = emissiveShape->ProbabilityDensityForGeneratedPoint(this, rco.m_hitPoint, x1);
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

                    ys_float32 pAngleTmp = mat1->ProbabilityDensityForGeneratedDirection(this, w10_LS1, w12_LS1);
                    denominator += pAngleTmp * pAngleTmp;

                    weight = numerator / denominator;
                }

                surfaceLitRadiance += ysSplat(weight) * radiance;
            }
        }
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

                                radiance += SampleRadiance(surfaceData, 0, input.m_maxBounceCount, input.m_sampleLight);
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