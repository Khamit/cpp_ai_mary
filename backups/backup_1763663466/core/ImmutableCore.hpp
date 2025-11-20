#pragma once
#include <string>
#include <functional>
#include "NeuralFieldSystem.hpp"

class ImmutableCore {
public:
    ImmutableCore();
    
    // Запечатанная функция запроса разрешения
    bool requestPermission(const std::string& action) const;
    
    // Константы системы
    const double MINIMAL_ENERGY_THRESHOLD = 0.0001;
    const size_t MAX_CODE_SIZE = 100000; // 100KB
    const double MAX_CPU_USAGE = 0.7; // 70%
    
    // Неизменяемые физические законы
    double computeEnergyConservation(const NeuralFieldSystem& system) const;
    bool validatePhysicalLaws(const NeuralFieldSystem& system) const;

private:
    // В реальной системе здесь могут быть более сложные проверки
};