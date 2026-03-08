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
    
    // Add some base animation even without audio
    float timeAnimation = sinf(time_) * 0.5f + 0.5f;  // 0 to 1
    float globalPulse = 0.5f + 0.5f * sinf(time_ * 0.7f);
    
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
    renderLegacyCircle(bassValue, midValue, highValue);

    // 2. Glow aura around the circle
    glPushMatrix();
    renderLegacyCircle(std::min(1.0f, bassValue * 0.6f + 0.2f + globalPulse * 0.2f),
                       std::min(1.0f, midValue * 0.6f + 0.1f),
                       std::min(1.0f, highValue * 0.6f + averageEnergy * 0.3f));
    glPopMatrix();
    
    // 3. Frequency bars with waves
    renderLegacyFrequencyBars(bassValue, midValue, highValue);
    
    // 4. Rotating rings with wobble
    renderLegacyRings(midValue, time_);
    
    // 5. High frequency sparkles + trails
    renderLegacySparkles(highValue);
    renderLegacySparkles(std::min(1.0f, highValue * 0.5f + averageEnergy * 0.4f));
    
    // 6. Floating energy orbs
    const int orbCount = 18;
    float aspectX = windowWidth_ / safeMinDimension;
    float aspectY = windowHeight_ / safeMinDimension;
    float orbitBase = minDimension * (0.32f + averageEnergy * 0.15f);
    float orbitRange = minDimension * (0.18f + bassValue * 0.15f);
    for (int i = 0; i < orbCount; ++i) {
        float offset = i * 0.349f;
        float radius = orbitBase + sinf(time_ * 0.8f + offset) * orbitRange;
        float angle = time_ * (0.45f + midValue * 1.6f) + offset;
        float x = centerX + cosf(angle) * radius * aspectX;
        float y = centerY + sinf(angle) * radius * aspectY;
        float intensity = 0.35f + 0.65f * (0.5f + 0.5f * sinf(time_ * 1.9f + offset));

        glPointSize(6.0f + highValue * 22.0f);
        glBegin(GL_POINTS);
        setColorWithAdjust(0.4f + highValue * 0.5f,
                           0.15f + midValue * 0.55f,
                           1.0f,
                           intensity,
                           legacyColorAdjust_.orbit);
        glVertex2f(x, y);
        glEnd();

        glBegin(GL_LINES);
        setColorWithAdjust(0.1f + bassValue * 0.3f,
                           0.2f + midValue * 0.3f,
                           0.6f + highValue * 0.3f,
                           0.12f,
                           legacyColorAdjust_.orbitTrail);
        glVertex2f(centerX, centerY);
        glVertex2f(x, y);
        glEnd();
    }
    
    // 7. Waveform ribbon
    renderLegacyWaveform();
    
    // Restore state
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

void Visualizer::renderLegacyCircle(float bass, float mid, float high) {
    float centerX = windowWidth_ / 2.0f;
    float centerY = windowHeight_ / 2.0f;
    float minDimension = std::min(windowWidth_, windowHeight_);

    float gridSize = std::max(4.0f, minDimension * 0.012f);
    auto quantize = [&](float value) {
        return std::round(value / gridSize) * gridSize;
    };

    float baseRadius = minDimension * 0.24f;
    float pulse = bass * minDimension * 0.18f;
    float wobble = sinf(time_ * 1.7f) * minDimension * 0.025f;
    float radius = baseRadius + pulse + wobble;

    float hue = std::fmod(time_ * 0.1f + bass * 0.3f + mid * 0.2f, 1.0f);
    float colorR = std::abs(std::sin(hue * 6.28318f)) * 0.8f + 0.2f;
    float colorG = std::abs(std::sin((hue + 0.33f) * 6.28318f)) * 0.8f + 0.2f;
    float colorB = std::abs(std::sin((hue + 0.66f) * 6.28318f)) * 0.8f + 0.2f;

    glBegin(GL_TRIANGLE_FAN);
    setColorWithAdjust(colorR, colorG, colorB, 0.75f, legacyColorAdjust_.circleFill);
    glVertex2f(centerX, centerY);
    int segments = 64;
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * 3.14159f * i / segments;
        float audioWarp = 1.0f + high * 0.25f * sinf(time_ * 5.0f + i * 0.4f);
        float x = quantize(centerX + cosf(angle) * radius * audioWarp);
        float y = quantize(centerY + sinf(angle) * radius * audioWarp);
        glVertex2f(x, y);
    }
    glEnd();

    float haloThickness = minDimension * (0.05f + mid * 0.06f);
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * 3.14159f * i / segments;
        float shimmer = 1.0f + high * 0.25f * sinf(time_ * 4.8f + i * 0.6f);
        float innerRadius = radius * shimmer;
        float outerRadius = innerRadius + haloThickness;

        setColorWithAdjust(colorR, colorG, colorB, 0.45f, legacyColorAdjust_.circleOutline);
        glVertex2f(quantize(centerX + cosf(angle) * innerRadius),
                   quantize(centerY + sinf(angle) * innerRadius));
        setColorWithAdjust(0.2f, 0.2f, 0.2f, 0.05f, legacyColorAdjust_.circleOutline);
        glVertex2f(quantize(centerX + cosf(angle) * outerRadius),
                   quantize(centerY + sinf(angle) * outerRadius));
    }
    glEnd();

    glLineWidth(4.0f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * 3.14159f * i / segments;
        float shimmer = 1.0f + mid * 0.2f * sinf(time_ * 3.0f + i * 0.9f);
        float x = quantize(centerX + cosf(angle) * (radius + 12.0f) * shimmer);
        float y = quantize(centerY + sinf(angle) * (radius + 12.0f) * shimmer);
        setColorWithAdjust(1.0f, 1.0f, 1.0f, 0.35f + 0.35f * high, legacyColorAdjust_.circleOutline);
        glVertex2f(x, y);
    }
    glEnd();

}

void Visualizer::renderLegacyFrequencyBars(float bass, float mid, float high) {
    float centerX = windowWidth_ / 2.0f;
    float centerY = windowHeight_ / 2.0f;
    float minDimension = std::min(windowWidth_, windowHeight_);
    float gridSize = std::max(3.0f, minDimension * 0.01f);
    auto quantize = [&](float value) {
        return std::round(value / gridSize) * gridSize;
    };

    auto drawArcBand = [&](float energy, float startAngleDeg, float endAngleDeg, float innerRadiusFactor,
                           float thicknessFactor, const ColorAdjust& adjust, float r, float g, float b) {
        int segments = 64;
        float innerRadius = minDimension * innerRadiusFactor;
        float thickness = minDimension * thicknessFactor * (0.6f + energy * 0.9f);
        float rotationOffset = time_ * (0.2f + energy * 1.1f);

        glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= segments; ++i) {
            float t = static_cast<float>(i) / segments;
            float angle = (startAngleDeg + (endAngleDeg - startAngleDeg) * t) * (3.14159f / 180.0f);
            angle += rotationOffset;
            float radiusWave = 1.0f + 0.15f * sinf(time_ * 3.0f + t * 12.0f);
            float glitchStep = std::fmod(time_ * 7.0f + t * 24.0f, 1.0f) < 0.12f ? gridSize * 2.0f : 0.0f;
            float outerRadius = innerRadius + thickness * radiusWave + glitchStep;

            setColorWithAdjust(r, g, b, 0.85f, adjust);
            glVertex2f(quantize(centerX + cosf(angle) * innerRadius),
                       quantize(centerY + sinf(angle) * innerRadius));
            setColorWithAdjust(r, g, b, 0.15f + 0.7f * energy, adjust);
            glVertex2f(quantize(centerX + cosf(angle) * outerRadius),
                       quantize(centerY + sinf(angle) * outerRadius));
        }
        glEnd();
    };

    drawArcBand(bass, -210.0f, -330.0f, 0.34f, 0.18f, legacyColorAdjust_.bassBars,
                1.0f, 0.3f + bass * 0.6f, 0.25f);
    drawArcBand(mid, -45.0f, 45.0f, 0.44f, 0.16f, legacyColorAdjust_.midBars,
                0.25f, 0.9f, 0.5f + 0.4f * mid);
    drawArcBand(high, 120.0f, 260.0f, 0.52f, 0.14f, legacyColorAdjust_.highBars,
                0.3f + high * 0.5f, 0.3f, 1.0f);

    glLineWidth(2.0f);
    glBegin(GL_LINES);
    for (int i = 0; i < 8; ++i) {
        float angle = time_ * (1.5f + high * 1.1f) + i * 0.8f;
        float length = minDimension * (0.2f + 0.05f * sinf(time_ * 2.0f + i));
        float x1 = quantize(centerX + cosf(angle) * length);
        float y1 = quantize(centerY + sinf(angle) * length);
        float x2 = quantize(centerX + cosf(angle) * (length + gridSize * 6.0f));
        float y2 = quantize(centerY + sinf(angle) * (length + gridSize * 6.0f));
        setColorWithAdjust(0.9f, 0.9f, 1.2f, 0.35f, legacyColorAdjust_.highBars);
        glVertex2f(x1, y1);
        glVertex2f(x2, y2);
    }
    glEnd();
}

void Visualizer::renderLegacyRings(float mid, float time) {
    float centerX = windowWidth_ / 2.0f;
    float centerY = windowHeight_ / 2.0f;
    
    int numRings = 3;
    for (int ring = 0; ring < numRings; ++ring) {
        float radius = 120.0f + ring * 40.0f;
        float rotation = time * (1.0f + ring * 0.3f) + mid * 2.0f;
        
        setColorWithAdjust(0.8f, 0.8f, 0.2f, 0.3f, legacyColorAdjust_.rings);
        glLineWidth(2.0f);
        glBegin(GL_LINE_LOOP);
        
        int segments = 64;
        for (int i = 0; i <= segments; ++i) {
            float angle = 2.0f * 3.14159f * i / segments + rotation;
            float x = centerX + cosf(angle) * radius;
            float y = centerY + sinf(angle) * radius;
            glVertex2f(x, y);
        }
        
        glEnd();
    }
}

void Visualizer::renderLegacySparkles(float high) {
    if (high < 0.1f) return;
    
    float centerX = windowWidth_ / 2.0f;
    float centerY = windowHeight_ / 2.0f;
    
    int numSparkles = (int)(high * 50);
    glBegin(GL_POINTS);

    for (int i = 0; i < numSparkles; ++i) {
        float angle = (2.0f * 3.14159f * i) / numSparkles + time_ * 5.0f;
        float radius = 80.0f + high * 100.0f + sinf(time_ * 10.0f + i) * 20.0f;
        
        float x = centerX + cosf(angle) * radius;
        float y = centerY + sinf(angle) * radius;

        setColorWithAdjust(1.0f, 1.0f, 0.8f, high, legacyColorAdjust_.sparkles);
        glVertex2f(x, y);
    }
    
    glEnd();
}

void Visualizer::renderLegacyWaveform() {
    if (waveformBuffer_.empty()) return;

    float centerX = windowWidth_ / 2.0f;
    float centerY = windowHeight_ / 2.0f;
    float minDimension = std::min(windowWidth_, windowHeight_);

    float gridSize = std::max(4.5f, minDimension * 0.014f);
    auto quantize = [&](float value) {
        return std::round(value / gridSize) * gridSize;
    };

    float baseRadius = minDimension * 0.46f;
    float ringThickness = minDimension * 0.18f;
    float waveformAmplitude = minDimension * 0.085f;
    float rotation = time_ * 0.45f;

    setColorWithAdjust(0.35f, 0.9f, 1.0f, 0.75f, legacyColorAdjust_.waveform);
    glBegin(GL_TRIANGLE_STRIP);
    size_t sampleCount = waveformBuffer_.size();
    for (size_t i = 0; i <= sampleCount; ++i) {
        float t = static_cast<float>(i % sampleCount) / static_cast<float>(sampleCount);
        float angle = t * 6.28318f + rotation;
        float sample = waveformBuffer_[i % sampleCount];
        float glitch = (std::fmod(time_ * 6.0f + t * 32.0f, 1.0f) < 0.22f) ? gridSize * 1.2f : 0.0f;
        float radialWave = sample * waveformAmplitude + glitch;

        float innerRadius = baseRadius - ringThickness * 0.55f + radialWave * 0.5f;
        float outerRadius = baseRadius + ringThickness * 0.55f + radialWave;

        glVertex2f(quantize(centerX + cosf(angle) * innerRadius),
                   quantize(centerY + sinf(angle) * innerRadius));
        glVertex2f(quantize(centerX + cosf(angle) * outerRadius),
                   quantize(centerY + sinf(angle) * outerRadius));
    }
    glEnd();

    auto drawRing = [&](float offset, float alpha, float freqMult) {
        setColorWithAdjust(0.4f, 1.0f, 1.3f, alpha, legacyColorAdjust_.waveform);
        glBegin(GL_LINE_LOOP);
        for (size_t i = 0; i < sampleCount; ++i) {
            float t = static_cast<float>(i) / sampleCount;
            float angle = t * 6.28318f + rotation * (1.0f + freqMult * 0.2f);
            float sample = waveformBuffer_[i];
            float radialWave = sample * waveformAmplitude * (0.6f + freqMult * 0.3f);
            float radius = baseRadius + offset + radialWave;
            glVertex2f(quantize(centerX + cosf(angle) * radius),
                       quantize(centerY + sinf(angle) * radius));
        }
        glEnd();
    };

    drawRing(-ringThickness * 0.5f, 0.35f, -0.6f);
    drawRing(0.0f, 0.55f, 0.0f);
    drawRing(ringThickness * 0.5f, 0.28f, 0.75f);

    setColorWithAdjust(0.6f, 1.2f, 1.4f, 0.25f, legacyColorAdjust_.waveform);
    glBegin(GL_LINES);
    const int spokes = 24;
    for (int i = 0; i < spokes; ++i) {
        float t = static_cast<float>(i) / spokes;
        float angle = t * 6.28318f + rotation * 1.3f;
        float jitter = (std::fmod(time_ * 3.5f + i * 0.37f, 1.0f) < 0.4f) ? gridSize * 1.5f : 0.0f;
        float innerRadius = baseRadius - ringThickness * 0.65f + jitter;
        float outerRadius = baseRadius + ringThickness * 0.7f + jitter;
        glVertex2f(quantize(centerX + cosf(angle) * innerRadius),
                   quantize(centerY + sinf(angle) * innerRadius));
        glVertex2f(quantize(centerX + cosf(angle) * outerRadius),
                   quantize(centerY + sinf(angle) * outerRadius));
    }
    glEnd();
}
