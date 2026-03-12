#pragma once
#include <vector>
#include <random>
#include <string>
#include "Synapse.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @class NeuralGroup
 * @brief Функциональная группа нейронов с энтропийной регуляризацией
 * 
 * Логика:
 * - 32 нейрона в группе с рекуррентными связями
 * - STDP с элигибилити-трейсами и модуляцией высотой (elevation)
 * - Энтропийная цель обучения вместо target=0.5
 * - Геометрическая инициализация весов с использованием π
 */
class NeuralGroup {
public:
    NeuralGroup(int size, double dt, std::mt19937& rng);

    void computeGradients(const std::vector<double>& target);
    void applyGradients();

    static constexpr int ENTROPY_BINS = 10;
    
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
    void consolidate();
    
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
    
    // Доступ к синапсам
    std::vector<Synapse>& getSynapses() { return synapses; }

    // Методы для работы с высотой
    void setElevation(float elev) { elevation_ = elev; }
    float getElevation() const { return elevation_; }
    
    // Уровень 1: быстрое обновление высоты
    void updateElevationFast(float reward, float activity);
    
    // Уровень 2: консолидация высоты
    void consolidateElevation(float globalImportance, float hallucinationPenalty = 0.0f);
    
    // Уровень 2: консолидация элигибилити
    void consolidateEligibility(float globalImportance);
    
    // Для совместимости
    void updateEligibilityTraces(float reward, int currentStep) {
        learnSTDP(reward, currentStep);
    }
    
    // Получить модифицированный порог с учетом высоты
    double getEffectiveThreshold() const {
        return threshold - elevation_ * 0.05; 
    }

    // Вспомогательный метод для прореживания
    void decayAllWeights(float factor);
    
    // Вычисление энтропии распределения активностей в группе
    double computeEntropy() const;

private:
    // Gradient Descent learning
    std::vector<std::vector<double>> weight_gradients;
    std::vector<std::vector<double>> velocity;
    double gd_learning_rate = 0.001;
    double gd_momentum = 0.9;

    int size;
    double dt;
    double threshold;
    
    std::vector<double> phi;
    std::vector<double> pi;
    std::vector<std::vector<double>> W_intra;
    
    std::vector<double> dH;
    
    // Синапсы и параметры пластичности
    std::vector<Synapse> synapses;
    PlasticityParams params;
    
    // Вспомогательные методы
    void computeDerivatives();
    double activationFunction(double x) const;
    void initializeRandom(std::mt19937& rng);
    void buildSynapsesFromWeights();
    
    // Целевая функция на основе энтропии
    double computeEntropyTarget() const;

    // Высота нейрона (положительная - консервация, отрицательная - пластичность)
    float elevation_ = 0.0f;  
    
    float elevation_learning_rate_ = 0.001f;
    float elevation_decay_ = 0.999f;
    
    float cumulative_importance_ = 0.0f;
    int activity_counter_ = 0;
};