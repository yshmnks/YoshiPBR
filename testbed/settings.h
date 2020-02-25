#pragma once

#include "YoshiPBR/ysTypes.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct TestbedSettings
{
    TestbedSettings();

    bool m_showUI;
    bool m_drawBVH;
    bool m_drawGeo;
    bool m_drawRender;
    ys_int32 m_drawBVHDepth;

    ys_float32 m_moveSpeed;
};

extern TestbedSettings g_settings;