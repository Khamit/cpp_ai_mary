//cpp_ai_test/modules/StatisticsModule.сpp
#include "StatisticsModule.hpp"
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>

void StatisticsModule::update(const NeuralFieldSystem& system, int step, double dt) {
    if (timer_running) {
        elapsed_time += dt;
    }
    
    const auto& phi = system.getPhi();
    const auto& pi = system.getPi();
    
    // Базовые статистики
    current_stats.step = step;
    current_stats.simulation_time = elapsed_time;
    current_stats.total_energy = system.computeTotalEnergy();
    
    // Статистики по phi
    double sum_phi = 0.0, sum_pi = 0.0;
    current_stats.min_phi = phi[0];
    current_stats.max_phi = phi[0];
    
    for (int i = 0; i < system.N; i++) {
        sum_phi += phi[i];
        sum_pi += pi[i];
        if (phi[i] < current_stats.min_phi) current_stats.min_phi = phi[i];
        if (phi[i] > current_stats.max_phi) current_stats.max_phi = phi[i];
    }
    
    current_stats.avg_phi = sum_phi / system.N;
    current_stats.avg_pi = sum_pi / system.N;
    current_stats.std_phi = calculateStdDev(phi, current_stats.avg_phi);
    
    // Классификация состояния
    if (current_stats.avg_phi < -0.05) current_stats.state = 0;
    else if (current_stats.avg_phi < 0.05) current_stats.state = 1;
    else current_stats.state = 2;
    
    // Сохраняем в историю каждые 100 шагов
    if (step % 100 == 0) {
        history.push_back(current_stats);
    }
}

void StatisticsModule::reset() {
    current_stats = Statistics();
    history.clear();
    elapsed_time = 0.0;
}

double StatisticsModule::calculateStdDev(const std::vector<double>& values, double mean) const {
    double sum_sq = 0.0;
    for (double v : values) {
        sum_sq += (v - mean) * (v - mean);
    }
    return std::sqrt(sum_sq / values.size());
}

void StatisticsModule::saveToFile(const std::string& filename) const {
    std::ofstream file(filename);
    file << "step,time,energy,avg_phi,avg_pi,min_phi,max_phi,std_phi,state\n";
    
    for (const auto& stats : history) {
        file << stats.step << ","
             << stats.simulation_time << ","
             << stats.total_energy << ","
             << stats.avg_phi << ","
             << stats.avg_pi << ","
             << stats.min_phi << ","
             << stats.max_phi << ","
             << stats.std_phi << ","
             << stats.state << "\n";
    }
}