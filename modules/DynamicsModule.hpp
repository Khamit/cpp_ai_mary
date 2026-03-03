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
    
    void applyDynamics(NeuralFieldSystem& system) {
        if (!config.enabled) return;
        
        // В новой архитектуре нужно работать через группы
        auto& groups = system.getGroups();
        
        if (config.damping_enabled) {
            for (auto& group : groups) {
                // Применяем демпфирование к каждой группе
                // Для простоты пока пропустим
            }
        }
        
        // Лимиты пока не применяем
    }
    
    void setConfig(const DynamicsConfig& newConfig) { config = newConfig; }

private:
    DynamicsConfig config;
};