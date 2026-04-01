cmake_minimum_required(VERSION 3.10)
project(AudioVisualizer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(glfw3 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
pkg_check_modules(ALSA REQUIRED alsa)
find_package(Threads REQUIRED)
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found, using header-only fallback")
    set(NLOHMANN_JSON_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann")
    if(NOT EXISTS "${NLOHMANN_JSON_INCLUDE_DIRS}/json.hpp")
        message(STATUS "Downloading nlohmann/json header...")
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/external)
        execute_process(
            COMMAND wget https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann
            OUTPUT_QUIET
        )
    endif()
    include_directories(${NLOHMANN_JSON_INCLUDE_DIRS})
else()
    include_directories(${nlohmann_json_INCLUDE_DIRS})
endif()

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
    src/visualizer.cpp
    src/visualizer_imgui.cpp
    src/visualizer_console.cpp
    src/visualizer_diagnostic.cpp
    src/visualizer_procedural.cpp
    src/audio_capture.cpp
    src/audio_analyzer.cpp
    src/modular_layer.cpp
    src/post_processor.cpp
    src/shader.cpp
    src/settings_manager.cpp
    src/midi_controller.cpp
    src/layer_validator.cpp
    src/shader_loader.cpp
    ${IMGUI_SOURCES}
)

# Layer validation utility
add_executable(validate_layers
    src/validate_layers.cpp
    src/layer_validator.cpp
    src/shader_loader.cpp
)

# Layer integration tests
add_executable(test_layer_integration
    src/test_layer_integration.cpp
    src/layer_validator.cpp
    src/modular_layer.cpp
    src/post_processor.cpp
    src/shader.cpp
    src/audio_analyzer.cpp
    src/settings_manager.cpp
    src/shader_loader.cpp
    ${IMGUI_SOURCES}
)

# Link libraries
target_link_libraries(audio_visualizer
    ${CMAKE_DL_LIBS}
    glfw
    ${GLEW_LIBRARIES}
    ${OPENGL_LIBRARIES}
    -lGL
    ${PORTAUDIO_LIBRARIES}
    ${ALSA_LIBRARIES}
    Threads::Threads
)

# Link libraries for validation utilities
target_link_libraries(validate_layers
    m
    pthread
)

target_link_libraries(test_layer_integration
    ${CMAKE_DL_LIBS}
    glfw
    ${OPENGL_LIBRARIES}
    ${GLEW_LIBRARIES}
    -lGL
    m
    pthread
)

# Compiler flags
target_compile_options(audio_visualizer PRIVATE ${PORTAUDIO_CFLAGS_OTHER})

# Copy shaders to build directory after build
add_custom_command(TARGET audio_visualizer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:audio_visualizer>/shaders
)

# Copy shaders for validation utilities
add_custom_command(TARGET validate_layers POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:validate_layers>/shaders
)

add_custom_command(TARGET test_layer_integration POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:test_layer_integration>/shaders
)

# Pre-build validation step (commented out for testing)
# add_custom_target(validate_shaders ALL
#     COMMAND validate_layers --all
#     COMMENT "Validating all shader layers before compilation..."
#     DEPENDS validate_layers
# )

# Make main executable depend on validation (commented out for testing)
# add_dependencies(audio_visualizer validate_shaders)
