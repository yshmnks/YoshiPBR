#pragma once

#include "YoshiPBR/YoshiPBR.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ScenePreview
{
    void Create(ysScene* scene);
    void Destroy();

    void Draw();

    ysScene* m_scene;
};