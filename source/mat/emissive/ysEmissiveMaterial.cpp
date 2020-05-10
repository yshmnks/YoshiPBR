#include "ysEmissiveMaterial.h"
#include "ysEmissiveMaterialUniform.h"
#include "scene/ysScene.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysRadiance ysEmissiveMaterial::EvaluateRadiance(const ysScene* scene, const ysVec4& w) const
{
    ysRadiance L;
    switch (m_type)
    {
        case Type::e_uniform:
        {
            const ysEmissiveMaterialUniform& subMat = scene->m_emissiveMaterialUniforms[m_typeIndex];
            L = subMat.EvaluateRadiance(w);
            break;
        }
        default:
        {
            ysAssert(false);
            L.SetInvalid();
            break;
        }
    }
    ysAssert(L.IsValid());
    return L;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysIrradiance ysEmissiveMaterial::EvaluateIrradiance(const ysScene* scene) const
{
    ysIrradiance E;
    switch (m_type)
    {
        case Type::e_uniform:
        {
            const ysEmissiveMaterialUniform& subMat = scene->m_emissiveMaterialUniforms[m_typeIndex];
            E = subMat.EvaluateIrradiance();
            break;
        }
        default:
        {
            ysAssert(false);
            E.SetInvalid();
            break;
        }
    }
    ysAssert(E.IsValid());
    // This is a material for a finite shape, so emission cannot emanate from an infinitesimal area element. Only things like point lights
    // and line lights can have unbounded irradiance.
    ysAssert(E.m_isFinite);
    return E;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysEmissiveMaterial::GenerateRandomDirection(const ysScene* scene, ysVec4* w, ysRadiance* L) const
{
    ysDirectionalProbabilityDensity p;
    switch (m_type)
    {
        case Type::e_uniform:
        {
            const ysEmissiveMaterialUniform& subMat = scene->m_emissiveMaterialUniforms[m_typeIndex];
            p = subMat.GenerateRandomDirection(w, L);
            break;
        }
        default:
        {
            ysAssert(false);
            p.SetInvalid();
            L->SetInvalid();
            break;
        }
    }
    ysAssert(p.IsValid() && L->IsValid());
    ysAssert(ysIsApproximatelyNormalized3(*w));
    return p;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysEmissiveMaterial::ProbabilityDensityForGeneratedDirection(const ysScene* scene, const ysVec4& w) const
{
    ysDirectionalProbabilityDensity p;
    switch (m_type)
    {
        case Type::e_uniform:
        {
            const ysEmissiveMaterialUniform& subMat = scene->m_emissiveMaterialUniforms[m_typeIndex];
            p = subMat.ProbabilityDensityForGeneratedDirection(w);
            break;
        }
        default:
        {
            ysAssert(false);
            p.SetInvalid();
            break;
        }
    }
    ysAssert(p.IsValid());
    return p;
}