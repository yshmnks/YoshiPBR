#pragma once

#include "YoshiPBR/ysAABB.h"

struct ysRayCastInput;
struct ysRayCastOutput;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysTriangle
{
    ysAABB ComputeAABB() const;
    bool RayCast(ysRayCastOutput*, const ysRayCastInput&) const;
    void ConvertFromWSToLS(ysVec4* posLS, ysVec4* dirLS, const ysVec4& posWS, const ysVec4& dirWS) const;
    void ConvertFromLSToWS(ysVec4* posWS, ysVec4* dirWS, const ysVec4& posLS, const ysVec4& dirLS) const;

    ysVec4 m_v[3]; // vertices
    ysVec4 m_n;    // normal
    ysVec4 m_t;    // tangent
    bool m_twoSided;
};