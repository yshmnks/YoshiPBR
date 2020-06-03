#pragma once

#include "YoshiPBR/ysArrayG.h"
#include "YoshiPBR/ysStructures.h"
#include "YoshiPBR/ysThreading.h"

#include <atomic>

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
    void Terminate(const ysScene*);

    const ysScene* m_scene;

    // Save off the input. This is a deep copy, which is why we have pre allocated some space for GI inputs.
    ysSceneRenderInput m_input;
    ysGlobalIlluminationInput_UniDirectional m_inputsUni[2];
    ysGlobalIlluminationInput_UniDirectional m_inputsBi[2];

    Pixel* m_pixels;
    Pixel* m_exposedPixels;
    ys_int32 m_pixelCount;

    ysLock m_interruptLock;

    std::atomic<State> m_state;

    union
    {
        ys_int32 m_poolIndex;
        ys_int32 m_poolNext;
    };
};