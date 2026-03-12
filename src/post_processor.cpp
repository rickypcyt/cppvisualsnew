#include "post_processor.h"

#include <array>
#include <iostream>
#include <fstream>
#include <sstream>

namespace {

const char *kPostVertexShader = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;

out vec2 vUV;

void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

std::string loadShaderFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "PostProcessor: Failed to open shader file: " << filepath << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string stripIncludeDirective(const std::string& source, const std::string& includeName) {
    std::istringstream input(source);
    std::ostringstream output;
    std::string line;
    bool first = true;

    while (std::getline(input, line)) {
        if (line.find("#include") != std::string::npos && line.find(includeName) != std::string::npos) {
            continue;
        }

        if (!first) {
            output << '\n';
        }
        output << line;
        first = false;
    }

    return output.str();
}

std::string combineShaderSources(const std::string& commonSource, const std::string& effectSource) {
    std::string processedEffect = stripIncludeDirective(effectSource, "post_common.glsl");
    if (!processedEffect.empty()) {
        return commonSource + "\n" + processedEffect;
    }
    return commonSource;
}

} // anonymous namespace

PostProcessor::PostProcessor() = default;

PostProcessor::~PostProcessor() { shutdown(); }

bool PostProcessor::initialize(int width, int height) {
    if (initialized_) {
        resize(width, height);
        return true;
    }

    if (!loadEffectShaders()) {
        std::cerr << "PostProcessor: Failed to load effect shaders" << std::endl;
        return false;
    }

    if (!createResources(width, height)) {
        return false;
    }

    initialized_ = true;
    width_ = width;
    height_ = height;
    return true;
}

void PostProcessor::shutdown() {
    destroyResources();
    shader_.reset();
    initialized_ = false;
}

void PostProcessor::resize(int width, int height) {
    if (!initialized_) {
        initialize(width, height);
        return;
    }

    if (width == width_ && height == height_) {
        return;
    }

    destroyResources();
    if (!createResources(width, height)) {
        std::cerr << "PostProcessor: failed to resize framebuffer" << std::endl;
        return;
    }
    width_ = width;
    height_ = height;
}

void PostProcessor::beginCapture(int width, int height) {
    if (!initialized_) {
        initialize(width, height);
    } else if (width != width_ || height != height_) {
        resize(width, height);
    }

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo_);
    glGetIntegerv(GL_VIEWPORT, previousViewport_);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, width_, height_);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void PostProcessor::endCapture() {
    glBindFramebuffer(GL_FRAMEBUFFER, previousFbo_);
    glViewport(previousViewport_[0], previousViewport_[1], previousViewport_[2],
               previousViewport_[3]);
}

void PostProcessor::apply(int mode, float strength, float time,
                          const std::array<float, 3> &colorAdjust, float bassLevel) {
    if (!initialized_) {
        return;
    }

    PostEffectPass pass;
    pass.mode = mode;
    pass.strength = strength;
    pass.rgbAdjust = colorAdjust;

    std::vector<PostEffectPass> passes;
    if (mode != 0 && strength > 0.0f) {
        passes.push_back(pass);
    }

    applyChain(passes, time, bassLevel);
}

void PostProcessor::applyChain(const std::vector<PostEffectPass> &passes, float time,
                               float bassLevel) {
    if (!initialized_) {
        return;
    }

    glDisable(GL_DEPTH_TEST);

    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    if (!blendWasEnabled) {
        glEnable(GL_BLEND);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLuint currentTexture = colorTexture_;
    int pingIndex = 0;
    bool drewPass = false;

    for (const auto &pass : passes) {
        if (pass.mode == 0 || pass.strength <= 0.0f) {
            continue;
        }

        auto effectShader = getEffectShader(pass.mode);
        if (!effectShader) {
            std::cerr << "PostProcessor: Invalid effect mode " << pass.mode << std::endl;
            continue;
        }

        effectShader->use();
        effectShader->setUniform1f("uTime", time);
        effectShader->setUniform2f("uResolution", static_cast<float>(width_), static_cast<float>(height_));
        effectShader->setUniform1f("uBassLevel", bassLevel);
        effectShader->setUniform1f("uStrength", pass.strength);
        effectShader->setUniform3f("uRgbAdjust", pass.rgbAdjust[0], pass.rgbAdjust[1], pass.rgbAdjust[2]);

        glActiveTexture(GL_TEXTURE0);
        effectShader->setUniform1i("uScene", 0);
        glBindTexture(GL_TEXTURE_2D, currentTexture);

        glBindFramebuffer(GL_FRAMEBUFFER, pingFbos_[pingIndex]);
        glViewport(0, 0, width_, height_);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glBindVertexArray(quadVAO_);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        currentTexture = pingTextures_[pingIndex];
        pingIndex = 1 - pingIndex;
        drewPass = true;
    }

    if (!drewPass) {
        // No effects applied, just render the original scene
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width_, height_);
        
        auto passthroughShader = getEffectShader(0);
        if (passthroughShader) {
            passthroughShader->use();
            passthroughShader->setUniform1i("uScene", 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, colorTexture_);
            glBindVertexArray(quadVAO_);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glBindVertexArray(0);
        }
    } else {
        // Render the final result
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width_, height_);
        
        auto passthroughShader = getEffectShader(0);
        if (passthroughShader) {
            passthroughShader->use();
            passthroughShader->setUniform1i("uScene", 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, currentTexture);
            glBindVertexArray(quadVAO_);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glBindVertexArray(0);
        }
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    if (!blendWasEnabled) {
        glDisable(GL_BLEND);
    }
    glEnable(GL_DEPTH_TEST);
}

bool PostProcessor::createResources(int width, int height) {
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
        std::cerr << "PostProcessor: framebuffer incomplete" << std::endl;
        destroyResources();
        return false;
    }

    for (int i = 0; i < 2; ++i) {
        glGenFramebuffers(1, &pingFbos_[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, pingFbos_[i]);

        glGenTextures(1, &pingTextures_[i]);
        glBindTexture(GL_TEXTURE_2D, pingTextures_[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               pingTextures_[i], 0);

        status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "PostProcessor: ping-pong framebuffer incomplete" << std::endl;
            destroyResources();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenVertexArrays(1, &quadVAO_);
    glBindVertexArray(quadVAO_);

    glGenBuffers(1, &quadVBO_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
    glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), nullptr, GL_STATIC_DRAW);

    std::array<float, 16> quadData = {-1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f,
                                      -1.0f, 1.0f,  0.0f, 1.0f, 1.0f, 1.0f,  1.0f, 1.0f};

    glBufferData(GL_ARRAY_BUFFER, quadData.size() * sizeof(float), quadData.data(), GL_STATIC_DRAW);

    constexpr GLsizei stride = 4 * sizeof(float);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void *>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    width_ = width;
    height_ = height;

    return true;
}

void PostProcessor::destroyResources() {
    for (GLuint &tex : pingTextures_) {
        if (tex) {
            glDeleteTextures(1, &tex);
            tex = 0;
        }
    }
    for (GLuint &fbo : pingFbos_) {
        if (fbo) {
            glDeleteFramebuffers(1, &fbo);
            fbo = 0;
        }
    }
    if (colorTexture_) {
        glDeleteTextures(1, &colorTexture_);
        colorTexture_ = 0;
    }
    if (quadVBO_) {
        glDeleteBuffers(1, &quadVBO_);
        quadVBO_ = 0;
    }
    if (quadVAO_) {
        glDeleteVertexArrays(1, &quadVAO_);
        quadVAO_ = 0;
    }
    if (fbo_) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
}

bool PostProcessor::ensureShader() {
    // This method is no longer needed as we load individual shaders
    return true;
}

bool PostProcessor::loadEffectShaders() {
    // Load common shader source
    commonShaderSource_ = loadShaderFile("shaders/post_effects/post_common.glsl");
    if (commonShaderSource_.empty()) {
        std::cerr << "PostProcessor: Failed to load common shader source" << std::endl;
        return false;
    }

    // Define effect file paths in mode order
    effectPaths_ = {
        "shaders/post_effects/effect_passthrough.glsl",
        "shaders/post_effects/effect_grayscale.glsl",
        "shaders/post_effects/effect_filmic.glsl",
        "shaders/post_effects/effect_crt.glsl",
        "shaders/post_effects/effect_chromatic_pulse.glsl",
        "shaders/post_effects/effect_bass_threshold.glsl",
        "shaders/post_effects/effect_radial_blur.glsl",
        "shaders/post_effects/effect_kaleidoscope.glsl",
        "shaders/post_effects/effect_digital_glitch.glsl",
        "shaders/post_effects/effect_pixelate_64.glsl",
        "shaders/post_effects/effect_pixelate_128.glsl",
        "shaders/post_effects/effect_pixelate_192.glsl",
        "shaders/post_effects/effect_pixelate_256.glsl",
        "shaders/post_effects/effect_lens_distort.glsl",
        "shaders/post_effects/effect_rotating_lens.glsl",
        "shaders/post_effects/effect_plasma_overlay.glsl",
        "shaders/post_effects/effect_rgb_shift.glsl",
        "shaders/post_effects/effect_recursive_energy.glsl",
        "shaders/post_effects/effect_bloom_aces.glsl",
        "shaders/post_effects/effect_pixel_tiles.glsl",
        "shaders/post_effects/effect_sobel_edge.glsl",
        "shaders/post_effects/effect_kaleidoscope_mirror.glsl",
        "shaders/post_effects/effect_sobel_advanced.glsl"
    };

    effectShaders_.resize(effectPaths_.size());

    // Load and compile each effect shader
    size_t compiledCount = 0;
    for (size_t i = 0; i < effectPaths_.size(); ++i) {
        std::string effectSource = loadShaderFile(effectPaths_[i]);
        if (effectSource.empty()) {
            std::cerr << "PostProcessor: Failed to load effect shader: " << effectPaths_[i] << std::endl;
            return false;
        }

        std::string fullSource = combineShaderSources(commonShaderSource_, effectSource);
        
        auto shader = std::make_unique<Shader>();
        if (!shader->loadFromSource(kPostVertexShader, fullSource.c_str())) {
            std::cerr << "PostProcessor: Failed to compile effect shader: " << effectPaths_[i] << " (index " << i << ")" << std::endl;
            // Don't return false, continue with other shaders
            continue;
        }
        
        effectShaders_[i] = std::move(shader);
        ++compiledCount;
    }
    
    std::cout << "PostProcessor: Loaded " << compiledCount
              << " of " << effectPaths_.size() << " post-process shaders successfully" << std::endl;

    return true;
}

Shader* PostProcessor::getEffectShader(int mode) {
    if (mode < 0 || static_cast<size_t>(mode) >= effectShaders_.size()) {
        return nullptr;
    }
    if (!effectShaders_[mode]) {
        std::cerr << "PostProcessor: Effect shader " << mode << " failed to compile during initialization" << std::endl;
        return nullptr;
    }
    return effectShaders_[mode].get();
}
