#include "NeuralGroup.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

NeuralGroup::NeuralGroup(int size, double dt, std::mt19937& rng)
    : size(size),
      dt(dt),
      learningRate(0.01),
      threshold(0.5),
      phi(size, 0.0),
      pi(size, 0.0),
      dH(size, 0.0),
      W_intra(size, std::vector<double>(size, 0.0)),
      specialization("unspecialized")
{
    initializeRandom(rng);
}

void NeuralGroup::initializeRandom(std::mt19937& rng) {
    std::uniform_real_distribution<double> dist(-0.1, 0.1);
    for (int i = 0; i < size; ++i) {
        phi[i] = dist(rng);
        pi[i] = dist(rng) * 0.1;
        for (int j = i + 1; j < size; ++j) {
            double w = dist(rng) * 0.02;
            W_intra[i][j] = w;
            W_intra[j][i] = w; // симметрия
        }
    }
    // нормируем, чтобы избежать взрывов
    for (int i = 0; i < size; ++i) {
        W_intra[i][i] = 0.0;
    }
}

void NeuralGroup::evolve() {
    // Первая половина шага: обновление phi
    for (int i = 0; i < size; ++i) {
        phi[i] += dt * pi[i];
    }

    // Вычисление производных
    computeDerivatives();

    // Вторая половина шага: обновление pi
    for (int i = 0; i < size; ++i) {
        pi[i] -= dt * dH[i];
    }

    // Применяем пороговую функцию активации к phi (ограничение активности)
    for (int i = 0; i < size; ++i) {
        phi[i] = activationFunction(phi[i]);
    }
}

void NeuralGroup::computeDerivatives() {
    // Упрощённая модель: dH = m*phi + lambda*phi^2 + сумма взаимодействий
    // Здесь m и lam могут быть общими для всех групп, либо индивидуальными.
    // Используем фиксированные значения (как в исходной symplecticEvolution).
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
    // Сигмоида с порогом: 1/(1+exp(-(x - threshold)/temp))
    const double temp = 0.1;
    return 1.0 / (1.0 + std::exp(-(x - threshold) / temp));
}

void NeuralGroup::learnHebbian(double globalReward) {
    // Простое правило Хебба с учётом глобальной награды (подкрепление)
    for (int i = 0; i < size; ++i) {
        for (int j = i + 1; j < size; ++j) {
            double hebb = phi[i] * phi[j];
            double update = learningRate * (hebb + globalReward * 0.1); // награда модулирует
            W_intra[i][j] += update;
            W_intra[j][i] = W_intra[i][j];

            // ограничение весов
            const double maxW = 0.3;
            if (W_intra[i][j] > maxW) W_intra[i][j] = maxW;
            if (W_intra[i][j] < -maxW) W_intra[i][j] = -maxW;
            W_intra[j][i] = W_intra[i][j];
        }
    }
}

double NeuralGroup::getAverageActivity() const {
    double sum = 0.0;
    for (double val : phi) sum += val;
    return sum / size;
}