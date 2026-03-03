#pragma once
#include "NeuralGroup.hpp"
#include <vector>
#include <random>
#include <string>
#include <deque>

/**
 * @class NeuralFieldSystem
 * @brief Контейнер групп нейронов с межгрупповыми связями и механизмом повторного входа.
 *
 * Поддерживает 32 группы по 32 нейрона (всего 1024 нейрона).
 * Межгрупповые связи представлены матрицей 32x32.
 */
class NeuralFieldSystem {
public:
    static constexpr int NUM_GROUPS = 32;       ///< 32 группы
    static constexpr int GROUP_SIZE = 32;       ///< 32 нейрона в группе
    static constexpr int TOTAL_NEURONS = NUM_GROUPS * GROUP_SIZE; // 1024

    /**
     * @param dt Глобальный шаг интегрирования
     */
    NeuralFieldSystem(double dt);

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
    void step();

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