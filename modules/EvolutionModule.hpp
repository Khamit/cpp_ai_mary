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
#include <deque>

// Forward declaration
class LanguageModule;
class MemoryManager;

struct FactCheckResult {
    bool isConsistent;
    float confidence;
    std::vector<std::string> contradictions;
    std::vector<std::string> supportingEvidence;
};

/**
 * @struct MutationContext
 * @brief Контекст для принятия решения о мутации
 */
struct MutationContext {
    double current_fitness;
    double best_fitness;
    int elite_neurons;
    int dead_neurons;
    double avg_connectivity;
    double system_entropy;
    int total_steps;
    bool is_stagnating;      // застой в обучении
    double lang_fitness;
    double orbit_health;
};

/**
 * @enum MutationType
 * @brief Типы мутаций для разных ситуаций
 */
enum class MutationType {
    REANIMATION,        // реанимация мёртвых нейронов
    ELITE_BOOST,        // усиление элитных нейронов
    CONNECTION_STRENGTHEN, // укрепление слабых связей
    CONNECTION_PRUNE,   // обрезка слишком сильных связей
    PARAMETER_TUNE,     // тонкая настройка параметров
    EXPLORATION,        // добавление шума для исследования
    STABILIZATION,      // стабилизация системы
    DIVERSITY,          // увеличение разнообразия
    HEALING             // лечение застрявших нейронов
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

    // Защита от частых мутаций
    std::chrono::seconds REDUCTION_COOLDOWN;
    int reductions_this_minute = 0;
    int MAX_REDUCTIONS_PER_MINUTE;
    std::chrono::steady_clock::time_point last_reduction_time;
    
    // Параметры эволюции
    double min_fitness_for_optimization;
    double mutation_rate;           // базовая вероятность мутации
    double adaptive_mutation_rate;  // адаптивная скорость мутации
    
    // НОВЫЕ ПАРАМЕТРЫ
    double stagnation_threshold = 0.02;      // порог застоя (изменение fitness < 2%)
    double elite_protection_factor = 0.7;    // защита элитных нейронов (70% не трогать)
    double mutation_strength = 0.05;         // сила мутации
    double mutation_decay = 0.99;            // затухание силы мутации со временем
    
    // Счётчики для отслеживания застоя
    int stagnation_counter = 0;
    double last_fitness_value = 0.0;
    std::deque<double> fitness_history;      // история fitness для анализа тренда
    
    bool canReduceComplexity();
    void recordReduction();

    mutable std::mt19937 rng_{std::random_device{}()};

public:
    void connectToSystem(NeuralFieldSystem& system) {
        detector_.setSystem(&system);
    }

    // Конструктор
    EvolutionModule(ImmutableCore& core, const EvolutionConfig& config, MemoryManager& memory);

    HNeuronDetector& getDetector() { return detector_; }
    
    // Удаляем getMasteryEvaluator - больше не нужен
    
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
    
    // НОВЫЕ МЕТОДЫ ДЛЯ АДАПТИВНОЙ ЭВОЛЮЦИИ
    bool shouldMutate() const {
        return std::uniform_real_distribution<>(0, 1)(rng_) < adaptive_mutation_rate;
    }
    
    float randomMutation() {
        std::normal_distribution<> dist(0.0, mutation_strength);
        return dist(rng_);
    }
    
    // НОВЫЕ МЕТОДЫ ДЛЯ АНАЛИЗА СОСТОЯНИЯ
    MutationContext analyzeSystemState(const NeuralFieldSystem& system, double current_fitness);
    MutationType selectMutationType(const MutationContext& ctx);
    
    // НОВЫЕ МЕТОДЫ МУТАЦИЙ
    void applyReanimationMutation(NeuralFieldSystem& system);
    void applyEliteBoostMutation(NeuralFieldSystem& system);
    void applyConnectionStrengthenMutation(NeuralFieldSystem& system);
    void applyConnectionPruneMutation(NeuralFieldSystem& system);
    void applyParameterTuneMutation(NeuralFieldSystem& system);
    void applyExplorationMutation(NeuralFieldSystem& system);
    void applyStabilizationMutation(NeuralFieldSystem& system);
    void applyDiversityMutation(NeuralFieldSystem& system);
    void applyHealingMutation(NeuralFieldSystem& system);
    
    // Адаптивные методы
    void updateAdaptiveParams(double current_fitness);
    void checkStagnation(double current_fitness);
    
    // Удаляем setMasteryEvaluator - больше не нужен

private: 
    std::vector<std::vector<float>> projectionMatrix;
    
    // Параметрическая эволюция (устаревший метод, заменяется на умные)
    void mutateParameters(NeuralFieldSystem& system);
    
    // Умная параметрическая мутация
    void mutateParametersSmart(NeuralFieldSystem& system);
    
    // Методы защиты и бэкапов
    bool createBackup();
    bool restoreFromBackup();
    bool checkForDegradation();
    void rollbackToBestVersion();
    double getCurrentCodeHash() const;
    bool validateImprovement() const;
    
    // Минимальная мутация для стазиса
    void applyMinimalMutation(NeuralFieldSystem& system);
    
    
    // Защита элитных нейронов
    bool isEliteNeuron(const NeuralGroup& group, int neuron_idx) const;
    void protectEliteConnections(NeuralGroup& group, int neuron_idx);
    
    // НОРМАЛИЗОВАННЫЕ ПАРАМЕТРЫ
    struct {
        double min_lr = 0.0005;
        double max_lr = 0.05;
        double min_threshold = 0.1;
        double max_threshold = 0.9;
        double min_mass = 0.5;
        double max_mass = 5.0;
        double min_connectivity = 0.01;
        double max_connectivity = 1.0;
    } bounds;
    
    // Статистика мутаций для анализа эффективности
    struct MutationStats {
        int total_mutations = 0;
        int successful_mutations = 0;
        std::map<MutationType, int> type_counts;
        std::map<MutationType, int> type_successes;
        
        void record(MutationType type, bool success) {
            total_mutations++;
            type_counts[type]++;
            if (success) {
                successful_mutations++;
                type_successes[type]++;
            }
        }
        
        double getSuccessRate() const {
            return total_mutations > 0 ? 
                   static_cast<double>(successful_mutations) / total_mutations : 0.0;
        }
        
        double getTypeSuccessRate(MutationType type) const {
            auto it = type_counts.find(type);
            if (it == type_counts.end() || it->second == 0) return 0.0;
            auto sit = type_successes.find(type);
            int successes = (sit != type_successes.end()) ? sit->second : 0;
            return static_cast<double>(successes) / it->second;
        }
    } mutation_stats;
};