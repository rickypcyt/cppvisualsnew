#pragma once

#include <GL/glew.h>
#include <memory>
#include <array>
#include <vector>
#include <thread>
#include <atomic>
#include <filesystem>

#include "shader.h"

class PostProcessor {
public:
    struct PostEffectPass {
        int mode = 0;
        float strength = 0.0f;
        std::array<float, 3> rgbAdjust{1.0f, 1.0f, 1.0f};
    };

    PostProcessor();
    ~PostProcessor();

    bool initialize(int width, int height);
    void shutdown();
    void resize(int width, int height);

    void beginCapture(int width, int height);
    void endCapture();
    void apply(int mode, float strength, float time, const std::array<float, 3>& colorAdjust, float bassLevel);
    void applyChain(const std::vector<PostEffectPass>& passes, float time, float bassLevel);

    bool isInitialized() const { return initialized_; }

    // Hot-reload methods
    void enableHotReload(bool enabled);
    bool isHotReloadEnabled() const { return hotReloadEnabled_; }
    void reloadShaders();
    bool shouldReloadShaders();

private:
    bool createResources(int width, int height);
    void destroyResources();
    bool ensureShader();

    GLuint fbo_{0};
    GLuint colorTexture_{0};
    GLuint quadVAO_{0};
    GLuint quadVBO_{0};
    std::array<GLuint, 2> pingFbos_{0, 0};
    std::array<GLuint, 2> pingTextures_{0, 0};

    int width_{0};
    int height_{0};
    bool initialized_{false};

    GLint previousFbo_{0};
    GLint previousViewport_[4]{0, 0, 0, 0};

    std::unique_ptr<Shader> shader_;
    std::string commonShaderSource_;
    std::vector<std::unique_ptr<Shader>> effectShaders_;
    std::vector<std::string> effectPaths_;

    // Hot-reload state
    std::atomic<bool> hotReloadEnabled_{false};
    std::atomic<bool> shouldWatchFiles_{false};
    std::thread fileWatcherThread_;
    std::filesystem::file_time_type lastShaderModifyTime_;
    static constexpr auto WATCH_INTERVAL = std::chrono::milliseconds(500);

    void watchShaderFiles();

    bool loadEffectShaders();
    Shader* getEffectShader(int mode);
};
