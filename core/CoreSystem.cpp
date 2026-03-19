// core/CoreSystem.cpp
#include "CoreSystem.hpp"
#include <iostream>
#include "MaryDefense.hpp"

CoreSystem::CoreSystem() 
    : neuralSystem(std::make_unique<NeuralFieldSystem>(0.001, eventSystem)) {  
        // ВАЖНО: передаем eventSystem
    std::cout << "CoreSystem created" << std::endl;
}

CoreSystem::~CoreSystem() {
    shutdown();
}

bool CoreSystem::initialize(const std::string& configFile) {
    std::cout << "CoreSystem initializing with config: " << configFile << std::endl;

        // Загружаем конфиг (уже есть через ConfigLoader)
    Config config;
    if (config.loadFromFile(configFile)) {
        system_mode = config.get<std::string>("system.mode", "personal");
        default_user = config.get<std::string>("system.user_name", "user");
    }

        // Загружаем режим
    system_mode = config.get<std::string>("system.mode", "personal");
    default_user = config.get<std::string>("system.user_name", "user");
    
    // Создаем нужную реализацию авторизации
    if (isEnterpriseMode()) {
        // Для enterprise используем полную логику
        personnel_db = PersonnelDatabase();  // если нужно создать
        // ... загружаем данные
        auth = std::make_unique<EnterpriseAuth>(personnel_db, master_key_mgr);
        std::cout << "Enterprise mode: full access control enabled" << std::endl;
    } else {
        // Для personal - упрощенная версия
        auth = std::make_unique<PersonalAuth>(default_user);
        std::cout << "Personal mode: simplified access (user: " << default_user << ")" << std::endl;
    }
    
    std::cout << "(!) System mode: " << system_mode << std::endl;
    
    // 1. Детектируем устройство
    deviceInfo = DeviceProbe::detect();
    capabilities = DeviceProbe::buildCapabilities(deviceInfo);
    
    std::cout << DeviceProbe::describe(deviceInfo) << std::endl;
    
    // 2. Инициализируем нейросистему (уже создана в конструкторе)
    std::mt19937 rng(std::random_device{}());

        // НОВОЕ: настраиваем лимиты массы под железо
    MassLimits mass_limits;
    if (deviceInfo.resources.ram_mb < 700) {  // меньше 700MB
        mass_limits = MassRecommendations::forLowMemory();
        std::cout << "Low memory mode: mass limit 2.0" << std::endl;
    } else if (deviceInfo.resources.ram_mb < 1500) {  // меньше 1.5GB
        mass_limits = MassRecommendations::forMediumMemory();
        std::cout << "Medium memory mode: mass limit 3.5" << std::endl;
    } else {
        mass_limits = MassRecommendations::forHighMemory();
        std::cout << "High performance mode: mass limit 5.0" << std::endl;
    }
    
    // Передаем лимиты в нейросистему (нужно будет модифицировать NeuralFieldSystem)
    neuralSystem->initializeWithLimits(rng, mass_limits);
    
    // 3. Определяем, мать мы или дочь (MaryDefense)
    lineage = MaryDefense::boot(rng);
    
    // 4. Регистрируем языковой модуль (теперь только для ввода/вывода)
    // ВАЖНО: LanguageModule теперь принимает только neuralSystem
    auto* language = registerComponent<LanguageModule>(
        "language", 
        *neuralSystem, 
        immutableCore, 
        *auth,
        nullptr  // SemanticGraphDatabase* graph = nullptr
    );
    if (language) {
        language->setSystemMode(system_mode, default_user);
        
        // Настраиваем поведение в зависимости от режима
        if (system_mode == "personal") {
            language->setCommandEnforcement(false);   // не блокируем команды
            language->setEmotionalResponses(true);    // добавляем эмоции
        } else {
            language->setCommandEnforcement(true);    // строгая проверка
            language->setEmotionalResponses(false);   // формальные ответы
        }
        
        std::cout << "LanguageModule configured for " << system_mode << " mode" << std::endl;
    }
    // 5. Загружаем остальные модули, соответствующие устройству
    loadModulesForDevice();
    
    return true;
}

void CoreSystem::activePerception() {
    /*
    // 1. Система решает, что исследовать
    auto curiosity = neuralSystem->getCuriositySignal();
    
    // 2. Активирует соответствующие сенсоры
    if (curiosity > 0.7) {
        activateCamera();
        activateMicrophone();
    }
    
    // 3. Получает новые данные
    auto new_data = sense();
    
    // 4. Обучается на них
    neuralSystem->learn(new_data);
    */
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
    // Сохраняем состояние ядра
    memory.saveAll(); 
    // Передаем менеджер компонентам, чтобы они могли его использовать
    for (auto& component : components) {
        component->saveState(memory); // Единая точка входа
    }
    lineage->save(memory);
}

void CoreSystem::loadState() {
    memory.loadAll();
    lineage->load(memory);
    for (auto& component : components) {
        component->loadState(memory);
    }
}