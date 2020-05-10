#include "ysEmissiveMaterialUniform.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysRadiance ysEmissiveMaterialUniform::EvaluateRadiance(const ysVec4&) const
{
    ysRadiance L;
    L.m_value = m_radiance;
    L.m_isFinite = true;
    return L;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysIrradiance ysEmissiveMaterialUniform::EvaluateIrradiance() const
{
    ysIrradiance E;
    E.m_value = m_radiance * ysVec4_pi;
    E.m_isFinite = true;
    return E;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysEmissiveMaterialUniform::GenerateRandomDirection(ysVec4* w, ysRadiance* L) const
{
    ys_float32 phi = ys_2pi * ysRandom(0.0f, 1.0f);
    ys_float32 cosTheta = ysRandom(0.0f, 1.0f);
    ys_float32 sinTheta = sqrtf(ysMax(0.0f, 1.0f - cosTheta * cosTheta));
    *w = ysVecSet(sinTheta * cosf(phi), sinTheta * sinf(phi), cosTheta);
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
    L->m_value = m_radiance;
    L->m_isFinite = true;
    return p;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysEmissiveMaterialUniform::ProbabilityDensityForGeneratedDirection(const ysVec4& w) const
{
    ysDirectionalProbabilityDensity p;
    if (w.z > 0.0f)
    {
        p.m_perSolidAngle.m_value = 1.0f / ys_2pi;
        p.m_perSolidAngle.m_isFinite = true;
        if (w.z > ys_zeroSafe)
        {
            p.m_perProjectedSolidAngle.m_value = 1.0f / (w.z * ys_2pi);
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