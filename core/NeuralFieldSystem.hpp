//cpp_ai_test/core/NeuralFieldSystem.hpp
#pragma once
#include <vector>
#include <random>
#include <string>

class NeuralFieldSystem {
public:
    // Получить вектор признаков для памяти (размер 64)
    std::vector<float> getFeatures() const;
    
    // Константа для количества нейронов - сделаем 1024 (32x32)
    static constexpr int DEFAULT_NSIDE = 32; // 32*32 = 1024 нейрона
    // подмножество для языкового модуля
    static constexpr int LANGUAGE_NEURONS = 200; // количество нейронов для языка
    static constexpr int LANGUAGE_START = 0;      // начальный индекс
    static constexpr int LANGUAGE_END = LANGUAGE_NEURONS; // 0-199
    
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

    // Метод для целевой мутации
    void applyTargetedMutation(double mutation_strength, int target_type);
    
    // Метод для энергоэффективного режима
    void enterLowPowerMode();

    // Метод для обучения языковых весов (только для выделенного диапазона)
    void learnLanguageHebbian(int startIdx, int endIdx, double lr, double decay);

private:
    std::vector<double> phi, pi, dH;
    std::vector<std::vector<double>> W;
    
    // Вспомогательные методы ядра
    double computeLocalEnergy(int i) const;
    void computeDerivatives();
};