#include "visualizer.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <iomanip>
#include <cmath>

void Visualizer::setupImGui() {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    
    // Customize style for audio visualizer
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.Alpha = 0.9f;
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    const char* glsl_version = "#version 130";
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    showImGuiWindow_ = true;
    showDeviceSelector_ = false;
    showDiagnosticInfo_ = false;
    showConsoleMode_ = false;
}

void Visualizer::shutdownImGui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Visualizer::renderImGui() {
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Main control window
    if (showImGuiWindow_) {
        renderMainImGuiWindow();
    }

    // Device selector window
    if (showDeviceSelector_) {
        renderDeviceSelectorImGui();
    }

    // Diagnostic info window
    if (showDiagnosticInfo_) {
        renderDiagnosticImGui();
    }

    // Console mode window
    if (showConsoleMode_) {
        renderConsoleImGui();
    }

    // Render ImGui
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Visualizer::renderMainImGuiWindow() {
    ImGui::Begin("Audio Visualizer Control Panel", &showImGuiWindow_, 
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

    // Device information
    ImGui::Separator();
    ImGui::Text("🎤 Current Device:");
    std::string deviceName = "Default";
    if (selectedDevice_ >= 0 && selectedDevice_ < deviceNames_.size()) {
        deviceName = deviceNames_[selectedDevice_];
    }
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.8f, 1.0f), "%s", deviceName.c_str());
    
    // Control buttons
    ImGui::Spacing();
    if (ImGui::Button("🔧 Select Device")) {
        showDeviceSelector_ = !showDeviceSelector_;
    }
    ImGui::SameLine();
    if (ImGui::Button("📊 Diagnostics")) {
        showDiagnosticInfo_ = !showDiagnosticInfo_;
    }
    ImGui::SameLine();
    if (ImGui::Button("🖥️ Console Mode")) {
        showConsoleMode_ = !showConsoleMode_;
    }

    // Audio levels visualization
    ImGui::Separator();
    ImGui::Text("📊 Audio Levels:");
    
    // Calculate RMS
    float rms = 0.0f;
    for (float sample : waveformBuffer_) {
        rms += sample * sample;
    }
    rms = sqrtf(rms / waveformBuffer_.size());
    float db = rms > 0.0f ? 20.0f * log10f(rms) : -60.0f;
    db = std::max(-60.0f, db);
    
    // RMS meter
    ImGui::Text("RMS Level:");
    ImGui::SameLine();
    ImGui::ProgressBar(std::max(0.0f, (db + 60.0f) / 60.0f), ImVec2(200, 15), 
                       (std::to_string((int)db) + " dB").c_str());
    
    // Peak meter
    float peak = 0.0f;
    for (float sample : waveformBuffer_) {
        peak = std::max(peak, std::abs(sample));
    }
    ImGui::Text("Peak Level:");
    ImGui::SameLine();
    ImGui::ProgressBar(peak, ImVec2(200, 15), 
                       (std::to_string((int)(peak * 100)) + "%").c_str());

    // Frequency bars
    ImGui::Separator();
    ImGui::Text("🎵 Frequency Analysis:");
    
    ImGui::Text("Bass (20-120Hz):");
    ImGui::SameLine();
    ImGui::ProgressBar(audioFeatures_.bassEnergy, ImVec2(150, 15));
    
    ImGui::Text("Mid (120-2kHz):");
    ImGui::SameLine();
    ImGui::ProgressBar(audioFeatures_.midEnergy, ImVec2(150, 15));
    
    ImGui::Text("High (2k-12kHz):");
    ImGui::SameLine();
    ImGui::ProgressBar(audioFeatures_.highEnergy, ImVec2(150, 15));

    // Beat detection indicators
    ImGui::Separator();
    ImGui::Text("🥁 Beat Detection:");
    
    ImVec4 beatColor = audioFeatures_.beat > 0.5f ? ImVec4(1.0f, 0.0f, 0.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    ImGui::TextColored(beatColor, "Beat: %s", audioFeatures_.beat > 0.5f ? "🔴 DETECTED" : "⚪ none");
    
    ImVec4 onsetColor = audioFeatures_.onset > 0.5f ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    ImGui::TextColored(onsetColor, "Onset: %s", audioFeatures_.onset > 0.5f ? "🔴 DETECTED" : "⚪ none");

    // Status
    ImGui::Separator();
    ImVec4 statusColor = rms > 0.01f ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
    ImGui::TextColored(statusColor, "Status: %s", rms > 0.01f ? "🟢 RECEIVING AUDIO" : "🔴 NO AUDIO INPUT");

    // Instructions
    ImGui::Separator();
    ImGui::Text("🎮 Controls:");
    ImGui::BulletText("Click buttons to toggle panels");
    ImGui::BulletText("ESC to exit application");
    ImGui::BulletText("Close windows to hide panels");

    ImGui::End();
}

void Visualizer::renderDeviceSelectorImGui() {
    if (!showDeviceSelector_) return;
    
    ImGui::Begin("Device Selector", &showDeviceSelector_, 
                 ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("Select Audio Input Device:");
    ImGui::Separator();

    int displayIndex = 0;
    for (int i = 0; i < deviceNames_.size(); ++i) {
        if (!deviceNames_[i].empty()) {
            std::string label = std::to_string(displayIndex + 1) + ". " + deviceNames_[i];
            if (i == selectedDevice_) {
                label += " [CURRENT]";
            }
            
            if (ImGui::Selectable(label.c_str(), i == selectedDevice_)) {
                selectedDevice_ = i;
                showDeviceSelector_ = false;
            }
            displayIndex++;
        }
    }

    ImGui::Separator();
    ImGui::Text("Click a device to select it");
    ImGui::Text("The audio will restart with the new device");

    ImGui::End();
}

void Visualizer::renderDiagnosticImGui() {
    if (!showDiagnosticInfo_) return;
    
    ImGui::Begin("Audio Diagnostics", &showDiagnosticInfo_, 
                 ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("📊 Detailed Audio Analysis");
    ImGui::Separator();

    // Calculate metrics
    float rms = 0.0f;
    float peak = 0.0f;
    for (float sample : waveformBuffer_) {
        rms += sample * sample;
        peak = std::max(peak, std::abs(sample));
    }
    rms = sqrtf(rms / waveformBuffer_.size());
    float db = rms > 0.0f ? 20.0f * log10f(rms) : -60.0f;
    db = std::max(-60.0f, db);

    ImGui::Text("RMS Level: %.2f dB", db);
    ImGui::Text("Peak Level: %.6f", peak);
    ImGui::Text("Bass Energy: %.6f", audioFeatures_.bassEnergy);
    ImGui::Text("Mid Energy: %.6f", audioFeatures_.midEnergy);
    ImGui::Text("High Energy: %.6f", audioFeatures_.highEnergy);
    ImGui::Text("Total Energy: %.6f", audioFeatures_.energy);
    ImGui::Text("Onset Value: %.6f", audioFeatures_.onset);
    ImGui::Text("Beat Value: %.6f", audioFeatures_.beat);

    ImGui::Separator();
    ImGui::Text("Waveform Buffer Size: %zu samples", waveformBuffer_.size());
    ImGui::Text("Window Size: %dx%d", windowWidth_, windowHeight_);
    ImGui::Text("Time: %.2f seconds", time_);

    ImGui::End();
}

void Visualizer::renderConsoleImGui() {
    if (!showConsoleMode_) return;
    
    ImGui::Begin("Console Visualization", &showConsoleMode_, 
                 ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("🌊 ASCII Waveform Visualization");
    ImGui::Separator();

    // Simple ASCII waveform in ImGui
    int waveWidth = 40;
    int waveHeight = 10;
    
    for (int y = waveHeight - 1; y >= 0; --y) {
        std::string line = "";
        for (int x = 0; x < waveWidth; ++x) {
            int sampleIndex = (int)(x * waveformBuffer_.size() / waveWidth);
            float sample = waveformBuffer_[sampleIndex];
            int sampleY = (int)((sample + 1.0f) * waveHeight / 2.0f);
            sampleY = std::max(0, std::min(waveHeight - 1, sampleY));
            
            if (y == sampleY) {
                line += "█";
            } else if (y == waveHeight / 2) {
                line += "─";
            } else {
                line += " ";
            }
        }
        ImGui::Text("%s", line.c_str());
    }

    ImGui::Separator();
    ImGui::Text("Frequency Bars:");
    
    // ASCII frequency bars
    std::string bassBar = "Bass: ";
    int bassBars = (int)(audioFeatures_.bassEnergy * 20);
    for (int i = 0; i < bassBars; ++i) bassBar += "█";
    ImGui::Text("%s", bassBar.c_str());
    
    std::string midBar = "Mid:  ";
    int midBars = (int)(audioFeatures_.midEnergy * 20);
    for (int i = 0; i < midBars; ++i) midBar += "█";
    ImGui::Text("%s", midBar.c_str());
    
    std::string highBar = "High: ";
    int highBars = (int)(audioFeatures_.highEnergy * 20);
    for (int i = 0; i < highBars; ++i) highBar += "█";
    ImGui::Text("%s", highBar.c_str());

    ImGui::End();
}
