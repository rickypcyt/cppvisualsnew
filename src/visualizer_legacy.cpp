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

    // 3. Rotating rings
    if (showLegacyRings_) {
        renderLegacyRings(mid, animatedTime, legacyMotionBlend_);
    }

    if (depthWasEnabled) {
        glEnable(GL_DEPTH_TEST);
    }

    // 6. Waveform portals
    if (showLegacyWaveforms_) {
        renderLegacyWaveform(animatedTime, legacyMotionBlend_);
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

    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= segments; ++i) {
        float t = static_cast<float>(i) / segments;
        float angleBase = t * 6.28318f;
        float jitterSeed = hash(i * 7.31f + glitchPhase * 0.71f);
        float jitterAngle = (jitterSeed - 0.5f) * (0.22f + glitchBurst * 0.55f);
        float angle = angleBase + jitterAngle;

        float ripple = sinf(angle * (3.5f + glitchBurst * 1.8f) + animatedTime * (3.0f + glitchBurst * 1.7f)) * radius * (0.14f + 0.05f * glitchBurst)
                       + harmonicPulse * radius * 0.07f * std::sin(angle * (6.0f + glitchBurst * 2.5f) + animatedTime * 2.8f)
                       + (jitterSeed - 0.5f) * minDimension * (0.012f + glitchBurst * 0.02f);

        float glitchStep = std::fmod(glitchPhase * 4.0f + i * 0.6f, 1.0f);
        float angularTear = std::sin(glitchStep * 6.28318f + jitterSeed * 12.0f) * minDimension * (0.01f + glitchBurst * 0.018f);

        float currentRadius = radius + wobble + ripple + angularTear;
        float innerRadius = std::max(currentRadius - ringThickness * (0.45f + 0.25f * jitterSeed), minDimension * 0.008f);
        float outerRadius = innerRadius + ringThickness * (0.9f + 0.35f * jitterSeed + 0.25f * glitchBurst);

        float cosA = cosf(angle);
        float sinA = sinf(angle);

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
                           ringAlpha * 0.6f,
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
                           ringAlpha,
                           legacyColorAdjust_.circleFill);
        glVertex2f(centerX + cosA * outerRadius,
                   centerY + sinA * outerRadius);
    }
    glEnd();

    glLineWidth(2.2f + 1.8f * std::clamp(harmonicPulse + glitchBurst, 0.0f, 1.6f));
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i <= segments; ++i) {
        float t = static_cast<float>(i) / segments;
        float angleBase = t * 6.28318f;
        float intensitySeed = hash(i * 9.19f + glitchPhase * 0.92f);
        float angle = angleBase + (intensitySeed - 0.5f) * (0.18f + glitchBurst * 0.42f);
        float outlineRadius = radius + wobble
                              + harmonicPulse * minDimension * (0.0075f + 0.003f * intensitySeed) * std::sin(angle * (2.5f + 1.1f * glitchBurst) + animatedTime * 1.9f)
                              + kickEnvelope * minDimension * (0.0065f + 0.0025f * intensitySeed) * std::sin(angle * (4.3f + 1.6f * glitchBurst) + animatedTime * 2.8f)
                              + (intensitySeed - 0.5f) * minDimension * (0.016f + glitchBurst * 0.026f);
        setColorWithAdjust(0.92f + 0.25f * intensitySeed,
                           0.95f + 0.18f * intensitySeed,
                           1.35f + 0.28f * high + harmonicGlow * (0.45f + 0.18f * intensitySeed),
                           0.32f + 0.38f * energyMix + 0.26f * kickEnvelope + 0.38f * harmonicPulse + 0.32f * glitchBurst,
                           legacyColorAdjust_.circleOutline);
        glVertex2f(centerX + cosf(angle) * outlineRadius,
                   centerY + sinf(angle) * outlineRadius);
    }
    glEnd();

    int shardCount = 14 + static_cast<int>(glitchBurst * 12.0f + energyMix * 10.0f);
    glLineWidth(1.0f + glitchBurst * 1.4f);
    glBegin(GL_LINES);
    for (int s = 0; s < shardCount; ++s) {
        float shardSeed = hash(s * 5.63f + glitchPhase * 1.13f);
        float shardAngle = shardSeed * 6.28318f;
        float shardLength = minDimension * (0.028f + shardSeed * 0.045f + glitchBurst * 0.038f);
        float shardOffset = radius * (0.65f + shardSeed * 0.55f);
        float jitter = (hash(shardSeed * 19.17f + glitchPhase) - 0.5f) * minDimension * (0.016f + glitchBurst * 0.022f);

        float sx = centerX + cosf(shardAngle) * (shardOffset + jitter);
        float sy = centerY + sinf(shardAngle) * (shardOffset + jitter);
        float ex = centerX + cosf(shardAngle) * (shardOffset + shardLength + jitter * 0.5f)
                   - sinf(shardAngle) * minDimension * (0.01f + glitchBurst * 0.015f);
        float ey = centerY + sinf(shardAngle) * (shardOffset + shardLength + jitter * 0.5f)
                   + cosf(shardAngle) * minDimension * (0.01f + glitchBurst * 0.015f);

        setColorWithAdjust(0.85f + 0.28f * shardSeed + glitchBurst * 0.22f,
                           0.38f + 0.55f * shardSeed + 0.25f * glitchBurst,
                           1.25f + 0.45f * shardSeed + 0.3f * glitchBurst,
                           0.25f + 0.5f * glitchBurst,
                           legacyColorAdjust_.circleOutline);
        glVertex2f(sx, sy);
        glVertex2f(ex, ey);
    }
    glEnd();
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
                                       + energyMix * 0.10f
                                       + motionBlend * 0.06f
                                       + beatPulse * 0.04f);
        float thickness = minDimension * (layer.thickness + 0.11f * energyMix) * (0.7f + 0.55f * motionBlend);
        float highActivation = std::clamp(high * 1.6f + onsetPulse * 0.4f, 0.0f, 1.5f);
        float swirlBase = 0.28f + motionBlend * 0.45f + highActivation * 0.4f;
        float swirlPhase = animatedTime * swirlBase * calmScale;
        float spiralTightness = (0.45f + motionBlend * 0.9f + highActivation * 0.28f + layer.radius * 0.35f)
                                * (0.6f + 0.4f * calmScale);

        float moduleDensity = 0.35f + dynamicsScale * 0.5f;
        int modules = std::max(5, static_cast<int>((10 + energyMix * 10.0f + motionBlend * 6.0f) * moduleDensity));
        float moduleSpan = 1.0f / std::max(modules, 1);
        float fillRatio = 0.62f + 0.25f * std::clamp(energyMix, 0.0f, 1.0f);

        for (int module = 0; module < modules; ++module) {
            float moduleStart = module * moduleSpan;
            float moduleEnd = moduleStart + moduleSpan * fillRatio;
            float moduleMid = (moduleStart + moduleEnd) * 0.5f;
            float moduleSeed = hash(module * 3.91f + layer.radius * 11.0f + animatedTime * 0.17f);
            float swingRate = 0.6f + tempoFactor * 0.9f;
            float rhythmicSwing = std::sin(animatedTime * swingRate + module * 1.17f + moduleSeed * 6.28318f);
            float modulePresence = highActivation * 0.42f
                                   + energyMix * 0.35f
                                   + beatPulse * 0.3f
                                   + rhythmicSwing * 0.3f * calmScale;
            modulePresence *= (0.35f + 0.65f * dynamicsScale);
            modulePresence = std::clamp(modulePresence, 0.0f, 1.0f);
            if (modulePresence < 0.08f) {
                continue;
            }

            EdgeCoord edges[2];

            for (int edgeIndex = 0; edgeIndex < 2; ++edgeIndex) {
                float u = (edgeIndex == 0) ? moduleStart : moduleEnd;
                float baseAngle = u * 6.28318f;
                float spiralOffset = spiralTightness * (layer.radius + u * 0.45f + moduleSeed * 0.2f);
                float angle = baseAngle + swirlPhase + spiralOffset;

                float mechPulse = (moduleSeed - 0.5f) * 0.55f + std::sin(swirlPhase * 0.7f + module * 0.8f) * 0.28f;
                float melt = std::sin(baseAngle * (layer.waveFrequency * 0.85f) + animatedTime * 1.9f + moduleSeed * 8.0f)
                             * thickness * (0.20f + 0.35f * modulePresence * dynamicsScale);
                float drip = std::sin(animatedTime * 2.4f + moduleSeed * 7.8f + angle * 1.4f)
                             * thickness * 0.22f * modulePresence * calmScale;

                float innerRadius = radius + melt - thickness * (0.55f + mechPulse * 0.25f) + drip * 0.4f;
                float outerRadius = radius + melt + thickness * (0.60f + mechPulse * 0.35f + std::clamp(high, 0.0f, 1.0f) * 0.25f)
                                    + drip;

                float cosA = std::cos(angle);
                float sinA = std::sin(angle);

                edges[edgeIndex].innerX = centerX + cosA * innerRadius * aspectX;
                edges[edgeIndex].innerY = centerY + sinA * innerRadius * aspectY;
                edges[edgeIndex].outerX = centerX + cosA * outerRadius * aspectX;
                edges[edgeIndex].outerY = centerY + sinA * outerRadius * aspectY;
            }

            float hue = std::fmod(layer.hueShift + moduleMid * 0.6f + moduleSeed * 0.08f + beatPulse * 0.1f, 1.0f);
            float saturation = std::clamp(0.45f + 0.28f * dynamicsScale + 0.22f * std::clamp(highActivation, 0.0f, 1.0f), 0.0f, 1.0f);
            float value = std::clamp(0.52f + 0.24f * motionBlend + 0.20f * globalEnergy + modulePresence * 0.20f, 0.0f, 1.0f);
            float baseR, baseG, baseB;
            hsvToRgb(hue, saturation, value, baseR, baseG, baseB);

            glBegin(GL_TRIANGLE_STRIP);
            for (int edgeIndex = 0; edgeIndex < 2; ++edgeIndex) {
                const EdgeCoord& e = edges[edgeIndex];
                setColorWithAdjust(baseR * 0.85f,
                                   baseG * 0.85f,
                                   baseB * 0.95f,
                                   (0.24f + 0.38f * energyMix) * modulePresence * (0.7f + 0.3f * calmScale),
                                   *layer.adjust);
                glVertex2f(e.innerX, e.innerY);

                setColorWithAdjust(baseR * 1.12f,
                                   baseG * 1.05f,
                                   baseB * 1.25f,
                                   (0.18f + 0.42f * (energyMix + beatPulse * 0.5f)) * modulePresence * (0.75f + 0.25f * calmScale),
                                   *layer.adjust);
                glVertex2f(e.outerX, e.outerY);
            }
            glEnd();

            glBegin(GL_LINE_STRIP);
            setColorWithAdjust(baseR * 0.9f,
                               baseG * 0.9f,
                               baseB * 1.15f,
                               (0.16f + 0.28f * energyMix) * modulePresence * (0.7f + 0.3f * calmScale),
                               *layer.adjust);
            glVertex2f(edges[0].innerX, edges[0].innerY);
            glVertex2f(edges[0].outerX, edges[0].outerY);
            glVertex2f(edges[1].outerX, edges[1].outerY);
            glVertex2f(edges[1].innerX, edges[1].innerY);
            glVertex2f(edges[0].innerX, edges[0].innerY);
            glEnd();

            glBegin(GL_LINES);
            float braceMidX = (edges[0].innerX + edges[0].outerX) * 0.5f;
            float braceMidY = (edges[0].innerY + edges[0].outerY) * 0.5f;
            float braceMidX2 = (edges[1].innerX + edges[1].outerX) * 0.5f;
            float braceMidY2 = (edges[1].innerY + edges[1].outerY) * 0.5f;
            setColorWithAdjust(baseR * 1.1f,
                               baseG * 1.1f,
                               baseB * 1.2f,
                               (0.14f + 0.26f * energyMix) * modulePresence * (0.65f + 0.35f * calmScale),
                               *layer.adjust);
            glVertex2f(braceMidX, braceMidY);
            glVertex2f(braceMidX2, braceMidY2);
            glEnd();
        }

        // Removed spiral line overlays to keep arcs clean per user request
    }
}

void Visualizer::renderLegacyRings(float mid, float animatedTime, float motionBlend) {
    if (motionBlend <= 0.01f) return;

    float centerX = windowWidth_ / 2.0f;
    float centerY = windowHeight_ / 2.0f;
    float minDimension = static_cast<float>(std::min(windowWidth_, windowHeight_));

    float beat = std::clamp(audioFeatures_.beat, 0.0f, 1.2f);
    float onset = std::clamp(audioFeatures_.onset, 0.0f, 1.2f);
    float clap = std::clamp(audioFeatures_.clap, 0.0f, 1.2f);
    float hiHat = std::clamp(audioFeatures_.hiHat, 0.0f, 1.2f);
    float spectral = std::clamp(mid * 0.9f + beat * 0.55f + onset * 0.5f + clap * 0.35f + hiHat * 0.32f,
                                0.0f,
                                2.8f);
    float tempo = (audioFeatures_.bpm > 0.01f)
        ? std::clamp((audioFeatures_.bpm - 65.0f) / 95.0f, 0.0f, 1.3f)
        : spectral * 0.35f;
    float veil = std::clamp(audioFeatures_.energy * 0.35f + mid * 0.35f + motionBlend * 0.5f, 0.0f, 2.0f);

    float baseRadius = minDimension * (0.24f + 0.19f * motionBlend + 0.22f * spectral + 0.06f * tempo);
    float layerSpacing = minDimension * (0.055f + 0.028f * spectral + 0.02f * motionBlend);
    int layers = std::clamp(4 + static_cast<int>(std::floor(spectral * 3.6f + motionBlend * 4.6f)), 4, 11);

    auto fract = [](float x) {
        return x - std::floor(x);
    };

    auto hash = [&](float n) {
        return fract(std::sin(n) * 43758.5453123f);
    };

    for (int layer = 0; layer < layers; ++layer) {
        float layerSeed = hash(layer * 7.19f + animatedTime * 0.17f);
        float radius = baseRadius + layerSpacing * layer;
        float swing = std::sin(animatedTime * (0.95f + layer * 0.24f) + layerSeed * 6.28318f);
        radius *= 1.0f + swing * (0.16f + 0.11f * motionBlend);
        radius += minDimension * (0.02f + 0.018f * spectral * (layer / std::max(1, layers - 1)))
                  + minDimension * 0.012f * layerSeed;

        float layerEnergy = std::clamp(spectral * (0.55f + layer * 0.08f) + motionBlend * 0.35f, 0.1f, 3.0f);
        float layerTempo = tempo * (0.8f + layer * 0.06f);
        float swirl = animatedTime * (0.75f + layerTempo * 1.5f) + layerSeed * 6.28318f;

        float thickness = minDimension * (0.02f + 0.013f * layerEnergy + 0.01f * motionBlend);
        int arcCount = 6 + layer + static_cast<int>(layerEnergy * 3.2f);
        int samples = 24;

        for (int arc = 0; arc < arcCount; ++arc) {
            float arcSeed = hash(layer * 11.53f + arc * 5.37f + animatedTime * 0.23f);
            float arcCenter = swirl + arcSeed * 6.28318f;
            float extent = (0.17f + 0.28f * arcSeed + 0.24f * layerEnergy) * (0.65f + 0.52f * tempo);
            float arcThickness = thickness * (0.95f + 0.6f * arcSeed + 0.45f * layerEnergy);

            glBegin(GL_TRIANGLE_STRIP);
            for (int i = 0; i <= samples; ++i) {
                float u = static_cast<float>(i) / samples;
                float offset = (u - 0.5f) * extent * 6.28318f;
                float angle = arcCenter + offset;

                float modulation = std::sin(angle * (2.6f + layerEnergy) + animatedTime * (2.5f + tempo * 1.4f) + arcSeed * 5.3f);
                float glitch = std::sin(angle * (12.0f + layer * 2.1f) + animatedTime * (7.0f + onset * 3.2f) + arcSeed * 11.0f);
                glitch = std::sin(glitch * 0.6f) * (0.6f + 0.4f * veil);

                float radialJitter = modulation * minDimension * (0.014f + 0.02f * layerEnergy)
                                     + glitch * minDimension * (0.012f + 0.018f * onset);
                float inner = radius + radialJitter - arcThickness * (0.55f + 0.25f * arcSeed);
                float outer = inner + arcThickness;

                float cosA = std::cos(angle);
                float sinA = std::sin(angle);

                float pulse = std::clamp(0.32f + layerEnergy * 0.42f + modulation * 0.25f + glitch * 0.2f,
                                         0.12f,
                                         1.6f);
                float hueShift = arcSeed * 0.35f;

                setColorWithAdjust(0.52f + 0.5f * pulse + 0.2f * hueShift,
                                   0.45f + 0.38f * mid + 0.2f * pulse,
                                   0.9f + 0.58f * pulse + 0.3f * hueShift,
                                   0.3f + 0.55f * pulse,
                                   legacyColorAdjust_.rings);
                glVertex2f(centerX + cosA * inner, centerY + sinA * inner);

                setColorWithAdjust(0.66f + 0.62f * pulse + 0.3f * hueShift,
                                   0.52f + 0.42f * mid + 0.26f * pulse,
                                   1.08f + 0.72f * pulse + 0.35f * hueShift,
                                   0.38f + 0.68f * pulse,
                                   legacyColorAdjust_.rings);
                glVertex2f(centerX + cosA * outer, centerY + sinA * outer);
            }
            glEnd();
        }

        int threadCount = 8 + static_cast<int>(layerEnergy * 7.0f);
        glLineWidth(1.1f + 0.9f * layerEnergy);
        glBegin(GL_LINES);
        for (int t = 0; t < threadCount; ++t) {
            float threadSeed = hash(layer * 23.31f + t * 2.17f + animatedTime * 0.41f);
            float angle = swirl + threadSeed * 6.28318f;
            float sweep = std::sin(animatedTime * (4.4f + tempo * 1.5f) + threadSeed * 8.0f);
            float startRadius = radius * (0.8f + 0.28f * threadSeed) - minDimension * 0.016f;
            float endRadius = startRadius + minDimension * (0.085f + 0.12f * layerEnergy + 0.05f * sweep);
            float cosA = std::cos(angle);
            float sinA = std::sin(angle);

            float alpha = std::clamp(0.2f + layerEnergy * 0.3f + sweep * 0.22f, 0.12f, 1.0f);
            setColorWithAdjust(0.78f + 0.22f * threadSeed,
                               0.64f + 0.36f * mid,
                               1.18f + 0.4f * threadSeed,
                               alpha,
                               legacyColorAdjust_.rings);
            glVertex2f(centerX + cosA * startRadius, centerY + sinA * startRadius);

            setColorWithAdjust(0.96f + 0.3f * threadSeed,
                               0.74f + 0.4f * mid,
                               1.35f + 0.42f * threadSeed,
                               alpha * (0.85f + 0.3f * sweep),
                               legacyColorAdjust_.rings);
            glVertex2f(centerX + cosA * endRadius, centerY + sinA * endRadius);
        }
        glEnd();
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

    // Sparkles remain as free-floating particles only (no extra rings)

    auto activationRamp = [](float level, float threshold, float softness) {
        float normalized = (level - threshold) / softness;
        return std::clamp(normalized, 0.0f, 1.0f);
    };

    float highTrigger = std::max({activationRamp(highBurst, 0.18f, 0.18f), activationRamp(highShare, 0.28f, 0.22f)});

    if (highTrigger > 0.05f) {
        static float smoothedTrigger = 0.0f;
        smoothedTrigger = smoothedTrigger * 0.78f + highTrigger * 0.22f;

        float laserStrength = std::clamp(smoothedTrigger * (0.8f + motionBlend * 0.4f), 0.0f, 1.0f);
        int laserBeams = 12 + static_cast<int>(20 * laserStrength + 16 * highBurst);

        glLineWidth(1.2f + 1.8f * laserStrength);
        glBegin(GL_LINES);
        for (int i = 0; i < laserBeams; ++i) {
            float t = (static_cast<float>(i) / std::max(1, laserBeams - 1));
            float angle = t * 6.28318f + animatedTime * (1.6f + 0.8f * motionBlend);
            float offsetSeed = hash(i * 4.73f + animatedTime * 0.41f);
            float direction = (i % 2 == 0) ? 1.0f : -1.0f;

            float baseRadius = atomShellRadius * (0.9f + offsetSeed * 0.4f);
            float beamLength = minDimension * (0.04f + 0.07f * laserStrength + 0.03f * offsetSeed);
            float startRadius = baseRadius + direction * offsetSeed * minDimension * 0.012f;
            float endRadius = startRadius + beamLength * direction;

            float sx = centerX + std::cos(angle) * startRadius;
            float sy = centerY + std::sin(angle) * startRadius;
            float ex = centerX + std::cos(angle) * endRadius;
            float ey = centerY + std::sin(angle) * endRadius;

            float colorPhase = std::fmod(t + animatedTime * 0.2f, 1.0f);
            float r = 0.55f + 0.4f * laserStrength + 0.18f * std::sin(colorPhase * 6.28318f + high * 2.8f);
            float g = 0.65f + 0.35f * laserStrength + 0.22f * std::sin(colorPhase * 6.28318f + high * 2.4f + 1.2f);
            float b = 0.95f + 0.35f * laserStrength;
            float alpha = 0.18f + 0.38f * laserStrength;

            setColorWithAdjust(r, g, b, alpha, legacyColorAdjust_.bloomOuter);
            glVertex2f(sx, sy);
            glVertex2f(ex, ey);
        }
        glEnd();
    }
}

void Visualizer::renderLegacyWaveform(float animatedTime, float motionBlend) {
    if (waveformBuffer_.empty() || motionBlend <= 0.01f) return;

    float centerX = windowWidth_ / 2.0f;
    float centerY = windowHeight_ / 2.0f;
    float minDimension = std::min(windowWidth_, windowHeight_);

    size_t sampleCount = waveformBuffer_.size();

    auto activationRamp = [](float share, float threshold, float softness) {
        float normalized = (share - threshold) / softness;
        return std::clamp(normalized, 0.0f, 1.0f);
    };

    float bassPresence = activationRamp(audioFeatures_.bassShare, 0.15f, 0.12f);
    float midPresence = activationRamp(audioFeatures_.midShare, 0.20f, 0.15f);
    float highPresence = activationRamp(audioFeatures_.highShare, 0.22f, 0.18f);

    float rawPresence = std::max(std::max(bassPresence, midPresence), highPresence);
    static float smoothedPresence = 0.0f;
    smoothedPresence = smoothedPresence * 0.82f + rawPresence * 0.18f;

    if (smoothedPresence < 0.03f) {
        return;
    }

    float presenceFactor = std::clamp(smoothedPresence * (0.6f + motionBlend * 0.5f), 0.0f, 1.0f);
    float bassFade = std::clamp(presenceFactor * (0.6f + bassPresence * 0.8f), 0.0f, 1.0f);
    float midFade = std::clamp(presenceFactor * (0.55f + midPresence * 0.9f), 0.0f, 1.0f);
    float highFade = std::clamp(presenceFactor * (0.5f + highPresence * 0.7f), 0.0f, 1.0f);

    float backgroundTint = 0.45f + 0.35f * presenceFactor;

    auto drawLaserRing = [&](float baseRadius,
                             float beamLengthBase,
                             float beamLengthScale,
                             float beamWidth,
                             float rotationSpeed,
                             float phaseOffset,
                             float ringR, float ringG, float ringB,
                             float beamR, float beamG, float beamB,
                             float direction,
                             float depthFade,
                             bool invertWaveform) {
        float rotation = animatedTime * rotationSpeed * direction + phaseOffset;
        size_t sampleOffset = static_cast<size_t>(std::fabs(phaseOffset) * sampleCount) % sampleCount;
        int beamCount = 96;
        float ringAlpha = (0.08f + 0.28f * motionBlend) * depthFade;

        glLineWidth((1.2f + 2.0f * motionBlend) * depthFade);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i <= beamCount; ++i) {
            float angle = 2.0f * 3.14159f * i / beamCount;
            float x = centerX + cosf(angle) * baseRadius;
            float y = centerY + sinf(angle) * baseRadius;
            setColorWithAdjust(ringR * backgroundTint,
                               ringG * backgroundTint,
                               ringB * backgroundTint,
                               ringAlpha,
                               legacyColorAdjust_.waveform);
            glVertex2f(x, y);
        }
        glEnd();

        for (int i = 0; i < beamCount; ++i) {
            float t = static_cast<float>(i) / beamCount;
            float angle = rotation + direction * 2.0f * 3.14159f * t;
            float sampleT = invertWaveform ? (1.0f - t) : t;
            if (sampleT >= 1.0f) sampleT -= 1.0f;
            size_t sampleIndex = (sampleOffset + static_cast<size_t>(sampleT * sampleCount)) % sampleCount;
            float sample = waveformBuffer_[sampleIndex];
            float magnitude = std::fabs(sample) * motionBlend * (0.6f + depthFade * 0.6f);

            float beamLength = beamLengthBase + magnitude * beamLengthScale;
            float halfWidth = beamWidth * (0.45f + 0.65f * motionBlend);
            float startRadius = baseRadius - beamWidth * 0.25f;
            float endRadius = startRadius + beamLength;

            float cosA = cosf(angle);
            float sinA = sinf(angle);
            float tx = -sinA;
            float ty = cosA;

            float innerX1 = centerX + cosA * startRadius + tx * halfWidth;
            float innerY1 = centerY + sinA * startRadius + ty * halfWidth;
            float innerX2 = centerX + cosA * startRadius - tx * halfWidth;
            float innerY2 = centerY + sinA * startRadius - ty * halfWidth;
            float outerX1 = centerX + cosA * endRadius + tx * halfWidth * 0.7f;
            float outerY1 = centerY + sinA * endRadius + ty * halfWidth * 0.7f;
            float outerX2 = centerX + cosA * endRadius - tx * halfWidth * 0.7f;
            float outerY2 = centerY + sinA * endRadius - ty * halfWidth * 0.7f;

            float alphaInner = (0.10f + 0.28f * magnitude) * depthFade;
            float alphaOuter = (0.16f + 0.42f * magnitude) * depthFade;

            glBegin(GL_TRIANGLE_STRIP);
            setColorWithAdjust(beamR * backgroundTint,
                               beamG * backgroundTint,
                               beamB * backgroundTint,
                               alphaInner,
                               legacyColorAdjust_.waveform);
            glVertex2f(innerX1, innerY1);
            glVertex2f(innerX2, innerY2);
            setColorWithAdjust(beamR * (backgroundTint + 0.15f * depthFade),
                               beamG * (backgroundTint + 0.15f * depthFade),
                               beamB * (backgroundTint + 0.15f * depthFade),
                               alphaOuter,
                               legacyColorAdjust_.waveform);
            glVertex2f(outerX1, outerY1);
            glVertex2f(outerX2, outerY2);
            glEnd();

            if (magnitude > 0.05f) {
                glPointSize((2.0f + 5.0f * magnitude) * depthFade);
                glBegin(GL_POINTS);
                setColorWithAdjust((beamR + 0.15f) * (0.5f + 0.5f * depthFade),
                                   (beamG + 0.15f) * (0.5f + 0.5f * depthFade),
                                   (beamB + 0.15f) * (0.5f + 0.5f * depthFade),
                                   alphaOuter,
                                   legacyColorAdjust_.waveform);
                glVertex2f(centerX + cosA * (endRadius + beamWidth * 0.3f),
                           centerY + sinA * (endRadius + beamWidth * 0.3f));
                glEnd();
            }
        }
    };

    drawLaserRing(minDimension * 0.82f,
                  minDimension * 0.16f,
                  minDimension * 0.24f,
                  minDimension * 0.014f,
                  -0.26f,
                  -0.18f,
                  0.28f, 0.6f, 1.0f,
                  0.65f, 1.15f, 1.35f,
                  -1.0f,
                  0.45f * std::max(highFade, presenceFactor * 0.5f),
                  true);

    drawLaserRing(minDimension * 0.62f,   // base radius
                  minDimension * 0.12f,   // base beam length
                  minDimension * 0.20f,   // beam length scale
                  minDimension * 0.012f,  // beam width
                  0.32f,                  // rotation speed
                  0.0f,                   // phase offset
                  0.35f, 0.85f, 1.1f,     // ring color
                  0.95f, 1.25f, 1.6f,     // beam color
                  1.0f,                   // direction
                  0.75f * bassFade,       // depth fade
                  false);                 // waveform direction

    drawLaserRing(minDimension * 0.48f,
                  minDimension * 0.09f,
                  minDimension * 0.16f,
                  minDimension * 0.010f,
                  0.54f,
                  0.22f,
                  1.15f, 0.55f, 1.35f,
                  1.35f, 0.6f, 1.55f,
                  -1.0f,
                  0.6f * midFade,
                  true);

    drawLaserRing(minDimension * 0.34f,
                  minDimension * 0.07f,
                  minDimension * 0.12f,
                  minDimension * 0.008f,
                  0.68f,
                  0.51f,
                  1.15f, 0.82f, 0.4f,
                  1.3f, 0.95f, 0.45f,
                  1.0f,
                  0.5f * highFade,
                  false);

    drawLaserRing(minDimension * 0.24f,
                  minDimension * 0.05f,
                  minDimension * 0.10f,
                  minDimension * 0.0065f,
                  -0.78f,
                  0.37f,
                  0.55f, 0.95f, 0.85f,
                  0.9f, 1.2f, 0.95f,
                  -1.0f,
                  0.42f * std::max(bassFade, presenceFactor * 0.35f),
                  false);
}
