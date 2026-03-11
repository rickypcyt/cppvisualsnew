#include "post_processor.h"

#include <array>
#include <iostream>

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

const char *kPostFragmentShader = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uScene;
uniform int uMode;
uniform float uStrength;
uniform float uTime;
uniform vec3 uRgbAdjust;
uniform vec2 uResolution;
uniform float uBassLevel;

const float THRESH = 0.10;
const float PI = 3.14159265359;

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

vec3 radialBlur(vec2 uv, float amount) {
    vec2 center = vec2(0.5);
    vec2 dir = uv - center;
    float radius = mix(0.25, 1.2, amount);
    float samples = 10.0;
    vec3 acc = vec3(0.0);
    for (int i = 0; i < 10; ++i) {
        float t = float(i) / (samples - 1.0);
        vec2 sampleUV = center + dir * t * radius;
        acc += texture(uScene, clamp(sampleUV, 0.0, 1.0)).rgb;
    }
    return acc / samples;
}

vec2 kaleido(vec2 uv, float segments, vec2 resolution) {
    vec2 centered = uv - 0.5;
    float aspect = resolution.x / max(resolution.y, 1.0);
    centered.x *= aspect;

    float r = length(centered);
    float a = atan(centered.y, centered.x);
    float sector = (2.0 * PI) / max(segments, 1.0);
    a = mod(a, sector);
    a = abs(a - sector * 0.5);

    vec2 result = vec2(cos(a), sin(a)) * r;
    result.x /= aspect;
    return result + 0.5;
}

float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 digitalGlitch(vec2 uv, float strength, float time) {
    float lines = 160.0;
    float line = floor(uv.y * lines);
    float r = rand(vec2(line, floor(time * 12.0)));
    vec2 displaced = uv;
    if (r < 0.12 * strength) {
        displaced.x += (r - 0.06) * 0.35 * strength;
    }
    displaced.x = clamp(displaced.x, 0.0, 1.0);
    return texture(uScene, displaced).rgb;
}

vec3 pixelate(vec2 uv, float strength) {
    float size = mix(800.0, 40.0, strength);
    vec2 grid = floor(uv * size) / size;
    return texture(uScene, grid).rgb;
}

vec3 pixelateRetro(vec2 uv, float cells, float levels) {
    vec2 grid = floor(uv * cells) / cells;
    vec3 sampled = texture(uScene, grid).rgb;
    float steps = max(levels - 1.0, 1.0);
    sampled = floor(sampled * steps + 0.5) / steps;
    return sampled;
}

vec2 lensDistort(vec2 uv, float power) {
    vec2 c = uv - 0.5;
    float r2 = dot(c, c);
    c *= 1.0 + r2 * power;
    return c + 0.5;
}

vec3 plasmaOverlay(vec2 uv, float time) {
    float plasma = sin(uv.x * 12.0 + time)
                 + sin(uv.y * 10.0 + time * 1.3)
                 + sin((uv.x + uv.y) * 8.0 + time * 0.7);
    plasma = plasma * 0.5 + 0.5;
    vec3 color = vec3(
        sin(plasma * PI),
        sin(plasma * PI + 2.0),
        sin(plasma * PI + 4.0)
    );
    return color * 0.5 + 0.5;
}

vec3 recursiveEnergy(vec2 uv, float strength) {
    vec2 offset = uv - 0.5;
    vec2 pos = offset;
    vec3 base = texture(uScene, clamp(vec2(0.5) + pos, 0.0, 1.0)).rbb;
    float depth = 0.0;
    float intensity = mix(0.3, 1.4, strength);

    for (int i = 0; i < 50; ++i) {
        pos *= 0.98;
        vec2 sampleUV = clamp(vec2(0.5) + pos, 0.0, 1.0);
        vec4 sampleTex = texture(uScene, sampleUV);
        float influence = pow(max(0.0, 0.5 - length(sampleTex.rg)), 2.0) * exp(-float(i) * 0.1);
        depth += influence * intensity;
    }

    vec3 energy = base * base + depth;
    return clamp(energy, 0.0, 1.0);
}

void main() {
    vec4 sceneColor = texture(uScene, vUV);
    float strength = clamp(uStrength, 0.0, 1.0);
    float bass = clamp(uBassLevel, 0.0, 1.0);
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
    } else if (uMode == 5) {
        float brightness = dot(sceneColor.rgb, vec3(0.2126, 0.7152, 0.0722));
        float bandCenter = (sin(uTime * (0.6 + bass * 1.2)) + 1.5) * 0.3;
        bandCenter = mix(bandCenter, clamp(bass * 0.8, 0.05, 0.95), 0.6);
        float window = mix(THRESH * 0.35, THRESH + 0.4, clamp(bass * 1.3, 0.0, 1.0));
        window += strength * 0.25;
        float lower = clamp(bandCenter - window, 0.0, 1.0);
        float upper = clamp(bandCenter + window, 0.0, 1.0);
        float mask = step(lower, brightness) * step(brightness, upper);
        float ridge = smoothstep(0.0, 1.0, abs(brightness - bandCenter) / max(window, 1e-4));
        float gain = mix(0.6, 1.6, clamp(bass * 1.1, 0.0, 1.0));
        result = sceneColor.rgb * mask * gain;
        result += sceneColor.rgb * (1.0 - mask) * clamp(0.15 - bass * 0.1, 0.0, 0.15);
    } else if (uMode == 6) {
        vec3 blurred = radialBlur(vUV, strength);
        result = mix(sceneColor.rgb, blurred, strength);
    } else if (uMode == 7) {
        float segments = 6.0 + strength * 10.0;
        vec2 uv = kaleido(vUV, segments, uResolution);
        result = texture(uScene, uv).rgb;
    } else if (uMode == 8) {
        result = mix(sceneColor.rgb, digitalGlitch(vUV, strength, uTime), strength * 0.9);
    } else if (uMode == 9) {
        vec3 pix = pixelateRetro(vUV, 64.0, 8.0);
        result = mix(sceneColor.rgb, pix, strength);
    } else if (uMode == 10) {
        vec3 pix = pixelateRetro(vUV, 128.0, 16.0);
        result = mix(sceneColor.rgb, pix, strength);
    } else if (uMode == 11) {
        vec3 pix = pixelateRetro(vUV, 192.0, 32.0);
        result = mix(sceneColor.rgb, pix, strength);
    } else if (uMode == 12) {
        vec3 pix = pixelateRetro(vUV, 256.0, 64.0);
        result = mix(sceneColor.rgb, pix, strength);
    } else if (uMode == 13) {
        vec2 uv = lensDistort(vUV, strength * 1.5);
        result = texture(uScene, clamp(uv, 0.0, 1.0)).rgb;
    } else if (uMode == 14) {
        vec3 plasma = plasmaOverlay(vUV, uTime * 1.2);
        result = mix(sceneColor.rgb, plasma, strength * 0.35);
    } else if (uMode == 15) {
        vec3 adjust = max(uRgbAdjust, vec3(0.0));
        float radius = strength * 0.01;
        float phase = uTime * 0.8;
        vec2 rOffset = vec2(cos(phase), sin(phase)) * radius * adjust.r;
        vec2 gOffset = vec2(cos(phase + 2.0943951), sin(phase + 2.0943951)) * radius * adjust.g;
        vec2 bOffset = vec2(cos(phase + 4.1887902), sin(phase + 4.1887902)) * radius * adjust.b;
        vec2 uvR = clamp(vUV + rOffset, 0.0, 1.0);
        vec2 uvG = clamp(vUV + gOffset, 0.0, 1.0);
        vec2 uvB = clamp(vUV + bOffset, 0.0, 1.0);
        vec3 shifted;
        shifted.r = texture(uScene, uvR).r;
        shifted.g = texture(uScene, uvG).g;
        shifted.b = texture(uScene, uvB).b;
        vec3 channelStrength = clamp(adjust, 0.0, 1.0) * strength;
        result.r = mix(sceneColor.r, shifted.r, channelStrength.r);
        result.g = mix(sceneColor.g, shifted.g, channelStrength.g);
        result.b = mix(sceneColor.b, shifted.b, channelStrength.b);
    } else if (uMode == 16) {
        vec3 energy = recursiveEnergy(vUV, strength);
        result = mix(sceneColor.rgb, energy, strength);
    }

    FragColor = vec4(result, sceneColor.a);
}
)";

} // namespace

PostProcessor::PostProcessor() = default;

PostProcessor::~PostProcessor() { shutdown(); }

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
    shader_->setUniform1f("uTime", time);
    shader_->setUniform2f("uResolution", static_cast<float>(width_), static_cast<float>(height_));
    shader_->setUniform1f("uBassLevel", bassLevel);

    GLuint currentTexture = colorTexture_;
    int pingIndex = 0;
    bool drewPass = false;

    glActiveTexture(GL_TEXTURE0);
    shader_->setUniform1i("uScene", 0);

    for (const auto &pass : passes) {
        if (pass.mode == 0 || pass.strength <= 0.0f) {
            continue;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, pingFbos_[pingIndex]);
        glViewport(0, 0, width_, height_);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        shader_->setUniform1i("uMode", pass.mode);
        shader_->setUniform1f("uStrength", pass.strength);
        shader_->setUniform3f("uRgbAdjust", pass.rgbAdjust[0], pass.rgbAdjust[1],
                              pass.rgbAdjust[2]);

        glBindTexture(GL_TEXTURE_2D, currentTexture);

        glBindVertexArray(quadVAO_);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        currentTexture = pingTextures_[pingIndex];
        pingIndex = 1 - pingIndex;
        drewPass = true;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width_, height_);

    shader_->setUniform1i("uMode", 0);
    shader_->setUniform1f("uStrength", 0.0f);
    shader_->setUniform3f("uRgbAdjust", 1.0f, 1.0f, 1.0f);

    glBindTexture(GL_TEXTURE_2D, currentTexture);

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
