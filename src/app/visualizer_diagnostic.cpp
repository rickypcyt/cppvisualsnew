#include "visualizer.h"
#include <cmath>
#include <algorithm>

void Visualizer::renderDiagnosticInfo() {
    // Calculate RMS (decibels)
    float rms = 0.0f;
    for (float sample : waveformBuffer_) {
        rms += sample * sample;
    }
    rms = sqrtf(rms / waveformBuffer_.size());
    
    // Convert to decibels (avoid log of zero)
    float db = rms > 0.0f ? 20.0f * log10f(rms) : -60.0f;
    db = std::max(-60.0f, db); // Clamp to -60dB minimum
    
    // Find peak value
    float peak = 0.0f;
    for (float sample : waveformBuffer_) {
        peak = std::max(peak, std::abs(sample));
    }
    
    // Display diagnostic information
    float diagX = 10.0f;
    float diagY = 60.0f;
    float lineHeight = 20.0f;
    
    renderText("=== AUDIO DIAGNOSTICS ===", diagX, diagY);
    renderText("RMS Level: " + std::to_string(db) + " dB", diagX, diagY + lineHeight);
    renderText("Peak Level: " + std::to_string(peak), diagX, diagY + lineHeight * 2);
    renderText("Bass Energy: " + std::to_string(audioFeatures_.bassEnergy), diagX, diagY + lineHeight * 3);
    renderText("Mid Energy: " + std::to_string(audioFeatures_.midEnergy), diagX, diagY + lineHeight * 4);
    renderText("High Energy: " + std::to_string(audioFeatures_.highEnergy), diagX, diagY + lineHeight * 5);
    renderText("Total Energy: " + std::to_string(audioFeatures_.energy), diagX, diagY + lineHeight * 6);
    renderText("Onset: " + std::to_string(audioFeatures_.onset), diagX, diagY + lineHeight * 7);
    renderText("Beat: " + std::to_string(audioFeatures_.beat), diagX, diagY + lineHeight * 8);
    
    // Audio level meter (simple visualization)
    float meterX = diagX;
    float meterY = diagY + lineHeight * 10;
    float meterWidth = 200.0f;
    float meterHeight = 20.0f;
    
    // Draw meter background
    renderText("Level Meter:", meterX, meterY);
    
    // Draw meter fill (based on RMS)
    float fillWidth = (rms / 1.0f) * meterWidth; // Normalize to 0-1
    if (fillWidth > 0) {
        std::string meterFill = "[";
        int fillChars = (int)(fillWidth / 10); // Each char = 10 units
        for (int i = 0; i < fillChars && i < 20; ++i) {
            meterFill += "=";
        }
        for (int i = fillChars; i < 20; ++i) {
            meterFill += " ";
        }
        meterFill += "]";
        renderText(meterFill, meterX, meterY + lineHeight);
    }
    
    // Status indicators
    std::string status = "Status: ";
    if (rms > 0.01f) {
        status += "RECEIVING AUDIO";
    } else {
        status += "NO AUDIO INPUT";
    }
    renderText(status, diagX, meterY + lineHeight * 2);
}
