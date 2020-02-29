#include "ysMaterial.h"
#include "ysMaterialStandard.h"
#include "YoshiPBR/ysScene.h"

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
void ysMaterial::GenerateRandomDirection(const ysScene* scene,
    ysVec4* outgoingDirectionLS, ys_float32* probabilityDensity, const ysVec4& incomingDirectionLS) const
{
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            subMat.GenerateRandomDirection(outgoingDirectionLS, probabilityDensity, incomingDirectionLS);
        }
        default:
            ysAssert(false);;
    }
}