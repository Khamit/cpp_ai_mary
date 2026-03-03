#include "StatisticsModule.hpp"
#include <fstream>

void StatisticsModule::startTimer() {
    timer_running = true;
}

void StatisticsModule::stopTimer() {
    timer_running = false;
}

void StatisticsModule::reset() {
    current_stats = Statistics();
    history.clear();
    elapsed_time = 0.0;
    timer_running = false;
}

void StatisticsModule::update(const NeuralFieldSystem& system,
                              int step,
                              double dt)
{
    if (timer_running) {
        elapsed_time += dt;
    }

    current_stats.step = step;
    current_stats.dt = dt;
    current_stats.simulation_time = elapsed_time;

    const auto& groups = system.getGroups();

    double sumPhi = 0.0;

    for (const auto& group : groups) {
        sumPhi += group.getAverageActivity();
    }

    if (!groups.empty())
        current_stats.avg_phi = sumPhi / groups.size();
    else
        current_stats.avg_phi = 0.0;

    current_stats.avg_pi = 0.0;
    current_stats.total_energy = system.computeTotalEnergy();

    history.push_back(current_stats);

    if (history.size() > historySize) {
        history.erase(history.begin());
    }
}

void StatisticsModule::saveToFile(const std::string& filename) const
{
    std::ofstream file(filename);
    if (!file.is_open()) return;

    file << "step,time,energy,avg_phi,avg_pi,dt\n";

    for (const auto& stat : history) {
        file << stat.step << ","
             << stat.simulation_time << ","
             << stat.total_energy << ","
             << stat.avg_phi << ","
             << stat.avg_pi << ","
             << stat.dt << "\n";
    }
}