#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <cstring>
#include <csignal>
#include <atomic>
#include "audio_engine.h"
#include "app/visualizer.h"
#include "renderer_interface.h"

// Global flag for signal handling
static std::atomic<bool> g_interrupted(false);

void signalHandler(int signal) {
    g_interrupted.store(true);
}

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

        int desiredDevice = deviceIndex;
        if (desiredDevice < 0) {
            desiredDevice = visualizer_.getSelectedDevice();
        }

        // Sync visualizer audio settings with AudioEngine
        audioEngine_.setEnabled(visualizer_.getAudioEngineEnabled());
        audioEngine_.setGain(visualizer_.getAudioInputGain());

        // Set up callbacks for audio data
        audioEngine_.setFeaturesCallback([this](const AudioFeatures& features) {
            visualizer_.updateAudioData(reinterpret_cast<const AudioAnalyzer::AudioFeatures&>(features));
        });
        
        audioEngine_.setWaveformCallback([this](const std::vector<float>& waveform) {
            visualizer_.updateAudioBuffer(waveform);
        });

        // Initialize audio engine
        if (visualizer_.getAudioEngineEnabled()) {
            if (!audioEngine_.initialize(desiredDevice)) {
                std::cerr << "Failed to initialize audio engine" << std::endl;
                return false;
            }
            if (!audioEngine_.start()) {
                std::cerr << "Failed to start audio engine" << std::endl;
                return false;
            }
        } else {
            std::cout << "[DEBUG] Audio engine disabled by default (use the Audio panel to enable it)" << std::endl;
        }

        std::cout << "Audio Visualizer initialized successfully!" << std::endl;
        std::cout << "Listening to microphone input..." << std::endl;
        std::cout << "Press ESC or close window to exit" << std::endl;

        return true;
    }

    void run() {
        static bool lastAudioEngineEnabled = false;
        static int lastDevice = -2;

        while (!visualizer_.shouldClose() && !g_interrupted.load()) {
            bool currentAudioEngineEnabled = visualizer_.getAudioEngineEnabled();
            int currentDevice = visualizer_.getSelectedDevice();
            
            if (lastDevice == -2) {
                lastDevice = currentDevice;
                lastAudioEngineEnabled = currentAudioEngineEnabled;
            }

            // Handle audio engine enable/disable
            if (currentAudioEngineEnabled != lastAudioEngineEnabled) {
                audioEngine_.setEnabled(currentAudioEngineEnabled);
                lastAudioEngineEnabled = currentAudioEngineEnabled;
            }

            // Handle device change
            if (currentAudioEngineEnabled && currentDevice != lastDevice) {
                std::cout << "Changing audio device to: " << currentDevice << std::endl;
                if (!audioEngine_.changeDevice(currentDevice)) {
                    std::cerr << "Failed to change audio device" << std::endl;
                    visualizer_.setSelectedDevice(lastDevice);
                } else {
                    lastDevice = currentDevice;
                }
            }

            // Update gain in real-time
            audioEngine_.setGain(visualizer_.getAudioInputGain());

            // Render frame
            visualizer_.beginFrame();
            visualizer_.render();
            visualizer_.endFrame();
        }
    }

    void shutdown() {
        audioEngine_.shutdown();
        visualizer_.shutdown();
    }

private:
    AudioEngine audioEngine_;
    Visualizer visualizer_;
};

int main(int argc, char* argv[]) {
    // Setup signal handlers for Ctrl+C and termination
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        AudioVisualizerApp app;

        // Parse command line arguments
        int deviceIndex = -1;
        bool showDevices = false;
        std::string backendStr = "";

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
            } else if (strcmp(argv[i], "--backend") == 0 || strcmp(argv[i], "-b") == 0) {
                if (i + 1 < argc) {
                    backendStr = argv[++i];
                } else {
                    std::cerr << "Error: --backend requires a value (opengl or vulkan)" << std::endl;
                    return -1;
                }
            } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
                std::cout << "Audio Visualizer - Real-time music visualization" << std::endl;
                std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
                std::cout << "Options:" << std::endl;
                std::cout << "  -l, --list-devices    List available audio devices" << std::endl;
                std::cout << "  -d, --device <index>   Use specific audio device" << std::endl;
                std::cout << "  -b, --backend <name>   Choose rendering backend (opengl or vulkan)" << std::endl;
                std::cout << "  -h, --help            Show this help message" << std::endl;
                std::cout << std::endl;
                std::cout << "Available backends:" << std::endl;
                std::cout << "  opengl (gl, ogl)  - OpenGL renderer (default)" << std::endl;
                if (RendererFactory::isBackendAvailable(RendererFactory::Backend::VULKAN)) {
                    std::cout << "  vulkan (vk)       - Vulkan renderer (experimental)" << std::endl;
                }
                return 0;
            }
        }

        // Determine rendering backend
        RendererFactory::Backend backend;
        if (backendStr.empty()) {
            backend = RendererFactory::getDefaultBackend();
        } else {
            backend = RendererFactory::parseBackend(backendStr);
            if (!RendererFactory::isBackendAvailable(backend)) {
                std::cerr << "Error: Backend '" << backendStr << "' is not available" << std::endl;
                std::cerr << "Falling back to OpenGL" << std::endl;
                backend = RendererFactory::Backend::OPENGL;
            }
        }

        std::cout << "Using rendering backend: " << RendererFactory::getBackendName(backend) << std::endl;

        // Show device list if requested
        if (showDevices) {
            AudioEngine engine;
            auto devices = engine.listDevices();
            std::cout << "\n=== Available Audio Devices ===" << std::endl;
            std::cout << "Index | Name                    | Inputs | Outputs | Default" << std::endl;
            std::cout << "------|-------------------------|--------|---------|--------" << std::endl;
            for (const auto& dev : devices) {
                std::string marker = dev.isDefault ? " [DEFAULT]" : "";
                printf("%-6d| %-23s | %-6d | %-7d |%s\n", 
                       dev.index, dev.name.c_str(), dev.inputChannels, dev.outputChannels, marker.c_str());
            }
            std::cout << "\nUse the device index to select input device." << std::endl;
            std::cout << "Example: ./audio_visualizer --device 2" << std::endl;
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
