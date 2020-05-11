#pragma once

#include "YoshiPBR/ysDebugDraw.h"

struct GLFWwindow;

struct GLRenderLines;
struct GLRenderTriangles;
struct GLRenderPrimitiveLines;
struct GLRenderPrimitiveTriangles;
struct GLRenderTexturedQuads;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Texture2D
{
    // row major pixels from top left to bottom right
    const Color* m_pixels;
    ys_int32 m_width;
    ys_int32 m_height;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct DebugDraw : public ysDebugDraw
{
    DebugDraw();
    ~DebugDraw();

    void Create();
    void Destroy();

    virtual void DrawSegmentList(const ysVec4* vertices, const Color* colors, ys_int32 segmentCount) override;
    virtual void DrawTriangleList(const ysVec4* vertices, const Color* colors, ys_int32 triangleCount) override;

    virtual void DrawWireBox(const ysVec4& halfDimensions, const ysTransform&, const Color&) override;
    virtual void DrawWireEllipsoid(const ysVec4& halfDimensions, const ysTransform&, const Color&) override;

    virtual void DrawBox(const ysVec4& halfDimensions, const ysTransform&, const Color&) override;
    virtual void DrawEllipsoid(const ysVec4& halfDimensions, const ysTransform&, const Color&) override;

    void DrawTexturedQuad(const Texture2D&, ys_float32 xWidth, ys_float32 yHeight, const ysTransform&, bool useDepthTest, bool useAlphaAsCutout);

    void Flush();

    GLRenderLines* m_lines;
    GLRenderTriangles* m_triangles;

    GLRenderPrimitiveLines* m_primLines;
    GLRenderPrimitiveTriangles* m_primTriangles;

    GLRenderTexturedQuads* m_texturedQuads;
};

extern DebugDraw g_debugDraw;
extern GLFWwindow* g_mainWindow;
