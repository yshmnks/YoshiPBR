#pragma once

#include "YoshiPBR/ysTypes.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysLight
{
    enum Type
    {
        e_point
    };

    Type m_type;
    ys_int32 m_typeIndex;
};