#include "post_processor.h"

#include <array>
#include <iostream>

namespace {

const char* kPostVertexShader = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;

out vec2 vUV;

void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char* kPostFragmentShader = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uScene;
uniform int uMode;
uniform float uStrength;
uniform float uTime;

vec3 toGrayscale(vec3 color) {
    float luminance = dot(color, vec3(0.299, 0.587, 0.114));
    return vec3(luminance);
}

vec3 applyFilmic(vec3 x) {
    const vec3 A = vec3(0.15, 0.15, 0.15);
    const vec3 B = vec3(0.50, 0.50, 0.50);
    const vec3 C = vec3(0.10, 0.10, 0.10);
    const vec3 D = vec3(0.20, 0.20, 0.20);
    const vec3 E = vec3(0.02, 0.02, 0.02);
    const vec3 F = vec3(0.30, 0.30, 0.30);
    return ((x * (A * x + B)) / (x * (C * x + D) + E)) - F;
}

vec3 chromaticAberration(vec2 uv, float strength) {
    float offset = strength * 0.003;
    vec3 col;
    col.r = texture(uScene, uv + vec2(offset, 0.0)).r;
    col.g = texture(uScene, uv).g;
    col.b = texture(uScene, uv - vec2(offset, 0.0)).b;
    return col;
}

float vignette(vec2 uv, float intensity) {
    vec2 centered = uv - 0.5;
    float dist = dot(centered, centered);
    return smoothstep(0.75, intensity, dist);
}

float filmGrain(vec2 uv, float time, float intensity) {
    float noise = fract(sin(dot(uv * 5.0 + time, vec2(12.9898, 78.233))) * 43758.5453);
    return mix(0.5, noise, intensity);
}

void main() {
    vec4 sceneColor = texture(uScene, vUV);
    float strength = clamp(uStrength, 0.0, 1.0);
    vec3 result = sceneColor.rgb;

    if (uMode == 1) {
        vec3 gray = toGrayscale(sceneColor.rgb);
        result = mix(sceneColor.rgb, gray, strength);
    } else if (uMode == 2) {
        vec3 filmic = applyFilmic(sceneColor.rgb * (1.5 + strength));
        filmic = clamp(filmic, 0.0, 1.0);
        float vig = vignette(vUV, 0.35 + strength * 0.3);
        float grain = filmGrain(vUV, uTime * 24.0, strength * 0.6);
        result = mix(sceneColor.rgb, filmic, strength * 0.7);
        result *= mix(1.0, 1.0 - vig, strength * 0.5);
        result = mix(result, result * grain, strength * 0.4);
    } else if (uMode == 3) {
        vec2 waveUV = vUV + vec2(sin(vUV.y * 40.0 + uTime * 3.0), cos(vUV.x * 30.0 + uTime * 2.0)) * 0.0025 * strength;
        vec3 distorted = texture(uScene, waveUV).rgb;
        vec3 aberration = chromaticAberration(vUV, strength);
        float scanline = 0.85 + 0.15 * sin(vUV.y * 900.0);
        result = mix(distorted, aberration, strength * 0.6);
        result *= mix(1.0, scanline, strength * 0.5);
    } else if (uMode == 4) {
        vec3 aberration = chromaticAberration(vUV, strength * 1.5);
        float wave = sin((vUV.x + vUV.y) * 25.0 + uTime * 4.0) * 0.5 + 0.5;
        vec3 pulseColor = mix(vec3(0.2, 0.4, 0.9), vec3(0.9, 0.4, 0.2), wave);
        result = mix(sceneColor.rgb, aberration, strength * 0.5);
        result += pulseColor * strength * 0.25;
    }

    FragColor = vec4(result, sceneColor.a);
}
)";

} // namespace

PostProcessor::PostProcessor() = default;

PostProcessor::~PostProcessor() {
    shutdown();
}

bool PostProcessor::initialize(int width, int height) {
    if (initialized_) {
        resize(width, height);
        return true;
    }

    if (!createResources(width, height)) {
        return false;
    }

    if (!ensureShader()) {
        destroyResources();
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
    glViewport(previousViewport_[0], previousViewport_[1], previousViewport_[2], previousViewport_[3]);
}

void PostProcessor::apply(int mode, float strength, float time) {
    if (!initialized_ || mode == 0 || strength <= 0.0f) {
        return;
    }

    if (!ensureShader()) {
        return;
    }

    glDisable(GL_DEPTH_TEST);

    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    if (!blendWasEnabled) {
        glEnable(GL_BLEND);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader_->use();
    shader_->setUniform1i("uMode", mode);
    shader_->setUniform1f("uStrength", strength);
    shader_->setUniform1f("uTime", time);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorTexture_);
    shader_->setUniform1i("uScene", 0);

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

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
        std::cerr << "PostProcessor: framebuffer incomplete (status=" << std::hex << status << std::dec << ")" << std::endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        destroyResources();
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

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

void PostProcessor::destroyResources() {
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
}

bool PostProcessor::ensureShader() {
    if (shader_) {
        return true;
    }

    shader_ = std::make_unique<Shader>();
    if (!shader_->loadFromSource(kPostVertexShader, kPostFragmentShader)) {
        std::cerr << "PostProcessor: failed to compile post-processing shader" << std::endl;
        shader_.reset();
        return false;
    }
    return true;
}
