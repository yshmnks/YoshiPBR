#pragma once

#include "YoshiPBR/ysStructures.h"

struct ysRayCastInput;
struct ysRayCastOutput;
struct ysScene;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysShape
{
    enum Type
    {
        e_triangle,
    };

    bool RayCast(const ysScene* scene, ysRayCastOutput*, const ysRayCastInput&) const;
    void ConvertFromWSToLS(const ysScene*, ysVec4* posLS, ysVec4* dirLS, const ysVec4& posWS, const ysVec4& dirWS) const;
    void ConvertFromLSToWS(const ysScene*, ysVec4* posWS, ysVec4* dirWS, const ysVec4& posLS, const ysVec4& dirLS) const;

    Type m_type;
    ys_int32 m_typeIndex;

    ysMaterialId m_materialId;
};