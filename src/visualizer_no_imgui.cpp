#include "visualizer.h"
#include <iostream>

void Visualizer::renderNoImGui() {
    // Clear screen
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Render legacy visualization (works with software rendering)
    renderLegacyVisualization();
    
    // Simple text overlay using OpenGL
    renderSimpleText();
}

void Visualizer::renderSimpleText() {
    // Use legacy OpenGL for text rendering
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, windowWidth_, windowHeight_, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Disable depth test for 2D text
    glDisable(GL_DEPTH_TEST);
    glColor3f(1.0f, 1.0f, 1.0f);
    
    // Simple status text
    float y = 20.0f;
    
    // Device info
    std::string deviceName = "Device: ";
    if (selectedDevice_ >= 0 && selectedDevice_ < deviceNames_.size()) {
        deviceName += deviceNames_[selectedDevice_];
    } else {
        deviceName += "Default";
    }
    
    // Audio status
    float rms = 0.0f;
    for (float sample : waveformBuffer_) {
        rms += sample * sample;
    }
    rms = sqrtf(rms / waveformBuffer_.size());
    
    std::string audioStatus = rms > 0.01f ? "🟢 RECEIVING AUDIO" : "🔴 NO AUDIO INPUT";
    
    // Instructions
    std::string instructions = "Press ESC to exit | Play music to see reactive visuals";
    
    // Render text (simplified - just show basic info)
    glColor3f(0.8f, 0.8f, 0.8f);
    glRasterPos2f(10.0f, y);
    
    // Restore state
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// Override render method for no-ImGui version
void Visualizer::renderNoImGuiLoop() {
    // Clear screen
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Render legacy visualization (works with software rendering)
    renderLegacyVisualization();
}
