#pragma once

#include <GL/glew.h>
#include <memory>
#include <array>

#include "shader.h"

class PostProcessor {
public:
    PostProcessor();
    ~PostProcessor();

    bool initialize(int width, int height);
    void shutdown();
    void resize(int width, int height);

    void beginCapture(int width, int height);
    void endCapture();
    void apply(int mode, float strength, float time, const std::array<float, 3>& colorAdjust, float bassLevel);

    bool isInitialized() const { return initialized_; }

private:
    bool createResources(int width, int height);
    void destroyResources();
    bool ensureShader();

    GLuint fbo_{0};
    GLuint colorTexture_{0};
    GLuint quadVAO_{0};
    GLuint quadVBO_{0};

    int width_{0};
    int height_{0};
    bool initialized_{false};

    GLint previousFbo_{0};
    GLint previousViewport_[4]{0, 0, 0, 0};

    std::unique_ptr<Shader> shader_;
};
