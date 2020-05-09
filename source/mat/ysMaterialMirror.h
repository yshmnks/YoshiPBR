#pragma once

#include "YoshiPBR/ysMath.h"
#include "common/ysProbability.h"
#include "common/ysRadiometry.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysMaterialMirror
{
    ysBSDF EvaluateBRDF(const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const;
    ysRadiance EvaluateEmittedRadiance(const ysVec4& directionLS) const;
    ysIrradiance EvaluateEmittedIrradiance() const;

    bool IsEmissive() const { return false; }

    ysDirectionalProbabilityDensity GenerateRandomDirection(const ysVec4& inLS, ysVec4* outLS, ysBSDF* f) const;
    ysDirectionalProbabilityDensity GenerateRandomDirection(ysVec4* inLS, const ysVec4& outLS, ysBSDF* f) const;
    ysDirectionalProbabilityDensity GenerateRandomDirection(const ysVec4& inLS, ysVec4* outLS, ysBSDF* f, ysDirectionalProbabilityDensity* pReverse) const;
    ysDirectionalProbabilityDensity GenerateRandomDirection(ysVec4* inLS, const ysVec4& outLS, ysBSDF* f, ysDirectionalProbabilityDensity* pReverse) const;
    ysDirectionalProbabilityDensity GenerateRandomEmission(ysVec4* outLS, ysRadiance* L) const;
    ysDirectionalProbabilityDensity ProbabilityDensityForGeneratedIncomingDirection(const ysVec4& inLS, const ysVec4& outLS) const;
    ysDirectionalProbabilityDensity ProbabilityDensityForGeneratedOutgoingDirection(const ysVec4& inLS, const ysVec4& outLS) const;
    ysDirectionalProbabilityDensity ProbabilityDensityForGeneratedEmission(const ysVec4& outLS) const;
};