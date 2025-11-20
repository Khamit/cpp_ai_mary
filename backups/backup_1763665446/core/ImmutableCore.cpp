#include "ImmutableCore.hpp"
#include <iostream>

ImmutableCore::ImmutableCore() {
    std::cout << "ImmutableCore initialized - core functions sealed" << std::endl;
}

bool ImmutableCore::requestPermission(const std::string& action) const {
    std::cout << "🔒 PERMISSION REQUESTED for: " << action << std::endl;
    
    // Автоматически разрешаем безопасные действия для тестирования
    if (action == "exit_stasis" || action == "minimal_mutation") {
        std::cout << "✅ AUTO-APPROVED: " << action << std::endl;
        return true;
    }
    
    // Для тестирования автоматически разрешаем все
    std::cout << "✅ AUTO-APPROVED for testing: " << action << std::endl;
    return true;
}

double ImmutableCore::computeEnergyConservation(const NeuralFieldSystem& system) const {
    return system.computeTotalEnergy();
}

bool ImmutableCore::validatePhysicalLaws(const NeuralFieldSystem& system) const {
    double energy = computeEnergyConservation(system);
    return energy >= 0 && energy < 1000;
}