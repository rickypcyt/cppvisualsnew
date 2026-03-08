#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include "audio_capture.h"
#include "audio_analyzer.h"
#include "visualizer.h"

class AudioVisualizerApp {
public:
    AudioVisualizerApp() = default;
    ~AudioVisualizerApp() = default;

    bool initialize() {
        // Initialize audio capture
        if (!audioCapture_.initialize()) {
            std::cerr << "Failed to initialize audio capture" << std::endl;
            return false;
        }

        // Initialize visualizer
        if (!visualizer_.initialize(1280, 720)) {
            std::cerr << "Failed to initialize visualizer" << std::endl;
            return false;
        }

        // Start audio capture
        if (!audioCapture_.start()) {
            std::cerr << "Failed to start audio capture" << std::endl;
            return false;
        }

        std::cout << "Audio Visualizer initialized successfully!" << std::endl;
        std::cout << "Listening to microphone input..." << std::endl;
        std::cout << "Press ESC or close window to exit" << std::endl;

        return true;
    }

    void run() {
        while (!visualizer_.shouldClose()) {
            // Process audio if new data is available
            if (audioCapture_.hasNewData()) {
                auto audioBuffer = audioCapture_.getAudioBuffer();
                audioAnalyzer_.processAudio(audioBuffer);
                audioCapture_.clearNewDataFlag();
            }

            // Update visualizer with audio data
            visualizer_.updateAudioData(audioAnalyzer_.getFeatures());

            // Render frame
            visualizer_.beginFrame();
            visualizer_.render();
            visualizer_.endFrame();

            // Small delay to prevent excessive CPU usage
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void shutdown() {
        audioCapture_.stop();
        audioCapture_.shutdown();
        visualizer_.shutdown();
    }

private:
    AudioCapture audioCapture_;
    AudioAnalyzer audioAnalyzer_;
    Visualizer visualizer_;
};

int main() {
    try {
        AudioVisualizerApp app;

        if (!app.initialize()) {
            std::cerr << "Failed to initialize application" << std::endl;
            return -1;
        }

        app.run();
        app.shutdown();

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
