#pragma once

struct EvolutionMetrics {
    double code_size_score;    // Меньше код = лучше (1.0 = идеал)
    double performance_score;  // Быстрее выполнение = лучше (1.0 = идеал)  
    double energy_score;       // Меньше энергозатраты = лучше (1.0 = идеал)
    double overall_fitness;    // Общая приспособленность
    
    EvolutionMetrics();
};