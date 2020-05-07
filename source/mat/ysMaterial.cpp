#include "ysMaterial.h"
#include "ysMaterialStandard.h"
#include "scene/ysScene.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysBSDF ysMaterial::EvaluateBRDF(const ysScene* scene, const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const
{
    ysBSDF f;
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            f = subMat.EvaluateBRDF(incomingDirectionLS, outgoingDirectionLS);
            break;
        }
        default:
        {
            ysAssert(false);
            f.SetInvalid();
            break;
        }
    }
    ysAssert(f.IsValid());
    return f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysRadiance ysMaterial::EvaluateEmittedRadiance(const ysScene* scene, const ysVec4& direction) const
{
    ysRadiance L;
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            L = subMat.EvaluateEmittedRadiance(direction);
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
ysIrradiance ysMaterial::EvaluateEmittedIrradiance(const ysScene* scene) const
{
    ysIrradiance E;
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            E = subMat.EvaluateEmittedIrradiance();
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
bool ysMaterial::IsEmissive(const ysScene* scene) const
{
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            return subMat.IsEmissive();
        }
        default:
            ysAssert(false);
            return false;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterial::GenerateRandomDirection(const ysScene* scene,
    const ysVec4& incomingDirectionLS, ysVec4* outgoingDirectionLS, ysBSDF* f) const
{
    ysDirectionalProbabilityDensity p;
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            p = subMat.GenerateRandomDirection(incomingDirectionLS, outgoingDirectionLS, f);
            break;
        }
        default:
        {
            ysAssert(false);
            p.SetInvalid();
            f->SetInvalid();
            break;
        }
    }
    ysAssert(p.IsValid() && f->IsValid());
    ysAssert(p.m_perProjectedSolidAngle.m_isFinite == f->m_isFinite);
    ysAssert(ysIsApproximatelyNormalized3(*outgoingDirectionLS));
    return p;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterial::GenerateRandomDirection(const ysScene* scene,
    ysVec4* incomingDirectionLS, const ysVec4& outgoingDirectionLS, ysBSDF* f) const
{
    ysDirectionalProbabilityDensity p;
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            p = subMat.GenerateRandomDirection(incomingDirectionLS, outgoingDirectionLS, f);
            break;
        }
        default:
        {
            ysAssert(false);
            p.SetInvalid();
            f->SetInvalid();
            break;
        }
    }
    ysAssert(p.IsValid() && f->IsValid());
    ysAssert(p.m_perProjectedSolidAngle.m_isFinite == f->m_isFinite);
    ysAssert(ysIsApproximatelyNormalized3(*incomingDirectionLS));
    return p;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterial::GenerateRandomEmission(const ysScene* scene, ysVec4* emittedDirectionLS, ysRadiance* L) const
{
    ysDirectionalProbabilityDensity p;
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            p = subMat.GenerateRandomEmission(emittedDirectionLS, L);
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
    ysAssert(ysIsApproximatelyNormalized3(*emittedDirectionLS));
    return p;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterial::ProbabilityDensityForGeneratedIncomingDirection(const ysScene* scene,
    const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const
{
    ysDirectionalProbabilityDensity p;
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            p = subMat.ProbabilityDensityForGeneratedIncomingDirection(incomingDirectionLS, outgoingDirectionLS);
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterial::ProbabilityDensityForGeneratedOutgoingDirection(const ysScene* scene,
    const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const
{
    ysDirectionalProbabilityDensity p;
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            p = subMat.ProbabilityDensityForGeneratedOutgoingDirection(incomingDirectionLS, outgoingDirectionLS);
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterial::ProbabilityDensityForGeneratedEmission(const ysScene* scene, const ysVec4& emittedDirectionLS) const
{
    ysDirectionalProbabilityDensity p;
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            p = subMat.ProbabilityDensityForGeneratedEmission(emittedDirectionLS);
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