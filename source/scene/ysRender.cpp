#include "ysRender.h"

#include "YoshiPBR/ysLock.h"

#include "scene/ysScene.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysRender::Reset()
{
    m_scene = nullptr;
    m_pixels = nullptr;
    m_pixelCount = 0;
    m_interruptLock.Reset();
    m_state = State::e_pending;
    m_worker = nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysRender::Create(const ysScene* scene, const ysSceneRenderInput& input)
{
    m_scene = scene;

    {
        ///////////////////////////////////
        // Make a DEEP COPY of the input //
        ///////////////////////////////////
        m_input = input;
        m_input.m_giInput = nullptr;
        m_input.m_giInputCompare = nullptr;
        switch (input.m_renderMode)
        {
            case ysSceneRenderInput::RenderMode::e_compare:
            {
                ysAssert(input.m_giInput != nullptr && input.m_giInputCompare != nullptr);
                switch (input.m_giInputCompare->m_type)
                {
                    case ysGlobalIlluminationInput::Type::e_uniDirectional:
                        ysMemCpy(m_inputsUni + 1, input.m_giInputCompare, sizeof(ysGlobalIlluminationInput_UniDirectional));
                        m_input.m_giInputCompare = m_inputsUni + 1;
                        break;
                    case ysGlobalIlluminationInput::Type::e_biDirectional:
                        ysMemCpy(m_inputsBi + 1, input.m_giInputCompare, sizeof(ysGlobalIlluminationInput_BiDirectional));
                        m_input.m_giInputCompare = m_inputsBi + 1;
                        break;
                    default:
                        ysAssert(false);
                        break;
                }
                // Yes, there is no break here. We still need to copy the base GI input.
            }
            case ysSceneRenderInput::RenderMode::e_regular:
            {
                ysAssert(input.m_renderMode == ysSceneRenderInput::RenderMode::e_compare || input.m_giInputCompare == nullptr);
                switch (input.m_giInput->m_type)
                {
                    case ysGlobalIlluminationInput::Type::e_uniDirectional:
                        ysMemCpy(m_inputsUni + 0, input.m_giInput, sizeof(ysGlobalIlluminationInput_UniDirectional));
                        m_input.m_giInput = m_inputsUni + 0;
                        break;
                    case ysGlobalIlluminationInput::Type::e_biDirectional:
                        ysMemCpy(m_inputsBi + 0, input.m_giInput, sizeof(ysGlobalIlluminationInput_BiDirectional));
                        m_input.m_giInput = m_inputsBi + 0;
                        break;
                    default:
                        ysAssert(false);
                        break;
                }
                break;
            }
            default:
            {
                ysAssert(input.m_giInput == nullptr && input.m_giInputCompare == nullptr);
                break;
            }
        }
    }

    m_pixelCount = input.m_pixelCountX * input.m_pixelCountY;
    m_pixels = static_cast<Pixel*>(ysMalloc(sizeof(Pixel) * m_pixelCount));
    for (ys_int32 i = 0; i < m_pixelCount; ++i)
    {
        m_pixels[i].m_value = ysVec4_zero;
        m_pixels[i].m_isNull = true;
    }

    m_interruptLock.Reset();

    m_state = State::e_initialized;
    m_worker = nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysRender::Destroy()
{
    if (m_worker != nullptr)
    {
        if (m_worker->joinable())
        {
            m_worker->join();
        }
        ysDelete(m_worker);
    }
    ysFree(m_pixels);
    Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysRender::DoWork()
{
    ysAssert(m_state == State::e_initialized);
    m_state = State::e_working;
    m_scene->DoRenderWork(this);
    if (m_state == State::e_terminated)
    {
        return;
    }
    m_state = State::e_finished;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysRender::GetOutputIntermediate(ysSceneRenderOutputIntermediate* output)
{
    ysAssert(m_state == State::e_working || m_state == State::e_finished);
    output->m_pixels.SetCount(m_pixelCount);

    ysScopedLock scoped(&m_interruptLock);

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
        case ysSceneRenderInput::RenderMode::e_compare:
        {
            const ys_int32 kernelHalfWidthX = 8;
            const ys_int32 kernelHalfWidthY = 8;
            ys_float32 kernel[1 + 2 * kernelHalfWidthY][1 + 2 * kernelHalfWidthX];
            {
                // Just uniformly average the values for now.
                ys_float32 w = 1.0f / ((1.0f + 2.0f * kernelHalfWidthY) * (1.0f + 2.0f * kernelHalfWidthX));
                for (ys_int32 yIdx = -kernelHalfWidthY; yIdx <= kernelHalfWidthY; ++yIdx)
                {
                    ys_int32 i = yIdx + kernelHalfWidthY;
                    for (ys_int32 xIdx = -kernelHalfWidthX; xIdx <= kernelHalfWidthX; ++xIdx)
                    {
                        ys_int32 j = xIdx + kernelHalfWidthX;
                        kernel[i][j] = w;
                    }
                }
            }

            for (ys_int32 yDst = 0; yDst < m_input.m_pixelCountY; ++yDst)
            {
                for (ys_int32 xDst = 0; xDst < m_input.m_pixelCountX; ++xDst)
                {
                    ys_int32 pixelIdx = m_input.m_pixelCountX * yDst + xDst;

                    if (yDst < kernelHalfWidthY || m_input.m_pixelCountY - yDst <= kernelHalfWidthY ||
                        xDst < kernelHalfWidthX || m_input.m_pixelCountX - xDst <= kernelHalfWidthX)
                    {
                        outPixels[pixelIdx].r = m_pixels[pixelIdx].m_value.x + 0.5f;
                        outPixels[pixelIdx].g = m_pixels[pixelIdx].m_value.y + 0.5f;
                        outPixels[pixelIdx].b = m_pixels[pixelIdx].m_value.z + 0.5f;
                        continue;
                    }

                    ysVec4 p = ysVec4_zero;
                    for (ys_int32 dy = -kernelHalfWidthY; dy <= kernelHalfWidthY; ++dy)
                    {
                        ys_int32 iKernel = dy + kernelHalfWidthY;
                        ys_int32 ySrc = yDst + dy;
                        for (ys_int32 dx = -kernelHalfWidthX; dx <= kernelHalfWidthX; ++dx)
                        {
                            ys_int32 jKernel = dx + kernelHalfWidthX;
                            ys_int32 xSrc = xDst + dx;

                            ys_float32 w = kernel[iKernel][jKernel];
                            ysVec4 pSrc = m_pixels[m_input.m_pixelCountX * ySrc + xSrc].m_value;
                            p += ysSplat(w) * pSrc;
                        }
                    }

                    outPixels[pixelIdx].r = p.x + 0.5f;
                    outPixels[pixelIdx].g = p.y + 0.5f;
                    outPixels[pixelIdx].b = p.z + 0.5f;
                }
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
        default:
        {
            ysAssert(false);
            break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysRender::Terminate()
{
    ysAssert(m_state == State::e_working || m_state == State::e_finished);

    if (m_state == State::e_working)
    {
        m_state = State::e_terminated;
    }

    if (m_worker != nullptr)
    {
        ysAssert(m_worker->joinable());
        m_worker->join();
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
static void sRenderDoWork(ysRender* render)
{
    render->DoWork();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysRender_BeginWork(ysRenderId id)
{
    ysRender* render = sGetRenderFromId(id);
    render->m_worker = ysNew std::thread(sRenderDoWork, render);
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