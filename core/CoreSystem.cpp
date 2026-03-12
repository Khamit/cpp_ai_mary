// core/CoreSystem.cpp
#include "CoreSystem.hpp"
#include <iostream>

CoreSystem::CoreSystem() 
    : neuralSystem(std::make_unique<NeuralFieldSystem>(0.001)) {
    std::cout << "CoreSystem created" << std::endl;
}

CoreSystem::~CoreSystem() {
    shutdown();
}

bool CoreSystem::initialize(const std::string& configFile) {
    std::cout << "CoreSystem initializing with config: " << configFile << std::endl;
    std::mt19937 rng(42);
    neuralSystem->initializeRandom(rng);
    return true;
}

void CoreSystem::shutdown() {
    std::cout << "CoreSystem shutting down" << std::endl;
    for (auto& component : components) {
        component->shutdown();
    }
    components.clear();
    componentRegistry.clear();
}

void CoreSystem::update(float dt) {
    // Обновляем нейросистему с передачей номера шага
    static int stepCounter = 0;
    neuralSystem->step(0.0f, stepCounter++);
    
    // НОВОЕ: мониторинг энтропии
    static int entropyCheckCounter = 0;
    if (++entropyCheckCounter % 100 == 0) {
        double entropy = neuralSystem->computeSystemEntropy();
        std::cout << "System entropy: " << entropy << std::endl;
        
        // Сохраняем высокоэнтропийные состояния в память
        if (entropy > 3.0) {  // порог можно настроить
            auto features = neuralSystem->getFeatures();
            memory.storeWithEntropy("system", features, entropy, 0.8f);
        }
    }
    
    // Обновляем все компоненты
    for (auto& component : components) {
        component->update(dt);
    }
}

void CoreSystem::saveState() {
    for (auto& component : components) {
        component->saveState(memory);
    }
    memory.saveAll();
}

void CoreSystem::loadState() {
    memory.loadAll();
    for (auto& component : components) {
        component->loadState(memory);
    }
}