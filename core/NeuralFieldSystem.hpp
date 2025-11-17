//cpp_ai_test/core/NeuralFieldSystem.hpp
#pragma once
#include <vector>
#include <random>
#include <string>

class NeuralFieldSystem {
public:
    // Конструктор и базовая инициализация
    NeuralFieldSystem(int Nside, double dt, double m, double lam);
    
    // Основные методы ядра (неизменяемые)
    void initializeRandom(std::mt19937& rng, double phi_range, double w_range);
    void symplecticEvolution();
    double computeTotalEnergy() const;
    
    // Геттеры для доступа к данным (для модулей)
    std::vector<double>& getPhi() { return phi; }
    std::vector<double>& getPi() { return pi; }
    std::vector<std::vector<double>>& getWeights() { return W; }
    
    // Константные геттеры для чтения
    const std::vector<double>& getPhi() const { return phi; }
    const std::vector<double>& getPi() const { return pi; }
    const std::vector<std::vector<double>>& getWeights() const { return W; }
    
    // Сеттеры для модулей
    void setPhi(int index, double value) { phi[index] = value; }
    void setPi(int index, double value) { pi[index] = value; }
    
    // Параметры системы
    const int Nside;
    const int N;
    const double dt;
    const double m;
    const double lam;

private:
    std::vector<double> phi, pi, dH;
    std::vector<std::vector<double>> W;
    
    // Вспомогательные методы ядра
    double computeLocalEnergy(int i) const;
    void computeDerivatives();
};