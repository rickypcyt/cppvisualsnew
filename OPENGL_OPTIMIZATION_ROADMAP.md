# OpenGL Optimization Roadmap

## Phase 0: Profiling First (CRITICAL - START HERE)

### 0.1 Determine CPU vs GPU Bound
**Goal:** Identify actual bottleneck before optimizing

**Implementation:**
```cpp
// Add frame time tracking
auto frameStart = std::chrono::high_resolution_clock::now();
// ... render
auto frameEnd = std::chrono::high_resolution_clock::now();
float cpuTime = std::chrono::duration<float, std::milli>(frameEnd - frameStart).count();

// Add GPU timer queries
GLuint query[2];
glGenQueries(2, query);
glBeginQuery(GL_TIME_ELAPSED, query[0]);
// ... render
glEndQuery(GL_TIME_ELAPSED);
glBeginQuery(GL_TIME_ELAPSED, query[1]);
// ... end
glEndQuery(GL_TIME_ELAPSED);

// Get results
GLuint64 cpuTime, gpuTime;
glGetQueryObjectui64v(query[0], GL_QUERY_RESULT, &cpuTime);
glGetQueryObjectui64v(query[1], GL_QUERY_RESULT, &gpuTime);
```

**Decision Tree:**
- CPU-bound → Optimize uniforms, draw calls, state changes
- GPU-bound → Optimize shaders, reduce complexity, compute shaders
- Memory-bound → Optimize texture bandwidth, reduce resolution

### 0.2 Measure Current Baseline
**Metrics to collect:**
- FPS
- Frame time (ms)
- CPU time per frame (ms)
- GPU time per frame (ms)
- Uniform call count per frame
- Draw call count per frame
- Shader compile time (if hot-reload enabled)

### 0.3 Add Pipeline Stall Detection
**Goal:** Detect hidden driver synchronization stalls

**Implementation:**
```cpp
// Temporary glFinish profiling (debug only)
auto syncStart = std::chrono::high_resolution_clock::now();
glFinish();
auto syncEnd = std::chrono::high_resolution_clock::now();
float syncTime = std::chrono::duration<float, std::milli>(syncEnd - syncStart).count();

// If syncTime is high (>5ms), driver is stalling
```

**Expected Outcome:** Identify sync-bound scenarios

### 0.4 Measure Frame Pacing Variance
**Goal:** Detect jitter (critical for audio-reactive apps)

**Implementation:**
```cpp
std::vector<float> frameTimes;
// Track last 60 frames
frameTimes.push_back(currentFrameTime);
if (frameTimes.size() > 60) frameTimes.erase(frameTimes.begin());

// Calculate variance
float mean = std::accumulate(frameTimes.begin(), frameTimes.end(), 0.0f) / frameTimes.size();
float variance = 0.0f;
for (float t : frameTimes) {
    variance += (t - mean) * (t - mean);
}
variance /= frameTimes.size();

// 1% low / 0.1% low (percentiles)
std::sort(frameTimes.begin(), frameTimes.end());
float p1low = frameTimes[static_cast<int>(frameTimes.size() * 0.01)];
float p01low = frameTimes[static_cast<int>(frameTimes.size() * 0.001)];
```

**Expected Outcome:** Detect frame pacing issues invisible in average FPS

### 0.5 Profile FFT CPU Usage
**Goal:** Check if FFT is hidden bottleneck

**Implementation:**
```cpp
// Measure FFT time in AudioAnalyzer
auto fftStart = std::chrono::high_resolution_clock::now();
// ... FFT computation
auto fftEnd = std::chrono::high_resolution_clock::now();
float fftTime = std::chrono::duration<float, std::milli>(fftEnd - fftStart).count();

// If fftTime > 2ms, it's significant
```

**Expected Outcome:** Identify if FFT optimization is needed (outside OpenGL scope)

### 0.6 Add Performance Display to UI
**Implementation:**
- Show FPS
- Show frame time breakdown (CPU/GPU/Sync)
- Show uniform/draw call counts
- Show frame pacing variance (jitter)
- Show FFT time
- Toggle visibility

**Expected Outcome:** Data-driven optimization decisions

---

## Phase 1: Quick Wins Based on Profiling (1-3 days)

### 1.1 IF CPU-bound: Implement UBO for Audio Features
**Goal:** Replace 6+ audio uniforms with single UBO

**Implementation:**
```cpp
struct AudioUBO {
    float energy;
    float bassEnergy;
    float midEnergy;
    float highEnergy;
    float onset;
    float beat;
    float padding[2]; // std140 alignment
};

// Upload once per frame
glBindBuffer(GL_UNIFORM_BUFFER, audioUBO_);
glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(AudioUBO), &audioData);
glBindBufferBase(GL_UNIFORM_BUFFER, 0, audioUBO_);
```

**Expected Gain:** 10-30% reduction in uniform calls (if CPU-bound)

### 1.2 IF CPU-bound: Implement UBO for Scene Data
**Goal:** Replace scene uniforms with UBO

**Expected Gain:** 10-20% reduction in uniform calls (if CPU-bound)

### 1.3 Disable Shader Hot-Reload by Default
**Goal:** Eliminate runtime shader recompilation

**Implementation:**
```cpp
// Default to false
bool hotReloadEnabled_ = false;

// Only enable with command line flag or UI toggle
```

**Expected Gain:** Eliminates stuttering during normal use

### 1.4 Add Detailed Performance Counters
**Goal:** Track specific costs

**Implementation:**
- Per-shader uniform call count
- Per-pass draw call count
- Per-pass GPU time
- Texture bind count

---

## Phase 2: Batch Rendering (IF CPU-bound, 2-3 days)

### 2.1 Create Global Vertex Buffer for Quad
**Goal:** Single fullscreen quad for all passes

### 2.2 Batch Procedural Layers
**Goal:** Render all procedural layers in single pass

**Expected Gain:** Reduce draw calls from 5-10 to 1-3 (if CPU-bound)

---

## Phase 3: Shader Optimization (IF GPU-bound, 3-5 days)

### 3.1 Profile Individual Shaders
**Goal:** Identify expensive shaders

**Implementation:**
- GPU timer query per shader
- Identify shader causing most GPU time

### 3.2 Optimize Expensive Shaders
**Implementation:**
- Reduce loop iterations
- Simplify complex calculations
- Use built-in functions (mix, smoothstep, etc.)
- Reduce texture lookups

### 3.3 Consider LOD (Level of Detail)
**Implementation:**
- Lower quality when FPS drops
- Simplified shaders for background layers

**Expected Gain:** 20-40% GPU time reduction (if GPU-bound)

---

## Phase 4: Advanced Optimizations (ONLY IF NEEDED)

### 4.1 Implement Uniform Buffer Object Pool
**Goal:** Efficient UBO management

### 4.2 Persistent Mapped Buffers
**Goal:** Eliminate glBufferSubData overhead

**Expected Gain:** 10-20% faster uniform updates (if CPU-bound)

### 4.3 Compute Shaders (ONLY for specific heavy workloads)
**Note:** NOT recommended for FFT unless profiling shows it's the bottleneck
- FFT is typically faster in CPU with FFTW
- GPU upload overhead often exceeds compute benefit
- Only consider if doing heavy per-pixel processing

---

## Expected Performance Gains (CONDITIONAL)

| Optimization | Expected Gain | Effort | Condition |
|-------------|---------------|---------|-----------|
| Phase 0: Profiling | Baseline data | 1 day | ALWAYS DO FIRST |
| UBO for Audio | 10-30% uniform reduction | 1 day | IF CPU-bound |
| UBO for Scene | 10-20% uniform reduction | 1 day | IF CPU-bound |
| Disable Hot-Reload | Eliminates stuttering | 0.5 day | ALWAYS |
| Batch Rendering | Reduce draw calls 5→3 | 2-3 days | IF CPU-bound |
| Shader Optimization | 20-40% GPU time reduction | 3-5 days | IF GPU-bound |
| State Optimization | 5-10% driver overhead | 1-2 days | IF profiling shows need |
| Persistent Mapped | 10-20% faster updates | 1-2 days | IF CPU-bound after UBOs |

**Total Expected Gain:** Depends on profiling results (not predetermined)

---

## Immediate Action Plan (START WITH PHASE 0)

### Day 1: Profiling (MANDATORY FIRST STEP)
1. **Add frame time tracking** (CPU + GPU)
2. **Add performance counters** (uniforms, draw calls)
3. **Add performance display to UI**
4. **Measure baseline performance**
5. **Determine: CPU-bound vs GPU-bound**

### After Day 1: Decision Point
- **If CPU-bound:** Proceed with Phase 1 (UBOs) → Phase 2 (Batch Rendering)
- **If GPU-bound:** Proceed with Phase 3 (Shader Optimization)
- **If neither:** Current performance is acceptable, stop here

---

## Success Criteria (AFTER PROFILING)

- [ ] Phase 0 completed: Have baseline performance data
- [ ] Identified bottleneck (CPU/GPU/Memory)
- [ ] Applied optimizations relevant to bottleneck
- [ ] Measured actual performance improvement
- [ ] Performance metrics visible in UI

---

## If These Optimizations Don't Help

If after Phase 1-2 you don't see significant improvement:

1. **Profile with actual tools:**
   - NVIDIA Nsight Graphics
   - AMD Radeon GPU Profiler
   - RenderDoc

2. **Identify real bottleneck:**
   - CPU-bound (likely uniforms/draw calls)
   - GPU-bound (shader complexity)
   - Memory-bound (texture bandwidth)

3. **Then decide:**
   - Continue optimizing OpenGL
   - Or proceed with Vulkan migration

---

## Code Changes Required

### Files to Modify:
- `src/visualizer.cpp` - Replace uniform calls with UBO updates
- `src/shader.cpp` - Add UBO support
- `src/modular_layer.cpp` - Batch rendering
- `src/post_processor.cpp` - State optimization
- `src/settings_manager.cpp` - Disable hot-reload default
- `src/visualizer_imgui.cpp` - Add performance metrics display

### New Files:
- `src/uniform_buffer.h/cpp` - UBO management
- `src/performance_counter.h/cpp` - Performance tracking
