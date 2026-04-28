cmake_minimum_required(VERSION 3.10)
project(AudioVisualizer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# CORE ENGINE LIBRARY (backend-agnostic)
# ============================================================================
add_library(core_engine STATIC
    src/audio_engine.cpp
    src/audio_capture.cpp
    src/audio_analyzer.cpp
    src/settings_manager.cpp
    src/midi_controller.cpp
    src/profiler.h
)

target_include_directories(core_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ============================================================================
# IMGUI STATIC LIBRARY (shared across all targets)
# ============================================================================
add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
)

# ============================================================================
# SHADER SYSTEM LIBRARY (backend-agnostic)
# ============================================================================
add_library(shader_system STATIC
    src/shader.cpp
    src/shader_loader.cpp
    src/layer_validator.cpp
)

target_include_directories(shader_system PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ============================================================================
# POST PROCESSING LIBRARY (backend-agnostic)
# ============================================================================
add_library(post_processing STATIC
    src/post_processor.cpp
)

target_include_directories(post_processing PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(post_processing PUBLIC
    shader_system
)

# ============================================================================
# MODULAR LAYER LIBRARY (backend-agnostic)
# ============================================================================
add_library(modular_layer STATIC
    src/modular_layer.cpp
)

target_include_directories(modular_layer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(modular_layer PUBLIC
    shader_system
)

# ============================================================================
# OPENGL BACKEND LIBRARY
# ============================================================================
add_library(gl_backend STATIC
    src/gl_renderer.cpp
    src/visualizer.cpp
    src/visualizer_imgui.cpp
    src/visualizer_console.cpp
    src/visualizer_diagnostic.cpp
    src/visualizer_procedural.cpp
)

target_include_directories(gl_backend PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ============================================================================
# VULKAN BACKEND LIBRARY (optional, disabled for now)
# ============================================================================
# add_library(vk_backend STATIC
#     src/vk_renderer.cpp
# )

# target_include_directories(vk_backend PUBLIC
#     ${CMAKE_CURRENT_SOURCE_DIR}/src
# )

# ============================================================================
# FIND DEPENDENCIES
# ============================================================================
find_package(glfw3 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
pkg_check_modules(ALSA REQUIRED alsa)
find_package(Threads REQUIRED)

# Vulkan (optional for future migration)
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    message(STATUS "Vulkan found - Vulkan renderer available")
    include_directories(${Vulkan_INCLUDE_DIRS})
else()
    message(STATUS "Vulkan not found - only OpenGL renderer available")
endif()

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

# ============================================================================
# LINK DEPENDENCIES TO LIBRARIES
# ============================================================================

# Core engine dependencies
target_link_libraries(core_engine PUBLIC
    ${PORTAUDIO_LIBRARIES}
    ${ALSA_LIBRARIES}
    Threads::Threads
)

# ImGui dependencies
target_link_libraries(imgui PUBLIC
    glfw
)

# OpenGL backend dependencies
target_link_libraries(gl_backend PUBLIC
    core_engine
    shader_system
    post_processing
    modular_layer
    imgui
    ${CMAKE_DL_LIBS}
    glfw
    ${GLEW_LIBRARIES}
    ${OPENGL_LIBRARIES}
    -lGL
)

# Vulkan backend dependencies (when enabled)
# if(Vulkan_FOUND)
#     target_link_libraries(vk_backend PUBLIC
#         core_engine
#         shader_system
#         ${Vulkan_LIBRARIES}
#     )
# endif()

# ============================================================================
# EXECUTABLES
# ============================================================================

# Main executable (OpenGL backend only for now)
add_executable(audio_visualizer
    src/main.cpp
)

target_link_libraries(audio_visualizer PRIVATE
    gl_backend
)

# Add Vulkan if available
if(Vulkan_FOUND)
    target_link_libraries(audio_visualizer PRIVATE ${Vulkan_LIBRARIES})
endif()

# Layer validation utility
add_executable(validate_layers
    src/validate_layers.cpp
)

target_link_libraries(validate_layers PRIVATE
    shader_system
    m
    pthread
)

# Layer integration tests
add_executable(test_layer_integration
    src/test_layer_integration.cpp
)

target_link_libraries(test_layer_integration PRIVATE
    core_engine
    shader_system
    post_processing
    modular_layer
    imgui
    ${CMAKE_DL_LIBS}
    glfw
    ${OPENGL_LIBRARIES}
    ${GLEW_LIBRARIES}
    -lGL
    m
    pthread
)

# ============================================================================
# COMPILER FLAGS
# ============================================================================
target_compile_options(audio_visualizer PRIVATE ${PORTAUDIO_CFLAGS_OTHER})

# ============================================================================
# INCREMENTAL SHADER COPY SYSTEM
# ============================================================================

# Function to copy shaders only if they changed
function(copy_shaders_if_changed target_name)
    add_custom_command(TARGET ${target_name} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:${target_name}>/shaders
        COMMENT "Copying shaders for ${target_name}..."
    )
endfunction()

copy_shaders_if_changed(audio_visualizer)
copy_shaders_if_changed(validate_layers)
copy_shaders_if_changed(test_layer_integration)
