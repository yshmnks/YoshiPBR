#pragma once

#include "YoshiPBR/ysAABB.h"
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

    ysAABB ComputeAABB(const ysScene* scene) const;
    bool RayCast(const ysScene* scene, ysRayCastOutput*, const ysRayCastInput&) const;

    Type m_type;
    ys_int32 m_typeIndex;

    ysMaterialId m_materialId;
};