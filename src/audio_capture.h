#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include <portaudio.h>
#include <vector>
#include <mutex>
#include <atomic>

class AudioCapture {
public:
    static const int SAMPLE_RATE = 44100;
    static const int FRAMES_PER_BUFFER = 512;
    static const int CHANNELS = 1;

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

private:
    PaStream* stream_;
    std::vector<float> audioBuffer_;
    std::mutex bufferMutex_;
    std::atomic<bool> hasNewData_;
    std::atomic<bool> isRunning_;

    static int audioCallback(const void* inputBuffer,
                           void* outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void* userData);
};

#endif // AUDIO_CAPTURE_H
