//cpp_ai_test/modules/DynamicsModule.hpp
#pragma once
#include "../core/NeuralFieldSystem.hpp"

struct DynamicsConfig {
    bool enabled = true;
    bool damping_enabled = true;
    double damping_factor = 0.999;
    bool limits_enabled = true;
    double max_phi = 2.0;
    double min_phi = -2.0;
    double max_pi = 10.0;
    double min_pi = -10.0;
};

class DynamicsModule {
public:
    DynamicsModule(const DynamicsConfig& config) : config(config) {}
    
    // Этот метод должен принимать НЕконстантную ссылку, так как изменяет систему
    void applyDynamics(NeuralFieldSystem& system);
    void setConfig(const DynamicsConfig& newConfig) { config = newConfig; }

private:
    DynamicsConfig config;
};