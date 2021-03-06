#pragma once

#include "YoshiPBR/ysMath.h"
#include "common/ysProbability.h"
#include "common/ysRadiometry.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysMaterialStandard
{
    ysBSDF EvaluateBRDF(const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const;
    ysDirectionalProbabilityDensity GenerateRandomDirection(const ysVec4& inLS, ysVec4* outLS, ysBSDF* f) const;
    ysDirectionalProbabilityDensity GenerateRandomDirection(ysVec4* inLS, const ysVec4& outLS, ysBSDF* f) const;
    ysDirectionalProbabilityDensity GenerateRandomDirection(const ysVec4& inLS, ysVec4* outLS, ysBSDF* f, ysDirectionalProbabilityDensity* pReverse) const;
    ysDirectionalProbabilityDensity GenerateRandomDirection(ysVec4* inLS, const ysVec4& outLS, ysBSDF* f, ysDirectionalProbabilityDensity* pReverse) const;
    ysDirectionalProbabilityDensity ProbabilityDensityForGeneratedIncomingDirection(const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const;
    ysDirectionalProbabilityDensity ProbabilityDensityForGeneratedOutgoingDirection(const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const;

    ysVec4 m_albedoDiffuse;
    ysVec4 m_albedoSpecular;
};