#include "modular_layer.h"

#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int kKaleidoscopeModeIndex = 23;

const char* kQuadVertexShader = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;

out vec2 vUV;

void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const std::array<const char*, 3> kShaderSearchRoots = {
    "shaders/",
    "../shaders/",
    "../../shaders/"
};

const std::array<const char*, 6> kProceduralShaderFiles = {
    "procedural_header.glsl",
    "procedural_helpers.glsl",
    "procedural_pack1.glsl",
    "procedural_pack2.glsl",
    "procedural_pack3.glsl",
    "procedural_main.glsl"
};

template <std::size_t N>
bool LoadShaderFiles(const std::array<const char*, 3>& roots,
                     const std::array<const char*, N>& files,
                     std::string& outSource) {
    for (const char* root : roots) {
        std::stringstream shaderStream;
        bool allLoaded = true;
        for (const char* file : files) {
            std::string path = std::string(root) + file;
            std::ifstream input(path, std::ios::in);
            if (!input.is_open()) {
                allLoaded = false;
                break;
            }
            shaderStream << input.rdbuf() << '\n';
        }
        if (allLoaded) {
            outSource = shaderStream.str();
            return true;
        }
    }
    return false;
}

const std::string& GetProceduralFragmentShaderSource() {
    static std::string source;
    if (source.empty()) {
        LoadShaderFiles(kShaderSearchRoots, kProceduralShaderFiles, source);
    }
    return source;
}

const char* kCompositeFragmentShader = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uLayerTex;
uniform float uOpacity;

void main() {
    vec4 color = texture(uLayerTex, vUV);
    FragColor = vec4(color.rgb, color.a * uOpacity);
}
)";

} // namespace

ModularLayer::ModularLayer() = default;

ModularLayer::~ModularLayer() {
    shutdown();
}

void ModularLayer::setColorPalette(const float primary[3], const float secondary[3], float blend) {
    if (primary) {
        colorPrimary_[0] = primary[0];
        colorPrimary_[1] = primary[1];
        colorPrimary_[2] = primary[2];
    }
    if (secondary) {
        colorSecondary_[0] = secondary[0];
        colorSecondary_[1] = secondary[1];
        colorSecondary_[2] = secondary[2];
    }
    if (blend < 0.0f) {
        colorBlend_ = 0.0f;
    } else if (blend > 1.0f) {
        colorBlend_ = 1.0f;
    } else {
        colorBlend_ = blend;
    }
}

bool ModularLayer::initialize(int width, int height) {
    if (initialized_) {
        resize(width, height);
        return true;
    }

    if (!createResources(width, height)) {
        std::cerr << "ModularLayer: failed to create framebuffer resources" << std::endl;
        return false;
    }

    if (!ensureShader() || !ensureCompositeShader()) {
        destroyResources();
        return false;
    }

    initialized_ = true;
    width_ = width;
    height_ = height;
    return true;
}

void ModularLayer::shutdown() {
    destroyResources();
    proceduralShader_.reset();
    compositeShader_.reset();
    kaleidoscopeShader_.reset();
    initialized_ = false;
}

void ModularLayer::resize(int width, int height) {
    if (!initialized_) {
        initialize(width, height);
        return;
    }

    if (width == width_ && height == height_) {
        return;
    }

    destroyResources();
    if (!createResources(width, height)) {
        std::cerr << "ModularLayer: failed to resize framebuffer" << std::endl;
        return;
    }
    width_ = width;
    height_ = height;
}

bool ModularLayer::createResources(int width, int height) {
    destroyResources();

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    glGenTextures(1, &colorTexture_);
    glBindTexture(GL_TEXTURE_2D, colorTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture_, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ModularLayer: framebuffer incomplete (status=" << std::hex << status << std::dec << ")" << std::endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        destroyResources();
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Fullscreen quad
    std::array<float, 16> quadData = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f
    };

    glGenVertexArrays(1, &quadVAO_);
    glGenBuffers(1, &quadVBO_);

    glBindVertexArray(quadVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
    glBufferData(GL_ARRAY_BUFFER, quadData.size() * sizeof(float), quadData.data(), GL_STATIC_DRAW);

    constexpr GLsizei stride = 4 * sizeof(float);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    width_ = width;
    height_ = height;

    return true;
}

void ModularLayer::destroyResources() {
    if (quadVBO_) {
        glDeleteBuffers(1, &quadVBO_);
        quadVBO_ = 0;
    }
    if (quadVAO_) {
        glDeleteVertexArrays(1, &quadVAO_);
        quadVAO_ = 0;
    }
    if (colorTexture_) {
        glDeleteTextures(1, &colorTexture_);
        colorTexture_ = 0;
    }
    if (fbo_) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }

    kaleidoscopeShader_.reset();
}

bool ModularLayer::ensureShader() {
    if (proceduralShader_) {
        return true;
    }

    const std::string& fragmentSource = GetProceduralFragmentShaderSource();
    if (fragmentSource.empty()) {
        std::cerr << "ModularLayer: failed to locate procedural shader sources" << std::endl;
        return false;
    }

    proceduralShader_ = std::make_unique<Shader>();
    if (!proceduralShader_->loadFromSource(kQuadVertexShader, fragmentSource)) {
        std::cerr << "ModularLayer: failed to compile procedural shader" << std::endl;
        proceduralShader_.reset();
        return false;
    }
    return true;
}

bool ModularLayer::ensureCompositeShader() {
    if (compositeShader_) {
        return true;
    }

    compositeShader_ = std::make_unique<Shader>();
    if (!compositeShader_->loadFromSource(kQuadVertexShader, kCompositeFragmentShader)) {
        std::cerr << "ModularLayer: failed to compile composite shader" << std::endl;
        compositeShader_.reset();
        return false;
    }
    return true;
}

bool ModularLayer::ensureKaleidoscopeShader() {
    if (kaleidoscopeShader_) {
        return true;
    }

    const std::array<const char*, 3> searchPaths = {
        "shaders/procedural_kaleidoscope.glsl",
        "../shaders/procedural_kaleidoscope.glsl",
        "../../shaders/procedural_kaleidoscope.glsl"
    };

    std::string fragmentSource;
    bool loaded = false;
    for (const char* path : searchPaths) {
        std::ifstream file(path, std::ios::in);
        if (!file.is_open()) {
            continue;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        fragmentSource = buffer.str();
        loaded = true;
        break;
    }

    if (!loaded) {
        std::cerr << "ModularLayer: unable to locate procedural_kaleidoscope.glsl" << std::endl;
        return false;
    }

    kaleidoscopeShader_ = std::make_unique<Shader>();
    if (!kaleidoscopeShader_->loadFromSource(kQuadVertexShader, fragmentSource)) {
        std::cerr << "ModularLayer: failed to compile kaleidoscope shader" << std::endl;
        kaleidoscopeShader_.reset();
        return false;
    }
    return true;
}

void ModularLayer::render(const LayerContext& context) {
    if (!initialized_ || (!enabled_ && !debugPreview_)) {
        return;
    }

    Shader* activeShader = nullptr;
    if (mode_ == kKaleidoscopeModeIndex) {
        if (!ensureKaleidoscopeShader()) {
            return;
        }
        activeShader = kaleidoscopeShader_.get();
    } else {
        if (!ensureShader()) {
            return;
        }
        activeShader = proceduralShader_.get();
    }

    GLint previousFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);
    GLint previousViewport[4];
    glGetIntegerv(GL_VIEWPORT, previousViewport);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, width_, height_);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (depthWasEnabled) {
        glDisable(GL_DEPTH_TEST);
    }

    activeShader->use();
    activeShader->setUniform2f("uResolution", static_cast<float>(width_), static_cast<float>(height_));
    activeShader->setUniform1f("uTime", context.time);
    activeShader->setUniform1f("uTempo", context.tempo);

    const auto* audio = context.audio;
    float energy = audio ? audio->energy : 0.0f;
    float bass = audio ? audio->bassEnergy : 0.0f;
    float mid = audio ? audio->midEnergy : 0.0f;
    float high = audio ? audio->highEnergy : 0.0f;

    activeShader->setUniform1f("uEnergy", energy);
    activeShader->setUniform1f("uBass", bass);
    activeShader->setUniform1f("uMid", mid);
    activeShader->setUniform1f("uHigh", high);
    activeShader->setUniform1f("uIntensity", context.intensity);
    activeShader->setUniform3f("uPrimaryColor", colorPrimary_[0], colorPrimary_[1], colorPrimary_[2]);
    activeShader->setUniform3f("uSecondaryColor", colorSecondary_[0], colorSecondary_[1], colorSecondary_[2]);
    activeShader->setUniform1f("uColorBlend", colorBlend_);

    if (activeShader == proceduralShader_.get()) {
        activeShader->setUniform1i("uMode", mode_);
    }

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glUseProgram(0);

    if (depthWasEnabled) {
        glEnable(GL_DEPTH_TEST);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, previousFbo);
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
}

void ModularLayer::composite(const LayerContext& context, float opacity) {
    if (!initialized_ || (!enabled_ && !debugPreview_)) {
        return;
    }

    if (!ensureCompositeShader()) {
        return;
    }

    if (opacity <= 0.0f && !debugPreview_) {
        return;
    }

    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (depthWasEnabled) {
        glDisable(GL_DEPTH_TEST);
    }

    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    if (!blendWasEnabled) {
        glEnable(GL_BLEND);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    compositeShader_->use();
    float finalOpacity = enabled_ ? opacity : opacity * 0.6f;
    compositeShader_->setUniform1f("uOpacity", finalOpacity);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorTexture_);
    compositeShader_->setUniform1i("uLayerTex", 0);

    GLint previousViewport[4];
    glGetIntegerv(GL_VIEWPORT, previousViewport);
    glViewport(0, 0, context.screenWidth, context.screenHeight);

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);

    if (!blendWasEnabled) {
        glDisable(GL_BLEND);
    }
    if (depthWasEnabled) {
        glEnable(GL_DEPTH_TEST);
    }
}
