#pragma once

#include "YoshiPBR/ysStructures.h"
#include "YoshiPBR/ysArrayG.h"

struct ysLock;
struct ysScene;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysRender
{
    enum State
    {
        e_pending,
        e_initialized,
        e_working,
        e_finished,
    };

    struct Pixel
    {
        ysVec4 m_value;
        bool m_isNull;
    };

    void Reset();
    void Create(const ysScene*, const ysSceneRenderInput&);
    void Destroy();

    void DoWork();
    void GetOutputIntermediate(ysSceneRenderOutputIntermediate*) const;
    void GetOutputFinal(ysSceneRenderOutput*);

    const ysScene* m_scene;
    ysSceneRenderInput m_input;

    Pixel* m_pixels;
    ys_int32 m_pixelCount;

    // TODO: Figure out how to allocate this on the stack.
    ysLock* m_interruptLock;

    State m_state;

    union
    {
        ys_int32 m_poolIndex;
        ys_int32 m_poolNext;
    };
};