#pragma once

#include <vector>
#include <deque>
#include <cstddef>

// Forward declarations
class NeuralFieldSystem;
class EmergentMemory;

/**
 * @class PredictiveCoder
 * @brief Реализация простого предсказательного кодирования
 *
 * Использует существующие семантические группы (16–21)
 * нейронного поля для обучения через ошибку предсказания.
 */
class PredictiveCoder {
public:
    PredictiveCoder(NeuralFieldSystem& neural_system,
                    EmergentMemory& memory);

    /**
     * Основной шаг предсказательного кодирования
     */
    float step(int step_number);

    float getLastError() const;
    float getAverageError() const;
    bool isAnomaly() const;

    /**
     * Обучение на паре (prediction, actual)
     */
    void learn(const std::vector<float>& predicted,
               const std::vector<float>& actual_next,
               int step);

private:
    void modulateLearning(float error, int step);
    void triggerAnomaly(float error, int step);
    void storeCompressedState(const std::vector<float>& state, float error);

private:
    NeuralFieldSystem& neural_system_;
    EmergentMemory& memory_;

    std::deque<std::vector<float>> state_history_;
    std::deque<float> prediction_errors_;

    static constexpr std::size_t MAX_HISTORY = 100;

    float error_threshold_ = 0.15f;
    float learning_rate_ = 0.01f;
};