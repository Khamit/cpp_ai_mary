#include "NeuralGroup.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

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
    buildSynapsesFromWeights(); // создаём синапсы из матрицы весов
    weight_gradients.resize(size, std::vector<double>(size, 0.0));
    velocity.resize(size, std::vector<double>(size, 0.0));

}

void NeuralGroup::initializeRandom(std::mt19937& rng) {
    std::uniform_real_distribution<double> dist(-0.1, 0.1);
    for (int i = 0; i < size; ++i) {
        phi[i] = dist(rng);
        pi[i] = dist(rng) * 0.1;
        for (int j = i + 1; j < size; ++j) {
            double w = dist(rng) * 0.02;
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
    // Создаём синапс для каждой связи (только верхний треугольник)
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

void NeuralGroup::learnSTDP(float reward, int currentStep)
{
    int synIndex = 0;

    for (int i = 0; i < size; ++i)
    {
        for (int j = i + 1; j < size; ++j)
        {
            if (synIndex >= synapses.size())
                break;

            auto& syn = synapses[synIndex++];

            const bool preSpike  = (phi[i] > threshold);
            const bool postSpike = (phi[j] > threshold);

            // Обновляем времена спайков
            if (preSpike)
                syn.lastPreFire = static_cast<float>(currentStep);

            if (postSpike)
                syn.lastPostFire = static_cast<float>(currentStep);

            // STDP работает только если есть новое событие
            if (!(preSpike || postSpike))
                continue;

            float delta = 0.0f;

            float dt = syn.lastPostFire - syn.lastPreFire;

            if (dt > 0.0f)
            {
                // LTP
                delta = params.A_plus *
                        std::exp(-dt / params.tau_plus);
            }
            else if (dt < 0.0f)
            {
                // LTD
                delta = -params.A_minus *
                        std::exp(dt / params.tau_minus);
            }

            // Обновляем eligibility trace (медленная память)
            syn.eligibility =
                syn.eligibility * params.eligibilityDecay +
                delta;

            // Reward-modulated update
            syn.weight += params.stdpRate *
                          reward *
                          syn.eligibility;

            // Ограничение веса
            syn.weight = std::clamp(
                syn.weight,
                -params.maxWeight,
                 params.maxWeight
            );

            // Синхронизация с матрицей
            W_intra[i][j] = syn.weight;
            W_intra[j][i] = syn.weight;
        }
    }
}

void NeuralGroup::learnHebbian(double globalReward) {
    for (int i = 0; i < size; ++i) {
        for (int j = i + 1; j < size; ++j) {
            double hebb = phi[i] * phi[j];
            double update = params.hebbianRate * (hebb + globalReward * 0.1);
            W_intra[i][j] += update;
            W_intra[j][i] = W_intra[i][j];
            
            if (W_intra[i][j] > params.maxWeight) W_intra[i][j] = params.maxWeight;
            if (W_intra[i][j] < -params.maxWeight) W_intra[i][j] = -params.maxWeight;
        }
    }
}

void NeuralGroup::consolidate() {
    for (auto& syn : synapses) {
        // Усредняем eligibility в вес
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

    for (int i = 0; i < size; ++i) {
        double error_i = phi[i] - target[i];
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

            // Ограничение
            W_intra[i][j] = std::clamp(
                W_intra[i][j],
                -static_cast<double>(params.maxWeight),
                static_cast<double>(params.maxWeight)
            );
            W_intra[j][i] = W_intra[i][j];
        }
    }
    int idx = 0;
        for (int i = 0; i < size; ++i)
            for (int j = i + 1; j < size; ++j)
                synapses[idx++].weight = static_cast<float>(W_intra[i][j]);
}