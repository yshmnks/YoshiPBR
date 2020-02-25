#include "camera.h"

Camera g_camera;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Camera::Camera()
{
    m_center = ysVecSet(0.0f, -16.0f, 0.0f);
    m_yaw = 0.0f;
    m_pitch = 0.0f;
    m_verticalFov = ys_pi * 0.25f;
    m_aspectRatio = 16.0f / 9.0f;
    m_near = 0.25f;
    m_far = 128.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Camera::BuildProjectionMatrix(ys_float32* m) const
{
    // http://www.songho.ca/opengl/gl_projectionmatrix.html
    ys_float32 h = m_near * tanf(m_verticalFov);
    ys_float32 w = h * m_aspectRatio;

    ys_float32 negInvLength = 1.0f / (m_near - m_far);

    m[0] = m_near / w;
    m[1] = 0.0f;
    m[2] = 0.0f;
    m[3] = 0.0f;

    m[4] = 0.0f;
    m[5] = m_near / h;
    m[6] = 0.0f;
    m[7] = 0.0f;

    m[8] = 0.0f;
    m[9] = 0.0f;
    m[10] = negInvLength * (m_near + m_far);
    m[11] = -1.0f;

    m[12] = 0.0f;
    m[13] = 0.0f;
    m[14] = negInvLength * (2.0f * m_near * m_far);
    m[15] = 0.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static ysVec4 sComputeCameraQuat(ys_float32 yaw, ys_float32 pitch)
{
    ysVec4 qYaw = ysQuatFromAxisAngle(ysVec4_unitZ, -yaw);
    ysVec4 qPitch = ysQuatFromAxisAngle(ysVec4_unitX, ys_pi * 0.5f + pitch);
    return ysMulQQ(qYaw, qPitch);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Camera::BuildViewMatrix(ys_float32* m) const
{
    ysTransform camXf;
    camXf.p = m_center;
    camXf.q = sComputeCameraQuat(m_yaw, m_pitch);
    ysTransform viewXf = ysInvert(camXf);
    ysMtx44 viewMtx = ysMtxFromTransform(viewXf);
    ysMemCpy(m, &viewMtx, sizeof(ysMtx44));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysVec4 Camera::ComputeRight() const
{
    ysVec4 q = sComputeCameraQuat(m_yaw, m_pitch);
    return ysRotate(q, ysVec4_unitX);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysVec4 Camera::ComputeUp() const
{
    ysVec4 q = sComputeCameraQuat(m_yaw, m_pitch);
    return ysRotate(q, ysVec4_unitY);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysVec4 Camera::ComputeForward() const
{
    ysVec4 q = sComputeCameraQuat(m_yaw, m_pitch);
    return -ysRotate(q, ysVec4_unitZ);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysTransform Camera::ComputeEyeTransform() const
{
    ysTransform camXf;
    camXf.p = m_center;
    camXf.q = sComputeCameraQuat(m_yaw, m_pitch);
    return camXf;
}