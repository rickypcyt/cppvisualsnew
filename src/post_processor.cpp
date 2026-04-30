#include "post_processor.h"

#include <array>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

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

std::string combineShaderSources(const std::string& commonSource, const std::string& mirrorCommonSource, const std::string& effectSource) {
    // Remove post_common.glsl include and prepend the actual content
    std::string processedEffect = stripIncludeDirective(effectSource, "post_common.glsl");
    
    // If this effect uses mirror_common.glsl, we need to insert it before the effect code
    if (!mirrorCommonSource.empty() && processedEffect.find("#include \"mirror_common.glsl\"") != std::string::npos) {
        processedEffect = stripIncludeDirective(processedEffect, "mirror_common.glsl");
        return commonSource + "\n" + mirrorCommonSource + "\n" + processedEffect;
    }
    
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
    // Stop hot-reload thread first
    if (hotReloadEnabled_) {
        enableHotReload(false);
    }
    
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

void PostProcessor::clearAccumulation() {
    if (!initialized_) {
        return;
    }

    // Clear both ping-pong framebuffers to remove ghosting/burn-in
    for (int i = 0; i < 2; ++i) {
        if (pingFbos_[i] != 0) {
            glBindFramebuffer(GL_FRAMEBUFFER, pingFbos_[i]);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }
    }

    // Also clear the main color texture
    if (fbo_ != 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // Disabled - statistics will be printed on shutdown instead
    // std::cout << "PostProcessor: Accumulation buffers cleared (ghosting reset)" << std::endl;
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
    // Alpha 0 para evitar acumulación en blending
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
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

    // Disable blending for post-processing chain - effects should replace, not blend
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    if (blendWasEnabled) {
        glDisable(GL_BLEND);
    }

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
        // Alpha 0 para evitar acumulación en blending
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glBindVertexArray(quadVAO_);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        currentTexture = pingTextures_[pingIndex];
        pingIndex = 1 - pingIndex;
        drewPass = true;
    }

    // Store which texture has the final result
    lastOutputTexture_ = drewPass ? currentTexture : colorTexture_;

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

    // Restore blending state
    if (blendWasEnabled) {
        glEnable(GL_BLEND);
    }
    glEnable(GL_DEPTH_TEST);
}

GLuint PostProcessor::getOutputTexture() const {
    return lastOutputTexture_;
}

bool PostProcessor::createResources(int width, int height) {
    destroyResources();

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    glGenTextures(1, &colorTexture_);
    glBindTexture(GL_TEXTURE_2D, colorTexture_);
    // Inicializar a cero para evitar ghosting de basura de VRAM
    std::vector<GLubyte> zeroData(width * height * 4, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, zeroData.data());
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
        // Inicializar a cero para evitar ghosting
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, zeroData.data());
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
    // Load common shader sources
    commonShaderSource_ = loadShaderFile("shaders/post_effects/post_common.glsl");
    if (commonShaderSource_.empty()) {
        std::cerr << "PostProcessor: Failed to load common shader source" << std::endl;
        return false;
    }
    
    // Load mirror common shader source
    std::string mirrorCommonSource = loadShaderFile("shaders/post_effects/mirror_common.glsl");
    if (mirrorCommonSource.empty()) {
        std::cerr << "PostProcessor: Failed to load mirror_common.glsl (optional)" << std::endl;
    }

    // Define effect file paths in mode order (must match kPostProcessModes in visualizer_imgui.cpp)
    effectPaths_ = {
        "shaders/post_effects/effect_passthrough.glsl",    // 0: None
        "shaders/post_effects/effect_grayscale.glsl",    // 1: Grayscale
        "shaders/post_effects/effect_filmic.glsl",        // 2: Filmic + Vignette
        "shaders/post_effects/effect_crt.glsl",           // 3: CRT Monitor
        "shaders/post_effects/effect_chromatic_pulse.glsl", // 4: Chromatic Pulse
        "shaders/post_effects/effect_bass_threshold.glsl",  // 5: Bass Threshold
        "shaders/post_effects/effect_radial_blur.glsl",   // 6: Radial Blur
        "shaders/post_effects/effect_kaleidoscope.glsl",  // 7: Kaleidoscope
        "shaders/post_effects/effect_digital_glitch.glsl", // 8: Digital Glitch
        "shaders/post_effects/effect_pixelate_64.glsl",   // 9: Pixelate 64px
        "shaders/post_effects/effect_pixelate_128.glsl",  // 10: Pixelate 128px
        "shaders/post_effects/effect_pixelate_192.glsl",  // 11: Pixelate 192px
        "shaders/post_effects/effect_pixelate_256.glsl",  // 12: Pixelate 256px
        "shaders/post_effects/effect_lens_distort.glsl",  // 13: Lens Distortion
        "shaders/post_effects/effect_rotating_lens.glsl", // 14: Rotating Lens (was Plasma Overlay in UI - mismatched!)
        "shaders/post_effects/effect_plasma_overlay.glsl", // 15: Plasma Overlay
        "shaders/post_effects/effect_rgb_shift.glsl",      // 16: RGB Split
        "shaders/post_effects/effect_recursive_energy.glsl", // 17: Recursive Energy
        "shaders/post_effects/effect_bloom_aces.glsl",    // 18: Bloom + ACES
        "shaders/post_effects/effect_pixel_tiles.glsl",   // 19: Pixel Tiles
        "shaders/post_effects/effect_sobel_edge.glsl",    // 20: Sobel Edge Detection
        "shaders/post_effects/effect_kaleidoscope_mirror.glsl", // 21: Kaleidoscope Mirror
        "shaders/post_effects/effect_sobel_advanced.glsl", // 22: Advanced Sobel
        "shaders/post_effects/effect_ring_distortion.glsl", // 23: Ring Distortion
        "shaders/post_effects/effect_mirror_horizontal.glsl", // 24: Mirror Horizontal
        "shaders/post_effects/effect_mirror_vertical.glsl",   // 25: Mirror Vertical
        "shaders/post_effects/effect_mirror_kaleido.glsl",    // 26: Mirror Kaleidoscope
        "shaders/post_effects/effect_mirror_rorschach.glsl",  // 27: Mirror Rorschach
        "shaders/post_effects/effect_posterize_edge.glsl",    // 28: Posterize + Edge
        "shaders/post_effects/effect_little_planet.glsl",      // 29: Little Planet
        "shaders/post_effects/effect_recursive_feedback.glsl",  // 30: Recursive Feedback
        "shaders/post_effects/effect_threshold_levels.glsl",     // 31: Threshold Levels
        "shaders/post_effects/effect_warp.glsl"                  // 32: Warp
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

        std::string fullSource = combineShaderSources(commonShaderSource_, mirrorCommonSource, effectSource);
        
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

// Hot-reload implementation
void PostProcessor::enableHotReload(bool enabled) {
    if (hotReloadEnabled_ == enabled) return;
    
    hotReloadEnabled_ = enabled;
    
    if (enabled) {
        lastShaderModifyTime_ = std::filesystem::file_time_type::min();
        shouldWatchFiles_ = true;
        fileWatcherThread_ = std::thread(&PostProcessor::watchShaderFiles, this);
        std::cout << "Hot-reload enabled for post-processing shaders" << std::endl;
    } else {
        shouldWatchFiles_ = false;
        if (fileWatcherThread_.joinable()) {
            fileWatcherThread_.join();
        }
        std::cout << "Hot-reload disabled for post-processing shaders" << std::endl;
    }
}

void PostProcessor::watchShaderFiles() {
    while (shouldWatchFiles_) {
        std::this_thread::sleep_for(WATCH_INTERVAL);
        
        if (!hotReloadEnabled_ || !initialized_) continue;
        
        if (shouldReloadShaders()) {
            reloadShaders();
        }
    }
}

bool PostProcessor::shouldReloadShaders() {
    try {
        std::filesystem::file_time_type latestTime = std::filesystem::file_time_type::min();
        
        // Check all post-effect shader files
        for (const auto& shaderPath : effectPaths_) {
            if (std::filesystem::exists(shaderPath)) {
                auto currentTime = std::filesystem::last_write_time(shaderPath);
                if (currentTime > latestTime) {
                    latestTime = currentTime;
                }
            }
        }
        
        // Also check common shader
        std::string commonPath = "shaders/post_effects/post_common.glsl";
        if (std::filesystem::exists(commonPath)) {
            auto currentTime = std::filesystem::last_write_time(commonPath);
            if (currentTime > latestTime) {
                latestTime = currentTime;
            }
        }
        
        if (latestTime > lastShaderModifyTime_) {
            lastShaderModifyTime_ = latestTime;
            return true;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error watching post-process shaders: " << e.what() << std::endl;
    }
    
    return false;
}

void PostProcessor::reloadShaders() {
    std::cout << "Reloading post-processing shaders..." << std::endl;
    
    // Clear existing shaders
    effectShaders_.clear();
    commonShaderSource_.clear();
    
    // Reload all shaders
    if (!loadEffectShaders()) {
        std::cerr << "Failed to reload post-processing shaders" << std::endl;
        return;
    }
    
    std::cout << "Post-processing shaders reloaded successfully" << std::endl;
}
