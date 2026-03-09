#ifndef AUDIO_ANALYZER_H
#define AUDIO_ANALYZER_H

#include <vector>
#include <complex>
#include <cmath>
#include <deque>

// Forward declaration to avoid circular include
class AudioCapture;

class AudioAnalyzer {
public:
    static const int FFT_SIZE = 1024;
    static const int SPECTRUM_SIZE = FFT_SIZE / 2;

    struct AudioFeatures {
        float energy;
        float bassEnergy;
        float midEnergy;
        float highEnergy;
        float bassShare;
        float midShare;
        float highShare;
        float onset;
        float beat;
        float bpm;
    };

    AudioAnalyzer();
    ~AudioAnalyzer();

    void processAudio(const std::vector<float>& audioBuffer);
    const AudioFeatures& getFeatures() const { return features_; }
    const std::vector<float>& getSpectrum() const { return spectrum_; }
    void setSampleRate(float sampleRate);

private:
    void applyWindow(std::vector<float>& buffer);
    void performFFT(const std::vector<float>& input);
    void extractFeatures();
    void smoothFeatures();

    std::vector<float> fftInput_;
    std::vector<std::complex<float>> fftOutput_;
    std::vector<float> spectrum_;
    std::vector<float> window_;
    
    AudioFeatures features_;
    AudioFeatures smoothedFeatures_;
    
    float sampleRate_;
    float previousEnergyRaw_;
    float bassPeak_;
    float midPeak_;
    float highPeak_;
    float energyPeak_;
    float onsetThreshold_;
    int beatCounter_;
    float beatTimer_;
    std::deque<float> beatIntervals_;
    float bpmEstimate_;

    static constexpr int MAX_BEAT_HISTORY = 8;
};

#endif // AUDIO_ANALYZER_H
