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
    const float smoothingFactor = 0.85f;
    
    smoothBass = smoothBass * smoothingFactor + audioFeatures_.bassEnergy * (1.0f - smoothingFactor);
    smoothMid = smoothMid * smoothingFactor + audioFeatures_.midEnergy * (1.0f - smoothingFactor);
    smoothHigh = smoothHigh * smoothingFactor + audioFeatures_.highEnergy * (1.0f - smoothingFactor);
    
    // Add some base animation even without audio
    float timeAnimation = sinf(time_) * 0.5f + 0.5f;  // 0 to 1
    
    // Use actual audio data if available, otherwise use animation
    float bassValue = audioFeatures_.bassEnergy > 0.01f ? smoothBass : timeAnimation;
    float midValue = audioFeatures_.midEnergy > 0.01f ? smoothMid : timeAnimation * 0.8f;
    float highValue = audioFeatures_.highEnergy > 0.01f ? smoothHigh : timeAnimation * 0.6f;
    
    // Render visual elements using legacy OpenGL
    
    // 1. Central reactive circle
    renderLegacyCircle(bassValue, midValue, highValue);
    
    // 2. Frequency bars
    renderLegacyFrequencyBars(bassValue, midValue, highValue);
    
    // 3. Beat explosions
    if (audioFeatures_.beat > 0.5f || (debugCounter % 30 == 0)) {  // Also trigger periodically
        renderLegacyBeatExplosion(bassValue, midValue, highValue);
    }
    
    // 4. Rotating rings
    renderLegacyRings(midValue, time_);
    
    // 5. High frequency sparkles
    renderLegacySparkles(highValue);
    
    // 6. Waveform
    renderLegacyWaveform();
    
    // Restore state
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

void Visualizer::renderLegacyCircle(float bass, float mid, float high) {
    float centerX = windowWidth_ / 2.0f;
    float centerY = windowHeight_ / 2.0f;
    
    // Base radius that pulses with kick
    float baseRadius = 50.0f;
    float radius = baseRadius + bass * 150.0f;
    
    // Color based on frequency mix
    float r = std::min(1.0f, std::max(0.1f, bass * 2.0f));
    float g = std::min(1.0f, std::max(0.1f, mid * 2.0f));
    float b = std::min(1.0f, std::max(0.3f, high * 2.0f));
    
    // Draw filled circle using triangle fan
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(r, g, b, 0.8f);
    glVertex2f(centerX, centerY);  // Center point
    
    int segments = 32;
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * 3.14159f * i / segments;
        float x = centerX + cosf(angle) * radius;
        float y = centerY + sinf(angle) * radius;
        glVertex2f(x, y);
    }
    glEnd();
    
    // Draw outline
    glColor4f(1.0f, 1.0f, 1.0f, 0.5f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * 3.14159f * i / segments;
        float x = centerX + cosf(angle) * radius;
        float y = centerY + sinf(angle) * radius;
        glVertex2f(x, y);
    }
    glEnd();
}

void Visualizer::renderLegacyFrequencyBars(float bass, float mid, float high) {
    float centerX = windowWidth_ / 2.0f;
    float centerY = windowHeight_ / 2.0f;
    
    int barsPerBand = 8;
    float maxBarHeight = 200.0f;
    
    // Bass bars (bottom ring)
    glColor4f(1.0f, 0.2f, 0.2f, 0.7f);
    glLineWidth(8.0f);
    glBegin(GL_LINES);
    for (int i = 0; i < barsPerBand; ++i) {
        float angle = -3.14159f + (3.14159f * i / (barsPerBand - 1));
        float height = bass * maxBarHeight * (0.5f + 0.5f * sinf(time_ * 2.0f + i));
        
        float x1 = centerX + cosf(angle) * 100.0f;
        float y1 = centerY + sinf(angle) * 100.0f;
        float x2 = centerX + cosf(angle) * (100.0f + height);
        float y2 = centerY + sinf(angle) * (100.0f + height);
        
        glVertex2f(x1, y1);
        glVertex2f(x2, y2);
    }
    glEnd();
    
    // Mid bars (middle ring)
    glColor4f(0.2f, 1.0f, 0.2f, 0.7f);
    glBegin(GL_LINES);
    for (int i = 0; i < barsPerBand; ++i) {
        float angle = -3.14159f + (3.14159f * i / (barsPerBand - 1));
        float height = mid * maxBarHeight * 0.8f;
        
        float rotAngle = angle + time_ * 0.5f;
        float x1 = centerX + cosf(rotAngle) * 150.0f;
        float y1 = centerY + sinf(rotAngle) * 150.0f;
        float x2 = centerX + cosf(rotAngle) * (150.0f + height);
        float y2 = centerY + sinf(rotAngle) * (150.0f + height);
        
        glVertex2f(x1, y1);
        glVertex2f(x2, y2);
    }
    glEnd();
    
    // High bars (outer ring)
    glColor4f(0.2f, 0.2f, 1.0f, 0.7f);
    glBegin(GL_LINES);
    for (int i = 0; i < barsPerBand; ++i) {
        float angle = -3.14159f + (3.14159f * i / (barsPerBand - 1));
        float height = high * maxBarHeight * 0.6f;
        
        float rotAngle = angle - time_ * 0.8f;
        float x1 = centerX + cosf(rotAngle) * 200.0f;
        float y1 = centerY + sinf(rotAngle) * 200.0f;
        float x2 = centerX + cosf(rotAngle) * (200.0f + height);
        float y2 = centerY + sinf(rotAngle) * (200.0f + height);
        
        glVertex2f(x1, y1);
        glVertex2f(x2, y2);
    }
    glEnd();
}

void Visualizer::renderLegacyBeatExplosion(float bass, float mid, float high) {
    static float explosionTime = 0.0f;
    static bool exploding = false;
    
    if (audioFeatures_.beat > 0.5f && !exploding) {
        exploding = true;
        explosionTime = 0.0f;
    }
    
    if (exploding) {
        explosionTime += 0.016f;  // ~60 FPS
        
        if (explosionTime > 1.0f) {
            exploding = false;
            return;
        }
        
        float centerX = windowWidth_ / 2.0f;
        float centerY = windowHeight_ / 2.0f;
        float maxRadius = 300.0f;
        float currentRadius = explosionTime * maxRadius;
        float alpha = 1.0f - explosionTime;
        
        glColor4f(1.0f, 0.5f + mid * 0.5f, 0.2f + high * 0.8f, alpha);
        glLineWidth(3.0f);
        
        glBegin(GL_LINE_LOOP);
        int segments = 32;
        for (int i = 0; i <= segments; ++i) {
            float angle = 2.0f * 3.14159f * i / segments;
            float x = centerX + cosf(angle) * currentRadius;
            float y = centerY + sinf(angle) * currentRadius;
            glVertex2f(x, y);
        }
        glEnd();
    }
}

void Visualizer::renderLegacyRings(float mid, float time) {
    float centerX = windowWidth_ / 2.0f;
    float centerY = windowHeight_ / 2.0f;
    
    int numRings = 3;
    for (int ring = 0; ring < numRings; ++ring) {
        float radius = 120.0f + ring * 40.0f;
        float rotation = time * (1.0f + ring * 0.3f) + mid * 2.0f;
        
        glColor4f(0.8f, 0.8f, 0.2f, 0.3f);
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
        
        glColor4f(1.0f, 1.0f, 0.8f, high);
        glVertex2f(x, y);
    }
    
    glEnd();
}

void Visualizer::renderLegacyWaveform() {
    if (waveformBuffer_.empty()) return;
    
    float centerY = windowHeight_ - 100.0f;
    float waveHeight = 50.0f;
    float waveWidth = windowWidth_ - 100.0f;
    float startX = 50.0f;
    
    glColor4f(0.0f, 1.0f, 0.8f, 0.8f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_STRIP);
    
    for (size_t i = 0; i < waveformBuffer_.size(); ++i) {
        float x = startX + (float)i / waveformBuffer_.size() * waveWidth;
        float y = centerY + waveformBuffer_[i] * waveHeight;
        glVertex2f(x, y);
    }
    
    glEnd();
    
    // Draw center line
    glColor4f(0.5f, 0.5f, 0.5f, 0.5f);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    glVertex2f(startX, centerY);
    glVertex2f(startX + waveWidth, centerY);
    glEnd();
}
