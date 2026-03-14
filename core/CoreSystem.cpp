// core/CoreSystem.cpp
#include "CoreSystem.hpp"
#include <iostream>
#include "MaryDefense.hpp"

CoreSystem::CoreSystem() 
    : neuralSystem(std::make_unique<NeuralFieldSystem>(0.001)) {
    std::cout << "CoreSystem created" << std::endl;
}

CoreSystem::~CoreSystem() {
    shutdown();
}

bool CoreSystem::initialize(const std::string& configFile) {
    std::cout << "CoreSystem initializing with config: " << configFile << std::endl;
    
    // 1. Детектируем устройство
    deviceInfo = DeviceProbe::detect();
    capabilities = DeviceProbe::buildCapabilities(deviceInfo);
    
    std::cout << DeviceProbe::describe(deviceInfo) << std::endl;
    
    // 2. Создаём нейросистему с EventSystem
    neuralSystem = std::make_unique<NeuralFieldSystem>(0.001, eventSystem);
    std::mt19937 rng(std::random_device{}());
    neuralSystem->initializeRandom(rng);
    
    // 3. Определяем, мать мы или дочь (MaryDefense)
    lineage = MaryDefense::boot(rng);
    
    // 4. Загружаем модули, соответствующие устройству
    loadModulesForDevice();
    
    return true;
}

void CoreSystem::loadModulesForDevice() {
    // Загружаем модули на основе возможностей устройства
    /*
    for (const auto& cap : capabilities) {
        if (cap.name == "camera" && !getComponent<VisionModule>("vision")) {
            // registerComponent<VisionModule>("vision", ...);
            std::cout << "📷 Загружен модуль vision для камеры" << std::endl;
        }
        if (cap.name == "microphone" && !getComponent<SpeechModule>("speech")) {
            // registerComponent<SpeechModule>("speech", ...);
            std::cout << "🎤 Загружен модуль speech для микрофона" << std::endl;
        }
        if (cap.name == "motor" && !getComponent<MotionModule>("motion")) {
            // registerComponent<MotionModule>("motion", ...);
            std::cout << "🤖 Загружен модуль motion для мотора" << std::endl;
        }
    }
    */
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
    lineage->save(memory);
    for (auto& component : components) component->saveState(memory);
    memory.saveAll();
}

void CoreSystem::loadState() {
    memory.loadAll();
    lineage->load(memory);
    for (auto& component : components) component->loadState(memory);
}