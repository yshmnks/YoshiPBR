#pragma once

#include "YoshiPBR/ysMath.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysProbabilityDensity
{
    void SetInvalid()
    {
        m_value = -1.0f;
        m_isFinite = false;
    }

    ys_float32 m_value;
    bool m_isFinite;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysDirectionalProbabilityDensity
{
    void SetInvalid()
    {
        m_perSolidAngle.SetInvalid();
        m_perProjectedSolidAngle.SetInvalid();
    }

    bool IsValid() const
    {
        if (m_perSolidAngle.m_value < 0.0f || m_perProjectedSolidAngle.m_value < 0.0f)
        {
            return false;
        }

        if (m_perProjectedSolidAngle.m_isFinite == false && m_perProjectedSolidAngle.m_value != 1.0f)
        {
            // When the probablility density per projected solid angle is unbounded, we assign 1.0 as the coefficient of the infinitely
            // sharp distribution in the space of projected directions. IF the probability per solid angle is also unbounded, then the
            // coefficient m_probabilityPerSolidAngle should be set to cos(theta_0), where theta_0 is the azimuthal of the perfectly
            // specular generated direction. This way, when the probability density with respect to both measures are unbounded, we
            // still preserve the identity
            //     m_probabilityPerProjectedSolidAngle * cos(theta_0) = m_probabilityPerSolidAngle
            // Of course, we could also express everything in the space of vanilla (non-projected) directions, assigning unit
            // coefficient for unbounded per-solid-angle probability. We chose against this because it would sometimes require us to
            // assign non-finite coefficients to the projected probability due to division by cos(theta_0).
            return false;
        }

        if (m_perSolidAngle.m_isFinite == false && m_perSolidAngle.m_value > 1.0f)
        {
            return false;
        }

        return true;
    }

    ysProbabilityDensity m_perSolidAngle;
    ysProbabilityDensity m_perProjectedSolidAngle;
};