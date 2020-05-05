#include "ysMaterialStandard.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysVec4 ysMaterialStandard::EvaluateBRDF(const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const
{
    return m_albedoDiffuse / ysVec4_pi;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysVec4 ysMaterialStandard::EvaluateEmittedRadiance(const ysVec4& direction) const
{
    return m_emissiveDiffuse;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysVec4 ysMaterialStandard::EvaluateEmittedIrradiance() const
{
    return m_emissiveDiffuse * ysVec4_pi;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ysMaterialStandard::IsEmissive() const
{
    return m_emissiveDiffuse != ysVec4_zero;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterialStandard::GenerateRandomDirection(const ysVec4& incomingLS, ysVec4* outgoingLS) const
{
    // Importance sample per-solid-angle probability  pdf(theta) = cos(theta) / pi
    //                                                cdf(theta) = (1 - cos(2*theta)) / 2
    //                                                ==> cos(theta) = sqrt(1 - cdf)
    // An equivalent description is that we are sampling per-PROJECTED-solid-angle probability 1/pi. This corresponds to uniform sampling of
    // the 2D unit disc where the remapping r=sin(theta) gives the 3D direction. A 2D ring has area 2pi*r*dr = pi*d(r^2), so we sample r^2
    // with uniform random variable v, or equivalently, sin(theta) = sqrt(v)
    //                                                ==> cos(theta) = sqrt(1 - v) ... which is identical to our first result.
    ys_float32 u = ysRandom(0.0f, 1.0f);
    ys_float32 v = ysRandom(0.0f, 1.0f);
    ys_float32 phi = ys_2pi * u;
    ys_float32 cosTheta = sqrtf(1.0f - v);
    ys_float32 sinTheta = sqrtf(v);
    *outgoingLS = ysVecSet(sinTheta * cosf(phi), sinTheta * sinf(phi), cosTheta);
    ysDirectionalProbabilityDensity p;
    if (cosTheta < ys_zeroSafe)
    {
        p.m_probabilityPerSolidAngle = 0.0f;
        p.m_probabilityPerSolidAngleInv = 0.0f;
        p.m_finiteProbabilityPerSolidAngle = true;
        p.m_finiteProbabilityPerSolidAngleInv = false;
    }
    else
    {
        p.m_probabilityPerSolidAngle = cosTheta / ys_pi;
        p.m_probabilityPerSolidAngleInv = ys_pi / cosTheta;
        p.m_finiteProbabilityPerSolidAngle = true;
        p.m_finiteProbabilityPerSolidAngleInv = true;
    }
    p.m_probabilityPerProjectedSolidAngle = 1.0f / ys_pi;
    p.m_probabilityPerProjectedSolidAngleInv = ys_pi;
    p.m_finiteProbabilityPerProjectedSolidAngle = true;
    p.m_finiteProbabilityPerProjectedSolidAngleInv = true;
    return p;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterialStandard::GenerateRandomDirection(ysVec4* incomingLS, const ysVec4& outgoingLS) const
{
    return GenerateRandomDirection(outgoingLS, incomingLS);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterialStandard::GenerateRandomEmission(ysVec4* emittedDirectionLS) const
{
    ys_float32 phi = ys_2pi * ysRandom(0.0f, 1.0f);
    ys_float32 cosTheta = ysRandom(0.0f, 1.0f);
    ys_float32 sinTheta = sqrtf(ysMax(0.0f, 1.0f - cosTheta * cosTheta));
    *emittedDirectionLS = ysVecSet(sinTheta * cosf(phi), sinTheta * sinf(phi), cosTheta);
    ysDirectionalProbabilityDensity p;
    p.m_probabilityPerSolidAngle = 1.0f / ys_2pi;
    p.m_probabilityPerSolidAngleInv = ys_2pi;
    p.m_finiteProbabilityPerSolidAngle = true;
    p.m_finiteProbabilityPerSolidAngleInv = true;
    if (cosTheta < ys_zeroSafe)
    {
        p.m_probabilityPerProjectedSolidAngle = 1.0f;
        p.m_probabilityPerProjectedSolidAngleInv = 0.0f;
        p.m_finiteProbabilityPerProjectedSolidAngle = false;
        p.m_finiteProbabilityPerProjectedSolidAngleInv = true;
    }
    else
    {
        p.m_probabilityPerProjectedSolidAngle = 1.0f / (cosTheta * ys_2pi);
        p.m_probabilityPerProjectedSolidAngleInv = cosTheta * ys_2pi;
        p.m_finiteProbabilityPerProjectedSolidAngle = true;
        p.m_finiteProbabilityPerProjectedSolidAngleInv = true;
    }
    return p;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterialStandard::ProbabilityDensityForGeneratedIncomingDirection(const ysVec4& inLS, const ysVec4& outLS) const
{
    return ProbabilityDensityForGeneratedOutgoingDirection(outLS, inLS);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterialStandard::ProbabilityDensityForGeneratedOutgoingDirection(const ysVec4& inLS, const ysVec4& outLS) const
{
    ysAssert(ysAbs(ysLength3(outLS) - 1.0f) < 0.001f);
    ysDirectionalProbabilityDensity p;
    if (outLS.z > 0.0f)
    {
        if (outLS.z > ys_zeroSafe)
        {
            p.m_probabilityPerSolidAngle = outLS.z / ys_pi;
            p.m_probabilityPerSolidAngleInv = ys_pi / outLS.z;
            p.m_finiteProbabilityPerSolidAngle = true;
            p.m_finiteProbabilityPerSolidAngleInv = true;
        }
        else
        {
            p.m_probabilityPerSolidAngle = 0.0f;
            p.m_probabilityPerSolidAngleInv = 0.0f;
            p.m_finiteProbabilityPerSolidAngle = true;
            p.m_finiteProbabilityPerSolidAngleInv = false;
        }
        p.m_probabilityPerProjectedSolidAngle = 1.0f / ys_pi;
        p.m_probabilityPerProjectedSolidAngleInv = ys_pi;
        p.m_finiteProbabilityPerProjectedSolidAngle = true;
        p.m_finiteProbabilityPerProjectedSolidAngleInv = true;
    }
    else
    {
        p.m_probabilityPerSolidAngle = 0.0f;
        p.m_probabilityPerSolidAngleInv = 0.0f;
        p.m_probabilityPerProjectedSolidAngle = 0.0f;
        p.m_probabilityPerProjectedSolidAngleInv = 0.0f;
        p.m_finiteProbabilityPerSolidAngle = true;
        p.m_finiteProbabilityPerSolidAngleInv = false;
        p.m_finiteProbabilityPerProjectedSolidAngle = true;
        p.m_finiteProbabilityPerProjectedSolidAngleInv = false;
    }
    return p;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterialStandard::ProbabilityDensityForGeneratedEmission(const ysVec4& emittedDirectionLS) const
{
    ysDirectionalProbabilityDensity p;
    if (emittedDirectionLS.z > 0.0f)
    {
        p.m_probabilityPerSolidAngle = 1.0f / ys_2pi;
        p.m_probabilityPerSolidAngleInv = ys_2pi;
        p.m_finiteProbabilityPerSolidAngle = true;
        p.m_finiteProbabilityPerSolidAngleInv = true;
        if (emittedDirectionLS.z > ys_zeroSafe)
        {
            p.m_probabilityPerProjectedSolidAngle = 1.0f / (emittedDirectionLS.z * ys_2pi);
            p.m_probabilityPerProjectedSolidAngleInv = emittedDirectionLS.z * ys_2pi;
            p.m_finiteProbabilityPerProjectedSolidAngle = true;
            p.m_finiteProbabilityPerProjectedSolidAngleInv = true;
        }
        else
        {
            p.m_probabilityPerProjectedSolidAngle = 1.0f;
            p.m_probabilityPerProjectedSolidAngleInv = emittedDirectionLS.z * ys_2pi;
            p.m_finiteProbabilityPerProjectedSolidAngle = false;
            p.m_finiteProbabilityPerProjectedSolidAngleInv = true;
        }
    }
    else
    {
        p.m_probabilityPerSolidAngle = 0.0f;
        p.m_probabilityPerSolidAngleInv = 0.0f;
        p.m_probabilityPerProjectedSolidAngle = 0.0f;
        p.m_probabilityPerProjectedSolidAngleInv = 0.0f;
        p.m_finiteProbabilityPerSolidAngle = true;
        p.m_finiteProbabilityPerSolidAngleInv = false;
        p.m_finiteProbabilityPerProjectedSolidAngle = true;
        p.m_finiteProbabilityPerProjectedSolidAngleInv = false;
    }
    return p;
}