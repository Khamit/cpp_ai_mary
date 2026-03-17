// modules/EvolutionModule.hpp
#pragma once

#include "hallucination/HNeuronDetector.hpp"
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
#include <memory>
#include <iostream>

// Forward declaration
class LanguageModule;
class MemoryManager; 

struct FactCheckResult {
    bool isConsistent;
    float confidence;
    std::vector<std::string> contradictions;
    std::vector<std::string> supportingEvidence;
};

class EvolutionModule : public Component {
private:
    ImmutableCore& immutable_core;
    MemoryManager& memoryManager;
    EvolutionMetrics current_metrics;
    HNeuronDetector detector_;
    std::vector<EvolutionMetrics> history;
    
    double total_energy_consumed;
    size_t total_steps;
    bool in_stasis;
    std::chrono::steady_clock::time_point last_evaluation;
    double best_fitness;
    std::string backup_dir;

    // УДАЛЯЕМ ЭТУ СТРОКУ - она здесь не нужна
    // std::vector<NeuralGroup>& getGroups() { return groups; }

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

    mutable std::mt19937 rng_{std::random_device{}()};

public:
    void connectToSystem(NeuralFieldSystem& system) {
        detector_.setSystem(&system);
    }
    
    FactCheckResult checkFactualConsistency(const std::string& statement);

    // Конструктор
    EvolutionModule(ImmutableCore& core, const EvolutionConfig& config, MemoryManager& memory);

    HNeuronDetector& getDetector() { return detector_; }
    
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
    }
    
    void saveState(MemoryManager& memory) override {
        saveEvolutionState();
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
        return std::uniform_real_distribution<>(0, 1)(rng_) < mutation_rate;
    }
    
    float randomMutation() {
        std::normal_distribution<> dist(0.0, 0.1);
        return dist(rng_);
    }
    
private: 
    std::vector<std::vector<float>> projectionMatrix;
    std::vector<std::string> knownFacts_;
    std::map<std::string, float> factConfidence_;
    
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
    
    // НОВЫЕ ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
    std::vector<std::string> splitIntoSentences(const std::string& text);
    std::vector<std::string> extractPotentialFacts(const std::string& sentence);
    bool areFactsConsistent(const std::string& fact1, const std::string& fact2);
};