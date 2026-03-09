#include "visualizer.h"
#include <cmath>
#include <algorithm>
#include <iostream>

void Visualizer::renderLegacyVisualization() {
    // Use legacy OpenGL 1.1 for maximum compatibility
    glUseProgram(0);  // Ensure no shader program is active
    
    // Setup projection using legacy matrix operations
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, windowWidth_, windowHeight_, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Disable depth test for 2D rendering
    glDisable(GL_DEPTH_TEST);
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

    // Ambient background (solid black to satisfy request)
    glBegin(GL_QUADS);
    glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(windowWidth_, 0.0f);
    glVertex2f(windowWidth_, windowHeight_);
    glVertex2f(0.0f, windowHeight_);
    glEnd();

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
        renderLegacyCircle(bass, mid, high, legacyMotionBlend_, animatedTime);

        float secondaryBoost = std::min(1.0f, bass * 0.5f + onsetFlash * 0.6f + beatFlash * 0.8f);
        glPushMatrix();
        renderLegacyCircle(secondaryBoost,
                           std::min(1.0f, mid * 0.6f + onsetFlash * 0.4f),
                           std::min(1.0f, high * 0.6f + energy * 0.3f),
                           legacyMotionBlend_, animatedTime + 1.37f);
        glPopMatrix();
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
    
    // Restore state
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

void Visualizer::renderLegacyCircle(float bass, float mid, float high, float motionBlend, float animatedTime) {
    float centerX = windowWidth_ / 2.0f;
    float centerY = windowHeight_ / 2.0f;
    float minDimension = static_cast<float>(std::min(windowWidth_, windowHeight_));

    float energyMix = std::clamp(0.48f * bass + 0.34f * mid + 0.24f * high, 0.0f, 1.3f);
    float globalEnergy = std::clamp(audioFeatures_.energy, 0.0f, 1.5f);
    float beatPulse = std::clamp(audioFeatures_.beat, 0.0f, 1.0f);
    float onsetPulse = std::clamp(audioFeatures_.onset, 0.0f, 1.0f);

    float baseRadius = minDimension * (0.045f + energyMix * 0.035f + motionBlend * 0.02f + beatPulse * 0.018f);
    float breathing = sinf(animatedTime * (1.9f + energyMix * 1.2f)) * minDimension * (0.004f + 0.012f * energyMix);
    float resonance = cosf(animatedTime * (1.3f + high * 2.8f)) * minDimension * (0.003f + 0.008f * motionBlend);
    float radius = std::max(baseRadius + breathing + resonance, minDimension * 0.02f);

    int segments = 72;
    float coreAlpha = 0.6f + 0.25f * std::clamp(energyMix, 0.0f, 1.0f);

    glBegin(GL_TRIANGLE_FAN);
    setColorWithAdjust(0.35f + 0.55f * energyMix,
                       0.25f + 0.60f * mid,
                       0.45f + 0.65f * high,
                       coreAlpha,
                       legacyColorAdjust_.circleFill);
    glVertex2f(centerX, centerY);

    for (int i = 0; i <= segments; ++i) {
        float t = static_cast<float>(i) / segments;
        float angle = t * 6.28318f;
        float turbulence = sinf(angle * (5.0f + high * 2.8f) + animatedTime * (2.2f + energyMix * 1.2f))
                           * minDimension * (0.005f + 0.012f * high + 0.01f * energyMix);
        float swirl = cosf(animatedTime * (1.0f + motionBlend * 0.7f) + angle * (1.9f + energyMix * 0.6f))
                      * minDimension * 0.009f * motionBlend;
        float currentRadius = radius + turbulence + swirl;

        float glow = 0.55f + 0.45f * std::sin(angle + animatedTime * 0.9f);
        setColorWithAdjust(0.45f + glow * 0.55f * energyMix,
                           0.35f + glow * 0.45f * mid,
                           0.55f + glow * 0.60f * high,
                           coreAlpha,
                           legacyColorAdjust_.circleFill);

        glVertex2f(centerX + cosf(angle) * currentRadius,
                   centerY + sinf(angle) * currentRadius);
    }
    glEnd();

    float auraInner = radius * (1.25f + 0.18f * energyMix);
    float auraOuter = auraInner + minDimension * (0.028f + 0.05f * globalEnergy + 0.03f * beatPulse);

    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= segments; ++i) {
        float t = static_cast<float>(i) / segments;
        float angle = t * 6.28318f;
        float shimmer = 1.0f + 0.32f * sinf(animatedTime * 3.6f + angle * 3.1f);
        float innerRadius = auraInner + shimmer * minDimension * (0.006f + 0.015f * energyMix);
        float outerRadius = auraOuter + shimmer * minDimension * (0.010f + 0.024f * motionBlend);

        setColorWithAdjust(0.55f + 0.45f * energyMix,
                           0.45f + 0.50f * mid,
                           0.75f + 0.35f * high,
                           0.48f + 0.32f * energyMix,
                           legacyColorAdjust_.circleOutline);
        glVertex2f(centerX + cosf(angle) * innerRadius,
                   centerY + sinf(angle) * innerRadius);

        setColorWithAdjust(0.1f + 0.15f * energyMix,
                           0.15f + 0.2f * mid,
                           0.2f + 0.25f * high,
                           0.08f + 0.25f * (motionBlend + onsetPulse * 0.6f),
                           legacyColorAdjust_.circleOutline);
        glVertex2f(centerX + cosf(angle) * outerRadius,
                   centerY + sinf(angle) * outerRadius);
    }
    glEnd();

    int filamentCount = 10;
    float filamentWidth = minDimension * (0.006f + 0.010f * std::clamp(high + motionBlend * 0.6f, 0.0f, 1.0f));
    float filamentLength = minDimension * (0.035f + 0.07f * energyMix + 0.05f * beatPulse);

    for (int i = 0; i < filamentCount; ++i) {
        float t = static_cast<float>(i) / filamentCount;
        float baseAngle = animatedTime * (0.8f + energyMix * 0.6f) + t * 6.28318f;
        float tipRadius = auraOuter + filamentLength * (0.7f + 0.4f * std::sin(baseAngle * 2.3f + animatedTime));

        float angledPulse = 0.6f + 0.35f * sinf(baseAngle * 3.1f + animatedTime * 2.1f);
        float width = filamentWidth * angledPulse;

        glBegin(GL_TRIANGLE_STRIP);
        setColorWithAdjust(0.8f + beatPulse * 0.4f,
                           0.6f + energyMix * 0.5f,
                           1.1f + high * 0.5f,
                           0.22f + 0.55f * beatPulse,
                           legacyColorAdjust_.beatExplosion);
        glVertex2f(centerX + cosf(baseAngle) * auraOuter,
                   centerY + sinf(baseAngle) * auraOuter);
        glVertex2f(centerX + cosf(baseAngle) * auraOuter,
                   centerY + sinf(baseAngle) * auraOuter);

        setColorWithAdjust(0.9f + energyMix * 0.4f,
                           0.7f + high * 0.6f,
                           1.4f,
                           0.20f + 0.60f * beatPulse,
                           legacyColorAdjust_.beatExplosion);
        glVertex2f(centerX + cosf(baseAngle + 1.5708f) * width + cosf(baseAngle) * tipRadius,
                   centerY + sinf(baseAngle + 1.5708f) * width + sinf(baseAngle) * tipRadius);
        glVertex2f(centerX + cosf(baseAngle - 1.5708f) * width + cosf(baseAngle) * tipRadius,
                   centerY + sinf(baseAngle - 1.5708f) * width + sinf(baseAngle) * tipRadius);
        glEnd();
    }

    glPointSize(1.5f + 3.0f * std::clamp(globalEnergy, 0.0f, 1.0f));
    glBegin(GL_POINTS);
    for (int i = 0; i < 40; ++i) {
        float t = static_cast<float>(i) / 40.0f;
        float angle = t * 6.28318f + animatedTime * (1.2f + energyMix);
        float shell = auraOuter * (0.65f + 0.25f * sinf(angle * 5.3f + animatedTime * 3.0f));
        float expansion = minDimension * (0.02f + 0.05f * beatPulse + 0.04f * onsetPulse);
        float radiusRing = shell + expansion * std::sin(angle * 2.0f + animatedTime * 1.7f);

        setColorWithAdjust(0.75f + energyMix * 0.5f,
                           0.55f + mid * 0.6f,
                           1.15f + high * 0.4f,
                           0.18f + 0.45f * std::clamp(energyMix + beatPulse, 0.0f, 1.2f),
                           legacyColorAdjust_.circleOutline);
        glVertex2f(centerX + cosf(angle) * radiusRing,
                   centerY + sinf(angle) * radiusRing);
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
        float swirlSpeed = 0.35f + motionBlend * 0.55f + highActivation * 0.65f;
        float swirlPhase = animatedTime * swirlSpeed;
        float spiralTightness = 0.55f + motionBlend * 1.05f + highActivation * 0.35f + layer.radius * 0.35f;

        int modules = 18 + static_cast<int>(energyMix * 14.0f + motionBlend * 8.0f);
        float moduleSpan = 1.0f / std::max(modules, 1);
        float fillRatio = 0.62f + 0.25f * std::clamp(energyMix, 0.0f, 1.0f);

        for (int module = 0; module < modules; ++module) {
            float moduleStart = module * moduleSpan;
            float moduleEnd = moduleStart + moduleSpan * fillRatio;
            float moduleMid = (moduleStart + moduleEnd) * 0.5f;
            float moduleSeed = hash(module * 3.91f + layer.radius * 11.0f + animatedTime * 0.17f);
            float modulePresence = std::clamp(highActivation * 0.6f
                                              + std::sin(animatedTime * 1.35f + module * 1.17f + moduleSeed * 6.28318f) * 0.5f,
                                              0.0f,
                                              1.0f);
            if (modulePresence < 0.08f) {
                continue;
            }

            EdgeCoord edges[2];

            for (int edgeIndex = 0; edgeIndex < 2; ++edgeIndex) {
                float u = (edgeIndex == 0) ? moduleStart : moduleEnd;
                float baseAngle = u * 6.28318f;
                float spiralOffset = spiralTightness * (layer.radius + u * 0.45f + moduleSeed * 0.2f);
                float angle = baseAngle + swirlPhase + spiralOffset;

                float mechPulse = (moduleSeed - 0.5f) * 0.7f + std::sin(swirlPhase * 0.9f + module * 0.8f) * 0.35f;
                float melt = std::sin(baseAngle * (layer.waveFrequency * 0.9f) + animatedTime * 2.2f + moduleSeed * 8.0f)
                             * thickness * (0.25f + 0.45f * modulePresence);
                float drip = std::sin(animatedTime * 3.1f + moduleSeed * 9.0f + angle * 1.6f)
                             * thickness * 0.28f * modulePresence;

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
            float saturation = std::clamp(0.55f + 0.35f * energyMix + 0.28f * std::clamp(highActivation, 0.0f, 1.0f), 0.0f, 1.0f);
            float value = std::clamp(0.58f + 0.32f * motionBlend + 0.25f * globalEnergy + modulePresence * 0.25f, 0.0f, 1.0f);
            float baseR, baseG, baseB;
            hsvToRgb(hue, saturation, value, baseR, baseG, baseB);

            glBegin(GL_TRIANGLE_STRIP);
            for (int edgeIndex = 0; edgeIndex < 2; ++edgeIndex) {
                const EdgeCoord& e = edges[edgeIndex];
                setColorWithAdjust(baseR * 0.85f,
                                   baseG * 0.85f,
                                   baseB * 0.95f,
                                   (0.24f + 0.42f * energyMix) * modulePresence,
                                   *layer.adjust);
                glVertex2f(e.innerX, e.innerY);

                setColorWithAdjust(baseR * 1.12f,
                                   baseG * 1.05f,
                                   baseB * 1.25f,
                                   (0.20f + 0.48f * (energyMix + beatPulse * 0.5f)) * modulePresence,
                                   *layer.adjust);
                glVertex2f(e.outerX, e.outerY);
            }
            glEnd();

            glBegin(GL_LINE_STRIP);
            setColorWithAdjust(baseR * 0.9f,
                               baseG * 0.9f,
                               baseB * 1.15f,
                               (0.18f + 0.32f * energyMix) * modulePresence,
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
                               (0.16f + 0.3f * energyMix) * modulePresence,
                               *layer.adjust);
            glVertex2f(braceMidX, braceMidY);
            glVertex2f(braceMidX2, braceMidY2);
            glEnd();
        }

        int swirlSegments = 220;
        glLineWidth(1.0f + 1.4f * std::clamp(energyMix + highActivation * 0.3f, 0.0f, 1.2f));
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= swirlSegments; ++i) {
            float t = static_cast<float>(i) / swirlSegments;
            float angle = t * 6.28318f * (0.8f + 0.25f * layer.radius) + swirlPhase * 0.3f;
            float spiral = spiralTightness * 0.28f * t;
            float finalAngle = angle + spiral;
            float vortexRadius = radius * 0.35f + thickness * 1.5f * t
                                 + std::sin(swirlPhase * 0.9f + t * 12.0f) * thickness * 0.25f
                                 + onsetPulse * thickness * 0.18f * std::sin(t * 20.0f + animatedTime * 2.6f);

            float px = centerX + std::cos(finalAngle) * vortexRadius * aspectX;
            float py = centerY + std::sin(finalAngle) * vortexRadius * aspectY;

            float hue = std::fmod(layer.hueShift + t * 0.9f + beatPulse * 0.12f, 1.0f);
            float cr, cg, cb;
            hsvToRgb(hue,
                     std::clamp(0.65f + 0.3f * energyMix + 0.15f * motionBlend, 0.0f, 1.0f),
                     std::clamp(0.55f + 0.38f * motionBlend + 0.2f * globalEnergy, 0.0f, 1.0f),
                     cr, cg, cb);

            setColorWithAdjust(cr,
                               cg,
                               cb,
                               0.18f + 0.32f * (energyMix + t * 0.6f),
                               *layer.adjust);
            glVertex2f(px, py);
        }
        glEnd();
    }
}

void Visualizer::renderLegacyRings(float mid, float animatedTime, float motionBlend) {
    if (motionBlend <= 0.01f) return;

    float centerX = windowWidth_ / 2.0f;
    float centerY = windowHeight_ / 2.0f;
    float minDimension = static_cast<float>(std::min(windowWidth_, windowHeight_));

    float baseRadius = minDimension * (0.28f + 0.12f * mid + 0.1f * motionBlend);
    float ringSpacing = minDimension * (0.055f + 0.025f * motionBlend);
    float glitchStrength = 0.035f + 0.12f * motionBlend;

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

    float energy = std::clamp(bass * 1.2f + high * 0.6f, 0.0f, 1.5f);
    float atomCloudRadius = minDimension * (0.18f + energy * 0.28f + motionBlend * 0.12f);
    float atomShellRadius = atomCloudRadius * (0.6f + 0.4f * bass);
    float orbitSpeed = 0.4f + 1.8f * motionBlend + bass * 1.6f;
    float wobbleStrength = 0.12f + 0.55f * motionBlend;

    int innerAtoms = 64;
    int orbitAtoms = 48;
    const float goldenAngle = 2.39996323f;

    auto fract = [](float x) {
        return x - std::floor(x);
    };

    auto hash = [&](float n) {
        return fract(std::sin(n) * 43758.5453f);
    };

    glPointSize(2.5f + 5.0f * std::clamp(energy, 0.0f, 1.0f));
    glBegin(GL_POINTS);

    for (int i = 0; i < innerAtoms; ++i) {
        float rndAngle = i * goldenAngle + animatedTime * (0.8f + motionBlend * 0.6f);
        float radialSeed = hash(i * 3.17f + animatedTime * 0.23f);
        float randRadius = atomCloudRadius * (0.15f + 0.85f * std::sqrt(radialSeed));
        float verticalSeed = hash(i * 5.71f - animatedTime * 0.41f);
        float bob = (verticalSeed - 0.5f) * wobbleStrength * minDimension * 0.12f;
        float swirl = std::sin(rndAngle * 1.7f + animatedTime * (0.5f + high * 2.0f))
                      * atomCloudRadius * 0.12f * motionBlend;

        float x = centerX + std::cos(rndAngle) * randRadius + swirl;
        float y = centerY + std::sin(rndAngle) * randRadius + bob;

        float bassGlow = std::clamp(bass * 1.6f + std::sin(animatedTime * 4.0f + rndAngle) * 0.25f, 0.1f, 1.6f);
        float highSpark = std::clamp(high * 1.4f + std::cos(animatedTime * 5.5f + rndAngle * 1.3f) * 0.35f, 0.0f, 1.5f);
        setColorWithAdjust(0.6f + bassGlow * 0.4f,
                           0.5f + highSpark * 0.35f,
                           0.9f + highSpark * 0.45f,
                           0.25f + 0.45f * energy,
                           legacyColorAdjust_.sparkles);
        glVertex2f(x, y);
    }

    glEnd();

    glLineWidth(1.0f + 2.5f * energy);
    glBegin(GL_LINE_LOOP);
    int shellSegments = 72;
    for (int i = 0; i <= shellSegments; ++i) {
        float t = static_cast<float>(i) / shellSegments;
        float baseAngle = t * 6.28318f + animatedTime * orbitSpeed;
        float radialPulse = sinf(baseAngle * 2.5f + animatedTime * 3.7f) * atomShellRadius * 0.08f * bass;
        float x = centerX + cosf(baseAngle) * (atomShellRadius + radialPulse);
        float y = centerY + sinf(baseAngle) * (atomShellRadius + radialPulse);
        setColorWithAdjust(0.75f + bass * 0.4f,
                           0.6f + high * 0.5f,
                           1.1f + high * 0.35f,
                           0.15f + 0.35f * energy,
                           legacyColorAdjust_.sparkles);
        glVertex2f(x, y);
    }
    glEnd();

    glPointSize(3.0f + 6.0f * std::clamp(bass, 0.0f, 1.0f));
    glBegin(GL_POINTS);
    for (int i = 0; i < orbitAtoms; ++i) {
        float angle = i * goldenAngle + animatedTime * (orbitSpeed * 1.3f);
        float jitterSeed = hash(i * 2.31f + animatedTime * 0.63f);
        float radialJitter = std::sin(angle * 3.0f + animatedTime * 2.5f + jitterSeed * 6.28318f)
                             * atomShellRadius * (0.18f + 0.12f * motionBlend);
        float beatPush = bass * 0.5f + std::max(0.0f, audioFeatures_.beat) * 0.8f;
        float radius = atomShellRadius + radialJitter + beatPush * minDimension * 0.06f;

        float trailOffset = std::cos(animatedTime * 1.6f + jitterSeed * 12.0f) * radius * 0.14f;
        float x = centerX + std::cos(angle) * (radius + trailOffset);
        float y = centerY + std::sin(angle) * (radius - trailOffset);

        float alpha = 0.35f + 0.55f * std::clamp(high + bass * 0.7f, 0.0f, 1.2f);
        setColorWithAdjust(0.8f + bass * 0.4f,
                           0.7f + high * 0.5f,
                           1.4f,
                           alpha,
                           legacyColorAdjust_.sparkles);
        glVertex2f(x, y);
    }
    glEnd();
}

void Visualizer::renderLegacyWaveform(float animatedTime, float motionBlend) {
    if (waveformBuffer_.empty() || motionBlend <= 0.01f) return;

    float centerX = windowWidth_ / 2.0f;
    float centerY = windowHeight_ / 2.0f;
    float minDimension = std::min(windowWidth_, windowHeight_);

    size_t sampleCount = waveformBuffer_.size();

    auto drawLaserRing = [&](float baseRadius,
                             float beamLengthBase,
                             float beamLengthScale,
                             float beamWidth,
                             float rotationSpeed,
                             float phaseOffset,
                             float ringR, float ringG, float ringB,
                             float beamR, float beamG, float beamB) {
        float rotation = animatedTime * rotationSpeed + phaseOffset;
        size_t sampleOffset = static_cast<size_t>(phaseOffset * sampleCount) % sampleCount;
        int beamCount = 96;
        float ringAlpha = 0.22f + 0.45f * motionBlend;

        glLineWidth(1.6f + 2.2f * motionBlend);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i <= beamCount; ++i) {
            float angle = 2.0f * 3.14159f * i / beamCount;
            float x = centerX + cosf(angle) * baseRadius;
            float y = centerY + sinf(angle) * baseRadius;
            setColorWithAdjust(ringR, ringG, ringB, ringAlpha, legacyColorAdjust_.waveform);
            glVertex2f(x, y);
        }
        glEnd();

        for (int i = 0; i < beamCount; ++i) {
            float t = static_cast<float>(i) / beamCount;
            float angle = rotation + 2.0f * 3.14159f * t;
            size_t sampleIndex = (sampleOffset + static_cast<size_t>(t * sampleCount)) % sampleCount;
            float sample = waveformBuffer_[sampleIndex];
            float magnitude = std::fabs(sample) * motionBlend;

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

            float alphaInner = 0.18f + 0.42f * magnitude;
            float alphaOuter = 0.32f + 0.55f * magnitude;

            glBegin(GL_TRIANGLE_STRIP);
            setColorWithAdjust(beamR, beamG, beamB, alphaInner, legacyColorAdjust_.waveform);
            glVertex2f(innerX1, innerY1);
            glVertex2f(innerX2, innerY2);
            setColorWithAdjust(beamR, beamG, beamB, alphaOuter, legacyColorAdjust_.waveform);
            glVertex2f(outerX1, outerY1);
            glVertex2f(outerX2, outerY2);
            glEnd();

            if (magnitude > 0.05f) {
                glPointSize(2.5f + 5.5f * magnitude);
                glBegin(GL_POINTS);
                setColorWithAdjust(beamR + 0.2f, beamG + 0.2f, beamB + 0.2f, alphaOuter, legacyColorAdjust_.waveform);
                glVertex2f(centerX + cosA * (endRadius + beamWidth * 0.3f),
                           centerY + sinA * (endRadius + beamWidth * 0.3f));
                glEnd();
            }
        }
    };

    drawLaserRing(minDimension * 0.62f,   // base radius
                  minDimension * 0.12f,   // base beam length
                  minDimension * 0.2f,    // beam length scale
                  minDimension * 0.012f,  // beam width
                  0.32f,                  // rotation speed
                  0.0f,                   // phase offset
                  0.35f, 0.85f, 1.1f,     // ring color
                  0.95f, 1.25f, 1.6f);    // beam color

    drawLaserRing(minDimension * 0.48f,
                  minDimension * 0.09f,
                  minDimension * 0.16f,
                  minDimension * 0.010f,
                  -0.54f,
                  0.22f,
                  1.15f, 0.55f, 1.35f,
                  1.35f, 0.6f, 1.55f);

    drawLaserRing(minDimension * 0.34f,
                  minDimension * 0.07f,
                  minDimension * 0.12f,
                  minDimension * 0.008f,
                  0.68f,
                  0.51f,
                  1.15f, 0.82f, 0.4f,
                  1.3f, 0.95f, 0.45f);
}
