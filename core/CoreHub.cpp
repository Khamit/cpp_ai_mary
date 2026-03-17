// CoreHub.cpp - ИСПРАВЛЕННЫЙ
#include "CoreHub.hpp"
#include "NeuralGroup.hpp"
#include <algorithm>

CoreHub::CoreHub(int numHubs) : 
    hubs(),
    hubWeights(numHubs, std::vector<float>(numHubs, 0.01f)),
    hubInputs(numHubs, 0.0f) {}

void CoreHub::connectToGroups(INeuralGroupAccess& system) {
    // Получаем указатели на группы-хабы через интерфейс
    hubs = system.getHubGroups();  // теперь это неконстантные указатели
    
    // Инициализируем веса если нужно
    if (hubs.size() != hubWeights.size()) {
        hubWeights.resize(hubs.size(), std::vector<float>(hubs.size(), 0.01f));
        hubInputs.resize(hubs.size(), 0.0f);
    }
}

void CoreHub::integrate(INeuralGroupAccess& system) {
    if (hubs.empty()) return;
    
    // Обновляем указатели на случай, если группы изменились
    hubs = system.getHubGroups();
    
    // Собираем входы от хабов с учетом их энтропии
    for (size_t i = 0; i < hubs.size(); ++i) {
        if (!hubs[i]) continue;
        hubInputs[i] = hubs[i]->getAverageActivity();
        
        double entropy = hubs[i]->computeEntropy();
        double entropy_factor = 1.0 + (entropy - 2.0) * 0.1;
        hubInputs[i] *= static_cast<float>(entropy_factor);
    }
    
    // Обмен между хабами
    std::vector<float> newInputs(hubs.size(), 0.0f);
    for (size_t i = 0; i < hubs.size(); ++i) {
        for (size_t j = 0; j < hubs.size(); ++j) {
            newInputs[i] += hubWeights[i][j] * hubInputs[j];
        }
    }
    
    // Применяем к хабам
    for (size_t i = 0; i < hubs.size(); ++i) {
        if (!hubs[i]) continue;
        auto& phi = hubs[i]->getPhiNonConst();
        float avg = newInputs[i];
        for (size_t n = 0; n < phi.size(); ++n) {
            phi[n] = phi[n] * 0.9f + avg * 0.1f;
        }
    }
}

void CoreHub::learnSTDP(float reward, int currentStep) {
    for (auto* hub : hubs) {
        if (hub) hub->learnSTDP(reward, currentStep);
    }
}