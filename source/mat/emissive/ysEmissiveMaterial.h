#pragma once

#include "YoshiPBR/ysMath.h"
#include "common/ysProbability.h"
#include "common/ysRadiometry.h"

struct ysScene;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysEmissiveMaterial
{
    enum Type
    {
        e_uniform,
    };

    // Get the radiance emitted in the specified direction
    ysRadiance EvaluateRadiance(const ysScene*, const ysVec4& directionLS) const;

    // Get the emitted irradiance. This is the integral of the emitted radiance over all projected solid angles.
    ysIrradiance EvaluateIrradiance(const ysScene*) const;

    ysDirectionalProbabilityDensity GenerateRandomDirection(const ysScene* scene, ysVec4* emittedDirectionLS, ysRadiance* rad) const;
    ysDirectionalProbabilityDensity ProbabilityDensityForGeneratedDirection(const ysScene*, const ysVec4& emittedDirectionLS) const;

    Type m_type;
    ys_int32 m_typeIndex;
};