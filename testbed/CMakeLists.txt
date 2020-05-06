project(testbed LANGUAGES CXX)

set (TESTBED_SOURCE_FILES
	main.cpp
    camera.cpp
    camera.h
    draw.cpp
    draw.h
	scenePreview.cpp
	scenePreview.h
    settings.cpp
    settings.h
	imgui_impl_glfw.cpp
	imgui_impl_glfw.h
	imgui_impl_opengl3.cpp
	imgui_impl_opengl3.h
)

set (TESTBED_RESOURCE_FILES
    data/DroidSans.ttf
    shaders/edge_VS.glsl
    shaders/edge_PS.glsl
    shaders/face_VS.glsl
    shaders/face_PS.glsl
    shaders/primEdge_VS.glsl
    shaders/primEdge_PS.glsl
    shaders/primFace_VS.glsl
    shaders/primFace_PS.glsl
    shaders/texturedQuad_VS.glsl
    shaders/texturedQuad_PS.glsl
)

add_executable(testbed ${TESTBED_SOURCE_FILES} ${TESTBED_RESOURCE_FILES})
target_include_directories(testbed PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(testbed PUBLIC YoshiPBR glfw glad imgui)
target_compile_options(testbed PRIVATE -W4 -WX)
set_target_properties(testbed PROPERTIES
	CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${TESTBED_SOURCE_FILES} ${TESTBED_RESOURCE_FILES})
