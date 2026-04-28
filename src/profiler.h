#ifndef PROFILER_H
#define PROFILER_H

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <iostream>

class Profiler {
public:
    struct Timer {
        std::chrono::high_resolution_clock::time_point start;
        std::string name;
        
        Timer(const std::string& n) : name(n) {
            start = std::chrono::high_resolution_clock::now();
        }
        
        ~Timer() {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<float, std::milli>(end - start).count();
            Profiler::getInstance().recordSample(name, duration);
        }
    };

    static Profiler& getInstance() {
        static Profiler instance;
        return instance;
    }

    void recordSample(const std::string& name, float duration_ms) {
        samples_[name].push_back(duration_ms);
        
        // Keep only last 1000 samples per category to avoid memory bloat
        if (samples_[name].size() > 1000) {
            samples_[name].erase(samples_[name].begin());
        }
    }

    struct Stats {
        float min_ms;
        float max_ms;
        float avg_ms;
        float median_ms;
        float p99_ms; // 99th percentile
        size_t sample_count;
    };

    Stats getStats(const std::string& name) const {
        auto it = samples_.find(name);
        if (it == samples_.end() || it->second.empty()) {
            return {0, 0, 0, 0, 0, 0};
        }

        const auto& data = it->second;
        std::vector<float> sorted = data;
        std::sort(sorted.begin(), sorted.end());

        Stats stats;
        stats.min_ms = sorted.front();
        stats.max_ms = sorted.back();
        stats.sample_count = sorted.size();

        float sum = 0;
        for (float val : sorted) {
            sum += val;
        }
        stats.avg_ms = sum / sorted.size();

        stats.median_ms = sorted[sorted.size() / 2];
        stats.p99_ms = sorted[static_cast<size_t>(sorted.size() * 0.99)];

        return stats;
    }

    void printReport() const {
        std::cout << "\n========== PROFILER REPORT ==========\n";
        std::cout << "Category                | Avg    | Min    | Max    | Median | P99    | Samples\n";
        std::cout << "------------------------|--------|--------|--------|--------|--------|--------\n";
        
        for (const auto& [name, data] : samples_) {
            Stats stats = getStats(name);
            printf("%-22s | %6.2f | %6.2f | %6.2f | %6.2f | %6.2f | %7zu\n",
                   name.c_str(), stats.avg_ms, stats.min_ms, stats.max_ms,
                   stats.median_ms, stats.p99_ms, stats.sample_count);
        }
        std::cout << "====================================\n\n";
    }

    void reset() {
        samples_.clear();
    }

private:
    Profiler() = default;
    std::unordered_map<std::string, std::vector<float>> samples_;
};

#define PROFILE_SCOPE(name) Profiler::Timer _profiler_timer_##__LINE__(name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCTION__)

#endif // PROFILER_H
