cmake_minimum_required(VERSION 3.10)
project(AudioVisualizer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)

# GLEW
find_path(GLEW_INCLUDE_DIRS GL/glew.h)
find_library(GLEW_LIBRARIES GLEW)
if(NOT GLEW_INCLUDE_DIRS OR NOT GLEW_LIBRARIES)
    message(FATAL_ERROR "GLEW not found. Please install libglew-dev or equivalent")
endif()

# ImGui (as submodule)
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
set(IMGUI_HEADERS
    ${IMGUI_DIR}/imgui.h
    ${IMGUI_DIR}/imgui_internal.h
    ${IMGUI_DIR}/imgui_demo.h
    ${IMGUI_DIR}/imgui_draw.h
    ${IMGUI_DIR}/imgui_tables.h
    ${IMGUI_DIR}/imgui_widgets.h
    ${IMGUI_DIR}/backends/imgui_impl_glfw.h
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.h
    ${IMGUI_DIR}/imstb_rectpack.h
    ${IMGUI_DIR}/imstb_textedit.h
    ${IMGUI_DIR}/imconfig.h
)

# PortAudio
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

# GLFW
find_package(glfw3 REQUIRED)
if(NOT glfw3_FOUND)
    find_path(GLFW_INCLUDE_DIRS GLFW/glfw3.h
        PATHS /usr/include /usr/local/include
        PATH_SUFFIXES glfw GL
    )
    find_library(GLFW_LIBRARIES glfw
        PATHS /usr/lib /usr/local/lib
    )
    if(NOT GLFW_INCLUDE_DIRS OR NOT GLFW_LIBRARIES)
        message(FATAL_ERROR "GLFW not found. Please install libglfw-dev or equivalent")
    endif()
endif()

# Include directories
include_directories(${OPENGL_INCLUDE_DIRS})
if(GLFW_INCLUDE_DIRS)
    include_directories(${GLFW_INCLUDE_DIRS})
endif()
if(GLEW_INCLUDE_DIRS)
    include_directories(${GLEW_INCLUDE_DIRS})
endif()
include_directories(${PORTAUDIO_INCLUDE_DIRS})
include_directories(${IMGUI_DIR})
include_directories(${IMGUI_DIR}/backends)

# Add executable
add_executable(audio_visualizer
    src/main.cpp
    src/audio_capture.cpp
    src/audio_analyzer.cpp
    src/visualizer.cpp
    src/visualizer_diagnostic.cpp
    src/visualizer_console.cpp
    src/visualizer_imgui.cpp
    src/visualizer_procedural.cpp
    src/post_processor.cpp
    src/modular_layer.cpp
    src/shader.cpp
    ${IMGUI_SOURCES}
)

# Link libraries
target_link_libraries(audio_visualizer
    ${OPENGL_LIBRARIES}
    ${GLEW_LIBRARIES}
    glfw
    ${PORTAUDIO_LIBRARIES}
    m
    pthread
)

# Compiler flags
target_compile_options(audio_visualizer PRIVATE ${PORTAUDIO_CFLAGS_OTHER})
