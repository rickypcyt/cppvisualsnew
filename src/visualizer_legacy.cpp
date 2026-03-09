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
    
    // DEBUG: Print audio features
    static int debugCounter = 0;
    debugCounter++;
    if (debugCounter % 60 == 0) {  // Print every 60 frames
        std::cout << "DEBUG: Audio Features - Bass: " << audioFeatures_.bassEnergy 
                  << " Mid: " << audioFeatures_.midEnergy 
                  << " High: " << audioFeatures_.highEnergy 
                  << " Beat: " << audioFeatures_.beat << std::endl;
    }
    
    // Calculate smoothed audio features
    static float smoothBass = 0.0f, smoothMid = 0.0f, smoothHigh = 0.0f;
    static float energyHistory[64] = {0.0f};
    static int energyIndex = 0;
    const float smoothingFactor = 0.82f;
    
    smoothBass = smoothBass * smoothingFactor + audioFeatures_.bassEnergy * (1.0f - smoothingFactor);
    smoothMid = smoothMid * smoothingFactor + audioFeatures_.midEnergy * (1.0f - smoothingFactor);
    smoothHigh = smoothHigh * smoothingFactor + audioFeatures_.highEnergy * (1.0f - smoothingFactor);

    // Track recent energy to drive ambient animations
    float combinedEnergy = (audioFeatures_.bassEnergy + audioFeatures_.midEnergy + audioFeatures_.highEnergy) / 3.0f;
    energyHistory[energyIndex] = combinedEnergy;
    energyIndex = (energyIndex + 1) % 64;
    float averageEnergy = 0.0f;
    for (float value : energyHistory) {
        averageEnergy += value;
    }
    averageEnergy /= 64.0f;
    
    const float silenceThreshold = 0.0025f;
    float energyForMotion = std::max(audioFeatures_.energy, combinedEnergy);
    float targetMotionBlend = energyForMotion > silenceThreshold ? 1.0f : 0.0f;
    legacyMotionBlend_ = legacyMotionBlend_ * 0.88f + targetMotionBlend * 0.12f;
    legacyMotionBlend_ = std::clamp(legacyMotionBlend_, 0.0f, 1.0f);

    if (energyForMotion > silenceThreshold) {
        legacyMotionPhase_ += deltaTime_;
    }

    float animatedTime = legacyMotionPhase_;

    // Add some base animation even without audio (will freeze when motion blend -> 0)
    float timeAnimation = sinf(animatedTime) * 0.5f + 0.5f;  // 0 to 1
    float globalPulse = 0.5f + 0.5f * sinf(animatedTime * 0.7f);
    
    // Use actual audio data if available, otherwise use animation
    float bassValue = audioFeatures_.bassEnergy > 0.01f ? smoothBass : timeAnimation;
    float midValue = audioFeatures_.midEnergy > 0.01f ? smoothMid : timeAnimation * 0.8f;
    float highValue = audioFeatures_.highEnergy > 0.01f ? smoothHigh : timeAnimation * 0.6f;
    
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

    float bloomRadius = minDimension * (0.68f + averageEnergy * 0.25f + globalPulse * 0.12f);
    glBegin(GL_TRIANGLE_FAN);
    setColorWithAdjust(0.05f + bassValue * 0.25f,
                       0.07f + midValue * 0.22f,
                       0.15f + highValue * 0.35f,
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
        renderLegacyCircle(bassValue, midValue, highValue, legacyMotionBlend_, animatedTime);

        glPushMatrix();
        renderLegacyCircle(std::min(1.0f, bassValue * 0.6f + 0.2f + globalPulse * 0.2f),
                           std::min(1.0f, midValue * 0.6f + 0.1f),
                           std::min(1.0f, highValue * 0.6f + averageEnergy * 0.3f),
                           legacyMotionBlend_, animatedTime + 1.37f);
        glPopMatrix();
    }

    // 2. Frequency arcs
    if (showLegacyArcs_) {
        renderLegacyFrequencyBars(bassValue, midValue, highValue, legacyMotionBlend_, animatedTime);
    }

    // 3. Rotating rings
    if (showLegacyRings_) {
        renderLegacyRings(midValue, animatedTime, legacyMotionBlend_);
    }

    // 4. Sparkles
    if (showLegacySparkles_) {
        renderLegacySparkles(highValue, animatedTime, legacyMotionBlend_);
        renderLegacySparkles(std::min(1.0f, highValue * 0.5f + averageEnergy * 0.4f), animatedTime + 0.77f, legacyMotionBlend_ * 0.8f);
    }

    // 5. Floating orbs
    if (showLegacyOrbs_) {
        float ringRadius = minDimension * (0.34f + averageEnergy * 0.08f);
        float ringThickness = minDimension * (0.05f + bassValue * 0.06f);
        int ringSegments = 96;

        glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= ringSegments; ++i) {
            float t = static_cast<float>(i) / ringSegments;
            float angle = t * 6.28318f;
            float wave = sinf(angle * 4.0f + animatedTime * 1.6f * legacyMotionBlend_) * 0.3f * midValue * legacyMotionBlend_;
            float innerRadius = ringRadius - ringThickness * (0.5f + 0.25f * wave);
            float outerRadius = ringRadius + ringThickness * (0.5f + 0.25f * wave);

            setColorWithAdjust(0.45f + highValue * 0.4f,
                               0.2f + midValue * 0.5f,
                               1.0f,
                               0.35f + 0.4f * legacyMotionBlend_,
                               legacyColorAdjust_.orbit);
            glVertex2f(centerX + cosf(angle) * innerRadius,
                       centerY + sinf(angle) * innerRadius);
            setColorWithAdjust(0.45f + highValue * 0.5f,
                               0.2f + midValue * 0.6f,
                               1.0f,
                               0.22f + 0.35f * legacyMotionBlend_,
                               legacyColorAdjust_.orbit);
            glVertex2f(centerX + cosf(angle) * outerRadius,
                       centerY + sinf(angle) * outerRadius);
        }
        glEnd();

        int pulseCount = 8;
        float pulseWidth = 0.18f;
        float pulseInner = ringRadius - ringThickness * 0.7f;
        float pulseOuter = ringRadius + ringThickness * 0.7f;
        float pulseRotation = animatedTime * 0.9f * legacyMotionBlend_;
        for (int i = 0; i < pulseCount; ++i) {
            float baseAngle = pulseRotation + i * (6.28318f / pulseCount);
            float bias = (float)i / pulseCount;
            float energyMix = 0.5f * bassValue + 0.3f * midValue + 0.2f * highValue;
            float alpha = 0.18f + 0.5f * energyMix * legacyMotionBlend_;
            setColorWithAdjust(0.8f, 0.9f, 1.4f, alpha, legacyColorAdjust_.orbitTrail);

            glBegin(GL_TRIANGLE_STRIP);
            for (int step = 0; step <= 1; ++step) {
                float angle = baseAngle + (step ? pulseWidth : -pulseWidth) * 0.5f;
                glVertex2f(centerX + cosf(angle) * pulseInner,
                           centerY + sinf(angle) * pulseInner);
                glVertex2f(centerX + cosf(angle) * pulseOuter,
                           centerY + sinf(angle) * pulseOuter);
            }
            glEnd();
        }
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
    float minDimension = std::min(windowWidth_, windowHeight_);

    float baseRadius = minDimension * 0.24f;
    float pulse = bass * minDimension * 0.18f * (0.25f + 0.75f * motionBlend);
    float wobble = sinf(animatedTime * 1.7f) * minDimension * 0.025f * motionBlend;
    float radius = baseRadius + pulse + wobble;

    float hue = std::fmod(animatedTime * 0.1f + bass * 0.3f + mid * 0.2f, 1.0f);
    float colorR = std::abs(std::sin(hue * 6.28318f)) * 0.8f + 0.2f;
    float colorG = std::abs(std::sin((hue + 0.33f) * 6.28318f)) * 0.8f + 0.2f;
    float colorB = std::abs(std::sin((hue + 0.66f) * 6.28318f)) * 0.8f + 0.2f;

    glBegin(GL_TRIANGLE_FAN);
    setColorWithAdjust(colorR, colorG, colorB, 0.75f, legacyColorAdjust_.circleFill);
    glVertex2f(centerX, centerY);
    int segments = 64;
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * 3.14159f * i / segments;
        float audioWarp = 1.0f + high * 0.25f * sinf(animatedTime * 5.0f + i * 0.4f * motionBlend);
        float x = centerX + cosf(angle) * radius * audioWarp;
        float y = centerY + sinf(angle) * radius * audioWarp;
        glVertex2f(x, y);
    }
    glEnd();

    float haloThickness = minDimension * (0.05f + mid * 0.06f);
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * 3.14159f * i / segments;
        float shimmer = 1.0f + high * 0.25f * sinf(animatedTime * 4.8f + i * 0.6f * motionBlend);
        float innerRadius = radius * shimmer;
        float outerRadius = innerRadius + haloThickness;

        setColorWithAdjust(colorR, colorG, colorB, 0.45f, legacyColorAdjust_.circleOutline);
        glVertex2f(centerX + cosf(angle) * innerRadius,
                   centerY + sinf(angle) * innerRadius);
        setColorWithAdjust(0.2f, 0.2f, 0.2f, 0.05f, legacyColorAdjust_.circleOutline);
        glVertex2f(centerX + cosf(angle) * outerRadius,
                   centerY + sinf(angle) * outerRadius);
    }
    glEnd();

    glLineWidth(4.0f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * 3.14159f * i / segments;
        float shimmer = 1.0f + mid * 0.2f * sinf(animatedTime * 3.0f + i * 0.9f * motionBlend);
        float x = centerX + cosf(angle) * (radius + 12.0f) * shimmer;
        float y = centerY + sinf(angle) * (radius + 12.0f) * shimmer;
        setColorWithAdjust(1.0f, 1.0f, 1.0f, 0.35f + 0.35f * high, legacyColorAdjust_.circleOutline);
        glVertex2f(x, y);
    }
    glEnd();

}

void Visualizer::renderLegacyFrequencyBars(float bass, float mid, float high, float motionBlend, float animatedTime) {
    float width = static_cast<float>(windowWidth_);
    float height = static_cast<float>(windowHeight_);
    float minDimension = std::min(width, height);

    auto drawStrip = [&](float energy,
                         float baseY,
                         float stripThickness,
                         float waveSpeed,
                         float waveFrequency,
                         const ColorAdjust& adjust,
                         float r, float g, float b,
                         float detailAlpha) {
        int segments = 160;
        float amplitude = stripThickness * ((0.2f + 0.4f * motionBlend) + energy * (0.9f + 0.6f * motionBlend));
        float shimmer = 0.35f + 0.45f * energy * motionBlend;

        glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= segments; ++i) {
            float t = static_cast<float>(i) / segments;
            float x = width * t;
            float wave = sinf(animatedTime * waveSpeed + t * waveFrequency) * amplitude;
            float drift = sinf(animatedTime * (waveSpeed * 0.35f) + t * 6.0f) * stripThickness * 0.35f * motionBlend;
            float innerY = baseY - stripThickness - wave + drift;
            float outerY = baseY + stripThickness + wave + drift;

            setColorWithAdjust(r * (0.8f + 0.25f * energy),
                               g * (0.85f + 0.3f * energy),
                               b * (0.9f + 0.35f * energy),
                               0.55f + 0.35f * motionBlend,
                               adjust);
            glVertex2f(x, innerY);
            setColorWithAdjust(r, g, b, 0.25f + 0.55f * shimmer, adjust);
            glVertex2f(x, outerY);
        }
        glEnd();

        glLineWidth(1.3f + 1.1f * (energy + motionBlend));
        glBegin(GL_LINES);
        for (int i = 0; i <= segments; ++i) {
            float t = static_cast<float>(i) / segments;
            float x = width * t;
            float wave = sinf(animatedTime * (waveSpeed * 1.4f) + t * (waveFrequency * 0.6f + 4.0f)) * amplitude * 0.55f;
            float y0 = baseY + sinf(t * 10.0f + animatedTime * 0.9f) * stripThickness * 0.3f * motionBlend - wave;
            setColorWithAdjust(r, g, b,
                               detailAlpha * (0.4f + 0.5f * energy + 0.4f * motionBlend),
                               adjust);
            glVertex2f(x, y0 - stripThickness * (1.0f + 0.3f * motionBlend));
            glVertex2f(x, y0 + stripThickness * (1.0f + 0.3f * motionBlend));
        }
        glEnd();
    };

    struct StripConfig {
        float offsetY;
        float baseThickness;
        float waveSpeed;
        float waveFrequency;
        float colorR, colorG, colorB;
        float detailAlpha;
        const ColorAdjust* adjust;
        float weightBass;
        float weightMid;
        float weightHigh;
    };

    StripConfig configs[] = {
        {-0.26f, 0.028f, 2.2f, 22.0f, 1.0f, 0.35f, 0.28f, 0.22f, &legacyColorAdjust_.bassBars, 1.0f, 0.25f, 0.15f},
        {-0.14f, 0.025f, 2.9f, 30.0f, 0.9f, 0.6f, 0.32f, 0.20f, &legacyColorAdjust_.bassBars, 0.8f, 0.35f, 0.2f},
        {-0.02f, 0.023f, 3.4f, 36.0f, 0.28f, 0.95f, 0.52f, 0.18f, &legacyColorAdjust_.midBars, 0.3f, 1.0f, 0.25f},
        {0.10f, 0.021f, 3.8f, 42.0f, 0.32f, 0.8f, 1.05f, 0.16f, &legacyColorAdjust_.highBars, 0.2f, 0.55f, 1.0f},
        {0.22f, 0.019f, 4.4f, 48.0f, 0.5f, 0.45f, 1.2f, 0.14f, &legacyColorAdjust_.highBars, 0.15f, 0.35f, 1.0f},
        {-0.34f, 0.018f, 1.8f, 18.0f, 0.95f, 0.4f, 0.6f, 0.15f, &legacyColorAdjust_.bassBars, 1.0f, 0.15f, 0.1f},
        {0.30f, 0.017f, 5.0f, 56.0f, 0.35f, 0.7f, 1.35f, 0.12f, &legacyColorAdjust_.highBars, 0.1f, 0.25f, 1.0f}
    };

    int maxStrips = static_cast<int>(sizeof(configs) / sizeof(configs[0]));
    float combinedEnergy = (bass + mid + high) / 3.0f;
    float intensity = std::clamp(combinedEnergy * motionBlend, 0.0f, 1.0f);
    int activeStrips = std::clamp(1 + static_cast<int>(std::floor(intensity * maxStrips)), 1, maxStrips);

    float centerYValue = height * 0.5f;

    for (int i = 0; i < activeStrips; ++i) {
        const StripConfig& cfg = configs[i];
        float weightSum = cfg.weightBass + cfg.weightMid + cfg.weightHigh;
        float energy = 0.0f;
        if (weightSum > 0.0f) {
            energy = (bass * cfg.weightBass + mid * cfg.weightMid + high * cfg.weightHigh) / weightSum;
        }
        float thickness = minDimension * cfg.baseThickness * (0.9f + 1.4f * energy);
        float baseY = centerYValue + cfg.offsetY * minDimension;
        drawStrip(energy,
                  baseY,
                  thickness,
                  cfg.waveSpeed,
                  cfg.waveFrequency,
                  *cfg.adjust,
                  cfg.colorR,
                  cfg.colorG,
                  cfg.colorB,
                  cfg.detailAlpha);
    }
}

void Visualizer::renderLegacyRings(float mid, float animatedTime, float motionBlend) {
    if (motionBlend <= 0.01f) return;

    float centerX = windowWidth_ / 2.0f;
    float centerY = windowHeight_ / 2.0f;
    float minDimension = std::min(windowWidth_, windowHeight_);

    float baseRadius = minDimension * 0.18f;
    float ringSpacing = minDimension * (0.05f + 0.08f * motionBlend);
    int baseRingCount = 2;
    int extraRings = static_cast<int>(std::floor((2.5f + mid * 6.0f) * motionBlend));
    int numRings = std::min(12, baseRingCount + extraRings);

    for (int ring = 0; ring < numRings; ++ring) {
        float radius = baseRadius + ring * ringSpacing;
        float rotation = animatedTime * (0.9f + ring * 0.22f * motionBlend) + mid * 1.8f * motionBlend;
        float alpha = 0.12f + 0.45f * motionBlend * (1.0f - ring / static_cast<float>(numRings));
        float lineWidth = 1.2f + 1.8f * (1.0f - ring / std::max(1, numRings - 1)) * (0.4f + 0.6f * motionBlend);

        setColorWithAdjust(0.8f, 0.8f, 0.2f + 0.3f * mid, alpha, legacyColorAdjust_.rings);
        glLineWidth(lineWidth);
        glBegin(GL_LINE_LOOP);

        int segments = 96;
        for (int i = 0; i <= segments; ++i) {
            float angle = 2.0f * 3.14159f * i / segments + rotation;
            float wobble = sinf(animatedTime * 2.5f + angle * 1.3f) * minDimension * 0.004f * motionBlend;
            float x = centerX + cosf(angle) * (radius + wobble);
            float y = centerY + sinf(angle) * (radius + wobble);
            glVertex2f(x, y);
        }

        glEnd();
    }
}

void Visualizer::renderLegacySparkles(float high, float animatedTime, float motionBlend) {
    if (high < 0.1f) return;
    
    float centerX = windowWidth_ / 2.0f;
    float centerY = windowHeight_ / 2.0f;
    
    int numSparkles = (int)(high * 50);
    glBegin(GL_POINTS);

    for (int i = 0; i < numSparkles; ++i) {
        float angle = (2.0f * 3.14159f * i) / numSparkles + animatedTime * 5.0f;
        float radius = 80.0f + high * 100.0f + sinf(animatedTime * 10.0f + i * motionBlend) * 20.0f * motionBlend;
        
        float x = centerX + cosf(angle) * radius;
        float y = centerY + sinf(angle) * radius;

        setColorWithAdjust(1.0f, 1.0f, 0.8f, high, legacyColorAdjust_.sparkles);
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
