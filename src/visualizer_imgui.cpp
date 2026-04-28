#include "visualizer.h"
#include "imgui.h"
#include "profiler.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <sstream>
#include <iomanip>

#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "shader_loader.h"

// Structure to hold effect info for ImGui with proper index mapping
struct EffectListForImGui {
    std::vector<std::string> names;      // Display names for ImGui
    std::vector<int> modeIndices;        // Actual mode indices (can be sparse)
    std::vector<const char*> ptrs;       // Pointers for ImGui
    std::vector<bool> enabledStates;     // Enabled state for each effect
    
    void rebuild(const Visualizer* visualizer = nullptr) {
        auto& registry = GetEffectRegistry();
        names.clear();
        modeIndices.clear();
        ptrs.clear();
        enabledStates.clear();
        
        // Always add "None" at index 0 with mode 0
        names.push_back("None");
        modeIndices.push_back(0);
        enabledStates.push_back(true);
        
        if (!registry.empty()) {
            auto effects = registry.getAllEffects();
            for (const auto& effect : effects) {
                // Check if this shader is enabled (default to true)
                bool enabled = true;
                if (visualizer && effect.modeIndex > 0) {
                    enabled = visualizer->isProceduralShaderEnabled(effect.modeIndex);
                }
                names.push_back(effect.name);
                modeIndices.push_back(effect.modeIndex);
                enabledStates.push_back(enabled);
            }
        }
        
        // Build pointer array
        ptrs.clear();
        for (const auto& name : names) {
            ptrs.push_back(name.c_str());
        }
    }
    
    // Find UI index for a given mode index
    int findUiIndex(int modeIndex) const {
        for (size_t i = 0; i < modeIndices.size(); ++i) {
            if (modeIndices[i] == modeIndex) {
                return static_cast<int>(i);
            }
        }
        return 0; // Default to None
    }
    
    // Get mode index from UI index
    int getModeIndex(int uiIndex) const {
        if (uiIndex >= 0 && uiIndex < static_cast<int>(modeIndices.size())) {
            return modeIndices[uiIndex];
        }
        return 0;
    }
    
    size_t size() const { return names.size(); }
};

// Helper to get effect names from registry for ImGui
static EffectListForImGui& GetEffectListForImGui() {
    static EffectListForImGui list;
    static bool initialized = false;
    
    auto& registry = GetEffectRegistry();
    if (!initialized || (registry.empty() == false && list.size() <= 1)) {
        // Pass nullptr on first init; will be rebuilt later with visualizer
        list.rebuild(nullptr);
        initialized = true;
    }
    
    return list;
}

// Helper to get effect list filtered by enabled state
static EffectListForImGui& GetEffectListForImGui(const Visualizer* visualizer) {
    static EffectListForImGui list;
    
    auto& registry = GetEffectRegistry();
    if (!registry.empty()) {
        list.rebuild(visualizer);
    }
    
    return list;
}



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
    "Rotating Lens",
    "Plasma Overlay",
    "RGB Shift",
    "Recursive Energy",
    "Bloom + ACES",
    "Pixel Tiles",
    "Sobel Edge Detection",
    "Kaleidoscope Mirror",
    "Advanced Sobel",
    "Ring Distortion",
    "Mirror Horizontal",
    "Mirror Vertical", 
    "Mirror Kaleidoscope",
    "Mirror Rorschach",
    "Posterize + Edge",
    "Little Planet",
    "Recursive Feedback",
    "Threshold Levels"
};

bool Visualizer::setupImGui() {
    // Use the separate ImGui controls window if available, otherwise fall back to main window
    GLFWwindow* imguiTargetWindow = imguiWindow_ ? imguiWindow_ : window_;

    if (!imguiTargetWindow) {
        std::cerr << "ImGui initialization failed: no window available" << std::endl;
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    
    // Multi-window cursor handling for Wayland/Hyprland
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.Alpha = 0.9f;

    // Initialize ImGui with callbacks - we'll chain our scroll handling
    if (!ImGui_ImplGlfw_InitForOpenGL(imguiTargetWindow, true)) {
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
    showImGuiColorsWindow_ = true;
    showImGuiProceduralWindow_ = true;
    showImGuiPostProcessWindow_ = true;
    showImGuiCameraWindow_ = true;  // Camera window always visible

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

void Visualizer::handleKeyboardInput() {
    // Check for 'D' key toggle
    if (isKeyPressed(GLFW_KEY_D)) {
        static double lastPress = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastPress > 0.5) { // 500ms debounce
            showImGuiWindow_ = !showImGuiWindow_;
            saveCurrentSettings();
            std::cout << "ImGui window toggled via D key: " << (showImGuiWindow_ ? "SHOWN" : "HIDDEN") << std::endl;
            lastPress = currentTime;
        }
    }

    // Check for 'I' key toggle for diagnostic mode
    if (isKeyPressed(GLFW_KEY_I)) {
        static double lastPress = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastPress > 0.5) { // 500ms debounce
            showCurrentEffects_ = !showCurrentEffects_;
            saveCurrentSettings();
            std::cout << "Current effects display toggled via I key: " << (showCurrentEffects_ ? "SHOWN" : "HIDDEN") << std::endl;
            lastPress = currentTime;
        }
    }

    // Check for 'C' key toggle for console mode
    if (isKeyPressed(GLFW_KEY_C)) {
        static double lastPress = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastPress > 0.5) { // 500ms debounce
            proceduralLayerDebug_ = !proceduralLayerDebug_;
            saveCurrentSettings();
            std::cout << "Procedural layer debug toggled via C key: " << (proceduralLayerDebug_ ? "ENABLED" : "DISABLED") << std::endl;
            lastPress = currentTime;
        }
    }

    // Check for 'O' key for post-processing effects when device menu is closed
    bool oKeyPressed = isKeyPressed(GLFW_KEY_O);
    bool deviceMenuOpen = showDeviceMenu_;
    
    // Debug: Show device menu state and O key detection
    static bool lastOKeyState = false;
    bool currentOKeyState = oKeyPressed;
    if (currentOKeyState != lastOKeyState) {
        std::cout << "O key detected: " << (currentOKeyState ? "PRESSED" : "RELEASED") 
                  << " | Device menu: " << (deviceMenuOpen ? "OPEN" : "CLOSED") << std::endl;
        lastOKeyState = currentOKeyState;
    }
    
    if (!deviceMenuOpen && oKeyPressed) {
        static double lastOPress = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastOPress > 0.5) { // 500ms debounce
            // Enable slot 1 (index 0) and cycle its mode backward
            auto& slot = postProcessSlots_[0];
            slot.enabled = true;
            slot.mode = (slot.mode - 1 + kPostProcessModeCount) % kPostProcessModeCount;
            if (slot.mode == 0) {
                slot.mode = kPostProcessModeCount - 1; // Skip mode 0 (no effect)
            }
            slot.strength = 1.0f;
            slot.rgbAdjust = {1.0f, 1.0f, 1.0f};
            std::cout << "Post-processing effect changed via O key: " << slot.mode << std::endl;
            lastOPress = currentTime;
        }
    }

    // Check for 'X' key toggle for corner orbs when device menu is closed
    bool xKeyPressed = isKeyPressed(GLFW_KEY_X);
    
    // Debug: Show X key detection
    static bool lastXKeyState = false;
    bool currentXKeyState = xKeyPressed;
    if (currentXKeyState != lastXKeyState) {
        std::cout << "X key detected: " << (currentXKeyState ? "PRESSED" : "RELEASED") 
                  << " | Device menu: " << (deviceMenuOpen ? "OPEN" : "CLOSED") << std::endl;
        lastXKeyState = currentXKeyState;
    }
    
    if (xKeyPressed) {
        static double lastXPress = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastXPress > 0.5) { // 500ms debounce
            showCornerOrbs_ = !showCornerOrbs_;
            saveCurrentSettings(); // Save like ImGui does
            std::cout << "Corner Orbs toggled via X key: " << (showCornerOrbs_ ? "ENABLED" : "DISABLED") << std::endl;
            lastXPress = currentTime;
        }
    }

    // Check for 'R' key to reload post-processing shaders
    if (isKeyPressed(GLFW_KEY_R)) {
        static double lastPress = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastPress > 0.5) { // 500ms debounce
            postProcessor_.reloadShaders();
            std::cout << "Post-processing shaders reloaded via R key" << std::endl;
            lastPress = currentTime;
        }
    }

    // Check for 'K' key to activate kaleidoscope mode
    if (isKeyPressed(GLFW_KEY_K)) {
        static double lastPress = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastPress > 0.5) { // 500ms debounce
            applyMainProceduralMode(29); // kKaleidoscopeModeIndex
            std::cout << "Kaleidoscope mode activated via K key" << std::endl;
            lastPress = currentTime;
        }
    }

    // Check for 'M' key to toggle multi-monitor mode
    if (isKeyPressed(GLFW_KEY_M)) {
        static double lastPress = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastPress > 0.5) { // 500ms debounce
            toggleMultiMonitorMode();
            std::cout << "Multi-monitor mode toggled via M key" << std::endl;
            lastPress = currentTime;
        }
    }

    // Check for F11 to toggle fullscreen on BOTH windows simultaneously
    if (isKeyPressed(GLFW_KEY_F11)) {
        static double lastF11Press = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastF11Press > 0.5) { // 500ms debounce
            toggleBothWindowsFullscreen();
            lastF11Press = currentTime;
        }
    }

    // Check for F10 to toggle ImGui window fullscreen
    if (isKeyPressed(GLFW_KEY_F10)) {
        static double lastF10Press = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastF10Press > 0.5) { // 500ms debounce
            toggleImGuiWindowFullscreen();
            lastF10Press = currentTime;
        }
    }

    // Check for F9 to toggle only the main rendering window fullscreen
    if (isKeyPressed(GLFW_KEY_F9)) {
        static double lastF9Press = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastF9Press > 0.5) { // 500ms debounce
            toggleMainWindowFullscreen();
            lastF9Press = currentTime;
        }
    }
}

void Visualizer::renderImGui() {
    PROFILE_FUNCTION();

    // Detect fullscreen mode on main window
    bool mainWindowFullscreen = false;
    if (window_) {
        GLFWmonitor* monitor = glfwGetWindowMonitor(window_);
        mainWindowFullscreen = (monitor != nullptr);
    }

    // Allow controls window even while main window is fullscreen if we are in multi-monitor mode
    // OR if user manually toggled fullscreen (we detect this by checking if imgui window is also fullscreen)
    bool imguiFullscreen = isImGuiWindowFullscreen();
    bool allowControlsDuringFullscreen = multiMonitorMode_ || monitors_.size() >= 2 || imguiFullscreen;

    const bool blockControlsForFullscreen = mainWindowFullscreen && !allowControlsDuringFullscreen;

    // If main window is fullscreen and we are not explicitly allowed to show controls, hide the ImGui window
    bool shouldHideControls = imguiWindow_ && blockControlsForFullscreen;
    if (shouldHideControls) {
        if (glfwGetWindowAttrib(imguiWindow_, GLFW_VISIBLE)) {
            glfwHideWindow(imguiWindow_);
            imguiWindowNeedsFocus_ = true; // Focus next time we show it
        }
    } else if (imguiWindow_) {
        if (!glfwGetWindowAttrib(imguiWindow_, GLFW_VISIBLE)) {
            glfwShowWindow(imguiWindow_);
            imguiWindowNeedsFocus_ = true;
        }
    }

    const bool controlsVisible = imguiWindow_ && glfwGetWindowAttrib(imguiWindow_, GLFW_VISIBLE);
    const bool controlsIconified = imguiWindow_ && glfwGetWindowAttrib(imguiWindow_, GLFW_ICONIFIED);
    const bool controlsFullscreen = isImGuiWindowFullscreen();
    bool renderControlsWindow = controlsVisible && !controlsIconified && !controlsFullscreen && (!blockControlsForFullscreen);

    // If we have a separate ImGui window and are allowed to render it, switch to it
    if (renderControlsWindow) {
        glfwMakeContextCurrent(imguiWindow_);

        // Only force focus when the window was previously hidden or flagged
        if (imguiWindowNeedsFocus_) {
            glfwFocusWindow(imguiWindow_);
            imguiWindowNeedsFocus_ = false;
        }

        // Ensure cursor mode remains free (Hyprland can latch to hidden windows)
        glfwSetInputMode(imguiWindow_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        // Get framebuffer size for the ImGui window
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(imguiWindow_, &fbWidth, &fbHeight);

        // Set viewport and clear
        glViewport(0, 0, fbWidth, fbHeight);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);  // Dark background for controls window
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // Always keep main window cursor unlocked as well
    if (window_) {
        glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    // Start the Dear ImGui frame
    {
        PROFILE_SCOPE("imgui_new_frame");
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }
    
    // Ensure cursor is properly updated
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) {
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    }

    // Handle keyboard input
    handleKeyboardInput();

    if (!blockControlsForFullscreen) {
        // Main control window
        if (showImGuiWindow_) {
            PROFILE_SCOPE("render_main_imgui_window");
            renderMainImGuiWindow();
        }

        // Separate Visual control windows
        if (showImGuiColorsWindow_) {
            PROFILE_SCOPE("render_colors_window");
            renderColorsWindow();
        }
        if (showImGuiProceduralWindow_) {
            PROFILE_SCOPE("render_procedural_window");
            renderProceduralWindow();
        }
        if (showImGuiPostProcessWindow_) {
            PROFILE_SCOPE("render_post_process_window");
            renderPostProcessWindow();
        }

        // Camera control window - always visible
        {
            PROFILE_SCOPE("render_camera_window");
            renderCameraWindow();
        }

        // Current effects display window
        if (showCurrentEffects_) {
            PROFILE_SCOPE("render_current_effects");
            renderCurrentEffectsDisplay();
        }

        // Render MIDI controls
        {
            PROFILE_SCOPE("render_midi_controls");
            renderMIDIControls();
        }

        // Device selector window
        if (showDeviceSelector_) {
            PROFILE_SCOPE("render_device_selector");
            renderDeviceSelectorImGui();
        }

        // Diagnostic info window
        if (showDiagnosticInfo_) {
            PROFILE_SCOPE("render_diagnostic");
            renderDiagnosticImGui();
        }

        // Console mode window
        if (showConsoleMode_) {
            PROFILE_SCOPE("render_console");
            renderConsoleImGui();
        }

        // FASE 0.4: Performance window
        if (showPerformanceWindow_) {
            PROFILE_SCOPE("render_performance");
            renderPerformanceImGui();
        }

        // Shader Presets window
        {
            PROFILE_SCOPE("render_shader_presets");
            renderShaderPresetsWindow();
        }

        // Render ImGui
        {
            PROFILE_SCOPE("imgui_render");
            ImGui::Render();
        }
        {
            PROFILE_SCOPE("imgui_render_draw_data");
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
    } else {
        // Skip drawing overlays entirely so fullscreen stays clean
        ImGui::EndFrame();
    }

    // Swap buffers for ImGui window if it exists and not fullscreen
    // (when fullscreen, we render ImGui as overlay on main window)
    if (!blockControlsForFullscreen && renderControlsWindow) {
        glfwSwapBuffers(imguiWindow_);

        // Return context to main window
        glfwMakeContextCurrent(window_);
    } else if (window_) {
        // Ensure we leave context on main window when controls window is hidden/offscreen
        glfwMakeContextCurrent(window_);
    }
}

void Visualizer::renderMainImGuiWindow() {
    ImGui::Begin("Info", &showImGuiWindow_, ImGuiWindowFlags_AlwaysAutoResize);

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

    // Multi-monitor setup section
    ImGui::Spacing();
    
    // Refresh monitors button
    if (ImGui::Button("🔄 Refresh Monitors")) {
        detectMonitors();
        std::cout << "[MONITOR] Monitors refreshed manually" << std::endl;
    }
    
    ImGui::Spacing();
    renderMonitorSelector();
    
    if (ImGui::Button("🔄 Reload Shaders")) {
        reloadProceduralShaders();
    }
    ImGui::SameLine();
    if (ImGui::Button("🔄 Reload Post FX")) {
        postProcessor_.reloadShaders();
        std::cout << "Post-processing shaders reloaded via button" << std::endl;
    }
    ImGui::SameLine();
    bool hotReloadPP = postProcessor_.isHotReloadEnabled();
    if (ImGui::Checkbox("Hot-Reload PP", &hotReloadPP)) {
        postProcessor_.enableHotReload(hotReloadPP);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Auto-reload post-processing shaders when files change");
    }

    ImGui::Spacing();
    if (ImGui::Checkbox("Show Performance Metrics", &showPerformanceWindow_)) {
        // FASE 0.4: Toggle performance window
    }

    ImGui::Spacing();
    ImGui::Text("🎚️ Visual Sensitivity");
    ImGui::SliderFloat("##VisualSensitivitySlider", &visualSensitivity_, 0.2f, 3.0f, "%.2fx", ImGuiSliderFlags_AlwaysClamp);

    ImGui::Text("🎛️ Input Gain");
    ImGui::SliderFloat("##InputGainSlider", &audioInputGain_, 0.1f, 5.0f, "%.2fx", ImGuiSliderFlags_AlwaysClamp);

    ImGui::Spacing();
    bool prevAudioEngineEnabled = audioEngineEnabled_;
    ImGui::Checkbox("Audio Engine", &audioEngineEnabled_);
    if (prevAudioEngineEnabled != audioEngineEnabled_) {
        saveCurrentSettings();
    }
    if (!audioEngineEnabled_) {
        ImGui::SameLine();
        ImGui::TextDisabled("(capture and analysis off)");
    }
    
    ImGui::Spacing();
    ImGui::Text("🎵 Manual BPM Mode");
    bool prevManualBPMMode = manualBPMMode_;
    ImGui::Checkbox("Enable Manual BPM", &manualBPMMode_);
    if (prevManualBPMMode != manualBPMMode_) {
        saveCurrentSettings(); // Auto-save when BPM mode changes
    }
    
    if (manualBPMMode_) {
        ImGui::Indent();
        float prevBPMValue = manualBPM_;
        ImGui::SliderFloat("##ManualBPMSlider", &manualBPM_, 5.0f, 200.0f, "%.0f BPM", ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::IsItemDeactivatedAfterEdit() && manualBPM_ != prevBPMValue) {
            saveCurrentSettings(); // Auto-save when BPM changes (after editing finishes)
        }
        ImGui::Unindent();
    }

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

    // === MEL-SCALE FREQUENCY BANDS (12 bands) ===
    if (ImGui::CollapsingHeader("🎵 Mel-Scale Bands (12 bands)")) {
        ImGui::Indent();
        const char* melBandNames[12] = {
            "Sub-Bass", "Bass", "Low-Mid", "Mid", "High-Mid",
            "Presence", "Brilliance 1", "Brilliance 2", "Air 1", "Air 2", "Air 3", "Air 4"
        };
        
        // Debug info
        ImGui::TextDisabled("Total Mel Energy: %.6f", 
            std::accumulate(audioFeatures_.melBandEnergies.begin(), 
                           audioFeatures_.melBandEnergies.end(), 0.0f));
        ImGui::Separator();
        
        for (int i = 0; i < 12; ++i) {
            float share = audioFeatures_.melBandShares[i];
            float energy = audioFeatures_.melBandEnergies[i];
            ImGui::Text("%s:", melBandNames[i]);
            ImGui::SameLine();
            ImGui::ProgressBar(std::clamp(share, 0.0f, 1.0f), ImVec2(100, 15), 
                (std::to_string(static_cast<int>(share * 100)) + "%").c_str());
            ImGui::SameLine();
            // Show energy with more precision and scientific notation for small values
            if (energy < 0.001f && energy > 0.0f) {
                ImGui::TextDisabled("%.2e", energy);
            } else {
                ImGui::TextDisabled("%.4f", energy);
            }
        }
        ImGui::Unindent();
    }

    // === SPECTRAL FEATURES ===
    if (ImGui::CollapsingHeader("📊 Spectral Features")) {
        ImGui::Indent();
        ImGui::Text("Spectral Flux: %.4f", audioFeatures_.spectralFlux);
        ImGui::Text("Zero-Crossing Rate: %.4f", audioFeatures_.zeroCrossingRate);
        ImGui::Text("Spectral Centroid: %.1f Hz", audioFeatures_.spectralCentroid);
        ImGui::Text("Spectral Rolloff: %.1f Hz", audioFeatures_.spectralRolloff);
        ImGui::Text("Percussion/Tonal: %.2f (0=tonal, 1=percussion)", audioFeatures_.percussionTonalRatio);
        ImGui::Unindent();
    }

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

    // Beat Detection Tuning (only if analyzer connected)
    if (audioAnalyzer_ && ImGui::CollapsingHeader("Beat Detection Tuning")) {
        ImGui::Indent();
        
        // Onset threshold
        float onsetThresh = audioAnalyzer_->getOnsetThreshold();
        ImGui::Text("Onset Threshold:");
        if (ImGui::SliderFloat("##onset_thresh", &onsetThresh, 1.0f, 3.0f, "%.2f")) {
            audioAnalyzer_->setOnsetThreshold(onsetThresh);
        }
        
        ImGui::Spacing();
        ImGui::Text("Kick Detection:");
        float kickThresh = audioAnalyzer_->getKickThresholdMultiplier();
        ImGui::SetNextItemWidth(120);
        if (ImGui::SliderFloat("Threshold##kick_thresh", &kickThresh, 0.5f, 5.0f, "%.2f")) {
            audioAnalyzer_->setKickThresholdMultiplier(kickThresh);
        }
        ImGui::SameLine();
        float kickInterval = audioAnalyzer_->getKickMinInterval();
        ImGui::SetNextItemWidth(120);
        if (ImGui::SliderFloat("Min Interval##kick_int", &kickInterval, 0.02f, 0.3f, "%.3fs")) {
            audioAnalyzer_->setKickMinInterval(kickInterval);
        }
        
        ImGui::Spacing();
        ImGui::Text("Clap Detection:");
        float clapThresh = audioAnalyzer_->getClapThresholdMultiplier();
        ImGui::SetNextItemWidth(120);
        if (ImGui::SliderFloat("Threshold##clap_thresh", &clapThresh, 0.5f, 5.0f, "%.2f")) {
            audioAnalyzer_->setClapThresholdMultiplier(clapThresh);
        }
        ImGui::SameLine();
        float clapBassShare = audioAnalyzer_->getClapMaxBassShare();
        ImGui::SetNextItemWidth(120);
        if (ImGui::SliderFloat("Max Bass##clap_bass", &clapBassShare, 0.1f, 0.9f, "%.2f")) {
            audioAnalyzer_->setClapMaxBassShare(clapBassShare);
        }
        
        ImGui::Spacing();
        ImGui::Text("Hi-Hat Detection:");
        float hiHatThresh = audioAnalyzer_->getHiHatThresholdMultiplier();
        ImGui::SetNextItemWidth(120);
        if (ImGui::SliderFloat("Threshold##hh_thresh", &hiHatThresh, 0.5f, 5.0f, "%.2f")) {
            audioAnalyzer_->setHiHatThresholdMultiplier(hiHatThresh);
        }
        ImGui::SameLine();
        float hiHatShare = audioAnalyzer_->getHiHatMinHighShare();
        ImGui::SetNextItemWidth(120);
        if (ImGui::SliderFloat("Min High##hh_share", &hiHatShare, 0.05f, 0.5f, "%.2f")) {
            audioAnalyzer_->setHiHatMinHighShare(hiHatShare);
        }
        
        ImGui::Unindent();
    }

    ImGui::Separator();
    ImVec4 statusColor = rms > 0.01f ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
    ImGui::TextColored(statusColor, "Status: %s", rms > 0.01f ? "🟢 RECEIVING AUDIO" : "🔴 NO AUDIO INPUT");

    ImGui::End();
}

void Visualizer::renderColorsWindow() {
    ImGui::Begin("Colors", &showImGuiColorsWindow_, ImGuiWindowFlags_AlwaysAutoResize);

    // === RENDER ENGINE SECTION ===
    ImGui::Text("🎬 Render Engine");
    ImGui::Separator();
    
    ImGui::Checkbox("Modern Shaders", &useModernPipeline_);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Overlays legacy render on top of modern pipeline");
    }
    
    ImGui::Spacing();
    ImGui::Spacing();

    // === COLOR PALETTE SECTION ===
    ImGui::Text("🎨 Color Palette");
    ImGui::Separator();
    
    int paletteCount = static_cast<int>(scenePalettes_.size());
    const char* currentName = (currentScenePaletteIndex_ >= 0 && currentScenePaletteIndex_ < paletteCount)
                                ? scenePalettes_[currentScenePaletteIndex_].name.c_str()
                                : "Sin límites";

    ImGui::Text("Preset:");
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo("##palette_preset", currentName)) {
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

    ImGui::Spacing();
    
    // Color Controls
    ImGui::Text("Colors:");
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::SliderFloat("Blend", &scenePaletteBlend_, 0.0f, 1.0f, "%.2f")) {
        scenePaletteBlend_ = std::clamp(scenePaletteBlend_, 0.0f, 1.0f);
        proceduralLayer_.setColorPalette(scenePrimaryColor_.data(), sceneSecondaryColor_.data(), scenePaletteBlend_);
        saveCurrentSettings();
    }

    bool prevColorAnimation = colorAnimationEnabled_;
    ImGui::Checkbox("Animate colors", &colorAnimationEnabled_);
    if (prevColorAnimation != colorAnimationEnabled_) {
        saveCurrentSettings();
    }

    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::ColorEdit3("Primary", scenePrimaryColor_.data(), ImGuiColorEditFlags_Float)) {
        proceduralLayer_.setColorPalette(scenePrimaryColor_.data(), sceneSecondaryColor_.data(), scenePaletteBlend_);
        saveCurrentSettings();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::ColorEdit3("Secondary", sceneSecondaryColor_.data(), ImGuiColorEditFlags_Float)) {
        proceduralLayer_.setColorPalette(scenePrimaryColor_.data(), sceneSecondaryColor_.data(), scenePaletteBlend_);
        saveCurrentSettings();
    }

    if (ImGui::Button("Restore Preset")) {
        applyScenePalette(currentScenePaletteIndex_);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // === RGB CHANNELS SECTION ===
    ImGui::Text("🔴🟢🔵 RGB Channels");
    ImGui::Separator();
    
    bool prevAutoRandomizeRgb = autoRandomizeRgbChannels_;
    ImGui::Checkbox("Auto-randomize", &autoRandomizeRgbChannels_);
    if (prevAutoRandomizeRgb != autoRandomizeRgbChannels_) {
        saveCurrentSettings();
    }
    if (autoRandomizeRgbChannels_) {
        ImGui::SameLine();
        ImGui::TextDisabled("(every %ds + 3 onsets)", static_cast<int>(rgbRandomInterval_));
        
        ImGui::Spacing();
        ImGui::Text("Interval (seconds):");
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::SliderFloat("##rgb_random_interval", &rgbRandomInterval_, 1.0f, 30.0f, "%.1fs")) {
            rgbRandomInterval_ = std::max(1.0f, rgbRandomInterval_);
            saveCurrentSettings();
        }
    }
    
    ImGui::Spacing();
    static const char* kLabels[3] = {"Red", "Green", "Blue"};
    for (int i = 0; i < 3; ++i) {
        bool prevChannelState = rgbChannelEnabled_[i];
        ImGui::Checkbox(kLabels[i], &rgbChannelEnabled_[i]);
        if (prevChannelState != rgbChannelEnabled_[i]) {
            saveCurrentSettings();
        }
        if (i < 2) {
            ImGui::SameLine();
        }
    }

    ImGui::Spacing();
    if (ImGui::Button("Enable All")) {
        rgbChannelEnabled_[0] = rgbChannelEnabled_[1] = rgbChannelEnabled_[2] = true;
        saveCurrentSettings();
    }
    ImGui::SameLine();
    if (ImGui::Button("Disable All")) {
        rgbChannelEnabled_[0] = rgbChannelEnabled_[1] = rgbChannelEnabled_[2] = false;
        saveCurrentSettings();
    }
    ImGui::SameLine();
    if (ImGui::Button("Solo B")) {
        rgbChannelEnabled_[0] = false;
        rgbChannelEnabled_[1] = false;
        rgbChannelEnabled_[2] = true;
        saveCurrentSettings();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // === RANDOM COLOR SETTINGS ===
    ImGui::Text("🎲 Random Color Settings");
    ImGui::Separator();
    
    bool prevAutoRandomize = autoRandomizeColors_;
    ImGui::Checkbox("Random auto", &autoRandomizeColors_);
    if (prevAutoRandomize != autoRandomizeColors_) {
        saveCurrentSettings();
    }
    ImGui::SameLine();
    bool prevOnsetCycling = onsetColorCyclingEnabled_;
    ImGui::Checkbox("Every 2 onsets", &onsetColorCyclingEnabled_);
    if (prevOnsetCycling != onsetColorCyclingEnabled_) {
        saveCurrentSettings();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::SliderFloat("Interval (s)", &colorRandomInterval_, 1.0f, 60.0f)) {
        colorRandomInterval_ = std::max(1.0f, colorRandomInterval_);
        saveCurrentSettings();
    }

    ImGui::End();
}

void Visualizer::renderProceduralWindow() {
    ImGui::Begin("Procedural", &showImGuiProceduralWindow_, ImGuiWindowFlags_AlwaysAutoResize);

    // === MODERN PIPELINE SECTION ===
    if (useModernPipeline_) {
        ImGui::Text("🔮 Modern Pipeline");
        ImGui::Separator();
        
        bool prevCornerOrbs = showCornerOrbs_;
        ImGui::Checkbox("Corner Orbs", &showCornerOrbs_);
        if (prevCornerOrbs != showCornerOrbs_) {
            saveCurrentSettings();
        }
        
        // Random Corner Orbs section
        ImGui::Spacing();
        bool prevRandomOrbs = randomCornerOrbsEnabled_;
        ImGui::Checkbox("##random_orbs_enable", &randomCornerOrbsEnabled_);
        ImGui::SameLine();
        ImGui::Text("Auto-randomize Orbs");
        if (prevRandomOrbs != randomCornerOrbsEnabled_) {
            saveCurrentSettings();
        }
        
        if (randomCornerOrbsEnabled_) {
            ImGui::Indent();
            ImGui::Text("Interval:");
            ImGui::SetNextItemWidth(200.0f);
            if (ImGui::SliderFloat("##random_orbs_interval", &randomCornerOrbsInterval_, 1.0f, 30.0f, "%.1f sec")) {
                randomCornerOrbsInterval_ = std::max(1.0f, randomCornerOrbsInterval_);
                saveCurrentSettings();
            }
            ImGui::Unindent();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Individual Slot Controls
        bool anySlotEnabled = false;
        for (int slotIndex = 0; slotIndex < kMaxProceduralSlots; ++slotIndex) {
            auto& slot = proceduralSlots_[slotIndex];

            ImGui::PushID(slotIndex);
            std::string slotName = "Slot " + std::to_string(slotIndex + 1) + (slotIndex == 0 ? " (Main)" : "");
            if (ImGui::CollapsingHeader(slotName.c_str(), slotIndex == 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {

                // Enable Control
                bool prevEnabled = slot.enabled;
                ImGui::Checkbox("##proc_enable", &slot.enabled);
                ImGui::SameLine();
                ImGui::Text("Enable");
                anySlotEnabled = anySlotEnabled || slot.enabled;
                if (prevEnabled != slot.enabled) {
                    saveCurrentSettings();
                }

                ImGui::Spacing();

                // Mode and Opacity Controls (only when enabled)
                if (slot.enabled) {
                    // Mode Selection with proper index mapping
                    ImGui::Text("Effect Mode:");
                    auto& effectList = GetEffectListForImGui(this);
                    int currentMode = (slotIndex == 0) ? proceduralLayerMode_ : slot.mode;
                    int uiIndex = effectList.findUiIndex(currentMode);
                    std::string currentEffectName = GetEffectRegistry().getEffectNameByIndex(currentMode);
                    const char* comboPreview = !currentEffectName.empty()
                        ? currentEffectName.c_str()
                        : ((uiIndex >= 0 && uiIndex < static_cast<int>(effectList.size()))
                               ? effectList.ptrs[uiIndex]
                               : "None");

                    if (slotIndex == 0) {
                        std::string slotEffectName = GetEffectRegistry().getEffectNameByIndex(proceduralSlots_[0].mode);
                        const char* slotEffect = !slotEffectName.empty() ? slotEffectName.c_str() : "Unknown";
                        std::string currentEffect = !currentEffectName.empty() ? currentEffectName : "Unknown";
                        ImGui::TextDisabled("Current mode: %d (%s)", proceduralLayerMode_, currentEffect.c_str());
                        ImGui::TextDisabled("Slot 0 mode: %d (%s)", proceduralSlots_[0].mode, slotEffect);
                        ImGui::TextDisabled("Last trigger: %s | requested=%d | applied=%d",
                                            lastProceduralModeSource_.c_str(),
                                            lastProceduralModeRequested_,
                                            lastProceduralModeApplied_);
                        if (proceduralSlots_[0].mode != proceduralLayerMode_) {
                            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
                                               "DESYNC: current mode != slot 0");
                        }
                    }
                    
                    if (ImGui::BeginCombo("##proc_mode", comboPreview)) {
                        for (int i = 0; i < static_cast<int>(effectList.size()); ++i) {
                            bool selected = (uiIndex == i);
                            if (ImGui::Selectable(effectList.ptrs[i], selected)) {
                                int newMode = effectList.getModeIndex(i);
                                std::cout << "[UI DEBUG] Selected effect '" << effectList.ptrs[i] 
                                          << "' -> mode=" << newMode << " (uiIndex=" << i << ")" << std::endl;
                                std::cout << "[UI DEBUG] Before apply: currentMode=" << proceduralLayerMode_
                                          << " slot.mode=" << slot.mode
                                          << " slot0.mode=" << proceduralSlots_[0].mode
                                          << " trigger=ui-proc-mode-combo" << std::endl;
                                if (slotIndex == 0) {
                                    applyMainProceduralMode(newMode, "ui-proc-mode-combo");
                                    std::cout << "[UI DEBUG] After apply: currentMode=" << proceduralLayerMode_
                                              << " slot0.mode=" << proceduralSlots_[0].mode
                                              << " lastTrigger=" << lastProceduralModeSource_ << std::endl;
                                } else {
                                    // For secondary slots, directly update the slot mode
                                    slot.mode = newMode;
                                    std::cout << "[UI DEBUG] Secondary slot " << slotIndex << " mode set to " << newMode << std::endl;
                                }
                                saveCurrentSettings();
                            }
                            if (selected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    
                    ImGui::Spacing();
                    
                    // Opacity Control
                    ImGui::Text("Opacity:");
                    float prevOpacity = slot.opacity;
                    ImGui::SliderFloat("##proc_opacity", &slot.opacity, 0.0f, 1.0f, "%.2f");
                    if (prevOpacity != slot.opacity) {
                        saveCurrentSettings();
                        if (slotIndex == 0) {
                            proceduralLayerOpacity_ = slot.opacity;
                            slot.enabled = true;
                            showProceduralLayer_ = true;
                        }
                    }
                }
            }
            ImGui::PopID();
            
            if (slotIndex < kMaxProceduralSlots - 1) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
            }
        }
        
        // Update global state based on slots
        if (!anySlotEnabled) {
            showProceduralLayer_ = false;
        } else if (proceduralSlots_[0].enabled) {
            showProceduralLayer_ = true;
            proceduralLayerOpacity_ = proceduralSlots_[0].opacity;
        }
        
        // Random Cycle Section
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::Text(" Random Cycle - CAPAS PROCEDURALES");
        ImGui::TextDisabled("Cambia automticamente los efectos de CAPAS cada cierto tiempo");
        
        ImGui::Spacing();
        
        bool prevRandomProcedural = randomProceduralEnabled_;
        ImGui::Checkbox("##proc_random_enable", &randomProceduralEnabled_);
        ImGui::SameLine();
        ImGui::Text("Enable Random Cycle");
        if (prevRandomProcedural != randomProceduralEnabled_) {
            saveCurrentSettings();
            if (randomProceduralEnabled_) {
                selectRandomProcedural();
            }
        }
        
        if (randomProceduralEnabled_) {
            ImGui::Spacing();
            
            ImGui::Text("Change Interval:");
            ImGui::SetNextItemWidth(200.0f);
            if (ImGui::SliderFloat("##proc_random_interval", &randomProceduralInterval_, 1.0f, 30.0f, "%.1f seconds")) {
                randomProceduralInterval_ = std::max(1.0f, randomProceduralInterval_);
                saveCurrentSettings();
            }
            
            ImGui::Spacing();
            
            auto& effectList = GetEffectListForImGui(this);
            int uiIdx = effectList.findUiIndex(currentRandomProcedural_);
            if (uiIdx >= 0 && uiIdx < static_cast<int>(effectList.size())) {
                ImGui::Text("Current Effect: %s", effectList.ptrs[uiIdx]);
            }
            
            ImGui::Spacing();
            if (ImGui::Button("Change Effect Now")) {
                selectRandomProcedural();
                saveCurrentSettings();
            }
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Shader Enable/Disable Section
        ImGui::Text("🎭 Shader Availability");
        ImGui::TextDisabled("Enable/disable shaders for UI and randomization");
        
        ImGui::Spacing();
        
        if (ImGui::CollapsingHeader("Manage Shaders")) {
            ImGui::Indent();
            
            // Get all effects from registry (including disabled ones)
            auto& registry = GetEffectRegistry();
            auto effects = registry.getAllEffects();
            
            bool anyChanged = false;
            
            // Two column layout for shader list
            ImGui::Columns(2, "shader_columns", false);
            
            int colIndex = 0;
            for (const auto& effect : effects) {
                if (effect.modeIndex == 0) continue; // Skip "None"
                
                bool enabled = isProceduralShaderEnabled(effect.modeIndex);
                std::string label = effect.name + "##" + std::to_string(effect.modeIndex);
                
                if (ImGui::Checkbox(label.c_str(), &enabled)) {
                    setProceduralShaderEnabled(effect.modeIndex, enabled);
                    anyChanged = true;
                }
                
                // Show tooltip with shader info
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Mode: %d", effect.modeIndex);
                    if (!effect.description.empty()) {
                        ImGui::Text("%s", effect.description.c_str());
                    }
                    if (!effect.author.empty()) {
                        ImGui::Text("Author: %s", effect.author.c_str());
                    }
                    ImGui::Text("File: %s", effect.shaderFile.c_str());
                    ImGui::EndTooltip();
                }
                
                // Move to next column every other item
                colIndex++;
                if (colIndex % 2 == 0) {
                    ImGui::NextColumn();
                }
            }
            
            ImGui::Columns(1); // Reset to single column
            
            if (anyChanged) {
                saveCurrentSettings();
                // Rebuild the random pool and ImGui list
                initializeRandomProcedural();
            }
            
            ImGui::Spacing();
            
            // Quick enable/disable all buttons (stacked vertically)
            if (ImGui::Button("Enable All")) {
                for (const auto& effect : effects) {
                    if (effect.modeIndex == 0) continue;
                    setProceduralShaderEnabled(effect.modeIndex, true);
                }
                saveCurrentSettings();
                initializeRandomProcedural();
            }
            ImGui::Spacing();
            if (ImGui::Button("Invert Selection")) {
                for (const auto& effect : effects) {
                    if (effect.modeIndex == 0) continue;
                    bool current = isProceduralShaderEnabled(effect.modeIndex);
                    setProceduralShaderEnabled(effect.modeIndex, !current);
                }
                saveCurrentSettings();
                initializeRandomProcedural();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Invertir: activados → desactivados, desactivados → activados");
            }
            ImGui::Spacing();
            if (ImGui::Button("Disable All")) {
                for (const auto& effect : effects) {
                    if (effect.modeIndex == 0) continue;
                    setProceduralShaderEnabled(effect.modeIndex, false);
                }
                // Also set current mode to None since all shaders are now disabled
                applyMainProceduralMode(0);
                saveCurrentSettings();
                initializeRandomProcedural();
            }
            
            ImGui::Unindent();
        }
    } else {
        ImGui::TextDisabled("Modern Pipeline disabled");
    }

    ImGui::End();
}

void Visualizer::renderPostProcessWindow() {
    ImGui::Begin("Post Process", &showImGuiPostProcessWindow_, ImGuiWindowFlags_AlwaysAutoResize);

    // Post Processing Slots
    ImGui::Text("🎨 Post Processing");
    ImGui::SameLine();
    if (ImGui::Button("🧹 Clear Ghosting")) {
        postProcessor_.clearAccumulation();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Clear accumulation buffers to remove burn-in/ghosting artifacts");
    }
    ImGui::Separator();
    
    const int postModeCount = static_cast<int>(std::size(kPostProcessModes));
    const int randomCycleIndex = postModeCount > 0 ? postModeCount - 1 : 0;

    auto buildSelectablePostModes = [&](std::vector<int>& outModes) {
        outModes.clear();
        outModes.push_back(0); // Always allow "None"
        for (int i = 1; i < postModeCount; ++i) {
            bool isRandomCycle = (i == randomCycleIndex);
            if (!isRandomCycle && !isPostProcessEffectEnabled(i)) {
                continue;
            }
            outModes.push_back(i);
        }
    };

    std::vector<int> selectableModes;
    selectableModes.reserve(postModeCount);

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
            ImGui::Checkbox("##post_enable", &slot.enabled);
            ImGui::SameLine();
            ImGui::Text("Enable");
            anySlotEnabled = anySlotEnabled || slot.enabled;
            if (prevEnabled != slot.enabled) {
                saveCurrentSettings();
            }

            if (slot.enabled) {
                int currentMode = slot.mode;
                if (currentMode < 0 || currentMode >= postModeCount) {
                    currentMode = 0;
                }

                bool modeAllowed = (currentMode == 0 || currentMode == randomCycleIndex);
                if (!modeAllowed) {
                    modeAllowed = isPostProcessEffectEnabled(currentMode);
                }
                if (!modeAllowed) {
                    currentMode = 0;
                    slot.mode = 0;
                }

                buildSelectablePostModes(selectableModes);

                ImGui::Text("Effect Mode:");
                if (ImGui::BeginCombo("##post_mode", kPostProcessModes[currentMode])) {
                    for (int mode : selectableModes) {
                        bool selected = (slot.mode == mode);
                        if (ImGui::Selectable(kPostProcessModes[mode], selected)) {
                            slot.mode = mode;
                            saveCurrentSettings();
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::Spacing();
                
                ImGui::Text("Intensity:");
                float prevStrength = slot.strength;
                ImGui::SliderFloat("##post_strength", &slot.strength, 0.0f, 1.0f, "%.2f");
                if (prevStrength != slot.strength) {
                    saveCurrentSettings();
                }

                if (slot.mode == 15) {
                    ImGui::Spacing();
                    ImGui::Text("RGB Channels:");
                    ImGui::SetNextItemWidth(180.0f);
                    ImGui::SliderFloat("##post_rgb_r", &slot.rgbAdjust[0], 0.0f, 1.5f, "%.2f");
                    ImGui::SameLine();
                    ImGui::Text("R");
                    ImGui::SetNextItemWidth(180.0f);
                    ImGui::SliderFloat("##post_rgb_g", &slot.rgbAdjust[1], 0.0f, 1.5f, "%.2f");
                    ImGui::SameLine();
                    ImGui::Text("G");
                    ImGui::SetNextItemWidth(180.0f);
                    ImGui::SliderFloat("##post_rgb_b", &slot.rgbAdjust[2], 0.0f, 1.5f, "%.2f");
                    ImGui::SameLine();
                    ImGui::Text("B");
                    saveCurrentSettings();
                    ImGui::TextDisabled("Adjust how much each channel shifts in the split.");
                }
            }
        }
        ImGui::PopID();
    }

    // Random Cycle Section
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::Text("🎲 Random Cycle - POST-PROCESSING");
    ImGui::TextDisabled("Cambia automáticamente los efectos de POST-PROCESSING cada cierto tiempo");
    
    ImGui::Spacing();
    
    bool prevRandomEnabled = randomPostProcessEnabled_;
    ImGui::Checkbox("##post_random_enable", &randomPostProcessEnabled_);
    ImGui::SameLine();
    ImGui::Text("Enable Random Cycle");
    if (prevRandomEnabled != randomPostProcessEnabled_) {
        saveCurrentSettings();
        if (randomPostProcessEnabled_) {
            selectRandomPostProcess();
        }
    }
    
    if (randomPostProcessEnabled_) {
        ImGui::Spacing();
        
        // Slot count slider
        ImGui::Text("Slots to Randomize:");
        ImGui::SetNextItemWidth(200.0f);
        int slotCount = randomPostProcessSlotCount_;
        if (ImGui::SliderInt("##post_random_slots", &slotCount, 1, kMaxPostProcessSlots, "%d slots")) {
            randomPostProcessSlotCount_ = std::clamp(slotCount, 1, kMaxPostProcessSlots);
            saveCurrentSettings();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Number of post-process slots to randomize (1-%d)", kMaxPostProcessSlots);
        }
        
        ImGui::Spacing();
        
        ImGui::Text("Change Interval:");
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::SliderFloat("##post_random_interval", &randomPostProcessInterval_, 1.0f, 30.0f, "%.1f seconds")) {
            randomPostProcessInterval_ = std::max(1.0f, randomPostProcessInterval_);
            saveCurrentSettings();
        }
        
        ImGui::Spacing();
        
        if (kMaxPostProcessSlots > 0 && currentRandomPostProcess_ >= 0 && currentRandomPostProcess_ < static_cast<int>(std::size(kPostProcessModes))) {
            ImGui::Text("Current Effect: %s", kPostProcessModes[currentRandomPostProcess_]);
        }
        
        ImGui::Spacing();
        if (ImGui::Button("Change Effect Now")) {
            selectRandomPostProcess();
            saveCurrentSettings();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("🎭 Disponibilidad de Post FX");
    ImGui::TextDisabled("Activa o desactiva efectos disponibles para slots y ciclo aleatorio");

    if (ImGui::CollapsingHeader("Gestionar Post FX")) {
        ImGui::Indent();
        bool anyChanged = false;
        ImGui::Columns(2, "post_fx_columns", false);
        int columnIndex = 0;

        for (int mode = 1; mode < postModeCount - 1; ++mode) {
            bool enabled = isPostProcessEffectEnabled(mode);
            std::string label = std::string(kPostProcessModes[mode]) + "##postfx_" + std::to_string(mode);
            if (ImGui::Checkbox(label.c_str(), &enabled)) {
                setPostProcessEffectEnabled(mode, enabled);
                anyChanged = true;
            }

            if (++columnIndex % 2 == 0) {
                ImGui::NextColumn();
            }
        }

        ImGui::Columns(1);

        if (anyChanged) {
            initializeRandomPostProcess();
            saveCurrentSettings();
        }

        ImGui::Spacing();
        if (ImGui::Button("Enable All Post FX")) {
            for (int mode = 1; mode < postModeCount - 1; ++mode) {
                setPostProcessEffectEnabled(mode, true);
            }
            initializeRandomPostProcess();
            saveCurrentSettings();
        }
        ImGui::Spacing();
        if (ImGui::Button("Invert Selection")) {
            for (int mode = 1; mode < postModeCount - 1; ++mode) {
                bool current = isPostProcessEffectEnabled(mode);
                setPostProcessEffectEnabled(mode, !current);
            }
            initializeRandomPostProcess();
            saveCurrentSettings();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Invertir: activados → desactivados, desactivados → activados");
        }
        ImGui::Spacing();
        if (ImGui::Button("Disable All Post FX")) {
            for (int mode = 1; mode < postModeCount - 1; ++mode) {
                setPostProcessEffectEnabled(mode, false);
            }
            initializeRandomPostProcess();
            saveCurrentSettings();
        }

        ImGui::Unindent();
    }

    ImGui::End();
}

void Visualizer::renderShaderPresetsWindow() {
    ImGui::Begin("Shader Presets", &showShaderPresetsWindow_, ImGuiWindowFlags_AlwaysAutoResize);
    
    ImGui::Text("🎨 Shader Presets");
    ImGui::TextDisabled("Save and load shader enable/disable configurations");
    
    ImGui::Spacing();
    
    // Get list of presets
    static std::vector<std::string> presetList;
    static bool presetListNeedsRefresh = true;
    static int selectedPresetIndex = -1;
    
    if (presetListNeedsRefresh) {
        presetList = settingsManager_->listShaderPresets();
        presetListNeedsRefresh = false;
        selectedPresetIndex = -1;
    }
    
    // Preset list
    ImGui::Text("Saved Presets:");
    ImGui::SetNextItemWidth(250.0f);
    if (ImGui::BeginListBox("##preset_list")) {
        for (int i = 0; i < static_cast<int>(presetList.size()); ++i) {
            bool isSelected = (selectedPresetIndex == i);
            if (ImGui::Selectable(presetList[i].c_str(), isSelected)) {
                selectedPresetIndex = i;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndListBox();
    }
    
    ImGui::Spacing();
    
    // Load button
    if (selectedPresetIndex >= 0 && selectedPresetIndex < static_cast<int>(presetList.size())) {
        if (ImGui::Button("Load Preset")) {
            const std::string& presetName = presetList[selectedPresetIndex];
            std::unordered_map<int, bool> loadedStates;
            int loadedMode = 0;
            if (settingsManager_->loadShaderPreset(presetName, loadedStates, loadedMode)) {
                // Apply the loaded states
                for (const auto& [modeIndex, enabled] : loadedStates) {
                    setProceduralShaderEnabled(modeIndex, enabled);
                }
                // Apply the loaded mode
                if (loadedMode != 0) {
                    // Check if the loaded mode is enabled
                    if (isProceduralShaderEnabled(loadedMode)) {
                        applyMainProceduralMode(loadedMode);
                    } else {
                        // Mode is disabled, switch to None
                        applyMainProceduralMode(0);
                    }
                } else {
                    // Loaded mode is None
                    applyMainProceduralMode(0);
                }
                saveCurrentSettings();
                initializeRandomProcedural();
                std::cout << "[PRESET] Loaded preset: " << presetName << std::endl;
            }
        }
        ImGui::SameLine();
        
        // Delete button
        if (ImGui::Button("Delete")) {
            const std::string& presetName = presetList[selectedPresetIndex];
            if (settingsManager_->deleteShaderPreset(presetName)) {
                presetListNeedsRefresh = true;
                selectedPresetIndex = -1;
            }
        }
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Save new preset
    static char presetNameBuffer[64] = "";
    ImGui::Text("Save Current Configuration:");
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputText("##preset_name", presetNameBuffer, sizeof(presetNameBuffer));
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        if (presetNameBuffer[0] != '\0') {
            std::string newPresetName(presetNameBuffer);
            int currentMode = proceduralSlots_[0].mode;
            if (settingsManager_->saveShaderPreset(newPresetName, currentMode)) {
                presetListNeedsRefresh = true;
                presetNameBuffer[0] = '\0'; // Clear input
            }
        }
    }
    
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
    // Create floating window to show current effects
    ImGui::Begin("Current Effects", &showCurrentEffects_, ImGuiWindowFlags_AlwaysAutoResize);
    
    // Title with style
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 1.0f, 1.0f));
    ImGui::Text("🎭 CURRENT EFFECTS");
    ImGui::PopStyleColor();
    ImGui::Separator();

    float fps = ImGui::GetIO().Framerate;
    if (fps > 0.0f) {
        ImGui::Text("⚡ FPS: %.1f (%.2f ms)", fps, 1000.0f / fps);
    } else {
        ImGui::Text("⚡ FPS: --");
    }

    ImGui::Spacing();
    
    // Random settings status
    ImGui::Text("🎲 Random Settings:");
    ImGui::Indent();
    
    // Post-processing random
    ImGui::Text("Post-Process: %s", randomPostProcessEnabled_ ? "🟢 ACTIVE" : "🔴 INACTIVE");
    if (randomPostProcessEnabled_) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%.1fs interval)", randomPostProcessInterval_);
    }
    
    // Procedural random
    ImGui::Text("Procedural: %s", randomProceduralEnabled_ ? "🟢 ACTIVE" : "🔴 INACTIVE");
    if (randomProceduralEnabled_) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%.1fs interval)", randomProceduralInterval_);
    }
    
    ImGui::Unindent();
    ImGui::Spacing();

    // Show procedural layer stack (base + overlays)
    ImGui::Text("📐 Procedural Layers:");
    ImGui::Indent();

    bool anyProceduralActive = false;
    const float activeColor[4] = {0.4f, 1.0f, 0.8f, 1.0f};

    for (int slotIndex = 0; slotIndex < kMaxProceduralSlots; ++slotIndex) {
        const auto& slot = proceduralSlots_[slotIndex];
        if (!slot.enabled || slot.opacity <= 0.001f) {
            continue;
        }

        anyProceduralActive = true;
        auto& effectList = GetEffectListForImGui(this);
        int uiIdx = effectList.findUiIndex(slot.mode);
        std::string modeNameString = GetEffectRegistry().getEffectNameByIndex(slot.mode);
        const char* modeName = !modeNameString.empty()
                                ? modeNameString.c_str()
                                : ((uiIdx >= 0 && uiIdx < static_cast<int>(effectList.size())) 
                                       ? effectList.ptrs[uiIdx] 
                                       : "Unknown");

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(activeColor[0], activeColor[1], activeColor[2], activeColor[3]));
        ImGui::Text("Slot %d%s: %s", slotIndex + 1, slotIndex == 0 ? " (base)" : "", modeName);
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::TextDisabled("Opacity %.0f%%", slot.opacity * 100.0f);

        ImGui::TextDisabled("Color Adjust: R %.2f  G %.2f  B %.2f",
                             slot.colorAdjust[0], slot.colorAdjust[1], slot.colorAdjust[2]);

        if (slotIndex < kMaxProceduralSlots - 1) {
            ImGui::Dummy(ImVec2(0.0f, 2.0f));
        }
    }

    if (!anyProceduralActive) {
        if (showProceduralLayer_) {
            auto& effectList = GetEffectListForImGui(this);
            int uiIdx = effectList.findUiIndex(proceduralLayerMode_);
            std::string modeNameString = GetEffectRegistry().getEffectNameByIndex(proceduralLayerMode_);
            const char* modeName = !modeNameString.empty()
                                    ? modeNameString.c_str()
                                    : ((uiIdx >= 0 && uiIdx < static_cast<int>(effectList.size())) 
                                           ? effectList.ptrs[uiIdx] 
                                           : "Unknown");
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(activeColor[0], activeColor[1], activeColor[2], activeColor[3]));
            ImGui::Text("Global: %s", modeName);
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextDisabled("Opacity %.0f%%", proceduralLayerOpacity_ * 100.0f);
        } else {
            ImGui::TextDisabled("Procedural layer disabled");
        }
    }

    ImGui::Unindent();

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
    
    ImGui::End();
}

void Visualizer::renderCameraWindow() {
    // Camera window always visible - no close button
    ImGui::Begin("Camera Control", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("🎥 Global Camera Settings");
    ImGui::Separator();

    // Zoom control - saves per shader mode
    float zoom = proceduralLayer_.cameraZoom();
    ImGui::Text("Zoom:");
    if (ImGui::SliderFloat("##camera_zoom", &zoom, 0.1f, 5.0f, "%.2fx")) {
        proceduralLayer_.setCameraZoom(zoom);
        // Save zoom for current shader mode
        int currentMode = proceduralLayerMode_;
        if (currentMode > 0 && settingsManager_) {
            settingsManager_->setProceduralZoom(currentMode, zoom);
        }
        saveCurrentSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Global zoom for all procedural effects (saved per shader)");
    }

    ImGui::Spacing();

    // Offset X control
    float offsetX = proceduralLayer_.cameraOffsetX();
    ImGui::Text("Offset X:");
    if (ImGui::SliderFloat("##camera_offset_x", &offsetX, -2.0f, 2.0f, "%.2f")) {
        proceduralLayer_.setCameraOffset(offsetX, proceduralLayer_.cameraOffsetY());
        saveCurrentSettings();
    }

    ImGui::Spacing();

    // Offset Y control
    float offsetY = proceduralLayer_.cameraOffsetY();
    ImGui::Text("Offset Y:");
    if (ImGui::SliderFloat("##camera_offset_y", &offsetY, -2.0f, 2.0f, "%.2f")) {
        proceduralLayer_.setCameraOffset(proceduralLayer_.cameraOffsetX(), offsetY);
        saveCurrentSettings();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Reset button - resets to shader default zoom
    if (ImGui::Button("Reset to Default")) {
        float defaultZoom = getZoomForShaderMode(proceduralLayerMode_);
        proceduralLayer_.setCameraZoom(defaultZoom);
        proceduralLayer_.setCameraOffset(0.0f, 0.0f);
        // Clear saved zoom for current mode to use shader default
        if (proceduralLayerMode_ > 0 && settingsManager_) {
            settingsManager_->setProceduralZoom(proceduralLayerMode_, 0.0f); // 0 = use shader default
        }
        saveCurrentSettings();
    }

    ImGui::SameLine();

    // Auto-animated zoom toggle
    static bool autoZoom = false;
    ImGui::Checkbox("Auto-Animate", &autoZoom);
    if (autoZoom) {
        // Get current range settings
        float zoomMin = proceduralLayer_.autoZoomMin();
        float zoomMax = proceduralLayer_.autoZoomMax();
        
        // Range controls
        ImGui::Text("Zoom Range:");
        bool rangeChanged = false;
        if (ImGui::SliderFloat("Min##auto_zoom_min", &zoomMin, 0.1f, zoomMax - 0.05f, "%.2fx")) {
            rangeChanged = true;
        }
        if (ImGui::SliderFloat("Max##auto_zoom_max", &zoomMax, zoomMin + 0.05f, 5.0f, "%.2fx")) {
            rangeChanged = true;
        }
        if (rangeChanged) {
            proceduralLayer_.setAutoZoomRange(zoomMin, zoomMax);
        }
        
        // Calculate animated zoom within the range
        float range = zoomMax - zoomMin;
        float mid = (zoomMax + zoomMin) / 2.0f;
        float animatedZoom = mid + sin(time_ * 0.5f) * (range * 0.5f) + audioFeatures_.bassEnergy * (range * 0.3f);
        animatedZoom = std::clamp(animatedZoom, zoomMin, zoomMax);
        proceduralLayer_.setCameraZoom(animatedZoom);
    }

    ImGui::End();
}

// FASE 0.4: Performance metrics display
void Visualizer::renderPerformanceImGui() {
    if (!ImGui::Begin("Performance Metrics", &showPerformanceWindow_)) {
        ImGui::End();
        return;
    }

    // Profiler controls
    ImGui::Separator();
    ImGui::Text("Profiler (CPU Timing)");
    if (ImGui::Button("Print Profiler Report")) {
        Profiler::getInstance().printReport();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Profiler")) {
        Profiler::getInstance().reset();
    }
    ImGui::Separator();

    ImGui::Text("Frame Timing:");
    ImGui::Text("  CPU: %.2f ms", frameTimeCPU_);
    ImGui::Text("  FPS: %.1f", fps_);

    ImGui::Spacing();
    ImGui::Text("API Calls:");
    ImGui::Text("  Draw calls: %d", drawCallsPerFrame_);
    ImGui::Text("  Uniform calls: %d (TODO)", uniformCallsPerFrame_);
    ImGui::Text("  Texture binds: %d (TODO)", textureBindsPerFrame_);
    ImGui::Text("  Shader switches: %d (TODO)", shaderSwitchesPerFrame_);

    ImGui::Spacing();
    ImGui::Text("Status:");
    if (frameTimeCPU_ > 16.67f) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "  CPU bottleneck detected");
    } else {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "  CPU OK");
    }

    ImGui::End();
}

void Visualizer::handleMouseScroll(double xoffset, double yoffset) {
    // Always process zoom with mouse wheel, even when ImGui has focus
    // This allows zooming from the ImGui window
    float currentZoom = proceduralLayer_.cameraZoom();
    float zoomDelta = static_cast<float>(yoffset) * 0.1f;  // 10% per scroll step
    float newZoom = std::clamp(currentZoom + zoomDelta, 0.1f, 5.0f);

    if (newZoom != currentZoom) {
        proceduralLayer_.setCameraZoom(newZoom);
        // Save zoom for current shader mode
        int currentMode = proceduralLayerMode_;
        if (currentMode > 0 && settingsManager_) {
            settingsManager_->setProceduralZoom(currentMode, newZoom);
        }
        saveCurrentSettings();
    }
}
