#pragma once
#include "../core/NeuralFieldSystem.hpp"
#include "../core/ImmutableCore.hpp"
#include "ConfigStructs.hpp"
#include "EvolutionMetrics.hpp"
#include <vector>
#include <chrono>
#include <string>

class EvolutionModule {
private:
    ImmutableCore& immutable_core;
    EvolutionMetrics current_metrics;
    std::vector<EvolutionMetrics> history;
    double total_energy_consumed;
    size_t total_steps;
    bool in_stasis;
    std::chrono::steady_clock::time_point last_evaluation;
    double best_fitness;
    std::string backup_dir;

    // ЗАЩИТА ОТ ЧАСТЫХ ВЫЗОВОВ reduce_complexity
    std::chrono::steady_clock::time_point last_reduction_time;
    std::chrono::seconds REDUCTION_COOLDOWN;
    int reductions_this_minute = 0;
    int MAX_REDUCTIONS_PER_MINUTE;
    
    // НОВАЯ ПЕРЕМЕННАЯ - добавьте эту строку
    double min_fitness_for_optimization;
    
    bool canReduceComplexity();
    void recordReduction();

public:
    void testEvolutionMethods();
    EvolutionModule(ImmutableCore& core);
    EvolutionModule(ImmutableCore& core, const EvolutionConfig& config); // Новый конструктор
    
    // Основные методы
    void evaluateFitness(const NeuralFieldSystem& system, double step_time);
    bool proposeMutation(NeuralFieldSystem& system);
    void enterStasis(NeuralFieldSystem& system);
    bool isInStasis() const;
    void exitStasis();
    
    // Геттеры
    const EvolutionMetrics& getCurrentMetrics() const { return current_metrics; }
    double getOverallFitness() const { return current_metrics.overall_fitness; }
    int getReductionCooldown() const { 
        return REDUCTION_COOLDOWN.count(); 
    }
    int getMaxReductionsPerMinute() const { return MAX_REDUCTIONS_PER_MINUTE; }
    double getMinFitnessForOptimization() const { return min_fitness_for_optimization; }
    double getBestFitness() const { return best_fitness; }
    void saveEvolutionState();
    
private:
    // Методы оценки приспособленности
    double calculateCodeSizeScore() const;
    double calculatePerformanceScore(double step_time) const;
    double calculateEnergyScore(const NeuralFieldSystem& system) const;
    
    // Методы эволюции кода
    void evolveCodeOptimization();
    void optimizeConfigFiles();
    void createOptimalConfig();
    void generateOptimizedCode();
    void generateOptimizedDynamics();
    void generateOptimizedLearning();
    void analyzeCodeEfficiency();
    void optimizeSystemParameters();
    void applyMinimalMutation(NeuralFieldSystem& system);
    
    // Методы защиты и бэкапов
    
    bool createBackup();
    bool restoreFromBackup();
    bool checkForDegradation();
    void rollbackToBestVersion();
    
    // Вспомогательные методы
    double getCurrentCodeHash() const;
    bool validateImprovement() const;
};