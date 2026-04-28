# Architecture Status & Roadmap

## Current Architecture (Post-Cleanup)

### Completed Structural Changes

**✅ Layer Separation**
- `src/app/` - Application logic layer (visualizer, UI, scene composition)
- `src/` - Core engine (audio, settings, MIDI) - backend-agnostic
- `gl_backend` - Pure OpenGL rendering (only gl_renderer.cpp)
- `shader_system` - Shader loading/management
- `post_processing` - Post-processing effects
- `modular_layer` - Procedural layer system

**✅ ImGui Decoupling**
- `imgui` - Core Dear ImGui (backend-agnostic)
- `imgui_glfw` - GLFW platform backend
- `imgui_opengl3` - OpenGL rendering backend

**✅ CMake Structure**
```
core_engine (audio, settings, MIDI)
├── shader_system
├── post_processing
├── modular_layer
└── app_layer (visualizer logic)
    ├── imgui
    ├── imgui_glfw
    └── imgui_opengl3
gl_backend (pure OpenGL)
```

### Current Limitations (Known Issues)

**❌ OpenGL Leakage in "Backend-Agnostic" Libraries**

The following libraries are marked as "backend-agnostic" but still contain OpenGL code:

1. **shader_system** (`shader.cpp`, `shader_loader.cpp`)
   - Uses `glCreateShader`, `glCompileShader`, `glLinkProgram`, etc.
   - Direct OpenGL shader compilation
   - Should use IShader abstraction from renderer_interface.h

2. **modular_layer** (`modular_layer.cpp`)
   - Uses `glBindVertexArray`, `glDrawArrays`, `glUseProgram`, etc.
   - Direct OpenGL rendering calls
   - Should use IRenderer/IRenderLayer abstractions

3. **post_processing** (`post_processor.cpp`)
   - Uses OpenGL framebuffer operations
   - Should use IFramebuffer abstraction

4. **app_layer** (visualizer files)
   - `visualizer.h` includes `GL/glew.h` and `GLFW/glfw3.h`
   - Contains inline shader sources
   - Direct OpenGL state management
   - Should use IRenderer interface exclusively

**Why This Matters**

These limitations prevent true backend swappability because:
- Cannot switch to Vulkan without rewriting these libraries
- Testing framework still coupled to OpenGL
- Shader system cannot output SPIR-V for Vulkan
- No clear boundary between app logic and rendering

## Architectural Roadmap

### Phase 1: Immediate Cleanup (Current)

**Status**: ✅ Structural separation complete

**Completed**:
- ✅ Created app layer directory
- ✅ Moved visualizer files to app layer
- ✅ Separated ImGui backends
- ✅ Updated CMake dependencies
- ✅ Build succeeds

### Phase 2: Shader System Abstraction ✅ COMPLETED

**Goal**: Make shader_system truly backend-neutral

**Completed changes**:
1. ✅ Created `shader_handle.h` - Opaque ShaderHandle abstraction (uint64_t)
2. ✅ Created `gl_shader_compiler.h/cpp` - OpenGL-specific compilation in gl_backend
3. ✅ Refactored `shader.h` to use ShaderHandle instead of GLuint
4. ✅ Refactored `shader.cpp` to delegate all OpenGL calls to GLShaderCompiler
5. ✅ Removed `<GL/glew.h>` include from shader.h
6. ✅ Updated CMake: shader_system no longer links to OpenGL libraries directly
7. ✅ shader_system now depends on gl_backend for compilation

**Impact**:
- shader_system is now backend-agnostic (only handles loading, caching, logical validation)
- OpenGL compilation logic isolated in gl_backend
- Enables future Vulkan shader compiler (vk_shader_compiler)
- Shader hot-reload still works through abstraction layer

**CMake dependency change**:
```
Before: shader_system → ${GLEW_LIBRARIES} ${OPENGL_LIBRARIES}
After:  shader_system → gl_backend
```

**Files changed**:
- `src/shader_handle.h` (new)
- `src/gl_shader_compiler.h` (new)
- `src/gl_shader_compiler.cpp` (new)
- `src/shader.h` (refactored)
- `src/shader.cpp` (refactored)
- `CMakeLists.txt` (updated dependencies)

### Phase 3.2: M oratemmodular_land Seo Rendem COmmaMds (IN PROGRESS)PLETED (Infrastructure)

**Goal**: Elipinatment bac callskend-agnostic rende by usingrRenderComm comlmiineo

**Completed changes**:
1. ✅ Created `render_command.h` - Backend-agnostic RenderCommand struct with union-based data
2. ✅ Created `render_queue.h/cpp` - Command collection and management
3. ✅ Created `gl_command_executor.h/cpp` - OpenGL-specific command executor in gl_backend
4. ✅ Updated CMake to include new files in shader_system and gl_backend
5. ✅ Build succeeds

**Architecture change**:
```
Before: modules → direct OpenGL calls
After:  modules → emit RenderCommand → RenderQueue → GLCommandExecutor → OpenGL
```

**RenderCommand types supported**:
- Shader commands (BindShader, SetUniform*)
- Texture commands (BindTexture, UnbindTexture)
- Buffer commands (BindVertexArray, BindBuffer)
- Drawing commands (DrawArrays, DrawElements)
- Framebuffer commands (BindFramebuffer, ClearFramebuffer)
- State commands (EnableBlend, SetViewport, etc.)

**Impact**:
- Enables true backend separation (same commands, different executors)
- Foundation for state batching and minimization
- Enables future Vulkan executor (VKCommandExecutor)
- Modules can now emit commands without knowing the backend

**Files created**:
- `src/render_command.h` (new)
- `src/render_queue.cpp` (new)
- `src/render_queue.h` (new)
- `src/gl_command_executor.cpp` (new)
- `src/gl_command_executor.h` (new)

**Next steps** (Phase 3.2-3.4):
- Migrate modular_layer to emit commands instead of direct OpenGL calls
- Migrate post_processing to emit commands
- Implement command batching/sorting in RenderQueue

**Impact**: Makes layers truly backend-agnostic, enables Vulkan post-processing

### Phase 4: App Layer Decoupling

**Goal**: Remove OpenGL/GLFW dependencies from app_layer

**Changes needed**:
1. Remove `#include <GL/glew.h>` and `#include <GLFW/glfw3.h>` from visualizer.h
2. Use IRenderer interface for all rendering operations
3. Move inline shader sources to shader system
4. Pass GLFWwindow* through IRenderer interface only

**Impact**: App logic becomes completely backend-agnostic

### Phase 5: Backend Runtime Selection

**Goal**: Enable runtime backend switching

**Changes needed**:
1. Implement RendererFactory::create() properly
2. Add backend selection logic in main.cpp
3. Ensure all libraries work with both backends
4. Add backend-specific resource managers

**Impact**: Users can switch between OpenGL and Vulkan at runtime

## Current Working State

**What Works**:
- ✅ Build succeeds with new structure
- ✅ Clear separation of concerns in CMake
- ✅ ImGui backends properly separated
- ✅ App layer isolated from core engine

**What Still Needs Work**:
- ❌ shader_system, modular_layer, post_processing still OpenGL-bound
- ❌ visualizer.h still includes OpenGL headers
- ❌ No true backend abstraction implementation
- ❌ Tests still OpenGL-coupled

## Next Steps

**Recommended Order**:
1. Complete Phase 2 (Shader System Abstraction) - Highest ROI
2. Complete Phase 3 (Rendering Layer Abstraction) - Enables Vulkan layers
3. Complete Phase 4 (App Layer Decoupling) - Finalizes separation
4. Complete Phase 5 (Runtime Selection) - Enables backend switching

**Estimated Effort**:
- Phase 2: 2-3 days
- Phase 3: 3-5 days
- Phase 4: 2-3 days
- Phase 5: 1-2 days

**Total**: ~8-13 days for complete backend-agnostic architecture

## Conclusion

The current architecture is **in transition**:
- ✅ Structural separation achieved
- ❌ True backend-agnosticity not yet realized
- ❌ OpenGL still "leaks" into supposedly backend-agnostic libraries

This is a **significant improvement** over the monolithic structure, but **not yet the final architecture** needed for clean Vulkan migration.

The next phase should focus on **shader system abstraction** as it provides the highest ROI and unblocks the most subsequent work.
