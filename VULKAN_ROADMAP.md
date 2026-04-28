# Vulkan Migration Roadmap

## Phase 1: Foundation (COMPLETED)
- [x] Create IRenderer interface abstraction
- [x] Create GLRenderer wrapper for existing OpenGL code
- [x] Create VKRenderer basic skeleton with Vulkan initialization
- [x] Add Vulkan dependencies to CMakeLists.txt
- [x] Implement backend selection (--backend command line argument)
- [x] Add Vulkan availability detection

## Phase 2: Core Vulkan Implementation (REORDERED BY DEPENDENCIES)

### 2.1 Swapchain + Frame Loop (CRITICAL - FIRST)
- [ ] Implement swapchain with recreation on resize
- [ ] Implement image views creation
- [ ] Implement render pass (single pass initially)
- [ ] Implement framebuffers
- [ ] Implement command buffer pool
- [ ] Implement per-frame command buffers
- [ ] Implement synchronization (fences, semaphores)
- [ ] Implement image acquisition / presentation sync
- [ ] Implement pipeline barriers
- [ ] Implement per-frame resource lifetime tracking

### 2.2 Memory Management Layer (NEW - CRITICAL)
- [ ] Integrate Vulkan Memory Allocator (VMA)
- [ ] Implement memory allocation abstraction
- [ ] Implement memory pool management
- [ ] Implement memory leak detection

### 2.3 Pipeline System
- [ ] Implement graphics pipeline creation
- [ ] Implement shader modules (SPIR-V)
- [ ] Implement pipeline cache system
- [ ] Implement descriptor set layout
- [ ] Implement descriptor pool
- [ ] Implement descriptor sets

### 2.4 Shader System (AFTER PIPELINE)
- [ ] Add glslangValidator to CMake build system
- [ ] Create shader compilation pipeline (.glsl → .spv)
- [ ] Implement VKShader class with SPIR-V loading
- [ ] Implement shader hot-reload mechanism (complex in Vulkan)
- [ ] Handle pipeline recreation on shader reload
- [ ] Handle descriptor set invalidation on reload

### 2.5 Resource Management (AFTER MEMORY + PIPELINE)
- [ ] Implement VKBuffer class (vertex/index/uniform)
- [ ] Implement VKTexture class
- [ ] Implement VKFramebuffer class
- [ ] Implement sampler management
- [ ] Implement SSBO for audio data

## Phase 3: Renderer Layer Migration
### 3.1 Background Rendering
- [ ] Port background rendering from OpenGL to Vulkan
- [ ] Create Vulkan compute shaders (optional optimization)

### 3.2 Procedural Layers
- [ ] Port ModularLayer to Vulkan
- [ ] Implement procedural shader pipeline in Vulkan
- [ ] Port shader hot-reload mechanism
- [ ] Implement framebuffer composition

### 3.3 Post-Processing
- [ ] Port PostProcessor to Vulkan
- [ ] Implement post-processing pipeline with ping-pong framebuffers
- [ ] Port all post-processing effects

## Phase 4: UI Integration
### 4.1 ImGui Migration
- [ ] Add ImGui Vulkan backend dependencies
- [ ] Implement VKImGuiRenderer
- [ ] Port all ImGui windows to Vulkan

### 4.2 Input Handling
- [ ] Port GLFW input handling for Vulkan
- [ ] Ensure keyboard shortcuts work with Vulkan

## Phase 5: Visualizer Integration
### 5.1 Architecture Migration
- [ ] Modify Visualizer to use IRenderer interface
- [ ] Remove direct OpenGL calls from Visualizer
- [ ] Implement renderer selection logic

### 5.2 Audio Integration
- [ ] Implement SSBO for audio data (ring buffer on CPU side)
- [ ] Pass audio data via SSBO instead of push constants (more efficient)
- [ ] Test audio-reactive rendering with Vulkan

### 5.3 Settings Migration
- [ ] Ensure Vulkan renderer respects all settings
- [ ] Test settings save/load with Vulkan

## Phase 6: Testing & Optimization
### 6.1 Testing
- [ ] Test all procedural modes with Vulkan
- [ ] Test all post-processing effects with Vulkan
- [ ] Test camera control with Vulkan
- [ ] Test shader hot-reload with Vulkan

### 6.2 Performance
- [ ] Profile Vulkan renderer performance
- [ ] Optimize descriptor set updates
- [ ] Optimize command buffer recording
- [ ] Compare performance with OpenGL

### 6.3 Debugging
- [ ] Add Vulkan validation layers for debugging
- [ ] Implement renderDoc integration
- [ ] Add error handling and fallback mechanisms

## Phase 7: Polish & Release
- [ ] Update documentation
- [ ] Add backend selection to UI (not just command line)
- [ ] Add performance metrics display
- [ ] Create Vulkan-specific settings (e.g., enable/disable validation layers)
- [ ] Test on different GPUs

## Estimated Effort
- Phase 1: ✅ Completed
- Phase 2: 3-6 weeks (core Vulkan infrastructure - if already know Vulkan basics)
- Phase 3: 3-5 weeks (rendering layers)
- Phase 4: 1-2 weeks (UI integration)
- Phase 5: 1-2 weeks (Visualizer integration)
- Phase 6: 1-2 weeks (testing & optimization)
- Phase 7: 1 week (polish)

**Total estimated: 2-4 months (realistic)**

## Current Status
- OpenGL renderer: ✅ Fully functional
- Vulkan renderer: ⚠️ Basic initialization only
- Backend selection: ✅ Command line implemented
- Migration progress: ~10%

## Next Immediate Steps
1. Start Phase 2.1: Implement proper swapchain and render pass
2. Add glslangValidator to build system
3. Create first working Vulkan triangle test

---

## Prerequisite: OpenGL Optimization (RECOMMENDED FIRST)

Before completing Vulkan migration, optimize OpenGL first:
- [ ] Eliminate shader recompilation at runtime
- [ ] Use UBO/SSBO for audio data instead of per-frame uniforms
- [ ] Batch uniforms in structs
- [ ] Reduce draw calls to 1-3 maximum

If this gives significant improvement → Vulkan may not be necessary yet.

---

## MVP Vulkan Real (SMALLER THAN FULL ROADMAP)

Don't aim for Phase 2 complete. Real MVP:
- [ ] Swapchain
- [ ] Single render pass
- [ ] Fullscreen quad
- [ ] SSBO audio buffer
- [ ] ImGui integration

Only that. Test this first before proceeding.

---

## Optional Architecture Improvements

### Frame Graph (Optional but useful)
- [ ] Implement frame graph for render pass dependencies
- [ ] Automatic barrier insertion
- [ ] Resource lifetime management

### Vulkan Architecture (Classes + Frame Flow + Ownership)
- [ ] Define ownership model for Vulkan objects
- [ ] Implement frame flow state machine
- [ ] Create abstraction layers for Vulkan complexity
