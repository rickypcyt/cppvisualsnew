#ifndef VISUALIZER_H
#define VISUALIZER_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include <array>
#include "audio_analyzer.h"
#include "shader.h"
#include "modular_layer.h"
#include "post_processor.h"
#include "settings_manager.h"
#include "midi_controller.h"

// Forward declarations for ImGui
struct ImGuiIO;

class Visualizer {
public:
    // Static mode name arrays - single source of truth
    static const char* const kProceduralModes[];
    static const char* const kPostProcessModes[];
    
    Visualizer();
    ~Visualizer();

    bool initialize(int width, int height);
    void shutdown();
    bool shouldClose();
    void beginFrame();
    void endFrame();
    void updateAudioData(const AudioAnalyzer::AudioFeatures& features);
    void updateAudioBuffer(const std::vector<float>& audioBuffer);
    void render();
    void renderGUI();
    bool showDeviceSelector();
    int getSelectedDevice() const { return selectedDevice_; }
    void setSelectedDevice(int device) { selectedDevice_ = device; }
    float getAudioInputGain() const { return audioInputGain_; }
    void setAudioInputGain(float gain) { audioInputGain_ = gain; }
    float getVisualSensitivity() const { return visualSensitivity_; }
    void setVisualSensitivity(float sensitivity) { visualSensitivity_ = sensitivity; }
    void toggleDiagnosticMode() { showDiagnostic_ = !showDiagnostic_; }
    bool isDiagnosticMode() const { return showDiagnostic_; }
    void toggleConsoleMode() { consoleMode_ = !consoleMode_; }
    bool isConsoleMode() const { return consoleMode_; }
    void handleFeatureToggleKeys();
    
    // Settings management
    void updateSettingsFromCurrentState();
    void saveCurrentSettings();
    
    // Random post process methods
    void updateRandomPostProcess(float deltaTime);
    void initializeRandomPostProcess();
    void selectRandomPostProcess();
    
    // Random procedural layer methods
    void updateRandomProcedural(float deltaTime);
    void initializeRandomProcedural();
    void selectRandomProcedural();
    void syncProceduralLayerWithSlot1(); // Sync procedural layer with Slot 1 state
    void renderNoImGuiLoop();
    
    // ImGui methods
    bool setupImGui();
    void renderCurrentEffectsDisplay();
    void shutdownImGui();
    void renderImGui();

private:

    struct ScenePalette {
        std::string name;
        std::array<float, 3> primary{};
        std::array<float, 3> secondary{};
        float blend = 0.5f;
    };

    struct CoreState {
        float radius = 0.0f;
        float baseRadius = 0.0f;
        float pulse = 0.0f;
        float energy = 0.0f;
        float kickEnvelope = 0.0f;
        float harmonicEnvelope = 0.0f;
        float growthTrend = 0.0f;
        float growthEnvelope = 0.0f;
        float maturity = 0.0f;
        float idlePulse = 0.0f;
        float idleWarp = 0.0f;
        float idleSpin = 0.0f;
    };

    struct GearNode {
        float x;
        float y;
        float radius;
        float baseRadius;
        float ringRadius;
        float angle;
        float angularVelocity;
        float orbitAngle;
        float orbitSpeed;
        int teeth;
        float jitterPhase;
        float orbitDirection;
        float anchorAngle;
        float anchorRadius;
        int ringIndex;
        float baseOffsetRadius;
        float baseOffsetAngle;
        float baseCenterX;
        float baseCenterY;
    };

    static constexpr int kMaxPostProcessSlots = 5;
    static constexpr int kMaxProceduralSlots = 5;
    
    // Procedural layer constants
    static constexpr int kProceduralModeCount = 47;
    
    // Post-process constants
    static constexpr int kPostProcessModeCount = 24;
    static constexpr int kPostProcessKaleidoscopeModeIndex = 7;
    static constexpr int kPostProcessGrayscaleModeIndex = 1;
    static constexpr int kDefaultPostProcessMode = 2;
    static constexpr float kDefaultPostProcessStrength = 0.65f;
    static constexpr int kKaleidoscopeSlotIndex = 1;
    static constexpr float kDefaultKaleidoscopeStrength = 0.75f;

    // Settings manager
    std::unique_ptr<SettingsManager> settingsManager_;

    struct LifeCellGrid {
        static constexpr int WIDTH = 96;
        static constexpr int HEIGHT = 96;
        std::array<uint8_t, WIDTH * HEIGHT> cells{};
        std::array<uint8_t, WIDTH * HEIGHT> next{};
    };

    GLFWwindow* window_;
    int windowWidth_;
    int windowHeight_;
    float time_;

    // OpenGL objects
    GLuint quadVAO_;
    GLuint quadVBO_;
    GLuint waveformVAO_;
    GLuint waveformVBO_;
    GLuint coreVAO_;
    GLuint coreVBO_;
    GLsizei coreVertexCount_;
    GLuint sparkVAO_;
    GLuint sparkVBO_;
    GLsizei sparkVertexCount_;
    GLuint cornerVAO_;
    GLuint cornerVBO_;
    GLsizei cornerVertexCount_;
    GLuint doodadVAO_;
    GLuint doodadVBO_;

    std::unique_ptr<Shader> shader_;
    std::unique_ptr<Shader> coreShader_;
    std::unique_ptr<Shader> sparkShader_;
    std::unique_ptr<Shader> cornerShader_;
    std::unique_ptr<Shader> doodadShader_;
    ModularLayer proceduralLayer_;
    PostProcessor postProcessor_;
    
    // MIDI controller
    std::unique_ptr<MidiController> midiController_;
    bool midiEnabled_ = false;
    std::array<PostProcessSlot, kMaxPostProcessSlots> postProcessSlots_;
    std::array<ProceduralSlot, kMaxProceduralSlots> proceduralSlots_;
    
    // Post-process controls
    int postProcessMode_;
    float postProcessStrength_;
    std::array<float, 3> postProcessRgbAdjust_;
    
    // Random post process cycle
    bool randomPostProcessEnabled_ = false;
    float randomPostProcessInterval_ = 5.0f; // seconds
    float randomPostProcessTimer_ = 0.0f;
    int currentRandomPostProcess_ = 0;
    std::vector<int> availablePostProcessModes_;
    
    // Random procedural layer cycle
    bool randomProceduralEnabled_ = false;
    float randomProceduralInterval_ = 8.0f; // seconds
    float randomProceduralTimer_ = 0.0f;
    int currentRandomProcedural_ = 0;
    std::vector<int> availableProceduralModes_;
    
    AudioAnalyzer::AudioFeatures audioFeatures_;
    std::vector<float> waveformBuffer_;
    int selectedDevice_;
    bool showDeviceMenu_;
    bool showDiagnostic_;
    bool consoleMode_;
    std::vector<std::string> deviceNames_;
    std::vector<bool> deviceIsInternal_;
    std::string rendererName_;
    std::string openglVersion_;
    float audioInputGain_;
    bool useModernPipeline_;

    // ImGui state
    bool showImGuiWindow_;
    bool showImGuiVisualWindow_;
    bool showDeviceSelector_;
    bool showCurrentEffects_ = true; // Current Effects window (independent, controlled by 'I' key)
    bool showDiagnosticInfo_;
    bool showConsoleMode_;
    bool imguiInitialized_;
    bool autoRandomizeColors_;
    float colorRandomInterval_;
    float colorRandomTimer_;
    std::array<bool, 3> rgbChannelEnabled_{};
    float globalIntensityEnvelope_;
    float visualSensitivity_;
    float tempoMultiplier_ = 1.0f;
    float midiTempoScale_ = 1.0f;
    
    // Manual BPM mode
    bool manualBPMMode_ = false;
    float manualBPM_ = 120.0f;
    
    // Scene palette settings
    std::array<float, 3> scenePrimaryColor_{0.25f, 0.32f, 0.58f};
    std::array<float, 3> sceneSecondaryColor_{0.35f, 0.65f, 0.92f};
    float scenePaletteBlend_ = 0.6f;
    int currentScenePaletteIndex_ = -1;
    float scenePaletteHueSeed_ = 0.0f;
    
    bool onsetColorCyclingEnabled_ = true;
    bool autoRandomizeRgbChannels_ = false;
    float rgbRandomTimer_ = 0.0f;
    float rgbRandomInterval_ = 3.0f;
    float lastRgbRandomTime_ = 0.0f;
    int lastOnsetCount_ = 0;
    
    // Random number generator
    std::mt19937 rng_;
    
    // Scene palettes
    std::vector<ScenePalette> scenePalettes_;
    
    // Timing
    float deltaTime_ = 0.0f;
    
    // Onset detection
    bool lastOnsetActive_ = false;
    int onsetTriggerCount_ = 0;
    
    bool coreShowSpokes_;
    bool coreShowRunes_;
    bool coreShowSparkles_;
    bool coreShowBloom_;
    bool showCornerOrbs_;
    bool showProceduralLayer_;
    bool showWaveformOverlay_;
    bool showPostProcess_;
    bool proceduralLayerDebug_;
    float proceduralLayerOpacity_;
    int proceduralLayerMode_;

    CoreState core_;
    std::vector<GearNode> gears_;
    LifeCellGrid lifeGrid_;
    float lifeTimeAccumulator_ = 0.0f;
    float gearSpawnRadius_ = 0.0f;
    float lastLifeSeedTime_ = 0.0f;
    float idleState_ = 0.0f;
    float idlePhase_ = 0.0f;

    bool setupOpenGL();
    bool setupGeometry();
    bool loadShaders();
    bool loadCoreShader();
    bool loadSparkShader();
    bool loadCornerShader();
    bool loadDoodadShader();
    void setupQuad();
    void setupWaveform();
    void setupCoreMesh();
    void setupSparkField();
    void setupCornerQuad();
    void setupDoodadMesh();
    void renderWaveform(const std::vector<float>& audioBuffer);
    void renderText(const std::string& text, float x, float y);
    void renderConsoleVisualization();
    void renderFallbackTriangle();
    void renderModernVisualization();
    void renderModernCore();
    void renderShaderSparkles();
    void renderCornerOrbs();
    void renderDoodadObject();
    void renderProceduralLayer();
    void renderCore();
    void handleVisualizationShortcuts();
    
    // Audio device methods
    void setupDeviceList();
    
    // Diagnostic methods
    void renderDiagnosticInfo();
    
    // MIDI methods
    bool initializeMIDI();
    void shutdownMIDI();
    void setupMIDIMappings();
    void renderMIDIControls();
    const char* getMIDIMappingName(int cc);

    // ImGui rendering methods
    void renderMainImGuiWindow();
    void renderDeviceSelectorImGui();
    void renderDiagnosticImGui();
    void renderConsoleImGui();

    void buildScenePalettes();
    void applyScenePalette(int index);
    void setDefaultScenePalette();
    void randomizeScenePalette();
    void cycleScenePaletteSequential();
    void updateDynamicScenePalette();
    void randomizeRgbChannels();
    void setUnifiedPalette(const std::array<float, 3>& primary,
                           const std::array<float, 3>& secondary,
                           float blend);


    // Procedural visualization methods
    void renderProceduralVisualization();
    void renderReactiveCircle(float bass, float mid, float high);
    void renderFrequencyBars(float bass, float mid, float high);
    void renderHighFrequencySparkles(float high);
    void renderWaveformVisualization();
    

    void initializeDynamicSystems();
    void updateCore(float dt, const AudioAnalyzer::AudioFeatures& features);
    float sampleCoreEnergyField(float x, float y) const;
    void updateGears(float dt, const AudioAnalyzer::AudioFeatures& features);
    void renderGears(float animatedTime) const;

    void seedLifeFromCore(float amount);
    void updateLife(float dt, const AudioAnalyzer::AudioFeatures& features);
    void renderLife(float animatedTime) const;
    void renderIdleSpinner(float animatedTime) const;

};

#endif // VISUALIZER_H
