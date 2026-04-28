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
        // Initialize visualizer first so persisted settings (like input device) are loaded
        if (!visualizer_.initialize(1280, 720)) {
            std::cerr << "Failed to initialize visualizer" << std::endl;
            return false;
        }

        visualizer_.setAudioAnalyzer(&audioAnalyzer_);

        int desiredDevice = deviceIndex;
        if (desiredDevice < 0) {
            desiredDevice = visualizer_.getSelectedDevice();
        }

        if (visualizer_.getAudioEngineEnabled()) {
            if (!startAudioEngine(desiredDevice)) {
                std::cerr << "Failed to initialize audio capture" << std::endl;
                return false;
            }
        } else {
            audioEngineRunning_ = false;
            std::cout << "[DEBUG] Audio engine disabled by default (use the Audio panel to enable it)" << std::endl;
        }

        std::cout << "Audio Visualizer initialized successfully!" << std::endl;
        std::cout << "Listening to microphone input..." << std::endl;
        std::cout << "Press ESC or close window to exit" << std::endl;

        return true;
    }

    void run() {
        static bool lastAudioEngineEnabled = false;
        static int lastDevice = -2; // sentinel to force first-run sync

        while (!visualizer_.shouldClose()) {
            bool currentAudioEngineEnabled = visualizer_.getAudioEngineEnabled();

            // Check for device change
            int currentDevice = visualizer_.getSelectedDevice();
            if (lastDevice == -2) {
                lastDevice = currentDevice;
            }
            if (currentAudioEngineEnabled != lastAudioEngineEnabled) {
                if (currentAudioEngineEnabled) {
                    if (!startAudioEngine(currentDevice)) {
                        std::cerr << "Failed to enable audio engine" << std::endl;
                        visualizer_.setAudioEngineEnabled(false);
                        currentAudioEngineEnabled = false;
                    }
                } else {
                    stopAudioEngine();
                }
                lastAudioEngineEnabled = currentAudioEngineEnabled;
                lastDevice = currentDevice;
            }

            if (currentAudioEngineEnabled && audioEngineRunning_ && currentDevice != lastDevice) {
                std::cout << "Changing audio device to: " << visualizer_.getSelectedDevice() << std::endl;
                
                // Restart audio with new device
                stopAudioEngine();
                
                if (!startAudioEngine(visualizer_.getSelectedDevice())) {
                    std::cerr << "Failed to initialize audio with new device" << std::endl;
                    visualizer_.setSelectedDevice(lastDevice);
                    if (!startAudioEngine(lastDevice)) {
                        std::cerr << "Failed to restore previous audio device" << std::endl;
                    }
                }
                
                lastDevice = visualizer_.getSelectedDevice();
                currentAudioEngineEnabled = visualizer_.getAudioEngineEnabled();
            }
            
            // Process audio if new data is available
            if (currentAudioEngineEnabled && audioEngineRunning_ && audioCapture_.hasNewData()) {
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
            } else {
                visualizer_.updateAudioBuffer(std::vector<float>{});
            }

            // Update visualizer with audio data
            if (currentAudioEngineEnabled && audioEngineRunning_) {
                visualizer_.updateAudioData(audioAnalyzer_.getFeatures());
            } else {
                visualizer_.updateAudioData(AudioAnalyzer::AudioFeatures{});
            }

            // Render frame
            visualizer_.beginFrame();
            visualizer_.render();
            visualizer_.endFrame();

            // Frame rate limiter: cap at ~60 FPS to prevent excessive CPU/GPU usage
            // This is especially important with vsync disabled (glfwSwapInterval(0))
            static auto lastFrameTime = std::chrono::high_resolution_clock::now();
            auto currentFrameTime = std::chrono::high_resolution_clock::now();
            auto frameDuration = std::chrono::duration<float, std::milli>(currentFrameTime - lastFrameTime).count();
            
            constexpr float targetFrameTime = 1000.0f / 60.0f; // ~16.67ms for 60 FPS
            if (frameDuration < targetFrameTime) {
                std::this_thread::sleep_for(std::chrono::milliseconds(
                    static_cast<int>(targetFrameTime - frameDuration)));
            }
            lastFrameTime = std::chrono::high_resolution_clock::now();
        }
    }

    void shutdown() {
        stopAudioEngine();
        visualizer_.shutdown();
    }

private:
    bool startAudioEngine(int deviceIndex) {
        if (!visualizer_.getAudioEngineEnabled()) {
            audioEngineRunning_ = false;
            return false;
        }

        audioCapture_.stop();
        audioCapture_.shutdown();

        auto initWithDevice = [&](int devIndex) -> bool {
            if (devIndex < 0) {
                return audioCapture_.initialize();
            }
            if (audioCapture_.initialize(devIndex)) {
                visualizer_.setSelectedDevice(devIndex);
                return true;
            }
            return false;
        };

        bool captureInitialized = initWithDevice(deviceIndex);
        if (!captureInitialized && deviceIndex >= 0) {
            std::cerr << "Failed to initialize audio capture with device " << deviceIndex
                      << ", falling back to system default" << std::endl;
            captureInitialized = initWithDevice(-1);
        }

        if (!captureInitialized) {
            audioEngineRunning_ = false;
            return false;
        }

        if (!audioCapture_.start()) {
            std::cerr << "Failed to start audio capture" << std::endl;
            audioCapture_.shutdown();
            audioEngineRunning_ = false;
            return false;
        }

        audioAnalyzer_.setSampleRate(static_cast<float>(audioCapture_.getSampleRate()));
        audioEngineRunning_ = true;
        return true;
    }

    void stopAudioEngine() {
        audioCapture_.stop();
        audioCapture_.shutdown();
        audioEngineRunning_ = false;
    }

    AudioCapture audioCapture_;
    AudioAnalyzer audioAnalyzer_;
    Visualizer visualizer_;
    bool audioEngineRunning_ = false;
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
