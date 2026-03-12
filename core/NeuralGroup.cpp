#include "NeuralGroup.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <numeric>

NeuralGroup::NeuralGroup(int size, double dt, std::mt19937& rng)
    : size(size),
      dt(dt),
      threshold(0.5),
      phi(size, 0.0),
      pi(size, 0.0),
      dH(size, 0.0),
      W_intra(size, std::vector<double>(size, 0.0)),
      specialization("unspecialized")
{
    initializeRandom(rng);
    buildSynapsesFromWeights();
    weight_gradients.resize(size, std::vector<double>(size, 0.0));
    velocity.resize(size, std::vector<double>(size, 0.0));
}

void NeuralGroup::initializeRandom(std::mt19937& rng) {
    std::uniform_real_distribution<double> dist(-0.1, 0.1);
    
    // Инициализация с геометрической структурой (циклические связи)
    for (int i = 0; i < size; ++i) {
        phi[i] = dist(rng);
        pi[i] = dist(rng) * 0.1;
        
        for (int j = i + 1; j < size; ++j) {
            // Геометрический фактор: синусоидальная зависимость от расстояния
            double angle = 2.0 * M_PI * (j - i) / size;
            double geometric_factor = std::sin(angle) * 0.5;
            
            double w = dist(rng) * 0.02 + geometric_factor * 0.01;
            W_intra[i][j] = w;
            W_intra[j][i] = w;
        }
    }
    
    for (int i = 0; i < size; ++i) {
        W_intra[i][i] = 0.0;
    }
}

void NeuralGroup::buildSynapsesFromWeights() {
    synapses.clear();
    for (int i = 0; i < size; ++i) {
        for (int j = i + 1; j < size; ++j) {
            Synapse syn;
            syn.weight = static_cast<float>(W_intra[i][j]);
            synapses.push_back(syn);
        }
    }
}

void NeuralGroup::evolve() {
    // Обновление phi
    for (int i = 0; i < size; ++i) {
        phi[i] += dt * pi[i];
    }
    
    computeDerivatives();
    
    // Обновление pi
    for (int i = 0; i < size; ++i) {
        pi[i] -= dt * dH[i];
    }
    
    // Применяем активационную функцию
    for (int i = 0; i < size; ++i) {
        phi[i] = activationFunction(phi[i]);
    }
}

void NeuralGroup::computeDerivatives() {
    const double m = 1.0;
    const double lam = 0.5;
    
    for (int i = 0; i < size; ++i) {
        double dV = m * m * phi[i] + 0.5 * lam * phi[i] * phi[i];
        double inter = 0.0;
        for (int j = 0; j < size; ++j) {
            inter += W_intra[i][j] * (phi[i] - phi[j]);
        }
        dH[i] = dV + inter;
    }
}

double NeuralGroup::activationFunction(double x) const {
    const double temp = 0.1;
    return 1.0 / (1.0 + std::exp(-(x - threshold) / temp));
}

double NeuralGroup::computeEntropy() const {
    const int BINS = 10;
    std::vector<int> hist(BINS, 0);
    
    for (double v : phi) {
        int bin = static_cast<int>(v * BINS);
        bin = std::clamp(bin, 0, BINS - 1);
        hist[bin]++;
    }
    
    double entropy = 0.0;
    for (int count : hist) {
        if (count > 0) {
            double p = static_cast<double>(count) / size;
            entropy -= p * std::log(p);
        }
    }
    
    return entropy;
}

double NeuralGroup::computeEntropyTarget() const {
    // Целевая энтропия - максимальна при равномерном распределении
    return std::log(static_cast<double>(size));
}

void NeuralGroup::learnSTDP(float reward, int currentStep) {
    int synIndex = 0;
    double current_entropy = computeEntropy();
    double target_entropy = computeEntropyTarget();
    double entropy_error = target_entropy - current_entropy;

    for (int i = 0; i < size; ++i) {
        for (int j = i + 1; j < size; ++j) {
            if (synIndex >= synapses.size()) break;

            auto& syn = synapses[synIndex++];

            const bool preSpike  = (phi[i] > threshold);
            const bool postSpike = (phi[j] > threshold);

            if (preSpike) syn.lastPreFire = static_cast<float>(currentStep);
            if (postSpike) syn.lastPostFire = static_cast<float>(currentStep);

            if (!(preSpike || postSpike)) continue;

            float delta = 0.0f;
            float dt = syn.lastPostFire - syn.lastPreFire;

            if (dt > 0.0f) {
                delta = params.A_plus * std::exp(-dt / params.tau_plus);
            } else if (dt < 0.0f) {
                delta = -params.A_minus * std::exp(dt / params.tau_minus);
            }

            float elevation_factor = 1.0f + elevation_;
            
            // Добавляем энтропийную компоненту в обновление
            // Если энтропия太低, поощряем изменения
            float entropy_factor = 1.0f + static_cast<float>(entropy_error * 0.1);
            
            syn.eligibility = syn.eligibility * params.eligibilityDecay + delta;
            
            syn.weight += params.stdpRate * reward * syn.eligibility * 
                         elevation_factor * entropy_factor;

            syn.weight = std::clamp(syn.weight, -params.maxWeight, params.maxWeight);

            W_intra[i][j] = syn.weight;
            W_intra[j][i] = syn.weight;
        }
    }
}

void NeuralGroup::learnHebbian(double globalReward) {
    double current_entropy = computeEntropy();
    double target_entropy = computeEntropyTarget();
    double entropy_factor = 1.0 + (target_entropy - current_entropy) * 0.1;

    for (int i = 0; i < size; ++i) {
        for (int j = i + 1; j < size; ++j) {
            double hebb = phi[i] * phi[j];
            double update = params.hebbianRate * (hebb + globalReward * 0.1) * entropy_factor;
            W_intra[i][j] += update;
            W_intra[j][i] = W_intra[i][j];
            
            if (W_intra[i][j] > params.maxWeight) W_intra[i][j] = params.maxWeight;
            if (W_intra[i][j] < -params.maxWeight) W_intra[i][j] = -params.maxWeight;
        }
    }
}

void NeuralGroup::consolidate() {
    for (auto& syn : synapses) {
        syn.weight += params.consolidationRate * syn.eligibility;
        syn.eligibility *= 0.9f;
        
        if (syn.weight > params.maxWeight) syn.weight = params.maxWeight;
        if (syn.weight < -params.maxWeight) syn.weight = -params.maxWeight;
    }
}

double NeuralGroup::getAverageActivity() const {
    double sum = 0.0;
    for (double val : phi) sum += val;
    return sum / size;
}

void NeuralGroup::computeGradients(const std::vector<double>& target) {
    if (target.size() != size) return;
    
    // Вместо target используем энтропийную цель
    double current_entropy = computeEntropy();
    double target_entropy = computeEntropyTarget();
    double entropy_gradient = target_entropy - current_entropy;

    for (int i = 0; i < size; ++i) {
        // Модифицируем ошибку с учетом энтропии
        double error_i = (phi[i] - 0.5) * entropy_gradient * 0.1;
        for (int j = 0; j < size; ++j) {
            weight_gradients[i][j] = error_i * phi[j];
        }
    }
}

void NeuralGroup::applyGradients() {
    for (int i = 0; i < size; ++i) {
        for (int j = i + 1; j < size; ++j) {
            velocity[i][j] = gd_momentum * velocity[i][j]
                           - gd_learning_rate * weight_gradients[i][j];

            W_intra[i][j] += velocity[i][j];
            W_intra[j][i] = W_intra[i][j];

            W_intra[i][j] = std::clamp(
                W_intra[i][j],
                -static_cast<double>(params.maxWeight),
                static_cast<double>(params.maxWeight)
            );
            W_intra[j][i] = W_intra[i][j];
        }
    }
    
    int idx = 0;
    for (int i = 0; i < size; ++i) {
        for (int j = i + 1; j < size; ++j) {
            synapses[idx++].weight = static_cast<float>(W_intra[i][j]);
        }
    }
}

void NeuralGroup::updateElevationFast(float reward, float activity) {
    double entropy = computeEntropy();
    double target_entropy = computeEntropyTarget();
    double entropy_contribution = -entropy * std::log(entropy + 1e-12);
    
    if (activity > getEffectiveThreshold()) {
        // Высота растет, если нейрон вносит вклад в энтропию
        elevation_ += elevation_learning_rate_ * reward * entropy_contribution;
    }
    
    elevation_ *= elevation_decay_;
    elevation_ = std::clamp(elevation_, -1.0f, 1.0f);
}

void NeuralGroup::decayAllWeights(float factor) {
    for (auto& syn : synapses) {
        syn.weight *= factor;
    }
    
    int idx = 0;
    for (int i = 0; i < size; ++i) {
        for (int j = i + 1; j < size; ++j) {
            W_intra[i][j] = synapses[idx].weight;
            W_intra[j][i] = synapses[idx].weight;
            idx++;
        }
    }
}

void NeuralGroup::consolidateElevation(float globalImportance, float hallucinationPenalty) {
    if (activity_counter_ > 0) {
        float avg_importance = cumulative_importance_ / activity_counter_;
        
        // Учитываем энтропийный вклад
        double entropy = computeEntropy();
        double entropy_factor = 1.0 + (entropy - 2.0) * 0.1;
        
        elevation_ += (avg_importance - 0.5f) * 0.1f * entropy_factor;
        elevation_ -= hallucinationPenalty * 0.2f;
        elevation_ = std::clamp(elevation_, -1.0f, 1.0f);
    }
    
    cumulative_importance_ = 0.0f;
    activity_counter_ = 0;
}

void NeuralGroup::consolidateEligibility(float globalImportance) {
    double entropy = computeEntropy();
    double entropy_factor = 1.0 / (1.0 + std::exp(-(entropy - 2.0)));

    for (auto& syn : synapses) {
        float consolidation_factor = params.consolidationRate * globalImportance;
        consolidation_factor *= (1.0f + elevation_) * static_cast<float>(entropy_factor);
        
        syn.weight += consolidation_factor * syn.eligibility;
        syn.eligibility *= 0.5f;
        
        syn.weight = std::clamp(syn.weight, -params.maxWeight, params.maxWeight);
    }
    // для обратной совместимости? - ДА, ЭТО НУЖНО!
    // W_intra используется в computeDerivatives() и evolve()
    int idx = 0;
    for (int i = 0; i < size; ++i) {
        for (int j = i + 1; j < size; ++j) {
            W_intra[i][j] = synapses[idx].weight;
            W_intra[j][i] = synapses[idx].weight;
            idx++;
        }
    }
}