//cpp_ai_test/core/NeuralFieldSystem.cpp
#include "NeuralFieldSystem.hpp"
#include <cmath>

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
    double V = 0.5 * m * m * phi[i] * phi[i] + (lam / 6.0) * phi[i] * phi[i] * phi[i];
    double inter = 0.0;
    for (int j = 0; j < N; j++) {
        inter += 0.5 * W[i][j] * (phi[i] - phi[j]) * (phi[i] - phi[j]);
    }
    return 0.5 * pi[i] * pi[i] + V + inter;
}

double NeuralFieldSystem::computeTotalEnergy() const {
    double total = 0.0;
    for (int i = 0; i < N; i++) {
        total += computeLocalEnergy(i);
    }
    return total / N;
}