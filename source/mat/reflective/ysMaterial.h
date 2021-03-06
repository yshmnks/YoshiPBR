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
        e_standard,
        e_mirror,
    };

    // Input incoming/outgoing directions are both assumed to point away from the surface
    // Directions are in the local space of the surface element with [xHat, yHat, zHat] = [tangent, bitangnet, normal]
    ysBSDF EvaluateBRDF(const ysScene* scene, const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const;

    ysDirectionalProbabilityDensity GenerateRandomDirection(const ysScene* scene,
        const ysVec4& incomingDirectionLS, ysVec4* outgoingDirectionLS, ysBSDF* bsdf) const;

    ysDirectionalProbabilityDensity GenerateRandomDirection(const ysScene* scene,
        ysVec4* incomingDirectionLS, const ysVec4& outgoingDirectionLS, ysBSDF* bsdf) const;

    // These APIs also return the probability of generating the scattering event in the "reverse" direction.
    // For non-specular probability distributions, this is equivalent to calling the appropriate ProbabilityDensityForGenerated...Direction.
    // However, extremely specular distributions will likely miss the spike with the aforementioned API and return 0.
    // Better to simply compute the "reverse" probability at the time of direction-generation.
    ysDirectionalProbabilityDensity GenerateRandomDirection(const ysScene* scene,
        const ysVec4& inLS, ysVec4* outLS, ysBSDF* f, ysDirectionalProbabilityDensity* pReverse) const;
    ysDirectionalProbabilityDensity GenerateRandomDirection(const ysScene* scene,
        ysVec4* inLS, const ysVec4& outLS, ysBSDF* f, ysDirectionalProbabilityDensity* pReverse) const;

    ysDirectionalProbabilityDensity ProbabilityDensityForGeneratedIncomingDirection(const ysScene*,
        const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const;
    ysDirectionalProbabilityDensity ProbabilityDensityForGeneratedOutgoingDirection(const ysScene*,
        const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const;

    Type m_type;
    ys_int32 m_typeIndex;
};