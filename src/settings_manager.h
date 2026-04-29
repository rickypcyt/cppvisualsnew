#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <string>
#include <array>
#include <vector>
#include <algorithm>
#include <memory>
#include <unordered_map>

struct PostProcessSlot {
    bool enabled = false;
    int mode = 0;
    float strength = 0.0f;
    std::array<float, 3> rgbAdjust{1.0f, 1.0f, 1.0f};
};

struct ProceduralSlot {
    bool enabled = false;
    int mode = 0;
    float opacity = 1.0f;
    std::array<float, 3> colorAdjust{1.0f, 1.0f, 1.0f};
};

struct FavoritePreset {
    std::string name;
    int proceduralMode;
    float proceduralOpacity;
    std::array<float, 3> proceduralColorAdjust{1.0f, 1.0f, 1.0f};
    std::array<PostProcessSlot, 5> postProcessSlots;
};

class SettingsManager {
public:
    SettingsManager();
    ~SettingsManager();

    // Load settings from JSON file
    bool loadSettings(const std::string& filename = "");
    
    // Save settings to JSON file
    bool saveSettings(const std::string& filename = "");

    // Get default settings file path (outside build directory)
    static std::string getDefaultSettingsPath();

    // Migrate old settings from current directory to new config directory
    static bool migrateOldSettings();

    // Getters and setters for all settings
    int getSelectedDevice() const { return selectedDevice_; }
    void setSelectedDevice(int device) { selectedDevice_ = device; }

    float getAudioInputGain() const { return audioInputGain_; }
    void setAudioInputGain(float gain) { audioInputGain_ = gain; }

    bool getAudioEngineEnabled() const { return audioEngineEnabled_; }
    void setAudioEngineEnabled(bool enabled) { audioEngineEnabled_ = enabled; }

    float getVisualSensitivity() const { return visualSensitivity_; }
    void setVisualSensitivity(float sensitivity) { visualSensitivity_ = sensitivity; }

    bool getShowImGuiWindow() const { return showImGuiWindow_; }
    void setShowImGuiWindow(bool show) { showImGuiWindow_ = show; }

    bool getShowCornerOrbs() const { return showCornerOrbs_; }
    void setShowCornerOrbs(bool show) { showCornerOrbs_ = show; }

    bool getRandomCornerOrbsEnabled() const { return randomCornerOrbsEnabled_; }
    void setRandomCornerOrbsEnabled(bool enabled) { randomCornerOrbsEnabled_ = enabled; }

    float getRandomCornerOrbsInterval() const { return randomCornerOrbsInterval_; }
    void setRandomCornerOrbsInterval(float interval) { randomCornerOrbsInterval_ = interval; }

    bool getAutoClearGhosting() const { return autoClearGhosting_; }
    void setAutoClearGhosting(bool enabled) { autoClearGhosting_ = enabled; }

    float getAutoClearGhostingInterval() const { return autoClearGhostingInterval_; }
    void setAutoClearGhostingInterval(float interval) { autoClearGhostingInterval_ = interval; }

    bool getShowProceduralLayer() const { return showProceduralLayer_; }
    void setShowProceduralLayer(bool show) { showProceduralLayer_ = show; }

    bool getShowCurrentEffects() const { return showCurrentEffects_; }
    void setShowCurrentEffects(bool show) { showCurrentEffects_ = show; }

    bool getProceduralLayerDebug() const { return proceduralLayerDebug_; }
    void setProceduralLayerDebug(bool debug) { proceduralLayerDebug_ = debug; }

    float getProceduralLayerOpacity() const { return proceduralLayerOpacity_; }
    void setProceduralLayerOpacity(float opacity) { proceduralLayerOpacity_ = opacity; }

    int getProceduralLayerMode() const { return proceduralLayerMode_; }
    void setProceduralLayerMode(int mode) { proceduralLayerMode_ = mode; }

    const std::array<PostProcessSlot, 5>& getPostProcessSlots() const { return postProcessSlots_; }
    void setPostProcessSlots(const std::array<PostProcessSlot, 5>& slots);

    const std::array<ProceduralSlot, 5>& getProceduralSlots() const { return proceduralSlots_; }
    void setProceduralSlots(const std::array<ProceduralSlot, 5>& slots);

    // Favorites presets
    const std::vector<FavoritePreset>& getFavorites() const { return favorites_; }
    void setFavorites(const std::vector<FavoritePreset>& favorites) { favorites_ = favorites; }
    void addFavorite(const FavoritePreset& favorite);
    void removeFavorite(size_t index);

    bool getRandomFavoritesOnly() const { return randomFavoritesOnly_; }
    void setRandomFavoritesOnly(bool enabled) { randomFavoritesOnly_ = enabled; }

    const std::array<float, 3>& getScenePrimaryColor() const { return scenePrimaryColor_; }
    void setScenePrimaryColor(const std::array<float, 3>& color) { scenePrimaryColor_ = color; }

    const std::array<float, 3>& getSceneSecondaryColor() const { return sceneSecondaryColor_; }
    void setSceneSecondaryColor(const std::array<float, 3>& color) { sceneSecondaryColor_ = color; }

    float getScenePaletteBlend() const { return scenePaletteBlend_; }
    void setScenePaletteBlend(float blend) { scenePaletteBlend_ = blend; }

    int getCurrentScenePaletteIndex() const { return currentScenePaletteIndex_; }
    void setCurrentScenePaletteIndex(int index) { currentScenePaletteIndex_ = index; }

    float getScenePaletteHueSeed() const { return scenePaletteHueSeed_; }
    void setScenePaletteHueSeed(float seed) { scenePaletteHueSeed_ = seed; }

    bool getColorAnimationEnabled() const { return colorAnimationEnabled_; }
    void setColorAnimationEnabled(bool enabled) { colorAnimationEnabled_ = enabled; }

    bool getAutoRandomizeColors() const { return autoRandomizeColors_; }
    void setAutoRandomizeColors(bool autoRandomize) { autoRandomizeColors_ = autoRandomize; }

    float getColorRandomInterval() const { return colorRandomInterval_; }
    void setColorRandomInterval(float interval) { colorRandomInterval_ = interval; }

    bool getOnsetColorCyclingEnabled() const { return onsetColorCyclingEnabled_; }
    void setOnsetColorCyclingEnabled(bool enabled) { onsetColorCyclingEnabled_ = enabled; }

    bool getAutoRandomizeRgbChannels() const { return autoRandomizeRgbChannels_; }
    void setAutoRandomizeRgbChannels(bool enabled) { autoRandomizeRgbChannels_ = enabled; }
    
    bool getManualBPMMode() const { return manualBPMMode_; }
    void setManualBPMMode(bool enabled) { manualBPMMode_ = enabled; }
    
    float getManualBPM() const { return manualBPM_; }
    void setManualBPM(float bpm) { manualBPM_ = std::clamp(bpm, 5.0f, 200.0f); }

    bool getRgbChannelEnabled(int channel) const;
    void setRgbChannelEnabled(int channel, bool enabled);

    // Per-shader enabled states
    bool getProceduralShaderEnabled(int modeIndex) const;
    void setProceduralShaderEnabled(int modeIndex, bool enabled);
    std::unordered_map<int, bool> getAllProceduralShaderEnabled() const;
    void setAllProceduralShaderEnabled(const std::unordered_map<int, bool>& states);

    // Per-post-processing-effect enabled states
    bool getPostProcessEffectEnabled(int modeIndex) const;
    void setPostProcessEffectEnabled(int modeIndex, bool enabled);
    std::unordered_map<int, bool> getAllPostProcessEffectEnabled() const;
    void setAllPostProcessEffectEnabled(const std::unordered_map<int, bool>& states);

    // Per-shader zoom values (persist user adjustments)
    float getProceduralZoom(int modeIndex) const;
    void setProceduralZoom(int modeIndex, float zoom);
    std::unordered_map<int, float> getAllProceduralZooms() const;
    void setAllProceduralZooms(const std::unordered_map<int, float>& zooms);

    // Per-shader offset values (persist user adjustments)
    float getProceduralOffsetX(int modeIndex) const;
    void setProceduralOffsetX(int modeIndex, float offsetX);
    float getProceduralOffsetY(int modeIndex) const;
    void setProceduralOffsetY(int modeIndex, float offsetY);
    std::unordered_map<int, float> getAllProceduralOffsetXs() const;
    std::unordered_map<int, float> getAllProceduralOffsetYs() const;
    void setAllProceduralOffsetXs(const std::unordered_map<int, float>& offsetXs);
    void setAllProceduralOffsetYs(const std::unordered_map<int, float>& offsetYs);

    // Preset management for shader enable/disable configurations
    struct ShaderPreset {
        std::string name;
        std::unordered_map<int, bool> shaderStates;
        int activeMode = 0; // Which shader mode is currently active (0 = None)
        std::string timestamp;
    };
    
    // Save current shader states as a preset
    bool saveShaderPreset(const std::string& presetName, int currentMode);
    // Load shader preset by name
    bool loadShaderPreset(const std::string& presetName, std::unordered_map<int, bool>& outStates, int& outActiveMode);
    // Get list of all saved preset names
    std::vector<std::string> listShaderPresets() const;
    // Delete a preset
    bool deleteShaderPreset(const std::string& presetName);
    // Get preset directory path
    static std::string getPresetsDirectory();

    // Random settings getters/setters
    bool getRandomPostProcessEnabled() const { return randomPostProcessEnabled_; }
    void setRandomPostProcessEnabled(bool enabled) { randomPostProcessEnabled_ = enabled; }

    float getRandomPostProcessInterval() const { return randomPostProcessInterval_; }
    void setRandomPostProcessInterval(float interval) { randomPostProcessInterval_ = interval; }
    int getRandomPostProcessSlotCount() const { return randomPostProcessSlotCount_; }
    void setRandomPostProcessSlotCount(int count) { randomPostProcessSlotCount_ = std::clamp(count, 1, 5); }

    bool getRandomProceduralEnabled() const { return randomProceduralEnabled_; }
    void setRandomProceduralEnabled(bool enabled) { randomProceduralEnabled_ = enabled; }

    float getRandomProceduralInterval() const { return randomProceduralInterval_; }
    void setRandomProceduralInterval(float interval) { randomProceduralInterval_ = interval; }

    bool getHotReloadEnabled() const { return hotReloadEnabled_; }
    void setHotReloadEnabled(bool enabled) { hotReloadEnabled_ = enabled; }

    // Set all settings from current visualizer state
    void updateFromVisualizerState();

    // Apply all settings to visualizer
    void applyToVisualizer();

private:
    // Audio settings
    int selectedDevice_ = -1;
    float audioInputGain_ = 1.0f;
    float visualSensitivity_ = 1.0f;
    bool audioEngineEnabled_ = true;

    // UI settings
    bool showImGuiWindow_ = true;
    bool showCornerOrbs_ = true;
    bool randomCornerOrbsEnabled_ = false;
    float randomCornerOrbsInterval_ = 5.0f;
    bool autoClearGhosting_ = true;
    float autoClearGhostingInterval_ = 1.0f;
    bool showProceduralLayer_ = true;
    bool showCurrentEffects_ = true;
    bool proceduralLayerDebug_ = false;
    float proceduralLayerOpacity_ = 0.85f;
    int proceduralLayerMode_ = 23; // Voxel Path Tracer

    // Post-processing settings
    std::array<PostProcessSlot, 5> postProcessSlots_;
    
    // Procedural slots settings
    std::array<ProceduralSlot, 5> proceduralSlots_;
    
    // Random settings
    bool randomPostProcessEnabled_ = false;
    float randomPostProcessInterval_ = 5.0f;
    int randomPostProcessSlotCount_ = 1; // Number of slots to randomize (1-5)
    bool randomProceduralEnabled_ = false;
    float randomProceduralInterval_ = 8.0f;
    bool randomPostProcessSlotEnabled_ = false;
    int randomPostProcessSlotIndex_ = 0;
    bool randomProceduralSlotEnabled_ = false;
    int randomProceduralSlotIndex_ = 0;
    bool showPostProcess_ = true;

    // Favorites presets
    std::vector<FavoritePreset> favorites_;
    bool randomFavoritesOnly_ = false;

    // Color palette settings
    std::array<float, 3> scenePrimaryColor_{0.25f, 0.32f, 0.58f};
    std::array<float, 3> sceneSecondaryColor_{0.35f, 0.65f, 0.92f};
    float scenePaletteBlend_ = 0.6f;
    int currentScenePaletteIndex_ = -1;
    float scenePaletteHueSeed_ = 0.0f;
    bool colorAnimationEnabled_ = true;

    // Color animation settings
    bool autoRandomizeColors_;
    float colorRandomInterval_;
    float colorRandomTimer_;

    float midiTempoScale_ = 1.0f;
    
    // Hot-reload settings
    bool hotReloadEnabled_ = false;
    
    // Manual BPM settings
    bool manualBPMMode_ = false;
    float manualBPM_ = 120.0f;

    bool onsetColorCyclingEnabled_ = true;
    bool autoRandomizeRgbChannels_ = false;
    float rgbRandomInterval_ = 5.0f;

    // RGB channel settings
    std::array<bool, 3> rgbChannelEnabled_{true, true, true};
    
    // Per-shader enabled states (mode index -> enabled)
    std::unordered_map<int, bool> proceduralShaderEnabled_;
    
    // Per-post-processing-effect enabled states (mode index -> enabled)
    std::unordered_map<int, bool> postProcessEffectEnabled_;

    // Per-shader zoom values (mode index -> zoom)
    std::unordered_map<int, float> proceduralZoomValues_;

    // Per-shader offset values (mode index -> offset)
    std::unordered_map<int, float> proceduralOffsetXValues_;
    std::unordered_map<int, float> proceduralOffsetYValues_;
};

#endif // SETTINGS_MANAGER_H
