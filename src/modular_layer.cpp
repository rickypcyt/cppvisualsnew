#include "modular_layer.h"

#include <array>
#include <iomanip>
#include <iostream>

namespace {

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

const char* kProceduralFragmentShader = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform vec2 uResolution;
uniform float uTime;
uniform float uTempo;
uniform float uEnergy;
uniform float uBass;
uniform float uMid;
uniform float uHigh;
uniform int uMode;

const float PI = 3.14159265359;
const float TAU = 6.28318530718;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

vec3 palette(float t) {
    vec3 a = vec3(0.5, 0.3, 0.6);
    vec3 b = vec3(0.5, 0.4, 0.4);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.0, 0.33, 0.67);
    return a + b * cos(TAU * (c * t + d));
}

vec4 renderNebula(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    float baseFreq = mix(1.5, 4.5, clamp(energy, 0.0, 1.0));
    float timeWarp = time * (0.2 + tempo * 0.1 + high * 0.2);

    float n = noise(st * baseFreq + timeWarp);
    float n2 = noise(st * (baseFreq * 1.8) - timeWarp * 0.6);
    float combined = mix(n, n2, 0.5 + 0.5 * sin(time * 0.8 + bass * 3.0));

    float poster = floor(combined * 8.0) / 8.0;
    float dither = fract(sin(dot(st + time, vec2(12.9898, 78.233))) * 43758.5453);
    float intensity = clamp(poster + dither * 0.02, 0.0, 1.0);

    vec3 color = palette(intensity + mid * 0.2);
    color *= vec3(0.6 + bass * 0.8, 0.6 + mid * 0.7, 0.7 + high * 0.9);

    float alpha = clamp(0.35 + intensity * 0.55 + energy * 0.25, 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderASCIIOcean(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 waves = st;
    waves.x += sin(st.y * 2.0 + time * 0.4) * 0.1;
    float surface = sin(waves.x * 3.2 + time * (0.9 + tempo * 0.2));
    surface += sin((waves.x + waves.y) * 5.5 + time * 1.6) * 0.6;
    surface += sin(waves.y * 7.5 - time * 1.1) * 0.4;
    surface *= 0.35;
    surface += 0.5 + energy * 0.25 + bass * 0.15;
    surface = clamp(surface, 0.0, 1.0);

    float levels = 12.0;
    float idx = floor(surface * levels);
    float asciiIntensity = idx / max(levels - 1.0, 1.0);
    float edge = smoothstep(0.1, 0.9, fract(surface * levels));

    vec3 deep = vec3(0.03, 0.08, 0.18);
    vec3 crest = vec3(0.22 + high * 0.35, 0.55 + mid * 0.3, 0.85 + bass * 0.2);
    vec3 foam = vec3(0.8 + high * 0.2, 0.9, 0.95);

    vec3 color = mix(deep, crest, asciiIntensity);
    color = mix(color, foam, edge * (0.4 + energy * 0.2));
    color += vec3(0.05, 0.07, 0.1) * sin((st.y + time * 0.5) * 40.0) * 0.2;

    float alpha = clamp(0.35 + asciiIntensity * 0.4 + energy * 0.25, 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderSacredGeometry(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 q = st * 1.2;
    float r = length(q);
    float angle = atan(q.y, q.x);

    float petals = 6.0 + floor(mid * 6.0);
    float radial = pow(abs(cos(petals * angle)), 2.5);
    float lattice = abs(cos((petals * 0.5 + 2.0) * angle + time * 0.6));
    float concentric = sin(r * (28.0 + high * 12.0) + time * (1.4 + tempo * 0.3)) * 0.5 + 0.5;
    float spiral = sin(angle * 4.0 + time * 1.6 + r * 12.0);

    float sacred = mix(radial, concentric, 0.6) + lattice * 0.2 + spiral * 0.2;
    sacred *= exp(-r * (1.6 - energy * 0.4));
    sacred = clamp(sacred, 0.0, 1.2);

    vec3 inner = vec3(0.95 + high * 0.3, 0.75 + mid * 0.2, 0.55 + bass * 0.2);
    vec3 outer = vec3(0.1 + bass * 0.25, 0.05 + mid * 0.2, 0.12 + high * 0.2);
    vec3 aura = vec3(0.6 + high * 0.3, 0.25 + mid * 0.2, 0.7 + bass * 0.25);

    float auraMask = smoothstep(0.25, 0.75, sacred) * (0.6 + energy * 0.3);
    vec3 color = mix(outer, inner, sacred);
    color += aura * auraMask;

    float alpha = clamp(0.4 + sacred * 0.5 + energy * 0.25, 0.0, 1.0);
    return vec4(color, alpha);
}

vec3 glitchPaletteColor(float seed, float energy, float bass, float high) {
    vec3 c0 = vec3(0.95, 0.25, 0.32);
    vec3 c1 = vec3(0.2, 0.85, 0.92);
    vec3 c2 = vec3(0.95, 0.8, 0.2);
    vec3 c3 = vec3(0.58, 0.28, 0.9);
    vec3 c4 = vec3(0.18, 0.95, 0.42);
    vec3 c5 = vec3(0.95, 0.48, 0.12);

    float band = floor(seed * 6.0);
    vec3 color = c5;
    if (band < 1.0) {
        color = c0;
    } else if (band < 2.0) {
        color = c1;
    } else if (band < 3.0) {
        color = c2;
    } else if (band < 4.0) {
        color = c3;
    } else if (band < 5.0) {
        color = c4;
    }

    vec3 gain = vec3(0.7 + energy * 0.4, 0.7 + bass * 0.35, 0.7 + high * 0.45);
    return color * gain;
}

vec4 renderGlitchGrid(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 jittered = st;
    jittered.x += (hash(vec2(floor(time * 7.0), floor(st.y * 32.0))) - 0.5) * 0.05 * (0.4 + high * 0.6);
    jittered.y += sin(time * 2.3 + st.x * 24.0) * 0.02 * (0.4 + mid * 0.6);

    vec3 accumColor = vec3(0.0);
    float accumWeight = 0.0;

    for (int i = 0; i < 3; ++i) {
        float fi = float(i);
        float scale = mix(6.0, 26.0, fract(sin(fi * 12.17 + energy * 5.73) * 43758.5453));
        scale += fi * 4.5;

        vec2 grid = jittered * (scale + tempo * 2.5 + fi * 3.0);
        vec2 cell = floor(grid);
        vec2 cellUV = fract(grid);

        float seed = hash(cell + fi * 19.31 + floor(time * (2.0 + tempo * 1.2)));
        float sizeX = mix(0.25, 0.95, seed);
        float sizeY = mix(0.25, 0.95, hash(cell.yx + fi * 7.91));
        float blockMask = step(cellUV.x, sizeX) * step(cellUV.y, sizeY);

        float flicker = step(0.35 + high * 0.45, hash(cell + vec2(fi * 11.3, floor(time * (8.0 + tempo * 3.0)))));
        float glitch = blockMask * flicker;

        if (glitch > 0.0) {
            vec3 color = glitchPaletteColor(seed, energy, bass, high);
            float pulse = 0.6 + 0.4 * sin(time * (6.0 + tempo * 1.5) + fi * 1.7 + cell.x * 0.8);
            color *= pulse;
            accumColor += color * glitch;
            accumWeight += glitch;
        }
    }

    if (accumWeight > 0.0) {
        accumColor /= accumWeight;
    }

    float scanline = sin((st.y + time * 1.6) * 140.0) * 0.05;
    accumColor += vec3(scanline * 0.35, scanline * 0.2, scanline * 0.4);

    float shimmer = hash(vec2(floor(st.y * 160.0), floor(time * 24.0))) * (0.08 + energy * 0.35);
    accumColor += vec3(shimmer * (0.6 + high * 0.4));

    accumColor = clamp(accumColor, 0.0, 1.0);
    accumColor = floor(accumColor * 6.0) / 6.0;

    float weightFactor = clamp(accumWeight * 0.35 + energy * 0.4 + bass * 0.25, 0.0, 1.0);
    float alpha = clamp(0.45 + weightFactor, 0.0, 1.0);
    return vec4(accumColor, alpha);
}

void main() {
    vec2 st = (vUV - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);

    vec4 color;
    if (uMode == 1) {
        color = renderASCIIOcean(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 2) {
        color = renderSacredGeometry(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 3) {
        color = renderGlitchGrid(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else {
        color = renderNebula(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    }

    FragColor = color;
}
)";

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
}

bool ModularLayer::ensureShader() {
    if (proceduralShader_) {
        return true;
    }

    proceduralShader_ = std::make_unique<Shader>();
    if (!proceduralShader_->loadFromSource(kQuadVertexShader, kProceduralFragmentShader)) {
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

void ModularLayer::render(const LayerContext& context) {
    if (!initialized_ || (!enabled_ && !debugPreview_)) {
        return;
    }

    if (!ensureShader()) {
        return;
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

    proceduralShader_->use();
    proceduralShader_->setUniform2f("uResolution", static_cast<float>(width_), static_cast<float>(height_));
    proceduralShader_->setUniform1f("uTime", context.time);
    proceduralShader_->setUniform1f("uTempo", context.tempo);

    const auto* audio = context.audio;
    float energy = audio ? audio->energy : 0.0f;
    float bass = audio ? audio->bassEnergy : 0.0f;
    float mid = audio ? audio->midEnergy : 0.0f;
    float high = audio ? audio->highEnergy : 0.0f;

    proceduralShader_->setUniform1f("uEnergy", energy);
    proceduralShader_->setUniform1f("uBass", bass);
    proceduralShader_->setUniform1f("uMid", mid);
    proceduralShader_->setUniform1f("uHigh", high);
    proceduralShader_->setUniform1i("uMode", mode_);

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
