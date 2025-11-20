//cpp_ai_test/modules/LearningModule.cpp
#include "LearningModule.hpp"
#include <cmath>

void LearningModule::applyLearning(NeuralFieldSystem& system) {
    if (!config.enabled) return;
    
    if (config.rule == "hebbian") {
        applyHebbianLearning(system);
    } else if (config.rule == "oja") {
        applyOjaLearning(system);
    }
}

void LearningModule::applyHebbianLearning(NeuralFieldSystem& system) {
    auto& W = system.getWeights();
    const auto& phi = system.getPhi();
    
    for (int i = 0; i < system.N; i++) {
        for (int j = i + 1; j < system.N; j++) {
            double update = config.learning_rate * phi[i] * phi[j];
            W[i][j] = W[i][j] * config.weight_decay + update;
            W[j][i] = W[i][j];
            
            // Ограничение весов
            if (W[i][j] > config.max_weight) W[i][j] = config.max_weight;
            if (W[i][j] < config.min_weight) W[i][j] = config.min_weight;
            W[j][i] = W[i][j];
        }
    }
}

void LearningModule::applyOjaLearning(NeuralFieldSystem& system) {
    // Реализация правила Oii для нормализации
    auto& W = system.getWeights();
    const auto& phi = system.getPhi();
    
    for (int i = 0; i < system.N; i++) {
        for (int j = i + 1; j < system.N; j++) {
            double update = config.learning_rate * (phi[i] * phi[j] - phi[i] * phi[i] * W[i][j]);
            W[i][j] = W[i][j] * config.weight_decay + update;
            W[j][i] = W[i][j];
            
            if (W[i][j] > config.max_weight) W[i][j] = config.max_weight;
            if (W[i][j] < config.min_weight) W[i][j] = config.min_weight;
            W[j][i] = W[i][j];
        }
    }
}