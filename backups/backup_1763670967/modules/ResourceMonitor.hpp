// modules/ResourceMonitor.hpp
#pragma once
#include <chrono>
#include "ConfigStructs.hpp" 

class ResourceMonitor {
private:
    std::chrono::steady_clock::time_point last_check;
    std::chrono::steady_clock::time_point last_overload_trigger;
    double cpu_usage = 0.0;
    double memory_usage = 0.0;
    double performance_factor = 1.0;
    int step_counter = 0;
    
    // АДАПТИВНЫЕ ПОРОГИ
    double cpu_threshold = 85.0;
    double memory_threshold = 90.0;
    
    // ДЕБАУНС
    std::chrono::seconds DEBOUNCE_TIME;
    int overload_count = 0;
    int MAX_OVERLOADS_PER_MINUTE;
    
public:
    ResourceMonitor();
    ResourceMonitor(const ResourceMonitorConfig& config);
    
    void update();
    double getCurrentLoad() const { return cpu_usage; }

    bool isOverloaded() const { 
        return (cpu_usage > cpu_threshold) || (memory_usage > memory_threshold);
    }
    bool checkAndTriggerOverload() {
    auto current_time = std::chrono::steady_clock::now();
    auto time_since_last_trigger = current_time - last_overload_trigger;
    
    if (time_since_last_trigger < DEBOUNCE_TIME) {
        return false;
    }
    
    if (overload_count >= MAX_OVERLOADS_PER_MINUTE) {
        auto time_since_reset = current_time - last_overload_trigger;
        if (time_since_reset < std::chrono::minutes(1)) {
            return false;
        } else {
            overload_count = 0;
        }
    }
    
    bool overload = isOverloaded();
    if (overload) {
        last_overload_trigger = current_time;
        overload_count++;
    }
    
    return overload;
}
    void adjustPerformance();
    double getPerformanceFactor() const { return performance_factor; }
    double getMemoryUsage() const { return memory_usage; }
    double getCPUThreshold() const { return cpu_threshold; }
    
private:
    double measureCPUUsage();
    double measureMemoryUsage();
    void updateAdaptiveThresholds();
};