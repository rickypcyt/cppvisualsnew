#include "visualizer.h"
#include <iostream>
#include <chrono>
#include "audio_capture.h"

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
      quadVAO_(0), quadVBO_(0), waveformVAO_(0), waveformVBO_(0),
      selectedDevice_(-1), showDeviceMenu_(false), showDiagnostic_(false), consoleMode_(false),
      showImGuiWindow_(true), showDeviceSelector_(false), showDiagnosticInfo_(false), showConsoleMode_(false) {
    waveformBuffer_.resize(512); // Same as audio buffer size
    setupDeviceList();
}

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
        std::cout << "Failed to load shaders, using fallback rendering" << std::endl;
        shader_.reset(); // Will trigger fallback triangle
    }

    // Don't setup ImGui for simple version
    showImGuiWindow_ = false;

    return true;
}

void Visualizer::shutdown() {
    // Don't shutdown ImGui for simple version
    
    if (quadVAO_) {
        glDeleteVertexArrays(1, &quadVAO_);
        quadVAO_ = 0;
    }
    if (quadVBO_) {
        glDeleteBuffers(1, &quadVBO_);
        quadVBO_ = 0;
    }
    if (waveformVAO_) {
        glDeleteVertexArrays(1, &waveformVAO_);
        waveformVAO_ = 0;
    }
    if (waveformVBO_) {
        glDeleteBuffers(1, &waveformVBO_);
        waveformVBO_ = 0;
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

void Visualizer::updateAudioBuffer(const std::vector<float>& audioBuffer) {
    if (audioBuffer.size() >= waveformBuffer_.size()) {
        std::copy(audioBuffer.begin(), audioBuffer.begin() + waveformBuffer_.size(), waveformBuffer_.begin());
    }
}

void Visualizer::render() {
    // Clear screen
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Render legacy visualization (works with software rendering)
    renderLegacyVisualization();
}

bool Visualizer::setupOpenGL() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);  // Use OpenGL 2.1 for compatibility
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);  // Don't force core profile
    
    // Try with OpenGL ES if desktop OpenGL fails
    bool useGLES = false;

    window_ = glfwCreateWindow(windowWidth_, windowHeight_, "Audio Visualizer", nullptr, nullptr);
    if (!window_) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        return false;
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
    setupWaveform();
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

void Visualizer::setupWaveform() {
    glGenVertexArrays(1, &waveformVAO_);
    glGenBuffers(1, &waveformVBO_);
    
    glBindVertexArray(waveformVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, waveformVBO_);
    
    // Allocate buffer memory (will be updated dynamically)
    glBufferData(GL_ARRAY_BUFFER, waveformBuffer_.size() * sizeof(float) * 2, nullptr, GL_DYNAMIC_DRAW);
    
    // position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
}

void Visualizer::renderWaveform(const std::vector<float>& audioBuffer) {
    if (audioBuffer.empty()) return;
    
    // Create vertices for waveform
    std::vector<float> vertices;
    vertices.reserve(audioBuffer.size() * 2);
    
    float waveHeight = 100.0f; // Height of waveform display
    float waveY = windowHeight_ - waveHeight - 20.0f; // Position at bottom
    
    for (size_t i = 0; i < audioBuffer.size(); ++i) {
        float x = (float)i / (audioBuffer.size() - 1) * windowWidth_;
        float y = waveY + audioBuffer[i] * waveHeight * 0.5f;
        vertices.push_back(x);
        vertices.push_back(y);
    }
    
    // Update VBO with new waveform data
    glBindBuffer(GL_ARRAY_BUFFER, waveformVBO_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());
    
    // Simple shader for waveform (colored line)
    glUseProgram(0); // Use fixed function pipeline for simplicity
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, windowWidth_, windowHeight_, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Disable depth test for overlay
    glDisable(GL_DEPTH_TEST);
    
    // Draw waveform as line strip
    glColor3f(0.0f, 1.0f, 0.5f); // Cyan color
    glLineWidth(2.0f);
    
    glBindVertexArray(waveformVAO_);
    glDrawArrays(GL_LINE_STRIP, 0, audioBuffer.size());
    glBindVertexArray(0);
    
    // Restore state
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void Visualizer::setupDeviceList() {
    // Get device names
    PaError err = Pa_Initialize();
    if (err != paNoError) return;
    
    int numDevices = Pa_GetDeviceCount();
    deviceNames_.clear();
    
    for (int i = 0; i < numDevices; ++i) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceInfo && deviceInfo->maxInputChannels > 0) {
            deviceNames_.push_back(deviceInfo->name);
        } else {
            deviceNames_.push_back(""); // No input channels
        }
    }
    
    Pa_Terminate();
}

void Visualizer::renderGUI() {
    // Check for 'D' key toggle
    if (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS) {
        static double lastPress = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastPress > 0.5) { // 500ms debounce
            showDeviceMenu_ = !showDeviceMenu_;
            lastPress = currentTime;
        }
    }
    
    // Check for 'I' key toggle for diagnostic mode
    if (glfwGetKey(window_, GLFW_KEY_I) == GLFW_PRESS) {
        static double lastPress = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastPress > 0.5) { // 500ms debounce
            showDiagnostic_ = !showDiagnostic_;
            lastPress = currentTime;
        }
    }
    
    // Check for 'C' key toggle for console mode
    if (glfwGetKey(window_, GLFW_KEY_C) == GLFW_PRESS) {
        static double lastPress = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastPress > 0.5) { // 500ms debounce
            consoleMode_ = !consoleMode_;
            lastPress = currentTime;
        }
    }
    
    if (showDeviceMenu_) {
        showDeviceSelector();
    }
    
    // Show current device info
    std::string deviceInfo = "Device: ";
    if (selectedDevice_ >= 0 && selectedDevice_ < deviceNames_.size()) {
        deviceInfo += deviceNames_[selectedDevice_];
    } else {
        deviceInfo += "Default";
    }
    deviceInfo += " (D:devices I:diagnostics C:console)";
    renderText(deviceInfo, 10, 30);
    
    // Show diagnostic info if enabled
    if (showDiagnostic_) {
        renderDiagnosticInfo();
    }
    
    // Show console visualization if enabled
    if (consoleMode_) {
        renderConsoleVisualization();
    }
}

bool Visualizer::showDeviceSelector() {
    // Simple device selector using text rendering
    float menuX = 50.0f;
    float menuY = 100.0f;
    float lineHeight = 25.0f;
    
    renderText("=== Select Audio Device ===", menuX, menuY);
    renderText("Use number keys 1-9 to select", menuX, menuY + lineHeight);
    renderText("Press ESC to cancel", menuX, menuY + lineHeight * 2);
    renderText("", menuX, menuY + lineHeight * 3);
    
    // Show available devices
    int displayCount = 0;
    for (int i = 0; i < deviceNames_.size() && displayCount < 9; ++i) {
        if (!deviceNames_[i].empty()) {
            std::string deviceText = std::to_string(displayCount + 1) + ". " + deviceNames_[i];
            if (i == selectedDevice_) {
                deviceText += " [CURRENT]";
            }
            renderText(deviceText, menuX, menuY + lineHeight * (4 + displayCount));
            displayCount++;
        }
    }
    
    // Handle number key presses
    for (int i = 0; i < 9; ++i) {
        if (glfwGetKey(window_, GLFW_KEY_1 + i) == GLFW_PRESS) {
            // Find the actual device index
            int actualIndex = -1;
            int count = 0;
            for (int j = 0; j < deviceNames_.size(); ++j) {
                if (!deviceNames_[j].empty()) {
                    if (count == i) {
                        actualIndex = j;
                        break;
                    }
                    count++;
                }
            }
            
            if (actualIndex >= 0) {
                selectedDevice_ = actualIndex;
                showDeviceMenu_ = false;
                return true; // Device changed
            }
        }
    }
    
    // Handle ESC
    if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        showDeviceMenu_ = false;
    }
    
    return false;
}

void Visualizer::renderText(const std::string& text, float x, float y) {
    // Simple text rendering using bitmap characters (basic implementation)
    // For now, we'll use a very simple approach with line segments
    
    glUseProgram(0); // Use fixed function pipeline
    
    // Setup 2D projection
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, windowWidth_, windowHeight_, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Disable depth test for overlay
    glDisable(GL_DEPTH_TEST);
    
    // Set text color
    glColor3f(1.0f, 1.0f, 1.0f);
    
    // Very basic text rendering (just show the text as a placeholder)
    // In a real implementation, you'd use a proper font rendering system
    glRasterPos2f(x, y);
    
    // For now, just print to console as a fallback
    // This is a placeholder - proper text rendering would require a font library
    static std::string lastText;
    if (text != lastText) {
        std::cout << "GUI: " << text << std::endl;
        lastText = text;
    }
    
    // Restore state
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}
