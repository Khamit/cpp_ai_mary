// modules/EvolutionModule.hpp
#pragma once

#include "../core/NeuralFieldSystem.hpp"
#include "../core/ImmutableCore.hpp"
#include "../core/Component.hpp"
#include "lang/LanguageModule.hpp"
#include "ConfigStructs.hpp"
#include "EvolutionMetrics.hpp"
#include <vector>
#include <chrono>
#include <string>
#include <random>

// Forward declaration
class LanguageModule;
class MemoryManager; 

class EvolutionModule : public Component {
private:
    ImmutableCore& immutable_core;
    MemoryManager& memoryManager;
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

    mutable std::mt19937 rng_{std::random_device{}()}; // если нет

public:
    // Конструктор
    EvolutionModule(ImmutableCore& core, const EvolutionConfig& config, MemoryManager& memory);
    
    // == РЕАЛИЗАЦИЯ МЕТОДОВ Component
    std::string getName() const override { return "EvolutionModule"; }

    std::vector<float> projectEmbeddingToGroups(const std::vector<float>& emb);
    
    bool initialize(const Config& config) override {
        std::cout << "EvolutionModule initialized from config" << std::endl;
        return true;
    }
    
    void shutdown() override {
        std::cout << "EvolutionModule shutting down" << std::endl;
        saveEvolutionState();
    }
    
    void update(float dt) override {
        // Эволюция не требует постоянного обновления на каждом шаге
        // Вызывается по таймеру через proposeMutation
    }
    
    void saveState(MemoryManager& memory) override {
        saveEvolutionState();  // используем существующий метод
    }
    
    void loadState(MemoryManager& memory) override {
        // Загрузка состояния - можно реализовать позже
    }
    // == КОНЕЦ МЕТОДОВ Component
    
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

        // НОВЫЕ МЕТОДЫ ДЛЯ ТРЕХУРОВНЕВОЙ АРХИТЕКТУРЫ
    bool shouldMutate() const {
        // Проверка, нужно ли мутировать на этом шаге
        return std::uniform_real_distribution<>(0, 1)(rng_) < mutation_rate;
    }
    
    float randomMutation() {
        std::normal_distribution<> dist(0.0, 0.1);
        return dist(rng_);
    }
    
    void setBaseElevation(float elev);  // если нужно
    float getBaseElevation() const;
    
private: 
    // == ЗАКОММЕНТИРОВАНЫ ЛИШНИЕ МЕТОДЫ
    /*
    // Оценка приспособленности
    double calculateCodeSizeScore() const;
    double calculatePerformanceScore(double step_time) const;
    double calculateEnergyScore(const NeuralFieldSystem& system) const;
    */

    std::vector<std::vector<float>> projectionMatrix;
    
    // Параметрическая эволюция
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
    
    // == УСТАРЕВШИЕ МЕТОДЫ - ЗАКОММЕНТИРОВАНЫ
    /*
    void optimizeSystemParameters();
    // Все методы генерации кода удалены
    */
};