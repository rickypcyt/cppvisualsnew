#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <string>
#include <array>
#include <vector>
#include <memory>

struct PostProcessSlot {
    bool enabled = false;
    int mode = 0;
    float strength = 0.0f;
    std::array<float, 3> rgbAdjust{1.0f, 1.0f, 1.0f};
};

class SettingsManager {
public:
    SettingsManager();
    ~SettingsManager();

    // Load settings from JSON file
    bool loadSettings(const std::string& filename = "visualizer_settings.json");
    
    // Save settings to JSON file
    bool saveSettings(const std::string& filename = "visualizer_settings.json");

    // Getters and setters for all settings
    int getSelectedDevice() const { return selectedDevice_; }
    void setSelectedDevice(int device) { selectedDevice_ = device; }

    float getAudioInputGain() const { return audioInputGain_; }
    void setAudioInputGain(float gain) { audioInputGain_ = gain; }

    float getVisualSensitivity() const { return visualSensitivity_; }
    void setVisualSensitivity(float sensitivity) { visualSensitivity_ = sensitivity; }

    bool getShowImGuiWindow() const { return showImGuiWindow_; }
    void setShowImGuiWindow(bool show) { showImGuiWindow_ = show; }

    bool getShowCornerOrbs() const { return showCornerOrbs_; }
    void setShowCornerOrbs(bool show) { showCornerOrbs_ = show; }

    bool getShowProceduralLayer() const { return showProceduralLayer_; }
    void setShowProceduralLayer(bool show) { showProceduralLayer_ = show; }

    bool getProceduralLayerDebug() const { return proceduralLayerDebug_; }
    void setProceduralLayerDebug(bool debug) { proceduralLayerDebug_ = debug; }

    float getProceduralLayerOpacity() const { return proceduralLayerOpacity_; }
    void setProceduralLayerOpacity(float opacity) { proceduralLayerOpacity_ = opacity; }

    int getProceduralLayerMode() const { return proceduralLayerMode_; }
    void setProceduralLayerMode(int mode) { proceduralLayerMode_ = mode; }

    const std::array<PostProcessSlot, 5>& getPostProcessSlots() const { return postProcessSlots_; }
    void setPostProcessSlots(const std::array<PostProcessSlot, 5>& slots);

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

    bool getAutoRandomizeColors() const { return autoRandomizeColors_; }
    void setAutoRandomizeColors(bool autoRandomize) { autoRandomizeColors_ = autoRandomize; }

    float getColorRandomInterval() const { return colorRandomInterval_; }
    void setColorRandomInterval(float interval) { colorRandomInterval_ = interval; }

    bool getOnsetColorCyclingEnabled() const { return onsetColorCyclingEnabled_; }
    void setOnsetColorCyclingEnabled(bool enabled) { onsetColorCyclingEnabled_ = enabled; }

    bool getRgbChannelEnabled(int channel) const;
    void setRgbChannelEnabled(int channel, bool enabled);

    // Set all settings from current visualizer state
    void updateFromVisualizerState();

    // Apply all settings to visualizer
    void applyToVisualizer();

private:
    // Audio settings
    int selectedDevice_ = -1;
    float audioInputGain_ = 1.0f;
    float visualSensitivity_ = 1.0f;

    // UI settings
    bool showImGuiWindow_ = true;
    bool showCornerOrbs_ = true;
    bool showProceduralLayer_ = true;
    bool proceduralLayerDebug_ = false;
    float proceduralLayerOpacity_ = 0.85f;
    int proceduralLayerMode_ = 23; // Voxel Path Tracer

    // Post-processing settings
    std::array<PostProcessSlot, 5> postProcessSlots_;
    bool showPostProcess_ = true;

    // Color palette settings
    std::array<float, 3> scenePrimaryColor_{0.25f, 0.32f, 0.58f};
    std::array<float, 3> sceneSecondaryColor_{0.35f, 0.65f, 0.92f};
    float scenePaletteBlend_ = 0.6f;
    int currentScenePaletteIndex_ = -1;
    float scenePaletteHueSeed_ = 0.0f;

    // Color animation settings
    bool autoRandomizeColors_ = true;
    float colorRandomInterval_ = 12.0f;
    bool onsetColorCyclingEnabled_ = true;

    // RGB channel settings
    std::array<bool, 3> rgbChannelEnabled_{true, true, true};
};

#endif // SETTINGS_MANAGER_H
