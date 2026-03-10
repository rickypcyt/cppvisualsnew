#include "visualizer.h"
#include <cmath>
#include <algorithm>
#include <iostream>

void Visualizer::renderLegacyVisualization(bool overlay) {
    // Use legacy OpenGL 1.1 for maximum compatibility
    glUseProgram(0);  // Ensure no shader program is active

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, windowWidth_, windowHeight_, 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (depthWasEnabled) {
        glDisable(GL_DEPTH_TEST);
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    const float silenceThreshold = 0.015f;
    const float beatFlash = audioFeatures_.beat > 0.5f ? 1.0f : 0.0f;
    const float onsetFlash = audioFeatures_.onset > 0.5f ? 1.0f : 0.0f;

    float sensitivity = std::clamp(legacySensitivity_, 0.1f, 5.0f);

    auto applySensitivity = [sensitivity](float value) {
        float scaled = value * sensitivity;
        return std::clamp(scaled, 0.0f, 1.5f);
    };

    float energy = applySensitivity(std::clamp(audioFeatures_.energy, 0.0f, 1.0f));
    float bass = applySensitivity(std::clamp(audioFeatures_.bassEnergy, 0.0f, 1.0f));
    float mid = applySensitivity(std::clamp(audioFeatures_.midEnergy, 0.0f, 1.0f));
    float high = applySensitivity(std::clamp(audioFeatures_.highEnergy, 0.0f, 1.0f));
    float energySnapshot = energy;

    float beatPulse = std::clamp(audioFeatures_.beat, 0.0f, 1.0f);
    float onsetPulse = std::clamp(audioFeatures_.onset, 0.0f, 1.0f);
    float transientActivity = std::max({audioFeatures_.kick, audioFeatures_.clap, audioFeatures_.hiHat,
                                        beatPulse, onsetPulse});
    transientActivity = std::clamp(transientActivity, 0.0f, 1.0f);

    static float transientEnvelope = 0.0f;
    transientEnvelope = transientEnvelope * 0.88f + transientActivity * 0.12f;
    transientEnvelope = std::clamp(transientEnvelope, 0.0f, 1.0f);

    float peak = std::clamp(energySnapshot, 0.0f, 1.5f);
    float peakBoost = std::clamp(0.45f + peak * 0.35f, 0.45f, 1.3f);
    float quietPressure = std::clamp(energy * (1.0f - transientEnvelope), 0.0f, 1.2f);
    float calmGate = std::clamp(1.0f - quietPressure * 0.65f, 0.25f, 1.0f);
    float highGate = std::clamp(0.45f + transientEnvelope * 0.55f, 0.35f, 1.05f);

    energy *= calmGate * peakBoost;
    bass *= calmGate * peakBoost;
    mid *= calmGate * peakBoost;
    high *= highGate * peakBoost;

    AudioAnalyzer::AudioFeatures driveFeatures = audioFeatures_;
    driveFeatures.energy = energy;
    driveFeatures.bassEnergy = bass;
    driveFeatures.midEnergy = mid;
    driveFeatures.highEnergy = high;

    updateCore(deltaTime_, driveFeatures);
    updateGears(deltaTime_, driveFeatures);
    updateLife(deltaTime_, driveFeatures);

    float targetMotionBlend = energy > silenceThreshold ? 1.0f : 0.0f;
    legacyMotionBlend_ = legacyMotionBlend_ * 0.85f + targetMotionBlend * 0.15f;
    legacyMotionBlend_ = std::clamp(legacyMotionBlend_, 0.0f, 1.0f);

    if (energy > silenceThreshold) {
        legacyMotionPhase_ += deltaTime_ * (0.5f + energy * 2.0f);
    }

    float animatedTime = legacyMotionPhase_;
    float globalPulse = 0.4f + 0.6f * energy;

    // Render visual elements using legacy OpenGL

    float minDimension = std::min(windowWidth_, windowHeight_);
    float safeMinDimension = std::max(minDimension, 1.0f);
    float centerX = windowWidth_ / 2.0f;
    float centerY = windowHeight_ / 2.0f;

    // Ambient background (solid black) only when not overlaying
    if (!overlay) {
        glBegin(GL_QUADS);
        glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
        glVertex2f(0.0f, 0.0f);
        glVertex2f(windowWidth_, 0.0f);
        glVertex2f(windowWidth_, windowHeight_);
        glVertex2f(0.0f, windowHeight_);
        glEnd();
    }

    float bloomRadius = minDimension * (0.60f + energy * 0.35f + globalPulse * 0.18f + beatFlash * 0.08f);
    glBegin(GL_TRIANGLE_FAN);
    setColorWithAdjust(0.05f + bass * 0.25f,
                       0.07f + mid * 0.22f,
                       0.15f + high * 0.35f,
                       0.38f,
                       legacyColorAdjust_.bloomInner);
    glVertex2f(centerX, centerY);
    int bloomSegments = 96;
    for (int i = 0; i <= bloomSegments; ++i) {
        float angle = 2.0f * 3.14159f * i / bloomSegments;
        float x = centerX + cosf(angle) * bloomRadius * (windowWidth_ / safeMinDimension);
        float y = centerY + sinf(angle) * bloomRadius * (windowHeight_ / safeMinDimension);
        glColor4f(0.0f, 0.0f, 0.0f, 0.0f);
        glVertex2f(x, y);
    }
    glEnd();

    // 1. Central reactive circle with aura
    if (showLegacyCore_) {
        renderGears(animatedTime);
    }
    // 2. Frequency arcs
    if (showLegacyArcs_) {
        renderLegacyFrequencyBars(bass, mid, high, legacyMotionBlend_, animatedTime);
    }

    if (depthWasEnabled) {
        glEnable(GL_DEPTH_TEST);
    }

    float bpm = audioFeatures_.bpm;
    if (bpm > 0.1f) {
        char bpmText[32];
        snprintf(bpmText, sizeof(bpmText), "BPM: %.0f", bpm);
        renderText(bpmText, 20.0f, windowHeight_ - 40.0f);
    }

    // Restore state
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    if (depthWasEnabled) {
        glEnable(GL_DEPTH_TEST);
    }

    glPopMatrix(); // MODELVIEW
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void Visualizer::renderLegacyCircle(float bass, float mid, float high, float motionBlend, float animatedTime) {
    float centerX = windowWidth_ / 2.0f;
    float centerY = windowHeight_ / 2.0f;
    float minDimension = static_cast<float>(std::min(windowWidth_, windowHeight_));

    float energyMix = std::clamp(0.48f * bass + 0.34f * mid + 0.24f * high, 0.0f, 1.3f);
    float beatPulse = std::clamp(audioFeatures_.beat, 0.0f, 1.0f);
    float kickRaw = std::clamp(audioFeatures_.kick, 0.0f, 1.0f);
    static float kickEnvelope = 0.0f;
    kickEnvelope = kickEnvelope * 0.74f + kickRaw * 0.26f;
    float harmonicEnvelope = std::clamp(core_.harmonicEnvelope, 0.0f, 1.6f);
    static float harmonicSwell = 0.0f;
    harmonicSwell = harmonicSwell * 0.78f + harmonicEnvelope * 0.22f;
    float harmonicPulse = harmonicSwell * (0.85f + 0.15f * std::sin(animatedTime * 3.2f + harmonicSwell * 1.6f));
    static float breath = 0.0f;
    breath = breath * 0.97f + energyMix * 0.03f;
    float kickPulse = kickEnvelope * (1.08f + energyMix * 0.45f);

    auto fract = [](float x) {
        return x - std::floor(x);
    };

    auto hash = [&](float n) {
        return fract(std::sin(n) * 43758.5453f);
    };

    float growthEnvelope = std::clamp(core_.growthEnvelope, 0.0f, 4.0f);
    float maturity = std::clamp(core_.maturity, 0.0f, 6.0f);

    float baseRadius = minDimension * (0.052f
                                       + energyMix * 0.028f
                                       + motionBlend * 0.020f
                                       + beatPulse * 0.012f
                                       + 0.014f * std::clamp(harmonicSwell, 0.0f, 1.2f)
                                       + 0.010f * std::clamp(growthEnvelope, 0.0f, 1.5f)
                                       + 0.008f * std::clamp(maturity, 0.0f, 1.8f));
    float pulseDrive = std::clamp(kickPulse * 1.35f + energyMix * 0.6f, 0.0f, 3.5f);
    float harmonicScale = 1.05f + 0.75f * std::clamp(harmonicPulse, 0.0f, 1.6f);
    float growthScale = 1.0f + 0.55f * std::clamp(growthEnvelope * 0.22f + maturity * 0.15f, 0.0f, 1.8f);

    float radius = baseRadius * (1.65f + pulseDrive) * harmonicScale * growthScale;
    float glitchDrive = std::clamp(high * 0.9f + audioFeatures_.onset * 1.3f + audioFeatures_.beat * 1.1f + audioFeatures_.clap * 0.7f, 0.0f, 4.0f);
    float tempoWarp = std::clamp(audioFeatures_.bpm > 10.0f ? (audioFeatures_.bpm - 70.0f) / 90.0f : energyMix, 0.0f, 1.0f);
    radius += minDimension * (0.018f + glitchDrive * 0.022f + motionBlend * 0.015f + tempoWarp * 0.012f);
    radius = std::clamp(radius, minDimension * 0.034f, minDimension * 0.22f);

    int segments = 96;
    float kickGlow = std::clamp(0.25f + 0.75f * kickEnvelope, 0.25f, 1.15f);
    float harmonicGlow = std::clamp(0.2f + 0.8f * harmonicPulse, 0.2f, 1.2f);
    float glitchPhase = animatedTime * (2.5f + glitchDrive * 0.4f + tempoWarp * 0.65f);
    float glitchBeat = std::sin(glitchPhase * 2.2f + audioFeatures_.beat * 5.2f);
    float glitchBurst = std::clamp(glitchDrive * 0.32f + audioFeatures_.onset * 0.52f + glitchBeat * 0.25f, 0.0f, 1.9f);

    float wobble = sinf(animatedTime * (2.2f + high * 1.4f)) * radius * (0.16f + 0.12f * glitchBurst)
                   + kickEnvelope * radius * 0.12f * std::sin(animatedTime * (13.5f + glitchBurst * 2.5f))
                   + harmonicPulse * radius * 0.12f * std::sin(animatedTime * (7.2f + glitchBurst * 1.2f) + 1.8f);

    float ringThickness = std::clamp(minDimension * (0.0065f + 0.0105f * energyMix + 0.0125f * harmonicPulse + 0.0075f * motionBlend + 0.010f * glitchBurst),
                                     minDimension * 0.0045f,
                                     minDimension * 0.04f);
    float ringAlpha = std::clamp(0.42f + 0.48f * energyMix + 0.35f * kickEnvelope + 0.45f * std::clamp(harmonicPulse, 0.0f, 1.2f)
                                 + glitchBurst * 0.35f,
                                 0.35f,
                                 1.35f);

    float breathing = sinf(animatedTime * (0.6f + breath * 1.2f)) * radius * (0.25f + breath * 0.4f);
    float globalSpin = animatedTime * (0.2f + energyMix * 0.5f);

    for (int layer = 0; layer < 3; ++layer) {
        float layerScale = 1.0f + layer * 0.18f;
        float layerAlpha = ringAlpha * (1.0f - layer * 0.3f);
        float layerRadius = radius * layerScale;
        float layerWobble = wobble * layerScale;
        float layerBreathing = breathing * layerScale;

        glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= segments; ++i) {
            float t = static_cast<float>(i) / segments;
            float angleBase = t * 6.28318f;
            float jitterSeed = hash(i * 7.31f + glitchPhase * 0.71f);
            float jitterAngle = (jitterSeed - 0.5f) * (0.22f + glitchBurst * 0.55f);
            float angle = angleBase + jitterAngle;
            angle += globalSpin;

            float warp1 = sinf(angle * 4.0f + animatedTime * 2.1f) * 0.15f;
            float warp2 = cosf(angle * 7.0f - animatedTime * 1.6f) * 0.10f;
            float warp3 = sinf(angle * 11.0f + animatedTime * 3.4f) * 0.07f;
            float angleWarp = angle + (warp1 + warp2 + warp3) * (0.4f + energyMix);

            float ripple = sinf(angle * (3.5f + glitchBurst * 1.8f) + animatedTime * (3.0f + glitchBurst * 1.7f)) * layerRadius * (0.14f + 0.05f * glitchBurst)
                           + harmonicPulse * layerRadius * 0.07f * std::sin(angle * (6.0f + glitchBurst * 2.5f) + animatedTime * 2.8f)
                           + (jitterSeed - 0.5f) * minDimension * (0.012f + glitchBurst * 0.02f);

            float glitchStep = std::fmod(glitchPhase * 4.0f + i * 0.6f, 1.0f);
            float angularTear = std::sin(glitchStep * 6.28318f + jitterSeed * 12.0f) * minDimension * (0.01f + glitchBurst * 0.018f);

            float currentRadius = layerRadius + layerWobble + ripple + layerBreathing + angularTear;
            float innerRadius = std::max(currentRadius - ringThickness * (0.45f + 0.25f * jitterSeed), minDimension * 0.008f);
            float outerRadius = innerRadius + ringThickness * (0.9f + 0.35f * jitterSeed + 0.25f * glitchBurst);

            float cosA = cosf(angleWarp);
            float sinA = sinf(angleWarp);

            float fillPulse = std::clamp(energyMix * 0.55f + harmonicPulse * 0.35f + glitchBurst * 0.6f, 0.0f, 2.0f);
            float fillHueShift = jitterSeed * 0.35f;

            setColorWithAdjust(0.45f + 0.55f * energyMix + kickGlow * (0.28f + 0.3f * jitterSeed)
                               + harmonicGlow * (0.32f + 0.2f * fillHueShift)
                               + 0.25f * fillPulse,
                               0.38f + 0.48f * mid + kickGlow * (0.18f + 0.12f * jitterSeed)
                               + harmonicGlow * (0.38f + 0.22f * fillHueShift)
                               + 0.18f * fillPulse,
                               0.68f + 0.7f * high + kickGlow * (0.3f + 0.18f * fillHueShift)
                               + harmonicGlow * (0.28f + 0.22f * jitterSeed)
                               + 0.28f * fillPulse,
                               layerAlpha * 0.6f,
                               legacyColorAdjust_.circleFill);
            glVertex2f(centerX + cosA * innerRadius,
                       centerY + sinA * innerRadius);

            setColorWithAdjust(0.58f + 0.65f * energyMix + kickGlow * (0.38f + 0.32f * jitterSeed)
                               + harmonicGlow * (0.42f + 0.28f * fillHueShift)
                               + 0.32f * fillPulse,
                               0.48f + 0.55f * mid + kickGlow * (0.24f + 0.15f * jitterSeed)
                               + harmonicGlow * (0.45f + 0.18f * fillHueShift)
                               + 0.24f * fillPulse,
                               0.82f + 0.82f * high + kickGlow * (0.34f + 0.22f * fillHueShift)
                               + harmonicGlow * (0.35f + 0.25f * jitterSeed)
                               + 0.34f * fillPulse,
                               layerAlpha,
                               legacyColorAdjust_.circleFill);
            glVertex2f(centerX + cosA * outerRadius,
                       centerY + sinA * outerRadius);
        }
        glEnd();
    }

    glBegin(GL_TRIANGLE_FAN);
    setColorWithAdjust(0.4f, 0.5f, 1.0f, 0.15f, legacyColorAdjust_.bloomInner);
    glVertex2f(centerX, centerY);

    for (int i = 0; i <= 128; ++i) {
        float a = i / 128.0f * 6.28318f;
        float r = radius * (2.5f + sinf(animatedTime + a * 3.0f) * 0.3f);

        glColor4f(0.0f, 0.0f, 0.0f, 0.0f);

        glVertex2f(
            centerX + cosf(a) * r,
            centerY + sinf(a) * r
        );
    }
    glEnd();

    // Outline and shard lines removed per design simplification.
}

void Visualizer::renderLegacyFrequencyBars(float bass, float mid, float high, float motionBlend, float animatedTime) {
    float width = static_cast<float>(windowWidth_);
    float height = static_cast<float>(windowHeight_);
    float minDimension = std::max(1.0f, std::min(width, height));
    float centerX = width * 0.5f;
    float centerY = height * 0.5f;
    float aspectX = width / minDimension;
    float aspectY = height / minDimension;

    float beatPulse = std::clamp(audioFeatures_.beat, 0.0f, 1.0f);
    float onsetPulse = std::clamp(audioFeatures_.onset, 0.0f, 1.0f);
    float globalEnergy = std::clamp((bass + mid + high) / 3.0f, 0.0f, 1.0f);
    float bpm = std::clamp(audioFeatures_.bpm, 0.0f, 220.0f);
    float tempoFactor = (bpm <= 0.01f)
        ? globalEnergy
        : std::clamp((bpm - 70.0f) / 90.0f, 0.0f, 1.0f);
    float calmScale = std::clamp(0.25f + tempoFactor * 0.75f, 0.18f, 1.0f);
    float dynamicsScale = std::clamp(0.35f + globalEnergy * 0.65f, 0.2f, 1.0f);

    auto hsvToRgb = [](float h, float s, float v, float& r, float& g, float& b) {
        h = std::fmod(h, 1.0f);
        if (h < 0.0f) {
            h += 1.0f;
        }
        s = std::clamp(s, 0.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);

        float c = v * s;
        float hp = h * 6.0f;
        float x = c * (1.0f - std::fabs(std::fmod(hp, 2.0f) - 1.0f));
        float m = v - c;

        float rr = 0.0f, gg = 0.0f, bb = 0.0f;
        if (hp < 1.0f) {
            rr = c; gg = x; bb = 0.0f;
        } else if (hp < 2.0f) {
            rr = x; gg = c; bb = 0.0f;
        } else if (hp < 3.0f) {
            rr = 0.0f; gg = c; bb = x;
        } else if (hp < 4.0f) {
            rr = 0.0f; gg = x; bb = c;
        } else if (hp < 5.0f) {
            rr = x; gg = 0.0f; bb = c;
        } else {
            rr = c; gg = 0.0f; bb = x;
        }

        r = rr + m;
        g = gg + m;
        b = bb + m;
    };

    auto fract = [](float x) {
        return x - std::floor(x);
    };

    auto hash = [&](float n) {
        return fract(std::sin(n) * 43758.5453f);
    };

    struct ArcLayer {
        float radius;
        float thickness;
        float speed;
        float waveFrequency;
        float hueShift;
        float bassWeight;
        float midWeight;
        float highWeight;
        const ColorAdjust* adjust;
    };

    ArcLayer layers[] = {
        {0.30f, 0.070f, 1.4f, 3.2f, 0.05f, 1.0f, 0.3f, 0.2f, &legacyColorAdjust_.bassBars},
        {0.45f, 0.060f, 1.7f, 3.9f, 0.30f, 0.4f, 1.0f, 0.35f, &legacyColorAdjust_.midBars},
        {0.60f, 0.052f, 2.1f, 4.8f, 0.58f, 0.25f, 0.55f, 1.0f, &legacyColorAdjust_.highBars},
        {0.76f, 0.038f, 2.8f, 6.2f, 0.82f, 0.35f, 0.45f, 0.9f, &legacyColorAdjust_.highBars}
    };

    struct EdgeCoord {
        float innerX;
        float innerY;
        float outerX;
        float outerY;
    };

    for (const ArcLayer& layer : layers) {
        float weightSum = layer.bassWeight + layer.midWeight + layer.highWeight;
        float energyMix = weightSum > 0.0f
            ? std::clamp((bass * layer.bassWeight + mid * layer.midWeight + high * layer.highWeight) / weightSum, 0.0f, 1.0f)
            : 0.0f;

        float radius = minDimension * (layer.radius
                                       + energyMix * 0.15f
                                       + motionBlend * 0.08f
                                       + beatPulse * 0.06f);
        float thickness = minDimension * (layer.thickness + 0.14f * energyMix) * (0.55f + 0.75f * motionBlend);
        float highActivation = std::clamp(high * 1.6f + onsetPulse * 0.4f, 0.0f, 1.5f);
        float swirlBase = 0.18f + motionBlend * 0.65f + highActivation * 0.55f;
        float swirlPhase = animatedTime * swirlBase * calmScale;
        float spiralTightness = (0.25f + motionBlend * 1.4f + highActivation * 0.35f + layer.radius * 0.5f)
                                * (0.5f + 0.5f * calmScale);

        float moduleDensity = 0.5f + dynamicsScale * 0.6f;
        int modules = std::max(6, static_cast<int>((14 + energyMix * 18.0f + motionBlend * 10.0f) * moduleDensity));
        float moduleSpan = 1.0f / std::max(modules, 1);
        float fillRatio = 0.48f + 0.35f * std::clamp(energyMix, 0.0f, 1.0f);

        for (int module = 0; module < modules; ++module) {
            float moduleStart = module * moduleSpan;
            float moduleEnd = moduleStart + moduleSpan * fillRatio;
            float moduleMid = (moduleStart + moduleEnd) * 0.5f;
            float moduleSeed = hash(module * 7.91f + layer.radius * 17.0f + animatedTime * 0.27f);
            float swingRate = 0.8f + tempoFactor * 1.2f;
            float rhythmicSwing = std::sin(animatedTime * swingRate + module * 1.87f + moduleSeed * 12.0f);
            auto fractFunc = [](float x) {
                return x - std::floor(x);
            };
            auto stepFunc = [](float edge, float x) {
                return x >= edge ? 1.0f : 0.0f;
            };
            float chunkMask = stepFunc(0.18f, fractFunc(moduleSeed * 13.7f + beatPulse * 2.3f + highActivation * 0.6f));
            float jitter = std::sin(module * 2.4f + animatedTime * 3.2f + moduleSeed * 9.1f) * 0.6f;
            float modulePresence = highActivation * 0.6f
                                   + energyMix * 0.4f
                                   + beatPulse * 0.5f
                                   + rhythmicSwing * 0.45f * calmScale
                                   + jitter * 0.25f;
            modulePresence *= (0.4f + 0.8f * dynamicsScale) * chunkMask;
            modulePresence = std::clamp(modulePresence, 0.0f, 1.0f);
            if (modulePresence < 0.08f) {
                continue;
            }

            EdgeCoord edges[2];

            for (int edgeIndex = 0; edgeIndex < 2; ++edgeIndex) {
                float u = (edgeIndex == 0) ? moduleStart : moduleEnd;
                float baseAngle = u * 6.28318f + floor(moduleSeed * 12.0f) * 0.12f;
                float spiralOffset = spiralTightness *
                                     (layer.radius + u * 0.8f) *
                                     (1.0f + std::sin(animatedTime * 0.8f + u * 10.0f) * 0.4f);
                float angle = baseAngle + swirlPhase + spiralOffset;

                float glitch = sinf(animatedTime * 12.0f + module * 3.7f) * 0.15f * std::clamp(high, 0.0f, 1.0f);
                angle += glitch;

                float mechPulse = (moduleSeed - 0.5f) * 0.85f + std::sin(swirlPhase * 1.1f + module * 1.6f) * 0.35f;
                float melt = std::sin(baseAngle * (layer.waveFrequency * 1.45f) + animatedTime * 2.7f + moduleSeed * 13.0f)
                             * thickness * (0.45f + 0.55f * modulePresence * dynamicsScale);
                float drip = std::sin(animatedTime * 4.4f + moduleSeed * 11.8f + angle * 3.1f)
                             * thickness * 0.32f * modulePresence * calmScale;

                float flow = std::sin(baseAngle * 5.0f + animatedTime * 1.8f)
                             + std::sin(baseAngle * 9.0f - animatedTime * 2.1f)
                             + std::sin(baseAngle * 13.0f + animatedTime * 3.3f);
                flow *= thickness * (0.25f + energyMix * 0.4f);

                float innerRadius = radius + melt - thickness * (0.75f + mechPulse * 0.35f) + drip * 0.6f;
                float outerRadius = radius + melt + thickness * (0.85f + mechPulse * 0.45f + std::clamp(high, 0.0f, 1.0f) * 0.35f)
                                    + drip + std::sin(baseAngle * 12.0f + moduleSeed * 20.0f) * thickness * 0.2f;

                innerRadius += flow;
                outerRadius += flow;

                float cosA = std::cos(angle);
                float sinA = std::sin(angle);

                edges[edgeIndex].innerX = centerX + cosA * innerRadius * aspectX;
                edges[edgeIndex].innerY = centerY + sinA * innerRadius * aspectY;
                edges[edgeIndex].outerX = centerX + cosA * outerRadius * aspectX;
                edges[edgeIndex].outerY = centerY + sinA * outerRadius * aspectY;
            }

            float hue = std::fmod(layer.hueShift + moduleMid * 0.9f + moduleSeed * 0.28f + beatPulse * 0.25f, 1.0f);
            float saturation = std::clamp(0.55f + 0.42f * dynamicsScale + 0.35f * std::clamp(highActivation, 0.0f, 1.0f), 0.0f, 1.0f);
            float value = std::clamp(0.42f + 0.34f * motionBlend + 0.28f * globalEnergy + modulePresence * 0.32f, 0.0f, 1.0f);
            float baseR, baseG, baseB;
            hsvToRgb(hue, saturation, value, baseR, baseG, baseB);

            float glitchAccentR = 0.85f + modulePresence * 0.5f;
            float glitchAccentG = 0.25f + modulePresence * 0.45f;
            float glitchAccentB = 0.35f + modulePresence * 0.65f;

            glBegin(GL_TRIANGLE_STRIP);
            for (int edgeIndex = 0; edgeIndex < 2; ++edgeIndex) {
                const EdgeCoord& e = edges[edgeIndex];
                setColorWithAdjust(baseR * (0.65f + modulePresence * 0.6f) + glitchAccentR * 0.2f,
                                   baseG * (0.55f + modulePresence * 0.5f) + glitchAccentG * 0.25f,
                                   baseB * (0.75f + modulePresence * 0.7f) + glitchAccentB * 0.3f,
                                   (0.18f + 0.42f * energyMix) * modulePresence * (0.5f + 0.5f * calmScale),
                                   *layer.adjust);
                glVertex2f(e.innerX, e.innerY);

                float highBoost = std::clamp(high * 0.6f, 0.0f, 1.0f);
                float midBoost = std::clamp(mid * 0.6f, 0.0f, 1.0f);
                float bassBoost = std::clamp(bass * 0.6f, 0.0f, 1.0f);

                setColorWithAdjust(baseR * (1.25f + highBoost * 0.4f) + glitchAccentR * 0.4f,
                                   baseG * (1.10f + midBoost * 0.3f) + glitchAccentG * 0.35f,
                                   baseB * (1.35f + bassBoost * 0.25f) + glitchAccentB * 0.45f,
                                   (0.22f + 0.55f * (energyMix + beatPulse * 0.5f)) * modulePresence * (0.65f + 0.35f * calmScale),
                                   *layer.adjust);
                glVertex2f(e.outerX, e.outerY);
            }
            glEnd();

            glBegin(GL_LINES);
            for (int i = 0; i < 4; ++i) {
                float lerp = static_cast<float>(i) / 3.0f;
                float mixSeed = hash(moduleSeed * 97.0f + i * 1.9f + animatedTime * 0.7f);
                float baseAngleForCuts = moduleMid * 6.28318f + floor(moduleSeed * 12.0f) * 0.12f;
                float cutAngle = lerp * (moduleEnd - moduleStart) * 6.28318f + baseAngleForCuts + mixSeed * 0.4f;
                float cutRadius = radius + thickness * (mixSeed - 0.5f) * 0.6f;
                float glitchAlpha = (0.10f + 0.32f * energyMix) * (0.4f + mixSeed * 0.6f);
                setColorWithAdjust(glitchAccentR * (0.6f + mixSeed * 0.8f),
                                   glitchAccentG * (0.5f + mixSeed * 0.9f),
                                   glitchAccentB * (0.65f + mixSeed * 0.7f),
                                   glitchAlpha,
                                   *layer.adjust);
                float gx = centerX + std::cos(cutAngle) * cutRadius * aspectX;
                float gy = centerY + std::sin(cutAngle) * cutRadius * aspectY;
                glVertex2f(centerX + std::cos(cutAngle) * (cutRadius - thickness * 0.4f) * aspectX,
                           centerY + std::sin(cutAngle) * (cutRadius - thickness * 0.4f) * aspectY);
                glVertex2f(gx, gy);
            }
            glEnd();
        }

        // Removed spiral line overlays to keep arcs clean per user request
    }
}

void Visualizer::renderLegacySparkles(float bass, float high, float animatedTime, float motionBlend) {
    float centerX = windowWidth_ / 2.0f;
    float centerY = windowHeight_ / 2.0f;
    float minDimension = std::max(1.0f,
                                  std::min(static_cast<float>(windowWidth_),
                                           static_cast<float>(windowHeight_)));

    float highShare = std::clamp(audioFeatures_.highShare, 0.0f, 1.0f);
    float hiHat = std::clamp(audioFeatures_.hiHat, 0.0f, 1.0f);
    float clap = std::clamp(audioFeatures_.clap, 0.0f, 1.0f);

    float highExcitation = std::max({high * 0.9f, highShare * 1.2f, hiHat * 1.35f, clap * 0.8f});
    static float highBurst = 0.0f;
    highBurst = highBurst * 0.68f + highExcitation * 0.32f;
    float energy = std::clamp(high * 1.1f + highShare * 0.9f + highBurst * 0.7f, 0.0f, 1.5f);

    float atomCloudRadius = minDimension * (0.16f + energy * 0.22f + motionBlend * 0.08f);
    float atomShellRadius = atomCloudRadius * (0.7f + 0.3f * high);
    float orbitSpeed = 0.35f + 1.2f * motionBlend + high * 1.3f;
    float wobbleStrength = 0.08f + 0.35f * motionBlend;

    int innerAtoms = std::clamp(20 + static_cast<int>(highBurst * 24.0f) + static_cast<int>(motionBlend * 10.0f), 16, 48);
    int orbitAtoms = std::clamp(8 + static_cast<int>(highBurst * 18.0f), 8, 28);
    const float goldenAngle = 2.39996323f;

    auto fract = [](float x) {
        return x - std::floor(x);
    };

    auto hash = [&](float n) {
        return fract(std::sin(n) * 43758.5453f);
    };

    glPointSize(2.0f + 3.2f * std::clamp(highBurst, 0.0f, 1.0f));
    glBegin(GL_POINTS);

    for (int i = 0; i < innerAtoms; ++i) {
        float anchorAngle = i * goldenAngle + animatedTime * (0.45f + highBurst * 0.6f);
        float radialSeed = hash(i * 3.17f + animatedTime * 0.19f);
        float spraySeed = hash(i * 4.21f - animatedTime * 0.37f);
        float splashRadius = atomCloudRadius * (0.35f + 0.45f * highBurst) + minDimension * 0.02f * (radialSeed - 0.5f);
        float splashOffset = minDimension * 0.06f * highBurst * spraySeed;
        float bob = (spraySeed - 0.5f) * wobbleStrength * minDimension * 0.08f;
        float spreadAngle = anchorAngle + (spraySeed - 0.5f) * (0.6f + 0.4f * highBurst);

        float x = centerX + std::cos(spreadAngle) * (splashRadius + splashOffset);
        float y = centerY + std::sin(spreadAngle) * (splashRadius + splashOffset) + bob;

        float shimmer = std::clamp(highBurst * 1.4f + std::sin(animatedTime * 6.0f + anchorAngle * 1.8f) * 0.3f, 0.0f, 1.6f);
        setColorWithAdjust(0.55f + shimmer * 0.25f,
                           0.65f + shimmer * 0.45f,
                           1.0f + shimmer * 0.35f,
                           0.18f + 0.36f * highBurst,
                           legacyColorAdjust_.bloomInner);
        glVertex2f(x, y);
    }

    glEnd();

    // Sparkles remain as free-floating particles only.
}

void Visualizer::renderLegacyWaveform(float animatedTime, float motionBlend) {
    (void)animatedTime;
    (void)motionBlend;
    // Wormhole overlay deshabilitado: función vacía.
}
