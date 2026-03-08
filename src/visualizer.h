#ifndef VISUALIZER_H
#define VISUALIZER_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <memory>
#include "audio_analyzer.h"
#include "shader.h"

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
    void render();

private:
    GLFWwindow* window_;
    int windowWidth_;
    int windowHeight_;
    float time_;

    // OpenGL objects
    GLuint quadVAO_;
    GLuint quadVBO_;
    
    std::unique_ptr<Shader> shader_;
    AudioAnalyzer::AudioFeatures audioFeatures_;

    bool setupOpenGL();
    bool setupGeometry();
    bool loadShaders();
    void setupQuad();
};

#endif // VISUALIZER_H
