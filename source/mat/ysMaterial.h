#pragma once

#include "YoshiPBR/ysMath.h"

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
    ysVec4 EvaluateBRDF(const ysScene* scene, const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const;

    // Get the radiance emitted in 'direction' from a surface element oriented with 'normal' and 'tangent'
    ysVec4 EvaluateEmittedRadiance(const ysScene*, const ysVec4& direction, const ysVec4& normal, const ysVec4& tangent) const;

    // Get the emitted irradiance. This is the integral of the emitted radiance over all projected solid angles.
    ysVec4 EvaluateEmittedIrradiance(const ysScene*) const;

    bool IsEmissive(const ysScene*) const;

    void GenerateRandomDirection(const ysScene* scene,
        const ysVec4& incomingDirectionLS, ysVec4* outgoingDirectionLS, ys_float32* probabilityDensity) const;

    void GenerateRandomDirection(const ysScene* scene,
        ysVec4* incomingDirectionLS, const ysVec4& outgoingDirectionLS, ys_float32* probabilityDensity) const;

    void GenerateRandomEmission(const ysScene* scene, ysVec4* emittedDirectionLS, ys_float32* probabilityDensity) const;

    ys_float32 ProbabilityDensityForGeneratedDirection(const ysScene*,
        const ysVec4& outgoingDirectionLS, const ysVec4& incomingDirectionLS) const;

    ys_float32 ProbabilityDensityForGeneratedEmission(const ysScene*, const ysVec4& emittedDirectionLS) const;

    Type m_type;
    ys_int32 m_typeIndex;
};