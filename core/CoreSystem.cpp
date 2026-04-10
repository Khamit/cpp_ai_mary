#include "CoreSystem.hpp"
#include <iostream>
#include <filesystem>
#include "MaryDefense.hpp"
#include "../modules/lang/LanguageModule.hpp"
#include "../modules/MetaCognitiveModule.hpp"
#include "../modules/learning/CuriosityDriver.hpp"
#include <random>

CoreSystem::CoreSystem() 
    : neuralSystem(std::make_unique<NeuralFieldSystem>(0.001, eventSystem))
{
    std::cout << "CoreSystem created" << std::endl;
}

CoreSystem::~CoreSystem() {
    std::cout << "CoreSystem destroyed" << std::endl;
}

void CoreSystem::loadConfig(const std::string& configFile) {
    Config config;
    if (config.loadFromFile(configFile)) {
        system_mode = config.get<std::string>("system.mode", "personal");
        default_user = config.get<std::string>("system.user_name", "user");
    }
}

bool CoreSystem::initialize(const std::string& configFile) {
    std::cout << "CoreSystem initializing with config: " << configFile << std::endl;

    // 1. Загружаем конфиг
    loadConfig(configFile);
    
    // 2. Создаём авторизацию
    if (isEnterpriseMode()) {
        personnel_db = PersonnelDatabase();
        auth = std::make_unique<EnterpriseAuth>(personnel_db, master_key_mgr);
        std::cout << "Enterprise mode: full access control enabled" << std::endl;
    } else {
        auth = std::make_unique<PersonalAuth>(default_user);
        std::cout << "Personal mode: simplified access (user: " << default_user << ")" << std::endl;
    }
    
    // 3. Детектируем устройство
    deviceInfo = DeviceProbe::detect();
    capabilities = DeviceProbe::buildCapabilities(deviceInfo);
    std::cout << DeviceProbe::describe(deviceInfo) << std::endl;
    
    // 4. Инициализируем нейросистему
    std::mt19937 rng(std::random_device{}());
    
    MassLimits mass_limits;
    if (deviceInfo.resources.ram_mb < 700) {
        mass_limits = MassRecommendations::forLowMemory();
        std::cout << "Low memory mode: mass limit 2.0" << std::endl;
    } else if (deviceInfo.resources.ram_mb < 1500) {
        mass_limits = MassRecommendations::forMediumMemory();
        std::cout << "Medium memory mode: mass limit 3.5" << std::endl;
    } else {
        mass_limits = MassRecommendations::forHighMemory();
        std::cout << "High performance mode: mass limit 5.0" << std::endl;
    }
    
    neuralSystem->initializeWithLimits(rng, mass_limits);
    
    // 5. Создаём lineage
    lineage = MaryDefense::boot(rng);
    
    auto* language = registerComponent<LanguageModule>(
        "language", *neuralSystem, immutableCore, *auth, personnel_db);
    
    if (language) {
        language->setSystemMode(system_mode, default_user);
        
        if (system_mode == "personal") {
            language->setCommandEnforcement(false);
            language->setEmotionalResponses(true);
        } else {
            language->setCommandEnforcement(true);
            language->setEmotionalResponses(false);
        }
        
        auto curiosity = language->createCuriosityDriver();
        if (curiosity) {
            language->setCuriosityDriver(curiosity);
        }
    }
    
    auto* metacog = registerComponent<MetaCognitiveModule>("metacognition", *neuralSystem);
    
    loadModulesForDevice();
    
    std::cout << "CoreSystem initialization complete!" << std::endl;
    return true;
}

void CoreSystem::loadModulesForDevice() {
    for (const auto& cap : capabilities) {
        if (cap.name == "camera" && !getComponent<Component>("vision")) {
            std::cout << "📷 Camera capability detected" << std::endl;
        }
        if (cap.name == "microphone" && !getComponent<Component>("speech")) {
            std::cout << "🎤 Microphone capability detected" << std::endl;
        }
        if (cap.name == "motor" && !getComponent<Component>("motion")) {
            std::cout << "🤖 Motor capability detected" << std::endl;
        }
    }
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
    static int stepCounter = 0;
    neuralSystem->step(0.0f, stepCounter++);
    
    for (auto& component : components) {
        component->update(dt);
    }
}