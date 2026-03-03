#pragma once
#include <vector>
#include <random>
#include <string>

/**
 * @class NeuralGroup
 * @brief Функциональная группа нейронов (аналог колонки в коре).
 *
 * Каждая группа содержит 32 нейрона,有自己的内部动态和内部连接。
 * Группа может быть специализирована (язык, внимание и т.д.).
 */
class NeuralGroup {
public:
    /**
     * @param size       Количество нейронов в группе (рекомендуется 32)
     * @param dt         Глобальный шаг интегрирования
     * @param rng        Генератор случайных чисел для инициализации
     */
    NeuralGroup(int size, double dt, std::mt19937& rng);

    // Запрет копирования (матрицы весов большие)
    NeuralGroup(const NeuralGroup&) = delete;
    NeuralGroup& operator=(const NeuralGroup&) = delete;

    // Разрешаем перемещение
    NeuralGroup(NeuralGroup&&) = default;
    NeuralGroup& operator=(NeuralGroup&&) = default;

    // в public:
std::vector<double>& getPhiNonConst() { return phi; }

    /** Эволюция внутренней динамики группы (симплектический интегратор). */
    void evolve();

    /** Локальное обучение (правило Хебба) внутри группы. */
    void learnHebbian(double globalReward);

    /** Получить среднюю активность группы (используется для межгрупповых связей). */
    double getAverageActivity() const;

    /** Получить вектор активностей нейронов (для визуализации). */
    const std::vector<double>& getPhi() const { return phi; }

    /** Получить вектор импульсов (для визуализации). */
    const std::vector<double>& getPi() const { return pi; }

    /** Установить новую скорость обучения (для эволюции). */
    void setLearningRate(double lr) { learningRate = lr; }

    /** Установить порог активации (для эволюции). */
    void setThreshold(double thr) { threshold = thr; }

    /** Получить текущую скорость обучения. */
    double getLearningRate() const { return learningRate; }

    /** Получить порог. */
    double getThreshold() const { return threshold; }

    /** Специализация группы (задаётся извне). */
    std::string specialization;

private:
    int size;                   // количество нейронов в группе
    double dt;                  // шаг интегрирования
    double learningRate;         // скорость обучения (может меняться)
    double threshold;            // порог активации (влияет на функцию активации)

    std::vector<double> phi;     // активности нейронов (поле)
    std::vector<double> pi;      // сопряжённые импульсы
    std::vector<std::vector<double>> W_intra; // внутригрупповые связи (size x size)

    // Вспомогательные векторы для вычислений
    std::vector<double> dH;      // производные гамильтониана

    void computeDerivatives();
    double activationFunction(double x) const; // сигмоида с порогом
    void initializeRandom(std::mt19937& rng);
};