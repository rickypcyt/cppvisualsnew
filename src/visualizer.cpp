#include "visualizer.h"
#include <iostream>
#include <chrono>

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main() {
    TexCoord = aTexCoord;
    gl_Position = vec4(aPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform float uTime;
uniform float uBass;
uniform float uMid;
uniform float uHigh;
uniform float uEnergy;
uniform float uOnset;
uniform float uBeat;
uniform vec2 uResolution;

// Raymarching SDF functions
float sdSphere(vec3 p, float r) {
    return length(p) - r;
}

float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdTorus(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}

float sceneSDF(vec3 p) {
    float bassPulse = 1.0 + uBass * 3.0;
    float midWobble = sin(uTime * 2.0 + uMid * 10.0) * 0.2;
    float highNoise = uHigh * 0.1;
    
    // Reactive geometry
    vec3 p1 = p;
    p1.y += sin(uTime + p1.x * 2.0) * uMid * 0.5;
    
    float sphere = sdSphere(p1, 0.5 + bassPulse * 0.3);
    float box = sdBox(p + vec3(0.0, sin(uTime) * uMid, 0.0), vec3(0.3 + uBass));
    float torus = sdTorus(p.xzy, vec2(0.8 + uBass * 0.5, 0.1 + uHigh * 0.2));
    
    // Combine shapes
    float result = min(sphere, box);
    result = min(result, torus);
    
    // Add some fractal-like detail when there's high energy
    if (uEnergy > 0.3) {
        float detail = sdSphere(p * 3.0 + vec3(sin(uTime * 5.0), cos(uTime * 3.0), sin(uTime * 7.0)), 0.1);
        result = mix(result, detail, uHigh * 0.3);
    }
    
    return result;
}

vec3 getNormal(vec3 p) {
    vec2 e = vec2(0.001, 0.0);
    return normalize(vec3(
        sceneSDF(p + e.xyy) - sceneSDF(p - e.xyy),
        sceneSDF(p + e.yxy) - sceneSDF(p - e.yxy),
        sceneSDF(p + e.yyx) - sceneSDF(p - e.yyx)
    ));
}

vec3 raymarch(vec3 ro, vec3 rd) {
    float t = 0.0;
    int maxSteps = 100;
    float maxDist = 10.0;
    
    for (int i = 0; i < maxSteps; i++) {
        vec3 p = ro + rd * t;
        float d = sceneSDF(p);
        
        if (d < 0.001) {
            // Hit!
            vec3 normal = getNormal(p);
            
            // Psychedelic lighting based on audio
            vec3 lightDir = normalize(vec3(
                sin(uTime * 0.5) * uMid,
                cos(uTime * 0.3) * uBass,
                sin(uTime * 0.7) * uHigh
            ));
            
            float diff = max(dot(normal, lightDir), 0.0);
            
            // Color based on frequency bands
            vec3 baseColor = vec3(uBass, uMid, uHigh);
            vec3 color = baseColor * diff;
            
            // Add glow on beats
            if (uBeat > 0.5) {
                color *= 2.0;
            }
            
            // Add onset flash
            if (uOnset > 0.5) {
                color = mix(color, vec3(1.0), 0.5);
            }
            
            return color;
        }
        
        t += d;
        if (t > maxDist) break;
    }
    
    // Background - dark with subtle audio-reactive gradient
    return vec3(0.02 + uEnergy * 0.05, 0.01 + uMid * 0.03, 0.03 + uHigh * 0.04);
}

void main() {
    vec2 uv = (TexCoord - 0.5) * 2.0;
    uv.x *= uResolution.x / uResolution.y;
    
    vec3 ro = vec3(0.0, 0.0, 3.0);
    vec3 rd = normalize(vec3(uv, -1.0));
    
    // Camera movement based on audio
    ro.x += sin(uTime * 0.5) * uMid * 0.5;
    ro.y += cos(uTime * 0.3) * uBass * 0.3;
    
    vec3 color = raymarch(ro, rd);
    
    // Post-processing
    color = pow(color, vec3(0.8)); // Gamma correction
    color = smoothstep(0.0, 1.0, color); // Contrast
    
    // Add scanlines for retro rave feel
    float scanline = sin(TexCoord.y * uResolution.y * 2.0) * 0.02;
    color -= scanline;
    
    FragColor = vec4(color, 1.0);
}
)";

Visualizer::Visualizer() 
    : window_(nullptr), windowWidth_(800), windowHeight_(600), time_(0.0f),
      quadVAO_(0), quadVBO_(0) {}

Visualizer::~Visualizer() {
    shutdown();
}

bool Visualizer::initialize(int width, int height) {
    windowWidth_ = width;
    windowHeight_ = height;

    if (!setupOpenGL()) {
        return false;
    }

    if (!setupGeometry()) {
        return false;
    }

    if (!loadShaders()) {
        return false;
    }

    return true;
}

void Visualizer::shutdown() {
    if (quadVAO_) {
        glDeleteVertexArrays(1, &quadVAO_);
        quadVAO_ = 0;
    }
    if (quadVBO_) {
        glDeleteBuffers(1, &quadVBO_);
        quadVBO_ = 0;
    }
    
    shader_.reset();
    
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

bool Visualizer::shouldClose() {
    return window_ ? glfwWindowShouldClose(window_) : true;
}

void Visualizer::beginFrame() {
    glfwPollEvents();
    
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Visualizer::endFrame() {
    glfwSwapBuffers(window_);
    
    // Update time
    static auto lastTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
    lastTime = currentTime;
    time_ += deltaTime;
}

void Visualizer::updateAudioData(const AudioAnalyzer::AudioFeatures& features) {
    audioFeatures_ = features;
}

void Visualizer::render() {
    shader_->use();
    
    // Set uniforms
    shader_->setUniform1f("uTime", time_);
    shader_->setUniform1f("uBass", audioFeatures_.bassEnergy);
    shader_->setUniform1f("uMid", audioFeatures_.midEnergy);
    shader_->setUniform1f("uHigh", audioFeatures_.highEnergy);
    shader_->setUniform1f("uEnergy", audioFeatures_.energy);
    shader_->setUniform1f("uOnset", audioFeatures_.onset);
    shader_->setUniform1f("uBeat", audioFeatures_.beat);
    shader_->setUniform2f("uResolution", windowWidth_, windowHeight_);
    
    // Render quad
    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

bool Visualizer::setupOpenGL() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    // Try with OpenGL ES if desktop OpenGL fails
    bool useGLES = false;

    window_ = glfwCreateWindow(windowWidth_, windowHeight_, "Audio Visualizer", nullptr, nullptr);
    if (!window_) {
        std::cerr << "Failed to create GLFW window with OpenGL 3.3" << std::endl;
        
        // Try OpenGL ES
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        
        window_ = glfwCreateWindow(windowWidth_, windowHeight_, "Audio Visualizer", nullptr, nullptr);
        if (!window_) {
            std::cerr << "Failed to create GLFW window with OpenGL ES" << std::endl;
            glfwTerminate();
            return false;
        }
        useGLES = true;
    }

    glfwMakeContextCurrent(window_);

    // Initialize GLEW with experimental features
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(glewInit()) << std::endl;
        
        // Try without GLEW for basic functionality
        std::cout << "Attempting to continue without GLEW..." << std::endl;
    }

    // Clear any potential GL errors from GLEW initialization
    glGetError();

    glViewport(0, 0, windowWidth_, windowHeight_);
    
    std::cout << "OpenGL setup successful!" << std::endl;
    return true;
}

bool Visualizer::setupGeometry() {
    setupQuad();
    return true;
}

bool Visualizer::loadShaders() {
    shader_ = std::make_unique<Shader>();
    return shader_->loadFromSource(vertexShaderSource, fragmentShaderSource);
}

void Visualizer::setupQuad() {
    float vertices[] = {
        // positions    // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f
    };

    glGenVertexArrays(1, &quadVAO_);
    glGenBuffers(1, &quadVBO_);
    
    glBindVertexArray(quadVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    // position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // tex coord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
}
