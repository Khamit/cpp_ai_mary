// modules/ConfigStructs.hpp
#pragma once

struct ResourceMonitorConfig {
    double cpu_threshold = 85.0;
    double memory_threshold = 90.0;
    int overload_debounce_seconds = 5;
    int max_overloads_per_minute = 2;
    bool adaptive_thresholds = true;
};

struct EvolutionConfig {
    int reduction_cooldown_seconds = 30;
    int max_reductions_per_minute = 2;
    double min_fitness_for_optimization = 0.8;
    int evolution_interval_steps = 1000;
    bool backup_on_improvement = true;
    bool enable_adaptive_mutations = true;
};