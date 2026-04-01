#ifndef AUDIO_ANALYZER_H
#define AUDIO_ANALYZER_H

#include <vector>
#include <complex>
#include <cmath>
#include <deque>
#include <array>

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
        float kick;
        float clap;
        float hiHat;
        float bpm;
        
        // Enhanced frequency analysis
        static constexpr int NUM_MEL_BANDS = 12;
        std::array<float, NUM_MEL_BANDS> melBandEnergies{};
        std::array<float, NUM_MEL_BANDS> melBandShares{};
        
        // Spectral features
        float spectralFlux = 0.0f;      // Frame-to-frame spectral change
        float zeroCrossingRate = 0.0f; // Percussion vs tonal
        float spectralCentroid = 0.0f; // Brightness of sound
        float spectralRolloff = 0.0f;  // Frequency below which 85% energy resides
        
        // Percussion/Tonal classification (0 = tonal, 1 = percussion)
        float percussionTonalRatio = 0.0f;
    };

    AudioAnalyzer();
    ~AudioAnalyzer();

    void processAudio(const std::vector<float>& audioBuffer);
    const AudioFeatures& getFeatures() const { return features_; }
    const std::vector<float>& getSpectrum() const { return spectrum_; }
    void setSampleRate(float sampleRate);

    // Beat detection parameter getters/setters
    void setKickThresholdMultiplier(float value) { kickThresholdMultiplier_ = value; }
    float getKickThresholdMultiplier() const { return kickThresholdMultiplier_; }
    void setKickMinInterval(float value) { kickMinInterval_ = value; }
    float getKickMinInterval() const { return kickMinInterval_; }
    
    void setClapThresholdMultiplier(float value) { clapThresholdMultiplier_ = value; }
    float getClapThresholdMultiplier() const { return clapThresholdMultiplier_; }
    void setClapMaxBassShare(float value) { clapMaxBassShare_ = value; }
    float getClapMaxBassShare() const { return clapMaxBassShare_; }
    void setClapMinInterval(float value) { clapMinInterval_ = value; }
    float getClapMinInterval() const { return clapMinInterval_; }
    
    void setHiHatThresholdMultiplier(float value) { hiHatThresholdMultiplier_ = value; }
    float getHiHatThresholdMultiplier() const { return hiHatThresholdMultiplier_; }
    void setHiHatMinHighShare(float value) { hiHatMinHighShare_ = value; }
    float getHiHatMinHighShare() const { return hiHatMinHighShare_; }
    void setHiHatMinInterval(float value) { hiHatMinInterval_ = value; }
    float getHiHatMinInterval() const { return hiHatMinInterval_; }
    
    void setOnsetThreshold(float value) { onsetThreshold_ = value; }
    float getOnsetThreshold() const { return onsetThreshold_; }

private:
    void applyWindow(std::vector<float>& buffer);
    void performFFT(const std::vector<float>& input);
    void extractFeatures();
    void smoothFeatures();

    std::vector<float> fftInput_;
    std::vector<std::complex<float>> fftOutput_;
    std::vector<float> spectrum_;
    std::vector<float> prevSpectrum_;  // For spectral flux
    std::vector<float> window_;
    std::vector<float> melFilterBank_; // Mel scale filter bank weights
    
    AudioFeatures features_;
    AudioFeatures smoothedFeatures_;
    
    float sampleRate_;
    float previousEnergyRaw_;
    float bassEnergyEMA_;
    float midEnergyEMA_;
    float highEnergyEMA_;
    float bassPeak_;
    float midPeak_;
    float highPeak_;
    float energyPeak_;
    float onsetThreshold_;
    int beatCounter_;
    float beatTimer_;
    std::deque<float> beatIntervals_;
    float bpmEstimate_;
    float lastBeatInterval_;
    float kickTimer_;
    float clapTimer_;
    float hiHatTimer_;

    // Tunable beat detection parameters
    float kickThresholdMultiplier_ = 1.6f;
    float kickMinInterval_ = 0.08f;
    
    float clapThresholdMultiplier_ = 1.4f;
    float clapMaxBassShare_ = 0.55f;
    float clapMinInterval_ = 0.1f;
    
    float hiHatThresholdMultiplier_ = 1.35f;
    float hiHatMinHighShare_ = 0.18f;
    float hiHatMinInterval_ = 0.05f;

    static constexpr int MAX_BEAT_HISTORY = 8;
};

#endif // AUDIO_ANALYZER_H
