// SystemHealthMonitor.hpp
#pragma once
#include "NeuralFieldSystem.hpp"
#include <random>

class SystemHealthMonitor {
public:
    void checkHealth(NeuralFieldSystem& system) {
        auto& groups = system.getGroups();
        
        // Проверяем "мертвые" нейроны
        int dead_neurons = 0;
        int total_neurons = 0;
        
        for (auto& group : groups) {
            for (int i = 0; i < group.getSize(); i++) {
                total_neurons++;
                
                // Если нейрон в сингулярности слишком долго
                if (group.getOrbitLevel(i) == 0 && 
                    group.getTimeOnOrbit(i) > 3600) {  // час в сингулярности
                    dead_neurons++;
                    
                    // Оживляем с 10% шансом
                    static std::mt19937 rng(std::random_device{}());
                    if (std::uniform_int_distribution<>(0, 99)(rng) < 10) {
                        std::cout << "Reviving dead neuron " << i << std::endl;
                        group.publicPromoteToBaseOrbit(i);  // используем публичный метод
                    }
                }
            }
        }
        
        // Если слишком много мертвых - общая стимуляция
        if (dead_neurons > total_neurons * 0.3) {  // >30% мертвы
            stimulateSystem(system);
        }
    }
    
private:
    void stimulateSystem(NeuralFieldSystem& system) {
        std::cout << "System stimulation activated - too many dead neurons" << std::endl;
        
        // Создаем случайную активность
        std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<double> dist(0.2, 0.5);
        
        for (auto& group : system.getGroups()) {
            auto& phi = group.getPhiNonConst();
            for (auto& val : phi) {
                val = dist(rng);
            }
        }
    }
};