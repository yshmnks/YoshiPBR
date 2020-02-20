#pragma once

#include "YoshiPBR/ysTypes.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysShapeId
{
    ys_int32 m_index;
};

extern const ysShapeId ys_nullShapeId;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysShape
{
    enum Type
    {
        e_triangle,
    };

    Type m_type;
    ys_int32 m_typeIndex;
};