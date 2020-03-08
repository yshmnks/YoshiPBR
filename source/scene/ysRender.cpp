#include "ysRender.h"

#include "YoshiPBR/ysLock.h"

#include "scene/ysScene.h"

#include <thread>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysRender::Reset()
{
    m_scene = nullptr;
    m_pixels = nullptr;
    m_pixelCount = 0;
    m_interruptLock = nullptr;
    m_state = State::e_pending;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysRender::Create(const ysScene* scene, const ysSceneRenderInput& input)
{
    m_scene = scene;
    m_input = input;

    m_pixelCount = input.m_pixelCountX * input.m_pixelCountY;
    m_pixels = static_cast<Pixel*>(ysMalloc(sizeof(Pixel) * m_pixelCount));
    for (ys_int32 i = 0; i < m_pixelCount; ++i)
    {
        m_pixels[i].m_value = ysVec4_zero;
        m_pixels[i].m_isNull = true;
    }

    m_interruptLock = ysNew(ysLock);

    m_state = State::e_initialized;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysRender::Destroy()
{
    ysFree(m_pixels);
    ysDelete(m_interruptLock);
    Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysRender::DoWork()
{
    ysAssert(m_state == State::e_initialized);

    {
        ysScopedLock scoped(m_interruptLock);
        m_state = State::e_working;
    }

    m_scene->DoRenderWork(this);

    {
        ysScopedLock scoped(m_interruptLock);
        m_state = State::e_finished;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysRender::GetOutputIntermediate(ysSceneRenderOutputIntermediate* output) const
{
    ysAssert(m_state == State::e_working || m_state == State::e_finished);
    output->m_pixels.SetCount(m_pixelCount);

    ysScopedLock scoped(m_interruptLock);

    for (ys_int32 i = 0; i < m_pixelCount; ++i)
    {
        ysFloat4& intermediatePixel = output->m_pixels[i];
        const ysRender::Pixel& workingPixel = m_pixels[i];
        if (workingPixel.m_isNull)
        {
            intermediatePixel.r = 0.0f;
            intermediatePixel.g = 0.0f;
            intermediatePixel.b = 0.0f;
            intermediatePixel.a = 0.0f;
        }
        else
        {
            intermediatePixel.r = workingPixel.m_value.x;
            intermediatePixel.g = workingPixel.m_value.y;
            intermediatePixel.b = workingPixel.m_value.z;
            intermediatePixel.a = 1.0f;
        }
    }

    if (m_interruptLock != nullptr)
    {
        m_interruptLock->Release();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysRender::GetOutputFinal(ysSceneRenderOutput* output)
{
    ysAssert(m_state == State::e_finished);
    output->m_pixels.SetCount(m_pixelCount);
    ysFloat3* outPixels = output->m_pixels.GetEntries();

    // Tone Mapping
    switch (m_input.m_renderMode)
    {
        case ysSceneRenderInput::RenderMode::e_regular:
        {
            for (ys_int32 i = 0; i < m_pixelCount; ++i)
            {
                outPixels[i].r = m_pixels[i].m_value.x;
                outPixels[i].g = m_pixels[i].m_value.y;
                outPixels[i].b = m_pixels[i].m_value.z;
            }
            break;
        }
        case ysSceneRenderInput::RenderMode::e_normals:
        {
            for (ys_int32 i = 0; i < m_pixelCount; ++i)
            {
                outPixels[i].r = m_pixels[i].m_value.x;
                outPixels[i].g = m_pixels[i].m_value.y;
                outPixels[i].b = m_pixels[i].m_value.z;
            }
            break;
        }
        case ysSceneRenderInput::RenderMode::e_depth:
        {
            ys_float32 minDepth = ys_maxFloat;
            ys_float32 maxDepth = 0.0f;
            for (ys_int32 i = 0; i < m_pixelCount; ++i)
            {
                ysRender::Pixel* pixel = m_pixels + i;
                ys_float32 depth = pixel->m_value.x;
                if (depth < 0.0f)
                {
                    continue;
                }
                minDepth = ysMin(minDepth, depth);
                maxDepth = ysMax(maxDepth, depth);
            }

            for (ys_int32 i = 0; i < m_pixelCount; ++i)
            {
                ysFloat3* outPixel = outPixels + i;

                ysRender::Pixel* pixel = m_pixels + i;
                ys_float32 depth = pixel->m_value.x;
                if (depth < 0.0f)
                {
                    outPixel->r = 1.0f;
                    outPixel->g = 0.0f;
                    outPixel->b = 0.0f;
                }
                else
                {
                    ys_float32 normalizedDepth = (depth - minDepth) / (maxDepth - minDepth);
                    outPixel->r = normalizedDepth;
                    outPixel->g = normalizedDepth;
                    outPixel->b = normalizedDepth;
                }
            }
            break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static ysRender* sGetRenderFromId(ysRenderId id)
{
    ysScene* scene = ysScene::s_scenes[id.m_sceneIdx];
    ysAssert(scene != nullptr);
    ysRender& render = scene->m_renders[id.m_index];
    ysAssert(render.m_poolIndex == id.m_index);
    return &render;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysRender_BeginWork(ysRenderId id)
{
    ysRender* render = sGetRenderFromId(id);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysRender_GetIntermediateOutput(ysRenderId id, ysSceneRenderOutputIntermediate* output)
{
    ysRender* render = sGetRenderFromId(id);
    render->GetOutputIntermediate(output);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ysRender_WorkFinished(ysRenderId id)
{
    ysRender* render = sGetRenderFromId(id);
    return render->m_state == ysRender::State::e_finished;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysRender_GetFinalOutput(ysRenderId id, ysSceneRenderOutput* output)
{
    ysRender* render = sGetRenderFromId(id);
    render->GetOutputFinal(output);
}