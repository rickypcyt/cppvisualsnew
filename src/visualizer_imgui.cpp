#include "visualizer.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>

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

    ImGui::TextWrapped("Selecciona un dispositivo interno (Loopback/Monitor) para capturar audio del sistema, por ejemplo la salida del navegador. En Windows busca entradas 'WASAPI (loopback)', en Linux 'Monitor', en macOS 'Loopback'.");

    ImGui::Spacing();
    ImGui::Text("🖥️ GPU Renderer:");
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", rendererName_.c_str());

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

    ImGui::Spacing();
    ImGui::Text("🎚️ Sensibilidad Visual");
    ImGui::SliderFloat("##LegacySensitivitySlider", &legacySensitivity_, 0.2f, 3.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);

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

    // Beat detection indicators
    ImGui::Separator();
    ImGui::Text("🥁 Beat Detection:");
    
    ImVec4 beatColor = audioFeatures_.beat > 0.5f ? ImVec4(1.0f, 0.0f, 0.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    ImGui::TextColored(beatColor, "Beat: %s", audioFeatures_.beat > 0.5f ? "🔴 DETECTED" : "⚪ none");
    
    ImVec4 onsetColor = audioFeatures_.onset > 0.5f ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    ImGui::TextColored(onsetColor, "Onset: %s", audioFeatures_.onset > 0.5f ? "🔴 DETECTED" : "⚪ none");

    float bpm = audioFeatures_.bpm;
    ImVec4 bpmColor = bpm > 0.1f ? ImVec4(0.2f, 0.8f, 1.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    ImGui::TextColored(bpmColor, "BPM Estimate: %s", bpm > 0.1f ? (std::to_string(static_cast<int>(std::round(bpm))) + " BPM").c_str() : "--");

    // Status
    ImGui::Separator();
    ImVec4 statusColor = rms > 0.01f ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
    ImGui::TextColored(statusColor, "Status: %s", rms > 0.01f ? "🟢 RECEIVING AUDIO" : "🔴 NO AUDIO INPUT");

    // Legacy color customization
    ImGui::Separator();
    ImGui::Text("🎨 Legacy Color Scheme:");
    ImGui::TextWrapped("Ajusta los multiplicadores de color para los elementos del render legacy.");

    if (ImGui::Button("Restablecer Colores")) {
        resetLegacyColorAdjustments();
    }
    ImGui::SameLine();
    if (ImGui::Button("Randomizar Ahora")) {
        randomizeLegacyColors();
    }

    ImGui::TextUnformatted("Motor visual:");
    ImGui::SameLine();
    ImGui::Checkbox("Shaders modernos", &useModernPipeline_);
    ImGui::SameLine();
    ImGui::Checkbox("Overlay legacy", &overlayLegacyOnModern_);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Superpone el render legacy encima del moderno");
    }

    if (useModernPipeline_) {
        if (ImGui::TreeNode("Capas núcleo shader")) {
            ImGui::Checkbox("Base", &coreShowBase_);
            ImGui::SameLine();
            ImGui::Checkbox("Corona", &coreShowCorona_);
            ImGui::SameLine();
            ImGui::Checkbox("Spokes", &coreShowSpokes_);

            ImGui::Checkbox("Runas", &coreShowRunes_);
            ImGui::SameLine();
            ImGui::Checkbox("Sparkles", &coreShowSparkles_);
            ImGui::SameLine();
            ImGui::Checkbox("Bloom", &coreShowBloom_);
            ImGui::TreePop();
        }

        ImGui::Checkbox("Chispas shader", &showShaderSparks_);
        ImGui::SameLine();
        ImGui::Checkbox("Orbes esquina", &showCornerOrbs_);

        if (ImGui::TreeNode("Capa procedural")) {
            ImGui::Checkbox("Mostrar", &showProceduralLayer_);
            ImGui::SameLine();
            ImGui::Checkbox("Debug preview", &proceduralLayerDebug_);
            ImGui::SliderFloat("Opacidad", &proceduralLayerOpacity_, 0.0f, 1.0f, "%.2f");

            static const char* kProceduralModes[] = {
                "Nebula",
                "ASCII Ocean",
                "Sacred Geometry",
                "Glitch Grid"
            };
            int modeIndex = std::clamp(proceduralLayerMode_, 0, static_cast<int>(std::size(kProceduralModes)) - 1);
            if (ImGui::BeginCombo("Modo", kProceduralModes[modeIndex])) {
                for (int i = 0; i < static_cast<int>(std::size(kProceduralModes)); ++i) {
                    bool selected = (proceduralLayerMode_ == i);
                    if (ImGui::Selectable(kProceduralModes[i], selected)) {
                        proceduralLayerMode_ = i;
                        proceduralLayer_.setMode(i);
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Post proceso")) {
            static const char* kModes[] = {
                "Off",
                "Grayscale",
                "Filmic",
                "Digital Wave",
                "Pulse Shift"
            };

            int currentMode = postProcessMode_;
            if (currentMode < 0 || currentMode >= static_cast<int>(std::size(kModes))) {
                currentMode = 0;
            }

            if (ImGui::BeginCombo("Modo", kModes[currentMode])) {
                for (int i = 0; i < static_cast<int>(std::size(kModes)); ++i) {
                    bool selected = (postProcessMode_ == i);
                    if (ImGui::Selectable(kModes[i], selected)) {
                        postProcessMode_ = i;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SliderFloat("Intensidad", &postProcessStrength_, 0.0f, 1.0f, "%.2f");
            ImGui::TreePop();
        }
    }

    ImGui::Checkbox("Random auto", &autoRandomizeColors_);
    ImGui::SameLine();
    ImGui::Checkbox("Cambiar colores cada 2 onsets", &onsetColorCyclingEnabled_);
    ImGui::SameLine();
    ImGui::Checkbox("Mezclar presets", &mixColorSchemes_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::SliderFloat("Intervalo (s)", &colorRandomInterval_, 1.0f, 60.0f)) {
        colorRandomInterval_ = std::max(1.0f, colorRandomInterval_);
    }

    ImGuiColorEditFlags colorFlags = ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float;
    if (ImGui::BeginTable("LegacyColorAdjustTable", 2, ImGuiTableFlags_SizingStretchProp)) {
        auto colorRow = [&](const char* label, Visualizer::ColorAdjust& adjust) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            ImGui::ColorEdit4(label, adjust.data(), colorFlags | ImGuiColorEditFlags_NoInputs);
        };

        colorRow("Circle Fill", legacyColorAdjust_.circleFill);
        colorRow("Circle Outline", legacyColorAdjust_.circleOutline);
        colorRow("Bloom Centro", legacyColorAdjust_.bloomInner);
        colorRow("Bloom Halo", legacyColorAdjust_.bloomOuter);
        colorRow("Barras Graves", legacyColorAdjust_.bassBars);
        colorRow("Barras Medios", legacyColorAdjust_.midBars);
        colorRow("Barras Agudos", legacyColorAdjust_.highBars);
        colorRow("Explosion Beat", legacyColorAdjust_.beatExplosion);
        colorRow("Anillos", legacyColorAdjust_.rings);
        colorRow("Órbitas", legacyColorAdjust_.orbit);
        colorRow("Trail Órbitas", legacyColorAdjust_.orbitTrail);
        colorRow("Chispas", legacyColorAdjust_.sparkles);
        colorRow("Waveform", legacyColorAdjust_.waveform);

        ImGui::EndTable();
    }

    // Instructions
    ImGui::Separator();
    ImGui::Text("�️ Visual Layers:");
    ImGui::Checkbox("Núcleo", &showLegacyCore_);
    ImGui::SameLine();
    ImGui::Checkbox("Arcos", &showLegacyArcs_);
    ImGui::SameLine();
    ImGui::Checkbox("Anillos", &showLegacyRings_);

    ImGui::Checkbox("Chispas", &showLegacySparkles_);
    ImGui::SameLine();
    ImGui::Checkbox("Órbitas", &showLegacyOrbs_);
    ImGui::SameLine();
    ImGui::Checkbox("Wormholes", &showLegacyWaveforms_);

    ImGui::Spacing();
    ImGui::Text("�� Controls:");
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
