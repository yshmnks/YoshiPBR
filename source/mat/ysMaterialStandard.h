#pragma once

#include "YoshiPBR/ysMath.h"
#include "common/ysProbability.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysMaterialStandard
{
    ysVec4 EvaluateBRDF(const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const;
    ysVec4 EvaluateEmittedRadiance(const ysVec4& directionLS) const;
    ysVec4 EvaluateEmittedIrradiance() const;

    bool IsEmissive() const;

    ysDirectionalProbabilityDensity GenerateRandomDirection(const ysVec4& incomingDirectionLS, ysVec4* outgoingDirectionLS) const;
    ysDirectionalProbabilityDensity GenerateRandomDirection(ysVec4* incomingDirectionLS, const ysVec4& outgoingDirectionLS) const;
    ysDirectionalProbabilityDensity GenerateRandomEmission(ysVec4* emittedDirectionLS) const;
    ysDirectionalProbabilityDensity ProbabilityDensityForGeneratedIncomingDirection(const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const;
    ysDirectionalProbabilityDensity ProbabilityDensityForGeneratedOutgoingDirection(const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const;
    ysDirectionalProbabilityDensity ProbabilityDensityForGeneratedEmission(const ysVec4& emittedDirectionLS) const;

    ysVec4 m_albedoDiffuse;
    ysVec4 m_albedoSpecular;

    ysVec4 m_emissiveDiffuse;
};