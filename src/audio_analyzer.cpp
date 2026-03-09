#include "audio_analyzer.h"
#include <algorithm>
#include <iostream>
#include <numeric>

AudioAnalyzer::AudioAnalyzer() 
    : fftInput_(FFT_SIZE), fftOutput_(FFT_SIZE), spectrum_(SPECTRUM_SIZE),
      window_(FFT_SIZE),
      features_{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
      smoothedFeatures_{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
      sampleRate_(48000.0f), previousEnergyRaw_(1e-4f),
      bassPeak_(1e-3f), midPeak_(1e-3f), highPeak_(1e-3f), energyPeak_(1e-3f),
      onsetThreshold_(1.25f), beatCounter_(0), beatTimer_(0.0f),
      beatIntervals_(), bpmEstimate_(0.0f) {
    
    // Create Hann window
    for (int i = 0; i < FFT_SIZE; ++i) {
        window_[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (FFT_SIZE - 1)));
    }
}
 
AudioAnalyzer::~AudioAnalyzer() {}

void AudioAnalyzer::setSampleRate(float sampleRate) {
    if (sampleRate > 0.0f) {
        sampleRate_ = sampleRate;
    }
}

void AudioAnalyzer::processAudio(const std::vector<float>& audioBuffer) {
    if (audioBuffer.size() < FFT_SIZE) {
        return;
    }

    if (sampleRate_ > 0.0f) {
        beatTimer_ += static_cast<float>(audioBuffer.size()) / sampleRate_;
        if (beatTimer_ > 3.0f) {
            // If we lose the beat for a while, slowly decay the BPM estimate
            bpmEstimate_ *= 0.98f;
            if (bpmEstimate_ < 1.0f) {
                bpmEstimate_ = 0.0f;
                beatIntervals_.clear();
            }
        }
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
    const float BIN_RESOLUTION = sampleRate_ / static_cast<float>(FFT_SIZE);
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
    features_.energy = features_.bassEnergy + features_.midEnergy + features_.highEnergy + 1e-6f;

    const float invEnergy = 1.0f / features_.energy;
    features_.bassShare = features_.bassEnergy * invEnergy;
    features_.midShare = features_.midEnergy * invEnergy;
    features_.highShare = features_.highEnergy * invEnergy;

    // Onset detection based on raw energy changes
    float energyRatio = features_.energy / (previousEnergyRaw_ + 1e-6f);
    features_.onset = (energyRatio > onsetThreshold_) ? 1.0f : 0.0f;
    previousEnergyRaw_ = 0.85f * previousEnergyRaw_ + 0.15f * features_.energy;
    
    // Simple beat detection (based on bass energy spikes)
    if (features_.onset > 0.5f && features_.bassEnergy > 0.1f) {
        beatCounter_++;
        features_.beat = 1.0f;
        if (beatTimer_ > 0.1f && beatTimer_ < 2.0f) {
            beatIntervals_.push_back(beatTimer_);
            if (beatIntervals_.size() > MAX_BEAT_HISTORY) {
                beatIntervals_.pop_front();
            }

            const float intervalSum = std::accumulate(beatIntervals_.begin(), beatIntervals_.end(), 0.0f);
            const float avgInterval = intervalSum / static_cast<float>(beatIntervals_.size());
            if (avgInterval > 1e-3f) {
                bpmEstimate_ = 60.0f / avgInterval;
            }
        }
        beatTimer_ = 0.0f;
    } else {
        features_.beat = 0.0f;
    }

    features_.bpm = bpmEstimate_;
}

void AudioAnalyzer::smoothFeatures() {
    const float ALPHA = 0.15f; // Smoothing factor
    const float BPM_ALPHA = 0.2f;

    smoothedFeatures_.energy = smoothedFeatures_.energy * (1.0f - ALPHA) + features_.energy * ALPHA;
    smoothedFeatures_.bassEnergy = smoothedFeatures_.bassEnergy * (1.0f - ALPHA) + features_.bassEnergy * ALPHA;
    smoothedFeatures_.midEnergy = smoothedFeatures_.midEnergy * (1.0f - ALPHA) + features_.midEnergy * ALPHA;
    smoothedFeatures_.highEnergy = smoothedFeatures_.highEnergy * (1.0f - ALPHA) + features_.highEnergy * ALPHA;
    smoothedFeatures_.bassShare = smoothedFeatures_.bassShare * (1.0f - ALPHA) + features_.bassShare * ALPHA;
    smoothedFeatures_.midShare = smoothedFeatures_.midShare * (1.0f - ALPHA) + features_.midShare * ALPHA;
    smoothedFeatures_.highShare = smoothedFeatures_.highShare * (1.0f - ALPHA) + features_.highShare * ALPHA;
    smoothedFeatures_.onset = features_.onset; // No smoothing for onset
    smoothedFeatures_.beat = features_.beat;   // No smoothing for beat
    smoothedFeatures_.bpm = smoothedFeatures_.bpm * (1.0f - BPM_ALPHA) + features_.bpm * BPM_ALPHA;

    features_ = smoothedFeatures_;

    // Adaptive normalization to keep values between 0 and 1
    auto normalizeWithPeak = [](float value, float& peak) {
        const float decay = 0.995f;
        peak = std::max(value, peak * decay);
        float normalized = peak > 1e-5f ? value / peak : 0.0f;
        return std::clamp(normalized, 0.0f, 1.0f);
    };

    features_.bassEnergy = std::pow(normalizeWithPeak(features_.bassEnergy, bassPeak_), 0.6f);
    features_.midEnergy = std::pow(normalizeWithPeak(features_.midEnergy, midPeak_), 0.7f);
    features_.highEnergy = std::pow(normalizeWithPeak(features_.highEnergy, highPeak_), 0.8f);
    features_.energy = std::pow(normalizeWithPeak(features_.energy, energyPeak_), 0.5f);

    float shareSum = features_.bassShare + features_.midShare + features_.highShare;
    if (shareSum > 1e-6f) {
        features_.bassShare = std::clamp(features_.bassShare / shareSum, 0.0f, 1.0f);
        features_.midShare = std::clamp(features_.midShare / shareSum, 0.0f, 1.0f);
        features_.highShare = std::clamp(features_.highShare / shareSum, 0.0f, 1.0f);
    } else {
        features_.bassShare = features_.midShare = features_.highShare = 0.0f;
    }
}
