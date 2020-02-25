#pragma once

#include "YoshiPBR/ysMath.h"

struct ysMaterialStandard
{
    ysVec4 EvaluateBRDF(const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const;

    ysVec4 m_albedoDiffuse;
    ysVec4 m_albedoSpecular;
};