#include "YoshiPBR/ysScene.h"
#include "YoshiPBR/ysDebugDraw.h"
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
    m_shapeCount = 0;
    m_triangleCount = 0;
    m_materialCount = 0;
    m_materialStandardCount = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene::Create(const ysSceneDef* def)
{
    m_shapeCount = def->m_triangleCount;
    m_shapes = static_cast<ysShape*>(ysMalloc(sizeof(ysShape) * m_shapeCount));

    m_triangleCount = def->m_triangleCount;
    m_triangles = static_cast<ysTriangle*>(ysMalloc(sizeof(ysTriangle) * m_triangleCount));

    ys_int32 shapeIdx = 0;

    ysAABB* aabbs = static_cast<ysAABB*>(ysMalloc(sizeof(ysAABB) * m_shapeCount));
    ysShapeId* shapeIds = static_cast<ysShapeId*>(ysMalloc(sizeof(ysShapeId) * m_shapeCount));

    for (ys_int32 i = 0; i < m_triangleCount; ++i, ++shapeIdx)
    {
        ysTriangle* dstTriangle = m_triangles + i;
        const ysInputTriangle* srcTriangle = def->m_triangles + i;
        dstTriangle->m_v[0] = srcTriangle->vertices[0];
        dstTriangle->m_v[1] = srcTriangle->vertices[1];
        dstTriangle->m_v[2] = srcTriangle->vertices[2];
        ysVec4 ab = dstTriangle->m_v[1] - dstTriangle->m_v[0];
        ysVec4 ac = dstTriangle->m_v[2] - dstTriangle->m_v[0];
        ysVec4 ab_x_ac = ysCross(ab, ac);
        dstTriangle->m_n = ysIsSafeToNormalize3(ab_x_ac) ? ysNormalize3(ab_x_ac) : ysVec4_zero;
        //triangle->m_t = TODO;
        dstTriangle->m_twoSided = srcTriangle->twoSided;

        ysShape* shape = m_shapes + shapeIdx;
        shape->m_type = ysShape::Type::e_triangle;
        shape->m_typeIndex = i;

        aabbs[shapeIdx] = dstTriangle->ComputeAABB();
        shapeIds[shapeIdx].m_index = shapeIdx;
    }

    ysAssert(shapeIdx == m_shapeCount);

    m_bvh.Create(aabbs, shapeIds, m_shapeCount);
    ysFree(shapeIds);
    ysFree(aabbs);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene::Destroy()
{
    m_bvh.Destroy();
    ysFree(m_shapes);
    ysFree(m_triangles);
    m_shapeCount = 0;
    m_triangleCount = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ysScene::RayCastClosest(ysRayCastOutput* output, const ysRayCastInput& input) const
{
    return m_bvh.RayCastClosest(this, output, input);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene::Render(ysSceneRenderOutput* output, const ysSceneRenderInput* input) const
{
    output->m_pixels.SetCount(input->m_pixelCountX * input->m_pixelCountY);

    ys_float32 maxDepth = 0.0f;

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

            ysRayCastInput rci;
            rci.m_maxLambda = ys_maxFloat;
            rci.m_ray.m_direction = pixelDirWS;
            rci.m_ray.m_origin = input->m_eye.p;

            ysRayCastOutput rco;
            bool hit = RayCastClosest(&rco, rci);
            if (hit)
            {
                ys_float32 depth = rco.m_lambda * ysLength3(pixelDirLS);
                maxDepth = ysMax(depth, maxDepth);
                output->m_pixels[pixelIdx].r = depth;
                output->m_pixels[pixelIdx].g = depth;
                output->m_pixels[pixelIdx].b = depth;
            }
            else
            {
                output->m_pixels[pixelIdx].r = 0.0f;
                output->m_pixels[pixelIdx].g = 0.0f;
                output->m_pixels[pixelIdx].b = 0.0f;
            }

            pixelIdx++;
        }
    }

    ys_float32 maxDepthInv = (maxDepth > 0.0f) ? 1.0f / maxDepth : 0.0f;
    for (ys_int32 i = 0; i < output->m_pixels.GetCount(); ++i)
    {
        output->m_pixels[i].r *= maxDepthInv;
        output->m_pixels[i].g *= maxDepthInv;
        output->m_pixels[i].b *= maxDepthInv;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysScene::DebugDrawGeo(const ysDrawInputGeo* input) const
{
    Color colors[1];
    colors[0] = Color(1.0f, 0.0f, 0.0f);
    for (ys_int32 i = 0; i < m_triangleCount; ++i)
    {
        ysTriangle* triangle = m_triangles + i;
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