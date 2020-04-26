#pragma once

#include "YoshiPBR/ysMath.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysMaterialStandard
{
    ysVec4 EvaluateBRDF(const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const;
    ysVec4 EvaluateEmittedRadiance(const ysVec4& direction, const ysVec4& normal, const ysVec4& tangent) const;
    ysVec4 EvaluateEmittedIrradiance() const;

    bool IsEmissive() const;

    void GenerateRandomDirection(ysVec4* outgoingDirectionLS, ys_float32* probabilityDensity, const ysVec4& incomingDirectionLS) const;
    void GenerateRandomEmission(ysVec4* emittedDirectionLS, ys_float32* probabilityDensity) const;
    ys_float32 ProbabilityDensityForGeneratedDirection(const ysVec4& outgoingDirectionLS, const ysVec4& incomingDirectionLS) const;

    ysVec4 m_albedoDiffuse;
    ysVec4 m_albedoSpecular;

    ysVec4 m_emissiveDiffuse;
};