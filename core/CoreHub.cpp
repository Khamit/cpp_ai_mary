#include "CoreHub.hpp"
#include <algorithm>

CoreHub::CoreHub(int numHubs) : hubs(numHubs, nullptr), 
                                 hubWeights(numHubs, std::vector<float>(numHubs, 0.01f)),
                                 hubInputs(numHubs, 0.0f) {}

void CoreHub::connectToGroups(const std::vector<NeuralGroup>& groups, const std::vector<int>& hubIndices) {
    hubs.clear();
    for (int idx : hubIndices) {
        if (idx >= 0 && idx < groups.size()) {
            hubs.push_back(const_cast<NeuralGroup*>(&groups[idx]));
        }
    }
}

void CoreHub::integrate(std::vector<NeuralGroup>& groups) {
    if (hubs.empty()) return;
    
    // Собираем входы от хабов
    for (size_t i = 0; i < hubs.size(); ++i) {
        hubInputs[i] = hubs[i]->getAverageActivity();
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
        auto& phi = hubs[i]->getPhiNonConst();
        float avg = newInputs[i];
        for (size_t n = 0; n < phi.size(); ++n) {
            phi[n] = phi[n] * 0.9f + avg * 0.1f;
        }
    }
}

void CoreHub::learnSTDP(float reward,int currentStep) {
    for (auto* hub : hubs) {
        if (hub) hub->learnSTDP(reward, currentStep);
    }
}