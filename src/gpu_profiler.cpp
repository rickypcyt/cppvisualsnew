#include "gpu_profiler.h"
#include <iostream>
#include <algorithm>
#include <cmath>

bool GPUProfiler::initialize() {
    if (initialized_) return true;

    // Check if GL timestamp queries are available
    GLint queryBits = 0;
    glGetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS, &queryBits);
    
    if (queryBits < 64) {
        std::cerr << "[GPUProfiler] GL_TIMESTAMP not supported or insufficient bits: " << queryBits << std::endl;
        available_ = false;
        initialized_ = true;
        return false;
    }

    // Generate query objects for ring buffer
    for (int i = 0; i < QUERY_RING_SIZE; i++) {
        glGenQueries(ZONE_COUNT, queryRing_[i].start_queries);
        glGenQueries(ZONE_COUNT, queryRing_[i].end_queries);
        queryRing_[i].ready = false;
    }

    available_ = true;
    initialized_ = true;
    
    std::cout << "[GPUProfiler] Initialized with " << QUERY_RING_SIZE << " frame ring buffer" << std::endl;
    return true;
}

void GPUProfiler::shutdown() {
    if (!initialized_) return;

    for (int i = 0; i < QUERY_RING_SIZE; i++) {
        if (queryRing_[i].start_queries[0]) {
            glDeleteQueries(ZONE_COUNT, queryRing_[i].start_queries);
            glDeleteQueries(ZONE_COUNT, queryRing_[i].end_queries);
        }
    }

    initialized_ = false;
    available_ = false;
}

void GPUProfiler::beginFrame() {
    if (!available_) return;

    // Reset current frame metrics
    currentFrame_ = FrameMetrics{};
    contextSwitchesThisFrame_ = 0;
    contextSwitchTimeThisFrame_ = 0.0f;
    framesSinceInit_++;

    // Start frame GPU timestamp
    gpuZoneStart(ZONE_FRAME_START);
}

void GPUProfiler::endFrame() {
    if (!available_) return;

    // End frame GPU timestamp
    gpuZoneEnd(ZONE_FRAME_START);

    // Mark current ring entry as ready for next frame
    queryRing_[currentRingIndex_].ready = true;
    
    // Move to next ring entry
    currentRingIndex_ = (currentRingIndex_ + 1) % QUERY_RING_SIZE;

    // Try to read results from the oldest ready frame
    int readIndex = (currentRingIndex_ - 1 + QUERY_RING_SIZE) % QUERY_RING_SIZE;
    if (queryRing_[readIndex].ready) {
        FrameQueries& queries = queryRing_[readIndex];
        
        GLuint64 timestamps[ZONE_COUNT * 2];
        bool allReady = true;
        
        for (int i = 0; i < ZONE_COUNT && allReady; i++) {
            if (!getQueryResult(queries.start_queries[i], &timestamps[i * 2])) {
                allReady = false;
            }
            if (!getQueryResult(queries.end_queries[i], &timestamps[i * 2 + 1])) {
                allReady = false;
            }
        }

        if (allReady) {
            // Calculate GPU timings (nanoseconds to milliseconds)
            double nsToMs = 1e-6;
            
            // Main render
            currentFrame_.gpu_main_render = (timestamps[ZONE_MAIN_RENDER * 2 + 1] - timestamps[ZONE_MAIN_RENDER * 2]) * nsToMs;
            // ImGui render
            currentFrame_.gpu_imgui_render = (timestamps[ZONE_IMGUI_RENDER * 2 + 1] - timestamps[ZONE_IMGUI_RENDER * 2]) * nsToMs;
            // Blit
            currentFrame_.gpu_blit = (timestamps[ZONE_BLIT * 2 + 1] - timestamps[ZONE_BLIT * 2]) * nsToMs;
            // Total GPU
            currentFrame_.gpu_total = (timestamps[ZONE_FRAME_START * 2 + 1] - timestamps[ZONE_FRAME_START * 2]) * nsToMs;

            // Store in history for statistics
            for (int i = 0; i < ZONE_COUNT; i++) {
                float duration = (timestamps[i * 2 + 1] - timestamps[i * 2]) * nsToMs;
                gpuHistory_[i].push_back(duration);
                if (gpuHistory_[i].size() > 1000) {
                    gpuHistory_[i].erase(gpuHistory_[i].begin());
                }
            }

            currentFrame_.valid = true;
            queries.ready = false;
        }
    }

    // Store context switch metrics
    currentFrame_.context_switches = contextSwitchesThisFrame_;

    // Store as last valid frame
    if (currentFrame_.valid) {
        lastValidFrame_ = currentFrame_;
    }
    currentFrame_.context_switch_time = contextSwitchTimeThisFrame_;
}

void GPUProfiler::cpuZoneStart(const char* name) {
    auto now = std::chrono::high_resolution_clock::now();
    cpuTimers_[name].start = now;
    cpuTimers_[name].active = true;
}

void GPUProfiler::cpuZoneEnd(const char* name) {
    auto it = cpuTimers_.find(name);
    if (it == cpuTimers_.end() || !it->second.active) return;

    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<float, std::milli>(now - it->second.start).count();
    
    cpuTimers_[name].active = false;
    cpuHistory_[name].push_back(duration);
    if (cpuHistory_[name].size() > 1000) {
        cpuHistory_[name].erase(cpuHistory_[name].begin());
    }

    // Update current frame metrics
    if (strcmp(name, "main_render") == 0) currentFrame_.cpu_main_render = duration;
    else if (strcmp(name, "imgui_render") == 0) currentFrame_.cpu_imgui_render = duration;
    else if (strcmp(name, "blit") == 0) currentFrame_.cpu_blit = duration;
    else if (strcmp(name, "swap") == 0) currentFrame_.cpu_swap = duration;
}

void GPUProfiler::gpuZoneStart(GPUZone zone) {
    if (!available_) return;
    glQueryCounter(queryRing_[currentRingIndex_].start_queries[zone], GL_TIMESTAMP);
}

void GPUProfiler::gpuZoneEnd(GPUZone zone) {
    if (!available_) return;
    glQueryCounter(queryRing_[currentRingIndex_].end_queries[zone], GL_TIMESTAMP);
}

void GPUProfiler::beforeSwap() {
    swapStart_ = std::chrono::high_resolution_clock::now();
}

void GPUProfiler::afterSwap() {
    auto now = std::chrono::high_resolution_clock::now();
    auto latency = std::chrono::duration<float, std::milli>(now - swapStart_).count();
    
    presentLatencyHistory_.push_back(latency);
    if (presentLatencyHistory_.size() > 1000) {
        presentLatencyHistory_.erase(presentLatencyHistory_.begin());
    }
    
    currentFrame_.present_latency = latency;
}

void GPUProfiler::beforeContextSwitch() {
    contextSwitchStart_ = std::chrono::high_resolution_clock::now();
}

void GPUProfiler::afterContextSwitch() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<float, std::milli>(now - contextSwitchStart_).count();
    
    contextSwitchesThisFrame_++;
    contextSwitchTimeThisFrame_ += duration;
    
    contextSwitchHistory_.push_back(duration);
    if (contextSwitchHistory_.size() > 1000) {
        contextSwitchHistory_.erase(contextSwitchHistory_.begin());
    }
}

GPUProfiler::ZoneStats GPUProfiler::getGPUZoneStats(GPUZone zone) const {
    ZoneStats stats;
    
    if (zone < 0 || zone >= ZONE_COUNT) return stats;
    
    const auto& data = gpuHistory_[zone];
    if (data.empty()) return stats;

    std::vector<float> sorted = data;
    std::sort(sorted.begin(), sorted.end());

    stats.min_ms = sorted.front();
    stats.max_ms = sorted.back();
    stats.sample_count = sorted.size();

    float sum = 0.0f;
    for (float val : sorted) sum += val;
    stats.avg_ms = sum / sorted.size();

    stats.median_ms = sorted[sorted.size() / 2];
    stats.p99_ms = sorted[static_cast<size_t>(sorted.size() * 0.99)];

    return stats;
}

void GPUProfiler::resetStats() {
    for (int i = 0; i < ZONE_COUNT; i++) {
        gpuHistory_[i].clear();
    }
    cpuHistory_.clear();
    presentLatencyHistory_.clear();
    contextSwitchHistory_.clear();
}

bool GPUProfiler::getQueryResult(GLuint query, GLuint64* result) {
    GLint available = 0;
    glGetQueryObjectiv(query, GL_QUERY_RESULT_AVAILABLE, &available);

    if (!available) return false;

    glGetQueryObjectui64v(query, GL_QUERY_RESULT, result);
    return true;
}
