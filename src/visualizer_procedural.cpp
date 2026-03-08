#include "visualizer.h"
#include <cmath>
#include <algorithm>
#include <vector>

struct Particle {
    float x, y, z;
    float vx, vy, vz;
    float life;
    float size;
    float r, g, b;
};

void Visualizer::renderProceduralVisualization() {
    // Use fixed function pipeline for procedural geometry
    glUseProgram(0);
    
    // Setup projection
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, windowWidth_, windowHeight_, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Disable depth test for 2D rendering
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Calculate smoothed audio features
    static float smoothBass = 0.0f, smoothMid = 0.0f, smoothHigh = 0.0f;
    const float smoothingFactor = 0.85f;
    
    smoothBass = smoothBass * smoothingFactor + audioFeatures_.bassEnergy * (1.0f - smoothingFactor);
    smoothMid = smoothMid * smoothingFactor + audioFeatures_.midEnergy * (1.0f - smoothingFactor);
    smoothHigh = smoothHigh * smoothingFactor + audioFeatures_.highEnergy * (1.0f - smoothingFactor);
    
    // Render different visual elements based on audio features
    
    // 1. Central reactive circle (kick/bass driven)
    renderReactiveCircle(smoothBass, smoothMid, smoothHigh);
    
    // 2. Frequency bars around the center
    renderFrequencyBars(smoothBass, smoothMid, smoothHigh);
    
    // 3. Particles on beat detection
    if (audioFeatures_.beat > 0.5f) {
        renderBeatExplosion(smoothBass, smoothMid, smoothHigh);
    }
    
    // 4. Rotating rings (mid frequency driven)
    renderRotatingRings(smoothMid, time_);
    
    // 5. High frequency sparkles
    renderHighFrequencySparkles(smoothHigh);
    
    // 6. Waveform visualization
    renderWaveformVisualization();
    
    // Restore state
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void Visualizer::renderReactiveCircle(float bass, float mid, float high) {
    float centerX = windowWidth_ / 2.0f;
    float centerY = windowHeight_ / 2.0f;
    
    // Base radius that pulses with kick
    float baseRadius = 50.0f;
    float radius = baseRadius + bass * 150.0f;
    
    // Color based on frequency mix
    float r = bass * 2.0f;
    float g = mid * 2.0f;
    float b = high * 2.0f;
    
    // Clamp colors and add base color
    r = std::min(1.0f, std::max(0.1f, r));
    g = std::min(1.0f, std::max(0.1f, g));
    b = std::min(1.0f, std::max(0.3f, b));
    
    // Draw filled circle with triangles
    int segments = 32;
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(r, g, b, 0.8f);
    glVertex2f(centerX, centerY);
    
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * M_PI * i / segments;
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
        float angle = 2.0f * M_PI * i / segments;
        float x = centerX + cosf(angle) * radius;
        float y = centerY + sinf(angle) * radius;
        glVertex2f(x, y);
    }
    glEnd();
}

void Visualizer::renderFrequencyBars(float bass, float mid, float high) {
    float centerX = windowWidth_ / 2.0f;
    float centerY = windowHeight_ / 2.0f;
    
    // Number of bars for each frequency band
    int barsPerBand = 8;
    float barWidth = 8.0f;
    float maxBarHeight = 200.0f;
    
    // Bass bars (bottom)
    for (int i = 0; i < barsPerBand; ++i) {
        float angle = -M_PI + (M_PI * i / (barsPerBand - 1));
        float height = bass * maxBarHeight * (0.5f + 0.5f * sinf(time_ * 2.0f + i));
        
        float x1 = centerX + cosf(angle) * 100.0f;
        float y1 = centerY + sinf(angle) * 100.0f;
        float x2 = centerX + cosf(angle) * (100.0f + height);
        float y2 = centerY + sinf(angle) * (100.0f + height);
        
        glColor4f(1.0f, 0.2f, 0.2f, 0.7f);
        glLineWidth(barWidth);
        glBegin(GL_LINES);
        glVertex2f(x1, y1);
        glVertex2f(x2, y2);
        glEnd();
    }
    
    // Mid bars (middle ring)
    for (int i = 0; i < barsPerBand; ++i) {
        float angle = -M_PI + (M_PI * i / (barsPerBand - 1));
        float height = mid * maxBarHeight * 0.8f;
        
        float x1 = centerX + cosf(angle + time_ * 0.5f) * 150.0f;
        float y1 = centerY + sinf(angle + time_ * 0.5f) * 150.0f;
        float x2 = centerX + cosf(angle + time_ * 0.5f) * (150.0f + height);
        float y2 = centerY + sinf(angle + time_ * 0.5f) * (150.0f + height);
        
        glColor4f(0.2f, 1.0f, 0.2f, 0.7f);
        glLineWidth(barWidth);
        glBegin(GL_LINES);
        glVertex2f(x1, y1);
        glVertex2f(x2, y2);
        glEnd();
    }
    
    // High bars (outer ring)
    for (int i = 0; i < barsPerBand; ++i) {
        float angle = -M_PI + (M_PI * i / (barsPerBand - 1));
        float height = high * maxBarHeight * 0.6f;
        
        float x1 = centerX + cosf(angle - time_ * 0.8f) * 200.0f;
        float y1 = centerY + sinf(angle - time_ * 0.8f) * 200.0f;
        float x2 = centerX + cosf(angle - time_ * 0.8f) * (200.0f + height);
        float y2 = centerY + sinf(angle - time_ * 0.8f) * (200.0f + height);
        
        glColor4f(0.2f, 0.2f, 1.0f, 0.7f);
        glLineWidth(barWidth);
        glBegin(GL_LINES);
        glVertex2f(x1, y1);
        glVertex2f(x2, y2);
        glEnd();
    }
}

void Visualizer::renderBeatExplosion(float bass, float mid, float high) {
    static std::vector<Particle> particles;
    static float lastBeatTime = 0.0f;
    
    // Create new particles on beat
    if (time_ - lastBeatTime > 0.1f) { // Prevent too many particles
        lastBeatTime = time_;
        
        for (int i = 0; i < 20; ++i) {
            Particle p;
            p.x = windowWidth_ / 2.0f;
            p.y = windowHeight_ / 2.0f;
            p.z = 0.0f;
            
            float angle = (2.0f * M_PI * i) / 20.0f;
            float speed = 100.0f + bass * 200.0f;
            p.vx = cosf(angle) * speed;
            p.vy = sinf(angle) * speed;
            p.vz = 0.0f;
            
            p.life = 1.0f;
            p.size = 3.0f + high * 5.0f;
            
            p.r = 1.0f;
            p.g = 0.5f + mid * 0.5f;
            p.b = 0.2f + high * 0.8f;
            
            particles.push_back(p);
        }
    }
    
    // Update and render particles
    glPointSize(5.0f);
    glBegin(GL_POINTS);
    
    for (auto it = particles.begin(); it != particles.end();) {
        // Update particle
        it->x += it->vx * 0.016f; // 60 FPS assumption
        it->y += it->vy * 0.016f;
        it->life -= 0.02f;
        it->vx *= 0.98f; // Damping
        it->vy *= 0.98f;
        
        if (it->life <= 0.0f) {
            it = particles.erase(it);
        } else {
            glColor4f(it->r, it->g, it->b, it->life);
            glVertex2f(it->x, it->y);
            ++it;
        }
    }
    
    glEnd();
}

void Visualizer::renderRotatingRings(float mid, float time) {
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
            float angle = 2.0f * M_PI * i / segments + rotation;
            float x = centerX + cosf(angle) * radius;
            float y = centerY + sinf(angle) * radius;
            glVertex2f(x, y);
        }
        
        glEnd();
    }
}

void Visualizer::renderHighFrequencySparkles(float high) {
    if (high < 0.1f) return; // Only show with significant high frequency
    
    float centerX = windowWidth_ / 2.0f;
    float centerY = windowHeight_ / 2.0f;
    
    int numSparkles = (int)(high * 50);
    for (int i = 0; i < numSparkles; ++i) {
        float angle = (2.0f * M_PI * i) / numSparkles + time_ * 5.0f;
        float radius = 80.0f + high * 100.0f + sinf(time_ * 10.0f + i) * 20.0f;
        
        float x = centerX + cosf(angle) * radius;
        float y = centerY + sinf(angle) * radius;
        
        glColor4f(1.0f, 1.0f, 0.8f, high);
        glPointSize(2.0f);
        glBegin(GL_POINTS);
        glVertex2f(x, y);
        glEnd();
    }
}

void Visualizer::renderWaveformVisualization() {
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
