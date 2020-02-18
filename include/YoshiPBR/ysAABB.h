#pragma once

#include "YoshiPBR/ysMath.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysAABB
{
    void SetInvalid()
    {
        m_min = ysVec4_maxFloat;
        m_max = -ysVec4_maxFloat;
    }

    ysVec4 m_min;
    ysVec4 m_max;
};