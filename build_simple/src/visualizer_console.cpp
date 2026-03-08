#include "visualizer.h"
#include <iostream>
#include <iomanip>
#include <cmath>

void Visualizer::renderConsoleVisualization() {
    // Clear console for better visualization
    static int consoleCounter = 0;
    consoleCounter++;
    
    // Update console every 10 frames to avoid spam
    if (consoleCounter % 10 != 0) return;
    
    // Clear screen (ANSI escape codes)
    std::cout << "\033[2J\033[H";
    
    // Calculate audio metrics
    float rms = 0.0f;
    float peak = 0.0f;
    for (float sample : waveformBuffer_) {
        rms += sample * sample;
        peak = std::max(peak, std::abs(sample));
    }
    rms = sqrtf(rms / waveformBuffer_.size());
    float db = rms > 0.0f ? 20.0f * log10f(rms) : -60.0f;
    db = std::max(-60.0f, db);
    
    // Header
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    AUDIO VISUALIZER CONSOLE                   ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n\n";
    
    // Device info
    std::cout << "🎤 DEVICE: ";
    if (selectedDevice_ >= 0 && selectedDevice_ < deviceNames_.size()) {
        std::cout << deviceNames_[selectedDevice_] << "\n";
    } else {
        std::cout << "Default\n";
    }
    
    // Audio levels
    std::cout << "\n📊 AUDIO LEVELS:\n";
    std::cout << "   RMS: " << std::fixed << std::setprecision(1) << db << " dB\n";
    std::cout << "   Peak: " << std::fixed << std::setprecision(3) << peak << "\n";
    
    // Frequency analysis
    std::cout << "\n🎵 FREQUENCY ANALYSIS:\n";
    std::cout << "   Bass (20-120Hz):   " << std::fixed << std::setprecision(4) << audioFeatures_.bassEnergy << "\n";
    std::cout << "   Mid (120-2kHz):    " << std::fixed << std::setprecision(4) << audioFeatures_.midEnergy << "\n";
    std::cout << "   High (2k-12kHz):   " << std::fixed << std::setprecision(4) << audioFeatures_.highEnergy << "\n";
    std::cout << "   Total Energy:     " << std::fixed << std::setprecision(4) << audioFeatures_.energy << "\n";
    
    // Beat detection
    std::cout << "\n🥁 BEAT DETECTION:\n";
    std::cout << "   Onset: " << (audioFeatures_.onset > 0.5f ? "🔴 DETECTED" : "⚪ none") << "\n";
    std::cout << "   Beat:  " << (audioFeatures_.beat > 0.5f ? "🔴 BEAT" : "⚪ none") << "\n";
    
    // Visual waveform representation
    std::cout << "\n🌊 WAVEFORM (ASCII):\n";
    std::cout << "   ";
    
    int waveWidth = 60;
    int waveHeight = 10;
    
    for (int x = 0; x < waveWidth; x++) {
        int sampleIndex = (int)(x * waveformBuffer_.size() / waveWidth);
        float sample = waveformBuffer_[sampleIndex];
        int y = (int)((sample + 1.0f) * waveHeight / 2.0f); // Normalize to 0-waveHeight
        y = std::max(0, std::min(waveHeight - 1, y));
        
        for (int row = 0; row < waveHeight; row++) {
            if (row == y) {
                std::cout << "█";
            } else if (row == waveHeight / 2) {
                std::cout << "─";
            } else {
                std::cout << " ";
            }
        }
        std::cout << "\n   ";
    }
    
    // Frequency bars
    std::cout << "\n📈 FREQUENCY BARS:\n";
    std::cout << "   Bass: ";
    int bassBars = (int)(audioFeatures_.bassEnergy * 20);
    for (int i = 0; i < bassBars; i++) std::cout << "█";
    std::cout << "\n   Mid:  ";
    int midBars = (int)(audioFeatures_.midEnergy * 20);
    for (int i = 0; i < midBars; i++) std::cout << "█";
    std::cout << "\n   High: ";
    int highBars = (int)(audioFeatures_.highEnergy * 20);
    for (int i = 0; i < highBars; i++) std::cout << "█";
    std::cout << "\n";
    
    // Status
    std::cout << "\n🟢 STATUS: ";
    if (rms > 0.01f) {
        std::cout << "RECEIVING AUDIO - VISUALIZATION ACTIVE\n";
    } else {
        std::cout << "NO AUDIO INPUT - CHECK DEVICE SELECTION\n";
    }
    
    std::cout << "\n🎮 CONTROLS: D=devices  I=diagnostics  C=console  ESC=exit\n";
    std::cout << "══════════════════════════════════════════════════════════════\n";
    
    // Flush output
    std::cout << std::flush;
}
