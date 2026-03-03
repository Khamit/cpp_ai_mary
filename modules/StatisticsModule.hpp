#pragma once
#include "../core/NeuralFieldSystem.hpp"
#include <vector>
#include <fstream>

struct Statistics {
    double total_energy = 0.0;
    double avg_phi = 0.0;
    double avg_pi = 0.0;
    int step = 0;
    double dt = 0.0;
    double simulation_time = 0.0;
};

class StatisticsModule {
public:
    StatisticsModule() : historySize(1000) {}

    // ===== Управление симуляцией =====
    void startTimer();
    void stopTimer();
    void reset();

    bool isRunning() const { return timer_running; }

    // ===== Обновление =====
    void update(const NeuralFieldSystem& system, int step, double dt);

    // ===== Доступ =====
    const std::vector<Statistics>& getHistory() const { return history; }
    const Statistics& getCurrentStats() const { return current_stats; }

    void saveToFile(const std::string& filename) const;

private:
    Statistics current_stats;
    std::vector<Statistics> history;
    size_t historySize;

    // ===== Таймер =====
    bool timer_running = false;
    double elapsed_time = 0.0;
};