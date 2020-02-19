#pragma once

#include "YoshiPBR/ysDebugDraw.h"

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
struct DebugDraw : public ysDebugDraw
{
    DebugDraw();
    ~DebugDraw();

    void Create();
    void Destroy();

    virtual void DrawSegmentList(const ysVec4* vertices, const Color* colors, ys_int32 segmentCount) override;
    virtual void DrawTriangleList(const ysVec4* vertices, const Color* colors, ys_int32 triangleCount) override;

    void Flush();

    bool m_showUI;
    bool m_drawBVH;
    bool m_drawGeo;

    GLRenderLines* m_lines;
    GLRenderTriangles* m_triangles;
};

extern DebugDraw g_debugDraw;
extern Camera g_camera;
extern GLFWwindow* g_mainWindow;
