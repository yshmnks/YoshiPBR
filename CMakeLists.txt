cmake_minimum_required(VERSION 3.10)

project(YoshiPBR LANGUAGES CXX)

set(CMAKE_CONFIGURATION_TYPES "Debug;RelWithDebInfo" CACHE STRING "" FORCE)

add_subdirectory(source)

option(BUILD_TESTBED "Build the YoshiPBR testbed" ON)

if (BUILD_TESTBED)
	add_subdirectory(external/glad)
	add_subdirectory(external/glfw)
	add_subdirectory(external/imgui)
	add_subdirectory(testbed)

	# default startup project for Visual Studio
	if (MSVC)
		set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT testbed)
		set_property(TARGET testbed PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/testbed")
	endif()
endif()
