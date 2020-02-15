#pragma once

#include "YoshiPBR/ysMath.h"

struct GLFWwindow;

struct GLRenderLines;
struct GLRenderTriangles;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Camera
{
    Camera();

    // m is populated in column-major order
    void BuildProjectionMatrix(ys_float32* m) const;
    void BuildViewMatrix(ys_float32* m) const;

    ysVec4 ComputeRight() const;
    ysVec4 ComputeUp() const;
    ysVec4 ComputeForward() const;

    ysVec4 m_center;
    ys_float32 m_yaw;
    ys_float32 m_pitch;
    ys_float32 m_verticalFov; // one-sided i.e. 60 degrees means we see a total of 120 degrees
    ys_float32 m_aspectRatio; // frustum width / height
    ys_float32 m_near;
    ys_float32 m_far;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Color
{
    Color()
    {
        r = 1.0f;
        g = 1.0f;
        b = 1.0f;
        a = 1.0f;
    }

    Color(ys_float32 rIn, ys_float32 gIn, ys_float32 bIn, ys_float32 aIn = 1.0f)
    {
        r = rIn;
        g = gIn;
        b = bIn;
        a = aIn;
    }

    ys_float32 r, g, b, a;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct DebugDraw
{
public:
    DebugDraw();
    ~DebugDraw();

    void Create();
    void Destroy();

    // color is per primitive (not per vertex) i.e. for triangles you should pass 3x as many vertices as colors
    void DrawSegmentList(const ysVec4* vertices, const Color* colors, ys_int32 segmentCount);
    void DrawTriangleList(const ysVec4* vertices, const Color* colors, ys_int32 triangleCount);

    void Flush();

    bool m_showUI;
    GLRenderLines* m_lines;
    GLRenderTriangles* m_triangles;
};

extern DebugDraw g_debugDraw;
extern Camera g_camera;
extern GLFWwindow* g_mainWindow;
