#include "PredictiveCoder.hpp"

#include "NeuralFieldSystem.hpp"
#include "MemoryManager.hpp"

#include <cmath>
#include <iostream>
#include <algorithm>

PredictiveCoder::PredictiveCoder(NeuralFieldSystem& neural_system,
                                 MemoryManager& memory_manager)
    : neural_system_(neural_system),
      memory_manager_(memory_manager) {}

float PredictiveCoder::step(int step_number)
{
auto current_state = neural_system_.getFeatures();
    
    if (!state_history_.empty()) {
        auto& prev_state = state_history_.back();
        
        float prediction_error = 0.0f;
        for (size_t i = 0; i < current_state.size(); i++) {
            float diff = current_state[i] - prev_state[i];
            prediction_error += diff * diff;
        }
        prediction_error = std::sqrt(prediction_error / current_state.size());
        
        prediction_errors_.push_back(prediction_error);
        if (prediction_errors_.size() > MAX_HISTORY) {
            prediction_errors_.pop_front();
        }
        
        // Не детектим аномалии слишком часто
        static int last_anomaly_step = 0;
        if (prediction_error > error_threshold_ && 
            step_number - last_anomaly_step > 50) {  // минимум 50 шагов между аномалиями
            triggerAnomaly(prediction_error, step_number);
            last_anomaly_step = step_number;
        }
        
        storeCompressedState(current_state, prediction_error);
        return prediction_error;
    }
    
    state_history_.push_back(current_state);
    if (state_history_.size() > MAX_HISTORY) {
        state_history_.pop_front();
    }
    
    return 0.0f;
}

float PredictiveCoder::getLastError() const
{
    if (prediction_errors_.empty()) {
        return 0.0f;
    }

    return prediction_errors_.back();
}

float PredictiveCoder::getAverageError() const
{
    if (prediction_errors_.empty()) {
        return 0.0f;
    }

    float sum = 0.0f;

    for (float e : prediction_errors_) {
        sum += e;
    }

    return sum / static_cast<float>(prediction_errors_.size());
}

bool PredictiveCoder::isAnomaly() const
{
    if (prediction_errors_.empty()) {
        return false;
    }

    return prediction_errors_.back() > error_threshold_;
}

void PredictiveCoder::learn(const std::vector<float>& predicted,
                            const std::vector<float>& actual_next,
                            int step)
{
    if (predicted.empty() || actual_next.empty()) {
        return;
    }

    float error = 0.0f;

    std::size_t n = std::min(predicted.size(), actual_next.size());

    for (std::size_t i = 0; i < n; ++i) {
        float diff = predicted[i] - actual_next[i];
        error += diff * diff;
    }

    error = std::sqrt(error / static_cast<float>(n));

    for (int g = 16; g <= 21; ++g) {

        auto& group = neural_system_.getGroups()[g];

        float reward = 1.0f - std::tanh(error * 5.0f);

        group.learnSTDP(reward, step);
    }
}

void PredictiveCoder::modulateLearning(float error, int step)
{
    float reward = 1.0f - std::tanh(error * 5.0f);

    for (int g = 16; g <= 21; ++g) {

        auto& group = neural_system_.getGroups()[g];

        group.learnSTDP(reward, step);
    }
}

// В PredictiveCoder.cpp, добавьте статическую переменную для отслеживания времени
// В PredictiveCoder.cpp, исправьте triggerAnomaly:
void PredictiveCoder::triggerAnomaly(float error, int step) {
    static int last_anomaly_step = 0;
    static int anomaly_count = 0;
    static int total_anomalies = 0;
    
    // Сбрасываем счетчик каждые 1000 шагов
    if (step % 1000 == 0) {
        anomaly_count = 0;
    }
    
    // Не детектим аномалии слишком часто
    if (step - last_anomaly_step < 100) {  // увеличили с 50 до 100
        return;
    }
    
    // Ограничиваем общее количество аномалий
    if (total_anomalies > 20) {
        return;  // Хватит аномалий
    }
    
    // Игнорируем первые 100 шагов
    if (step < 100) {
        return;
    }
    
    // Только серьезные аномалии
    if (error < 0.5) {  // игнорируем мелкие ошибки
        return;
    }
    
    last_anomaly_step = step;
    anomaly_count++;
    total_anomalies++;
    
    std::cout << "Predictive anomaly: error=" << error << " step=" << step << std::endl;
    
    // НЕ генерируем событие, чтобы избежать двойного вывода
    // Просто логируем
}

void PredictiveCoder::storeCompressedState(const std::vector<float>& state,
                                           float error)
{
    // ЗАЩИТА 1: Проверка валидности
    if (state.size() < 64) {
        return;
    }
    
    // ЗАЩИТА 2: Проверка на NaN/Inf
    for (size_t i = 0; i < std::min(state.size(), size_t(10)); i++) {
        if (!std::isfinite(state[i])) {
            std::cerr << "PredictiveCoder: невалидное состояние" << std::endl;
            return;
        }
    }
    
    // ЗАЩИТА 3: Слишком частая запись
    static int last_store_step = 0;
    static int store_count = 0;
    
    // Сбрасываем счетчик каждые 1000 шагов
    static int current_step = 0;
    if (current_step % 1000 == 0) {
        store_count = 0;
    }
    
    // Не сохраняем слишком часто (максимум 1 раз в 10 шагов)
    if (current_step - last_store_step < 10) {
        return;
    }
    
    // Не сохраняем слишком много всего (максимум 100 сохранений)
    if (store_count > 100) {
        return;
    }
    
    // Создаем сжатое представление
    std::vector<float> compressed(16, 0.0f);
    for (int i = 0; i < 16; ++i) {
        float sum = 0.0f;
        for (int j = 0; j < 4; ++j) {
            sum += state[i * 4 + j];
        }
        compressed[i] = sum / 4.0f;
    }
    
    // ЗАЩИТА 4: Проверяем сжатые данные
    for (float val : compressed) {
        if (!std::isfinite(val)) {
            std::cerr << "PredictiveCoder: невалидное сжатое состояние" << std::endl;
            return;
        }
    }
    
    // Сохраняем с защитой от исключений
    try {
        memory_manager_.storeWithEntropy(
            "predictive",
            compressed,
            static_cast<double>(error),
            0.7f
        );
        last_store_step = current_step;
        store_count++;
    } catch (const std::exception& e) {
        std::cerr << "Ошибка сохранения в PredictiveCoder: " << e.what() << std::endl;
    }
    
    current_step++;
}