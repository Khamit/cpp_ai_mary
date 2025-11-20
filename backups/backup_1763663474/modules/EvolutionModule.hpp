#pragma once
#include "../core/NeuralFieldSystem.hpp"
#include "../core/ImmutableCore.hpp"
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
    double best_fitness;  // Лучшая достигнутая приспособленность
    std::string backup_dir;  // Директория для бэкапов
    
public:
    EvolutionModule(ImmutableCore& core);
    
    // Основные методы
    void evaluateFitness(const NeuralFieldSystem& system, double step_time);
    bool proposeMutation(NeuralFieldSystem& system);
    void enterStasis(NeuralFieldSystem& system);
    bool isInStasis() const;
    void exitStasis();
    
    // Геттеры
    const EvolutionMetrics& getCurrentMetrics() const { return current_metrics; }
    double getOverallFitness() const { return current_metrics.overall_fitness; }
    
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
    void saveEvolutionState();
    bool createBackup();
    bool restoreFromBackup();
    bool checkForDegradation();
    void rollbackToBestVersion();
    
    // Вспомогательные методы
    double getCurrentCodeHash() const;
    bool validateImprovement() const;
};