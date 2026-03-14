// modules/PredictorGroup.cpp
#include "PredictorGroup.hpp"
#include <cmath>
#include <numeric>
#include <algorithm>

PredictorGroup::PredictorGroup(size_t input_dim, size_t latent_dim, std::mt19937& rng)
    : input_size(input_dim), latent_size(latent_dim)
{
    // Группа-предсказатель (рекуррентная, 16 нейронов)
    predictor = std::make_unique<NeuralGroup>(16, 0.001, rng);
    
    // Группа для кодирования ошибки (8 нейронов)
    error_encoder = std::make_unique<NeuralGroup>(8, 0.001, rng);
    
    // Инициализируем специально для предсказаний (усилим рекуррентность)
    // Здесь можно добавить специальную инициализацию весов
}

std::vector<double> PredictorGroup::predict(const std::vector<double>& current_state) {
    if (current_state.size() != input_size) {
        std::cerr << "PredictorGroup: неверный размер входного состояния" << std::endl;
        return {};
    }
    
    // Подаём текущее состояние на вход предсказателя
    // Используем первые 16 нейронов для кодирования состояния (упрощённо)
    for (size_t i = 0; i < std::min(current_state.size(), predictor->getPhi().size()); ++i) {
        predictor->getPhiNonConst()[i] = current_state[i];
    }
    
    // Эволюция предсказателя (несколько шагов для рекуррентности)
    for (int i = 0; i < 5; ++i) {
        predictor->evolve();
    }
    
    // Получаем предсказание из активности предсказателя
    std::vector<double> prediction(input_size, 0.0);
    for (size_t i = 0; i < std::min(prediction.size(), predictor->getPhi().size()); ++i) {
        prediction[i] = predictor->getPhi()[i];
    }
    
    return prediction;
}

double PredictorGroup::update(const std::vector<double>& predicted, 
                              const std::vector<double>& actual,
                              int step_number) {
    // Вычисляем ошибку предсказания (surprise)
    double error = 0.0;
    std::vector<double> error_signal(predicted.size(), 0.0);
    
    for (size_t i = 0; i < predicted.size(); ++i) {
        double diff = actual[i] - predicted[i];
        error_signal[i] = diff;
        error += diff * diff;
    }
    error = std::sqrt(error / predicted.size());  // RMSE
    
    // Сохраняем в историю
    prediction_errors.push_back(error);
    if (prediction_errors.size() > 100) {
        prediction_errors.pop_front();
    }
    
    // Обучаем предсказатель через STDP с наградой, обратной ошибке
    // Чем меньше ошибка, тем больше награда
    float reward = static_cast<float>(1.0 - std::tanh(error * 5.0));
    predictor->learnSTDP(reward, step_number);
    
    // Кодируем ошибку для долговременной памяти
    for (size_t i = 0; i < std::min(error_signal.size(), error_encoder->getPhi().size()); ++i) {
        error_encoder->getPhiNonConst()[i] = error_signal[i];
    }
    error_encoder->evolve();
    error_encoder->learnSTDP(reward * 0.5f, step_number);  // учимся кодировать ошибку
    
    return error;
}

std::vector<double> PredictorGroup::encode(const std::vector<double>& state) {
    std::vector<double> latent(latent_size, 0.0);
    
    // Простое сжатие: усредняем по блокам
    size_t block_size = input_size / latent_size;
    for (size_t i = 0; i < latent_size; ++i) {
        size_t start = i * block_size;
        size_t end = std::min(start + block_size, input_size);
        double sum = 0.0;
        for (size_t j = start; j < end; ++j) {
            sum += state[j];
        }
        latent[i] = sum / (end - start);
    }
    
    return latent;
}

std::vector<double> PredictorGroup::decode(const std::vector<double>& latent) {
    std::vector<double> reconstructed(input_size, 0.0);
    
    // Обратное преобразование (линейная интерполяция)
    size_t block_size = input_size / latent_size;
    for (size_t i = 0; i < latent_size; ++i) {
        size_t start = i * block_size;
        size_t end = std::min(start + block_size, input_size);
        for (size_t j = start; j < end; ++j) {
            reconstructed[j] = latent[i];
        }
    }
    
    return reconstructed;
}

bool PredictorGroup::isAnomaly(const std::vector<double>& state) {
    auto predicted = predict(state);
    auto error = update(predicted, state, 0);  // step_number = 0 для проверки
    
    return error > error_threshold;
}

double PredictorGroup::getPredictionEntropy() const {
    if (prediction_errors.size() < 2) return 0.0;
    
    // Вычисляем энтропию распределения ошибок
    const int BINS = 10;
    std::vector<int> hist(BINS, 0);
    
    double min_err = *std::min_element(prediction_errors.begin(), prediction_errors.end());
    double max_err = *std::max_element(prediction_errors.begin(), prediction_errors.end());
    double range = max_err - min_err;
    
    if (range < 1e-6) return 0.0;
    
    for (double err : prediction_errors) {
        int bin = static_cast<int>((err - min_err) / range * BINS);
        bin = std::clamp(bin, 0, BINS - 1);
        hist[bin]++;
    }
    
    double entropy = 0.0;
    double total = static_cast<double>(prediction_errors.size());
    
    for (int count : hist) {
        if (count > 0) {
            double p = count / total;
            entropy -= p * std::log(p);
        }
    }
    
    return entropy;
}

void PredictorGroup::save(MemoryManager& memory) const {
    // Сохраняем веса предсказателя
    std::vector<float> pred_weights;
    for (const auto& syn : predictor->getSynapses()) {
        pred_weights.push_back(syn.weight);
    }
    std::map<std::string, std::string> meta;
    meta["type"] = "predictor";
    memory.store("predictor", "weights", pred_weights, 1.0f, meta);
    
    // Сохраняем веса кодера ошибок
    std::vector<float> error_weights;
    for (const auto& syn : error_encoder->getSynapses()) {
        error_weights.push_back(syn.weight);
    }
    meta["type"] = "error_encoder";
    memory.store("predictor", "error_weights", error_weights, 1.0f, meta);
}

void PredictorGroup::load(MemoryManager& memory) {
    auto pred_records = memory.getRecords("predictor");
    for (const auto& rec : pred_records) {
        auto type_it = rec.metadata.find("type");
        if (type_it != rec.metadata.end()) {
            if (type_it->second == "weights") {
                // Восстановить веса predictor (нужна реализация)
            } else if (type_it->second == "error_weights") {
                // Восстановить веса error_encoder
            }
        }
    }
}