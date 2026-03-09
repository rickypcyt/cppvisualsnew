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

// Forward declarations for ImGui
struct ImGuiIO;

class Visualizer {
public:
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
    void toggleDiagnosticMode() { showDiagnostic_ = !showDiagnostic_; }
    bool isDiagnosticMode() const { return showDiagnostic_; }
    void toggleConsoleMode() { consoleMode_ = !consoleMode_; }
    bool isConsoleMode() const { return consoleMode_; }
    
    // No-ImGui methods
    void renderNoImGuiLoop();
    
    // ImGui methods
    bool setupImGui();
    void shutdownImGui();
    void renderImGui();

private:
    struct ColorAdjust {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float a = 1.0f;

        float* data() { return &r; }
        const float* data() const { return &r; }
    };

    struct LegacyColorAdjust {
        ColorAdjust circleFill;
        ColorAdjust circleOutline;
        ColorAdjust bloomInner;
        ColorAdjust bloomOuter;
        ColorAdjust bassBars;
        ColorAdjust midBars;
        ColorAdjust highBars;
        ColorAdjust beatExplosion;
        ColorAdjust rings;
        ColorAdjust orbit;
        ColorAdjust orbitTrail;
        ColorAdjust sparkles;
        ColorAdjust waveform;
    };

    struct ColorPreset {
        std::string name;
        LegacyColorAdjust adjust;
    };

    struct CoreState {
        float radius = 0.0f;
        float baseRadius = 0.0f;
        float pulse = 0.0f;
        float energy = 0.0f;
        float kickEnvelope = 0.0f;
        float harmonicEnvelope = 0.0f;
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

    std::unique_ptr<Shader> shader_;
    std::unique_ptr<Shader> coreShader_;
    AudioAnalyzer::AudioFeatures audioFeatures_;
    std::vector<float> waveformBuffer_;
    int selectedDevice_;
    bool showDeviceMenu_;
    bool showDiagnostic_;
    bool consoleMode_;
    std::vector<std::string> deviceNames_;
    std::vector<bool> deviceIsInternal_;
    std::string rendererName_;
    float legacyMotionBlend_;
    float legacyMotionPhase_;
    float legacySensitivity_;
    bool showLegacyCore_;
    bool showLegacyArcs_;
    bool showLegacyRings_;
    bool showLegacySparkles_;
    bool showLegacyOrbs_;
    bool showLegacyWaveforms_;
    bool useModernPipeline_;

    // ImGui state
    bool showImGuiWindow_;
    bool showDeviceSelector_;
    bool showDiagnosticInfo_;
    bool showConsoleMode_;
    bool imguiInitialized_;
    LegacyColorAdjust legacyColorAdjust_;
    bool autoRandomizeColors_;
    float colorRandomInterval_;
    float colorRandomTimer_;
    float deltaTime_;
    std::mt19937 rng_;
    std::vector<ColorPreset> colorPresets_;
    int currentPresetIndex_;
    bool onsetColorCyclingEnabled_;
    int onsetTriggerCount_;
    bool lastOnsetActive_;
    float tempoMultiplier_;
    bool mixColorSchemes_;
    bool overlayLegacyOnModern_;

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
    void setupQuad();
    void setupWaveform();
    void setupCoreMesh();
    void renderWaveform(const std::vector<float>& audioBuffer);
    void renderText(const std::string& text, float x, float y);
    void setupDeviceList();
    void renderDiagnosticInfo();
    void renderConsoleVisualization();
    void renderFallbackTriangle();
    void renderModernVisualization();
    void renderModernCore();

    // ImGui rendering methods
    void renderMainImGuiWindow();
    void renderDeviceSelectorImGui();
    void renderDiagnosticImGui();
    void renderConsoleImGui();

    void buildColorPresets();
    void resetLegacyColorAdjustments();
    void setColorWithAdjust(float r, float g, float b, float a, const ColorAdjust& adjust) const;
    void applyLegacyPreset(int index);
    void randomizeLegacyColors();

    // Procedural visualization methods
    void renderProceduralVisualization();
    void renderReactiveCircle(float bass, float mid, float high);
    void renderFrequencyBars(float bass, float mid, float high);
    void renderRotatingRings(float mid, float time);
    void renderHighFrequencySparkles(float high);
    void renderWaveformVisualization();
    
    // Legacy OpenGL methods for software rendering
    void renderLegacyVisualization(bool overlay = false);
    void renderLegacyCircle(float bass, float mid, float high, float motionBlend, float animatedTime);
    void renderLegacyFrequencyBars(float bass, float mid, float high, float motionBlend, float animatedTime);
    void renderLegacyRings(float mid, float animatedTime, float motionBlend);
    void renderLegacySparkles(float bass, float high, float animatedTime, float motionBlend);
    void renderLegacyWaveform(float animatedTime, float motionBlend);

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
