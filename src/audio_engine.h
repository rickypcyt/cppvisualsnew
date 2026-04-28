#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>

// Forward declarations
class AudioCapture;
class AudioAnalyzer;

// Audio features struct (copied from AudioAnalyzer for independence)
struct AudioFeatures {
    float energy = 0.0f;
    float bassEnergy = 0.0f;
    float midEnergy = 0.0f;
    float highEnergy = 0.0f;
    float bassShare = 0.0f;
    float midShare = 0.0f;
    float highShare = 0.0f;
    float onset = 0.0f;
    float beat = 0.0f;
    float kick = 0.0f;
    float clap = 0.0f;
    float hiHat = 0.0f;
    float bpm = 0.0f;
    
    // Enhanced frequency analysis
    static constexpr int NUM_MEL_BANDS = 12;
    std::array<float, NUM_MEL_BANDS> melBandEnergies{};
    std::array<float, NUM_MEL_BANDS> melBandShares{};
    
    // Spectral features
    float spectralFlux = 0.0f;
    float zeroCrossingRate = 0.0f;
    float spectralCentroid = 0.0f;
    float spectralRolloff = 0.0f;
    float percussionTonalRatio = 0.0f;
};

// Audio device information
struct AudioDeviceInfo {
    int index;
    std::string name;
    int inputChannels;
    int outputChannels;
    bool isDefault;
    bool isInternalLoopback;
};

// Unified Audio Engine
// Combines capture, analysis, and processing into a single class
// with threading for optimal performance
class AudioEngine {
public:
    using FeaturesCallback = std::function<void(const AudioFeatures&)>;
    using WaveformCallback = std::function<void(const std::vector<float>&)>;

    AudioEngine();
    ~AudioEngine();

    // ========== Lifecycle ==========
    bool initialize(int deviceIndex = -1);
    void shutdown();
    bool start();
    void stop();
    bool isRunning() const { return running_.load(); }

    // ========== Device Management ==========
    std::vector<AudioDeviceInfo> listDevices() const;
    bool changeDevice(int deviceIndex);
    int getCurrentDevice() const { return currentDevice_.load(); }
    std::string getCurrentDeviceName() const;

    // ========== Settings ==========
    void setGain(float gain);
    float getGain() const { return gain_.load(); }
    void setEnabled(bool enabled);
    bool isEnabled() const { return enabled_.load(); }

    // ========== Beat Detection Tuning ==========
    void setKickThresholdMultiplier(float value);
    void setKickMinInterval(float value);
    void setClapThresholdMultiplier(float value);
    void setClapMaxBassShare(float value);
    void setClapMinInterval(float value);
    void setHiHatThresholdMultiplier(float value);
    void setHiHatMinHighShare(float value);
    void setHiHatMinInterval(float value);
    void setOnsetThreshold(float value);

    // ========== Data Access ==========
    // Callback-based (recommended - async, non-blocking)
    void setFeaturesCallback(FeaturesCallback callback);
    void setWaveformCallback(WaveformCallback callback);

    // Direct polling (for compatibility)
    AudioFeatures getFeatures() const;
    std::vector<float> getWaveform() const;
    bool hasNewData();

    // ========== Stats ==========
    float getCurrentSampleRate() const { return currentSampleRate_.load(); }
    double getProcessingTimeMs() const { return lastProcessingTimeMs_.load(); }
    size_t getDroppedFrames() const { return droppedFrames_.load(); }

private:
    // Processing thread
    void processingLoop();
    void processAudioFrame(const std::vector<float>& audioBuffer);

    // Internal state
    std::unique_ptr<AudioCapture> capture_;
    std::unique_ptr<AudioAnalyzer> analyzer_;

    // Threading
    std::thread processingThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> shouldStop_{false};
    std::atomic<bool> enabled_{true};

    // Synchronization
    mutable std::mutex featuresMutex_;
    mutable std::mutex waveformMutex_;
    std::condition_variable processingCondition_;
    std::queue<std::vector<float>> audioQueue_;
    std::mutex queueMutex_;
    static constexpr size_t MAX_QUEUE_SIZE = 3;

    // Audio buffer management
    std::atomic<float> gain_{1.0f};
    std::atomic<int> currentDevice_{-1};
    std::atomic<float> currentSampleRate_{48000.0f};

    // Callbacks
    FeaturesCallback featuresCallback_;
    WaveformCallback waveformCallback_;

    // Cached data for polling interface
    AudioFeatures cachedFeatures_;
    std::vector<float> cachedWaveform_;
    std::atomic<bool> newDataAvailable_{false};

    // Performance stats
    std::atomic<double> lastProcessingTimeMs_{0.0};
    std::atomic<size_t> droppedFrames_{0};

    // Timestamp for timing
    using Clock = std::chrono::high_resolution_clock;
    Clock::time_point lastFrameTime_;
};

#endif // AUDIO_ENGINE_H
