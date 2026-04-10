#include "modular_layer.h"

#include <array>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "shader_loader.h"

namespace {

constexpr int kKaleidoscopeModeIndex = 30;

// Global flag to force shader source reload
bool g_forceShaderReload = false;

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

const std::array<const char*, 36> kProceduralShaderFiles = {
    "procedural_header.glsl",
    "procedural_helpers.glsl",
    "procedural_shadertoy_bridge.glsl",
    "procedural_pack1.glsl",
    "procedural_pack2.glsl",
    "procedural_pack3.glsl",
    "procedural_pack4.glsl",
    "procedural_pack5.glsl",
    "procedural_pack6.glsl",
    "procedural_pack7.glsl",
    "procedural_pack8.glsl",
    "procedural_pack9.glsl",
    "procedural_pack10.glsl",
    "procedural_pack11.glsl",
    "procedural_pack13.glsl",
    "procedural_pack14.glsl",
    "procedural_pack15.glsl",
    "procedural_pack16.glsl",
    "procedural_anaglyph.glsl",
    "procedural_message.glsl",
    "procedural_pouet.glsl",
    "procedural_cylinder_repeat.glsl",
    "procedural_head.glsl",
    "procedural_power_particle.glsl",
    "procedural_flopine.glsl",
    "procedural_eiyeron.glsl",
    "procedural_pack22.glsl",
    "procedural_pack23.glsl",
    "procedural_pack24.glsl",
    "procedural_pack25.glsl",
    "procedural_pack26.glsl",
    "procedural_pack27.glsl",
    "procedural_pack28.glsl",
    "procedural_pack29.glsl",
    "procedural_pack30.glsl",
    "procedural_main.glsl"
};

// Helper to initialize effect registry from shader files
void InitializeEffectRegistry() {
    // Always reload - no static cache
    std::vector<std::string> files;
    for (const auto& f : kProceduralShaderFiles) {
        files.push_back(f);
    }
    GetEffectRegistry().scanShaderFiles(kShaderSearchRoots, files);
}

const std::array<const char*, 25> kProceduralPackFiles = {
    "procedural_pack1.glsl",
    "procedural_pack2.glsl",
    "procedural_pack3.glsl",
    "procedural_pack4.glsl",
    "procedural_pack5.glsl",
    "procedural_pack6.glsl",
    "procedural_pack7.glsl",
    "procedural_pack8.glsl",
    "procedural_pack9.glsl",
    "procedural_pack10.glsl",
    "procedural_pack11.glsl",
    "procedural_pack13.glsl",
    "procedural_pack14.glsl",
    "procedural_pack15.glsl",
    "procedural_pack16.glsl",
    "procedural_pack22.glsl",
    "procedural_pack23.glsl",
    "procedural_pack24.glsl",
    "procedural_pack25.glsl",
    "procedural_pack26.glsl",
    "procedural_pack27.glsl",
    "procedural_pack28.glsl",
    "procedural_pack29.glsl",
    "procedural_pack30.glsl"
};

const char* kProceduralDebugMain = R"(
// --- Debug main stub ---
void main() {
    FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
)";

std::string TrimmedLower(std::string value) {
    const auto first = value.find_first_not_of(" \t\n\r");
    const auto last = value.find_last_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return {};
    }
    value = value.substr(first, last - first + 1U);
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

bool ResolveDebugPack(const std::string& request, std::string& outFile) {
    if (request.empty()) {
        return false;
    }

    const std::string key = TrimmedLower(request);
    if (key.empty()) {
        return false;
    }

    auto digitsToIndex = [](const std::string& digits) -> int {
        if (digits.empty()) {
            return -1;
        }
        for (char ch : digits) {
            if (!std::isdigit(static_cast<unsigned char>(ch))) {
                return -1;
            }
        }
        int value = std::stoi(digits);
        return (value >= 1 && value <= static_cast<int>(kProceduralPackFiles.size())) ? value - 1 : -1;
    };

    int packIndex = -1;

    // Accept plain numbers (e.g. "8")
    packIndex = digitsToIndex(key);

    // Accept forms like "pack8" or "pack8.glsl"
    if (packIndex == -1 && key.rfind("pack", 0) == 0) {
        std::string suffix = key.substr(4);
        if (suffix.size() > 5 && suffix.substr(suffix.size() - 5) == ".glsl") {
            suffix.erase(suffix.size() - 5);
        }
        packIndex = digitsToIndex(suffix);
    }

    // Accept forms like "procedural_pack8" or with extension
    if (packIndex == -1 && key.rfind("procedural_pack", 0) == 0) {
        std::string suffix = key.substr(15);
        if (suffix.size() > 5 && suffix.substr(suffix.size() - 5) == ".glsl") {
            suffix.erase(suffix.size() - 5);
        }
        packIndex = digitsToIndex(suffix);
    }

    if (packIndex >= 0) {
        outFile = kProceduralPackFiles[static_cast<size_t>(packIndex)];
        return true;
    }

    // Accept full filename match
    for (const char* packFile : kProceduralPackFiles) {
        if (key == TrimmedLower(packFile)) {
            outFile = packFile;
            return true;
        }
    }

    return false;
}

const std::string& GetProceduralFragmentShaderSource() {
    static std::string source;
    static std::string lastDebugKey;

    const char* debugEnv = std::getenv("PROCEDURAL_DEBUG_PACK");
    const std::string debugKey = debugEnv ? debugEnv : std::string{};

    if (source.empty() || debugKey != lastDebugKey || g_forceShaderReload) {
        lastDebugKey = debugKey;
        source.clear();
        g_forceShaderReload = false;  // Reset the flag after reload
        std::cout << "[SHADER RELOAD] Reloading procedural shader source..." << std::endl;

        if (!debugKey.empty()) {
            std::string packFile;
            if (ResolveDebugPack(debugKey, packFile)) {
                std::vector<std::string> debugFiles = {
                    "procedural_header.glsl",
                    "procedural_helpers.glsl",
                    "procedural_shadertoy_bridge.glsl",
                    packFile
                };

                std::string error;
                if (LoadShaderSources(kShaderSearchRoots, debugFiles, source, &error)) {
                    source += kProceduralDebugMain;
                    std::cout << "ModularLayer: PROCEDURAL_DEBUG_PACK active (" << packFile
                              << ")" << std::endl;
                } else {
                    std::cerr << "ModularLayer: failed to load debug pack '" << debugKey
                              << "': " << error << std::endl;
                    source.clear();
                }
            } else {
                std::cerr << "ModularLayer: unknown PROCEDURAL_DEBUG_PACK value '" << debugKey
                          << "'" << std::endl;
            }
        }

        if (source.empty()) {
            std::string error;
            if (!LoadShaderSources(kShaderSearchRoots, kProceduralShaderFiles, source, &error)) {
                std::cerr << "ModularLayer: failed to load procedural shader sources: " << error << std::endl;
            }
        }
    }

    return source;
}

// Force reload shader source from disk
void ForceReloadShaderSource() {
    g_forceShaderReload = true;
    std::cout << "Shader source reload requested, will reload from disk on next access" << std::endl;
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

    // Initialize effect registry (scans shaders for @EFFECT metadata)
    InitializeEffectRegistry();

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
    // Stop hot-reload thread first
    enableHotReload(false);
    
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
    std::cout << "[SHADER DEBUG] Compiling procedural shader with " << fragmentSource.size() << " bytes" << std::endl;
    if (!proceduralShader_->loadFromSource(kQuadVertexShader, fragmentSource)) {
        std::cerr << "ModularLayer: failed to compile procedural shader" << std::endl;
        proceduralShader_.reset();
        return false;
    }
    std::cout << "[SHADER DEBUG] Procedural shader compiled successfully" << std::endl;
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

    std::string fragmentSource = GetProceduralFragmentShaderSource();
    if (fragmentSource.empty()) {
        std::cerr << "ModularLayer: failed to locate procedural shader sources" << std::endl;
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

void ModularLayer::render(const LayerContext& context, bool clearFramebuffer) {
    if (!initialized_ || (!enabled_ && !debugPreview_)) {
        return;
    }

    // DEBUG: Log mode being rendered (only every 60 frames to avoid spam)
    static int frameCount = 0;
    static int lastLoggedMode = -1;
    frameCount++;
    if (frameCount % 60 == 0 || mode_ != lastLoggedMode) {
        const char* modeName = "Unknown";
        switch(mode_) {
            case 0: modeName = "None"; break;
            case 45: modeName = "Flopine"; break;
            case 46: modeName = "Eiyeron Deform"; break;
            default: modeName = "Other"; break;
        }
        std::cout << "[LAYER] Mode=" << mode_ << " (" << modeName << ")" << std::endl;
        lastLoggedMode = mode_;
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

    if (clearFramebuffer) {
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

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
        static int lastModeLogged = -1;
        if (mode_ != lastModeLogged) {
            std::cout << "[SHADER DEBUG] Setting uMode=" << mode_ << std::endl;
            lastModeLogged = mode_;
        }
        activeShader->setUniform1i("uMode", mode_);
        // Camera uniforms
        activeShader->setUniform1f("uCameraZoom", cameraZoom_);
        activeShader->setUniform1f("uCameraOffsetX", cameraOffsetX_);
        activeShader->setUniform1f("uCameraOffsetY", cameraOffsetY_);
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

// Hot-reload implementation
void ModularLayer::enableHotReload(bool enabled) {
    if (hotReloadEnabled_ == enabled) return;
    
    hotReloadEnabled_ = enabled;
    
    if (enabled) {
        // Initialize last modify time
        lastShaderModifyTime_ = std::filesystem::file_time_type::min();
        
        // Start file watcher thread
        shouldWatchFiles_ = true;
        fileWatcherThread_ = std::thread(&ModularLayer::watchShaderFiles, this);
        
        std::cout << "Hot-reload enabled for shaders" << std::endl;
    } else {
        // Stop file watcher thread
        shouldWatchFiles_ = false;
        if (fileWatcherThread_.joinable()) {
            fileWatcherThread_.join();
        }
        
        std::cout << "Hot-reload disabled for shaders" << std::endl;
    }
}

void ModularLayer::watchShaderFiles() {
    while (shouldWatchFiles_) {
        std::this_thread::sleep_for(WATCH_INTERVAL);
        
        if (!hotReloadEnabled_ || !initialized_) continue;
        
        if (shouldReloadShaders()) {
            reloadShaders();
        }
    }
}

bool ModularLayer::shouldReloadShaders() {
    try {
        std::filesystem::file_time_type latestTime = std::filesystem::file_time_type::min();
        
        // Check all shader files
        for (const auto& shaderFile : kProceduralShaderFiles) {
            std::string fullPath = "shaders/" + std::string(shaderFile);
            
            if (std::filesystem::exists(fullPath)) {
                auto currentTime = std::filesystem::last_write_time(fullPath);
                if (currentTime > latestTime) {
                    latestTime = currentTime;
                }
            }
        }
        
        // Also check the build directory
        for (const auto& shaderFile : kProceduralShaderFiles) {
            std::string fullPath = "build/shaders/" + std::string(shaderFile);
            
            if (std::filesystem::exists(fullPath)) {
                auto currentTime = std::filesystem::last_write_time(fullPath);
                if (currentTime > latestTime) {
                    latestTime = currentTime;
                }
            }
        }
        
        if (latestTime > lastShaderModifyTime_) {
            lastShaderModifyTime_ = latestTime;
            return true;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error watching shaders: " << e.what() << std::endl;
    }
    
    return false;
}

void ModularLayer::reloadShaders() {
    std::cout << "Reloading shaders..." << std::endl;
    
    try {
        // Copy shaders from source to build directory first
        namespace fs = std::filesystem;
        fs::path sourceDir = "../shaders";
        fs::path buildDir = "./shaders";
        
        if (fs::exists(sourceDir)) {
            std::cout << "Copying updated shaders from source..." << std::endl;
            if (fs::exists(buildDir)) {
                fs::remove_all(buildDir);
            }
            fs::copy(sourceDir, buildDir, fs::copy_options::recursive);
            std::cout << "Shaders copied successfully!" << std::endl;
        }
        
        // Force source reload from disk
        ForceReloadShaderSource();
        
        // Force shader reload by resetting the shader pointer
        proceduralShader_.reset();
        kaleidoscopeShader_.reset();
        
        // Recreate shader
        if (!ensureShader()) {
            std::cerr << "Failed to reload shaders" << std::endl;
            return;
        }
        
        std::cout << "Shaders reloaded successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error reloading shaders: " << e.what() << std::endl;
    }
}
