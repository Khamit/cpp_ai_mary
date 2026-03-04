// EvolutionModule.hpp (обновленный)
#pragma once

#include "../core/NeuralFieldSystem.hpp"
#include "../core/ImmutableCore.hpp"
#include "lang/LanguageModule.hpp"  // или просто forward declaration
#include "ConfigStructs.hpp"
#include "EvolutionMetrics.hpp"
#include <vector>
#include <chrono>
#include <string>
#include <random>

// Forward declaration
class LanguageModule;

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

    // Защита от частых мутаций
    std::chrono::seconds REDUCTION_COOLDOWN;
    int reductions_this_minute = 0;
    int MAX_REDUCTIONS_PER_MINUTE;
    std::chrono::steady_clock::time_point last_reduction_time;
    
    // Параметры эволюции
    double min_fitness_for_optimization;
    double mutation_rate;  // вероятность мутации
    
    bool canReduceComplexity();
    void recordReduction();

public:
    EvolutionModule(ImmutableCore& core);
    EvolutionModule(ImmutableCore& core, const EvolutionConfig& config);
    
    void testEvolutionMethods();
    
    // Основные методы
    void evaluateFitness(const NeuralFieldSystem& system, double step_time, LanguageModule& lang);
    bool proposeMutation(NeuralFieldSystem& system);
    void enterStasis(NeuralFieldSystem& system);
    bool isInStasis() const;
    void exitStasis();
    
    // Геттеры
    const EvolutionMetrics& getCurrentMetrics() const { return current_metrics; }
    double getOverallFitness() const { return current_metrics.overall_fitness; }
    double getBestFitness() const { return best_fitness; }
    void saveEvolutionState();
    
private:
    // Оценка приспособленности
    double calculateCodeSizeScore() const;
    double calculatePerformanceScore(double step_time) const;
    double calculateEnergyScore(const NeuralFieldSystem& system) const;
    
    // Параметрическая эволюция (НОВЫЙ МЕТОД)
    void mutateParameters(NeuralFieldSystem& system);
    
    // Методы защиты и бэкапов
    bool createBackup();
    bool restoreFromBackup();
    bool checkForDegradation();
    void rollbackToBestVersion();
    double getCurrentCodeHash() const;
    bool validateImprovement() const;
    
    // Минимальная мутация для стазиса
    void applyMinimalMutation(NeuralFieldSystem& system);
    
    // Устаревшие методы - оставлены для совместимости
    void optimizeSystemParameters();
    // Все методы генерации кода удалены
};