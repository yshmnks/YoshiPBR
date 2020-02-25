#pragma once

#include "YoshiPBR/ysMath.h"

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

    ysTransform ComputeEyeTransform() const;

    ysVec4 m_center;
    ys_float32 m_yaw;
    ys_float32 m_pitch;
    ys_float32 m_verticalFov; // one-sided i.e. 60 degrees means we see a total of 120 degrees
    ys_float32 m_aspectRatio; // frustum width / height
    ys_float32 m_near;
    ys_float32 m_far;
};

extern Camera g_camera;