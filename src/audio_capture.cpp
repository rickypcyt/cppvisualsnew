#include "audio_capture.h"
#include <iostream>
#include <cstring>

AudioCapture::AudioCapture() 
    : stream_(nullptr), hasNewData_(false), isRunning_(false) {
    audioBuffer_.resize(FRAMES_PER_BUFFER);
}

AudioCapture::~AudioCapture() {
    shutdown();
}

bool AudioCapture::initialize() {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }

    PaStreamParameters inputParameters;
    inputParameters.device = Pa_GetDefaultInputDevice();
    if (inputParameters.device == paNoDevice) {
        std::cerr << "No default input device found" << std::endl;
        return false;
    }

    inputParameters.channelCount = CHANNELS;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = nullptr;

    err = Pa_OpenStream(&stream_,
                       &inputParameters,
                       nullptr,
                       SAMPLE_RATE,
                       FRAMES_PER_BUFFER,
                       paClipOff,
                       audioCallback,
                       this);

    if (err != paNoError) {
        std::cerr << "Failed to open stream: " << Pa_GetErrorText(err) << std::endl;
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
    std::copy(samples, samples + framesPerBuffer, self->audioBuffer_.begin());
    self->hasNewData_.store(true);

    return paContinue;
}
