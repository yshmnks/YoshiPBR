#pragma once

#include "YoshiPBR/ysMath.h"
#include "common/ysProbability.h"
#include "common/ysRadiometry.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysEmissiveMaterialUniform
{
    ysRadiance EvaluateRadiance(const ysVec4& outLS) const;
    ysIrradiance EvaluateIrradiance() const;
    ysDirectionalProbabilityDensity GenerateRandomDirection(ysVec4* outLS, ysRadiance* L) const;
    ysDirectionalProbabilityDensity ProbabilityDensityForGeneratedDirection(const ysVec4& outLS) const;

    ysVec4 m_radiance;
};