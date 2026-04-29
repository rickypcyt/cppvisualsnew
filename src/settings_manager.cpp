#include "settings_manager.h"
#include "app/visualizer.h"
#include "shader_loader.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>

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
    proceduralLayerMode_ = 0; // Default to None (0) - user must select a shader
    colorAnimationEnabled_ = true;

    // RGB channels always enabled by default
    rgbChannelEnabled_ = {true, true, true};
}

SettingsManager::~SettingsManager() = default;

std::string SettingsManager::getDefaultSettingsPath() {
    // Try to use XDG config directory on Linux, fallback to home directory
    const char* xdgConfigHome = std::getenv("XDG_CONFIG_HOME");
    std::string configDir;

    if (xdgConfigHome && xdgConfigHome[0] != '\0') {
        configDir = xdgConfigHome;
    } else {
        const char* home = std::getenv("HOME");
        if (home && home[0] != '\0') {
            configDir = std::string(home) + "/.config";
        } else {
            // Fallback to current directory
            return "visualizer_settings.json";
        }
    }

    // Create config directory if it doesn't exist
    std::string visualsConfigDir = configDir + "/visuals";
    std::filesystem::create_directories(visualsConfigDir);

    return visualsConfigDir + "/visualizer_settings.json";
}

bool SettingsManager::migrateOldSettings() {
    std::string oldPath = "visualizer_settings.json";
    std::string newPath = getDefaultSettingsPath();

    // Check if old file exists
    if (!std::filesystem::exists(oldPath)) {
        return false;
    }

    // Check if new file doesn't exist, or if old file is larger (has more data)
    bool shouldMigrate = false;
    if (!std::filesystem::exists(newPath)) {
        shouldMigrate = true;
    } else {
        // Compare file sizes - if old is significantly larger, it has more data
        auto oldSize = std::filesystem::file_size(oldPath);
        auto newSize = std::filesystem::file_size(newPath);
        if (oldSize > newSize * 2) { // Old file is more than 2x larger
            shouldMigrate = true;
        }
    }

    if (shouldMigrate) {
        try {
            std::filesystem::copy_file(oldPath, newPath, std::filesystem::copy_options::overwrite_existing);
            std::cout << "[SETTINGS MIGRATION] Migrated settings from " << oldPath
                      << " to " << newPath << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[SETTINGS MIGRATION] Failed to migrate: " << e.what() << std::endl;
            return false;
        }
    }

    return false;
}

bool SettingsManager::loadSettings(const std::string& filename) {
    try {
        std::string actualFilename = filename.empty() ? getDefaultSettingsPath() : filename;
        std::ifstream file(actualFilename);
        if (!file.is_open()) {
            std::cout << "Settings file not found, using defaults: " << actualFilename << std::endl;
            return false;
        }

        json j;
        file >> j;

        bool loadedMainModeFromUi = false;

        // Load audio settings
        if (j.contains("audio")) {
            const auto& audio = j["audio"];
            if (audio.contains("selectedDevice")) selectedDevice_ = audio["selectedDevice"];
            if (audio.contains("inputGain")) audioInputGain_ = audio["inputGain"];
            if (audio.contains("visualSensitivity")) visualSensitivity_ = audio["visualSensitivity"];
            if (audio.contains("audioEngineEnabled")) audioEngineEnabled_ = audio["audioEngineEnabled"];
            if (audio.contains("manualBPMMode")) manualBPMMode_ = audio["manualBPMMode"];
            if (audio.contains("manualBPM")) manualBPM_ = audio["manualBPM"];
        }

        // Load UI settings
        if (j.contains("ui")) {
            const auto& ui = j["ui"];
            if (ui.contains("showImGuiWindow")) showImGuiWindow_ = ui["showImGuiWindow"];
            if (ui.contains("showCornerOrbs")) showCornerOrbs_ = ui["showCornerOrbs"];
            if (ui.contains("randomCornerOrbsEnabled")) randomCornerOrbsEnabled_ = ui["randomCornerOrbsEnabled"];
            if (ui.contains("randomCornerOrbsInterval")) randomCornerOrbsInterval_ = ui["randomCornerOrbsInterval"];
            if (ui.contains("autoClearGhosting")) autoClearGhosting_ = ui["autoClearGhosting"];
            if (ui.contains("autoClearGhostingInterval")) autoClearGhostingInterval_ = ui["autoClearGhostingInterval"];
            if (ui.contains("showProceduralLayer")) showProceduralLayer_ = ui["showProceduralLayer"];
            if (ui.contains("showCurrentEffects")) showCurrentEffects_ = ui["showCurrentEffects"];
            if (ui.contains("proceduralLayerDebug")) proceduralLayerDebug_ = ui["proceduralLayerDebug"];
            if (ui.contains("proceduralLayerOpacity")) proceduralLayerOpacity_ = ui["proceduralLayerOpacity"];
            if (ui.contains("proceduralLayerMode")) {
                int loadedMode = ui["proceduralLayerMode"];
                // Clamp to valid range (0-65 for all modes)
                proceduralLayerMode_ = std::clamp(loadedMode, 0, 73);
                loadedMainModeFromUi = true;
            }
            std::cout << "[SETTINGS LOAD] UI layer mode=" << proceduralLayerMode_;
            if (ui.contains("proceduralLayerEffectName")) {
                std::cout << " effectName=" << ui["proceduralLayerEffectName"].get<std::string>();
            }
            std::cout << std::endl;
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
                        // Clamp to valid range (0-31 for 32 modes)
                        postProcessSlots_[i].mode = std::clamp(loadedMode, 0, 31);
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

        // Load procedural slots settings - now with name-based lookup
        if (j.contains("proceduralSlots")) {
            const auto& procedural = j["proceduralSlots"];
            if (procedural.contains("slots")) {
                const auto& slots = procedural["slots"];
                for (size_t i = 0; i < slots.size() && i < proceduralSlots_.size(); ++i) {
                    const auto& slot = slots[i];
                    if (slot.contains("enabled")) proceduralSlots_[i].enabled = slot["enabled"];
                    
                    // Try to load by name first (new format)
                    if (slot.contains("effectName")) {
                        std::string effectName = slot["effectName"];
                        int idx = GetEffectRegistry().getEffectIndexByName(effectName);
                        if (idx >= 0) {
                            proceduralSlots_[i].mode = idx;
                        } else {
                            // Fallback: try to load by index for backwards compatibility
                            if (slot.contains("mode")) {
                                int loadedMode = slot["mode"];
                                proceduralSlots_[i].mode = std::clamp(loadedMode, 0, 73);
                            }
                        }
                    } else if (slot.contains("mode")) {
                        // Legacy: load by index
                        int loadedMode = slot["mode"];
                        proceduralSlots_[i].mode = std::clamp(loadedMode, 0, 73);
                    }
                    
                    if (slot.contains("opacity")) proceduralSlots_[i].opacity = slot["opacity"];
                    if (slot.contains("colorAdjust")) {
                        const auto& rgb = slot["colorAdjust"];
                        if (rgb.size() >= 3) {
                            proceduralSlots_[i].colorAdjust[0] = rgb[0];
                            proceduralSlots_[i].colorAdjust[1] = rgb[1];
                            proceduralSlots_[i].colorAdjust[2] = rgb[2];
                        }
                    }
                }
            }
        }

        if (!proceduralSlots_.empty()) {
            std::cout << "[SETTINGS LOAD] Slot0 after file parse: enabled=" << proceduralSlots_[0].enabled
                      << " mode=" << proceduralSlots_[0].mode
                      << " opacity=" << proceduralSlots_[0].opacity << std::endl;
        }

        // Load main procedural layer mode (with name support)
        if (j.contains("ui") && j["ui"].contains("proceduralLayerMode")) {
            // Try new format first: effect name
            if (j["ui"].contains("proceduralLayerEffectName")) {
                std::string effectName = j["ui"]["proceduralLayerEffectName"];
                int idx = GetEffectRegistry().getEffectIndexByName(effectName);
                if (idx >= 0) {
                    proceduralLayerMode_ = idx;
                } else {
                    // Fallback to index
                    int loadedMode = j["ui"]["proceduralLayerMode"];
                    proceduralLayerMode_ = std::clamp(loadedMode, 0, 73);
                }
            } else {
                // Legacy: load by index
                int loadedMode = j["ui"]["proceduralLayerMode"];
                proceduralLayerMode_ = std::clamp(loadedMode, 0, 73);
            }
        }

        std::cout << "[SETTINGS LOAD] Pre-reconcile: proceduralLayerMode_=" << proceduralLayerMode_;
        if (!proceduralSlots_.empty()) {
            std::cout << " slot0.mode=" << proceduralSlots_[0].mode << " slot0.enabled=" << proceduralSlots_[0].enabled;
        }
        std::cout << std::endl;

        if (!proceduralSlots_.empty()) {
            if (loadedMainModeFromUi) {
                if (proceduralSlots_[0].mode != proceduralLayerMode_) {
                    std::cout << "[SETTINGS LOAD] UI mode wins over slot0: slot0.mode "
                              << proceduralSlots_[0].mode << " -> " << proceduralLayerMode_ << std::endl;
                }
                proceduralSlots_[0].mode = proceduralLayerMode_;
                if (proceduralLayerMode_ > 0) {
                    proceduralSlots_[0].enabled = true;
                }
            } else if (proceduralSlots_[0].enabled && proceduralSlots_[0].mode > 0) {
                std::cout << "[SETTINGS LOAD] Legacy slot0 mode used as main mode: "
                          << proceduralSlots_[0].mode << std::endl;
                proceduralLayerMode_ = proceduralSlots_[0].mode;
            }
        }

        std::cout << "[SETTINGS LOAD] Final main mode=" << proceduralLayerMode_;
        if (!proceduralSlots_.empty()) {
            std::cout << " finalSlot0.mode=" << proceduralSlots_[0].mode
                      << " finalSlot0.enabled=" << proceduralSlots_[0].enabled;
        }
        std::cout << std::endl;

        std::cout << "[SETTINGS LOAD] Loaded procedural mode: " << proceduralLayerMode_ << std::endl;

        // Load random settings
        if (j.contains("random")) {
            const auto& random = j["random"]; 
            if (random.contains("postProcessEnabled")) randomPostProcessEnabled_ = random["postProcessEnabled"];
            if (random.contains("postProcessInterval")) randomPostProcessInterval_ = random["postProcessInterval"];
            if (random.contains("postProcessSlotCount")) randomPostProcessSlotCount_ = std::clamp(random["postProcessSlotCount"].get<int>(), 1, 5);
            if (random.contains("proceduralEnabled")) randomProceduralEnabled_ = random["proceduralEnabled"];
            if (random.contains("proceduralInterval")) randomProceduralInterval_ = random["proceduralInterval"];
            if (random.contains("postProcessSlotEnabled")) randomPostProcessSlotEnabled_ = random["postProcessSlotEnabled"];
            if (random.contains("postProcessSlotIndex")) randomPostProcessSlotIndex_ = random["postProcessSlotIndex"];
            if (random.contains("proceduralSlotEnabled")) randomProceduralSlotEnabled_ = random["proceduralSlotEnabled"];
            if (random.contains("proceduralSlotIndex")) randomProceduralSlotIndex_ = random["proceduralSlotIndex"];
        }
        
        // Load hot-reload setting
        if (j.contains("hotReloadEnabled")) hotReloadEnabled_ = j["hotReloadEnabled"];

        // Load post-processing random settings (legacy compatibility)
        if (j.contains("postProcess")) {
            const auto& postProcess = j["postProcess"];
            if (postProcess.contains("randomEnabled")) randomPostProcessEnabled_ = postProcess["randomEnabled"];
            if (postProcess.contains("randomInterval")) randomPostProcessInterval_ = postProcess["randomInterval"];
        }

        if (j.contains("midi")) {
            const auto& midi = j["midi"];
            if (midi.contains("tempoScale")) midiTempoScale_ = midi["tempoScale"];
        }

        // Load procedural random settings (legacy compatibility)
        if (j.contains("procedural")) {
            const auto& procedural = j["procedural"];
            if (procedural.contains("randomEnabled")) randomProceduralEnabled_ = procedural["randomEnabled"];
            if (procedural.contains("randomInterval")) randomProceduralInterval_ = procedural["randomInterval"];
        }

        // Load favorites
        if (j.contains("favorites")) {
            const auto& favorites = j["favorites"];
            favorites_.clear();
            for (const auto& fav : favorites) {
                FavoritePreset preset;
                if (fav.contains("name")) preset.name = fav["name"];
                if (fav.contains("proceduralMode")) preset.proceduralMode = fav["proceduralMode"];
                if (fav.contains("proceduralOpacity")) preset.proceduralOpacity = fav["proceduralOpacity"];
                if (fav.contains("proceduralColorAdjust")) {
                    const auto& rgb = fav["proceduralColorAdjust"];
                    if (rgb.size() >= 3) {
                        preset.proceduralColorAdjust[0] = rgb[0];
                        preset.proceduralColorAdjust[1] = rgb[1];
                        preset.proceduralColorAdjust[2] = rgb[2];
                    }
                }
                if (fav.contains("postProcessSlots")) {
                    const auto& slots = fav["postProcessSlots"];
                    for (size_t i = 0; i < slots.size() && i < preset.postProcessSlots.size(); ++i) {
                        const auto& slot = slots[i];
                        if (slot.contains("enabled")) preset.postProcessSlots[i].enabled = slot["enabled"];
                        if (slot.contains("mode")) preset.postProcessSlots[i].mode = slot["mode"];
                        if (slot.contains("strength")) preset.postProcessSlots[i].strength = slot["strength"];
                        if (slot.contains("rgbAdjust")) {
                            const auto& rgb = slot["rgbAdjust"];
                            if (rgb.size() >= 3) {
                                preset.postProcessSlots[i].rgbAdjust[0] = rgb[0];
                                preset.postProcessSlots[i].rgbAdjust[1] = rgb[1];
                                preset.postProcessSlots[i].rgbAdjust[2] = rgb[2];
                            }
                        }
                    }
                }
                favorites_.push_back(preset);
            }
            std::cout << "[SETTINGS] Loaded " << favorites_.size() << " favorites" << std::endl;
        }

        // Load random favorites only setting
        if (j.contains("randomFavoritesOnly")) randomFavoritesOnly_ = j["randomFavoritesOnly"];

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
            if (colors.contains("animationEnabled")) colorAnimationEnabled_ = colors["animationEnabled"];
        }

        // Load color animation settings
        if (j.contains("animation")) {
            const auto& animation = j["animation"];
            if (animation.contains("autoRandomize")) autoRandomizeColors_ = animation["autoRandomize"];
            if (animation.contains("randomInterval")) colorRandomInterval_ = animation["randomInterval"];
            if (animation.contains("onsetColorCycling")) onsetColorCyclingEnabled_ = animation["onsetColorCycling"];
            if (animation.contains("autoRandomizeRgb")) autoRandomizeRgbChannels_ = animation["autoRandomizeRgb"];
            if (animation.contains("rgbRandomInterval")) rgbRandomInterval_ = animation["rgbRandomInterval"];
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

        // Load per-shader enabled states
        if (j.contains("proceduralShaderEnabled")) {
            const auto& shaderStates = j["proceduralShaderEnabled"];
            proceduralShaderEnabled_.clear();
            for (auto& [key, value] : shaderStates.items()) {
                int modeIndex = std::stoi(key);
                proceduralShaderEnabled_[modeIndex] = value.get<bool>();
            }
        }

        // Load per-post-processing-effect enabled states
        if (j.contains("postProcessEffectEnabled")) {
            const auto& effectStates = j["postProcessEffectEnabled"];
            postProcessEffectEnabled_.clear();
            for (auto& [key, value] : effectStates.items()) {
                int modeIndex = std::stoi(key);
                postProcessEffectEnabled_[modeIndex] = value.get<bool>();
            }
        }

        // Load per-shader zoom values
        if (j.contains("proceduralZoomValues")) {
            const auto& zoomValues = j["proceduralZoomValues"];
            proceduralZoomValues_.clear();
            for (auto& [key, value] : zoomValues.items()) {
                int modeIndex = std::stoi(key);
                proceduralZoomValues_[modeIndex] = value.get<float>();
            }
        }

        // Load per-shader offset values
        if (j.contains("proceduralOffsetXValues")) {
            const auto& offsetXValues = j["proceduralOffsetXValues"];
            proceduralOffsetXValues_.clear();
            for (auto& [key, value] : offsetXValues.items()) {
                int modeIndex = std::stoi(key);
                proceduralOffsetXValues_[modeIndex] = value.get<float>();
            }
        }
        if (j.contains("proceduralOffsetYValues")) {
            const auto& offsetYValues = j["proceduralOffsetYValues"];
            proceduralOffsetYValues_.clear();
            for (auto& [key, value] : offsetYValues.items()) {
                int modeIndex = std::stoi(key);
                proceduralOffsetYValues_[modeIndex] = value.get<float>();
            }
        }

        // Reconcile the main procedural mode with Slot 0 when it is enabled.
        // This keeps the UI state aligned with the actual main procedural slot.
        if (!proceduralSlots_.empty() && proceduralSlots_[0].enabled && proceduralSlots_[0].mode > 0) {
            if (proceduralLayerMode_ != proceduralSlots_[0].mode) {
                std::cout << "[SETTINGS] Reconciling proceduralLayerMode_ "
                          << proceduralLayerMode_ << " -> Slot 0 mode "
                          << proceduralSlots_[0].mode << std::endl;
            }
            proceduralLayerMode_ = proceduralSlots_[0].mode;
        }

        // Validate loaded mode - if current mode is disabled, switch to None or next enabled
        if (proceduralLayerMode_ != 0) {
            auto it = proceduralShaderEnabled_.find(proceduralLayerMode_);
            if (it != proceduralShaderEnabled_.end() && !it->second) {
                // Current mode is disabled, switch to None
                std::cout << "[SETTINGS] Loaded mode " << proceduralLayerMode_ << " is disabled, switching to None" << std::endl;
                proceduralLayerMode_ = 0;
                // Also update slot 0
                if (!proceduralSlots_.empty()) {
                    proceduralSlots_[0].mode = 0;
                }
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

        std::string actualFilename = filename.empty() ? getDefaultSettingsPath() : filename;

        std::cout << "[SETTINGS SAVE] proceduralLayerMode=" << proceduralLayerMode_
                  << " slot0.mode=" << (proceduralSlots_.empty() ? -1 : proceduralSlots_[0].mode)
                  << " slot0.enabled=" << (proceduralSlots_.empty() ? false : proceduralSlots_[0].enabled)
                  << std::endl;

        // Save audio settings
        j["audio"]["selectedDevice"] = selectedDevice_;
        j["audio"]["inputGain"] = audioInputGain_;
        j["audio"]["visualSensitivity"] = visualSensitivity_;
        j["audio"]["audioEngineEnabled"] = audioEngineEnabled_;

        // Save UI settings with effect name
        j["ui"]["showImGuiWindow"] = showImGuiWindow_;
        j["ui"]["showCornerOrbs"] = showCornerOrbs_;
        j["ui"]["randomCornerOrbsEnabled"] = randomCornerOrbsEnabled_;
        j["ui"]["randomCornerOrbsInterval"] = randomCornerOrbsInterval_;
        j["ui"]["autoClearGhosting"] = autoClearGhosting_;
        j["ui"]["autoClearGhostingInterval"] = autoClearGhostingInterval_;
        j["ui"]["showProceduralLayer"] = showProceduralLayer_;
        j["ui"]["showCurrentEffects"] = showCurrentEffects_;
        j["ui"]["proceduralLayerDebug"] = proceduralLayerDebug_;
        j["ui"]["proceduralLayerOpacity"] = proceduralLayerOpacity_;
        j["ui"]["proceduralLayerMode"] = std::clamp(proceduralLayerMode_, 0, 73);
        // Also save effect name for robustness
        std::string mainEffectName = GetEffectRegistry().getEffectNameByIndex(proceduralLayerMode_);
        if (!mainEffectName.empty()) {
            j["ui"]["proceduralLayerEffectName"] = mainEffectName;
        }

        // Save post-processing settings
        for (size_t i = 0; i < postProcessSlots_.size(); ++i) {
            json slot;
            slot["enabled"] = postProcessSlots_[i].enabled;
            // Ensure mode is within valid range before saving
            slot["mode"] = std::clamp(postProcessSlots_[i].mode, 0, 31);
            slot["strength"] = postProcessSlots_[i].strength;
            slot["rgbAdjust"] = postProcessSlots_[i].rgbAdjust;
            j["postProcess"]["slots"].push_back(slot);
        }

        // Save procedural slots settings with name support
        for (size_t i = 0; i < proceduralSlots_.size(); ++i) {
            json slot;
            slot["enabled"] = proceduralSlots_[i].enabled;
            // Save both index (for compatibility) and name (for robustness)
            int modeIndex = std::clamp(proceduralSlots_[i].mode, 0, 75);
            slot["mode"] = modeIndex;
            std::string effectName = GetEffectRegistry().getEffectNameByIndex(modeIndex);
            if (!effectName.empty()) {
                slot["effectName"] = effectName;
            }
            slot["opacity"] = proceduralSlots_[i].opacity;
            slot["colorAdjust"] = proceduralSlots_[i].colorAdjust;
            j["proceduralSlots"]["slots"].push_back(slot);
        }

        // Save random settings
        j["random"]["postProcessEnabled"] = randomPostProcessEnabled_;
        j["random"]["postProcessInterval"] = randomPostProcessInterval_;
        j["random"]["postProcessSlotCount"] = randomPostProcessSlotCount_;
        j["random"]["proceduralEnabled"] = randomProceduralEnabled_;
        j["random"]["proceduralInterval"] = randomProceduralInterval_;
        j["random"]["postProcessSlotEnabled"] = randomPostProcessSlotEnabled_;
        j["random"]["postProcessSlotIndex"] = randomPostProcessSlotIndex_;
        j["random"]["proceduralSlotEnabled"] = randomProceduralSlotEnabled_;
        j["random"]["proceduralSlotIndex"] = randomProceduralSlotIndex_;
        
        // Save hot-reload setting
        j["hotReloadEnabled"] = hotReloadEnabled_;

        // Save manual BPM settings
        j["audio"]["manualBPMMode"] = manualBPMMode_;
        j["audio"]["manualBPM"] = manualBPM_;

        // Save post-processing random settings (legacy compatibility)
        j["postProcess"]["randomEnabled"] = randomPostProcessEnabled_;
        j["postProcess"]["randomInterval"] = randomPostProcessInterval_;

        // Save procedural random settings (legacy compatibility)
        j["procedural"]["randomEnabled"] = randomProceduralEnabled_;
        j["procedural"]["randomInterval"] = randomProceduralInterval_;

        // Save favorites
        json favoritesArray;
        for (const auto& fav : favorites_) {
            json favJson;
            favJson["name"] = fav.name;
            favJson["proceduralMode"] = fav.proceduralMode;
            favJson["proceduralOpacity"] = fav.proceduralOpacity;
            favJson["proceduralColorAdjust"] = fav.proceduralColorAdjust;

            json slotsArray;
            for (const auto& slot : fav.postProcessSlots) {
                json slotJson;
                slotJson["enabled"] = slot.enabled;
                slotJson["mode"] = slot.mode;
                slotJson["strength"] = slot.strength;
                slotJson["rgbAdjust"] = slot.rgbAdjust;
                slotsArray.push_back(slotJson);
            }
            favJson["postProcessSlots"] = slotsArray;
            favoritesArray.push_back(favJson);
        }
        j["favorites"] = favoritesArray;
        j["randomFavoritesOnly"] = randomFavoritesOnly_;

        // Save color palette settings
        j["colors"]["primary"] = scenePrimaryColor_;
        j["colors"]["secondary"] = sceneSecondaryColor_;
        j["colors"]["blend"] = scenePaletteBlend_;
        j["colors"]["paletteIndex"] = currentScenePaletteIndex_;
        j["colors"]["hueSeed"] = scenePaletteHueSeed_;
        j["colors"]["animationEnabled"] = colorAnimationEnabled_;

        // Save color animation settings
        j["animation"]["autoRandomize"] = autoRandomizeColors_;
        j["animation"]["randomInterval"] = colorRandomInterval_;
        j["animation"]["onsetColorCycling"] = onsetColorCyclingEnabled_;
        j["animation"]["autoRandomizeRgb"] = autoRandomizeRgbChannels_;
        j["animation"]["rgbRandomInterval"] = rgbRandomInterval_;
        
        // Save RGB channel settings
        j["rgbChannels"] = rgbChannelEnabled_;
        
        // Save per-shader enabled states
        json shaderStates;
        for (const auto& [modeIndex, enabled] : proceduralShaderEnabled_) {
            shaderStates[std::to_string(modeIndex)] = enabled;
        }
        j["proceduralShaderEnabled"] = shaderStates;
        
        // Save per-post-processing-effect enabled states
        json postProcessEffectStates;
        for (const auto& [modeIndex, enabled] : postProcessEffectEnabled_) {
            postProcessEffectStates[std::to_string(modeIndex)] = enabled;
        }
        j["postProcessEffectEnabled"] = postProcessEffectStates;

        // Save per-shader zoom values
        json zoomValues;
        for (const auto& [modeIndex, zoom] : proceduralZoomValues_) {
            zoomValues[std::to_string(modeIndex)] = zoom;
        }
        j["proceduralZoomValues"] = zoomValues;

        // Save per-shader offset values
        json offsetXValues;
        for (const auto& [modeIndex, offsetX] : proceduralOffsetXValues_) {
            offsetXValues[std::to_string(modeIndex)] = offsetX;
        }
        j["proceduralOffsetXValues"] = offsetXValues;

        json offsetYValues;
        for (const auto& [modeIndex, offsetY] : proceduralOffsetYValues_) {
            offsetYValues[std::to_string(modeIndex)] = offsetY;
        }
        j["proceduralOffsetYValues"] = offsetYValues;

        j["midi"]["tempoScale"] = midiTempoScale_;

        std::ofstream file(actualFilename);
        if (!file.is_open()) {
            std::cerr << "Failed to open settings file for writing: " << actualFilename << std::endl;
            return false;
        }

        file << j.dump(4); // Pretty print with 4 spaces indentation
        std::cout << "Settings saved successfully to: " << actualFilename << std::endl;
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

void SettingsManager::setProceduralSlots(const std::array<ProceduralSlot, 5>& slots) {
    proceduralSlots_ = slots;
}

bool SettingsManager::getProceduralShaderEnabled(int modeIndex) const {
    auto it = proceduralShaderEnabled_.find(modeIndex);
    if (it != proceduralShaderEnabled_.end()) {
        return it->second;
    }
    return true; // Default to enabled if not explicitly set
}

void SettingsManager::setProceduralShaderEnabled(int modeIndex, bool enabled) {
    proceduralShaderEnabled_[modeIndex] = enabled;
}

std::unordered_map<int, bool> SettingsManager::getAllProceduralShaderEnabled() const {
    return proceduralShaderEnabled_;
}

void SettingsManager::setAllProceduralShaderEnabled(const std::unordered_map<int, bool>& states) {
    proceduralShaderEnabled_ = states;
}

// Per-shader zoom methods
float SettingsManager::getProceduralZoom(int modeIndex) const {
    auto it = proceduralZoomValues_.find(modeIndex);
    if (it != proceduralZoomValues_.end()) {
        return it->second;
    }
    return 0.0f; // Return 0 to indicate no saved zoom (use default from shader)
}

void SettingsManager::setProceduralZoom(int modeIndex, float zoom) {
    proceduralZoomValues_[modeIndex] = zoom;
}

std::unordered_map<int, float> SettingsManager::getAllProceduralZooms() const {
    return proceduralZoomValues_;
}

void SettingsManager::setAllProceduralZooms(const std::unordered_map<int, float>& zooms) {
    proceduralZoomValues_ = zooms;
}

// Per-shader offset methods
float SettingsManager::getProceduralOffsetX(int modeIndex) const {
    auto it = proceduralOffsetXValues_.find(modeIndex);
    if (it != proceduralOffsetXValues_.end()) {
        return it->second;
    }
    return 0.0f; // Return 0 to indicate no saved offset (use default)
}

void SettingsManager::setProceduralOffsetX(int modeIndex, float offsetX) {
    proceduralOffsetXValues_[modeIndex] = offsetX;
}

float SettingsManager::getProceduralOffsetY(int modeIndex) const {
    auto it = proceduralOffsetYValues_.find(modeIndex);
    if (it != proceduralOffsetYValues_.end()) {
        return it->second;
    }
    return 0.0f; // Return 0 to indicate no saved offset (use default)
}

void SettingsManager::setProceduralOffsetY(int modeIndex, float offsetY) {
    proceduralOffsetYValues_[modeIndex] = offsetY;
}

std::unordered_map<int, float> SettingsManager::getAllProceduralOffsetXs() const {
    return proceduralOffsetXValues_;
}

std::unordered_map<int, float> SettingsManager::getAllProceduralOffsetYs() const {
    return proceduralOffsetYValues_;
}

void SettingsManager::setAllProceduralOffsetXs(const std::unordered_map<int, float>& offsetXs) {
    proceduralOffsetXValues_ = offsetXs;
}

void SettingsManager::setAllProceduralOffsetYs(const std::unordered_map<int, float>& offsetYs) {
    proceduralOffsetYValues_ = offsetYs;
}

// Post-processing effect enabled methods
bool SettingsManager::getPostProcessEffectEnabled(int modeIndex) const {
    auto it = postProcessEffectEnabled_.find(modeIndex);
    if (it != postProcessEffectEnabled_.end()) {
        return it->second;
    }
    return true; // Default to enabled if not explicitly set
}

void SettingsManager::setPostProcessEffectEnabled(int modeIndex, bool enabled) {
    postProcessEffectEnabled_[modeIndex] = enabled;
}

std::unordered_map<int, bool> SettingsManager::getAllPostProcessEffectEnabled() const {
    return postProcessEffectEnabled_;
}

void SettingsManager::setAllPostProcessEffectEnabled(const std::unordered_map<int, bool>& states) {
    postProcessEffectEnabled_ = states;
}

std::string SettingsManager::getPresetsDirectory() {
    return "presets/";
}

bool SettingsManager::saveShaderPreset(const std::string& presetName, int currentMode) {
    try {
        // Ensure presets directory exists
        std::string dir = getPresetsDirectory();
        std::filesystem::create_directories(dir);
        
        json j;
        j["name"] = presetName;
        j["activeMode"] = currentMode;
        
        // Get current timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        j["timestamp"] = ss.str();
        
        // Save shader states
        json shaderStates;
        for (const auto& [modeIndex, enabled] : proceduralShaderEnabled_) {
            shaderStates[std::to_string(modeIndex)] = enabled;
        }
        j["shaderStates"] = shaderStates;
        
        // Write to file
        std::string filename = dir + presetName + ".json";
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "[PRESET] Failed to open file for writing: " << filename << std::endl;
            return false;
        }
        
        file << j.dump(4);
        std::cout << "[PRESET] Saved preset: " << presetName << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[PRESET] Error saving preset: " << e.what() << std::endl;
        return false;
    }
}

bool SettingsManager::loadShaderPreset(const std::string& presetName, 
                                          std::unordered_map<int, bool>& outStates,
                                          int& outActiveMode) {
    try {
        std::string filename = getPresetsDirectory() + presetName + ".json";
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "[PRESET] Failed to open file for reading: " << filename << std::endl;
            return false;
        }
        
        json j;
        file >> j;
        
        // Load active mode
        if (j.contains("activeMode")) {
            outActiveMode = j["activeMode"];
        } else {
            outActiveMode = 0;
        }
        
        // Load shader states
        outStates.clear();
        if (j.contains("shaderStates")) {
            const auto& shaderStates = j["shaderStates"];
            for (auto& [key, value] : shaderStates.items()) {
                int modeIndex = std::stoi(key);
                outStates[modeIndex] = value.get<bool>();
            }
        }
        
        std::cout << "[PRESET] Loaded preset: " << presetName << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[PRESET] Error loading preset: " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::string> SettingsManager::listShaderPresets() const {
    std::vector<std::string> presets;
    
    try {
        std::string dir = getPresetsDirectory();
        if (!std::filesystem::exists(dir)) {
            return presets; // Empty list
        }
        
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                // Remove .json extension
                std::string name = entry.path().stem().string();
                presets.push_back(name);
            }
        }
        
        // Sort alphabetically
        std::sort(presets.begin(), presets.end());
        
    } catch (const std::exception& e) {
        std::cerr << "[PRESET] Error listing presets: " << e.what() << std::endl;
    }
    
    return presets;
}

bool SettingsManager::deleteShaderPreset(const std::string& presetName) {
    try {
        std::string filename = getPresetsDirectory() + presetName + ".json";
        
        if (!std::filesystem::exists(filename)) {
            std::cerr << "[PRESET] Preset not found: " << presetName << std::endl;
            return false;
        }
        
        std::filesystem::remove(filename);
        std::cout << "[PRESET] Deleted preset: " << presetName << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[PRESET] Error deleting preset: " << e.what() << std::endl;
        return false;
    }
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

void SettingsManager::addFavorite(const FavoritePreset& favorite) {
    favorites_.push_back(favorite);
}

void SettingsManager::removeFavorite(size_t index) {
    if (index < favorites_.size()) {
        favorites_.erase(favorites_.begin() + index);
    }
}
