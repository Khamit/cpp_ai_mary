#include "StatisticsModule.hpp"
#include "../core/NeuralFieldSystem.hpp" 

#include <fstream>
#include <cmath>

StatisticsModule::StatisticsModule()
    : timer_running(false),
      stats_collector(nullptr)
{
    reset();
}

void StatisticsModule::setStatsCollector(UnifiedStatsCollector* collector)
{
    stats_collector = collector;
}

void StatisticsModule::startTimer()
{
    start_time = std::chrono::steady_clock::now();
    last_step_time = start_time;
    timer_running = true;
}

void StatisticsModule::stopTimer()
{
    timer_running = false;
}

void StatisticsModule::reset()
{
    history.clear();
    timer_running = false;
}

void StatisticsModule::update(const NeuralFieldSystem& system, int step, double dt)
{
    if (!timer_running)
        return;

    auto now = std::chrono::steady_clock::now();

    double real_dt =
        std::chrono::duration<double>(now - last_step_time).count();

    last_step_time = now;

    SystemSnapshot snap;

    snap.step = step;
    snap.dt = real_dt;

    snap.total_energy = system.computeTotalEnergy();

    const auto& phi = system.getPhi();
    double sum_phi = 0.0;
    for (double v : phi)
        sum_phi += v;

    snap.avg_phi = phi.empty() ? 0.0 : sum_phi / phi.size();

    const auto& pi = system.getPi();
    double sum_pi = 0.0;
    for (double v : pi)
        sum_pi += std::abs(v);

    snap.avg_pi = pi.empty() ? 0.0 : sum_pi / pi.size();

    snap.system_entropy = system.computeSystemEntropy();

    snap.fitness = 0.0;
    snap.memory_records = 0;

    snap.timestamp =
        std::chrono::duration<double>(now - start_time).count();

    history.push_back(snap);

    if (history.size() > 10000)
    {
        history.erase(history.begin(), history.begin() + 1000);
    }
}

void StatisticsModule::updateFromCollector()
{
    if (!stats_collector || history.empty())
        return;

    auto& last = history.back();

    const auto& all_stats = stats_collector->getAllStats();

    auto evo_it = all_stats.find("evolution");
    if (evo_it != all_stats.end())
    {
        auto fit_it = evo_it->second.numeric_stats.find("fitness");
        if (fit_it != evo_it->second.numeric_stats.end())
        {
            last.fitness = fit_it->second;
        }
    }

    auto mem_it = all_stats.find("memory");
    if (mem_it != all_stats.end())
    {
        auto rec_it = mem_it->second.numeric_stats.find("long_term");
        if (rec_it != mem_it->second.numeric_stats.end())
        {
            last.memory_records = static_cast<size_t>(rec_it->second);
        }
    }
}

const SystemSnapshot& StatisticsModule::getCurrentStats() const
{
    static SystemSnapshot empty{};
    return history.empty() ? empty : history.back();
}

const std::vector<SystemSnapshot>& StatisticsModule::getHistory() const
{
    return history;
}

void StatisticsModule::saveToFile(const std::string& filename) const
{
    std::ofstream file(filename);

    if (!file.is_open())
        return;

    file << "step,time,energy,avg_phi,avg_pi,entropy,fitness,memory\n";

    for (const auto& snap : history)
    {
        file << snap.step << ","
             << snap.timestamp << ","
             << snap.total_energy << ","
             << snap.avg_phi << ","
             << snap.avg_pi << ","
             << snap.system_entropy << ","
             << snap.fitness << ","
             << snap.memory_records << "\n";
    }
}