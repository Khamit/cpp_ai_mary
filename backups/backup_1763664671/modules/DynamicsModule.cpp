//cpp_ai_test/modules/DynamicsModule.cpp
#include "DynamicsModule.hpp"

void DynamicsModule::applyDynamics(NeuralFieldSystem& system) {
    if (!config.enabled) return;
    
    auto& phi = system.getPhi();
    auto& pi = system.getPi();
    
    if (config.damping_enabled) {
        for (int i = 0; i < system.N; i++) {
            pi[i] *= config.damping_factor;
        }
    }
    
    if (config.limits_enabled) {
        for (int i = 0; i < system.N; i++) {
            if (phi[i] > config.max_phi) phi[i] = config.max_phi;
            if (phi[i] < config.min_phi) phi[i] = config.min_phi;
            if (pi[i] > config.max_pi) pi[i] = config.max_pi;
            if (pi[i] < config.min_pi) pi[i] = config.min_pi;
        }
    }
}