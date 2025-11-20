#include "ResourceMonitor.hpp"
#include <iostream>
#include <thread>

ResourceMonitor::ResourceMonitor() 
    : DEBOUNCE_TIME(std::chrono::seconds(5)),
      MAX_OVERLOADS_PER_MINUTE(2),
      last_check(std::chrono::steady_clock::now()),
      last_overload_trigger(std::chrono::steady_clock::now() - DEBOUNCE_TIME) {
    std::cout << "✅ ResourceMonitor initialized with default thresholds" << std::endl;
}

ResourceMonitor::ResourceMonitor(const ResourceMonitorConfig& config)
    : cpu_threshold(config.cpu_threshold),
      memory_threshold(config.memory_threshold),
      DEBOUNCE_TIME(std::chrono::seconds(config.overload_debounce_seconds)),
      MAX_OVERLOADS_PER_MINUTE(config.max_overloads_per_minute),
      last_check(std::chrono::steady_clock::now()),
      last_overload_trigger(std::chrono::steady_clock::now() - DEBOUNCE_TIME) {
    std::cout << "✅ ResourceMonitor initialized with config" << std::endl;
}

void ResourceMonitor::update() {
    auto current_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        current_time - last_check);
    
    if (elapsed.count() > 1000) {
        cpu_usage = measureCPUUsage() * 100;
        memory_usage = measureMemoryUsage() * 100;
        last_check = current_time;
        
        updateAdaptiveThresholds();
        
        if (step_counter++ % 30 == 0) {
            std::cout << "📊 Resource Monitor - CPU: " << cpu_usage 
                      << "%, Memory: " << memory_usage 
                      << "%, Thresholds: " << cpu_threshold << "/" << memory_threshold << "%" << std::endl;
        }
    }
}

void ResourceMonitor::updateAdaptiveThresholds() {
    double cpu_avg = cpu_usage;
    cpu_threshold = 0.7 * cpu_threshold + 0.3 * (cpu_avg + 20.0);
    cpu_threshold = std::max(60.0, std::min(95.0, cpu_threshold));
    
    memory_threshold = 0.7 * memory_threshold + 0.3 * (memory_usage + 25.0);
    memory_threshold = std::max(70.0, std::min(98.0, memory_threshold));
}

double ResourceMonitor::measureCPUUsage() {
    static double simulated_load = 0.3;
    simulated_load += 0.01;
    if (simulated_load > 0.8) simulated_load = 0.3;
    return simulated_load;
}

double ResourceMonitor::measureMemoryUsage() {
    static double simulated_memory = 0.4;
    simulated_memory += 0.005;
    if (simulated_memory > 0.7) simulated_memory = 0.3;
    return simulated_memory;
}

void ResourceMonitor::adjustPerformance() {
    if (cpu_usage > cpu_threshold * 1.1) {
        performance_factor = 0.7;
    } else if (cpu_usage > cpu_threshold) {
        performance_factor = 0.9;
    } else {
        performance_factor = 1.0;
    }
}
