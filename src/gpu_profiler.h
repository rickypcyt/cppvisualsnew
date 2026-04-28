#ifndef GPU_PROFILER_H
#define GPU_PROFILER_H

#include <GL/glew.h>
#include <chrono>
#include <string>
#include <vector>
#include <array>
#include <cstring>
#include <unordered_map>

class GPUProfiler {
public:
    // GPU zones for timestamp queries
    enum GPUZone {
        ZONE_FRAME_START = 0,
        ZONE_MAIN_RENDER,
        ZONE_IMGUI_RENDER,
        ZONE_BLIT,
        ZONE_COUNT
    };

    // Statistics for a zone
    struct ZoneStats {
        float avg_ms = 0.0f;
        float min_ms = 0.0f;
        float median_ms = 0.0f;
        float max_ms = 0.0f;
        float p99_ms = 0.0f;
        size_t sample_count = 0;
    };

    // Frame metrics
    struct FrameMetrics {
        // CPU timings (ms)
        float cpu_main_render = 0.0f;
        float cpu_imgui_render = 0.0f;
        float cpu_blit = 0.0f;
        float cpu_swap = 0.0f;

        // GPU timings (ms)
        float gpu_main_render = 0.0f;
        float gpu_imgui_render = 0.0f;
        float gpu_blit = 0.0f;
        float gpu_total = 0.0f;

        // Present latency (ms)
        float present_latency = 0.0f;

        // Context switches
        int context_switches = 0;
        float context_switch_time = 0.0f;

        bool valid = false;
    };

    static GPUProfiler& getInstance() {
        static GPUProfiler instance;
        return instance;
    }

    // Initialize GPU profiler (call after GL context creation)
    bool initialize();
    void shutdown();

    // Frame management
    void beginFrame();
    void endFrame();

    // CPU timing (use these around code sections)
    void cpuZoneStart(const char* name);
    void cpuZoneEnd(const char* name);

    // GPU timing (inserts GL timestamp queries)
    void gpuZoneStart(GPUZone zone);
    void gpuZoneEnd(GPUZone zone);

    // Present latency tracking
    void beforeSwap();
    void afterSwap();

    // Context switch tracking
    void beforeContextSwitch();
    void afterContextSwitch();

    // Get current frame metrics (call after endFrame)
    FrameMetrics getCurrentFrameMetrics() const {
        // Return last valid frame if current is not ready yet
        if (currentFrame_.valid) {
            return currentFrame_;
        }
        return lastValidFrame_;
    }

    // Get statistics for a GPU zone
    ZoneStats getGPUZoneStats(GPUZone zone) const;

    // Reset statistics
    void resetStats();

    // Check if GPU timestamp queries are available
    bool isAvailable() const { return available_; }

    // Get frame count since initialization
    int getFramesSinceInit() const { return framesSinceInit_; }

private:

    GPUProfiler() = default;
    ~GPUProfiler() { shutdown(); }

    // Ring buffer for frame queries (to avoid synchronization issues)
    static constexpr int QUERY_RING_SIZE = 4; // 4 frames in flight
    
    struct FrameQueries {
        GLuint start_queries[ZONE_COUNT];
        GLuint end_queries[ZONE_COUNT];
        bool ready = false;
    };

    FrameQueries queryRing_[QUERY_RING_SIZE];
    int currentRingIndex_ = 0;

    // CPU timing
    struct CPUTimer {
        std::chrono::high_resolution_clock::time_point start;
        bool active = false;
    };

    std::unordered_map<std::string, CPUTimer> cpuTimers_;
    std::unordered_map<std::string, std::vector<float>> cpuHistory_;

    // Present latency
    std::chrono::high_resolution_clock::time_point swapStart_;
    std::vector<float> presentLatencyHistory_;

    // Context switch tracking
    std::chrono::high_resolution_clock::time_point contextSwitchStart_;
    int contextSwitchesThisFrame_ = 0;
    float contextSwitchTimeThisFrame_ = 0.0f;
    std::vector<float> contextSwitchHistory_;

    // GPU history
    std::vector<float> gpuHistory_[ZONE_COUNT];

    // Current frame metrics
    FrameMetrics currentFrame_;
    FrameMetrics lastValidFrame_;

    // Frame counter for warmup
    int framesSinceInit_ = 0;

    // Availability
    bool available_ = false;
    bool initialized_ = false;

    // Helper to get timestamp in nanoseconds from query
    bool getQueryResult(GLuint query, GLuint64* result);
};

// Convenience macros for CPU timing
#define GPU_CPU_ZONE(name) \
    GPUProfiler::getInstance().cpuZoneStart(name); \
    struct GPUCpuZoneEnd_##name { \
        ~GPUCpuZoneEnd_##name() { GPUProfiler::getInstance().cpuZoneEnd(name); } \
    } _gpu_cpu_zone_##name;

#endif // GPU_PROFILER_H
