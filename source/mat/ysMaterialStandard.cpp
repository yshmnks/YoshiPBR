#include "ysMaterialStandard.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysBSDF ysMaterialStandard::EvaluateBRDF(const ysVec4& w_i, const ysVec4& w_o) const
{
    YS_REF(w_i);
    YS_REF(w_o);
    ysBSDF f;
    f.m_value = m_albedoDiffuse / ysVec4_pi;
    f.m_isFinite = true;
    return f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysRadiance ysMaterialStandard::EvaluateEmittedRadiance(const ysVec4&) const
{
    ysRadiance L;
    L.m_value = m_emissiveDiffuse;
    L.m_isFinite = true;
    return L;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysIrradiance ysMaterialStandard::EvaluateEmittedIrradiance() const
{
    ysIrradiance E;
    E.m_value = m_emissiveDiffuse * ysVec4_pi;
    E.m_isFinite = true;
    return E;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ysMaterialStandard::IsEmissive() const
{
    return m_emissiveDiffuse != ysVec4_zero;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterialStandard::GenerateRandomDirection(const ysVec4&, ysVec4* outgoingLS, ysBSDF* f) const
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
    p.m_perSolidAngle.m_value = cosTheta / ys_pi;
    p.m_perSolidAngle.m_isFinite = true;
    p.m_perProjectedSolidAngle.m_value = 1.0f / ys_pi;
    p.m_perProjectedSolidAngle.m_isFinite = true;
    f->m_value = m_albedoDiffuse / ysVec4_pi;
    f->m_isFinite = true;
    return p;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterialStandard::GenerateRandomDirection(ysVec4* incomingLS, const ysVec4& outgoingLS, ysBSDF* f) const
{
    return GenerateRandomDirection(outgoingLS, incomingLS, f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterialStandard::GenerateRandomDirection(const ysVec4& w_i, ysVec4* w_o, ysBSDF* f, ysDirectionalProbabilityDensity* pReverse) const
{
    *pReverse = GenerateRandomDirection(w_i, w_o, f);
    return *pReverse;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterialStandard::GenerateRandomDirection(ysVec4* w_i, const ysVec4& w_o, ysBSDF* f, ysDirectionalProbabilityDensity* pReverse) const
{
    *pReverse = GenerateRandomDirection(w_i, w_o, f);
    return *pReverse;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterialStandard::GenerateRandomEmission(ysVec4* emittedDirectionLS, ysRadiance* L) const
{
    ys_float32 phi = ys_2pi * ysRandom(0.0f, 1.0f);
    ys_float32 cosTheta = ysRandom(0.0f, 1.0f);
    ys_float32 sinTheta = sqrtf(ysMax(0.0f, 1.0f - cosTheta * cosTheta));
    *emittedDirectionLS = ysVecSet(sinTheta * cosf(phi), sinTheta * sinf(phi), cosTheta);
    ysDirectionalProbabilityDensity p;
    p.m_perSolidAngle.m_value = 1.0f / ys_2pi;
    p.m_perSolidAngle.m_isFinite = true;
    if (cosTheta < ys_zeroSafe)
    {
        p.m_perProjectedSolidAngle.m_value = 1.0f;
        p.m_perProjectedSolidAngle.m_isFinite = false;
    }
    else
    {
        p.m_perProjectedSolidAngle.m_value = 1.0f / (cosTheta * ys_2pi);
        p.m_perProjectedSolidAngle.m_isFinite = true;
    }
    L->m_value = m_emissiveDiffuse;
    L->m_isFinite = true;
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
ysDirectionalProbabilityDensity ysMaterialStandard::ProbabilityDensityForGeneratedOutgoingDirection(const ysVec4&, const ysVec4& outLS) const
{
    ysAssert(ysAbs(ysLength3(outLS) - 1.0f) < 0.001f);
    ysDirectionalProbabilityDensity p;
    if (outLS.z > 0.0f)
    {
        p.m_perSolidAngle.m_value = outLS.z / ys_pi;
        p.m_perSolidAngle.m_isFinite = true;
        p.m_perProjectedSolidAngle.m_value = 1.0f / ys_pi;
        p.m_perProjectedSolidAngle.m_isFinite = true;
    }
    else
    {
        p.m_perSolidAngle.m_value = 0.0f;
        p.m_perSolidAngle.m_isFinite = true;
        p.m_perProjectedSolidAngle.m_value = 0.0f;
        p.m_perProjectedSolidAngle.m_isFinite = true;
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
        p.m_perSolidAngle.m_value = 1.0f / ys_2pi;
        p.m_perSolidAngle.m_isFinite = true;
        if (emittedDirectionLS.z > ys_zeroSafe)
        {
            p.m_perProjectedSolidAngle.m_value = 1.0f / (emittedDirectionLS.z * ys_2pi);
            p.m_perProjectedSolidAngle.m_isFinite = true;
        }
        else
        {
            p.m_perProjectedSolidAngle.m_value = 1.0f;
            p.m_perProjectedSolidAngle.m_isFinite = false;
        }
    }
    else
    {
        p.m_perSolidAngle.m_value = 0.0f;
        p.m_perSolidAngle.m_isFinite = true;
        p.m_perProjectedSolidAngle.m_value = 0.0f;
        p.m_perProjectedSolidAngle.m_isFinite = true;
    }
    return p;
}