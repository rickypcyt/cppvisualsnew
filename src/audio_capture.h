#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include <portaudio.h>
#include <vector>
#include <mutex>
#include <atomic>

#include "audio_analyzer.h"

class AudioCapture {
public:
    static constexpr int SAMPLE_RATE = 48000;
    static constexpr int FRAMES_PER_BUFFER = AudioAnalyzer::FFT_SIZE;
    static constexpr int MAX_CAPTURE_CHANNELS = 2;

    AudioCapture();
    ~AudioCapture();

    bool initialize();
    bool initialize(int deviceIndex);
    void shutdown();
    bool start();
    void stop();
    
    static void listAvailableDevices();
    static int getDefaultInputDevice();
    
    std::vector<float> getAudioBuffer();
    bool hasNewData();
    void clearNewDataFlag();
    double getSampleRate() const { return sampleRate_; }

private:
    PaStream* stream_;
    std::vector<float> audioBuffer_;
    std::mutex bufferMutex_;
    std::atomic<bool> hasNewData_;
    std::atomic<bool> isRunning_;
    int channelCount_;
    double sampleRate_;

    static int audioCallback(const void* inputBuffer,
                           void* outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void* userData);
};

#endif // AUDIO_CAPTURE_H
