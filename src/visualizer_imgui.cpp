#include "visualizer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <sstream>
#include <iomanip>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "shader_loader.h"

// Static mode name arrays - single source of truth
const char* const Visualizer::kProceduralModes[] = {
    "None",
    "ASCII Ocean",
    "Sacred Geometry",
    "Glitch Grid",
    "Chemical Flow",
    "Crystal Lattice",
    "Phantom Fractals",
    "Fractal Object",
    "Pulsar Tunnel",
    "Aurora Bloom",
    "Ribbon Scanlines",
    "Nebula",
    "Kaleidoscope Fractal",
    "Voronoi Cells",
    "Raymarched Object",
    "Reaction Diffusion",
    "Liquid Refraction",
    "Starfield Warp",
    "Plasma Classic",
    "Domain Warped Fractal",
    "Fractal Tunnel",
    "Volumetric Starfield",
    "Voxel Path Tracer",
    "Etienne Pulse",
    "Fractal Runway",
    "Volumetric Tunnel",
    "Chromatic Swirl",
    "Hyper Pulse",
    "Gyroid Reflections",
    "Head",
    "Metal Gyroid Hall",
    "Hex Kaleidoscope",
    "HSV Color Shift",
    "Crypt Roots",
    "Breathing",
    "Crystal Tetrahedron",
    "Evolution Noise",
    "Phi Fields",
    "Stage7",
    "Weird Creature"
};

const char* const Visualizer::kPostProcessModes[] = {
    "None",
    "Grayscale",
    "Filmic + Vignette",
    "CRT Monitor",
    "Chromatic Pulse",
    "Bass Threshold",
    "Radial Blur",
    "Kaleidoscope",
    "Digital Glitch",
    "Pixelate 64px",
    "Pixelate 128px", 
    "Pixelate 192px",
    "Pixelate 256px",
    "Lens Distortion",
    "Plasma Overlay",
    "RGB Split",
    "Recursive Energy",
    "Bloom + ACES",
    "Pixel Tiles",
    "Sobel Edge Detection",
    "Kaleidoscope Mirror",
    "Advanced Sobel",
    "Ring Distortion",
    "Random Cycle"
};

bool Visualizer::setupImGui() {
    if (!window_) {
        std::cerr << "ImGui initialization failed: window not created" << std::endl;
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.Alpha = 0.9f;

    if (!ImGui_ImplGlfw_InitForOpenGL(window_, true)) {
        std::cerr << "ImGui initialization failed: ImGui_ImplGlfw_InitForOpenGL" << std::endl;
        ImGui::DestroyContext();
        return false;
    }

    const char* glsl_version = "#version 130";
    if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
        std::cerr << "ImGui initialization failed: ImGui_ImplOpenGL3_Init" << std::endl;
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    showImGuiWindow_ = true;
    showDeviceSelector_ = false;
    showDiagnosticInfo_ = false;
    showConsoleMode_ = false;

    return true;
}

void Visualizer::shutdownImGui() {
    if (!ImGui::GetCurrentContext()) {
        return;
    }
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

    // Current effects display window
    if (showImGuiWindow_) {
        renderCurrentEffectsDisplay();
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
    ImGui::Begin("Info", &showImGuiWindow_, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::Text("🎤 Current Device:");
    std::string deviceName = "System Default";
    bool currentInternal = false;
    if (selectedDevice_ >= 0 && selectedDevice_ < deviceNames_.size()) {
        deviceName = deviceNames_[selectedDevice_];
        if (selectedDevice_ < deviceIsInternal_.size()) {
            currentInternal = deviceIsInternal_[selectedDevice_];
        }
    }
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.8f, 1.0f), "%s", deviceName.c_str());
    if (currentInternal) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "(Internal / Loopback)");
    }

    ImGui::Spacing();
    ImGui::Text("Input Source:");
    std::string comboLabel = currentInternal ? deviceName + "  (Internal)" : deviceName;
    if (ImGui::BeginCombo("##InputDeviceCombo", comboLabel.c_str())) {
        if (ImGui::Selectable("System Default", selectedDevice_ < 0)) {
            selectedDevice_ = -1;
            showDeviceSelector_ = false;
        }
        for (int i = 0; i < static_cast<int>(deviceNames_.size()); ++i) {
            if (deviceNames_[i].empty()) continue;
            std::string label = deviceNames_[i];
            if (i < deviceIsInternal_.size() && deviceIsInternal_[i]) {
                label += "  (Internal)";
            }
            bool isSelected = (i == selectedDevice_);
            if (ImGui::Selectable(label.c_str(), isSelected)) {
                selectedDevice_ = i;
                showDeviceSelector_ = false;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::TextWrapped("Select an internal device (Loopback/Monitor) to capture system audio, for example browser output. On Windows look for 'WASAPI (loopback)' entries, on Linux 'Monitor', on macOS 'Loopback'.");

    ImGui::Spacing();
    ImGui::Text("🖥️ GPU Renderer:");
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", rendererName_.c_str());
    ImGui::Text("OpenGL version:");
    ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.6f, 1.0f), "%s", openglVersion_.c_str());

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

    ImGui::Spacing();
    ImGui::Text("🎚️ Visual Sensitivity");
    ImGui::SliderFloat("##VisualSensitivitySlider", &visualSensitivity_, 0.2f, 3.0f, "%.2fx", ImGuiSliderFlags_AlwaysClamp);

    ImGui::Text("🎛️ Input Gain");
    ImGui::SliderFloat("##InputGainSlider", &audioInputGain_, 0.1f, 5.0f, "%.2fx", ImGuiSliderFlags_AlwaysClamp);

    ImGui::Separator();
    ImGui::Text("📊 Audio Levels:");

    float rms = 0.0f;
    for (float sample : waveformBuffer_) {
        rms += sample * sample;
    }
    rms = sqrtf(rms / waveformBuffer_.size());
    float db = rms > 0.0f ? 20.0f * log10f(rms) : -60.0f;
    db = std::max(-60.0f, db);

    ImGui::Text("RMS Level:");
    ImGui::SameLine();
    ImGui::ProgressBar(std::max(0.0f, (db + 60.0f) / 60.0f), ImVec2(200, 15), (std::to_string((int)db) + " dB").c_str());

    float peak = 0.0f;
    for (float sample : waveformBuffer_) {
        peak = std::max(peak, std::abs(sample));
    }
    ImGui::Text("Peak Level:");
    ImGui::SameLine();
    ImGui::ProgressBar(peak, ImVec2(200, 15), (std::to_string((int)(peak * 100)) + "%").c_str());

    ImGui::Separator();
    ImGui::Text("🎵 Frequency Analysis:");

    auto renderBandRow = [](const char* label,
                            float share,
                            float energy) {
        ImGui::Text("%s", label);
        ImGui::SameLine();
        float pct = share * 100.0f;
        std::string overlay = std::to_string(static_cast<int>(pct)) + "%";
        ImGui::ProgressBar(std::clamp(share, 0.0f, 1.0f), ImVec2(160, 15), overlay.c_str());
        ImGui::SameLine();
        ImGui::Text("energy %.2f", energy);
    };

    renderBandRow("Bass (20-120Hz):", audioFeatures_.bassShare, audioFeatures_.bassEnergy);
    renderBandRow("Mid (120-2kHz):", audioFeatures_.midShare, audioFeatures_.midEnergy);
    renderBandRow("High (2k-12kHz):", audioFeatures_.highShare, audioFeatures_.highEnergy);

    ImGui::Separator();
    ImGui::Text("🥁 Beat Detection:");

    ImVec4 beatColor = audioFeatures_.beat > 0.5f ? ImVec4(1.0f, 0.0f, 0.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    ImGui::TextColored(beatColor, "Beat: %s", audioFeatures_.beat > 0.5f ? "🔴 DETECTED" : "⚪ none");

    ImVec4 onsetColor = audioFeatures_.onset > 0.5f ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    ImGui::TextColored(onsetColor, "Onset: %s", audioFeatures_.onset > 0.5f ? "🔴 DETECTED" : "⚪ none");

    ImVec4 kickColor = audioFeatures_.kick > 0.5f ? ImVec4(0.9f, 0.3f, 0.1f, 1.0f) : ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    ImGui::TextColored(kickColor, "Kick: %s", audioFeatures_.kick > 0.5f ? "🟥 accent" : "⬜ idle");

    ImVec4 clapColor = audioFeatures_.clap > 0.5f ? ImVec4(0.95f, 0.6f, 0.2f, 1.0f) : ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    ImGui::TextColored(clapColor, "Clap: %s", audioFeatures_.clap > 0.5f ? "🟧 accent" : "⬜ idle");

    ImVec4 hiHatColor = audioFeatures_.hiHat > 0.5f ? ImVec4(0.5f, 0.8f, 1.0f, 1.0f) : ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    ImGui::TextColored(hiHatColor, "Hi-Hat: %s", audioFeatures_.hiHat > 0.5f ? "🟦 accent" : "⬜ idle");

    float bpm = audioFeatures_.bpm;
    ImVec4 bpmColor = bpm > 0.1f ? ImVec4(0.2f, 0.8f, 1.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    ImGui::TextColored(bpmColor, "BPM Estimate: %s", bpm > 0.1f ? (std::to_string(static_cast<int>(std::round(bpm))) + " BPM").c_str() : "--");

    ImGui::Separator();
    ImVec4 statusColor = rms > 0.01f ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
    ImGui::TextColored(statusColor, "Status: %s", rms > 0.01f ? "🟢 RECEIVING AUDIO" : "🔴 NO AUDIO INPUT");

    ImGui::End();

    ImGui::Begin("Visual", &showImGuiVisualWindow_, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::TextUnformatted("Motor visual:");
    ImGui::SameLine();
    ImGui::Checkbox("Shaders modernos", &useModernPipeline_);
    ImGui::SameLine();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Superpone el render legacy encima del moderno");
    }

    int paletteCount = static_cast<int>(scenePalettes_.size());
    const char* currentName = (currentScenePaletteIndex_ >= 0 && currentScenePaletteIndex_ < paletteCount)
                                ? scenePalettes_[currentScenePaletteIndex_].name.c_str()
                                : "Sin límites";

    ImGui::Text("🎨 Paleta moderna:");
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo("Preset", currentName)) {
        bool noneSelected = currentScenePaletteIndex_ < 0;
        if (ImGui::Selectable("Sin límites", noneSelected)) {
            currentScenePaletteIndex_ = -1;
            setDefaultScenePalette();
        }
        if (noneSelected) {
            ImGui::SetItemDefaultFocus();
        }
        for (int i = 0; i < paletteCount; ++i) {
            bool selected = (currentScenePaletteIndex_ == i);
            if (ImGui::Selectable(scenePalettes_[i].name.c_str(), selected)) {
                applyScenePalette(i);
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::SliderFloat("Blend", &scenePaletteBlend_, 0.0f, 1.0f, "%.2f")) {
        scenePaletteBlend_ = std::clamp(scenePaletteBlend_, 0.0f, 1.0f);
        proceduralLayer_.setColorPalette(scenePrimaryColor_.data(), sceneSecondaryColor_.data(), scenePaletteBlend_);
        saveCurrentSettings(); // Auto-save when blend changes
    }

    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::ColorEdit3("Primary", scenePrimaryColor_.data(), ImGuiColorEditFlags_Float)) {
        proceduralLayer_.setColorPalette(scenePrimaryColor_.data(), sceneSecondaryColor_.data(), scenePaletteBlend_);
        saveCurrentSettings(); // Auto-save when primary color changes
    }
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::ColorEdit3("Secondary", sceneSecondaryColor_.data(), ImGuiColorEditFlags_Float)) {
        proceduralLayer_.setColorPalette(scenePrimaryColor_.data(), sceneSecondaryColor_.data(), scenePaletteBlend_);
        saveCurrentSettings(); // Auto-save when secondary color changes
    }

    if (ImGui::Button("Restore preset")) {
        applyScenePalette(currentScenePaletteIndex_);
    }

    if (useModernPipeline_) {
        bool prevCornerOrbs = showCornerOrbs_;
        ImGui::Checkbox("Corner Orbs", &showCornerOrbs_);
        if (prevCornerOrbs != showCornerOrbs_) {
            saveCurrentSettings(); // Auto-save when corner orbs setting changes
        }

        if (ImGui::CollapsingHeader("Procedural Layer", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool prevShowLayer = showProceduralLayer_;
            bool prevDebug = proceduralLayerDebug_;
            ImGui::Checkbox("Show", &showProceduralLayer_);
            ImGui::SameLine();
            ImGui::Checkbox("Debug preview", &proceduralLayerDebug_);
            if (prevShowLayer != showProceduralLayer_ || prevDebug != proceduralLayerDebug_) {
                saveCurrentSettings(); // Auto-save when procedural layer settings change
            }
            float prevOpacity = proceduralLayerOpacity_;
            ImGui::SliderFloat("Opacity", &proceduralLayerOpacity_, 0.0f, 1.0f, "%.2f");
            if (prevOpacity != proceduralLayerOpacity_) {
                saveCurrentSettings(); // Auto-save when opacity changes
            }

            int modeIndex = std::clamp(proceduralLayerMode_, 0, static_cast<int>(std::size(kProceduralModes)) - 1);
            if (ImGui::BeginCombo("Mode", kProceduralModes[modeIndex])) {
                for (int i = 0; i < static_cast<int>(std::size(kProceduralModes)); ++i) {
                    bool selected = (proceduralLayerMode_ == i);
                    if (ImGui::Selectable(kProceduralModes[i], selected)) {
                        proceduralLayerMode_ = i;
                        proceduralLayer_.setMode(i);
                        saveCurrentSettings(); // Auto-save when procedural mode changes
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

        }
    }

    if (ImGui::CollapsingHeader("Post Processing", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool anySlotEnabled = false;
        for (int slotIndex = 0; slotIndex < kMaxPostProcessSlots; ++slotIndex) {
            auto& slot = postProcessSlots_[slotIndex];

            ImGui::PushID(slotIndex);
            if (ImGui::CollapsingHeader(slotIndex == 0 ? "Slot 1 (Main)" :
                               slotIndex == 1 ? "Slot 2" :
                               slotIndex == 2 ? "Slot 3" :
                               slotIndex == 3 ? "Slot 4" : "Slot 5", 
                               slotIndex == 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {

                bool prevEnabled = slot.enabled;
                ImGui::Checkbox("Enable", &slot.enabled);
                anySlotEnabled = anySlotEnabled || slot.enabled;
                if (prevEnabled != slot.enabled) {
                    saveCurrentSettings(); // Auto-save when slot enabled/disabled
                }

                if (slot.enabled) {
                    int currentMode = slot.mode;
                    if (currentMode < 0 || currentMode >= static_cast<int>(std::size(kPostProcessModes))) {
                        currentMode = 0;
                    }

                    if (ImGui::BeginCombo("Mode", kPostProcessModes[currentMode])) {
                        for (int i = 0; i < static_cast<int>(std::size(kPostProcessModes)); ++i) {
                            bool selected = (slot.mode == i);
                            if (ImGui::Selectable(kPostProcessModes[i], selected)) {
                                slot.mode = i;
                                saveCurrentSettings(); // Auto-save when mode changes
                            }
                            if (selected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }

                    float prevStrength = slot.strength;
                    ImGui::SliderFloat("Intensity", &slot.strength, 0.0f, 1.0f, "%.2f");
                    if (prevStrength != slot.strength) {
                        saveCurrentSettings(); // Auto-save when intensity changes
                    }

                    if (slot.mode == 15) {
                        ImGui::SetNextItemWidth(180.0f);
                        ImGui::SliderFloat("R Channel", &slot.rgbAdjust[0], 0.0f, 1.5f, "%.2f");
                        ImGui::SetNextItemWidth(180.0f);
                        ImGui::SliderFloat("G Channel", &slot.rgbAdjust[1], 0.0f, 1.5f, "%.2f");
                        ImGui::SetNextItemWidth(180.0f);
                        ImGui::SliderFloat("B Channel", &slot.rgbAdjust[2], 0.0f, 1.5f, "%.2f");
                        saveCurrentSettings(); // Auto-save when RGB adjust changes
                        ImGui::TextDisabled("Adjust how much each channel shifts in the split.");
                    }
                }
            }
            ImGui::PopID();
        }
    }

    if (ImGui::CollapsingHeader("External RGB Channels", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool prevAutoRandomizeRgb = autoRandomizeRgbChannels_;
        ImGui::Checkbox("Music-based randomization", &autoRandomizeRgbChannels_);
        if (prevAutoRandomizeRgb != autoRandomizeRgbChannels_) {
            saveCurrentSettings(); // Auto-save when randomization setting changes
        }
        if (autoRandomizeRgbChannels_) {
            ImGui::SameLine();
            ImGui::TextDisabled("(changes every 3 onsets)");
        }
        
        ImGui::Spacing();
        static const char* kLabels[3] = {"Red", "Green", "Blue"};
        for (int i = 0; i < 3; ++i) {
            bool prevChannelState = rgbChannelEnabled_[i];
            ImGui::Checkbox(kLabels[i], &rgbChannelEnabled_[i]);
            if (prevChannelState != rgbChannelEnabled_[i]) {
                saveCurrentSettings(); // Auto-save when RGB channel toggled
            }
            if (i < 2) {
                ImGui::SameLine();
            }
        }

        ImGui::Spacing();
        if (ImGui::Button("Enable All")) {
            rgbChannelEnabled_[0] = rgbChannelEnabled_[1] = rgbChannelEnabled_[2] = true;
            saveCurrentSettings(); // Auto-save when RGB channels changed
        }
        ImGui::SameLine();
        if (ImGui::Button("Disable All")) {
            rgbChannelEnabled_[0] = rgbChannelEnabled_[1] = rgbChannelEnabled_[2] = false;
            saveCurrentSettings(); // Auto-save when RGB channels changed
        }
        ImGui::SameLine();
        if (ImGui::Button("Solo B")) {
            rgbChannelEnabled_[0] = false;
            rgbChannelEnabled_[1] = false;
            rgbChannelEnabled_[2] = true;
            saveCurrentSettings(); // Auto-save when RGB channels changed
        }

    }

    bool prevAutoRandomize = autoRandomizeColors_;
    ImGui::Checkbox("Random auto", &autoRandomizeColors_);
    if (prevAutoRandomize != autoRandomizeColors_) {
        saveCurrentSettings(); // Auto-save when auto-randomize changes
    }
    ImGui::SameLine();
    bool prevOnsetCycling = onsetColorCyclingEnabled_;
    ImGui::Checkbox("Change colors every 2 onsets", &onsetColorCyclingEnabled_);
    if (prevOnsetCycling != onsetColorCyclingEnabled_) {
        saveCurrentSettings(); // Auto-save when onset cycling changes
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::SliderFloat("Interval (s)", &colorRandomInterval_, 1.0f, 60.0f)) {
        colorRandomInterval_ = std::max(1.0f, colorRandomInterval_);
        saveCurrentSettings(); // Auto-save when interval changes
    }

    ImGui::Spacing();
    ImGui::Text("🎲 Random Post Process:");
    ImGui::TextDisabled("Randomizes Slot 1 effect automatically");
    bool prevRandomEnabled = randomPostProcessEnabled_;
    ImGui::Checkbox("Enable Random Cycle", &randomPostProcessEnabled_);
    if (prevRandomEnabled != randomPostProcessEnabled_) {
        saveCurrentSettings(); // Auto-save when random post process changes
        if (randomPostProcessEnabled_) {
            selectRandomPostProcess(); // Select initial random mode
        }
    }
    
    if (randomPostProcessEnabled_) {
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::SliderFloat("Change Interval (s)", &randomPostProcessInterval_, 1.0f, 30.0f)) {
            randomPostProcessInterval_ = std::max(1.0f, randomPostProcessInterval_);
            saveCurrentSettings(); // Auto-save when interval changes
        }
        
        // Show current random mode for Slot 1
        if (kMaxPostProcessSlots > 0 && currentRandomPostProcess_ >= 0 && currentRandomPostProcess_ < static_cast<int>(std::size(kPostProcessModes))) {
            ImGui::SameLine();
            ImGui::TextDisabled("(Slot 1: %s)", kPostProcessModes[currentRandomPostProcess_]);
        }
        
        // Button to force change
        if (ImGui::Button("Change Now")) {
            selectRandomPostProcess();
            saveCurrentSettings();
        }
    }

    ImGuiColorEditFlags colorFlags = ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float;

    bool prevCornerOrbs2 = showCornerOrbs_;
    bool prevProceduralLayer = showProceduralLayer_;
    ImGui::Text("🎞️ Visual Layers:");
    ImGui::Checkbox("Corner Orbs", &showCornerOrbs_);
    ImGui::SameLine();
    ImGui::Checkbox("Procedural Layer", &showProceduralLayer_);
    if (prevCornerOrbs2 != showCornerOrbs_ || prevProceduralLayer != showProceduralLayer_) {
        saveCurrentSettings(); // Auto-save when layer toggles change
    }

    ImGui::Spacing();
    ImGui::Text("🎮 Controls:");
    ImGui::BulletText("Flechas ←/→: cambiar modo procedural");
    ImGui::BulletText("ESC: salir de la aplicación");
    ImGui::BulletText("TAB: mostrar/ocultar paneles");

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
            if (i < deviceIsInternal_.size() && deviceIsInternal_[i]) {
                label += "  (Internal)";
            }
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

void Visualizer::renderCurrentEffectsDisplay() {
    if (!showImGuiWindow_) return;
    
    // Create floating window to show current effects
    ImGui::Begin("Current Effects", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration);
    
    // Title with style
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 1.0f, 1.0f));
    ImGui::Text("🎭 CURRENT EFFECTS");
    ImGui::PopStyleColor();
    ImGui::Separator();
    
    // Show current procedural layer
    ImGui::Text("📐 Procedural Layer:");
    ImGui::SameLine();
    
    const char* proceduralModeName = "None";
    if (proceduralLayerMode_ >= 0 && proceduralLayerMode_ < static_cast<int>(std::size(kProceduralModes))) {
        proceduralModeName = kProceduralModes[proceduralLayerMode_];
    }
    
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.8f, 1.0f));
    ImGui::Text("%s", proceduralModeName);
    ImGui::PopStyleColor();
    
    if (showProceduralLayer_) {
        ImGui::SameLine();
        ImGui::TextDisabled("(✓)");
    } else {
        ImGui::SameLine();
        ImGui::TextDisabled("(✗)");
    }
    
    ImGui::Spacing();
    
    // Show active post-processing effects
    ImGui::Text("🎨 Post-Processing:");
    
    bool anyActive = false;
    for (int slotIndex = 0; slotIndex < kMaxPostProcessSlots; ++slotIndex) {
        const auto& slot = postProcessSlots_[slotIndex];
        if (slot.enabled && slot.mode > 0) {
            anyActive = true;
            
            ImGui::Indent();
            ImGui::Text("Slot %d:", slotIndex + 1);
            ImGui::SameLine();
            
            const char* postProcessModeName = "None";
            if (slot.mode >= 0 && slot.mode < static_cast<int>(std::size(kPostProcessModes))) {
                if (slot.mode < static_cast<int>(std::size(kPostProcessModes))) {
                    postProcessModeName = kPostProcessModes[slot.mode];
                }
            }
            
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.4f, 1.0f));
            ImGui::Text("%s", postProcessModeName);
            ImGui::PopStyleColor();
            
            ImGui::SameLine();
            ImGui::TextDisabled("(%.2f)", slot.strength);
            ImGui::Unindent();
        }
    }
    
    if (!anyActive) {
        ImGui::Indent();
        ImGui::TextDisabled("No active effects");
        ImGui::Unindent();
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    
    // Keyboard shortcuts
    ImGui::Text("⌨️ Shortcuts:");
    ImGui::TextDisabled("I - Show/Hide this window");
    ImGui::TextDisabled("P - Advance post-processing");
    ImGui::TextDisabled("O - Rewind post-processing");
    ImGui::TextDisabled("1 - Toggle corner orbs");
    
    ImGui::End();
}
