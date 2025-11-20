#include "ResourceMonitor.hpp"
#include <iostream>
#include <thread>

ResourceMonitor::ResourceMonitor() 
    : last_check(std::chrono::steady_clock::now()) {
    std::cout << "ResourceMonitor initialized" << std::endl;
}

void ResourceMonitor::update() {
    auto current_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        current_time - last_check);
    
    if (elapsed.count() > 1000) { // Обновляем каждую секунду
        cpu_usage = measureCPUUsage();
        memory_usage = measureMemoryUsage();
        last_check = current_time;
        
        // Вывод каждые 10 секунд
        if (step_counter++ % 10 == 0) {
            std::cout << "Resource Monitor - CPU: " << (cpu_usage * 100) 
                      << "%, Memory: " << (memory_usage * 100) << "%" << std::endl;
        }
    }
}

double ResourceMonitor::measureCPUUsage() {
    // Упрощенное измерение нагрузки на CPU
    // В реальной системе здесь были бы системные вызовы
    static double simulated_load = 0.3;
    
    // Имитируем колебания нагрузки
    simulated_load += 0.01;
    if (simulated_load > 0.8) simulated_load = 0.3;
    
    return simulated_load;
}

double ResourceMonitor::measureMemoryUsage() {
    // Упрощенное измерение использования памяти
    static double simulated_memory = 0.4;
    
    // Имитируем колебания памяти
    simulated_memory += 0.005;
    if (simulated_memory > 0.7) simulated_memory = 0.3;
    
    return simulated_memory;
}

void ResourceMonitor::adjustPerformance() {
    // Регулируем производительность в зависимости от нагрузки
    if (cpu_usage > 0.8) {
        performance_factor = 0.5; // Сильное снижение
    } else if (cpu_usage > 0.6) {
        performance_factor = 0.7; // Умеренное снижение
    } else {
        performance_factor = 1.0; // Полная производительность
    }
}