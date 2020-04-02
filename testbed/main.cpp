#include "YoshiPBR/YoshiPBR.h"

#include "camera.h"
#include "draw.h"
#include "settings.h"

#include "glad/glad.h"
#include "glfw/glfw3.h"
#include "imgui/imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <crtdbg.h>
#include <stdio.h>

GLFWwindow* g_mainWindow = nullptr;
static ysSceneId s_sceneId = ys_nullSceneId;
static ysRenderId s_renderId = ys_nullRenderId;
static bool s_rightMouseDown = false;
static ys_float32 s_uiScale = 1.0f;
static ys_float32 s_mouseX = 0.0f;
static ys_float32 s_mouseY = 0.0f;

static ysArrayG<Color> s_pixels = ysArrayG<Color>();
static ys_int32 s_pixelCountX = 0;
static ys_int32 s_pixelCountY = 0;
static ysSceneRenderInput s_renderInput = ysSceneRenderInput();

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void glfwErrorCallback(int error, const char* description)
{
    fprintf(stderr, "GLFW error occured. Code: %d. Description: %s\n", error, description);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void sCreateUI(GLFWwindow* window, const char* glslVersion = nullptr)
{
    ys_float32 xscale, yscale;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    s_uiScale = xscale;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    bool success;
    success = ImGui_ImplGlfw_InitForOpenGL(window, false);
    if (success == false)
    {
        printf("ImGui_ImplGlfw_InitForOpenGL failed\n");
        assert(false);
    }

    success = ImGui_ImplOpenGL3_Init(glslVersion);
    if (success == false)
    {
        printf("ImGui_ImplOpenGL3_Init failed\n");
        assert(false);
    }

    const char* fontPath = "Data/DroidSans.ttf";
    ImGui::GetIO().Fonts->AddFontFromFileTTF(fontPath, s_uiScale * 14.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void sDestroyUI()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void sUpdateUI(ys_int32 windowWidth, ys_int32 windowHeight)
{
    int menuWidth = 256;
    if (g_settings.m_showUI)
    {
        ImGui::SetNextWindowPos(ImVec2((float)windowWidth - menuWidth - 10, 10));
        ImGui::SetNextWindowSize(ImVec2((float)menuWidth, (float)windowHeight - 20));
        ImGui::Begin("Tools", &g_settings.m_showUI, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        if (ImGui::BeginTabBar("ControlTabs", ImGuiTabBarFlags_None))
        {
            if (ImGui::BeginTabItem("Controls"))
            {
                ImGui::SliderAngle("FoV", &g_camera.m_verticalFov, 1.0f , 89.0f, "%.0f degrees");
                ImGui::SliderFloat("Speed", &g_settings.m_moveSpeed, 0.0f, 100.0f, "%.1f m/s");
                ImGui::Separator();

                ImGui::Checkbox("Draw Geo", &g_settings.m_drawGeo);
                ImGui::Checkbox("Draw Lights", &g_settings.m_drawLights);
                ImGui::Checkbox("Draw BVH", &g_settings.m_drawBVH);
                ImGui::SameLine();
                ImGui::SliderInt("Depth", &g_settings.m_drawBVHDepth, -1, ysScene_GetBVHDepth(s_sceneId) - 1);
                ImGui::Separator();

                ImGui::Checkbox("Draw Render", &g_settings.m_drawRender);
                ImGui::SameLine();
                ImVec2 button_sz = ImVec2(-1, 0);
                if (ImGui::Button("Render", button_sz))
                {
                    ys_float32 aspectRatio = (ys_float32)windowWidth / (ys_float32)windowHeight;
                    ys_int32 xCount = ys_int32(aspectRatio * windowHeight);
                    ys_int32 yCount = windowHeight;

                    s_renderInput.m_eye = g_camera.ComputeEyeTransform();
                    s_renderInput.m_fovY = g_camera.m_verticalFov;
                    s_renderInput.m_pixelCountY = yCount;
                    s_renderInput.m_pixelCountX = xCount;

                    if (s_renderId != ys_nullRenderId)
                    {
                        ysScene_DestroyRender(s_renderId);
                    }
                    s_renderId = ysScene_CreateRender(s_sceneId, s_renderInput);
                    ysRender_BeginWork(s_renderId);

                    s_pixels.SetCount(xCount * yCount);
                    s_pixelCountX = xCount;
                    s_pixelCountY = yCount;
                }

                ImGui::SliderInt("Bounce Count", &s_renderInput.m_maxBounceCount, 0, 8);
                ImGui::SliderInt("Samples Per Pixel", &s_renderInput.m_samplesPerPixel, 1, 1000);

                const char* renderModes[] = { "Global Illumination", "Normals", "Depth"};
                static int selectedRenderMode = 0;
                ImGui::Combo("RenderMode", &selectedRenderMode, renderModes, 3);
                switch (selectedRenderMode)
                {
                    case 0:
                        s_renderInput.m_renderMode = ysSceneRenderInput::RenderMode::e_regular;
                        break;
                    case 1:
                        s_renderInput.m_renderMode = ysSceneRenderInput::RenderMode::e_normals;
                        break;
                    case 2:
                        s_renderInput.m_renderMode = ysSceneRenderInput::RenderMode::e_depth;
                        break;
                }

                ImGui::Separator();

                if (ImGui::Button("Quit", button_sz))
                {
                    glfwSetWindowShouldClose(g_mainWindow, GL_TRUE);
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void sResizeWindowCallback(GLFWwindow*, int width, int height)
{
    g_camera.m_aspectRatio = (ys_float32)width / (ys_float32)height;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void sKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
    if (ImGui::GetIO().WantCaptureKeyboard)
    {
        return;
    }

    if (action == GLFW_PRESS)
    {
        switch (key)
        {
            case GLFW_KEY_ESCAPE:   // Quit
                glfwSetWindowShouldClose(g_mainWindow, GL_TRUE);
                break;
            case GLFW_KEY_TAB:      // Toggle UI visibility
                g_settings.m_showUI = !g_settings.m_showUI;
                break;
            default:
                break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void sCharCallback(GLFWwindow* window, unsigned int c)
{
    ImGui_ImplGlfw_CharCallback(window, c);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void sMouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void sMouseMotionCallback(GLFWwindow* window, double xd, double yd)
{
    ys_float32 x = (ys_float32)xd;
    ys_float32 y = (ys_float32)yd;

    bool rightMouseButtonHeld = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
    if (rightMouseButtonHeld)
    {
        const ys_float32 w = ys_pi * 0.001f;
        g_camera.m_yaw += w * (x - s_mouseX);
        g_camera.m_pitch -= w * (y - s_mouseY);
    }

    s_mouseX = x;
    s_mouseY = y;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void sScrollCallback(GLFWwindow* window, double dx, double dy)
{
    ImGui_ImplGlfw_ScrollCallback(window, dx, dy);
    if (ImGui::GetIO().WantCaptureMouse)
    {
        return;
    }

    if (dy > 0.0)
    {
        g_camera.m_verticalFov /= 1.1f;
    }
    else
    {
        g_camera.m_verticalFov *= 1.1f;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void sUpdateCamera(ys_float32 moveDistance)
{
    ysVec4 sRight = ysVec4_zero;
    ysVec4 sUp = ysVec4_zero;
    ysVec4 sForward = ysVec4_zero;

    // right
    if (glfwGetKey(g_mainWindow, GLFW_KEY_D) == GLFW_PRESS)
    {
        sRight = sRight + ysVec4_one;
    }

    // left
    if (glfwGetKey(g_mainWindow, GLFW_KEY_A) == GLFW_PRESS)
    {
        sRight = sRight - ysVec4_one;
    }

    // up
    if (glfwGetKey(g_mainWindow, GLFW_KEY_E) == GLFW_PRESS)
    {
       sUp = sUp + ysVec4_one;
    }

    // down
    if (glfwGetKey(g_mainWindow, GLFW_KEY_C) == GLFW_PRESS)
    {
        sUp = sUp - ysVec4_one;
    }

    // forward
    if (glfwGetKey(g_mainWindow, GLFW_KEY_W) == GLFW_PRESS)
    {
        sForward = sForward + ysVec4_one;
    }

    // backward
    if (glfwGetKey(g_mainWindow, GLFW_KEY_S) == GLFW_PRESS)
    {
        sForward = sForward - ysVec4_one;
    }

    ysVec4 vRight = g_camera.ComputeRight();
    ysVec4 vUp = g_camera.ComputeUp();
    ysVec4 vForward = g_camera.ComputeForward();

    ysVec4 deltaCenter = (sRight * vRight + sUp * vUp + sForward * vForward) * ysSplat(moveDistance);
    g_camera.m_center = g_camera.m_center + deltaCenter;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void sCreateScene()
{
    const ys_float32 h = 4.0f;
    ysVec4 corners[2][2][2];
    for (ys_int32 i = 0; i < 2; ++i)
    {
        ys_float32 x = 2.0f * i - 1.0f;
        for (ys_int32 j = 0; j < 2; ++j)
        {
            ys_float32 y = 2.0f * j - 1.0f;
            for (ys_int32 k = 0; k < 2; ++k)
            {
                ys_float32 z = 2.0f * k - 1.0f;
                corners[i][j][k] = ysVecSet(x, y, z) * ysSplat(h);
            }
        }
    }

    ysInputTriangle triangles[14];
    ys_int32 wallIdx = 0;
    triangles[2 * wallIdx + 0].m_vertices[0] = corners[0][0][0];
    triangles[2 * wallIdx + 0].m_vertices[1] = corners[1][0][0];
    triangles[2 * wallIdx + 0].m_vertices[2] = corners[1][1][0];
    triangles[2 * wallIdx + 1].m_vertices[0] = corners[0][0][0];
    triangles[2 * wallIdx + 1].m_vertices[1] = corners[1][1][0];
    triangles[2 * wallIdx + 1].m_vertices[2] = corners[0][1][0];
    wallIdx++;
    triangles[2 * wallIdx + 0].m_vertices[0] = corners[1][1][1];
    triangles[2 * wallIdx + 0].m_vertices[1] = corners[1][0][1];
    triangles[2 * wallIdx + 0].m_vertices[2] = corners[0][0][1];
    triangles[2 * wallIdx + 1].m_vertices[0] = corners[0][1][1];
    triangles[2 * wallIdx + 1].m_vertices[1] = corners[1][1][1];
    triangles[2 * wallIdx + 1].m_vertices[2] = corners[0][0][1];
    wallIdx++;
    triangles[2 * wallIdx + 0].m_vertices[0] = corners[0][0][0];
    triangles[2 * wallIdx + 0].m_vertices[1] = corners[0][1][0];
    triangles[2 * wallIdx + 0].m_vertices[2] = corners[0][1][1];
    triangles[2 * wallIdx + 1].m_vertices[0] = corners[0][0][0];
    triangles[2 * wallIdx + 1].m_vertices[1] = corners[0][1][1];
    triangles[2 * wallIdx + 1].m_vertices[2] = corners[0][0][1];
    wallIdx++;
    triangles[2 * wallIdx + 0].m_vertices[0] = corners[1][1][1];
    triangles[2 * wallIdx + 0].m_vertices[1] = corners[1][1][0];
    triangles[2 * wallIdx + 0].m_vertices[2] = corners[1][0][0];
    triangles[2 * wallIdx + 1].m_vertices[0] = corners[1][0][1];
    triangles[2 * wallIdx + 1].m_vertices[1] = corners[1][1][1];
    triangles[2 * wallIdx + 1].m_vertices[2] = corners[1][0][0];
    wallIdx++;
    triangles[2 * wallIdx + 0].m_vertices[0] = corners[0][1][0];
    triangles[2 * wallIdx + 0].m_vertices[1] = corners[1][1][0];
    triangles[2 * wallIdx + 0].m_vertices[2] = corners[1][1][1];
    triangles[2 * wallIdx + 1].m_vertices[0] = corners[0][1][0];
    triangles[2 * wallIdx + 1].m_vertices[1] = corners[1][1][1];
    triangles[2 * wallIdx + 1].m_vertices[2] = corners[0][1][1];
    wallIdx++;
    triangles[2 * wallIdx + 0].m_vertices[0] = corners[1][0][1];
    triangles[2 * wallIdx + 0].m_vertices[1] = corners[1][0][0];
    triangles[2 * wallIdx + 0].m_vertices[2] = corners[0][0][0];
    triangles[2 * wallIdx + 1].m_vertices[0] = corners[0][0][1];
    triangles[2 * wallIdx + 1].m_vertices[1] = corners[1][0][1];
    triangles[2 * wallIdx + 1].m_vertices[2] = corners[0][0][0];
    wallIdx++;

    for (ys_int32 i = 0; i < 6; ++i)
    {
        triangles[2 * i + 0].m_twoSided = false;
        triangles[2 * i + 1].m_twoSided = false;
        triangles[2 * i + 0].m_materialType = ysMaterialType::e_standard;
        triangles[2 * i + 1].m_materialType = ysMaterialType::e_standard;
        triangles[2 * i + 0].m_materialTypeIndex = 0;
        triangles[2 * i + 1].m_materialTypeIndex = 0;
    }

    const ys_float32 asdf = 0.8f;
    const ys_float32 qwer = 0.5f;
    triangles[12].m_vertices[0] = ysVecSet(-qwer, -qwer, asdf) * ysSplat(h);
    triangles[12].m_vertices[1] = ysVecSet(qwer, -qwer, asdf) * ysSplat(h);
    triangles[12].m_vertices[2] = ysVecSet(qwer, qwer, asdf) * ysSplat(h);
    triangles[12].m_twoSided = true;
    triangles[12].m_materialType = ysMaterialType::e_standard;
    triangles[12].m_materialTypeIndex = 1;
    triangles[13].m_vertices[0] = ysVecSet(-qwer, -qwer, asdf) * ysSplat(h);
    triangles[13].m_vertices[1] = ysVecSet(qwer, qwer, asdf) * ysSplat(h);
    triangles[13].m_vertices[2] = ysVecSet(-qwer, qwer, asdf) * ysSplat(h);
    triangles[13].m_twoSided = true;
    triangles[13].m_materialType = ysMaterialType::e_standard;
    triangles[13].m_materialTypeIndex = 1;

    ysMaterialStandardDef materialStandards[2];
    materialStandards[0].m_albedoDiffuse = ysVecSet(1.0f, 1.0f, 1.0f);
    materialStandards[0].m_albedoSpecular = ysVecSet(0.0f, 0.0f, 0.0f);
    materialStandards[0].m_emissiveDiffuse = ysVecSet(0.0f, 0.0f, 0.0f);
    materialStandards[1].m_albedoDiffuse = ysVecSet(1.0f, 1.0f, 1.0f);
    materialStandards[1].m_albedoSpecular = ysVecSet(0.0f, 0.0f, 0.0f);
    materialStandards[1].m_emissiveDiffuse = ysVecSet(1.0f, 1.0f, 1.0f) * ysSplat(1.0f);

    ysLightPointDef lightPoints[1];
    lightPoints[0].m_position = ysVecSet(0.0f, 0.0f, 0.9f) * ysSplat(h);
    lightPoints[0].m_wattage = ysSplat(100.0f);

    ysSceneDef sceneDef;
    sceneDef.m_triangles = triangles;
    sceneDef.m_triangleCount = 14;
    sceneDef.m_materialStandards = materialStandards;
    sceneDef.m_materialStandardCount = 2;
    sceneDef.m_lightPoints = lightPoints;
    sceneDef.m_lightPointCount = 1;

    s_sceneId = ysScene_Create(sceneDef);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void sDestroyScene()
{
    if (s_renderId != ys_nullRenderId)
    {
        ysScene_DestroyRender(s_renderId);
        s_renderId = ys_nullRenderId;
    }
    ysScene_Destroy(s_sceneId);
    s_sceneId = ys_nullSceneId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int, char**)
{
    // Enable memory-leak reports
    _CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF | _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG));

    glfwSetErrorCallback(glfwErrorCallback);
    if (glfwInit() == 0)
    {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }

    const char* glslVersion = nullptr;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    ys_int32 windowHeight = 800;
    ys_int32 windowWidth = ys_int32((ys_float32)windowHeight * g_camera.m_aspectRatio);
    g_mainWindow = glfwCreateWindow(windowWidth, windowHeight, "YoshiPBR TestBed", nullptr, nullptr);
    if (g_mainWindow == nullptr)
    {
        fprintf(stderr, "Failed to open GLFW g_mainWindow.\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(g_mainWindow);

    // Load OpenGL functions using glad
    int gladVersion = gladLoadGL();
    printf("OpenGL %s, GLSL %s\n", glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));

    glfwSetScrollCallback(g_mainWindow, sScrollCallback);
    glfwSetWindowSizeCallback(g_mainWindow, sResizeWindowCallback);
    glfwSetKeyCallback(g_mainWindow, sKeyCallback);
    glfwSetCharCallback(g_mainWindow, sCharCallback);
    glfwSetMouseButtonCallback(g_mainWindow, sMouseButtonCallback);
    glfwSetCursorPosCallback(g_mainWindow, sMouseMotionCallback);
    glfwSetScrollCallback(g_mainWindow, sScrollCallback);

    g_debugDraw.Create();

    sCreateUI(g_mainWindow, glslVersion);

    // Control the frame rate. One draw per monitor refresh.
    glfwSwapInterval(1);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.3f, 0.3f, 0.3f, 1.f);

    ys_float64 time1 = glfwGetTime();
    ys_float64 frameTime = 0.0;

    sCreateScene();

    while (!glfwWindowShouldClose(g_mainWindow))
    {
        glfwGetWindowSize(g_mainWindow, &windowWidth, &windowHeight);

        int bufferWidth, bufferHeight;
        glfwGetFramebufferSize(g_mainWindow, &bufferWidth, &bufferHeight);
        glViewport(0, 0, bufferWidth, bufferHeight);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();

        if (g_settings.m_showUI)
        {
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize(ImVec2(float(windowWidth), float(windowHeight)));
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGui::Begin("Overlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
            ImGui::End();
        }

        ys_float32 cameraMoveStep = g_settings.m_moveSpeed * (ys_float32)frameTime;
        sUpdateCamera(cameraMoveStep);

        if (g_settings.m_drawBVH)
        {
            ysDrawInputBVH input;
            input.debugDraw = &g_debugDraw;
            input.depth = g_settings.m_drawBVHDepth;
            ysScene_DebugDrawBVH(s_sceneId, input);
        }

        if (g_settings.m_drawGeo)
        {
            ysDrawInputGeo input;
            input.debugDraw = &g_debugDraw;
            ysScene_DebugDrawGeo(s_sceneId, input);
        }

        if (g_settings.m_drawLights)
        {
            ysDrawInputLights input;
            input.debugDraw = &g_debugDraw;
            ysScene_DebugDrawLights(s_sceneId, input);
        }

        if (g_settings.m_drawRender)
        {
            if (s_renderId != ys_nullRenderId)
            {
                if (ysRender_WorkFinished(s_renderId))
                {
                    ysSceneRenderOutput output;
                    ysRender_GetFinalOutput(s_renderId, &output);
                    ysAssert(output.m_pixels.GetCount() == s_pixelCountX * s_pixelCountY);
                    for (ys_int32 i = 0; i < output.m_pixels.GetCount(); ++i)
                    {
                        const ysFloat3& rgb = output.m_pixels[i];
                        s_pixels[i] = Color(rgb.r, rgb.g, rgb.b, 1.0f);
                    }
                    ysScene_DestroyRender(s_renderId);
                    s_renderId = ys_nullRenderId;
                }
                else
                {
                    ysSceneRenderOutputIntermediate output;
                    ysRender_GetIntermediateOutput(s_renderId, &output);
                    ysAssert(output.m_pixels.GetCount() == s_pixelCountX * s_pixelCountY);
                    for (ys_int32 i = 0; i < output.m_pixels.GetCount(); ++i)
                    {
                        const ysFloat4& rgb = output.m_pixels[i];
                        s_pixels[i] = Color(rgb.r, rgb.g, rgb.b, rgb.a);
                    }
                }
            }

            if (s_pixelCountX > 0 && s_pixelCountY > 0)
            {
                Texture2D tex;
                tex.m_pixels = s_pixels.GetEntries();
                tex.m_width = s_pixelCountX;
                tex.m_height = s_pixelCountY;

                ys_float32 aspectRatio = (ys_float32)s_renderInput.m_pixelCountX / (ys_float32)s_renderInput.m_pixelCountY;

                ysTransform xf;
                xf.p = ysMul(s_renderInput.m_eye, -ysVec4_unitZ);
                xf.q = s_renderInput.m_eye.q;
                ys_float32 quadHeight = tanf(s_renderInput.m_fovY) * 2.0f;
                ys_float32 quadWidth = quadHeight * aspectRatio;
                g_debugDraw.DrawTexturedQuad(tex, quadWidth, quadHeight, xf, true, true);

                ysVec4 a = ysMul(s_renderInput.m_eye, ysVecSet(-quadWidth * 0.5f, -quadHeight * 0.5f, -1.0f));
                ysVec4 b = ysMul(s_renderInput.m_eye, ysVecSet(quadWidth * 0.5f, -quadHeight * 0.5f, -1.0f));
                ysVec4 c = ysMul(s_renderInput.m_eye, ysVecSet(quadWidth * 0.5f, quadHeight * 0.5f, -1.0f));
                ysVec4 d = ysMul(s_renderInput.m_eye, ysVecSet(-quadWidth * 0.5f, quadHeight * 0.5f, -1.0f));
                ysVec4 e = s_renderInput.m_eye.p;

                ysVec4 frustum[8][2];
                frustum[0][0] = e;
                frustum[0][1] = a;
                frustum[1][0] = e;
                frustum[1][1] = b;
                frustum[2][0] = e;
                frustum[2][1] = c;
                frustum[3][0] = e;
                frustum[3][1] = d;
                frustum[4][0] = a;
                frustum[4][1] = b;
                frustum[5][0] = b;
                frustum[5][1] = c;
                frustum[6][0] = c;
                frustum[6][1] = d;
                frustum[7][0] = d;
                frustum[7][1] = a;
                Color colors[8];
                for (ys_int32 i = 0; i < 8; ++i)
                {
                    colors[i] = Color(1.0f, 1.0f, 1.0f, 1.0f);
                }
                g_debugDraw.DrawSegmentList(&frustum[0][0], colors, 8);
            }
        }

        g_debugDraw.Flush();

        sUpdateUI(windowWidth, windowHeight);

        //ImGui::ShowDemoWindow();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(g_mainWindow);

        glfwPollEvents();

        ys_float64 time2 = glfwGetTime();
        ys_float64 alpha = 0.9;
        frameTime = alpha * frameTime + (1.0 - alpha) * (time2 - time1);
        time1 = time2;
    }

    sDestroyScene();

    g_debugDraw.Destroy();
    sDestroyUI();
    glfwTerminate();
    return 0;
}