//cpp_ai_test/modules/LearningModule.hpp
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
    
    // Этот метод тоже принимает НЕконстантную ссылку
    void applyLearning(NeuralFieldSystem& system);
    void setConfig(const LearningConfig& newConfig) { config = newConfig; }

private:
    LearningConfig config;
    
    void applyHebbianLearning(NeuralFieldSystem& system);
    void applyOjaLearning(NeuralFieldSystem& system);
};