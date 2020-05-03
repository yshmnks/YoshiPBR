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
ysVec4 ysMaterial::EvaluateEmittedRadiance(const ysScene* scene, const ysVec4& direction, const ysVec4& normal, const ysVec4& tangent) const
{
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            return subMat.EvaluateEmittedRadiance(direction, normal, tangent);
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
void ysMaterial::GenerateRandomDirection(const ysScene* scene,
    const ysVec4& incomingDirectionLS, ysVec4* outgoingDirectionLS, ys_float32* probabilityDensity) const
{
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            subMat.GenerateRandomDirection(incomingDirectionLS, outgoingDirectionLS, probabilityDensity);
            break;
        }
        default:
            ysAssert(false);
    }
    ysAssert(*probabilityDensity >= 0.0f);
    ysAssert(*probabilityDensity == 0.0f || ysIsApproximatelyNormalized3(*outgoingDirectionLS));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysMaterial::GenerateRandomDirection(const ysScene* scene,
    ysVec4* incomingDirectionLS, const ysVec4& outgoingDirectionLS, ys_float32* probabilityDensity) const
{
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            subMat.GenerateRandomDirection(incomingDirectionLS, outgoingDirectionLS, probabilityDensity);
            break;
        }
        default:
            ysAssert(false);
    }
    ysAssert(*probabilityDensity >= 0.0f);
    ysAssert(*probabilityDensity == 0.0f || ysIsApproximatelyNormalized3(*incomingDirectionLS));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysMaterial::GenerateRandomEmission(const ysScene* scene, ysVec4* emittedDirectionLS, ys_float32* probabilityDensity) const
{
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            subMat.GenerateRandomEmission(emittedDirectionLS, probabilityDensity);
            break;
        }
        default:
            ysAssert(false);
    }
    ysAssert(*probabilityDensity >= 0.0f);
    ysAssert(*probabilityDensity == 0.0f || ysIsApproximatelyNormalized3(*emittedDirectionLS));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ys_float32 ysMaterial::ProbabilityDensityForGeneratedDirection(const ysScene* scene,
    const ysVec4& outgoingDirectionLS, const ysVec4& incomingDirectionLS) const
{
    ys_float32 probDens;
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            probDens = subMat.ProbabilityDensityForGeneratedDirection(outgoingDirectionLS, incomingDirectionLS);
            break;
        }
        default:
        {
            ysAssert(false);
            probDens = 0.0f;
            break;
        }
    }
    ysAssert(probDens >= 0.0f);
    return probDens;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ys_float32 ysMaterial::ProbabilityDensityForGeneratedEmission(const ysScene* scene, const ysVec4& emittedDirectionLS) const
{
    ys_float32 probDens;
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            probDens = subMat.ProbabilityDensityForGeneratedEmission(emittedDirectionLS);
            break;
        }
        default:
        {
            ysAssert(false);
            probDens = 0.0f;
            break;
        }
    }
    ysAssert(probDens >= 0.0f);
    return probDens;
}