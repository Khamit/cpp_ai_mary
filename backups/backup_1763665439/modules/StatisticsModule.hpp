//cpp_ai_test/modules/StatisticsModule.hpp
#pragma once
#include "../core/NeuralFieldSystem.hpp"
#include <vector>
#include <string>
#include <map>

struct Statistics {
    double total_energy = 0.0;
    double avg_phi = 0.0;
    double avg_pi = 0.0;
    double min_phi = 0.0;
    double max_phi = 0.0;
    double std_phi = 0.0;
    int state = 0;
    int step = 0;
    double simulation_time = 0.0;
};

class StatisticsModule {
public:
    StatisticsModule() = default;
    
    void update(const NeuralFieldSystem& system, int step, double dt);
    void reset();
    
    const Statistics& getCurrentStats() const { return current_stats; }
    const std::vector<Statistics>& getHistory() const { return history; }
    void saveToFile(const std::string& filename) const;
    
    void startTimer() { timer_running = true; }
    void stopTimer() { timer_running = false; }
    double getElapsedTime() const { return elapsed_time; }

private:
    Statistics current_stats;
    std::vector<Statistics> history;
    bool timer_running = false;
    double elapsed_time = 0.0;
    
    double calculateStdDev(const std::vector<double>& values, double mean) const;
};