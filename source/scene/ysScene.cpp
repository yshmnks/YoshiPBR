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
void ysScene::Create(const ysSceneDef* def)
{
    m_shapeCount = def->m_triangleCount;
    m_shapes = static_cast<ysShape*>(ysMalloc(sizeof(ysShape) * m_shapeCount));

    m_triangleCount = def->m_triangleCount;
    m_triangles = static_cast<ysTriangle*>(ysMalloc(sizeof(ysTriangle) * m_triangleCount));

    m_materialCount = def->m_materialStandardCount;
    m_materials = static_cast<ysMaterial*>(ysMalloc(sizeof(ysMaterial) * m_materialCount));

    m_materialStandardCount = def->m_materialStandardCount;
    m_materialStandards = static_cast<ysMaterialStandard*>(ysMalloc(sizeof(ysMaterialStandard) * m_materialStandardCount));

    m_lightCount = def->m_lightPointCount;
    m_lights = static_cast<ysLight*>(ysMalloc(sizeof(ysLight) * m_lightCount));

    m_lightPointCount = def->m_lightPointCount;
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
        const ysInputTriangle* srcTriangle = def->m_triangles + i;
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
        const ysMaterialStandardDef* src = def->m_materialStandards + i;
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
        const ysLightPointDef* src = def->m_lightPoints + i;
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
ysVec4 ysScene::SampleRadiance(ysShapeId shapeId, const ysVec4& posWS, const ysVec4& normalWS, const ysVec4& inWS, ys_int32 bounceCount, ys_int32 maxBounceCount) const
{
    const ysShape* shape = m_shapes + shapeId.m_index;
    const ysMaterial* material = m_materials + shape->m_materialId.m_index;
    ysVec4 inLS;
    ysVec4 posLS_dummy;
    ysVec4 posWS_dummy = ysVec4_zero;
    shape->ConvertFromWSToLS(this, &posLS_dummy, &inLS, posWS_dummy, inWS);

    ysVec4 radiance = ysVec4_zero;

    for (ys_int32 i = 0; i < m_lightPointCount; ++i)
    {
        const ysLightPoint* light = m_lightPoints + i;
        ysVec4 surfaceToLightWS = light->m_position - posWS;
        ys_float32 rr = ysLengthSqr3(surfaceToLightWS);
        if (ysIsSafeToNormalize3(surfaceToLightWS) == false)
        {
            continue;
        }
        ysVec4 nSurfaceToLightWS = ysNormalize3(surfaceToLightWS);
        ys_float32 cosTheta = ysDot3(nSurfaceToLightWS, normalWS);
        ysVec4 projIrradiance = light->m_radiantIntensity * ysSplat(cosTheta) / ysSplat(rr);
        ysVec4 nSurfaceToLightLS;
        shape->ConvertFromWSToLS(this, &posLS_dummy, &nSurfaceToLightLS, posWS_dummy, nSurfaceToLightWS);
        ysVec4 brdf = material->EvaluateBRDF(this, inLS, nSurfaceToLightLS);
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
        ysVec4 outLS;
        ys_float32 probDens;
        material->GenerateRandomDirection(this, &outLS, &probDens, inLS);
        ysAssert(probDens > 0.0f);
        ysVec4 outWS;
        shape->ConvertFromLSToWS(this, &posWS_dummy, &outWS, posLS_dummy, inLS);

        ysSceneRayCastInput rci;
        rci.m_maxLambda = ys_maxFloat;
        rci.m_direction = outWS;
        rci.m_origin = posWS + outWS * ysSplat(0.001f); // Hack. Push it out a little to collision with the source shape.

        ysSceneRayCastOutput rco;
        bool hit = RayCastClosest(&rco, rci);
        if (hit == false)
        {
            continue;
        }

        ysVec4 brdf = material->EvaluateBRDF(this, inLS, outLS);
        ysAssert(ysAllGE3(brdf, ysVec4_zero));
        ysVec4 irradiance = SampleRadiance(rco.m_shapeId, rco.m_hitPoint, rco.m_hitNormal, -outWS, bounceCount + 1, maxBounceCount);
        ysAssert(ysAllGE3(irradiance, ysVec4_zero));
        indirectRadiance = indirectRadiance + brdf * irradiance / ysSplat(probDens);
    }

    ys_float32 invSampleCount = 1.0f / sampleDirCount;
    radiance = radiance + indirectRadiance * ysSplat(invSampleCount);
    return radiance;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene::Render(ysSceneRenderOutput* output, const ysSceneRenderInput* input) const
{
    output->m_pixels.SetCount(input->m_pixelCountX * input->m_pixelCountY);

    const ys_float32 aspectRatio = ys_float32(input->m_pixelCountX) / ys_float32(input->m_pixelCountY);
    const ys_float32 height = tanf(input->m_fovY);
    const ys_float32 width = height * aspectRatio;

    ys_int32 pixelIdx = 0;
    for (ys_int32 i = 0; i < input->m_pixelCountY; ++i)
    {
        ys_float32 yFraction = 1.0f - 2.0f * ys_float32(i + 1) / ys_float32(input->m_pixelCountY);
        ys_float32 y = height * yFraction;
        for (ys_int32 j = 0; j < input->m_pixelCountX; ++j)
        {
            ys_float32 xFraction = 2.0f * ys_float32(j + 1) / ys_float32(input->m_pixelCountX) - 1.0f;
            ys_float32 x = width * xFraction;

            ysVec4 pixelDirLS = ysVecSet(x, y, -1.0f, 0.0f);
            ysVec4 pixelDirWS = ysRotate(input->m_eye.q, pixelDirLS);

            ysSceneRayCastInput rci;
            rci.m_maxLambda = ys_maxFloat;
            rci.m_direction = pixelDirWS;
            rci.m_origin = input->m_eye.p;

            ysSceneRayCastOutput rco;
            bool hit = RayCastClosest(&rco, rci);
            if (hit == false)
            {
                output->m_pixels[pixelIdx].r = 0.0f;
                output->m_pixels[pixelIdx].g = 0.0f;
                output->m_pixels[pixelIdx].b = 0.0f;
                pixelIdx++;
                continue;
            }

            ysVec4 radiance = SampleRadiance(rco.m_shapeId, rco.m_hitPoint, rco.m_hitNormal, -pixelDirWS, 0, input->m_maxBounceCount);
            output->m_pixels[pixelIdx].r = radiance.x;
            output->m_pixels[pixelIdx].g = radiance.y;
            output->m_pixels[pixelIdx].b = radiance.z;
            pixelIdx++;
        }
    }

    //ys_float32 maxDepthInv = (maxDepth > 0.0f) ? 1.0f / maxDepth : 0.0f;
    //for (ys_int32 i = 0; i < output->m_pixels.GetCount(); ++i)
    //{
    //    output->m_pixels[i].r *= maxDepthInv;
    //    output->m_pixels[i].g *= maxDepthInv;
    //    output->m_pixels[i].b *= maxDepthInv;
    //}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene::DebugDrawGeo(const ysDrawInputGeo* input) const
{
    Color colors[1];
    colors[0] = Color(1.0f, 0.0f, 0.0f);
    for (ys_int32 i = 0; i < m_triangleCount; ++i)
    {
        const ysTriangle* triangle = m_triangles + i;
        input->debugDraw->DrawTriangleList(triangle->m_v, colors, 1);
        if (triangle->m_twoSided)
        {
            ysVec4 cba[3] = { triangle->m_v[2], triangle->m_v[1], triangle->m_v[0] };
            input->debugDraw->DrawTriangleList(cba, colors, 1);
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
        input->debugDraw->DrawSegmentList(&segments[0][0], c, 3);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene::DebugDrawLights(const ysDrawInputLights* input) const
{
    Color wireFrameColor = Color(1.0f, 1.0f, 1.0f);
    for (ys_int32 i = 0; i < m_lightPointCount; ++i)
    {
        const ysLightPoint* light = m_lightPoints + i;
        ysTransform xf;
        xf.q = ysQuat_identity;
        xf.p = light->m_position;
        ysVec4 halfDims = ysSplat(0.25f);
        input->debugDraw->DrawWireEllipsoid(halfDims, xf, wireFrameColor);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysScene* ysScene_Create(const ysSceneDef* def)
{
    ysScene* scene = static_cast<ysScene*>(ysMalloc(sizeof(ysScene)));
    scene->Create(def);
    return scene;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene_Destroy(ysScene* scene)
{
    scene->Destroy();
    ysFree(scene);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene_Render(const ysScene* scene, ysSceneRenderOutput* output, const ysSceneRenderInput* input)
{
    scene->Render(output, input);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ys_int32 ysScene_GetBVHDepth(const ysScene* scene)
{
    return scene->m_bvh.m_depth;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene_DebugDrawBVH(const ysScene* scene, const ysDrawInputBVH* input)
{
    scene->m_bvh.DebugDraw(input);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene_DebugDrawGeo(const ysScene* scene, const ysDrawInputGeo* input)
{
    scene->DebugDrawGeo(input);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene_DebugDrawLights(const ysScene* scene, const ysDrawInputLights* input)
{
    scene->DebugDrawLights(input);
}