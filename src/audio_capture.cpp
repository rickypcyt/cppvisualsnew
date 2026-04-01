#include "audio_capture.h"
#include <iostream>
#include <cstring>
#include <algorithm>

AudioCapture::AudioCapture() 
    : stream_(nullptr), hasNewData_(false), isRunning_(false),
      channelCount_(1), sampleRate_(SAMPLE_RATE) {
    audioBuffer_.resize(FRAMES_PER_BUFFER);
}

AudioCapture::~AudioCapture() {
    shutdown();
}

bool AudioCapture::initialize() {
    // Initialize PortAudio first
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }
    
    int defaultDevice = getDefaultInputDevice();
    
    // If default device is invalid, try to find the first available input device
    if (defaultDevice < 0) {
        std::cout << "No default input device found, searching for available input devices..." << std::endl;
        int numDevices = Pa_GetDeviceCount();
        std::cout << "Found " << numDevices << " total devices" << std::endl;
        
        for (int i = 0; i < numDevices; ++i) {
            const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
            if (deviceInfo && deviceInfo->maxInputChannels > 0) {
                defaultDevice = i;
                std::cout << "Found input device: " << deviceInfo->name << " (index: " << i << ")" << std::endl;
                break;
            }
        }
    }
    
    if (defaultDevice < 0) {
        std::cerr << "No input devices available" << std::endl;
        Pa_Terminate();
        return false;
    }
    
    bool result = initialize(defaultDevice);
    if (!result) {
        Pa_Terminate();
    }
    return result;
}

bool AudioCapture::initialize(int deviceIndex) {
    // Initialize PortAudio if not already initialized
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }

    // Validate device index
    int numDevices = Pa_GetDeviceCount();
    if (deviceIndex < 0 || deviceIndex >= numDevices) {
        std::cerr << "Invalid device index: " << deviceIndex << std::endl;
        return false;
    }

    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(deviceIndex);
    if (!deviceInfo) {
        std::cerr << "Failed to get device info for index: " << deviceIndex << std::endl;
        return false;
    }

    std::cout << "Using audio device: " << deviceInfo->name << std::endl;

    audioBuffer_.assign(FRAMES_PER_BUFFER, 0.0f);
    hasNewData_.store(false);

    // Try different sample rates if the default doesn't work
    std::vector<double> sampleRates;
    if (deviceInfo->defaultSampleRate > 0.0) {
        sampleRates.push_back(deviceInfo->defaultSampleRate);
    }
    sampleRates.insert(sampleRates.end(), {48000.0, 44100.0, 22050.0, 16000.0, 8000.0});
    sampleRates.erase(std::unique(sampleRates.begin(), sampleRates.end()), sampleRates.end());

    PaStreamParameters inputParameters;
    
    inputParameters.device = deviceIndex;
    inputParameters.channelCount = std::clamp(deviceInfo->maxInputChannels, 1, MAX_CAPTURE_CHANNELS);
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = deviceInfo->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = nullptr;

    bool streamOpened = false;

    for (double sampleRate : sampleRates) {
        std::cout << "Trying sample rate: " << sampleRate << " Hz..." << std::endl;
        
        PaError err = Pa_IsFormatSupported(&inputParameters, nullptr, sampleRate);
        if (err == paFormatIsSupported) {
            err = Pa_OpenStream(&stream_,
                               &inputParameters,
                               nullptr,
                               sampleRate,
                               FRAMES_PER_BUFFER,
                               paClipOff,
                               audioCallback,
                               this);

            if (err == paNoError) {
                if (const PaStreamInfo* info = Pa_GetStreamInfo(stream_)) {
                    sampleRate_ = info->sampleRate;
                } else {
                    sampleRate_ = sampleRate;
                }
                channelCount_ = inputParameters.channelCount;
                streamOpened = true;
                std::cout << "Successfully opened stream with sample rate: " << sampleRate_ << " Hz" << std::endl;
                break;
            } else {
                std::cerr << "Failed to open stream with " << sampleRate << " Hz: " << Pa_GetErrorText(err) << std::endl;
            }
        } else {
            std::cout << "Sample rate " << sampleRate << " Hz not supported" << std::endl;
        }
    }

    if (!streamOpened) {
        std::cerr << "Failed to open stream with any supported sample rate" << std::endl;
        return false;
    }

    return true;
}

void AudioCapture::shutdown() {
    if (stream_) {
        stop();
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }
    Pa_Terminate();
}

bool AudioCapture::start() {
    if (!stream_) return false;
    
    PaError err = Pa_StartStream(stream_);
    if (err != paNoError) {
        std::cerr << "Failed to start stream: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }

    isRunning_ = true;
    return true;
}

void AudioCapture::stop() {
    if (stream_ && isRunning_) {
        Pa_StopStream(stream_);
        isRunning_ = false;
    }
}

std::vector<float> AudioCapture::getAudioBuffer() {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    return audioBuffer_;
}

bool AudioCapture::hasNewData() {
    return hasNewData_.load();
}

void AudioCapture::clearNewDataFlag() {
    hasNewData_.store(false);
}

int AudioCapture::audioCallback(const void* inputBuffer,
                               void* outputBuffer,
                               unsigned long framesPerBuffer,
                               const PaStreamCallbackTimeInfo* timeInfo,
                               PaStreamCallbackFlags statusFlags,
                               void* userData) {
    AudioCapture* self = static_cast<AudioCapture*>(userData);
    
    if (inputBuffer == nullptr) {
        return paContinue;
    }

    std::lock_guard<std::mutex> lock(self->bufferMutex_);
    const float* samples = static_cast<const float*>(inputBuffer);

    const int channels = std::max(1, self->channelCount_);
    const size_t availableFrames = std::min<size_t>(framesPerBuffer, self->audioBuffer_.size());

    for (size_t frame = 0; frame < availableFrames; ++frame) {
        float mixed = 0.0f;
        for (int ch = 0; ch < channels; ++ch) {
            mixed += samples[frame * channels + ch];
        }
        self->audioBuffer_[frame] = mixed / static_cast<float>(channels);
    }
    if (availableFrames < self->audioBuffer_.size()) {
        std::fill(self->audioBuffer_.begin() + availableFrames, self->audioBuffer_.end(), 0.0f);
    }
    self->hasNewData_.store(true);

    return paContinue;
}

void AudioCapture::listAvailableDevices() {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        return;
    }

    int numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) {
        std::cerr << "No audio devices found" << std::endl;
        Pa_Terminate();
        return;
    }

    std::cout << "\n=== Available Audio Devices ===" << std::endl;
    std::cout << "Index | Name                    | Max Inputs | Max Outputs" << std::endl;
    std::cout << "------|-------------------------|------------|------------" << std::endl;

    int defaultInput = getDefaultInputDevice();
    
    for (int i = 0; i < numDevices; ++i) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        if (!deviceInfo) continue;
        
        std::string marker = (i == defaultInput) ? " [DEFAULT]" : "";
        
        printf("%-6d| %-23s | %-10d | %-11d%s\n", 
               i, 
               deviceInfo->name, 
               deviceInfo->maxInputChannels, 
               deviceInfo->maxOutputChannels,
               marker.c_str());
    }
    
    std::cout << "\nUse the device index to select input device." << std::endl;
    std::cout << "Example: ./audio_visualizer --device 2" << std::endl;
    
    Pa_Terminate();
}

int AudioCapture::getDefaultInputDevice() {
    return Pa_GetDefaultInputDevice();
}
