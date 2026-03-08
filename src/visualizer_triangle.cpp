#include "visualizer.h"
#include <cmath>
#include <algorithm>

void Visualizer::renderFallbackTriangle() {
    // Use fixed function pipeline for simple triangle
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
    
    // Calculate triangle properties based on audio
    float centerX = windowWidth_ / 2.0f;
    float centerY = windowHeight_ / 2.0f;
    
    // Size based on bass energy
    float baseSize = 100.0f;
    float size = baseSize + audioFeatures_.bassEnergy * 200.0f;
    
    // Rotation based on time and mid energy
    float rotation = time_ + audioFeatures_.midEnergy * 5.0f;
    
    // Color based on frequency bands
    float r = audioFeatures_.bassEnergy;
    float g = audioFeatures_.midEnergy;
    float b = audioFeatures_.highEnergy;
    
    // Clamp colors
    r = std::min(1.0f, r);
    g = std::min(1.0f, g);
    b = std::min(1.0f, b);
    
    // Add some base color so it's visible even with no audio
    r = std::max(0.2f, r);
    g = std::max(0.2f, g);
    b = std::max(0.4f, b);
    
    // Beat detection - make triangle flash
    if (audioFeatures_.beat > 0.5f) {
        r = 1.0f;
        g = 1.0f;
        b = 1.0f;
        size *= 1.5f; // Make it bigger on beat
    }
    
    // Set color
    glColor3f(r, g, b);
    
    // Calculate triangle vertices
    float cos_r = cosf(rotation);
    float sin_r = sinf(rotation);
    
    // Triangle points (before rotation)
    float x1 = 0.0f;
    float y1 = -size;
    float x2 = -size * 0.866f;  // cos(120°) = -0.5, sin(120°) = 0.866
    float y2 = size * 0.5f;
    float x3 = size * 0.866f;
    float y3 = size * 0.5f;
    
    // Rotate and translate
    float rx1 = x1 * cos_r - y1 * sin_r + centerX;
    float ry1 = x1 * sin_r + y1 * cos_r + centerY;
    float rx2 = x2 * cos_r - y2 * sin_r + centerX;
    float ry2 = x2 * sin_r + y2 * cos_r + centerY;
    float rx3 = x3 * cos_r - y3 * sin_r + centerX;
    float ry3 = x3 * sin_r + y3 * cos_r + centerY;
    
    // Draw filled triangle
    glBegin(GL_TRIANGLES);
    glVertex2f(rx1, ry1);
    glVertex2f(rx2, ry2);
    glVertex2f(rx3, ry3);
    glEnd();
    
    // Draw outline for better visibility
    glColor3f(1.0f, 1.0f, 1.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(rx1, ry1);
    glVertex2f(rx2, ry2);
    glVertex2f(rx3, ry3);
    glEnd();
    
    // Draw inner triangles based on high frequency
    if (audioFeatures_.highEnergy > 0.1f) {
        float innerSize = size * 0.5f;
        float innerRotation = -rotation * 2.0f;
        float cos_ir = cosf(innerRotation);
        float sin_ir = sinf(innerRotation);
        
        float ix1 = 0.0f;
        float iy1 = -innerSize;
        float ix2 = -innerSize * 0.866f;
        float iy2 = innerSize * 0.5f;
        float ix3 = innerSize * 0.866f;
        float iy3 = innerSize * 0.5f;
        
        float irx1 = ix1 * cos_ir - iy1 * sin_ir + centerX;
        float iry1 = ix1 * sin_ir + iy1 * cos_ir + centerY;
        float irx2 = ix2 * cos_ir - iy2 * sin_ir + centerX;
        float iry2 = ix2 * sin_ir + iy2 * cos_ir + centerY;
        float irx3 = ix3 * cos_ir - iy3 * sin_ir + centerX;
        float iry3 = ix3 * sin_ir + iy3 * cos_ir + centerY;
        
        glColor3f(1.0f, 1.0f, 0.0f); // Yellow inner triangle
        glBegin(GL_TRIANGLES);
        glVertex2f(irx1, iry1);
        glVertex2f(irx2, iry2);
        glVertex2f(irx3, iry3);
        glEnd();
    }
    
    // Restore state
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}
