#include "audio_analyzer.h"
#include <algorithm>
#include <iostream>

AudioAnalyzer::AudioAnalyzer() 
    : fftInput_(FFT_SIZE), fftOutput_(FFT_SIZE), spectrum_(SPECTRUM_SIZE),
      window_(FFT_SIZE), previousEnergy_(0.0f), onsetThreshold_(1.3f),
      beatCounter_(0), beatTimer_(0.0f) {
    
    // Create Hann window
    for (int i = 0; i < FFT_SIZE; ++i) {
        window_[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (FFT_SIZE - 1)));
    }

    // Initialize features
    features_ = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    smoothedFeatures_ = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
}

AudioAnalyzer::~AudioAnalyzer() {}

void AudioAnalyzer::processAudio(const std::vector<float>& audioBuffer) {
    if (audioBuffer.size() < FFT_SIZE) {
        return;
    }

    // Copy and window the audio
    std::copy(audioBuffer.begin(), audioBuffer.begin() + FFT_SIZE, fftInput_.begin());
    applyWindow(fftInput_);
    
    // Perform FFT
    performFFT(fftInput_);
    
    // Extract features
    extractFeatures();
    
    // Smooth features
    smoothFeatures();
}

void AudioAnalyzer::applyWindow(std::vector<float>& buffer) {
    for (int i = 0; i < FFT_SIZE; ++i) {
        buffer[i] *= window_[i];
    }
}

void AudioAnalyzer::performFFT(const std::vector<float>& input) {
    // Simple DFT implementation (for demonstration)
    // In production, use FFTW or KissFFT
    for (int k = 0; k < SPECTRUM_SIZE; ++k) {
        std::complex<float> sum(0.0f, 0.0f);
        for (int n = 0; n < FFT_SIZE; ++n) {
            float angle = -2.0f * M_PI * k * n / FFT_SIZE;
            sum += input[n] * std::complex<float>(cosf(angle), sinf(angle));
        }
        fftOutput_[k] = sum;
        spectrum_[k] = std::abs(sum) / FFT_SIZE;
    }
}

void AudioAnalyzer::extractFeatures() {
    const float SAMPLE_RATE = 44100.0f;
    const float BIN_RESOLUTION = SAMPLE_RATE / FFT_SIZE;
    const int SPECTRUM_SIZE_LOCAL = FFT_SIZE / 2;
    
    // Calculate frequency band indices
    int bassStart = 0;
    int bassEnd = static_cast<int>(120.0f / BIN_RESOLUTION);
    int midEnd = static_cast<int>(2000.0f / BIN_RESOLUTION);
    int highEnd = static_cast<int>(12000.0f / BIN_RESOLUTION);
    
    // Clamp values
    bassEnd = std::min(bassEnd, SPECTRUM_SIZE_LOCAL);
    midEnd = std::min(midEnd, SPECTRUM_SIZE_LOCAL);
    highEnd = std::min(highEnd, SPECTRUM_SIZE_LOCAL);
    
    // Calculate energy in different bands
    features_.bassEnergy = 0.0f;
    for (int i = bassStart; i < bassEnd; ++i) {
        features_.bassEnergy += spectrum_[i];
    }
    
    features_.midEnergy = 0.0f;
    for (int i = bassEnd; i < midEnd; ++i) {
        features_.midEnergy += spectrum_[i];
    }
    
    features_.highEnergy = 0.0f;
    for (int i = midEnd; i < highEnd; ++i) {
        features_.highEnergy += spectrum_[i];
    }
    
    // Total energy (RMS approximation)
    features_.energy = features_.bassEnergy + features_.midEnergy + features_.highEnergy;
    
    // Onset detection
    float energyRatio = features_.energy / (previousEnergy_ + 1e-6f);
    features_.onset = (energyRatio > onsetThreshold_) ? 1.0f : 0.0f;
    previousEnergy_ = features_.energy;
    
    // Simple beat detection (based on bass energy spikes)
    if (features_.onset > 0.5f && features_.bassEnergy > 0.1f) {
        beatCounter_++;
        features_.beat = 1.0f;
    } else {
        features_.beat = 0.0f;
    }
}

void AudioAnalyzer::smoothFeatures() {
    const float ALPHA = 0.1f; // Smoothing factor
    
    smoothedFeatures_.energy = smoothedFeatures_.energy * (1.0f - ALPHA) + features_.energy * ALPHA;
    smoothedFeatures_.bassEnergy = smoothedFeatures_.bassEnergy * (1.0f - ALPHA) + features_.bassEnergy * ALPHA;
    smoothedFeatures_.midEnergy = smoothedFeatures_.midEnergy * (1.0f - ALPHA) + features_.midEnergy * ALPHA;
    smoothedFeatures_.highEnergy = smoothedFeatures_.highEnergy * (1.0f - ALPHA) + features_.highEnergy * ALPHA;
    smoothedFeatures_.onset = features_.onset; // No smoothing for onset
    smoothedFeatures_.beat = features_.beat;   // No smoothing for beat
    
    // Update features with smoothed values
    features_ = smoothedFeatures_;
}
