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

    ysMtx44 R1; // The frame at surface 1
    {
        R1.cx = t1;
        R1.cy = ysCross(n1, t1);
        R1.cz = n1;
    }
    ysVec4 w12_LS1 = ysMulT33(R1, w12); // direction 1->2 expressed in the frame of surface 1 (LS1: "in the local space of surface 1").

    ysVec4 emittedRadiance = mat1->EvaluateEmittedRadiance(this, w12_LS1);

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
            mat1->GenerateRandomDirection(this, &w10_LS1, w12_LS1, &pAngle);
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
struct PathVertex
{
    const ysShape* m_shape;
    const ysMaterial* m_material;
    ysVec4 m_posWS;
    ysVec4 m_normalWS;
    ysVec4 m_tangentWS;

    // 0: probability per projected solid angle of generating the direction to backtrack this subpath.
    // 1: probability per projected solid angle of generating the direction to move forward on this subpath.
    // ... If not finite, the corresponding probability per projected solid angle is delta(w-w0), and m_probProj[i] is expected to 1.0
    ys_float32 m_probProj[2];
    bool m_probFinite[2];

    // conversion factor from per-projected-solid-angle to per-area probabilities. Corresponds to the m_probProj[1].
    // Note: The conversion factor for m_probProj[0] lives on the previous vertex.
    ys_float32 m_projToArea1;

    // BRDF for scattering at this point... or if this is the vertex on the light/sensor, the directional distribution of emitted radiance/
    // importance (directional distribution of radiance is the the radiance divided by the integral of radiance over projected solid angle)
    ysVec4 m_f;
    bool m_fFinite;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct InputEyePathVertex
{
    void InitializePathVertex(PathVertex* pv) const
    {
        pv->m_shape = m_shape;
        pv->m_material = m_material;
        pv->m_posWS = m_posWS;
        pv->m_normalWS = m_normalWS;
        pv->m_tangentWS = m_tangentWS;
    }

    const ysShape* m_shape;
    const ysMaterial* m_material;
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
    ysVec4 m_estimatorFactor_L0; // The emitted irradiance divided by the per-area-probability of generating vertex 0 of the light subpath
    ysVec4 m_estimatorFactor_W0; // ........... importance .................................................................  eye  .......
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene::GenerateSubpaths(GenerateSubpathOutput* output, const GenerateSubpathInput& input) const
{
    const ys_float32 rayCastNudge = 0.001f; // Hack. Push ray cast origins out a little to avoid collision with the source shape.
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
    ys_float32 probArea_L0;
    ys_float32 probArea_W0 = -1.0f; // TODO

    ysVec4 LSpatialOverPSpatial0;
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

            const ysMaterial* emissiveMaterial = m_materials + emissiveShape->m_materialId.m_index;
            emittedIrradiance = emissiveMaterial->EvaluateEmittedIrradiance(this);
            LSpatialOverPSpatial0 = emittedIrradiance / ysSplat(probArea_L0);

            y[nL].m_material = m_materials + emissiveShape->m_materialId.m_index;
            y[nL].m_shape = emissiveShape;
            y[nL].m_posWS = sp.m_point;
            y[nL].m_normalWS = sp.m_normal;
            y[nL].m_tangentWS = sp.m_tangent;
            y[nL].m_probProj[0] = -1.0f;
            y[nL].m_probProj[1] = -1.0f;
            y[nL].m_probFinite[0] = false;
            y[nL].m_probFinite[1] = false;
            y[nL].m_projToArea1 = -1.0f;
            y[nL].m_f = -ysVec4_one;
            y[nL].m_fFinite = false;

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
            ys_float32 probAngle12;
            y1->m_material->GenerateRandomEmission(this, &u12_LS1, &probAngle12);
            ysAssert(u12_LS1.z >= 0.0f);
            if (u12_LS1.z < divZeroThresh)
            {
                break;
            }
            ysVec4 u12 = ysMul33(R1, u12_LS1);
            ys_float32 probProj12 = probAngle12 / u12_LS1.z;

            ysVec4 emittedRadiance = y1->m_material->EvaluateEmittedRadiance(this, u12_LS1);
            ysVec4 Ldirectional = emittedRadiance / emittedIrradiance;

            ys_float32 q = 1.0f; // Russian Roulette probability that we will attempt to produce y2.
            if (nL >= nLMin)
            {
                // We've already generated the minimum number of vertices requested. Do Russian Roulette termination.
                ys_float32 f = ysMax(ysMax(Ldirectional.x, Ldirectional.y), Ldirectional.z);
                ys_float32 q = ysMin(1.0f, f / probProj12);
                ys_float32 r = ysRandom(0.0f, 1.0f);
                bool absorbed = (r > q);
                if (absorbed)
                {
                    break;
                }
            }

            y1->m_f = Ldirectional;
            y1->m_fFinite = true;

            ysSceneRayCastInput srci;
            srci.m_maxLambda = ys_maxFloat;
            srci.m_direction = u12;
            srci.m_origin = y1->m_posWS + u12 * ysSplat(rayCastNudge);

            ysSceneRayCastOutput srco;
            bool hit = RayCastClosest(&srco, srci);
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
            ysAssert(srco.m_lambda > 0.0f);
            ys_float32 d12 = srco.m_lambda + rayCastNudge;
            ys_float32 d12Sqr = d12 * d12;
            if (d12Sqr < divZeroThresh)
            {
                break;
            }
            y1->m_probProj[1] = probProj12 * q;
            y1->m_probFinite[1] = true;
            y1->m_projToArea1 = u12_LS1.z * u21_LS2.z / d12Sqr;

            y2->m_shape = m_shapes + srco.m_shapeId.m_index;
            y2->m_material = m_materials + y2->m_shape->m_materialId.m_index;
            y2->m_posWS = srco.m_hitPoint;
            y2->m_normalWS = srco.m_hitNormal;
            y2->m_tangentWS = srco.m_hitTangent;
            // Set some garbage values. They will be fixed when the next vertex is generated or when we join the light and eye paths
            y2->m_probProj[0] = -1.0f;
            y2->m_probProj[1] = -1.0f;
            y2->m_probFinite[0] = false;
            y2->m_probFinite[1] = false;
            y2->m_projToArea1 = -1.0f;
            y2->m_f = -ysVec4_one;
            y2->m_fFinite = false;

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
            ys_float32 probAngle012;
            y1->m_material->GenerateRandomDirection(this, u10_LS1, &u12_LS1, &probAngle012);
            ysAssert(u12_LS1.z >= 0.0f);
            if (u12_LS1.z < divZeroThresh)
            {
                break;
            }
            ysVec4 u12 = ysMul33(R1, u12_LS1);
            ys_float32 probProj012 = probAngle012 / u12_LS1.z;
            ysVec4 f012 = y1->m_material->EvaluateBRDF(this, u10_LS1, u12_LS1);

            ys_float32 q = 1.0f; // Russian Roulette probability that we will attempt to produce y2.
            if (nL >= nLMin)
            {
                // We've already generated the minimum number of vertices requested. Do Russian Roulette termination.
                ys_float32 f = ysMax(ysMax(f012.x, f012.y), f012.z);
                ys_float32 q = ysMin(1.0f, f / probProj012);
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
            srci.m_origin = y1->m_posWS + u12 * ysSplat(rayCastNudge);

            ysSceneRayCastOutput srco;
            bool hit = RayCastClosest(&srco, srci);
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

            ysAssert(srco.m_lambda > 0.0f);
            ys_float32 d12 = srco.m_lambda + rayCastNudge;
            ys_float32 d12Sqr = d12 * d12;
            if (d12Sqr < divZeroThresh)
            {
                break;
            }

            y1->m_probProj[0] = y1->m_material->ProbabilityDensityForGeneratedDirection(this, u10_LS1, u12_LS1) / u10_LS1.z;
            y1->m_probProj[1] = probProj012 * q;
            y1->m_probFinite[0] = true;
            y1->m_probFinite[1] = true;
            y1->m_projToArea1 = u12_LS1.z * u21_LS2.z / d12Sqr;
            y1->m_f = f012;
            y1->m_fFinite = true;

            y2->m_shape = m_shapes + srco.m_shapeId.m_index;
            y2->m_material = m_materials + y2->m_shape->m_materialId.m_index;
            y2->m_posWS = srco.m_hitPoint;
            y2->m_normalWS = srco.m_hitNormal;
            y2->m_tangentWS = srco.m_hitTangent;
            // Set some garbage values. They will be fixed when the next vertex is generated or when we join the light and eye paths
            y2->m_probProj[0] = -1.0f;
            y2->m_probProj[1] = -1.0f;
            y2->m_probFinite[0] = false;
            y2->m_probFinite[1] = false;
            y2->m_projToArea1 = -1.0f;
            y2->m_f = -ysVec4_one;
            y2->m_fFinite = false;

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
        z[0].m_probProj[0] = -1.0f;
        z[0].m_probProj[1] = 1.0f;
        z[0].m_probFinite[0] = false;
        z[0].m_probFinite[1] = false;
        z[1].m_probProj[0] = -1.0f;
        z[1].m_probProj[1] = -1.0f;
        z[1].m_probFinite[0] = false;
        z[1].m_probFinite[1] = false;
        z[1].m_projToArea1 = -1.0f;
        z[1].m_f = -ysVec4_one;

        ysVec4 v10 = z[0].m_posWS - z[1].m_posWS;
        ysVec4 u10 = ysNormalize3(v10);
        ys_float32 d10Sqr = ysLengthSqr3(v10);
        ys_float32 c = ysDot3(u10, z[1].m_normalWS);
        ysAssert(c >= 0.0f && d10Sqr > divZeroThresh);
        z[0].m_projToArea1 = c / d10Sqr;
        z[0].m_f = input.WDirectionalOverPDirectional01;
        z[0].m_fFinite = false;
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
        ys_float32 probAngle012;
        z1->m_material->GenerateRandomDirection(this, &u12_LS1, u10_LS1, &probAngle012);
        ysAssert(u12_LS1.z >= 0.0f);
        if (u12_LS1.z < divZeroThresh)
        {
            break;
        }
        ysVec4 u12 = ysMul33(R1, u12_LS1);
        ys_float32 probProj012 = probAngle012 / u12_LS1.z;
        ysVec4 f210 = z1->m_material->EvaluateBRDF(this, u12_LS1, u10_LS1);

        ys_float32 q = 1.0f;
        if (nE >= nEMin)
        {
            ys_float32 f = ysMax(ysMax(f210.x, f210.y), f210.z);
            ys_float32 q = ysMin(1.0f, f / probProj012);
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
        srci.m_origin = z1->m_posWS + u12 * ysSplat(rayCastNudge);

        ysSceneRayCastOutput srco;
        bool hit = RayCastClosest(&srco, srci);
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

        ysAssert(srco.m_lambda > 0.0f);
        ys_float32 d12 = srco.m_lambda + rayCastNudge;
        ys_float32 d12Sqr = d12 * d12;
        if (d12Sqr < divZeroThresh)
        {
            break;
        }

        z1->m_probProj[0] = z1->m_material->ProbabilityDensityForGeneratedDirection(this, u10_LS1, u12_LS1) / u10_LS1.z;
        z1->m_probProj[1] = probProj012 * q;
        z1->m_projToArea1 = u12_LS1.z * u21_LS2.z / d12Sqr;
        z1->m_f = f210;
        z1->m_fFinite = true;

        z2->m_shape = m_shapes + srco.m_shapeId.m_index;
        z2->m_material = m_materials + z2->m_shape->m_materialId.m_index;
        z2->m_posWS = srco.m_hitPoint;
        z2->m_normalWS = srco.m_hitNormal;
        z2->m_tangentWS = srco.m_hitTangent;
        // Set some garbage values. They will be fixed when the next vertex is generated or when we join the light and eye paths
        z2->m_probProj[0] = -1.0f;
        z2->m_probProj[1] = -1.0f;
        z2->m_probFinite[0] = false;
        z2->m_probFinite[1] = false;
        z2->m_projToArea1 = -1.0f;
        z2->m_f = -ysVec4_one;
        z2->m_fFinite = false;

        nE++;
    }

    output->m_nL = nL;
    output->m_nE = nE;
    output->m_dPdA_L0 = probArea_L0;
    output->m_dPdA_W0 = probArea_W0;
    output->m_estimatorFactor_L0 = LSpatialOverPSpatial0;
    output->m_estimatorFactor_W0 = WSpatialOverPSpatial0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysVec4 ysScene::EvaluateTruncatedSubpaths(const GenerateSubpathOutput& subpaths, ys_int32 s, ys_int32 t) const
{
    const ys_float32 rayCastNudge = 0.001f; // Hack. Push ray cast origins out a little to avoid collision with the source shape.
    const ys_float32 divZeroThresh = ys_epsilon; // Prevent unsafe division.

    ysAssert(0 <= s && s <= subpaths.m_nL);
    ysAssert(2 <= t && t <= subpaths.m_nE);
    PathVertex yz[GenerateSubpathOutput::s_nLCeil + GenerateSubpathOutput::s_nECeil];
    PathVertex* y = yz + 0;
    PathVertex* z = yz + s;
    ysMemCpy(y, subpaths.m_y, sizeof(PathVertex) * s);
    ysMemCpy(z, subpaths.m_z, sizeof(PathVertex) * t);

    const ysVec4& LSpatialOverPSpatial0 = subpaths.m_estimatorFactor_L0;
    const ysVec4& WSpatialOverPSpatial0 = subpaths.m_estimatorFactor_W0;
    const ys_float32& probArea_L0 = subpaths.m_dPdA_L0;
    const ys_float32& probArea_W0 = subpaths.m_dPdA_W0;

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
            // What about the other connections?
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
            // What about the other connections?
            return ysVec4_zero;
        }

        ys_float32 d12 = ysLength3(v12);

        ysSceneRayCastInput srci;
        srci.m_origin = x1->m_posWS + u12 * ysSplat(rayCastNudge);
        srci.m_direction = v12;
        srci.m_maxLambda = (d12 - rayCastNudge) / d12;

        ysSceneRayCastOutput srco;
        bool hit = RayCastClosest(&srco, srci);
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
            ysAssert(x1->m_material->IsEmissive(this));
            ysVec4 LSpatial = x1->m_material->EvaluateEmittedIrradiance(this);
            ysVec4 L = x1->m_material->EvaluateEmittedRadiance(this, u12_LS1);
            ys_float32 probAngle12 = x1->m_material->ProbabilityDensityForGeneratedEmission(this, u12_LS1);
            x1->m_probProj[1] = probAngle12 / u12_LS1.z;
            x1->m_probFinite[1] = true;
            x1->m_f = L / LSpatial;
            x1->m_fFinite = true;
        }
        else
        {
            ysAssert(s > 1);
            PathVertex* x0 = y + (s - 2);
            ysVec4 v10 = x0->m_posWS - x1->m_posWS;
            ysVec4 u10 = ysNormalize3(v10);
            ysVec4 u10_LS1 = ysMulT33(R1, u10);
            ys_float32 probAngle012 = x1->m_material->ProbabilityDensityForGeneratedDirection(this, u12_LS1, u10_LS1);
            ys_float32 probAngle210 = x1->m_material->ProbabilityDensityForGeneratedDirection(this, u10_LS1, u12_LS1);
            x1->m_probProj[0] = probAngle210 / u10_LS1.z;
            x1->m_probProj[1] = probAngle012 / u12_LS1.z;
            x1->m_probFinite[0] = true;
            x1->m_probFinite[1] = true;
            x1->m_f = x1->m_material->EvaluateBRDF(this, u10_LS1, u12_LS1);
            x1->m_fFinite = true;
        }

        PathVertex* x3 = z + (t - 2);
        ysVec4 v23 = x3->m_posWS - x2->m_posWS;
        ysVec4 u23 = ysNormalize3(v23);
        ysVec4 u23_LS2 = ysMulT33(R2, u23);
        ys_float32 probAngle123 = x2->m_material->ProbabilityDensityForGeneratedDirection(this, u23_LS2, u21_LS2);
        ys_float32 probAngle321 = x2->m_material->ProbabilityDensityForGeneratedDirection(this, u21_LS2, u23_LS2);
        x2->m_probProj[0] = probAngle123 / u23_LS2.z;
        x2->m_probProj[1] = probAngle321 / u21_LS2.z;
        x2->m_probFinite[0] = true;
        x2->m_probFinite[1] = true;
        x2->m_f = x2->m_material->EvaluateBRDF(this, u21_LS2, u23_LS2);
        x2->m_fFinite = true;
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
                estimatorL *= y[i].m_f / ysSplat(y[i].m_probProj[1]);
            }
        }

        ysVec4 estimatorE = ysVec4_one;
        if (t > 0)
        {
            estimatorE *= WSpatialOverPSpatial0;
            for (ys_int32 i = 0; i < t - 1; ++i)
            {
                estimatorE *= z[i].m_f / ysSplat(z[i].m_probProj[1]);
            }
        }

        ysVec4 estimatorJoin;
        ysAssert(s > 0 || t > 0);
        if (s == 0)
        {
            ysAssert(t >= 2);
            const PathVertex& z1 = z[t - 1];
            const PathVertex& z0 = z[t - 2];
            ysMtx44 R1;
            {
                R1.cx = z1.m_tangentWS;
                R1.cy = ysCross(z1.m_normalWS, z1.m_tangentWS);
                R1.cz = z1.m_normalWS;
            }
            ysVec4 v10 = z0.m_posWS - z1.m_posWS;
            ysVec4 u10 = ysNormalize3(v10);
            ysVec4 u10_LS1 = ysMulT33(R1, u10);
            estimatorJoin = z1.m_material->EvaluateEmittedRadiance(this, u10_LS1);
        }
        else if (t == 0)
        {
            ysAssert(false);
            estimatorJoin = ysVec4_zero; // TODO: Should be the emitted importance
        }
        else
        {
            ysAssert(s > 0 && t > 0);
            ysAssert(y[s - 1].m_projToArea1 == z[t - 1].m_projToArea1);
            ysVec4 g = ysSplat(y[s - 1].m_projToArea1);
            estimatorJoin = y[s - 1].m_f * g * z[t - 1].m_f;
        }

        estimator = estimatorL * estimatorJoin * estimatorE;
        ysAssert(ysAllGE3(estimator, ysVec4_zero));
    }

    //////////////////////////////
    // Compute estimator weight //
    //////////////////////////////
    ys_float32 weight;
    {
        ysAssert(s >= 0 && t >= 0 && s + t >= 2);

        // pRatioL[i] = P[i-1] / P[i] ... where P[n] is the full path probability assuming y[n] is the last vertex on the light subpath.
        ys_float32 pRatioL[GenerateSubpathOutput::s_nLCeil] = { -1.0f };
        if (s > 1)
        {
            pRatioL[0] = (y[1].m_probProj[0] * y[0].m_projToArea1) / probArea_L0;
            for (ys_int32 i = 1; i < s - 1; ++i)
            {
                // The probability to generate y[i-1]->y[i] as part of the light subpath
                ys_float32 p01 = y[i - 1].m_probProj[1] * y[i - 1].m_projToArea1;

                // The probability to generate y[i+1]->y[i] as part of the eye subpath
                ys_float32 p21 = y[i + 1].m_probProj[0] * y[i].m_projToArea1;

                pRatioL[i] = p21 / p01;
            }
            ys_float32 p01 = y[s - 2].m_probProj[1] * y[s - 2].m_projToArea1;
            ys_float32 p21 = (t > 0)
                ? z[t - 1].m_probProj[1] * z[t - 1].m_projToArea1
                : probArea_W0;
            pRatioL[s - 1] = p21 / p01;
        }
        else if (s == 1)
        {
            ysAssert(t > 0);
            pRatioL[0] = (z[t - 1].m_probProj[1] * z[t - 1].m_projToArea1) / probArea_L0;
        }

        ys_float32 pRatioE[GenerateSubpathOutput::s_nECeil] = { -1.0f };
        if (t > 1)
        {
            //pRatioE[0] = (z[1].m_probProj[0] * z[0].m_projToArea1) / probArea_W1;
            pRatioE[0] = 0.0f; // Because we only support pinhole cameras not modeled as part of the scene at the moment
            for (ys_int32 i = 1; i < t - 1; ++i)
            {
                ys_float32 p01 = z[i - 1].m_probProj[1] * z[i - 1].m_projToArea1;
                ys_float32 p21 = z[i + 1].m_probProj[0] * z[i].m_projToArea1;
                pRatioE[i] = p21 / p01;
            }
            ys_float32 p01 = z[t - 2].m_probProj[1] * z[t - 2].m_projToArea1;
            ys_float32 p21 = (s > 0)
                ? y[s - 1].m_probProj[1] * y[s - 1].m_projToArea1
                : probArea_L0;
            pRatioE[t - 1] = p21 / p01;
        }
        else if (t == 1)
        {
            ysAssert(s > 0);
            pRatioE[0] = (y[s - 1].m_probProj[1] * y[s - 1].m_projToArea1) / probArea_W0;
        }

        ys_float32 weightInv;
        {
            weightInv = 1.0f;

            ys_float32 pRatio = 1.0f;
            for (ys_int32 i = s - 1; i > -1; --i)
            {
                pRatio *= pRatioL[i];
                weightInv += (pRatio * pRatio); // Balance heuristic with exponent 2
            }

            pRatio = 1.0f;
            for (ys_int32 i = t - 1; i > -1; --i)
            {
                pRatio *= pRatioL[i];
                weightInv += (pRatio * pRatio);
            }
        }

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

    ysVec4 radiance = ysVec4_zero;
    for (ys_int32 s = 0; s < subpaths.m_nL; ++s)
    {
        for (ys_int32 t = 2; t < subpaths.m_nE; ++t)
        {
            radiance += EvaluateTruncatedSubpaths(subpaths, s, t);
        }
    }

    return radiance;
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

                                switch (input.m_giMethod)
                                {
                                    case ysSceneRenderInput::GlobalIlluminationMethod::e_uniDirectional:
                                    {
                                        // To be precise, what we should actually accumulate here is...
                                        //     radiance += Irradiance / wproj_pixel = (SampleRadiance / (dP/dwproj)) / wproj_pixel
                                        // ... where dP/dwproj is the probability per projected solid angle of sampling this point on the pixel
                                        //           wproj_pixel is the projected solid angle subtended by the pixel as seen by the eye.
                                        // Now for our specific implementation, we sample uniformly across the pixel's area such that dP/dwproj = wproj_pixel
                                        // (here we have assumed that the pixel subtends an infinitesimal solid angle)
                                        // Therefore, we merely need to accumulate radiance += SampleRadiance
                                        radiance += SampleRadiance(surfaceData, 0, input.m_maxBounceCount, input.m_sampleLight);
                                        break;
                                    }
                                    case ysSceneRenderInput::GlobalIlluminationMethod::e_biDirectional:
                                    {
                                        GenerateSubpathInput args;
                                        args.minLightPathVertexCount = 1;
                                        args.maxLightPathVertexCount = 8;
                                        args.minEyePathVertexCount = 2;
                                        args.maxEyePathVertexCount = 8;

                                        args.eyePathVertex0.m_shape = nullptr;
                                        args.eyePathVertex0.m_material = nullptr;
                                        args.eyePathVertex0.m_posWS = input.m_eye.p;
                                        args.eyePathVertex0.m_normalWS = ysVec4_zero;
                                        args.eyePathVertex0.m_tangentWS = ysVec4_zero;

                                        args.eyePathVertex1.m_shape = shape;
                                        args.eyePathVertex1.m_material = material;
                                        args.eyePathVertex1.m_posWS = rco.m_hitPoint;
                                        args.eyePathVertex1.m_normalWS = rco.m_hitNormal;
                                        args.eyePathVertex1.m_tangentWS = rco.m_hitTangent;

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