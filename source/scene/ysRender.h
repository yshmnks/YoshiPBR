#pragma once

#include "YoshiPBR/ysArrayG.h"
#include "YoshiPBR/ysLock.h"
#include "YoshiPBR/ysStructures.h"

#include <atomic>
#include <thread>

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
        e_terminated,
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
    void GetOutputIntermediate(ysSceneRenderOutputIntermediate*);
    void GetOutputFinal(ysSceneRenderOutput*);
    void Terminate();

    const ysScene* m_scene;
    ysSceneRenderInput m_input;

    Pixel* m_pixels;
    ys_int32 m_pixelCount;

    // TODO: Figure out how to allocate these on the stack.
    ysLock m_interruptLock;
    std::thread* m_worker;

    std::atomic<State> m_state;

    union
    {
        ys_int32 m_poolIndex;
        ys_int32 m_poolNext;
    };
};