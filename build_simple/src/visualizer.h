#ifndef VISUALIZER_H
#define VISUALIZER_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <memory>
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
    
    // No-ImGui methods (public)
    void renderNoImGuiLoop();
    
    // ImGui methods
    void setupImGui();
    void shutdownImGui();
    void renderImGui();

private:
    GLFWwindow* window_;
    int windowWidth_;
    int windowHeight_;
    float time_;

    // OpenGL objects
    GLuint quadVAO_;
    GLuint quadVBO_;
    GLuint waveformVAO_;
    GLuint waveformVBO_;
    
    std::unique_ptr<Shader> shader_;
    AudioAnalyzer::AudioFeatures audioFeatures_;
    std::vector<float> waveformBuffer_;
    int selectedDevice_;
    bool showDeviceMenu_;
    bool showDiagnostic_;
    bool consoleMode_;
    std::vector<std::string> deviceNames_;
    
    // ImGui state
    bool showImGuiWindow_;
    bool showDeviceSelector_;
    bool showDiagnosticInfo_;
    bool showConsoleMode_;

    bool setupOpenGL();
    bool setupGeometry();
    bool loadShaders();
    void setupQuad();
    void setupWaveform();
    void renderWaveform(const std::vector<float>& audioBuffer);
    void renderText(const std::string& text, float x, float y);
    void setupDeviceList();
    void renderDiagnosticInfo();
    void renderConsoleVisualization();
    void renderFallbackTriangle();
    
    // ImGui rendering methods
    void renderMainImGuiWindow();
    void renderDeviceSelectorImGui();
    void renderDiagnosticImGui();
    void renderConsoleImGui();
    
    // Procedural visualization methods
    void renderProceduralVisualization();
    void renderReactiveCircle(float bass, float mid, float high);
    void renderFrequencyBars(float bass, float mid, float high);
    void renderBeatExplosion(float bass, float mid, float high);
    void renderRotatingRings(float mid, float time);
    void renderHighFrequencySparkles(float high);
    void renderWaveformVisualization();
    
    // Legacy OpenGL methods for software rendering
    void renderLegacyVisualization();
    void renderLegacyCircle(float bass, float mid, float high);
    void renderLegacyFrequencyBars(float bass, float mid, float high);
    void renderLegacyBeatExplosion(float bass, float mid, float high);
    void renderLegacyRings(float mid, float time);
    void renderLegacySparkles(float high);
    void renderLegacyWaveform();
};

#endif // VISUALIZER_H
