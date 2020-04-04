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
    ysVec4* outgoingDirectionLS, ys_float32* probabilityDensity, const ysVec4& incomingDirectionLS) const
{
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            subMat.GenerateRandomDirection(outgoingDirectionLS, probabilityDensity, incomingDirectionLS);
            break;
        }
        default:
            ysAssert(false);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ys_float32 ysMaterial::ProbabilityDensityForGeneratedDirection(const ysScene* scene,
    const ysVec4& outgoingDirectionLS, const ysVec4& incomingDirectionLS) const
{
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            return subMat.ProbabilityDensityForGeneratedDirection(outgoingDirectionLS, incomingDirectionLS);
        }
        default:
            ysAssert(false);
            return 0.0f;
    }
}