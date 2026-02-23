//cpp_ai_test/core/NeuralFieldSystem.cpp
#include "NeuralFieldSystem.hpp"
#include <cmath>
#include <iostream>
// need some fix - the part neurons for outside from other modules - she need write code
//TODO: use words for learning
NeuralFieldSystem::NeuralFieldSystem(int Nside, double dt, double m, double lam)
    : Nside(Nside), N(Nside * Nside), dt(dt), m(m), lam(lam),
      phi(N), pi(N, 0.0), dH(N, 0.0), W(N, std::vector<double>(N, 0.0)) {}

void NeuralFieldSystem::initializeRandom(std::mt19937& rng, double phi_range, double w_range) {
    std::uniform_real_distribution<double> phi_dist(-phi_range, phi_range);
    std::uniform_real_distribution<double> w_dist(-w_range, w_range);
    
    for (int i = 0; i < N; i++) {
        phi[i] = phi_dist(rng);
        for (int j = i + 1; j < N; j++) {
            W[i][j] = w_dist(rng);
            W[j][i] = W[i][j]; // Симметрия
        }
    }
}

void NeuralFieldSystem::symplecticEvolution() {
    // Первая половина шага: обновление φ
    for (int i = 0; i < N; i++) {
        phi[i] += dt * pi[i];
    }
    
    // Вычисление производных
    computeDerivatives();
    
    // Вторая половина шага: обновление π
    for (int i = 0; i < N; i++) {
        pi[i] -= dt * dH[i];
    }
}

void NeuralFieldSystem::computeDerivatives() {
    for (int i = 0; i < N; i++) {
        double dV = m * m * phi[i] + 0.5 * lam * phi[i] * phi[i];
        double inter = 0.0;
        for (int j = 0; j < N; j++) {
            inter += W[i][j] * (phi[i] - phi[j]);
        }
        dH[i] = dV + inter;
    }
}

double NeuralFieldSystem::computeLocalEnergy(int i) const {
    double kinetic = 0.5 * pi[i] * pi[i];  // Кинетическая энергия всегда ≥ 0
    double potential = 0.5 * m * m * phi[i] * phi[i] + (lam / 6.0) * phi[i] * phi[i] * phi[i];
    double interaction = 0.0;
    
    for (int j = 0; j < N; j++) {
        double diff = phi[i] - phi[j];
        interaction += 0.5 * W[i][j] * diff * diff;  // Взаимодействие всегда ≥ 0
    }
    
    return kinetic + potential + interaction;
}

double NeuralFieldSystem::computeTotalEnergy() const {
    double total = 0.0;
    for (int i = 0; i < N; i++) {
        double local = computeLocalEnergy(i);
        if (local < 0) local = 0;  // Защита от отрицательной энергии?
        total += local;
    }
    return total / N;
}

//MARK: Mutation
void NeuralFieldSystem::applyTargetedMutation(double mutation_strength, int target_type) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-mutation_strength, mutation_strength);
    
    switch(target_type) {
        case 0: // Мутация весов - наиболее эффективная
            for (int i = 0; i < N; i++) {
                for (int j = i + 1; j < N; j++) {
                    double mutation = dis(gen);
                    W[i][j] += mutation;
                    W[j][i] = W[i][j]; // Сохраняем симметрию
                    
                    // Ограничение весов для стабильности
                    if (W[i][j] > 0.1) W[i][j] = 0.1;
                    if (W[i][j] < -0.1) W[i][j] = -0.1;
                }
            }
            std::cout << "Applied weight mutation with strength: " << mutation_strength << std::endl;
            break;
            
        case 1: // Мутация начальных условий - менее затратная
            for (int i = 0; i < N; i++) {
                phi[i] += dis(gen) * 0.1;
            }
            std::cout << "Applied phi mutation with strength: " << mutation_strength << std::endl;
            break;
            
        case 2: // Минимальная мутация - для стазиса
            for (int i = 0; i < std::min(5, N); i++) {
                int idx = gen() % N;
                phi[idx] += dis(gen) * 0.01;
            }
            std::cout << "Applied minimal mutation in stasis mode" << std::endl;
            break;
    }
}

void NeuralFieldSystem::enterLowPowerMode() {
    // В режиме низкого энергопотребления уменьшаем активность
    for (int i = 0; i < N; i++) {
        if (std::abs(phi[i]) < 0.01) {
            phi[i] = 0.0; // Обнуляем очень маленькие значения
        }
        if (std::abs(pi[i]) < 0.01) {
            pi[i] = 0.0;
        }
    }
    std::cout << "Entered low power mode" << std::endl;
}
// Языковой модуль
void NeuralFieldSystem::learnLanguageHebbian(int startIdx, int endIdx, double lr, double decay) {
    for (int i = startIdx; i < endIdx; ++i) {
        for (int j = i+1; j < endIdx; ++j) {
            double update = lr * phi[i] * phi[j];
            W[i][j] = W[i][j] * decay + update;
            W[j][i] = W[i][j];
            // ограничение
            if (W[i][j] > 0.1) W[i][j] = 0.1;
            if (W[i][j] < -0.1) W[i][j] = -0.1;
        }
    }
}

// забыл!
std::vector<float> NeuralFieldSystem::getFeatures() const {
    std::vector<float> features(64, 0.0f); // 64 признака для памяти
    
    // Сжимаем 1024 нейрона в 64 признака через простое усреднение
    // Каждый признак = среднее по группе из 16 нейронов (1024/64 = 16)
    const int groupSize = N / 64; // 1024/64 = 16
    
    for (int f = 0; f < 64; ++f) {
        float sumPhi = 0.0f;
        float sumPi = 0.0f;
        int startIdx = f * groupSize;
        int endIdx = std::min(startIdx + groupSize, N);
        
        for (int i = startIdx; i < endIdx; ++i) {
            sumPhi += static_cast<float>(phi[i]);
            sumPi += static_cast<float>(pi[i]);
        }
        
        int count = endIdx - startIdx;
        features[f] = (sumPhi / count) * 0.7f + (sumPi / count) * 0.3f;
    }
    
    return features;
}

/// REFLECTION SECTION !!!
// TODO: test it!

void NeuralFieldSystem::reflect() {
    // 1. Сохраняем текущее состояние в историю
    state_history.push_back(phi);
    energy_history.push_back(computeTotalEnergy());
    if (state_history.size() > HISTORY_SIZE) {
        state_history.pop_front();
        energy_history.pop_front();
    }
    
    // 2. Вычисляем метрики саморефлексии
    double attention = 0.0;
    double surprise = 0.0;
    double stability = 0.0;
    
    // Анализируем изменения в разных группах нейронов
    for (int group = 0; group < 4; group++) {
        int start = group * 256;
        int end = start + 256;
        
        double group_energy = 0.0;
        for (int i = start; i < end; i++) {
            group_energy += phi[i] * phi[i];
            
            // Само-внимание: насколько нейрон влияет на другие
            attention += computeSelfAttention(i);
            
            // Предсказание следующего состояния
            double predicted = predictNextState(i);
            surprise += std::abs(phi[i] - predicted);
        }
        
        // Записываем в нейроны метапознания
        int meta_idx = METACOGNITION_START + group;
        phi[meta_idx] = group_energy / 256.0; // энергия группы
    }
    
    // 3. Обновляем нейроны рефлексии
    if (state_history.size() > 10) {
        // Вычисляем стабильность (обратная величина к дисперсии)
        double variance = 0.0;
        for (size_t i = state_history.size() - 10; i < state_history.size(); i++) {
            for (int j = 0; j < 10; j++) {
                double diff = state_history[i][j] - phi[j];
                variance += diff * diff;
            }
        }
        stability = 1.0 / (1.0 + variance / 1000.0);
        
        // Записываем в нейроны рефлексии
        phi[REFLECTION_START] = stability;           // стабильность
        phi[REFLECTION_START + 1] = surprise / N;    // удивление
        phi[REFLECTION_START + 2] = attention / N;   // внимание
        phi[REFLECTION_START + 3] = computeTotalEnergy() / 10.0; // энергия
    }
    
    // 4. Проверяем аномалии
    if (detectAnomaly()) {
        // Активируем нейроны "тревоги"
        for (int i = REFLECTION_START + 10; i < REFLECTION_START + 20; i++) {
            phi[i] = 1.0;
        }
    }
    
    // 5. Обновляем связи на основе рефлексии
    for (int i = REFLECTION_START; i < REFLECTION_END; i++) {
        for (int j = METACOGNITION_START; j < METACOGNITION_END; j++) {
            // Усиливаем связи между рефлексией и метапознанием
            if (std::abs(phi[i] - phi[j]) < 0.3) {
                W[i][j] += 0.001;
                W[j][i] = W[i][j];
            }
        }
    }
}

double NeuralFieldSystem::computeSelfAttention(int neuron_idx) {
    double attention = 0.0;
    
    // Смотрим, насколько этот нейрон влияет на другие
    for (int j = 0; j < N; j++) {
        if (j != neuron_idx) {
            attention += std::abs(W[neuron_idx][j] * phi[j]);
        }
    }
    
    return attention / (N - 1);
}

double NeuralFieldSystem::predictNextState(int neuron_idx) {
    if (state_history.size() < 5) return phi[neuron_idx];
    
    // Простое линейное предсказание на основе истории
    double trend = 0.0;
    for (size_t i = state_history.size() - 5; i < state_history.size(); i++) {
        trend += state_history[i][neuron_idx] * (i - (state_history.size() - 5));
    }
    
    return phi[neuron_idx] + trend / 10.0;
}

bool NeuralFieldSystem::detectAnomaly() {
    if (energy_history.size() < 20) return false;
    
    // Вычисляем скользящее среднее энергии
    double recent_avg = 0.0;
    for (size_t i = energy_history.size() - 10; i < energy_history.size(); i++) {
        recent_avg += energy_history[i];
    }
    recent_avg /= 10;
    
    double old_avg = 0.0;
    for (size_t i = 0; i < 10; i++) {
        old_avg += energy_history[i];
    }
    old_avg /= 10;
    
    // Аномалия: резкое изменение энергии
    return std::abs(recent_avg - old_avg) > 5.0;
}

NeuralFieldSystem::ReflectionState NeuralFieldSystem::getReflectionState() const {
    ReflectionState state;
    
    // Декодируем состояние из нейронов рефлексии
    state.confidence = phi[REFLECTION_START];
    state.curiosity = phi[REFLECTION_START + 1];
    state.satisfaction = phi[REFLECTION_START + 2];
    state.confusion = 1.0 - state.confidence;
    
    // Карта внимания (на какие группы нейронов смотрит)
    state.attention_map.resize(4);
    for (int i = 0; i < 4; i++) {
        state.attention_map[i] = phi[METACOGNITION_START + i];
    }
    
    return state;
}

void NeuralFieldSystem::setGoal(const std::string& goal) {
    current_goal = goal;
    
    // Кодируем цель в нейроны
    goal_embedding.resize(20);
    for (size_t i = 0; i < goal.size() && i < 20; i++) {
        goal_embedding[i] = static_cast<double>(goal[i]) / 255.0;
    }
    
    // Записываем цель в специальную область
    for (int i = 0; i < 20; i++) {
        phi[REFLECTION_END - 20 + i] = goal_embedding[i];
    }
}

bool NeuralFieldSystem::evaluateProgress() {
    if (goal_embedding.empty()) return false;
    
    // Вычисляем, насколько текущее состояние близко к цели
    double similarity = 0.0;
    for (int i = 0; i < 20; i++) {
        double diff = phi[REFLECTION_END - 20 + i] - goal_embedding[i];
        similarity += diff * diff;
    }
    
    similarity = 1.0 / (1.0 + similarity);
    
    // Если близко к цели, усиливаем связи
    if (similarity > 0.8) {
        for (int i = 0; i < 20; i++) {
            int target = REFLECTION_END - 20 + i;
            for (int j = LANGUAGE_START; j < LANGUAGE_END; j++) {
                W[target][j] += 0.01;
                W[j][target] = W[target][j];
            }
        }
        return true;
    }
    
    return false;
}

void NeuralFieldSystem::learnFromReflection(float outcome) {
    // Мета-обучение: учимся тому, как учиться
    
    // Хороший исход - усиливаем текущие паттерны рефлексии
    if (outcome > 0.7) {
        for (int i = REFLECTION_START; i < REFLECTION_END; i++) {
            for (int j = 0; j < N; j++) {
                if (std::abs(phi[i] - phi[j]) < 0.2) {
                    W[i][j] += 0.001;
                    W[j][i] = W[i][j];
                }
            }
        }
    }
    // Плохой исход - пробуем новые паттерны
    else if (outcome < 0.3) {
        // Добавляем шум для исследования
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(-0.01, 0.01);
        
        for (int i = REFLECTION_START; i < REFLECTION_END; i++) {
            phi[i] += dis(gen);
            // Ограничиваем
            if (phi[i] > 1.0) phi[i] = 1.0;
            if (phi[i] < 0.0) phi[i] = 0.0;
        }
    }
}