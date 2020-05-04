#pragma once

#include "YoshiPBR/ysMath.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysDirectionalProbabilityDensity
{
    void SetInvalid()
    {
        m_probabilityPerSolidAngle = -1.0f;
        m_probabilityPerSolidAngleInv = -1.0f;
        m_probabilityPerProjectedSolidAngle = -1.0f;
        m_probabilityPerProjectedSolidAngleInv = -1.0f;
    }

    bool IsValid() const
    {
        return
            m_probabilityPerSolidAngle >= 0.0f &&
            m_probabilityPerSolidAngleInv >= 0.0f &&
            m_probabilityPerProjectedSolidAngle >= 0.0f &&
            m_probabilityPerProjectedSolidAngleInv >= 0.0f;
    }

    ys_float32 m_probabilityPerSolidAngle;
    ys_float32 m_probabilityPerSolidAngleInv;

    ys_float32 m_probabilityPerProjectedSolidAngle;
    ys_float32 m_probabilityPerProjectedSolidAngleInv;
};