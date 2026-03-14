#pragma once
#include "NeuralGroup.hpp"
#include <vector>
#include <random>
#include <string>
#include <deque>
#include <algorithm>
#include <cmath>
#include "EventSystem.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "modules/PredictorGroup.hpp"

/**
 * @class AttentionMechanism
 * @brief Механизм внимания с энтропийной регуляризацией и геометрической нормализацией
 * 
 * Логика: 
 * 1. Веса внимания нормализуются как распределение на гиперсфере (с использованием π)
 * 2. Энтропия распределения используется как мера неопределенности
 * 3. Температура регулирует компромисс между исследованием и эксплуатацией
 */
struct AttentionMechanism {
    std::vector<double> attention_weights;  // веса внимания для групп
    std::vector<float> context_vector;      // контекстный вектор
    float temperature = 1.0f;                // температура для softmax
    double entropy = 0.0;                    // текущая энтропия распределения
    
    /**
     * Вычисление весов внимания с softmax и расчет энтропии
     * @param groupActivities - вектор активностей групп
     */
    void computeAttention(const std::vector<double>& groupActivities) {
        const size_t n = groupActivities.size();
        attention_weights.resize(n);

        if (n == 0) return;

        float maxVal = static_cast<float>(*std::max_element(
            groupActivities.begin(), groupActivities.end()));

        const float temp = (temperature <= 1e-6f) ? 1e-6f : temperature;

        float sum = 0.0f;
        for (size_t i = 0; i < n; i++) {
            float v = std::exp((static_cast<float>(groupActivities[i]) - maxVal) / temp);
            attention_weights[i] = v;
            sum += v;
        }

        if (sum <= 1e-12f) {
            float uniform = 1.0f / static_cast<float>(n);
            for (auto& w : attention_weights) w = uniform;
            entropy = std::log(static_cast<double>(n)); // максимальная энтропия для равномерного
            return;
        }

        float invSum = 1.0f / sum;
        entropy = 0.0;
        for (auto& w : attention_weights) {
            w *= invSum;
            if (w > 1e-12) {
                entropy -= w * std::log(static_cast<double>(w));
            }
        }
    }
    
    /**
     * Вычисление геометрической нормализации на гиперсфере (с использованием π)
     * @param groupActivities - вектор активностей групп
     * @return нормированный вектор на сфере
     */
    std::vector<double> computeSphericalAttention(const std::vector<double>& groupActivities) {
        const size_t n = groupActivities.size();
        std::vector<double> result(n, 0.0);
        
        // Нормализация вектора как точки на сфере
        double norm = 0.0;
        for (double a : groupActivities) norm += a * a;
        if (norm < 1e-12) return result;
        
        norm = std::sqrt(norm);
        
        // Площадь поверхности единичной сферы в n-мерном пространстве
        // S(n) = 2 * π^(n/2) / Γ(n/2)
        double sphere_area = 2.0 * std::pow(M_PI, n / 2.0) / std::tgamma(n / 2.0 + 1);
        
        for (size_t i = 0; i < n; i++) {
            result[i] = (groupActivities[i] / norm) / sphere_area;
        }
        
        return result;
    }
};

/**
 * @class NeuralFieldSystem
 * @brief Контейнер групп нейронов с межгрупповыми связями и механизмом повторного входа
 * 
 * Архитектура:
 * - 32 группы по 32 нейрона (1024 нейрона)
 * - Трехуровневая система обучения (быстрое/медленное/эволюция)
 * - Энтропийная регуляризация для предотвращения коллапса режимов
 * - Геометрические ограничения связей через π-функции
 */
class NeuralFieldSystem {
public:
    static constexpr int NUM_GROUPS = 32;       ///< 32 группы
    static constexpr int GROUP_SIZE = 32;       ///< 32 нейрона в группе
    static constexpr int TOTAL_NEURONS = NUM_GROUPS * GROUP_SIZE; // 1024
    static constexpr int SYSTEM_ENTROPY_BINS = 20;  // можно оставить 20 для системы

    /**
     * @param dt Глобальный шаг интегрирования
     */
    NeuralFieldSystem(double dt, EventSystem& events);
   
    // Запрет копирования
    NeuralFieldSystem(const NeuralFieldSystem&) = delete;
    NeuralFieldSystem& operator=(const NeuralFieldSystem&) = delete;

    /** Инициализация случайными значениями */
    void initializeRandom(std::mt19937& rng);

    /**
     * Основной шаг симуляции с трехуровневой архитектурой
     * @param globalReward - глобальный сигнал награды
     * @param stepNumber - номер текущего шага
     */
    void step(float globalReward, int stepNumber);

    /** Вычислить общую энергию системы */
    double computeTotalEnergy() const;
    
    /** Вычислить энтропию всей системы */
    double computeSystemEntropy() const;

    /** Возвращает вектор активностей всех нейронов */
    const std::vector<double>& getPhi() const { return flatPhi; }

    /** Возвращает вектор импульсов всех нейронов */
    const std::vector<double>& getPi() const { return flatPi; }

    /**
     * Для совместимости возвращает пустую матрицу
     */
    const std::vector<std::vector<double>> getWeights() const { return {}; }

    /** Получить средние активности групп */
    std::vector<double> getGroupAverages() const;

    /** Получить матрицу межгрупповых связей */
    const std::vector<std::vector<double>>& getInterWeights() const { return interWeights; }

    /** Укрепить связь от группы from к группе to */
    void strengthenInterConnection(int from, int to, double delta);

    /** Ослабить связь */
    void weakenInterConnection(int from, int to, double delta);

    /** Получить вектор признаков для памяти (64 значения) */
    std::vector<float> getFeatures() const;

    // Структура состояния рефлексии
    struct ReflectionState {
        double confidence, curiosity, satisfaction, confusion;
        std::vector<double> attention_map;
    };
    
    ReflectionState getReflectionState() const;
    void reflect();
    void setGoal(const std::string& goal);
    bool evaluateProgress();
    void learnFromReflection(float outcome);

    // Методы для эволюции
    void applyTargetedMutation(double strength, int targetType);
    void enterLowPowerMode();

    // Доступ к группам
    std::vector<NeuralGroup>& getGroups() { return groups; }
    const std::vector<NeuralGroup>& getGroups() const { return groups; }
    //-----------------------------------------

    // НОВЫЕ МЕТОДЫ
    void initializePredictor(size_t input_dim, size_t latent_dim, std::mt19937& rng);
    
    // Предсказать следующее состояние системы
    std::vector<double> predictNextState(const std::vector<double>& current_features);
    
    // Обновить предсказатель на основе реального следующего состояния
    double updatePredictor(const std::vector<double>& current_features,
                          const std::vector<double>& next_features,
                          int step_number);
    
    // Проверить, является ли текущее состояние аномальным
    bool detectAnomaly(const std::vector<double>& features);
    
    // Получить сжатое представление текущего состояния
    std::vector<double> getCompressedState(const std::vector<double>& features);
    
    // Получить энтропию ошибок предсказания
    double getPredictionEntropy() const;

private:
    AttentionMechanism attention;
    
    EventSystem& events;
    
    double dt;
    std::vector<NeuralGroup> groups;                 // 32 группы
    std::vector<std::vector<double>> interWeights;   // межгрупповые связи 32x32

    // Плоские векторы для обратной совместимости
    mutable std::vector<double> flatPhi;
    mutable std::vector<double> flatPi;
    mutable bool flatDirty;

    void rebuildFlatVectors() const;

    // Механизм повторного входа
    void applyReentry(int iterations = 3);

    // Вспомогательные методы
    std::deque<std::vector<double>> state_history;
    std::deque<double> energy_history;
    std::deque<double> entropy_history;              // история энтропии для анализа
    static constexpr int HISTORY_SIZE = 100;
    std::string current_goal;
    std::vector<double> goal_embedding;

    // Методы с энтропийной регуляризацией
    float computeGlobalImportance();                  // на основе изменения энтропии
    void applyPruningByElevation();                   
    void consolidateInterWeights();
    
    bool pendingEvolutionRequest_ = false;

    // НОВОЕ: группа предсказательного кодирования
    std::unique_ptr<PredictorGroup> predictor;

    // Вспомогательный метод для получения признаков в double
    std::vector<double> NeuralFieldSystem::getFeaturesDouble() const {
        auto float_features = getFeatures();
        return std::vector<double>(float_features.begin(), float_features.end());
    }
};