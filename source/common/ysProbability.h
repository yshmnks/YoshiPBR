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
        m_finiteProbabilityPerSolidAngle = false;
        m_finiteProbabilityPerSolidAngleInv = false;
        m_finiteProbabilityPerProjectedSolidAngle = false;
        m_finiteProbabilityPerProjectedSolidAngleInv = false;
    }

    bool IsValid() const
    {
        if (m_probabilityPerSolidAngle < 0.0f ||
            m_probabilityPerSolidAngleInv < 0.0f ||
            m_probabilityPerProjectedSolidAngle < 0.0f ||
            m_probabilityPerProjectedSolidAngleInv < 0.0f)
        {
            return false;
        }

        if (m_finiteProbabilityPerSolidAngle == false && m_finiteProbabilityPerSolidAngleInv == false)
        {
            return false;
        }

        if (m_finiteProbabilityPerProjectedSolidAngle == false && m_finiteProbabilityPerProjectedSolidAngleInv == false)
        {
            return false;
        }

        if (m_finiteProbabilityPerSolidAngle == false)
        {
            if (m_probabilityPerSolidAngleInv != 0.0f)
            {
                return false;
            }
        }

        if (m_finiteProbabilityPerProjectedSolidAngle == false)
        {
            if (m_probabilityPerProjectedSolidAngle != 1.0f)
            {
                // When the probablility density per projected solid angle is unbounded, we assign 1.0 as the coefficient of the infinitely
                // sharp distribution in the space of projected directions. IF the probability per solid angle is also unbounded, then the
                // coefficient m_probabilityPerSolidAngle should be set tocos(theta_0), where theta_0 is the azimuthal of the perfectly
                // specular generated direction. This way, when the probability density with respect to both measures are unbounded, we
                // still preserve the identity
                //     m_probabilityPerProjectedSolidAngle * cos(theta_0) = m_probabilityPerSolidAngle
                // Of course, we could also express everything in the space of vanilla (non-projected) directions, assigning unit
                // coefficient for unbounded per-solid-angle probability. We chose against this because it would sometimes require us to
                // assign non-finite coefficients to the projected probability due to division by cos(theta_0).
                return false;
            }

            if (m_probabilityPerProjectedSolidAngleInv != 0.0f)
            {
                return false;
            }
        }

        if (m_finiteProbabilityPerSolidAngleInv == false)
        {
            if (m_probabilityPerSolidAngle != 0.0f)
            {
                return false;
            }
        }

        if (m_finiteProbabilityPerProjectedSolidAngleInv == false)
        {
            if (m_probabilityPerProjectedSolidAngle != 0.0f)
            {
                return false;
            }
        }

        return true;
    }

    ys_float32 m_probabilityPerSolidAngle;
    ys_float32 m_probabilityPerSolidAngleInv;
    ys_float32 m_probabilityPerProjectedSolidAngle;
    ys_float32 m_probabilityPerProjectedSolidAngleInv;
    bool m_finiteProbabilityPerSolidAngle               : 1;
    bool m_finiteProbabilityPerSolidAngleInv            : 1;
    bool m_finiteProbabilityPerProjectedSolidAngle      : 1;
    bool m_finiteProbabilityPerProjectedSolidAngleInv   : 1;
};