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

    void GenerateRandomDirection(const ysScene* scene,
        ysVec4* outgoingDirectionLS, ys_float32* probabilityDensity, const ysVec4& incomingDirectionLS) const;

    Type m_type;
    ys_int32 m_typeIndex;
};