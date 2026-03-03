#pragma once
#include "../core/NeuralFieldSystem.hpp"
#include <string>

struct LearningConfig {
    bool enabled = true;
    double learning_rate = 0.001;
    double weight_decay = 0.999;
    double max_weight = 0.1;
    double min_weight = -0.1;
    std::string rule = "hebbian";
};

class LearningModule {
public:
    LearningModule(const LearningConfig& config) : config(config) {}
    
    void applyLearning(NeuralFieldSystem& system) {
        if (!config.enabled) return;
        
        if (config.rule == "hebbian") {
            applyHebbianLearning(system);
        }
    }
    
    void setConfig(const LearningConfig& newConfig) { config = newConfig; }

private:
    LearningConfig config;
    
    void applyHebbianLearning(NeuralFieldSystem& system) {
        // В новой архитектуре обучение идет через межгрупповые связи
        // Для простоты пока пропускаем
    }
};