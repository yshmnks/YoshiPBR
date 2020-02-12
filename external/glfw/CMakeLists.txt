project(glfw C)

find_package(Threads REQUIRED)

set(common_HEADERS src/internal.h src/mappings.h
                   include/GLFW/glfw3.h
                   include/GLFW/glfw3native.h)
set(common_SOURCES src/context.c src/init.c src/input.c src/monitor.c src/vulkan.c src/window.c)

if (WIN32)
    add_compile_definitions(_GLFW_WIN32=1)

    set(glfw_HEADERS ${common_HEADERS} src/win32_platform.h src/win32_joystick.h
                     src/wgl_context.h src/egl_context.h src/osmesa_context.h)
    set(glfw_SOURCES ${common_SOURCES} src/win32_init.c src/win32_joystick.c
                     src/win32_monitor.c src/win32_time.c src/win32_thread.c src/win32_window.c
                     src/wgl_context.c src/egl_context.c src/osmesa_context.c)
endif()

add_library(glfw ${glfw_SOURCES} ${glfw_HEADERS})
target_include_directories(glfw PUBLIC include)
target_include_directories(glfw PRIVATE ${glfw_INCLUDE_DIRS})
target_link_libraries(glfw INTERFACE ${glfw_LIBRARIES})
