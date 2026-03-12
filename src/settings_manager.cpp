#include "settings_manager.h"
#include "visualizer.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

SettingsManager::SettingsManager() {
    // Initialize all post-process slots as active but in None mode
    for (int i = 0; i < 5; ++i) {
        postProcessSlots_[i].enabled = true;
        postProcessSlots_[i].mode = 0; // None
        postProcessSlots_[i].strength = 1.0f;
        postProcessSlots_[i].rgbAdjust = {1.0f, 1.0f, 1.0f};
    }

    // Set UI elements to always be expanded by default
    showImGuiWindow_ = true;
    showProceduralLayer_ = true;
    proceduralLayerDebug_ = false;
    proceduralLayerOpacity_ = 0.85f;
    proceduralLayerMode_ = 1; // ASCII Ocean (safe default within 0-31 range)

    // RGB channels always enabled by default
    rgbChannelEnabled_ = {true, true, true};
}

SettingsManager::~SettingsManager() = default;

bool SettingsManager::loadSettings(const std::string& filename) {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cout << "Settings file not found, using defaults: " << filename << std::endl;
            return false;
        }

        json j;
        file >> j;

        // Load audio settings
        if (j.contains("audio")) {
            const auto& audio = j["audio"];
            if (audio.contains("selectedDevice")) selectedDevice_ = audio["selectedDevice"];
            if (audio.contains("inputGain")) audioInputGain_ = audio["inputGain"];
            if (audio.contains("visualSensitivity")) visualSensitivity_ = audio["visualSensitivity"];
        }

        // Load UI settings
        if (j.contains("ui")) {
            const auto& ui = j["ui"];
            if (ui.contains("showImGuiWindow")) showImGuiWindow_ = ui["showImGuiWindow"];
            if (ui.contains("showCornerOrbs")) showCornerOrbs_ = ui["showCornerOrbs"];
            if (ui.contains("showProceduralLayer")) showProceduralLayer_ = ui["showProceduralLayer"];
            if (ui.contains("proceduralLayerDebug")) proceduralLayerDebug_ = ui["proceduralLayerDebug"];
            if (ui.contains("proceduralLayerOpacity")) proceduralLayerOpacity_ = ui["proceduralLayerOpacity"];
            if (ui.contains("proceduralLayerMode")) {
                int loadedMode = ui["proceduralLayerMode"];
                // Clamp to valid range (0-38 for 39 modes)
                proceduralLayerMode_ = std::clamp(loadedMode, 0, 38);
            }
        }

        // Load post-processing settings
        if (j.contains("postProcess")) {
            const auto& postProcess = j["postProcess"];
            if (postProcess.contains("slots")) {
                const auto& slots = postProcess["slots"];
                for (size_t i = 0; i < slots.size() && i < postProcessSlots_.size(); ++i) {
                    const auto& slot = slots[i];
                    if (slot.contains("enabled")) postProcessSlots_[i].enabled = slot["enabled"];
                    if (slot.contains("mode")) {
                        int loadedMode = slot["mode"];
                        // Clamp to valid range (0-22 for 23 modes)
                        postProcessSlots_[i].mode = std::clamp(loadedMode, 0, 22);
                    }
                    if (slot.contains("strength")) postProcessSlots_[i].strength = slot["strength"];
                    if (slot.contains("rgbAdjust")) {
                        const auto& rgb = slot["rgbAdjust"];
                        if (rgb.size() >= 3) {
                            postProcessSlots_[i].rgbAdjust[0] = rgb[0];
                            postProcessSlots_[i].rgbAdjust[1] = rgb[1];
                            postProcessSlots_[i].rgbAdjust[2] = rgb[2];
                        }
                    }
                }
            }
        }

        // Load color palette settings
        if (j.contains("colors")) {
            const auto& colors = j["colors"];
            if (colors.contains("primary")) {
                const auto& primary = colors["primary"];
                if (primary.size() >= 3) {
                    scenePrimaryColor_[0] = primary[0];
                    scenePrimaryColor_[1] = primary[1];
                    scenePrimaryColor_[2] = primary[2];
                }
            }
            if (colors.contains("secondary")) {
                const auto& secondary = colors["secondary"];
                if (secondary.size() >= 3) {
                    sceneSecondaryColor_[0] = secondary[0];
                    sceneSecondaryColor_[1] = secondary[1];
                    sceneSecondaryColor_[2] = secondary[2];
                }
            }
            if (colors.contains("blend")) scenePaletteBlend_ = colors["blend"];
            if (colors.contains("paletteIndex")) currentScenePaletteIndex_ = colors["paletteIndex"];
            if (colors.contains("hueSeed")) scenePaletteHueSeed_ = colors["hueSeed"];
        }

        // Load color animation settings
        if (j.contains("animation")) {
            const auto& animation = j["animation"];
            if (animation.contains("autoRandomize")) autoRandomizeColors_ = animation["autoRandomize"];
            if (animation.contains("randomInterval")) colorRandomInterval_ = animation["randomInterval"];
            if (animation.contains("onsetColorCycling")) onsetColorCyclingEnabled_ = animation["onsetColorCycling"];
            if (animation.contains("autoRandomizeRgb")) autoRandomizeRgbChannels_ = animation["autoRandomizeRgb"];
        }

        // Load RGB channel settings
        if (j.contains("rgbChannels")) {
            const auto& rgb = j["rgbChannels"];
            if (rgb.size() >= 3) {
                rgbChannelEnabled_[0] = rgb[0];
                rgbChannelEnabled_[1] = rgb[1];
                rgbChannelEnabled_[2] = rgb[2];
            }
        }

        std::cout << "Settings loaded successfully from: " << filename << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error loading settings: " << e.what() << std::endl;
        return false;
    }
}

bool SettingsManager::saveSettings(const std::string& filename) {
    try {
        json j;

        // Save audio settings
        j["audio"]["selectedDevice"] = selectedDevice_;
        j["audio"]["inputGain"] = audioInputGain_;
        j["audio"]["visualSensitivity"] = visualSensitivity_;

        // Save UI settings
        j["ui"]["showImGuiWindow"] = showImGuiWindow_;
        j["ui"]["showCornerOrbs"] = showCornerOrbs_;
        j["ui"]["showProceduralLayer"] = showProceduralLayer_;
        j["ui"]["proceduralLayerDebug"] = proceduralLayerDebug_;
        j["ui"]["proceduralLayerOpacity"] = proceduralLayerOpacity_;
        j["ui"]["proceduralLayerMode"] = std::clamp(proceduralLayerMode_, 0, 38);

        // Save post-processing settings
        for (size_t i = 0; i < postProcessSlots_.size(); ++i) {
            json slot;
            slot["enabled"] = postProcessSlots_[i].enabled;
            // Ensure mode is within valid range before saving
            slot["mode"] = std::clamp(postProcessSlots_[i].mode, 0, 22);
            slot["strength"] = postProcessSlots_[i].strength;
            slot["rgbAdjust"] = postProcessSlots_[i].rgbAdjust;
            j["postProcess"]["slots"].push_back(slot);
        }

        // Save color palette settings
        j["colors"]["primary"] = scenePrimaryColor_;
        j["colors"]["secondary"] = sceneSecondaryColor_;
        j["colors"]["blend"] = scenePaletteBlend_;
        j["colors"]["paletteIndex"] = currentScenePaletteIndex_;
        j["colors"]["hueSeed"] = scenePaletteHueSeed_;

        // Save color animation settings
        j["animation"]["autoRandomize"] = autoRandomizeColors_;
        j["animation"]["randomInterval"] = colorRandomInterval_;
        j["animation"]["onsetColorCycling"] = onsetColorCyclingEnabled_;
        j["animation"]["autoRandomizeRgb"] = autoRandomizeRgbChannels_;

        // Save RGB channel settings
        j["rgbChannels"] = rgbChannelEnabled_;

        // Write to file
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open settings file for writing: " << filename << std::endl;
            return false;
        }

        file << j.dump(4); // Pretty print with 4 spaces indentation
        std::cout << "Settings saved successfully to: " << filename << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error saving settings: " << e.what() << std::endl;
        return false;
    }
}

bool SettingsManager::getRgbChannelEnabled(int channel) const {
    if (channel >= 0 && channel < 3) {
        return rgbChannelEnabled_[channel];
    }
    return false;
}

void SettingsManager::setRgbChannelEnabled(int channel, bool enabled) {
    if (channel >= 0 && channel < 3) {
        rgbChannelEnabled_[channel] = enabled;
    }
}

void SettingsManager::setPostProcessSlots(const std::array<PostProcessSlot, 5>& slots) {
    postProcessSlots_ = slots;
}

void SettingsManager::updateFromVisualizerState() {
    // This method needs access to Visualizer instance
    // For now, we'll save current settings when this is called from Visualizer
    // The actual implementation will be in Visualizer class
}

void SettingsManager::applyToVisualizer() {
    // This method will be called to apply settings to Visualizer
    // Implementation will be added after integrating with Visualizer class
}
