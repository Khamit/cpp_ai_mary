// modules/ResourceMonitor.hpp
#pragma once
#include <chrono>

class ResourceMonitor {
private:
    std::chrono::steady_clock::time_point last_check;
    double cpu_usage = 0.0;
    double memory_usage = 0.0;
    double performance_factor = 1.0;
    int step_counter = 0;
    
public:
    ResourceMonitor();
    void update();
    double getCurrentLoad() const { return cpu_usage; }
    bool isOverloaded() const { return cpu_usage > 0.7; } // 70% нагрузка
    void adjustPerformance();
    double getPerformanceFactor() const { return performance_factor; }
    
private:
    double measureCPUUsage();
    double measureMemoryUsage();
};