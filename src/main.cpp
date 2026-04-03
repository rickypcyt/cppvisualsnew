#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <cstring>
#include "audio_capture.h"
#include "audio_analyzer.h"
#include "visualizer.h"

class AudioVisualizerApp {
public:
    AudioVisualizerApp() = default;
    ~AudioVisualizerApp() = default;

    bool initialize(int deviceIndex = -1) {
        // Initialize audio capture with specified device
        if (deviceIndex >= 0) {
            if (!audioCapture_.initialize(deviceIndex)) {
                std::cerr << "Failed to initialize audio capture with device " << deviceIndex << std::endl;
                return false;
            }
        } else {
            if (!audioCapture_.initialize()) {
                std::cerr << "Failed to initialize audio capture" << std::endl;
                return false;
            }
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

        audioAnalyzer_.setSampleRate(static_cast<float>(audioCapture_.getSampleRate()));
        visualizer_.setAudioAnalyzer(&audioAnalyzer_);

        std::cout << "Audio Visualizer initialized successfully!" << std::endl;
        std::cout << "Listening to microphone input..." << std::endl;
        std::cout << "Press ESC or close window to exit" << std::endl;

        return true;
    }

    void run() {
        while (!visualizer_.shouldClose()) {
            // Process window events - CRITICAL for Wayland/Hyprland
            glfwPollEvents();
            
            // Check for device change
            static int lastDevice = visualizer_.getSelectedDevice();
            if (visualizer_.getSelectedDevice() != lastDevice) {
                std::cout << "Changing audio device to: " << visualizer_.getSelectedDevice() << std::endl;
                
                // Restart audio with new device
                audioCapture_.stop();
                audioCapture_.shutdown();
                
                if (!audioCapture_.initialize(visualizer_.getSelectedDevice())) {
                    std::cerr << "Failed to initialize audio with new device" << std::endl;
                    visualizer_.setSelectedDevice(lastDevice);
                    audioCapture_.initialize(lastDevice);
                }
                
                if (!audioCapture_.start()) {
                    std::cerr << "Failed to start audio with new device" << std::endl;
                }

                audioAnalyzer_.setSampleRate(static_cast<float>(audioCapture_.getSampleRate()));
                
                lastDevice = visualizer_.getSelectedDevice();
            }
            
            // Process audio if new data is available
            if (audioCapture_.hasNewData()) {
                auto audioBuffer = audioCapture_.getAudioBuffer();
                float gain = visualizer_.getAudioInputGain();
                if (gain != 1.0f) {
                    for (float& sample : audioBuffer) {
                        sample *= gain;
                    }
                }
                audioAnalyzer_.processAudio(audioBuffer);
                visualizer_.updateAudioBuffer(audioBuffer); // Update waveform
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

int main(int argc, char* argv[]) {
    try {
        AudioVisualizerApp app;
        
        // Parse command line arguments
        int deviceIndex = -1;
        bool showDevices = false;
        
        for (int i = 1; i < argc; ++i) {
            if (strcmp(argv[i], "--list-devices") == 0 || strcmp(argv[i], "-l") == 0) {
                showDevices = true;
            } else if (strcmp(argv[i], "--device") == 0 || strcmp(argv[i], "-d") == 0) {
                if (i + 1 < argc) {
                    deviceIndex = std::atoi(argv[i + 1]);
                    i++; // Skip the next argument
                } else {
                    std::cerr << "Error: --device requires an index" << std::endl;
                    return -1;
                }
            } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
                std::cout << "Audio Visualizer - Real-time music visualization" << std::endl;
                std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
                std::cout << "Options:" << std::endl;
                std::cout << "  -l, --list-devices    List available audio devices" << std::endl;
                std::cout << "  -d, --device <index>   Use specific audio device" << std::endl;
                std::cout << "  -h, --help            Show this help message" << std::endl;
                return 0;
            }
        }
        
        // Show device list if requested
        if (showDevices) {
            AudioCapture::listAvailableDevices();
            return 0;
        }

        if (!app.initialize(deviceIndex)) {
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
