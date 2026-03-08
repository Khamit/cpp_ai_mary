#pragma once
#include "NeuralGroup.hpp"
#include <vector>
#include <random>
#include <string>
#include <deque>
#include <algorithm>
/**
 * @class NeuralFieldSystem
 * @brief Контейнер групп нейронов с межгрупповыми связями и механизмом повторного входа.
 *
 * Поддерживает 32 группы по 32 нейрона (всего 1024 нейрона).
 * Межгрупповые связи представлены матрицей 32x32.
 */
 struct AttentionMechanism {
    std::vector<double> attention_weights;  // веса внимания для групп
    //TODO: Реализовать - нигде не используется!
    std::vector<float> context_vector;     // контекстный вектор
    // температура задана здесь!
    float temperature = 1.0f;              // температура для softmax
    
    void computeAttention(const std::vector<double>& groupActivities) {
        const size_t n = groupActivities.size();
        attention_weights.resize(n);

        if (n == 0) return;

        float maxVal = static_cast<float>(*std::max_element(
            groupActivities.begin(), groupActivities.end()));

        // Защита от нулевой или отрицательной температуры
        const float temp = (temperature <= 1e-6f) ? 1e-6f : temperature;

        float sum = 0.0f;
        for (size_t i = 0; i < n; i++) {
            float v = std::exp((static_cast<float>(groupActivities[i]) - maxVal) / temp);
            attention_weights[i] = v;
            sum += v;
        }

        // Защита от деления на 0
        if (sum <= 1e-12f) {
            float uniform = 1.0f / static_cast<float>(n);
            for (auto& w : attention_weights) w = uniform;
            return;
        }

        float invSum = 1.0f / sum;
        for (auto& w : attention_weights) w *= invSum;
    }
};
class NeuralFieldSystem {
public:
    static constexpr int NUM_GROUPS = 32;       ///< 32 группы
    static constexpr int GROUP_SIZE = 32;       ///< 32 нейрона в группе
    static constexpr int TOTAL_NEURONS = NUM_GROUPS * GROUP_SIZE; // 1024

    /**
     * @param dt Глобальный шаг интегрирования
     */
    NeuralFieldSystem(double dt);
   // void learn(float globalReward);  // Обучение всех групп
    // Запрет копирования
    NeuralFieldSystem(const NeuralFieldSystem&) = delete;
    NeuralFieldSystem& operator=(const NeuralFieldSystem&) = delete;

    /** Инициализация случайными значениями. */
    void initializeRandom(std::mt19937& rng);

    /**
     * Основной шаг симуляции:
     * 1. Эволюция каждой группы (внутренняя динамика).
     * 2. Межгрупповое взаимодействие (повторный вход).
     * 3. Обучение (внутригрупповое и межгрупповое).
     */
    void step(float globalReward = 0.0f, bool useSTDP = true);  // значение по умолчанию

    /** Вычислить общую энергию системы (для совместимости). */
    double computeTotalEnergy() const;

    // ---------- Интерфейсы для модулей (совместимость) ----------
    /** Возвращает вектор активностей всех нейронов (для визуализации). */
    const std::vector<double>& getPhi() const { return flatPhi; }

    /** Возвращает вектор импульсов всех нейронов. */
    const std::vector<double>& getPi() const { return flatPi; }

    /**
     * Возвращает "веса" - для совместимости возвращает пустую матрицу.
     * В новой архитектуре веса разделены на внутригрупповые и межгрупповые.
     */
    const std::vector<std::vector<double>> getWeights() const { return {}; }

    // ---------- Новые методы для межгруппового взаимодействия ----------
    /** Получить средние активности групп (вектор размера NUM_GROUPS). */
    std::vector<double> getGroupAverages() const;

    /** Получить матрицу межгрупповых связей. */
    const std::vector<std::vector<double>>& getInterWeights() const { return interWeights; }

    /** Укрепить связь от группы from к группе to на величину delta. */
    void strengthenInterConnection(int from, int to, double delta);

    /** Ослабить связь. */
    void weakenInterConnection(int from, int to, double delta);

    /** Получить вектор признаков для памяти (64 значения). */
    std::vector<float> getFeatures() const;

    // ---------- Методы для рефлексии и целей (оставлены для совместимости) ----------
    struct ReflectionState {
        double confidence, curiosity, satisfaction, confusion;
        std::vector<double> attention_map;
    };
    ReflectionState getReflectionState() const;
    void reflect();
    void setGoal(const std::string& goal);
    bool evaluateProgress();
    void learnFromReflection(float outcome);

    // ---------- Методы для эволюции (групповой уровень) ----------
    void applyTargetedMutation(double strength, int targetType);
    void enterLowPowerMode();

    // Доступ к группам (для эволюции и др.)
    std::vector<NeuralGroup>& getGroups() { return groups; }
    const std::vector<NeuralGroup>& getGroups() const { return groups; }

private:
    // ВНИМАНИЕ
    AttentionMechanism attention;

    double dt;
    std::vector<NeuralGroup> groups;                 // 32 группы
    std::vector<std::vector<double>> interWeights;   // межгрупповые связи 32x32

    // Плоские векторы для обратной совместимости (обновляются после каждого шага)
    mutable std::vector<double> flatPhi;
    mutable std::vector<double> flatPi;
    mutable bool flatDirty; // флаг, что flat нужно пересчитать

    void rebuildFlatVectors() const;

    // Механизм повторного входа
    void applyReentry(int iterations = 3);

    // Вспомогательные методы для рефлексии (заглушки)
    std::deque<std::vector<double>> state_history;
    std::deque<double> energy_history;
    static constexpr int HISTORY_SIZE = 100;
    std::string current_goal;
    std::vector<double> goal_embedding;
};