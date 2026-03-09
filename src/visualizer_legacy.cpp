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

    // 4. Sparkles
    if (showLegacySparkles_) {
        float sparkleIntensity = std::min(1.0f, high + energy * 0.5f + onsetFlash * 0.4f);
        renderLegacySparkles(bass, sparkleIntensity, animatedTime, legacyMotionBlend_);
        renderLegacySparkles(bass * 0.7f + beatFlash * 0.6f,
                             std::min(1.0f, sparkleIntensity * 0.6f + beatFlash * 0.4f),
                             animatedTime + 0.77f,
                             legacyMotionBlend_ * 0.8f);
    }

    // 5. Floating orbs
    if (showLegacyOrbs_) {
        float baseRadius = minDimension * (0.26f + energy * 0.10f + beatFlash * 0.05f);
        float maxRadius = minDimension * (0.72f + legacyMotionBlend_ * 0.18f);
        float segmentLength = minDimension * (0.035f + 0.04f * legacyMotionBlend_);
        float expansionSpeed = 0.7f + energy * 0.8f + beatFlash * 0.6f;

        int beamCount = 48;
        glLineWidth(1.8f + 2.6f * std::clamp(high + legacyMotionBlend_ * 0.4f, 0.0f, 1.0f));

        auto fract = [](float x) {
            return x - std::floor(x);
        };

        auto hash = [&](float n) {
            return fract(std::sin(n) * 43758.5453f);
        };

        for (int i = 0; i < beamCount; ++i) {
            float laneSeed = hash(i * 8.37f + animatedTime * 0.27f);
            float drift = std::sin(animatedTime * (0.6f + laneSeed * 1.4f)) * minDimension * 0.012f;
            float laneAngle = (static_cast<float>(i) / beamCount) * 6.28318f
                              + animatedTime * (0.4f + laneSeed * 0.9f)
                              + drift;

            float laneOffset = std::sin(animatedTime * 1.2f + i * 0.45f) * minDimension * 0.02f;
            float startRadius = baseRadius + laneOffset;
            float endRadius = std::min(maxRadius,
                                       startRadius + segmentLength * (1.4f + laneSeed * 0.6f + beatFlash * 0.8f));

            setColorWithAdjust(0.38f + 0.6f * laneSeed + 0.3f * energy,
                               0.45f + 0.4f * mid + 0.15f * laneSeed,
                               0.88f + 0.4f * high,
                               0.28f + 0.5f * std::clamp(energy + beatFlash * 0.6f, 0.0f, 1.1f),
                               legacyColorAdjust_.orbit);
            glBegin(GL_LINES);
            glVertex2f(centerX + cosf(laneAngle) * startRadius,
                       centerY + sinf(laneAngle) * startRadius);
            glVertex2f(centerX + cosf(laneAngle) * endRadius,
                       centerY + sinf(laneAngle) * endRadius);
            glEnd();

            int capsuleCount = 5;
            for (int c = 0; c < capsuleCount; ++c) {
                float capsuleSeed = hash(i * 2.3f + c * 1.7f + animatedTime * 0.33f);
                float capsuleRadius = startRadius + (endRadius - startRadius) * (static_cast<float>(c) / capsuleCount);
                float capsuleWidth = minDimension * (0.0065f + capsuleSeed * 0.008f);
                float capsuleLength = minDimension * (0.012f + capsuleSeed * 0.03f + beatFlash * 0.015f);
                float capsuleAngle = laneAngle + std::sin(animatedTime * 2.0f + capsuleSeed * 6.28318f) * 0.12f;

                float cosA = std::cos(capsuleAngle);
                float sinA = std::sin(capsuleAngle);

                float sx = centerX + cosf(laneAngle) * (capsuleRadius - capsuleLength * 0.5f);
                float sy = centerY + sinf(laneAngle) * (capsuleRadius - capsuleLength * 0.5f);
                float ex = centerX + cosf(laneAngle) * (capsuleRadius + capsuleLength * 0.5f);
                float ey = centerY + sinf(laneAngle) * (capsuleRadius + capsuleLength * 0.5f);

                setColorWithAdjust(0.55f + 0.45f * laneSeed,
                                   0.6f + 0.35f * mid,
                                   1.1f + 0.3f * high,
                                   0.24f + 0.42f * std::clamp(energy + beatFlash * 0.4f, 0.0f, 1.0f),
                                   legacyColorAdjust_.orbit);
                glBegin(GL_TRIANGLE_STRIP);
                glVertex2f(sx + cosA * capsuleWidth, sy + sinA * capsuleWidth);
                glVertex2f(sx - cosA * capsuleWidth, sy - sinA * capsuleWidth);
                glVertex2f(ex + cosA * capsuleWidth, ey + sinA * capsuleWidth);
                glVertex2f(ex - cosA * capsuleWidth, ey - sinA * capsuleWidth);
                glEnd();
            }
        }

        int waveSegments = 160;
        glLineWidth(1.1f + 1.5f * energy);
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= waveSegments; ++i) {
            float t = static_cast<float>(i) / waveSegments;
            float angle = t * 6.28318f + animatedTime * expansionSpeed;
            float radiusSpan = (maxRadius - baseRadius) * t;
            float phase = animatedTime * (0.8f + energy * 0.6f) + t * 8.0f;
            float radialOffset = std::sin(phase) * minDimension * (0.01f + 0.02f * legacyMotionBlend_);

            float radius = baseRadius + radiusSpan + radialOffset;
            float px = centerX + std::cos(angle) * radius;
            float py = centerY + std::sin(angle) * radius;

            setColorWithAdjust(0.75f + 0.25f * energy,
                               0.7f + 0.2f * mid,
                               1.2f + 0.25f * high,
                               0.16f + 0.35f * std::clamp(energy + beatFlash * 0.5f, 0.0f, 1.0f),
                               legacyColorAdjust_.orbitTrail);
            glVertex2f(px, py);
        }
        glEnd();
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

    float baseRadius = minDimension * (0.016f
                                       + energyMix * 0.010f
                                       + motionBlend * 0.006f
                                       + beatPulse * 0.0035f
                                       + 0.0042f * std::clamp(harmonicSwell, 0.0f, 1.2f));
    float kickScale = 0.65f + 1.35f * std::clamp(kickPulse, 0.0f, 1.6f);
    float harmonicScale = 0.72f + 0.55f * std::clamp(harmonicPulse, 0.0f, 1.4f);
    float radius = baseRadius * kickScale * harmonicScale;
    radius = std::clamp(radius, minDimension * 0.009f, minDimension * 0.05f);

    int segments = 64;
    float ringAlpha = std::clamp(0.32f + 0.38f * energyMix + 0.25f * kickEnvelope + 0.35f * std::clamp(harmonicPulse, 0.0f, 1.1f),
                                 0.25f,
                                 1.2f);
    float wobble = sinf(animatedTime * (2.2f + high * 1.4f)) * radius * 0.12f
                   + kickEnvelope * radius * 0.08f * std::sin(animatedTime * 12.0f)
                   + harmonicPulse * radius * 0.09f * std::sin(animatedTime * 6.5f + 1.2f);

    float kickGlow = std::clamp(0.25f + 0.75f * kickEnvelope, 0.25f, 1.05f);
    float harmonicGlow = std::clamp(0.2f + 0.8f * harmonicPulse, 0.2f, 1.1f);
    float ringThickness = std::clamp(minDimension * (0.0035f + 0.0065f * energyMix + 0.0075f * harmonicPulse + 0.0045f * motionBlend),
                                     minDimension * 0.0025f,
                                     minDimension * 0.022f);

    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= segments; ++i) {
        float t = static_cast<float>(i) / segments;
        float angle = t * 6.28318f;
        float ripple = sinf(angle * 3.5f + animatedTime * 3.0f) * radius * 0.08f
                       + harmonicPulse * radius * 0.05f * std::sin(angle * 6.0f + animatedTime * 2.4f);
        float currentRadius = radius + wobble + ripple;
        float innerRadius = std::max(currentRadius - ringThickness * 0.55f, minDimension * 0.005f);
        float outerRadius = innerRadius + ringThickness;

        float cosA = cosf(angle);
        float sinA = sinf(angle);

        setColorWithAdjust(0.35f + 0.45f * energyMix + kickGlow * 0.22f + harmonicGlow * 0.28f,
                           0.32f + 0.38f * mid + kickGlow * 0.12f + harmonicGlow * 0.33f,
                           0.55f + 0.52f * high + kickGlow * 0.15f + harmonicGlow * 0.24f,
                           ringAlpha * 0.6f,
                           legacyColorAdjust_.circleFill);
        glVertex2f(centerX + cosA * innerRadius,
                   centerY + sinA * innerRadius);

        setColorWithAdjust(0.45f + 0.55f * energyMix + kickGlow * 0.3f + harmonicGlow * 0.36f,
                           0.4f + 0.45f * mid + kickGlow * 0.16f + harmonicGlow * 0.4f,
                           0.68f + 0.6f * high + kickGlow * 0.25f + harmonicGlow * 0.32f,
                           ringAlpha,
                           legacyColorAdjust_.circleFill);
        glVertex2f(centerX + cosA * outerRadius,
                   centerY + sinA * outerRadius);
    }
    glEnd();

    glLineWidth(1.6f + 1.2f * std::clamp(harmonicPulse, 0.0f, 1.2f));
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i <= segments; ++i) {
        float angle = (static_cast<float>(i) / segments) * 6.28318f;
        float outlineRadius = radius + wobble
                              + harmonicPulse * minDimension * 0.0045f * std::sin(angle * 2.5f + animatedTime * 1.6f)
                              + kickEnvelope * minDimension * 0.0035f * std::sin(angle * 4.0f + animatedTime * 2.5f);
        setColorWithAdjust(0.85f,
                           0.88f,
                           1.25f + 0.18f * high + harmonicGlow * 0.35f,
                           0.28f + 0.35f * energyMix + 0.22f * kickEnvelope + 0.3f * harmonicPulse,
                           legacyColorAdjust_.circleOutline);
        glVertex2f(centerX + cosf(angle) * outlineRadius,
                   centerY + sinf(angle) * outlineRadius);
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

    float baseRadius = minDimension * (0.34f + 0.15f * mid + 0.12f * motionBlend);
    float ringSpacing = minDimension * (0.072f + 0.032f * motionBlend);
    float glitchStrength = 0.038f + 0.12f * motionBlend;

    int baseRingCount = 3;
    int extraRings = static_cast<int>(std::floor(mid * 3.0f + motionBlend * 2.0f));
    int numRings = std::min(12, baseRingCount + extraRings);

    const float goldenAngle = 2.39996323f;
    auto fract = [](float x) {
        return x - std::floor(x);
    };

    auto hash = [&](float n) {
        return fract(std::sin(n) * 43758.5453f);
    };

    for (int ring = 0; ring < numRings; ++ring) {
        float radius = baseRadius + ring * ringSpacing;
        radius += minDimension * 0.018f * std::clamp(mid, 0.0f, 1.0f) * (ring * 0.35f);
        float rotation = animatedTime * (0.9f + ring * 0.22f * motionBlend) + mid * 1.8f * motionBlend;
        float ringEnergy = std::clamp(mid * (0.6f + ring * 0.12f) + motionBlend * 0.4f, 0.0f, 1.4f);
        float alpha = 0.1f + 0.45f * ringEnergy * (1.0f - ring / std::max(1, numRings - 1));
        float lineWidth = 1.0f + 1.6f * (1.0f - ring / std::max(1, numRings - 1)) * (0.35f + 0.65f * motionBlend);

        setColorWithAdjust(0.72f + 0.28f * ringEnergy,
                           0.75f + 0.20f * mid,
                           0.95f + 0.25f * ringEnergy,
                           alpha,
                           legacyColorAdjust_.rings);
        glLineWidth(lineWidth);
        glBegin(GL_LINE_LOOP);

        int segments = 140;
        for (int i = 0; i <= segments; ++i) {
            float t = static_cast<float>(i) / segments;
            float angle = t * 6.28318f + rotation;

            float jitterSeed = hash(ring * 5.73f + i * 2.19f + animatedTime * 0.47f);
            float smoothNoise = std::sin(angle * (2.6f + ringEnergy * 1.3f) + animatedTime * (1.8f + ring * 0.3f));
            float glitchPulse = std::sin(angle * 12.0f + animatedTime * 9.0f + jitterSeed * 6.28318f);
            glitchPulse = std::sin(glitchPulse * 0.5f) * std::exp(-std::fabs(glitchPulse) * 0.6f);

            float warp = smoothNoise * glitchStrength * minDimension * (0.10f + 0.35f * ringEnergy);
            float microRipple = std::sin(angle * 40.0f + animatedTime * 20.0f) * glitchStrength * minDimension * 0.03f;
            float randomPush = (jitterSeed - 0.5f) * glitchStrength * minDimension * (0.08f + 0.12f * motionBlend);

            float glitchOffset = glitchPulse * glitchStrength * minDimension * (0.12f + 0.28f * motionBlend);
            float currentRadius = radius + warp + microRipple + randomPush + glitchOffset;

            float x = centerX + std::cos(angle) * currentRadius;
            float y = centerY + std::sin(angle) * currentRadius;
            setColorWithAdjust(0.72f + 0.28f * ringEnergy,
                               0.75f + 0.20f * mid,
                               0.95f + 0.25f * ringEnergy,
                               alpha,
                               legacyColorAdjust_.rings);
            glVertex2f(x, y);
        }

        glEnd();

        glBegin(GL_LINE_LOOP);
        for (int i = 0; i <= segments; ++i) {
            float t = static_cast<float>(i) / segments;
            float angle = t * 6.28318f + rotation;
            float jitterSeed = hash(i * goldenAngle + ring * 1.37f + animatedTime * 0.31f);
            float haloRadius = radius + minDimension * (0.014f + 0.045f * ringEnergy)
                               + std::sin(angle * 6.0f + animatedTime * 5.2f + jitterSeed * 4.0f)
                                 * minDimension * 0.015f * motionBlend;

            float x = centerX + std::cos(angle) * haloRadius;
            float y = centerY + std::sin(angle) * haloRadius;
            setColorWithAdjust(0.9f + 0.2f * ringEnergy,
                               0.85f + 0.22f * mid,
                               1.2f,
                               0.08f + 0.3f * motionBlend,
                               legacyColorAdjust_.rings);
            glVertex2f(x, y);
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
                           legacyColorAdjust_.sparkles);
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

            setColorWithAdjust(r, g, b, alpha, legacyColorAdjust_.orbit);
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
