#pragma once

#include <vector>
#include <chrono>
#include <string>

#include "UnifiedStatsCollector.hpp"

class NeuralFieldSystem;

struct SystemSnapshot {
    int step;
    double dt;
    double total_energy;
    double avg_phi;
    double avg_pi;
    double system_entropy;
    double fitness;
    size_t memory_records;
    double timestamp;
};

class StatisticsModule {
private:
    std::vector<SystemSnapshot> history;

    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_step_time;

    bool timer_running;

    UnifiedStatsCollector* stats_collector;

public:
    StatisticsModule();

    void setStatsCollector(UnifiedStatsCollector* collector);

    void startTimer();
    void stopTimer();

    void reset();

    void update(const NeuralFieldSystem& system, int step, double dt);

    void updateFromCollector();

    const SystemSnapshot& getCurrentStats() const;

    const std::vector<SystemSnapshot>& getHistory() const;

    void saveToFile(const std::string& filename) const;
};