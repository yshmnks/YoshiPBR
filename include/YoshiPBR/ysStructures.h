#pragma once

#include "YoshiPBR/ysArrayG.h"
#include "YoshiPBR/ysMath.h"

struct ysDebugDraw;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysSceneId
{
    ys_int32 m_index;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysShapeId
{
    ys_int32 m_index;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysMaterialId
{
    ys_int32 m_index;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysRenderId
{
    ys_int32 m_sceneIdx;
    ys_int32 m_index;
};

extern const ysSceneId      ys_nullSceneId;
extern const ysShapeId      ys_nullShapeId;
extern const ysMaterialId   ys_nullMaterialId;
extern const ysRenderId     ys_nullRenderId;

bool operator==(const ysSceneId&, const ysSceneId&);
bool operator!=(const ysSceneId&, const ysSceneId&);
bool operator==(const ysShapeId&, const ysShapeId&);
bool operator!=(const ysShapeId&, const ysShapeId&);
bool operator==(const ysMaterialId&, const ysMaterialId&);
bool operator!=(const ysMaterialId&, const ysMaterialId&);
bool operator==(const ysRenderId&, const ysRenderId&);
bool operator!=(const ysRenderId&, const ysRenderId&);

enum struct ysMaterialType
{
    e_standard,
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysInputTriangle
{
    // A free-standing triangle
    ysVec4 m_vertices[3];
    bool m_twoSided;

    ysMaterialType m_materialType;
    ys_int32 m_materialTypeIndex;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysMaterialStandardDef
{
    ysVec4 m_albedoDiffuse;
    ysVec4 m_albedoSpecular;

    ysVec4 m_emissiveDiffuse;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysLightPointDef
{
    ysVec4 m_position;
    ysVec4 m_wattage;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysSceneDef
{
    ysSceneDef()
    {
        m_triangles = nullptr;
        m_triangleCount = 0;

        m_materialStandards = nullptr;
        m_materialStandardCount = 0;

        m_lightPoints = nullptr;
        m_lightPointCount = 0;
    }

    const ysInputTriangle* m_triangles;
    ys_int32 m_triangleCount;

    const ysMaterialStandardDef* m_materialStandards;
    ys_int32 m_materialStandardCount;

    const ysLightPointDef* m_lightPoints;
    ys_int32 m_lightPointCount;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysSceneRayCastInput
{
    ysSceneRayCastInput()
    {
        m_origin = ysVec4_zero;
        m_direction = ysVec4_unitZ;
        m_maxLambda = ys_maxFloat;
    }

    ysVec4 m_origin;
    ysVec4 m_direction;
    ys_float32 m_maxLambda;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysSceneRayCastOutput
{
    ysVec4 m_hitPoint;
    ysVec4 m_hitNormal;
    ysVec4 m_hitTangent;
    ys_float32 m_lambda;
    ysShapeId m_shapeId;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysSceneRenderInput
{
    enum RenderMode
    {
        e_regular,
        e_normals,
        e_depth,
    };

    ysSceneRenderInput()
    {
        m_eye = ysTransform_identity;
        m_fovY = 0.0f;
        m_pixelCountX = 0;
        m_pixelCountY = 0;
        m_maxBounceCount = 1;
        m_samplesPerPixel = 16;
        m_sampleLight = true;
        m_sampleBRDF = true;
        m_useRussianRouletteTermination = true;

        m_renderMode = RenderMode::e_regular;
    }

    // For identity-eye-orientation, the eye looks down the -z axis (such that the x axis points right and y axis points up).
    ysTransform m_eye;

    // Vertical field of view in radians
    ys_float32 m_fovY;

    // Pixels are assumed to be "square,"  so the horizontal (x) fov is inferred from the pixel count aspect ratio.
    ys_int32 m_pixelCountX;
    ys_int32 m_pixelCountY;

    // 1 is direct illumination only.
    ys_int32 m_maxBounceCount;

    ys_int32 m_samplesPerPixel;

    // Sampling strategies. If multiple strategies are enabled, the results are combined via multiple importance sampling.
    // If no strategies are enabled, then it is assumed that all strategies should be employed.
    bool m_sampleLight;
    bool m_sampleBRDF;

    bool m_useRussianRouletteTermination;

    RenderMode m_renderMode;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysSceneRenderOutput
{
    ysSceneRenderOutput()
    {
        m_pixels.Create();
    }

    ysArrayG<ysFloat3> m_pixels;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysSceneRenderOutputIntermediate
{
    ysSceneRenderOutputIntermediate()
    {
        m_pixels.Create();
    }

    ysArrayG<ysFloat4> m_pixels;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysDrawInputBVH
{
    ysDrawInputBVH()
    {
        debugDraw = nullptr;
        depth = -1;
    }

    ysDebugDraw* debugDraw;

    // If negative, all nodes will be drawn. (depth=0 at the root and increases descending into the children)
    ys_int32 depth;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysDrawInputGeo
{
    ysDrawInputGeo()
    {
        debugDraw = nullptr;
    }

    ysDebugDraw* debugDraw;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysDrawInputLights
{
    ysDrawInputLights()
    {
        debugDraw = nullptr;
    }

    ysDebugDraw* debugDraw;
};