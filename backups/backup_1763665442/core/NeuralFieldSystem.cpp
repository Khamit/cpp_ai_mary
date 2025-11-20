//cpp_ai_test/core/NeuralFieldSystem.cpp
#include "NeuralFieldSystem.hpp"
#include <cmath>
#include <iostream>

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
        total += computeLocalEnergy(i);
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