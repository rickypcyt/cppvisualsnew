#include "audio_analyzer.h"
#include <algorithm>
#include <iostream>
#include <numeric>

AudioAnalyzer::AudioAnalyzer() 
    : fftInput_(FFT_SIZE), fftOutput_(FFT_SIZE), spectrum_(SPECTRUM_SIZE),
      prevSpectrum_(SPECTRUM_SIZE, 0.0f),
      window_(FFT_SIZE),
      features_{},
      smoothedFeatures_{},
      sampleRate_(48000.0f), previousEnergyRaw_(1e-4f),
      bassEnergyEMA_(1e-3f), midEnergyEMA_(1e-3f), highEnergyEMA_(1e-3f),
      bassPeak_(1e-3f), midPeak_(1e-3f), highPeak_(1e-3f), energyPeak_(1e-3f),
      onsetThreshold_(1.25f), beatCounter_(0), beatTimer_(0.0f),
      beatIntervals_(), bpmEstimate_(0.0f), lastBeatInterval_(0.5f),
      kickTimer_(1.0f), clapTimer_(1.0f), hiHatTimer_(1.0f) {
    
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

    float frameDuration = 0.0f;
    if (sampleRate_ > 0.0f) {
        frameDuration = static_cast<float>(audioBuffer.size()) / sampleRate_;
        beatTimer_ += frameDuration;
        kickTimer_ += frameDuration;
        clapTimer_ += frameDuration;
        hiHatTimer_ += frameDuration;
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
    
    // === ZERO-CROSSING RATE (percussion vs tonal) ===
    // High ZCR = more noise-like/percussive, Low ZCR = more tonal/harmonic
    int zeroCrossings = 0;
    for (int i = 1; i < FFT_SIZE; ++i) {
        if ((fftInput_[i] > 0.0f) != (fftInput_[i-1] > 0.0f)) {
            zeroCrossings++;
        }
    }
    features_.zeroCrossingRate = static_cast<float>(zeroCrossings) / static_cast<float>(FFT_SIZE - 1);
    
    applyWindow(fftInput_);
    
    // Perform FFT
    performFFT(fftInput_);
    
    // Extract features
    extractFeatures();
    
    // === PERCUSSION vs TONAL CLASSIFICATION ===
    // Combine spectral flux and ZCR for classification
    // Percussion: high spectral flux + high ZCR
    // Tonal: low spectral flux + low ZCR
    float fluxNorm = std::min(features_.spectralFlux / (features_.energy + 1e-6f), 1.0f);
    features_.percussionTonalRatio = 0.6f * fluxNorm + 0.4f * features_.zeroCrossingRate;
    
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
    int clapStart = static_cast<int>(500.0f / BIN_RESOLUTION);
    int clapEnd = static_cast<int>(2500.0f / BIN_RESOLUTION);
    int hiHatStart = static_cast<int>(6000.0f / BIN_RESOLUTION);
    int hiHatEnd = highEnd;

    // Clamp values
    bassEnd = std::min(bassEnd, SPECTRUM_SIZE_LOCAL);
    midEnd = std::min(midEnd, SPECTRUM_SIZE_LOCAL);
    highEnd = std::min(highEnd, SPECTRUM_SIZE_LOCAL);
    clapStart = std::clamp(clapStart, bassEnd, SPECTRUM_SIZE_LOCAL);
    clapEnd = std::clamp(clapEnd, clapStart + 1, SPECTRUM_SIZE_LOCAL);
    hiHatStart = std::clamp(hiHatStart, midEnd, SPECTRUM_SIZE_LOCAL);
    hiHatEnd = std::clamp(hiHatEnd, hiHatStart + 1, SPECTRUM_SIZE_LOCAL);

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

    float clapBandEnergy = 0.0f;
    for (int i = clapStart; i < clapEnd; ++i) {
        clapBandEnergy += spectrum_[i];
    }

    float hiHatBandEnergy = 0.0f;
    for (int i = hiHatStart; i < hiHatEnd; ++i) {
        hiHatBandEnergy += spectrum_[i];
    }

    // Total energy (RMS approximation)
    features_.energy = features_.bassEnergy + features_.midEnergy + features_.highEnergy + 1e-6f;

    const float invEnergy = 1.0f / features_.energy;
    features_.bassShare = features_.bassEnergy * invEnergy;
    features_.midShare = features_.midEnergy * invEnergy;
    features_.highShare = features_.highEnergy * invEnergy;

    // === MEL-SCALE FREQUENCY BANDS (12 bands) ===
    // Convert Hz to Mel: mel = 2595 * log10(1 + hz/700)
    // Mel scale better represents human hearing perception
    const float MIN_MEL = 0.0f;
    const float MAX_MEL = 2595.0f * log10f(1.0f + 12000.0f / 700.0f); // ~3600 mel for 12kHz
    const float MEL_STEP = (MAX_MEL - MIN_MEL) / AudioFeatures::NUM_MEL_BANDS;
    
    for (int melBand = 0; melBand < AudioFeatures::NUM_MEL_BANDS; ++melBand) {
        float melStart = MIN_MEL + melBand * MEL_STEP;
        float melEnd = melStart + MEL_STEP;
        
        // Convert back to Hz: hz = 700 * (10^(mel/2595) - 1)
        float freqStart = 700.0f * (powf(10.0f, melStart / 2595.0f) - 1.0f);
        float freqEnd = 700.0f * (powf(10.0f, melEnd / 2595.0f) - 1.0f);
        
        int binStart = static_cast<int>(freqStart / BIN_RESOLUTION);
        int binEnd = static_cast<int>(freqEnd / BIN_RESOLUTION);
        binStart = std::clamp(binStart, 0, SPECTRUM_SIZE_LOCAL);
        binEnd = std::clamp(binEnd, binStart + 1, SPECTRUM_SIZE_LOCAL);
        
        // Calculate energy in this mel band
        features_.melBandEnergies[melBand] = 0.0f;
        for (int i = binStart; i < binEnd; ++i) {
            features_.melBandEnergies[melBand] += spectrum_[i];
        }
    }
    
    // Calculate mel band shares
    float totalMelEnergy = 0.0f;
    for (int i = 0; i < AudioFeatures::NUM_MEL_BANDS; ++i) {
        totalMelEnergy += features_.melBandEnergies[i];
    }
    if (totalMelEnergy > 1e-6f) {
        for (int i = 0; i < AudioFeatures::NUM_MEL_BANDS; ++i) {
            features_.melBandShares[i] = features_.melBandEnergies[i] / totalMelEnergy;
        }
    }

    // === SPECTRAL FLUX (frame-to-frame spectral change) ===
    // High flux = sudden change in spectrum = likely percussion/onset
    features_.spectralFlux = 0.0f;
    for (int i = 0; i < SPECTRUM_SIZE_LOCAL; ++i) {
        float diff = spectrum_[i] - prevSpectrum_[i];
        if (diff > 0) {
            features_.spectralFlux += diff;
        }
    }
    
    // Store current spectrum for next frame
    std::copy(spectrum_.begin(), spectrum_.begin() + SPECTRUM_SIZE_LOCAL, prevSpectrum_.begin());

    // === SPECTRAL CENTROID (brightness) ===
    float weightedSum = 0.0f;
    float sum = 0.0f;
    for (int i = 0; i < SPECTRUM_SIZE_LOCAL; ++i) {
        float freq = i * BIN_RESOLUTION;
        weightedSum += freq * spectrum_[i];
        sum += spectrum_[i];
    }
    features_.spectralCentroid = (sum > 1e-6f) ? weightedSum / sum : 0.0f;

    // === SPECTRAL ROLLOFF (frequency below which 85% of energy resides) ===
    float cumulative = 0.0f;
    float threshold = 0.85f * sum;
    features_.spectralRolloff = 0.0f;
    for (int i = 0; i < SPECTRUM_SIZE_LOCAL; ++i) {
        cumulative += spectrum_[i];
        if (cumulative >= threshold) {
            features_.spectralRolloff = i * BIN_RESOLUTION;
            break;
        }
    }

    // Note: Zero-crossing rate and percussion/tonal ratio calculated in processAudio
    // where we have access to the raw audio buffer

    // Update band EMAs for adaptive thresholds (keep in raw energy domain)
    const float EMA_ALPHA = 0.12f;
    auto updateEMA = [EMA_ALPHA](float ema, float value) {
        return (1.0f - EMA_ALPHA) * ema + EMA_ALPHA * value;
    };
    bassEnergyEMA_ = updateEMA(bassEnergyEMA_, features_.bassEnergy);
    midEnergyEMA_ = updateEMA(midEnergyEMA_, clapBandEnergy);
    highEnergyEMA_ = updateEMA(highEnergyEMA_, hiHatBandEnergy);

    // Onset detection based on raw energy changes
    float energyRatio = features_.energy / (previousEnergyRaw_ + 1e-6f);
    features_.onset = (energyRatio > onsetThreshold_) ? 1.0f : 0.0f;
    previousEnergyRaw_ = 0.85f * previousEnergyRaw_ + 0.15f * features_.energy;
    
    // Simple beat detection (based on bass energy spikes)
    if (features_.onset > 0.5f && features_.bassEnergy > 0.1f) {
        beatCounter_++;
        features_.beat = 1.0f;

        const float minInterval = 0.24f;   // ~250 BPM upper bound
        const float maxInterval = 2.0f;     // ~30 BPM lower bound
        bool acceptBeat = beatTimer_ >= minInterval && beatTimer_ <= maxInterval;
        if (acceptBeat && lastBeatInterval_ > 1e-3f) {
            float minSpacing = std::max(0.18f, lastBeatInterval_ * 0.6f);
            if (beatTimer_ < minSpacing) {
                acceptBeat = false;
            }
        }

        if (acceptBeat) {
            beatIntervals_.push_back(beatTimer_);
            if (beatIntervals_.size() > MAX_BEAT_HISTORY) {
                beatIntervals_.pop_front();
            }

            const float intervalSum = std::accumulate(beatIntervals_.begin(), beatIntervals_.end(), 0.0f);
            const float avgInterval = intervalSum / static_cast<float>(beatIntervals_.size());
            if (avgInterval > 1e-3f) {
                bpmEstimate_ = 60.0f / avgInterval;
            }

            lastBeatInterval_ = beatTimer_;
        }

        beatTimer_ = 0.0f;
    } else {
        features_.beat = 0.0f;
    }

    features_.bpm = bpmEstimate_;

    // Kick detection: strong bass spike on onset
    bool kickDetected = false;
    if (features_.onset > 0.5f && kickTimer_ > kickMinInterval_) {
        float kickThreshold = std::max(bassEnergyEMA_ * kickThresholdMultiplier_, 0.0025f);
        if (features_.bassEnergy > kickThreshold) {
            kickDetected = true;
            kickTimer_ = 0.0f;
        }
    }
    features_.kick = kickDetected ? 1.0f : 0.0f;

    // Clap detection: mid-band burst with onset and moderate bass dominance
    bool clapDetected = false;
    if (features_.onset > 0.5f && clapTimer_ > clapMinInterval_) {
        float clapThreshold = std::max(midEnergyEMA_ * clapThresholdMultiplier_, 0.0015f);
        if (clapBandEnergy > clapThreshold && features_.bassShare < clapMaxBassShare_) {
            clapDetected = true;
            clapTimer_ = 0.0f;
        }
    }
    features_.clap = clapDetected ? 1.0f : 0.0f;

    // Hi-hat detection: persistent high-frequency spikes
    bool hiHatDetected = false;
    if (hiHatTimer_ > hiHatMinInterval_) {
        float hiHatThreshold = std::max(highEnergyEMA_ * hiHatThresholdMultiplier_, 0.001f);
        if (hiHatBandEnergy > hiHatThreshold && features_.highShare > hiHatMinHighShare_) {
            hiHatDetected = true;
            hiHatTimer_ = 0.0f;
        }
    }
    features_.hiHat = hiHatDetected ? 1.0f : 0.0f;
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
    smoothedFeatures_.kick = features_.kick;
    smoothedFeatures_.clap = features_.clap;
    smoothedFeatures_.hiHat = features_.hiHat;
    smoothedFeatures_.bpm = smoothedFeatures_.bpm * (1.0f - BPM_ALPHA) + features_.bpm * BPM_ALPHA;
    
    // Smooth mel band energies
    for (int i = 0; i < AudioFeatures::NUM_MEL_BANDS; ++i) {
        smoothedFeatures_.melBandEnergies[i] = smoothedFeatures_.melBandEnergies[i] * (1.0f - ALPHA) + features_.melBandEnergies[i] * ALPHA;
        smoothedFeatures_.melBandShares[i] = smoothedFeatures_.melBandShares[i] * (1.0f - ALPHA) + features_.melBandShares[i] * ALPHA;
    }
    
    // Smooth spectral features
    smoothedFeatures_.spectralFlux = smoothedFeatures_.spectralFlux * (1.0f - ALPHA) + features_.spectralFlux * ALPHA;
    smoothedFeatures_.zeroCrossingRate = smoothedFeatures_.zeroCrossingRate * (1.0f - ALPHA) + features_.zeroCrossingRate * ALPHA;
    smoothedFeatures_.spectralCentroid = smoothedFeatures_.spectralCentroid * (1.0f - ALPHA) + features_.spectralCentroid * ALPHA;
    smoothedFeatures_.spectralRolloff = smoothedFeatures_.spectralRolloff * (1.0f - ALPHA) + features_.spectralRolloff * ALPHA;
    smoothedFeatures_.percussionTonalRatio = smoothedFeatures_.percussionTonalRatio * (1.0f - ALPHA) + features_.percussionTonalRatio * ALPHA;

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
