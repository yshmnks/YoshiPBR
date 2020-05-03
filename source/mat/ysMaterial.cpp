#include "ysMaterial.h"
#include "ysMaterialStandard.h"
#include "scene/ysScene.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysVec4 ysMaterial::EvaluateBRDF(const ysScene* scene, const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const
{
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            return subMat.EvaluateBRDF(incomingDirectionLS, outgoingDirectionLS);
        }
        default:
            ysAssert(false);
            return ysVec4_zero;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysVec4 ysMaterial::EvaluateEmittedRadiance(const ysScene* scene, const ysVec4& direction) const
{
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            return subMat.EvaluateEmittedRadiance(direction);
        }
        default:
            ysAssert(false);
            return ysVec4_zero;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysVec4 ysMaterial::EvaluateEmittedIrradiance(const ysScene* scene) const
{
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            return subMat.EvaluateEmittedIrradiance();
        }
        default:
            ysAssert(false);
            return ysVec4_zero;
    }
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
    const ysVec4& incomingDirectionLS, ysVec4* outgoingDirectionLS) const
{
    ysDirectionalProbabilityDensity p;
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            p = subMat.GenerateRandomDirection(incomingDirectionLS, outgoingDirectionLS);
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
    ysAssert(p.m_probabilityPerSolidAngle == 0.0f || ysIsApproximatelyNormalized3(*outgoingDirectionLS));
    return p;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterial::GenerateRandomDirection(const ysScene* scene,
    ysVec4* incomingDirectionLS, const ysVec4& outgoingDirectionLS) const
{
    ysDirectionalProbabilityDensity p;
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            p = subMat.GenerateRandomDirection(incomingDirectionLS, outgoingDirectionLS);
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
    ysAssert(p.m_probabilityPerSolidAngle == 0.0f || ysIsApproximatelyNormalized3(*incomingDirectionLS));
    return p;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterial::GenerateRandomEmission(const ysScene* scene, ysVec4* emittedDirectionLS) const
{
    ysDirectionalProbabilityDensity p;
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            p = subMat.GenerateRandomEmission(emittedDirectionLS);
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
    ysAssert(p.m_probabilityPerSolidAngle == 0.0f || ysIsApproximatelyNormalized3(*emittedDirectionLS));
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