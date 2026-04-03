#pragma once

#include <GL/glew.h>
#include <memory>
#include <thread>
#include <atomic>
#include <filesystem>
#include <chrono>

#include "audio_analyzer.h"
#include "shader.h"

struct LayerContext {
    int screenWidth;
    int screenHeight;
    float time;
    float tempo;
    const AudioAnalyzer::AudioFeatures* audio;
    float intensity;
};

class ModularLayer {
public:
    ModularLayer();
    ~ModularLayer();

    bool initialize(int width, int height);
    void shutdown();
    void resize(int width, int height);

    void render(const LayerContext& context, bool clearFramebuffer = true);
    void composite(const LayerContext& context, float opacity);

    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }
    void setDebugPreview(bool enabled) { debugPreview_ = enabled; }
    bool debugPreview() const { return debugPreview_; }
    void setMode(int mode) { mode_ = mode; }
    int mode() const { return mode_; }
    void setColorPalette(const float primary[3], const float secondary[3], float blend);
    
    // Camera control
    void setCameraZoom(float zoom) { cameraZoom_ = zoom; }
    float cameraZoom() const { return cameraZoom_; }
    void setCameraOffset(float x, float y) { cameraOffsetX_ = x; cameraOffsetY_ = y; }
    float cameraOffsetX() const { return cameraOffsetX_; }
    float cameraOffsetY() const { return cameraOffsetY_; }
    
    // Hot-reload methods
    void enableHotReload(bool enabled);
    bool isHotReloadEnabled() const { return hotReloadEnabled_; }
    void reloadShaders(); // Public manual reload

private:
    bool createResources(int width, int height);
    void destroyResources();
    bool ensureShader();
    bool ensureCompositeShader();
    bool ensureKaleidoscopeShader();
    
    // Hot-reload private methods
    void watchShaderFiles();
    bool shouldReloadShaders();
    
    // Hot-reload members
    std::thread fileWatcherThread_;
    std::atomic<bool> hotReloadEnabled_{false};
    std::atomic<bool> shouldWatchFiles_{false};
    std::filesystem::file_time_type lastShaderModifyTime_;
    static constexpr std::chrono::milliseconds WATCH_INTERVAL{500};

    GLuint fbo_{0};
    GLuint colorTexture_{0};
    GLuint quadVAO_{0};
    GLuint quadVBO_{0};
    int width_{0};
    int height_{0};
    bool initialized_{false};
    bool enabled_{true};
    bool debugPreview_{false};
    int mode_{0};
    float colorPrimary_[3]{1.0f, 1.0f, 1.0f};
    float colorSecondary_[3]{1.0f, 1.0f, 1.0f};
    float colorBlend_{0.0f};
    
    // Camera control
    float cameraZoom_{1.0f};
    float cameraOffsetX_{0.0f};
    float cameraOffsetY_{0.0f};

    std::unique_ptr<Shader> proceduralShader_;
    std::unique_ptr<Shader> compositeShader_;
    std::unique_ptr<Shader> kaleidoscopeShader_;
};
