// modules/PredictorGroup.hpp
#pragma once
#include "NeuralGroup.hpp"
#include <deque>
#include <vector>
#include "MemoryManager.hpp"

/**
 * @class PredictorGroup
 * @brief Специализированная группа для предсказательного кодирования
 * 
 * Реализует идею predictive coding:
 * - Предсказывает следующее состояние системы
 * - Хранит только ошибку предсказания (компрессия)
 * - Обучается минимизировать surprise (Free Energy Principle)
 */
class PredictorGroup {
private:
    std::unique_ptr<NeuralGroup> predictor;      // группа-предсказатель (16 нейронов)
    std::unique_ptr<NeuralGroup> error_encoder;  // группа для кодирования ошибки (8 нейронов)
    
    std::deque<std::vector<double>> state_history;  // история состояний
    std::deque<double> prediction_errors;            // история ошибок
    
    size_t input_size;      // размер входного состояния (например, 64 признака)
    size_t latent_size;     // размер латентного пространства (например, 16)
    
    // Параметры обучения
    double learning_rate = 0.01;
    double error_threshold = 0.1;  // порог для обнаружения аномалий
    
public:
    PredictorGroup(size_t input_dim, size_t latent_dim, std::mt19937& rng);
    
    /**
     * @brief Предсказать следующее состояние
     * @param current_state Текущее состояние системы (например, 64 признака)
     * @return Предсказанное следующее состояние
     */
    std::vector<double> predict(const std::vector<double>& current_state);
    
    /**
     * @brief Обновить модель на основе реального следующего состояния
     * @param predicted Предсказанное состояние
     * @param actual Реальное следующее состояние
     * @param step_number Текущий шаг для STDP
     * @return Ошибка предсказания (surprise)
     */
    double update(const std::vector<double>& predicted, 
                  const std::vector<double>& actual,
                  int step_number);
    
    /**
     * @brief Сжать состояние до латентного представления
     * @param state Входное состояние
     * @return Сжатое представление (латентный код)
     */
    std::vector<double> encode(const std::vector<double>& state);
    
    /**
     * @brief Восстановить состояние из латентного представления
     * @param latent Латентный код
     * @return Восстановленное состояние
     */
    std::vector<double> decode(const std::vector<double>& latent);
    
    /**
     * @brief Проверить, является ли состояние аномалией
     * @param state Текущее состояние
     * @return true если ошибка предсказания > порога
     */
    bool isAnomaly(const std::vector<double>& state);
    
    /**
     * @brief Получить текущую энтропию ошибок предсказания
     */
    double getPredictionEntropy() const;
    
    /**
     * @brief Сохранить/загрузить веса
     */
    void save(MemoryManager& memory) const;
    void load(MemoryManager& memory);
    
    // Геттеры
    double getLastError() const { return prediction_errors.empty() ? 0.0 : prediction_errors.back(); }
    size_t getHistorySize() const { return state_history.size(); }
};