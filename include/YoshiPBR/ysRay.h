#pragma once

#include "YoshiPBR/ysMath.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysRay
{
    void Reset();

    ysVec4 m_origin;
    ysVec4 m_direction;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysRayCastInput
{
    ysRayCastInput()
    {
        m_ray.Reset();
    }

    ysRay m_ray;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysRayCastOutput
{
    ysVec4 m_hitPoint;
    ysVec4 m_hitNormal;
    ys_float32 m_lambda; // hitpoint = origin + lambda * direction
};