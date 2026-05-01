#include "audio_engine.h"
#include "audio_capture.h"
#include "audio_analyzer.h"
#include <iostream>
#include <algorithm>

// Conversion from internal AudioAnalyzer::AudioFeatures to our AudioFeatures
AudioFeatures convertFeatures(const AudioAnalyzer::AudioFeatures& internal) {
    AudioFeatures f;
    f.energy = internal.energy;
    f.bassEnergy = internal.bassEnergy;
    f.midEnergy = internal.midEnergy;
    f.highEnergy = internal.highEnergy;
    f.bassShare = internal.bassShare;
    f.midShare = internal.midShare;
    f.highShare = internal.highShare;
    f.onset = internal.onset;
    f.beat = internal.beat;
    f.kick = internal.kick;
    f.clap = internal.clap;
    f.hiHat = internal.hiHat;
    f.bpm = internal.bpm;
    f.spectralFlux = internal.spectralFlux;
    f.zeroCrossingRate = internal.zeroCrossingRate;
    f.spectralCentroid = internal.spectralCentroid;
    f.spectralRolloff = internal.spectralRolloff;
    f.percussionTonalRatio = internal.percussionTonalRatio;
    
    for (int i = 0; i < AudioFeatures::NUM_MEL_BANDS; ++i) {
        f.melBandEnergies[i] = internal.melBandEnergies[i];
        f.melBandShares[i] = internal.melBandShares[i];
    }
    
    return f;
}

AudioEngine::AudioEngine()
    : capture_(std::make_unique<AudioCapture>())
    , analyzer_(std::make_unique<AudioAnalyzer>())
    , lastFrameTime_(Clock::now()) {
}

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::initialize(int deviceIndex) {
    if (!enabled_.load()) {
        std::cout << "[AudioEngine] Disabled, skipping initialization" << std::endl;
        return true;
    }

    // Initialize capture
    bool success = false;
    if (deviceIndex < 0) {
        success = capture_->initialize();
    } else {
        success = capture_->initialize(deviceIndex);
    }

    // If failed with specified device, try with default device
    if (!success && deviceIndex >= 0) {
        std::cerr << "[AudioEngine] Failed to initialize with device " << deviceIndex << std::endl;
        std::cerr << "[AudioEngine] Attempting to use default input device..." << std::endl;
        success = capture_->initialize(); // Use default device
    }

    if (!success) {
        std::cerr << "[AudioEngine] Failed to initialize audio capture" << std::endl;
        return false;
    }

    currentDevice_.store(deviceIndex >= 0 ? deviceIndex : capture_->getDefaultInputDevice());
    currentSampleRate_.store(static_cast<float>(capture_->getSampleRate()));
    analyzer_->setSampleRate(currentSampleRate_.load());

    return true;
}

void AudioEngine::shutdown() {
    stop();
    
    if (capture_) {
        capture_->shutdown();
    }
    
    running_.store(false);
}

bool AudioEngine::start() {
    if (!enabled_.load()) {
        std::cout << "[AudioEngine] Disabled, not starting" << std::endl;
        return true;
    }

    if (running_.load()) {
        return true;
    }

    if (!capture_->start()) {
        std::cerr << "[AudioEngine] Failed to start capture" << std::endl;
        return false;
    }

    shouldStop_.store(false);
    running_.store(true);
    
    // Start processing thread
    processingThread_ = std::thread(&AudioEngine::processingLoop, this);
    
    return true;
}

void AudioEngine::stop() {
    if (!running_.load()) {
        return;
    }

    shouldStop_.store(true);
    processingCondition_.notify_all();
    
    if (processingThread_.joinable()) {
        processingThread_.join();
    }

    capture_->stop();
    running_.store(false);
}

std::vector<AudioDeviceInfo> AudioEngine::listDevices() const {
    std::vector<AudioDeviceInfo> devices;
    
    // PortAudio requires initialization to list devices
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "[AudioEngine] PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        return devices;
    }

    int numDevices = Pa_GetDeviceCount();
    int defaultInput = Pa_GetDefaultInputDevice();

    for (int i = 0; i < numDevices; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (!info || info->maxInputChannels <= 0) continue;

        AudioDeviceInfo dev;
        dev.index = i;
        dev.name = info->name ? info->name : "Unknown";
        dev.inputChannels = info->maxInputChannels;
        dev.outputChannels = info->maxOutputChannels;
        dev.isDefault = (i == defaultInput);

        // Detect internal loopback
        dev.isInternalLoopback = false;
        if (const PaHostApiInfo* hostInfo = Pa_GetHostApiInfo(info->hostApi)) {
            std::string hostName(hostInfo->name ? hostInfo->name : "");
            std::string deviceName(info->name ? info->name : "");
            
            if (hostName.find("WASAPI") != std::string::npos) {
                if (deviceName.find("(loopback)") != std::string::npos) {
                    dev.isInternalLoopback = true;
                }
            } else if (hostName.find("PulseAudio") != std::string::npos || 
                       deviceName.find("pulse") != std::string::npos) {
                // PulseAudio is the preferred way for app visibility
            }
        }

        devices.push_back(dev);
    }

    Pa_Terminate();
    return devices;
}

bool AudioEngine::changeDevice(int deviceIndex) {
    if (!running_.load()) {
        // If not running, just reinitialize
        capture_->shutdown();
        return initialize(deviceIndex);
    }

    // Hot-swap while running
    stop();
    capture_->shutdown();
    
    bool success = initialize(deviceIndex);
    if (success) {
        success = start();
    }
    
    return success;
}

std::string AudioEngine::getCurrentDeviceName() const {
    auto devices = listDevices();
    int current = currentDevice_.load();
    for (const auto& dev : devices) {
        if (dev.index == current) {
            return dev.name;
        }
    }
    return "Default";
}

void AudioEngine::setGain(float gain) {
    gain_.store(std::clamp(gain, 0.0f, 10.0f));
}

void AudioEngine::setEnabled(bool enabled) {
    bool wasEnabled = enabled_.exchange(enabled);
    if (wasEnabled != enabled) {
        if (enabled && !running_.load()) {
            initialize(currentDevice_.load());
            start();
        } else if (!enabled && running_.load()) {
            stop();
        }
    }
}

void AudioEngine::setKickThresholdMultiplier(float value) {
    analyzer_->setKickThresholdMultiplier(value);
}

void AudioEngine::setKickMinInterval(float value) {
    analyzer_->setKickMinInterval(value);
}

void AudioEngine::setClapThresholdMultiplier(float value) {
    analyzer_->setClapThresholdMultiplier(value);
}

void AudioEngine::setClapMaxBassShare(float value) {
    analyzer_->setClapMaxBassShare(value);
}

void AudioEngine::setClapMinInterval(float value) {
    analyzer_->setClapMinInterval(value);
}

void AudioEngine::setHiHatThresholdMultiplier(float value) {
    analyzer_->setHiHatThresholdMultiplier(value);
}

void AudioEngine::setHiHatMinHighShare(float value) {
    analyzer_->setHiHatMinHighShare(value);
}

void AudioEngine::setHiHatMinInterval(float value) {
    analyzer_->setHiHatMinInterval(value);
}

void AudioEngine::setOnsetThreshold(float value) {
    analyzer_->setOnsetThreshold(value);
}

void AudioEngine::setFeaturesCallback(FeaturesCallback callback) {
    std::lock_guard<std::mutex> lock(featuresMutex_);
    featuresCallback_ = callback;
}

void AudioEngine::setWaveformCallback(WaveformCallback callback) {
    std::lock_guard<std::mutex> lock(waveformMutex_);
    waveformCallback_ = callback;
}

AudioFeatures AudioEngine::getFeatures() const {
    std::lock_guard<std::mutex> lock(featuresMutex_);
    return cachedFeatures_;
}

std::vector<float> AudioEngine::getWaveform() const {
    std::lock_guard<std::mutex> lock(waveformMutex_);
    return cachedWaveform_;
}

bool AudioEngine::hasNewData() {
    return newDataAvailable_.exchange(false);
}

void AudioEngine::processingLoop() {
    while (!shouldStop_.load()) {
        auto frameStart = Clock::now();
        
        // Check if capture has new data
        if (capture_->hasNewData()) {
            auto audioBuffer = capture_->getAudioBuffer();
            capture_->clearNewDataFlag();
            
            processAudioFrame(audioBuffer);
            
            // Calculate processing time
            auto frameEnd = Clock::now();
            auto duration = std::chrono::duration<double, std::milli>(frameEnd - frameStart);
            lastProcessingTimeMs_.store(duration.count());
        } else {
            // Small sleep to prevent busy-waiting
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
}

void AudioEngine::processAudioFrame(const std::vector<float>& audioBuffer) {
    if (audioBuffer.empty()) return;

    // Apply gain
    std::vector<float> processedBuffer = audioBuffer;
    float gain = gain_.load();
    if (gain != 1.0f) {
        for (float& sample : processedBuffer) {
            sample *= gain;
        }
    }

    // Analyze audio
    analyzer_->processAudio(processedBuffer);

    // Convert and cache features
    auto internalFeatures = analyzer_->getFeatures();
    AudioFeatures features = convertFeatures(internalFeatures);

    // Update caches
    {
        std::lock_guard<std::mutex> lock(featuresMutex_);
        cachedFeatures_ = features;
        newDataAvailable_.store(true);
    }
    
    {
        std::lock_guard<std::mutex> lock(waveformMutex_);
        cachedWaveform_ = processedBuffer;
    }

    // Trigger callbacks
    {
        std::lock_guard<std::mutex> lock(featuresMutex_);
        if (featuresCallback_) {
            featuresCallback_(features);
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(waveformMutex_);
        if (waveformCallback_) {
            waveformCallback_(processedBuffer);
        }
    }
}
