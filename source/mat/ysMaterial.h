#pragma once

#include "YoshiPBR/ysMath.h"
#include "common/ysProbability.h"
#include "common/ysRadiometry.h"

struct ysScene;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysMaterial
{
    enum Type
    {
        e_standard
    };

    // Input incoming/outgoing directions are both assumed to point away from the surface
    // Directions are in the local space of the surface element with [xHat, yHat, zHat] = [tangent, bitangnet, normal]
    ysBSDF EvaluateBRDF(const ysScene* scene, const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const;

    // Get the radiance emitted in 'direction' from a surface element oriented with 'normal' and 'tangent'
    ysRadiance EvaluateEmittedRadiance(const ysScene*, const ysVec4& directionLS) const;

    // Get the emitted irradiance. This is the integral of the emitted radiance over all projected solid angles.
    ysIrradiance EvaluateEmittedIrradiance(const ysScene*) const;

    bool IsEmissive(const ysScene*) const;

    ysDirectionalProbabilityDensity GenerateRandomDirection(const ysScene* scene,
        const ysVec4& incomingDirectionLS, ysVec4* outgoingDirectionLS, ysBSDF* bsdf) const;

    ysDirectionalProbabilityDensity GenerateRandomDirection(const ysScene* scene,
        ysVec4* incomingDirectionLS, const ysVec4& outgoingDirectionLS, ysBSDF* bsdf) const;

    ysDirectionalProbabilityDensity GenerateRandomEmission(const ysScene* scene, ysVec4* emittedDirectionLS, ysRadiance* rad) const;

    ysDirectionalProbabilityDensity ProbabilityDensityForGeneratedIncomingDirection(const ysScene*,
        const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const;
    ysDirectionalProbabilityDensity ProbabilityDensityForGeneratedOutgoingDirection(const ysScene*,
        const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const;

    ysDirectionalProbabilityDensity ProbabilityDensityForGeneratedEmission(const ysScene*, const ysVec4& emittedDirectionLS) const;

    Type m_type;
    ys_int32 m_typeIndex;
};