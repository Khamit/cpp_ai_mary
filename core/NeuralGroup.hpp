#pragma once
#include <vector>
#include <random>
#include <string>
#include "Synapse.hpp"

/**
 * @class NeuralGroup
 * @brief Функциональная группа нейронов (аналог колонки в core).
 *
 * Каждая группа содержит 32 нейрона
 * Группа может быть специализирована (язык, внимание и т.д.).
 */
class NeuralGroup {
public:
    NeuralGroup(int size, double dt, std::mt19937& rng);

    void computeGradients(const std::vector<double>& target);
    void applyGradients();
    
    // Запрет копирования
    NeuralGroup(const NeuralGroup&) = delete;
    NeuralGroup& operator=(const NeuralGroup&) = delete;
    
    // Разрешаем перемещение
    NeuralGroup(NeuralGroup&&) = default;
    NeuralGroup& operator=(NeuralGroup&&) = default;
    
    // Основные методы
    void evolve();
    void learnSTDP(float reward, int currentStep);
    void learnHebbian(double globalReward);
    void consolidate(); // долговременная консолидация
    
    // Геттеры/сеттеры
    double getAverageActivity() const;
    const std::vector<double>& getPhi() const { return phi; }
    std::vector<double>& getPhiNonConst() { return phi; }
    const std::vector<double>& getPi() const { return pi; }
    
    void setLearningRate(double lr) { params.hebbianRate = lr; }
    double getLearningRate() const { return params.hebbianRate; }
    
    void setThreshold(double thr) { threshold = thr; }
    double getThreshold() const { return threshold; }
    
    std::string specialization;
    
    // Доступ к синапсам (для консолидации)
    std::vector<Synapse>& getSynapses() { return synapses; }

private:

    // --- Gradient Descent learning ---
    std::vector<std::vector<double>> weight_gradients;
    std::vector<std::vector<double>> velocity;
    double gd_learning_rate = 0.001;
    double gd_momentum = 0.9;

    int size;                           // количество нейронов
    double dt;                          // шаг интегрирования
    double threshold;                    // порог активации
    
    std::vector<double> phi;             // активности нейронов
    std::vector<double> pi;              // импульсы
    std::vector<std::vector<double>> W_intra; // временно оставляем для совместимости
    
    std::vector<double> dH;               // производные
    
    // НОВОЕ: синапсы и параметры пластичности
    std::vector<Synapse> synapses;        // список всех синапсов в группе
    PlasticityParams params;               // параметры пластичности
    
    // Вспомогательные методы
    void computeDerivatives();
    double activationFunction(double x) const;
    void initializeRandom(std::mt19937& rng);
    
    // Построение синапсов из W_intra (для миграции)
    void buildSynapsesFromWeights();
};