#include "visualizer.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <random>
#include "audio_capture.h"
#include "imgui.h"

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main() {
    TexCoord = aTexCoord;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char* coreVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aDir;
layout (location = 1) in float aRadial;
layout (location = 2) in float aAngle;

out float vAngle;
out float vRadial;

uniform vec2 uResolution;
uniform float uBaseRadius;
uniform float uPulse;
uniform float uKick;
uniform float uHarmonic;
uniform float uEnergy;
uniform float uBass;
uniform float uMid;
uniform float uHigh;
uniform float uTime;
uniform float uTempo;

void main() {
    vAngle = aAngle;
    vRadial = aRadial;

    float pulseFactor = mix(0.35 + uPulse * 0.25, 1.15 + uKick * 0.4, aRadial);
    float harmonicWarp = sin(aAngle * (4.0 + uHarmonic * 1.6) + uTime * (1.8 + uTempo * 0.5)) * (0.12 + uHarmonic * 0.18);
    harmonicWarp += sin(aAngle * (9.0 + uHigh * 3.0) + uTime * (2.6 + uTempo * 0.35)) * (0.05 + uHigh * 0.12);

    float radius = uBaseRadius * pulseFactor * (1.0 + harmonicWarp);
    radius += aRadial * (uBass * 60.0 + uMid * 40.0 + uHigh * 30.0);

    vec2 pos = aDir * radius;
    vec2 scale = vec2(2.0 / uResolution.x, 2.0 / uResolution.y);
    gl_Position = vec4(pos * scale, 0.0, 1.0);
}
)";

const char* coreFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in float vAngle;
in float vRadial;

uniform float uBass;
uniform float uMid;
uniform float uHigh;
uniform float uEnergy;
uniform float uPulse;
uniform float uKick;
uniform float uHarmonic;
uniform float uTime;
uniform float uTempo;

void main() {
    float glow = smoothstep(0.05, 1.0, vRadial);
    float harmonicWaves = sin(vAngle * (6.0 + uHarmonic * 2.2) + uTime * (2.5 + uTempo * 0.6));
    float spark = sin(vAngle * (14.0 + uHigh * 5.0) + uTime * (4.0 + uTempo * 0.9));

    vec3 baseColor = vec3(0.32 + uBass * 0.8,
                          0.36 + uMid * 0.7,
                          0.48 + uHigh * 0.9);
    vec3 accentColor = vec3(0.65 + uHigh * 0.8,
                            0.45 + uMid * 0.6,
                            0.92 + uBass * 0.7);

    float accentMix = glow * (0.5 + uPulse * 0.3 + uKick * 0.35);
    vec3 color = mix(baseColor, accentColor, clamp(accentMix, 0.0, 1.0));
    color += vec3(0.2, 0.18, 0.28) * harmonicWaves * (0.25 + uHarmonic * 0.4);
    color += vec3(0.35, 0.28, 0.55) * spark * (0.12 + uHigh * 0.25);
    color = max(color, vec3(0.0));

    float alpha = 0.18 + glow * 0.55 + uEnergy * 0.18 + uKick * 0.25;
    alpha = clamp(alpha, 0.1, 0.85);

    FragColor = vec4(color, alpha);
}
)";

void Visualizer::setupCoreMesh() {
    if (coreVAO_) {
        glDeleteVertexArrays(1, &coreVAO_);
        coreVAO_ = 0;
    }
    if (coreVBO_) {
        glDeleteBuffers(1, &coreVBO_);
        coreVBO_ = 0;
    }

    const int segments = 180;
    std::vector<float> data;
    data.reserve((segments + 1) * 2 * 4);

    for (int i = 0; i <= segments; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(segments);
        float angle = t * 6.28318530718f;
        float c = std::cos(angle);
        float s = std::sin(angle);

        // inner vertex
        data.push_back(c);
        data.push_back(s);
        data.push_back(0.0f);
        data.push_back(angle);

        // outer vertex
        data.push_back(c);
        data.push_back(s);
        data.push_back(1.0f);
        data.push_back(angle);
    }

    coreVertexCount_ = static_cast<GLsizei>((segments + 1) * 2);

    glGenVertexArrays(1, &coreVAO_);
    glGenBuffers(1, &coreVBO_);

    glBindVertexArray(coreVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, coreVBO_);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);

    GLsizei stride = 4 * sizeof(float);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Visualizer::initializeDynamicSystems() {
    core_ = {};
    core_.baseRadius = std::max(16.0f, std::min(windowWidth_, windowHeight_) * 0.02f);
    core_.radius = core_.baseRadius;
    core_.energy = 0.0f;
    core_.kickEnvelope = 0.0f;

    gears_.clear();
    std::uniform_real_distribution<float> distPhase(0.0f, 6.28318f);
    std::uniform_real_distribution<float> distOffset(0.85f, 1.15f);
    std::uniform_int_distribution<int> distTeeth(9, 16);

    const float minDim = static_cast<float>(std::min(windowWidth_, windowHeight_));
    const int ringCount = 3;
    const float ringBase = minDim * 0.16f;
    const float ringSpacing = minDim * 0.11f;

    auto starAngles = [](int starPoints) {
        std::vector<float> result;
        float base = 6.28318f / std::max(1, starPoints);
        for (int i = 0; i < starPoints; ++i) {
            result.push_back(base * i);
        }
        return result;
    };

    auto anchorAngles = starAngles(5);

    for (int ring = 0; ring < ringCount; ++ring) {
        int gearsOnRing = 6 + ring * 4;
        float orbitRadius = ringBase + ringSpacing * ring;
        float anchorRadius = orbitRadius * (0.6f + 0.25f * ring);
        for (int i = 0; i < gearsOnRing; ++i) {
            float phase = static_cast<float>(i) / gearsOnRing * 6.28318f;
            GearNode gear{};
            gear.orbitAngle = phase;
            gear.orbitSpeed = 0.18f + 0.12f * ring;
            gear.ringRadius = orbitRadius * distOffset(rng_);
            gear.baseRadius = minDim * (0.018f + 0.014f * ring) * distOffset(rng_);
            gear.radius = gear.baseRadius;
            gear.angle = distPhase(rng_);
            gear.angularVelocity = 0.0f;
            gear.teeth = distTeeth(rng_);
            gear.jitterPhase = distPhase(rng_);
            gear.x = 0.0f;
            gear.y = 0.0f;
            gear.orbitDirection = (i % 2 == 0) ? 1.0f : -1.0f;
            gear.anchorAngle = phase;
            gear.anchorRadius = gear.ringRadius;
            gear.ringIndex = ring;
            gear.baseOffsetRadius = anchorRadius;
            gear.baseOffsetAngle = anchorAngles[(i + ring) % anchorAngles.size()] + distPhase(rng_) * 0.12f;
            gear.baseCenterX = windowWidth_ * 0.5f + std::cos(gear.baseOffsetAngle) * gear.baseOffsetRadius;
            gear.baseCenterY = windowHeight_ * 0.5f + std::sin(gear.baseOffsetAngle) * gear.baseOffsetRadius;
            gears_.push_back(gear);
        }
    }

    gearSpawnRadius_ = ringBase + ringSpacing * (ringCount - 1);
    lifeGrid_.cells.fill(0);
    lifeGrid_.next.fill(0);
    lifeTimeAccumulator_ = 0.0f;
    lastLifeSeedTime_ = 0.0f;
}

void Visualizer::updateCore(float dt, const AudioAnalyzer::AudioFeatures& features) {
    if (!std::isfinite(dt) || dt <= 0.0f) {
        dt = 1.0f / 60.0f;
    }

    float tempoDt = dt * tempoMultiplier_;

    float minDim = static_cast<float>(std::min(windowWidth_, windowHeight_));
    float bass = std::clamp(features.bassEnergy, 0.0f, 2.0f);
    float mid = std::clamp(features.midEnergy, 0.0f, 2.0f);
    float high = std::clamp(features.highEnergy, 0.0f, 2.0f);
    float energy = std::clamp(features.energy, 0.0f, 3.0f);

    core_.pulse = energy;
    core_.kickEnvelope = core_.kickEnvelope * std::pow(0.08f, tempoDt) + features.kick * 0.9f;
    core_.kickEnvelope = std::clamp(core_.kickEnvelope, 0.0f, 2.5f);

    float harmonicInput = mid * 0.6f + high * 0.85f
                          + features.clap * 0.7f
                          + features.hiHat * 0.75f
                          + features.onset * 0.4f;
    harmonicInput = std::clamp(harmonicInput, 0.0f, 2.5f);
    float riseAlpha = 1.0f - std::pow(0.04f, tempoDt);
    float decayFactor = std::pow(0.35f, tempoDt);
    if (harmonicInput > core_.harmonicEnvelope) {
        core_.harmonicEnvelope += (harmonicInput - core_.harmonicEnvelope) * riseAlpha;
    } else {
        core_.harmonicEnvelope = core_.harmonicEnvelope * decayFactor + harmonicInput * (1.0f - decayFactor);
    }
    core_.harmonicEnvelope = std::clamp(core_.harmonicEnvelope, 0.0f, 2.2f);

    float steadyEnergy = std::max({features.kick, features.clap, features.hiHat, features.beat, features.onset});
    steadyEnergy = std::clamp(steadyEnergy * 1.2f, 0.0f, 1.5f);
    idleState_ = idleState_ * std::pow(0.2f, tempoDt) + (1.0f - steadyEnergy) * tempoDt * 0.8f;
    idleState_ = std::clamp(idleState_, 0.0f, 1.0f);
    idlePhase_ += dt * tempoMultiplier_ * (0.3f + 0.4f * idleState_);

    float base = minDim * (0.015f + 0.018f * std::clamp(energy, 0.0f, 1.5f))
                 + minDim * 0.01f * std::clamp(mid, 0.0f, 1.0f)
                 + minDim * 0.0045f * std::clamp(core_.harmonicEnvelope, 0.0f, 1.5f);
    core_.baseRadius = std::max(12.0f, base);
    float harmonicScale = 0.22f * std::clamp(core_.harmonicEnvelope, 0.0f, 1.6f);
    core_.radius = core_.baseRadius * (1.0f + 0.38f * core_.kickEnvelope + 0.2f * features.beat + harmonicScale);

    float harmonicContribution = core_.harmonicEnvelope * 1.05f + harmonicInput * 0.4f;
    float injection = (energy * 0.55f + bass * 0.75f + mid * 0.45f + high * 0.4f)
                      + core_.kickEnvelope * 1.1f
                      + harmonicContribution;
    injection *= std::clamp(1.0f - idleState_ * 0.75f, 0.2f, 1.0f);
    core_.energy += injection * tempoDt;
    core_.energy *= std::pow(0.45f, tempoDt);
    core_.energy = std::clamp(core_.energy, 0.0f, 8.0f);
}

float Visualizer::sampleCoreEnergyField(float x, float y) const {
    float centerX = windowWidth_ * 0.5f;
    float centerY = windowHeight_ * 0.5f;
    float dx = x - centerX;
    float dy = y - centerY;
    float dist = std::sqrt(dx * dx + dy * dy);
    float radius = std::max(core_.radius, 1.0f);

    float falloff = std::exp(-dist / (radius * 1.6f));
    float oscillation = 0.7f + 0.3f * std::sin(dist / std::max(radius * 0.6f, 1.0f) + core_.pulse * 0.4f);
    float kickBoost = 1.0f + 0.8f * core_.kickEnvelope * std::exp(-dist / (radius * 0.8f));

    return core_.energy * falloff * oscillation * kickBoost;
}

void Visualizer::updateGears(float dt, const AudioAnalyzer::AudioFeatures& features) {
    if (gears_.empty()) {
        return;
    }

    if (!std::isfinite(dt) || dt <= 0.0f) {
        dt = 1.0f / 60.0f;
    }

    float tempoDt = dt * tempoMultiplier_;

    float bass = std::clamp(features.bassEnergy, 0.0f, 2.0f);
    float mid = std::clamp(features.midEnergy, 0.0f, 2.0f);
    float high = std::clamp(features.highEnergy, 0.0f, 2.0f);
    float minDimension = static_cast<float>(std::min(windowWidth_, windowHeight_));

    for (auto& gear : gears_) {
        float syncPhase = idlePhase_ * 0.6f + core_.pulse * 0.4f + gear.ringIndex * 0.25f;
        float orientationBase = gear.baseOffsetAngle
                                 + gear.orbitDirection * (0.18f * core_.pulse + 0.22f * high)
                                 + 0.12f * std::sin(syncPhase + gear.jitterPhase * 0.6f);
        float radialBase = gear.anchorRadius + core_.baseRadius * (0.65f + 0.18f * gear.ringIndex);
        float radialMod = minDimension * (0.012f + 0.025f * mid + 0.03f * core_.harmonicEnvelope + 0.02f * bass);
        float radial = radialBase + radialMod;

        gear.x = gear.baseCenterX + std::cos(orientationBase) * radial;
        gear.y = gear.baseCenterY + std::sin(orientationBase) * radial;
        gear.orbitAngle = orientationBase;

        float targetRadius = gear.baseRadius * (1.0f + 0.25f * mid + 0.32f * core_.harmonicEnvelope + 0.2f * core_.kickEnvelope);
        gear.radius = gear.radius + (targetRadius - gear.radius) * std::min(1.0f, tempoDt * 4.5f);

        gear.angularVelocity = gear.angularVelocity * std::pow(0.55f, tempoDt);
        gear.jitterPhase += (0.6f + high * 1.1f) * tempoDt;
        gear.angle = orientationBase;
    }
}

void Visualizer::renderGears(float animatedTime) const {
    float centerX = windowWidth_ * 0.5f;
    float centerY = windowHeight_ * 0.5f;
    float minDim = static_cast<float>(std::min(windowWidth_, windowHeight_));
    float safeMinDim = std::max(minDim, 1.0f);

    float bass = std::clamp(audioFeatures_.bassEnergy, 0.0f, 2.0f);
    float mid = std::clamp(audioFeatures_.midEnergy, 0.0f, 2.0f);
    float high = std::clamp(audioFeatures_.highEnergy, 0.0f, 2.0f);
    float energy = std::clamp(audioFeatures_.energy, 0.0f, 3.0f);
    float idleAttenuation = std::clamp(1.0f - idleState_ * 0.55f, 0.35f, 1.0f);
    bass *= idleAttenuation;
    mid *= idleAttenuation;
    high *= idleAttenuation;
    energy *= idleAttenuation;

    float harmonic = std::clamp(core_.harmonicEnvelope, 0.0f, 2.2f);
    float kick = std::clamp(core_.kickEnvelope, 0.0f, 2.5f);
    float pulse = std::clamp(core_.pulse, 0.0f, 3.0f);
    float glitchPhase = animatedTime * (2.6f + harmonic * 0.8f) + kick * 1.5f;
    float scanPhase = animatedTime * (3.2f + high * 0.5f) + energy * 0.8f;
    float runeRotation = animatedTime * (0.35f + pulse * 0.12f);
    float prismShift = 0.5f + 0.5f * std::sin(animatedTime * 1.8f + pulse * 0.6f);

    float radius = std::max(core_.radius, safeMinDim * 0.05f);
    struct FractalPoint {
        float x;
        float y;
        float angle;
    };

    std::vector<FractalPoint> contour;
    std::vector<FractalPoint> scratch;

    auto generateContour = [&](int detailLevel, float radiusScale, float warpStrength, std::vector<FractalPoint>& out) {
        int segments = std::max(96, detailLevel * 64);
        out.clear();
        out.reserve(segments + 1);

        float baseRadius = radius * radiusScale;
        float warpBase = warpStrength * (0.35f + 0.45f * harmonic);
        float timePhase = animatedTime * (0.8f + 0.45f * energy);

        for (int i = 0; i <= segments; ++i) {
            float t = static_cast<float>(i) / segments;
            float angle = t * 6.28318f;
            float offset = 0.0f;
            float amplitude = warpBase;
            float freq = 2.0f;

            for (int octave = 0; octave < detailLevel; ++octave) {
                float phase = timePhase * (1.0f + 0.23f * octave)
                              + pulse * 0.35f
                              + harmonic * (0.16f + 0.08f * octave);
                offset += std::sin(angle * freq + phase + octave * 1.3f) * amplitude;
                freq *= 2.2f;
                amplitude *= 0.55f;
            }

            float glitchStep = 0.5f + 0.5f * std::sin(angle * (6.0f + harmonic * 1.7f) + glitchPhase);
            offset += std::sin(angle * (13.0f + high * 2.5f) + glitchPhase * 1.3f) * warpStrength * 0.12f * glitchStep;
            offset += std::sin(angle * (3.0f + harmonic * 1.5f) + timePhase * 1.6f) * warpStrength * 0.25f * kick;
            float radial = baseRadius * (1.0f + offset);
            float x = centerX + std::cos(angle) * radial;
            float y = centerY + std::sin(angle) * radial;
            out.push_back({x, y, angle});
        }
    };

    auto drawLayer = [&](int detailLevel,
                         float radiusScale,
                         float warpStrength,
                         float innerScale,
                         float fillIntensity,
                         const ColorAdjust& fillAdjust,
                         const ColorAdjust& outlineAdjust) {
        generateContour(detailLevel, radiusScale, warpStrength, contour);

        glBegin(GL_TRIANGLE_STRIP);
        for (const auto& point : contour) {
            float dx = point.x - centerX;
            float dy = point.y - centerY;
            float innerX = centerX + dx * innerScale;
            float innerY = centerY + dy * innerScale;
            float baseOsc = std::sin(point.angle * (3.2f + harmonic) + animatedTime * 2.1f);
            float glitchOsc = std::sin(point.angle * 16.0f + glitchPhase);
            float sparkle = 0.55f + 0.45f * baseOsc * (0.6f + 0.4f * std::fabs(glitchOsc));
            float glitchPulse = 0.5f + 0.5f * glitchOsc;

            float innerAlpha = std::clamp(0.14f + fillIntensity * 0.22f, 0.12f, 0.58f);
            float outerAlpha = std::clamp(0.26f + fillIntensity * 0.55f + sparkle * 0.18f + glitchPulse * 0.22f, 0.2f, 0.95f);

            setColorWithAdjust(0.28f + bass * 0.2f + prismShift * 0.08f,
                               0.4f + mid * 0.24f + prismShift * 0.06f,
                               0.6f + high * 0.18f + prismShift * 0.12f,
                               innerAlpha,
                               fillAdjust);
            glVertex2f(innerX, innerY);

            setColorWithAdjust(0.48f + bass * 0.32f + sparkle * 0.18f + prismShift * 0.12f,
                               0.55f + mid * 0.38f + sparkle * 0.16f + prismShift * 0.10f,
                               0.88f + high * 0.48f + sparkle * 0.22f + prismShift * 0.18f,
                               outerAlpha,
                               fillAdjust);
            glVertex2f(point.x, point.y);
        }
        glEnd();

        glLineWidth(1.4f + fillIntensity * 2.0f + high * 0.6f);
        glBegin(GL_LINE_LOOP);
        for (const auto& point : contour) {
            float edgeGlow = 0.3f + 0.7f * std::sin(point.angle * 4.0f + animatedTime * 3.0f + pulse);
            setColorWithAdjust(0.75f + edgeGlow * 0.2f + bass * 0.1f,
                               0.82f + edgeGlow * 0.15f + mid * 0.1f,
                               1.2f + edgeGlow * 0.3f + high * 0.2f,
                               0.28f + fillIntensity * 0.4f,
                               outlineAdjust);
            glVertex2f(point.x, point.y);
        }
        glEnd();
    };

    float layerEnergy = std::clamp(energy * 0.4f + pulse * 0.35f, 0.0f, 2.0f);
    drawLayer(3,
              0.95f + layerEnergy * 0.18f,
              0.28f + harmonic * 0.12f,
              0.38f,
              0.65f + layerEnergy * 0.35f,
              legacyColorAdjust_.circleFill,
              legacyColorAdjust_.circleOutline);

    drawLayer(2,
              0.62f + bass * 0.22f,
              0.35f + harmonic * 0.14f,
              0.42f,
              0.45f + mid * 0.3f,
              legacyColorAdjust_.bloomInner,
              legacyColorAdjust_.circleOutline);

    drawLayer(1,
              0.38f + std::clamp(idleState_, 0.0f, 1.0f) * 0.25f,
              0.42f + high * 0.1f,
              0.52f,
              0.32f + bass * 0.25f,
              legacyColorAdjust_.bloomOuter,
              legacyColorAdjust_.circleOutline);

    generateContour(5,
                    1.12f + layerEnergy * 0.2f,
                    0.46f + harmonic * 0.18f,
                    scratch);
    size_t shardCount = scratch.size();
    if (shardCount >= 3) {
        glBegin(GL_TRIANGLES);
        for (size_t i = 0; i < shardCount; ++i) {
            size_t j = (i + 1) % shardCount;
            const auto& a = scratch[i];
            const auto& b = scratch[j];
            float midAngle = 0.5f * (a.angle + b.angle);
            float gate = 0.5f + 0.5f * std::sin(midAngle * (8.0f + harmonic * 1.5f) + glitchPhase * 1.6f);
            float burst = 0.5f + 0.5f * std::sin(midAngle * 14.0f + scanPhase);
            if (gate < 0.72f && j != 0) {
                continue;
            }

            float extrude = radius * (0.08f + 0.15f * gate + 0.12f * burst + 0.08f * high);
            float cx = centerX + std::cos(midAngle) * (radius * 0.42f + extrude);
            float cy = centerY + std::sin(midAngle) * (radius * 0.42f + extrude);
            float alpha = std::clamp(0.18f + gate * 0.35f + burst * 0.25f, 0.16f, 0.88f);

            setColorWithAdjust(0.65f + bass * 0.28f + prismShift * 0.2f,
                               0.82f + mid * 0.35f + prismShift * 0.18f,
                               1.25f + high * 0.5f + gate * 0.25f,
                               alpha,
                               legacyColorAdjust_.sparkles);
            glVertex2f(cx, cy);
            glVertex2f(a.x, a.y);
            glVertex2f(b.x, b.y);
        }
        glEnd();
    }

    int runeBranches = 6;
    glLineWidth(2.2f + prismShift * 1.4f + harmonic * 0.4f);
    glBegin(GL_LINES);
    for (int i = 0; i < runeBranches; ++i) {
        float baseAngle = runeRotation + (static_cast<float>(i) / runeBranches) * 6.28318f;
        float jitter = 0.12f * std::sin(glitchPhase + i * 1.37f + pulse * 0.5f);
        float inner = radius * (0.16f + 0.08f * bass + 0.04f * prismShift);
        float outer = radius * (0.62f + 0.28f * harmonic + 0.12f * energy);

        setColorWithAdjust(0.78f + prismShift * 0.22f + bass * 0.1f,
                           0.85f + prismShift * 0.18f + mid * 0.12f,
                           1.3f + prismShift * 0.26f + high * 0.22f,
                           0.32f + energy * 0.35f,
                           legacyColorAdjust_.circleOutline);
        glVertex2f(centerX + std::cos(baseAngle - jitter) * inner,
                   centerY + std::sin(baseAngle - jitter) * inner);
        glVertex2f(centerX + std::cos(baseAngle + jitter) * outer,
                   centerY + std::sin(baseAngle + jitter) * outer);
    }
    glEnd();

    glLineWidth(1.1f + high * 0.5f + pulse * 0.3f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 8; ++i) {
        float t = static_cast<float>(i) / 8.0f;
        float angle = runeRotation * 1.5f + t * 6.28318f + 0.12f * std::sin(glitchPhase + i * 0.9f);
        float radial = radius * (0.28f + 0.1f * ((i % 2 == 0) ? prismShift : harmonic * 0.35f));
        setColorWithAdjust(0.6f + bass * 0.25f + prismShift * 0.15f,
                           0.78f + mid * 0.3f + prismShift * 0.12f,
                           1.15f + high * 0.42f + prismShift * 0.2f,
                           0.26f + energy * 0.28f,
                           legacyColorAdjust_.circleOutline);
        glVertex2f(centerX + std::cos(angle) * radial,
                   centerY + std::sin(angle) * radial);
    }
    glEnd();

    glLineWidth(1.0f + high * 0.5f);
    glBegin(GL_LINES);
    for (int i = -2; i <= 2; ++i) {
        float stripeT = static_cast<float>(i) / 2.0f;
        float offset = std::sin(scanPhase + stripeT * 3.6f) * radius * 0.45f;
        float y = centerY + offset;
        float alpha = std::clamp(0.18f + energy * 0.2f + std::cos(scanPhase * 1.7f + i) * 0.12f, 0.08f, 0.45f);

        setColorWithAdjust(0.45f + bass * 0.2f + prismShift * 0.15f,
                           0.65f + mid * 0.3f + prismShift * 0.1f,
                           0.95f + high * 0.4f + prismShift * 0.2f,
                           alpha,
                           legacyColorAdjust_.bloomInner);
        glVertex2f(centerX - radius * (1.15f + stripeT * 0.2f), y);
        glVertex2f(centerX + radius * (1.15f + stripeT * 0.2f), y);
    }
    glEnd();

    int spokeCount = 10 + static_cast<int>(high * 7.0f) + static_cast<int>(pulse * 1.5f) + static_cast<int>(prismShift * 6.0f);
    glLineWidth(1.5f + harmonic * 0.5f + energy * 0.4f + prismShift * 0.6f);
    glBegin(GL_LINES);
    for (int i = 0; i < spokeCount; ++i) {
        float t = static_cast<float>(i) / spokeCount;
        float angle = t * 6.28318f + animatedTime * (0.6f + 0.3f * energy);
        float innerRadius = radius * (0.24f + 0.12f * std::sin(angle * 2.3f + pulse));
        float outerRadius = radius * (0.92f + 0.55f * std::sin(angle * 3.6f + harmonic * 1.3f));

        float innerX = centerX + std::cos(angle) * innerRadius;
        float innerY = centerY + std::sin(angle) * innerRadius;
        float outerX = centerX + std::cos(angle) * outerRadius;
        float outerY = centerY + std::sin(angle) * outerRadius;

        float spokeAlpha = std::clamp(0.22f + energy * 0.35f + high * 0.2f + prismShift * 0.25f, 0.18f, 0.95f);
        setColorWithAdjust(0.55f + bass * 0.25f + prismShift * 0.15f,
                           0.78f + mid * 0.3f + prismShift * 0.12f,
                           1.15f + high * 0.45f + prismShift * 0.2f,
                           spokeAlpha,
                           legacyColorAdjust_.circleOutline);
        glVertex2f(innerX, innerY);
        glVertex2f(outerX, outerY);
    }
    glEnd();

    generateContour(4,
                    1.08f + layerEnergy * 0.16f,
                    0.34f + harmonic * 0.14f,
                    scratch);
    glPointSize(3.0f + std::clamp(energy, 0.0f, 1.2f) * 6.0f + high * 3.0f + prismShift * 2.5f);
    glBegin(GL_POINTS);
    size_t step = std::max<size_t>(1, scratch.size() / (18 + static_cast<int>(high * 12.0f)));
    for (size_t i = 0; i < scratch.size(); i += step) {
        const auto& point = scratch[i];
        float sparkle = 0.6f + 0.4f * std::sin(point.angle * 5.0f + animatedTime * 4.0f + kick);
        setColorWithAdjust(0.65f + bass * 0.25f + sparkle * 0.2f + prismShift * 0.15f,
                           0.82f + mid * 0.35f + sparkle * 0.2f + prismShift * 0.12f,
                           1.25f + high * 0.5f + sparkle * 0.25f + prismShift * 0.22f,
                           0.4f + 0.45f * sparkle,
                           legacyColorAdjust_.sparkles);
        glVertex2f(point.x, point.y);
    }
    glEnd();
}

void Visualizer::seedLifeFromCore(float amount) {
    (void)amount;
}

void Visualizer::updateLife(float dt, const AudioAnalyzer::AudioFeatures& features) {
    (void)dt;
    (void)features;
}

void Visualizer::renderLife(float animatedTime) const {
    (void)animatedTime;
}

void Visualizer::renderIdleSpinner(float animatedTime) const {
    float idleEnergy = std::clamp(audioFeatures_.energy, 0.0f, 1.0f);
    if (idleEnergy > 0.08f) {
        return;
    }

    float centerX = windowWidth_ * 0.5f;
    float centerY = windowHeight_ * 0.5f;
    float minDim = static_cast<float>(std::min(windowWidth_, windowHeight_));
    float spinnerRadius = std::clamp(minDim * 0.08f, 18.0f, 64.0f);

    float visibility = std::clamp(1.0f - idleEnergy * 6.0f, 0.0f, 1.0f);
    float phase = animatedTime * (1.2f + 0.8f * visibility);
    int segments = 12;

    glPushMatrix();
    glTranslatef(centerX, centerY, 0.0f);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glLineWidth(2.0f);
    glBegin(GL_LINES);
    for (int i = 0; i < segments; ++i) {
        float t = static_cast<float>(i) / segments;
        float angle = phase + t * 6.28318f;
        float intensity = std::pow(t, 1.8f) * visibility;

        float innerRadius = spinnerRadius * (0.45f + 0.25f * intensity);
        float outerRadius = spinnerRadius * (0.8f + 0.35f * intensity);

        float cosA = std::cos(angle);
        float sinA = std::sin(angle);

        float r = 0.2f + 0.6f * intensity;
        float g = 0.4f + 0.4f * intensity;
        float b = 0.9f;
        float a = 0.18f + 0.55f * intensity;
        glColor4f(r, g, b, a);

        glVertex2f(cosA * innerRadius, sinA * innerRadius);
        glVertex2f(cosA * outerRadius, sinA * outerRadius);
    }
    glEnd();

    glDisable(GL_BLEND);
    glPopMatrix();
}

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform float uTime;
uniform float uBass;
uniform float uMid;
uniform float uHigh;
uniform float uEnergy;
uniform float uOnset;
uniform float uBeat;
uniform vec2 uResolution;

// Raymarching SDF functions
float sdSphere(vec3 p, float r) {
    return length(p) - r;
}

float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdTorus(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}

float sceneSDF(vec3 p) {
    float bassPulse = 1.0 + uBass * 3.0;
    float midWobble = sin(uTime * 2.0 + uMid * 10.0) * 0.2;
    float highNoise = uHigh * 0.1;
    
    // Reactive geometry
    vec3 p1 = p;
    p1.y += sin(uTime + p1.x * 2.0) * uMid * 0.5;
    
    float sphere = sdSphere(p1, 0.5 + bassPulse * 0.3);
    float box = sdBox(p + vec3(0.0, sin(uTime) * uMid, 0.0), vec3(0.3 + uBass));
    float torus = sdTorus(p.xzy, vec2(0.8 + uBass * 0.5, 0.1 + uHigh * 0.2));
    
    // Combine shapes
    float result = min(sphere, box);
    result = min(result, torus);
    
    // Add some fractal-like detail when there's high energy
    if (uEnergy > 0.3) {
        float detail = sdSphere(p * 3.0 + vec3(sin(uTime * 5.0), cos(uTime * 3.0), sin(uTime * 7.0)), 0.1);
        result = mix(result, detail, uHigh * 0.3);
    }
    
    return result;
}

vec3 getNormal(vec3 p) {
    vec2 e = vec2(0.001, 0.0);
    return normalize(vec3(
        sceneSDF(p + e.xyy) - sceneSDF(p - e.xyy),
        sceneSDF(p + e.yxy) - sceneSDF(p - e.yxy),
        sceneSDF(p + e.yyx) - sceneSDF(p - e.yyx)
    ));
}

vec3 raymarch(vec3 ro, vec3 rd) {
    float t = 0.0;
    int maxSteps = 100;
    float maxDist = 10.0;
    
    for (int i = 0; i < maxSteps; i++) {
        vec3 p = ro + rd * t;
        float d = sceneSDF(p);
        
        if (d < 0.001) {
            // Hit!
            vec3 normal = getNormal(p);
            
            // Psychedelic lighting based on audio
            vec3 lightDir = normalize(vec3(
                sin(uTime * 0.5) * uMid,
                cos(uTime * 0.3) * uBass,
                sin(uTime * 0.7) * uHigh
            ));
            
            float diff = max(dot(normal, lightDir), 0.0);
            
            // Color based on frequency bands
            vec3 baseColor = vec3(uBass, uMid, uHigh);
            vec3 color = baseColor * diff;

            float distToCenter = length(p);
            float coreEnvelope = exp(-distToCenter * (2.2 - uBass * 0.6)) * (0.6 + uEnergy * 0.8);
            float massGlow = smoothstep(0.0, 0.8 + uEnergy * 0.4, coreEnvelope);
            vec3 coreColor = mix(baseColor, vec3(1.0), 0.25 + uHigh * 0.35);
            color = mix(color, coreColor * (0.8 + uBass * 0.9), massGlow);
            color += coreColor * coreEnvelope * 0.35;

            // Add glow on beats
            if (uBeat > 0.5) {
                color *= 2.0;
            }
            
            // Add onset flash
            if (uOnset > 0.5) {
                color = mix(color, vec3(1.0), 0.5);
            }
            
            return color;
        }
        
        t += d;
        if (t > maxDist) break;
    }
    
    // Background - dark with subtle audio-reactive gradient
    return vec3(0.02 + uEnergy * 0.05, 0.01 + uMid * 0.03, 0.03 + uHigh * 0.04);
}

void main() {
    vec2 uv = (TexCoord - 0.5) * 2.0;
    uv.x *= uResolution.x / uResolution.y;
    
    vec3 ro = vec3(0.0, 0.0, 3.0);
    vec3 rd = normalize(vec3(uv, -1.0));
    
    // Camera movement based on audio
    ro.x += sin(uTime * 0.5) * uMid * 0.5;
    ro.y += cos(uTime * 0.3) * uBass * 0.3;
    
    vec3 color = raymarch(ro, rd);
    
    // Post-processing
    color = pow(color, vec3(0.8)); // Gamma correction
    color = smoothstep(0.0, 1.0, color); // Contrast
    
    // Add scanlines for retro rave feel
    float scanline = sin(TexCoord.y * uResolution.y * 2.0) * 0.02;
    color -= scanline;
    
    FragColor = vec4(color, 1.0);
}
)";

Visualizer::Visualizer() 
    : window_(nullptr), windowWidth_(800), windowHeight_(600), time_(0.0f),
      quadVAO_(0), quadVBO_(0), waveformVAO_(0), waveformVBO_(0),
      coreVAO_(0), coreVBO_(0), coreVertexCount_(0),
      audioFeatures_{},
      selectedDevice_(-1), showDeviceMenu_(false), showDiagnostic_(false), consoleMode_(false),
      showImGuiWindow_(true), showDeviceSelector_(false), showDiagnosticInfo_(false), showConsoleMode_(false),
      imguiInitialized_(false), autoRandomizeColors_(true), colorRandomInterval_(12.0f),
      colorRandomTimer_(0.0f), deltaTime_(0.0f), rng_(std::random_device{}()), currentPresetIndex_(0),
      onsetColorCyclingEnabled_(true), onsetTriggerCount_(0), lastOnsetActive_(false),
      tempoMultiplier_(1.0f),
      mixColorSchemes_(true),
      overlayLegacyOnModern_(false),
      legacyMotionBlend_(0.0f), legacyMotionPhase_(0.0f), legacySensitivity_(1.0f),
      showLegacyCore_(true), showLegacyArcs_(true), showLegacyRings_(true),
      showLegacySparkles_(true), showLegacyOrbs_(true), showLegacyWaveforms_(true),
      useModernPipeline_(false) {
    waveformBuffer_.resize(512); // Same as audio buffer size
    buildColorPresets();
    if (!colorPresets_.empty()) {
        applyLegacyPreset(0);
    } else {
        legacyColorAdjust_ = LegacyColorAdjust{};
    }
    setupDeviceList();
    core_ = {};
    idleState_ = 0.0f;
    idlePhase_ = 0.0f;
}

Visualizer::~Visualizer() {
    shutdown();
}

bool Visualizer::initialize(int width, int height) {
    windowWidth_ = width;
    windowHeight_ = height;

    if (!setupOpenGL()) {
        return false;
    }

    if (!setupGeometry()) {
        return false;
    }

    setupCoreMesh();

    if (!loadShaders()) {
        std::cout << "Failed to load shaders, using fallback rendering" << std::endl;
        shader_.reset(); // Will trigger fallback triangle
        useModernPipeline_ = false;
    } else {
        useModernPipeline_ = shader_ != nullptr;
    }

    if (!loadCoreShader()) {
        coreShader_.reset();
    }

    if (setupImGui()) {
        imguiInitialized_ = true;
    } else {
        std::cout << "ImGui initialization failed, continuing without ImGui interface" << std::endl;
        showImGuiWindow_ = false;
        imguiInitialized_ = false;
    }

    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    rendererName_ = renderer ? renderer : "Unknown";

    initializeDynamicSystems();

    return true;
}

void Visualizer::setColorWithAdjust(float r, float g, float b, float a, const ColorAdjust& adjust) const {
    auto clampComponent = [](float value) {
        if (value < 0.0f) return 0.0f;
        if (value > 1.0f) return 1.0f;
        return value;
    };

    glColor4f(
        clampComponent(r * adjust.r),
        clampComponent(g * adjust.g),
        clampComponent(b * adjust.b),
        clampComponent(a * adjust.a)
    );
}

void Visualizer::buildColorPresets() {
    colorPresets_.clear();

    auto makeAdjust = [](float r, float g, float b, float a) {
        ColorAdjust adjust;
        adjust.r = r;
        adjust.g = g;
        adjust.b = b;
        adjust.a = a;
        return adjust;
    };

    {
        LegacyColorAdjust preset;
        preset.circleFill = makeAdjust(0.7f, 1.35f, 1.6f, 1.0f);
        preset.circleOutline = makeAdjust(0.6f, 1.3f, 1.6f, 1.0f);
        preset.bloomInner = makeAdjust(0.5f, 1.3f, 1.5f, 0.85f);
        preset.bloomOuter = makeAdjust(0.4f, 1.2f, 1.5f, 0.45f);
        preset.bassBars = makeAdjust(0.6f, 1.4f, 1.5f, 1.0f);
        preset.midBars = makeAdjust(0.6f, 1.5f, 1.45f, 1.0f);
        preset.highBars = makeAdjust(0.6f, 1.55f, 1.6f, 1.0f);
        preset.beatExplosion = makeAdjust(0.6f, 1.35f, 1.6f, 1.0f);
        preset.rings = makeAdjust(0.5f, 1.4f, 1.4f, 0.55f);
        preset.orbit = makeAdjust(0.5f, 1.6f, 1.6f, 1.0f);
        preset.orbitTrail = makeAdjust(0.4f, 1.4f, 1.4f, 0.35f);
        preset.sparkles = makeAdjust(0.6f, 1.6f, 1.6f, 1.0f);
        preset.waveform = makeAdjust(0.4f, 1.4f, 1.4f, 1.0f);
        colorPresets_.push_back({"Neon Aqua", preset});
    }

    {
        LegacyColorAdjust preset;
        preset.circleFill = makeAdjust(1.5f, 0.6f, 1.6f, 1.0f);
        preset.circleOutline = makeAdjust(1.6f, 0.5f, 1.7f, 1.0f);
        preset.bloomInner = makeAdjust(1.4f, 0.5f, 1.6f, 0.85f);
        preset.bloomOuter = makeAdjust(1.3f, 0.4f, 1.5f, 0.45f);
        preset.bassBars = makeAdjust(1.6f, 0.5f, 1.6f, 1.0f);
        preset.midBars = makeAdjust(1.5f, 0.6f, 1.7f, 1.0f);
        preset.highBars = makeAdjust(1.4f, 0.5f, 1.8f, 1.0f);
        preset.beatExplosion = makeAdjust(1.6f, 0.4f, 1.6f, 1.0f);
        preset.rings = makeAdjust(1.5f, 0.5f, 1.5f, 0.55f);
        preset.orbit = makeAdjust(1.6f, 0.5f, 1.8f, 1.0f);
        preset.orbitTrail = makeAdjust(1.4f, 0.4f, 1.6f, 0.35f);
        preset.sparkles = makeAdjust(1.6f, 0.5f, 1.8f, 1.0f);
        preset.waveform = makeAdjust(1.4f, 0.5f, 1.6f, 1.0f);
        colorPresets_.push_back({"Magenta Pulse", preset});
    }

    {
        LegacyColorAdjust preset;
        preset.circleFill = makeAdjust(0.7f, 1.6f, 0.6f, 1.0f);
        preset.circleOutline = makeAdjust(0.6f, 1.7f, 0.6f, 1.0f);
        preset.bloomInner = makeAdjust(0.5f, 1.6f, 0.5f, 0.85f);
        preset.bloomOuter = makeAdjust(0.4f, 1.5f, 0.4f, 0.45f);
        preset.bassBars = makeAdjust(0.6f, 1.7f, 0.6f, 1.0f);
        preset.midBars = makeAdjust(0.6f, 1.6f, 0.5f, 1.0f);
        preset.highBars = makeAdjust(0.6f, 1.8f, 0.6f, 1.0f);
        preset.beatExplosion = makeAdjust(0.6f, 1.7f, 0.4f, 1.0f);
        preset.rings = makeAdjust(0.5f, 1.6f, 0.5f, 0.55f);
        preset.orbit = makeAdjust(0.5f, 1.8f, 0.6f, 1.0f);
        preset.orbitTrail = makeAdjust(0.4f, 1.5f, 0.4f, 0.35f);
        preset.sparkles = makeAdjust(0.6f, 1.7f, 0.5f, 1.0f);
        preset.waveform = makeAdjust(0.4f, 1.6f, 0.5f, 1.0f);
        colorPresets_.push_back({"Electric Lime", preset});
    }

    {
        LegacyColorAdjust preset;
        preset.circleFill = makeAdjust(1.6f, 0.7f, 0.5f, 1.0f);
        preset.circleOutline = makeAdjust(1.6f, 0.6f, 0.4f, 1.0f);
        preset.bloomInner = makeAdjust(1.5f, 0.6f, 0.4f, 0.85f);
        preset.bloomOuter = makeAdjust(1.4f, 0.5f, 0.3f, 0.45f);
        preset.bassBars = makeAdjust(1.7f, 0.6f, 0.4f, 1.0f);
        preset.midBars = makeAdjust(1.6f, 0.5f, 0.3f, 1.0f);
        preset.highBars = makeAdjust(1.7f, 0.6f, 0.4f, 1.0f);
        preset.beatExplosion = makeAdjust(1.8f, 0.6f, 0.4f, 1.0f);
        preset.rings = makeAdjust(1.6f, 0.6f, 0.4f, 0.55f);
        preset.orbit = makeAdjust(1.7f, 0.5f, 0.3f, 1.0f);
        preset.orbitTrail = makeAdjust(1.4f, 0.4f, 0.2f, 0.35f);
        preset.sparkles = makeAdjust(1.8f, 0.6f, 0.4f, 1.0f);
        preset.waveform = makeAdjust(1.5f, 0.5f, 0.3f, 1.0f);
        colorPresets_.push_back({"Inferno Neon", preset});
    }
}

void Visualizer::resetLegacyColorAdjustments() {
    if (currentPresetIndex_ >= 0 && currentPresetIndex_ < static_cast<int>(colorPresets_.size())) {
        legacyColorAdjust_ = colorPresets_[currentPresetIndex_].adjust;
    } else {
        legacyColorAdjust_ = LegacyColorAdjust{};
    }
    colorRandomTimer_ = 0.0f;
}

void Visualizer::applyLegacyPreset(int index) {
    if (index < 0 || index >= static_cast<int>(colorPresets_.size())) {
        return;
    }

    legacyColorAdjust_ = colorPresets_[index].adjust;
    currentPresetIndex_ = index;
    colorRandomTimer_ = 0.0f;
}

void Visualizer::randomizeLegacyColors() {
    if (colorPresets_.empty()) {
        return;
    }

    std::uniform_int_distribution<int> dist(0, static_cast<int>(colorPresets_.size()) - 1);

    if (!mixColorSchemes_ || colorPresets_.size() == 1) {
        int nextIndex = dist(rng_);
        if (colorPresets_.size() > 1) {
            int guard = 0;
            while (nextIndex == currentPresetIndex_ && guard < 8) {
                nextIndex = dist(rng_);
                ++guard;
            }
        }
        applyLegacyPreset(nextIndex);
        return;
    }

    // Mix different presets across visual layers
    LegacyColorAdjust mixed = legacyColorAdjust_;

    auto pickAdjust = [&](ColorAdjust LegacyColorAdjust::*member) {
        int idx = dist(rng_);
        const auto& preset = colorPresets_[idx].adjust;
        ColorAdjust value = preset.*member;
        mixed.*member = value;
    };

    pickAdjust(&LegacyColorAdjust::circleFill);
    pickAdjust(&LegacyColorAdjust::circleOutline);
    pickAdjust(&LegacyColorAdjust::bloomInner);
    pickAdjust(&LegacyColorAdjust::bloomOuter);
    pickAdjust(&LegacyColorAdjust::bassBars);
    pickAdjust(&LegacyColorAdjust::midBars);
    pickAdjust(&LegacyColorAdjust::highBars);
    pickAdjust(&LegacyColorAdjust::beatExplosion);
    pickAdjust(&LegacyColorAdjust::rings);
    pickAdjust(&LegacyColorAdjust::orbit);
    pickAdjust(&LegacyColorAdjust::orbitTrail);
    pickAdjust(&LegacyColorAdjust::sparkles);
    pickAdjust(&LegacyColorAdjust::waveform);

    legacyColorAdjust_ = mixed;
    colorRandomTimer_ = 0.0f;
}

void Visualizer::shutdown() {
    if (imguiInitialized_) {
        shutdownImGui();
        imguiInitialized_ = false;
        showImGuiWindow_ = false;
    }

    if (quadVAO_) {
        glDeleteVertexArrays(1, &quadVAO_);
        quadVAO_ = 0;
    }
    if (quadVBO_) {
        glDeleteBuffers(1, &quadVBO_);
        quadVBO_ = 0;
    }
    if (coreVAO_) {
        glDeleteVertexArrays(1, &coreVAO_);
        coreVAO_ = 0;
    }
    if (coreVBO_) {
        glDeleteBuffers(1, &coreVBO_);
        coreVBO_ = 0;
    }
    if (waveformVAO_) {
        glDeleteVertexArrays(1, &waveformVAO_);
        waveformVAO_ = 0;
    }
    if (waveformVBO_) {
        glDeleteBuffers(1, &waveformVBO_);
        waveformVBO_ = 0;
    }
    
    shader_.reset();
    coreShader_.reset();

    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

bool Visualizer::shouldClose() {
    return window_ ? glfwWindowShouldClose(window_) : true;
}

void Visualizer::beginFrame() {
    glfwPollEvents();

    if (window_) {
        int fbWidth = 0;
        int fbHeight = 0;
        glfwGetFramebufferSize(window_, &fbWidth, &fbHeight);
        if (fbWidth > 0 && fbHeight > 0 && (fbWidth != windowWidth_ || fbHeight != windowHeight_)) {
            windowWidth_ = fbWidth;
            windowHeight_ = fbHeight;
            glViewport(0, 0, windowWidth_, windowHeight_);
        }
    }

    if (imguiInitialized_ && window_) {
        static double lastTabToggle = 0.0;
        double now = glfwGetTime();
        bool tabDown = glfwGetKey(window_, GLFW_KEY_TAB) == GLFW_PRESS;
        ImGuiIO* io = ImGui::GetCurrentContext() ? &ImGui::GetIO() : nullptr;
        bool allowToggle = !tabDown ? false
                          : (!io || !io->WantCaptureKeyboard || !showImGuiWindow_);

        if (allowToggle && (now - lastTabToggle) > 0.25) {
            showImGuiWindow_ = !showImGuiWindow_;
            if (!showImGuiWindow_) {
                showDeviceSelector_ = false;
                showDiagnosticInfo_ = false;
                showConsoleMode_ = false;
            }
            lastTabToggle = now;
        }
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Visualizer::endFrame() {
    glfwSwapBuffers(window_);
    
    // Update time
    static auto lastTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
    lastTime = currentTime;
    time_ += deltaTime;
    deltaTime_ = deltaTime;

    if (autoRandomizeColors_) {
        if (colorRandomInterval_ < 0.5f) {
            colorRandomInterval_ = 0.5f;
        }
        colorRandomTimer_ += deltaTime;
        if (colorRandomTimer_ >= colorRandomInterval_) {
            randomizeLegacyColors();
        }
    }

    if (onsetColorCyclingEnabled_) {
        bool onsetActive = audioFeatures_.onset > 0.5f;
        if (onsetActive && !lastOnsetActive_) {
            ++onsetTriggerCount_;
            if (onsetTriggerCount_ >= 2) {
                onsetTriggerCount_ = 0;
                if (!colorPresets_.empty()) {
                    int nextIndex = (currentPresetIndex_ + 1) % static_cast<int>(colorPresets_.size());
                    applyLegacyPreset(nextIndex);
                } else {
                    randomizeLegacyColors();
                }
            }
        }
        lastOnsetActive_ = onsetActive;
    } else {
        onsetTriggerCount_ = 0;
        lastOnsetActive_ = audioFeatures_.onset > 0.5f;
    }
}

void Visualizer::updateAudioData(const AudioAnalyzer::AudioFeatures& features) {
    audioFeatures_ = features;

    float targetTempo = 1.0f;
    float bpm = features.bpm;
    if (bpm > 30.0f) {
        targetTempo = std::clamp(bpm / 120.0f, 0.35f, 1.8f);
    } else {
        float energy = std::clamp(features.energy, 0.0f, 1.2f);
        targetTempo = 0.75f + energy * 0.35f;
    }

    tempoMultiplier_ = std::clamp(tempoMultiplier_ * 0.9f + targetTempo * 0.1f, 0.3f, 2.0f);
}

void Visualizer::updateAudioBuffer(const std::vector<float>& audioBuffer) {
    if (audioBuffer.size() >= waveformBuffer_.size()) {
        std::copy(audioBuffer.begin(), audioBuffer.begin() + waveformBuffer_.size(), waveformBuffer_.begin());
    }
}

void Visualizer::render() {
    // Clear screen
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (useModernPipeline_ && shader_) {
        renderModernVisualization();
        if (overlayLegacyOnModern_) {
            renderLegacyVisualization(true);
        }
    } else {
        renderLegacyVisualization();
    }

    renderIdleSpinner(time_);

    if (imguiInitialized_ && showImGuiWindow_) {
        renderImGui();
    } else if (!imguiInitialized_) {
        renderGUI();
    }
}

bool Visualizer::setupOpenGL() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);  // Use OpenGL 2.1 for compatibility
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);  // Don't force core profile
    
    // Try with OpenGL ES if desktop OpenGL fails
    bool useGLES = false;

    window_ = glfwCreateWindow(windowWidth_, windowHeight_, "Audio Visualizer", nullptr, nullptr);
    if (!window_) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        return false;
    }

    glfwMakeContextCurrent(window_);

    // Initialize GLEW with experimental features
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(glewInit()) << std::endl;
        
        // Try without GLEW for basic functionality
        std::cout << "Attempting to continue without GLEW..." << std::endl;
    }

    // Clear any potential GL errors from GLEW initialization
    glGetError();

    glViewport(0, 0, windowWidth_, windowHeight_);
    
    std::cout << "OpenGL setup successful!" << std::endl;
    return true;
}

bool Visualizer::setupGeometry() {
    setupQuad();
    setupWaveform();
    return true;
}

bool Visualizer::loadShaders() {
    shader_ = std::make_unique<Shader>();
    return shader_->loadFromSource(vertexShaderSource, fragmentShaderSource);
}

bool Visualizer::loadCoreShader() {
    coreShader_ = std::make_unique<Shader>();
    return coreShader_->loadFromSource(coreVertexShaderSource, coreFragmentShaderSource);
}

void Visualizer::setupQuad() {
    float vertices[] = {
        // positions    // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f
    };

    glGenVertexArrays(1, &quadVAO_);
    glGenBuffers(1, &quadVBO_);
    
    glBindVertexArray(quadVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    // position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // tex coord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
}

void Visualizer::setupWaveform() {
    glGenVertexArrays(1, &waveformVAO_);
    glGenBuffers(1, &waveformVBO_);
    
    glBindVertexArray(waveformVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, waveformVBO_);
    
    // Allocate buffer memory (will be updated dynamically)
    glBufferData(GL_ARRAY_BUFFER, waveformBuffer_.size() * sizeof(float) * 2, nullptr, GL_DYNAMIC_DRAW);
    
    // position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
}

void Visualizer::renderWaveform(const std::vector<float>& audioBuffer) {
    if (audioBuffer.empty()) return;
    
    // Create vertices for waveform
    std::vector<float> vertices;
    vertices.reserve(audioBuffer.size() * 2);
    
    float waveHeight = 100.0f; // Height of waveform display
    float waveY = windowHeight_ - waveHeight - 20.0f; // Position at bottom
    
    for (size_t i = 0; i < audioBuffer.size(); ++i) {
        float x = (float)i / (audioBuffer.size() - 1) * windowWidth_;
        float y = waveY + audioBuffer[i] * waveHeight * 0.5f;
        vertices.push_back(x);
        vertices.push_back(y);
    }
    
    // Update VBO with new waveform data
    glBindBuffer(GL_ARRAY_BUFFER, waveformVBO_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());
    
    // Simple shader for waveform (colored line)
    glUseProgram(0); // Use fixed function pipeline for simplicity
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, windowWidth_, windowHeight_, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Disable depth test for overlay
    glDisable(GL_DEPTH_TEST);
    
    // Draw waveform as line strip
    glColor3f(0.0f, 1.0f, 0.5f); // Cyan color
    glLineWidth(2.0f);
    
    glBindVertexArray(waveformVAO_);
    glDrawArrays(GL_LINE_STRIP, 0, audioBuffer.size());
    glBindVertexArray(0);
    
    // Restore state
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void Visualizer::renderModernVisualization() {
    if (!shader_ || quadVAO_ == 0) {
        renderFallbackTriangle();
        return;
    }

    shader_->use();
    shader_->setUniform1f("uTime", time_);
    shader_->setUniform1f("uBass", audioFeatures_.bassEnergy);
    shader_->setUniform1f("uMid", audioFeatures_.midEnergy);
    shader_->setUniform1f("uHigh", audioFeatures_.highEnergy);
    shader_->setUniform1f("uEnergy", audioFeatures_.energy);
    shader_->setUniform1f("uOnset", audioFeatures_.onset);
    shader_->setUniform1f("uBeat", audioFeatures_.beat);
    shader_->setUniform2f("uResolution",
                          static_cast<float>(windowWidth_),
                          static_cast<float>(windowHeight_));

    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (depthWasEnabled) {
        glDisable(GL_DEPTH_TEST);
    }

    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    if (!blendWasEnabled) {
        glEnable(GL_BLEND);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glUseProgram(0);

    if (!blendWasEnabled) {
        glDisable(GL_BLEND);
    }
    if (depthWasEnabled) {
        glEnable(GL_DEPTH_TEST);
    }

    renderModernCore();
}

void Visualizer::renderModernCore() {
    if (!coreShader_ || coreVAO_ == 0) {
        renderGears(time_);
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
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    coreShader_->use();
    coreShader_->setUniform2f("uResolution", static_cast<float>(windowWidth_), static_cast<float>(windowHeight_));
    coreShader_->setUniform1f("uBaseRadius", core_.baseRadius);
    coreShader_->setUniform1f("uPulse", core_.pulse);
    coreShader_->setUniform1f("uKick", core_.kickEnvelope);
    coreShader_->setUniform1f("uHarmonic", core_.harmonicEnvelope);
    coreShader_->setUniform1f("uEnergy", audioFeatures_.energy);
    coreShader_->setUniform1f("uBass", audioFeatures_.bassEnergy);
    coreShader_->setUniform1f("uMid", audioFeatures_.midEnergy);
    coreShader_->setUniform1f("uHigh", audioFeatures_.highEnergy);
    coreShader_->setUniform1f("uTime", time_);
    coreShader_->setUniform1f("uTempo", tempoMultiplier_);

    glBindVertexArray(coreVAO_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, coreVertexCount_);
    glBindVertexArray(0);

    glUseProgram(0);

    if (!blendWasEnabled) {
        glDisable(GL_BLEND);
    }
    if (depthWasEnabled) {
        glEnable(GL_DEPTH_TEST);
    }
}

void Visualizer::setupDeviceList() {
    // Get device names
    PaError err = Pa_Initialize();
    if (err != paNoError) return;
    
    int numDevices = Pa_GetDeviceCount();
    deviceNames_.clear();
    deviceIsInternal_.clear();
    
    for (int i = 0; i < numDevices; ++i) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceInfo && deviceInfo->maxInputChannels > 0) {
            deviceNames_.push_back(deviceInfo->name);
            bool isInternalLoopback = false;
            if (const PaHostApiInfo* hostInfo = Pa_GetHostApiInfo(deviceInfo->hostApi)) {
                const std::string hostName(hostInfo->name ? hostInfo->name : "");
                const std::string deviceName(deviceInfo->name ? deviceInfo->name : "");
                if (hostName.find("WASAPI") != std::string::npos) {
                    if (deviceName.find("(loopback)") != std::string::npos) {
                        isInternalLoopback = true;
                    }
                } else if (hostName.find("WDM-KS") != std::string::npos) {
                    if (deviceName.find("Loopback") != std::string::npos ||
                        deviceName.find("(loopback)") != std::string::npos) {
                        isInternalLoopback = true;
                    }
                } else if (hostName.find("Core Audio") != std::string::npos) {
                    if (deviceName.find("Loopback") != std::string::npos) {
                        isInternalLoopback = true;
                    }
                } else if (hostName.find("ALSA") != std::string::npos) {
                    if (deviceName.find("Monitor") != std::string::npos) {
                        isInternalLoopback = true;
                    }
                }
            }
            deviceIsInternal_.push_back(isInternalLoopback);
        } else {
            deviceNames_.push_back(""); // No input channels
            deviceIsInternal_.push_back(false);
        }
    }
    
    Pa_Terminate();
}

void Visualizer::renderGUI() {
    // Check for 'D' key toggle
    if (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS) {
        static double lastPress = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastPress > 0.5) { // 500ms debounce
            showDeviceMenu_ = !showDeviceMenu_;
            lastPress = currentTime;
        }
    }
    
    // Check for 'I' key toggle for diagnostic mode
    if (glfwGetKey(window_, GLFW_KEY_I) == GLFW_PRESS) {
        static double lastPress = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastPress > 0.5) { // 500ms debounce
            showDiagnostic_ = !showDiagnostic_;
            lastPress = currentTime;
        }
    }
    
    // Check for 'C' key toggle for console mode
    if (glfwGetKey(window_, GLFW_KEY_C) == GLFW_PRESS) {
        static double lastPress = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastPress > 0.5) { // 500ms debounce
            consoleMode_ = !consoleMode_;
            lastPress = currentTime;
        }
    }
    
    if (showDeviceMenu_) {
        showDeviceSelector();
    }
    
    // Show current device info
    std::string deviceInfo = "Device: ";
    if (selectedDevice_ >= 0 && selectedDevice_ < deviceNames_.size()) {
        deviceInfo += deviceNames_[selectedDevice_];
    } else {
        deviceInfo += "Default";
    }
    deviceInfo += " (D:devices I:diagnostics C:console)";
    renderText(deviceInfo, 10, 30);
    
    // Show diagnostic info if enabled
    if (showDiagnostic_) {
        renderDiagnosticInfo();
    }
    
    // Show console visualization if enabled
    if (consoleMode_) {
        renderConsoleVisualization();
    }
}

bool Visualizer::showDeviceSelector() {
    // Simple device selector using text rendering
    float menuX = 50.0f;
    float menuY = 100.0f;
    float lineHeight = 25.0f;
    
    renderText("=== Select Audio Device ===", menuX, menuY);
    renderText("Use number keys 1-9 to select", menuX, menuY + lineHeight);
    renderText("Press ESC to cancel", menuX, menuY + lineHeight * 2);
    renderText("", menuX, menuY + lineHeight * 3);
    
    // Show available devices
    int displayCount = 0;
    for (int i = 0; i < deviceNames_.size() && displayCount < 9; ++i) {
        if (!deviceNames_[i].empty()) {
            std::string deviceText = std::to_string(displayCount + 1) + ". " + deviceNames_[i];
            if (i == selectedDevice_) {
                deviceText += " [CURRENT]";
            }
            renderText(deviceText, menuX, menuY + lineHeight * (4 + displayCount));
            displayCount++;
        }
    }
    
    // Handle number key presses
    for (int i = 0; i < 9; ++i) {
        if (glfwGetKey(window_, GLFW_KEY_1 + i) == GLFW_PRESS) {
            // Find the actual device index
            int actualIndex = -1;
            int count = 0;
            for (int j = 0; j < deviceNames_.size(); ++j) {
                if (!deviceNames_[j].empty()) {
                    if (count == i) {
                        actualIndex = j;
                        break;
                    }
                    count++;
                }
            }
            
            if (actualIndex >= 0) {
                selectedDevice_ = actualIndex;
                showDeviceMenu_ = false;
                return true; // Device changed
            }
        }
    }
    
    // Handle ESC
    if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        showDeviceMenu_ = false;
    }
    
    return false;
}

void Visualizer::renderText(const std::string& text, float x, float y) {
    // Simple text rendering using bitmap characters (basic implementation)
    // For now, we'll use a very simple approach with line segments
    
    glUseProgram(0); // Use fixed function pipeline
    
    // Setup 2D projection
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, windowWidth_, windowHeight_, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Disable depth test for overlay
    glDisable(GL_DEPTH_TEST);
    
    // Set text color
    glColor3f(1.0f, 1.0f, 1.0f);
    
    // Very basic text rendering (just show the text as a placeholder)
    // In a real implementation, you'd use a proper font rendering system
    glRasterPos2f(x, y);
    
    // For now, just print to console as a fallback
    // This is a placeholder - proper text rendering would require a font library
    static std::string lastText;
    if (text != lastText) {
        std::cout << "GUI: " << text << std::endl;
        lastText = text;
    }
    
    // Restore state
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}
