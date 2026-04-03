// core/CoreSystem.cpp
#include "CoreSystem.hpp"
#include <iostream>
#include <filesystem>
#include "MaryDefense.hpp"
#include "../modules/EvolutionModule.hpp"
#include "../modules/lang/LearningOrchestrator.hpp"
#include "../modules/lang/LanguageModule.hpp"
#include "../modules/MetaCognitiveModule.hpp"
#include "../modules/learning/CuriosityDriver.hpp"

#include <mach/mach.h>  
#include <sys/sysctl.h> 
#ifdef __linux__
#include <unistd.h>
#endif

CoreSystem::CoreSystem() 
    : neuralSystem(std::make_unique<NeuralFieldSystem>(0.001, eventSystem))
    , semantic_graph()  // инициализация графа
{
    std::cout << "CoreSystem created" << std::endl;
}

float CoreSystem::getCurrentCPUUsage() const {
#ifdef __APPLE__
    // macOS: используем host_statistics
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    host_cpu_load_info_data_t cpu_load;
    kern_return_t kr = host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
                                       (host_info_t)&cpu_load, &count);
    if (kr == KERN_SUCCESS) {
        natural_t total = cpu_load.cpu_ticks[CPU_STATE_USER] +
                          cpu_load.cpu_ticks[CPU_STATE_SYSTEM] +
                          cpu_load.cpu_ticks[CPU_STATE_IDLE] +
                          cpu_load.cpu_ticks[CPU_STATE_NICE];
        natural_t active = total - cpu_load.cpu_ticks[CPU_STATE_IDLE];
        return static_cast<float>(active) / static_cast<float>(total);
    }
#elif defined(__linux__)
    // Linux: читаем /proc/stat
    std::ifstream stat("/proc/stat");
    std::string line;
    if (std::getline(stat, line)) {
        long user, nice, system, idle;
        sscanf(line.c_str(), "cpu %ld %ld %ld %ld", &user, &nice, &system, &idle);
        long total = user + nice + system + idle;
        static long prev_total = 0, prev_idle = 0;
        long diff_total = total - prev_total;
        long diff_idle = idle - prev_idle;
        prev_total = total;
        prev_idle = idle;
        if (diff_total > 0) {
            return 1.0f - static_cast<float>(diff_idle) / static_cast<float>(diff_total);
        }
    }
#endif
    return 0.5f;  // fallback
}

float CoreSystem::getCurrentRAMUsage() const {
#ifdef __APPLE__
    // macOS: используем mach_task_basic_info
    struct task_basic_info info;
    mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
    kern_return_t kr = task_info(mach_task_self(), TASK_BASIC_INFO,
                                 (task_info_t)&info, &count);
    if (kr == KERN_SUCCESS) {
        uint64_t ram_used_mb = info.resident_size / (1024 * 1024);
        return static_cast<float>(ram_used_mb) / static_cast<float>(deviceInfo.resources.ram_mb);
    }
#elif defined(__linux__)
    // Linux: читаем /proc/self/status
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            long ram_kb;
            sscanf(line.c_str(), "VmRSS: %ld kB", &ram_kb);
            float ram_mb = ram_kb / 1024.0f;
            return ram_mb / static_cast<float>(deviceInfo.resources.ram_mb);
        }
    }
#endif
    return 0.5f;  // fallback
}

void CoreSystem::applyResourcePenalty(float penalty) {
    // Передаём штраф в нейросистему через специальный метод
    // Можно добавить в NeuralFieldSystem метод applyGlobalPenalty
    // Пока просто логируем при высоком потреблении
    if (penalty < -0.3f) {  // значительный штраф
        std::cout << "⚠️ Resource penalty: " << penalty << std::endl;
        // В будущем: neuralSystem->applyResourcePenalty(penalty);
    }
}

void CoreSystem::initializeSemanticGraph() {
    // Заполняем граф базовыми понятиями
    std::cout << "Initializing semantic graph with basic concepts..." << std::endl;
    
    // Пример базовых понятий (можно загрузить из файла)
    // semantic_graph.addNode(...);
    
    std::cout << "Semantic graph initialized" << std::endl;
}

CoreSystem::~CoreSystem() {
    shutdown();
}

bool CoreSystem::initialize(const std::string& configFile) {
    std::cout << "CoreSystem initializing with config: " << configFile << std::endl;

    // Загружаем конфиг
    Config config;
    if (config.loadFromFile(configFile)) {
        system_mode = config.get<std::string>("system.mode", "personal");
        default_user = config.get<std::string>("system.user_name", "user");
    }

    // Создаем нужную реализацию авторизации
    if (isEnterpriseMode()) {
        personnel_db = PersonnelDatabase();
        auth = std::make_unique<EnterpriseAuth>(personnel_db, master_key_mgr);
        std::cout << "Enterprise mode: full access control enabled" << std::endl;
    } else {
        auth = std::make_unique<PersonalAuth>(default_user);
        std::cout << "Personal mode: simplified access (user: " << default_user << ")" << std::endl;
    }
    
    std::cout << "(!) System mode: " << system_mode << std::endl;
    
    // 1. Детектируем устройство
    deviceInfo = DeviceProbe::detect();
    capabilities = DeviceProbe::buildCapabilities(deviceInfo);
    
    std::cout << DeviceProbe::describe(deviceInfo) << std::endl;
    
    // 2. Инициализируем нейросистему
    std::mt19937 rng(std::random_device{}());

    // Настраиваем лимиты массы под железо
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
    
    // 3. Определяем, мать мы или дочь
    lineage = MaryDefense::boot(rng);
    
    // ===== ИНИЦИАЛИЗИРУЕМ СЕМАНТИЧЕСКИЙ ГРАФ =====
    // Загружаем граф из файла, если есть
    if (std::filesystem::exists("data/semantic_graph.bin")) {
        // Убираем проверку на bool, так как loadFromFile возвращает void
        semantic_graph.loadFromFile("data/semantic_graph.bin");
        std::cout << "Loaded semantic graph from data/semantic_graph.bin" << std::endl;
    } else {
        std::cout << "No semantic graph found, initializing empty" << std::endl;
        initializeSemanticGraph();
    }
    
  // ===== РЕГИСТРИРУЕМ МОДУЛИ В ПРАВИЛЬНОМ ПОРЯДКЕ =====
    
    // 1. EvolutionModule (не зависит от других)
    auto* evolution = registerComponent<EvolutionModule>(
        "evolution",
        immutableCore,
        EvolutionConfig{},
        memory
    );
    if (evolution) {
        std::cout << "EvolutionModule registered" << std::endl;
    }
    
    // 2. LanguageModule - ПЕРЕДАЕМ УКАЗАТЕЛЬ НА ГРАФ!
    auto* language = registerComponent<LanguageModule>(
        "language", 
        *neuralSystem, 
        immutableCore, 
        *auth,
        &semantic_graph  // передаем указатель на граф!
    );
    
    if (language) {
        language->setSystemMode(system_mode, default_user);
        
        if (system_mode == "personal") {
            language->setCommandEnforcement(false);
            language->setEmotionalResponses(true);
        } else {
            language->setCommandEnforcement(true);
            language->setEmotionalResponses(false);
        }
        
        std::cout << "LanguageModule configured for " << system_mode << " mode" << std::endl;
        
        // Убедимся, что словарь инициализирован
        language->initializeWordDictionary();
        
        // ← СОЗДАЁМ CURIOSITYDRIVER ОДИН РАЗ
        auto curiosity = language->createCuriosityDriver();
        if (curiosity) {
            language->setCuriosityDriver(curiosity);
            std::cout << "CuriosityDriver created and attached" << std::endl;
        } else {
            std::cerr << "WARNING: Failed to create CuriosityDriver!" << std::endl;
        }
        
    } else {
        std::cerr << "ERROR: Failed to create LanguageModule!" << std::endl;
    }
    
    // 3. LearningOrchestrator (зависит от LanguageModule)
    if (language) {
        auto& semantic_manager = language->getSemanticManager();
        
        auto* learning = registerComponent<LearningOrchestrator>(
            "learning",
            *neuralSystem,
            *language,
            semantic_manager,
            memory,
            *auth,
            deviceInfo,
            semantic_graph  // передаем ссылку на граф
        );
        
        if (learning) {
            std::cout << "LearningOrchestrator registered successfully!" << std::endl;
            std::cout << learning->getStats() << std::endl;
            
            // ← НЕ СОЗДАЁМ CURIOSITYDRIVER ВТОРОЙ РАЗ
            // Просто проверяем, что он уже есть
            if (!language->hasCuriosityDriver()) {
                std::cerr << "WARNING: CuriosityDriver not available for LearningOrchestrator!" << std::endl;
            }
        } else {
            std::cerr << "ERROR: Failed to create LearningOrchestrator!" << std::endl;
        }
    }
    // 4. MetaCognitiveModule
    auto* metacog = registerComponent<MetaCognitiveModule>(
        "metacognition", 
        *neuralSystem
    );
    if (metacog) {
        std::cout << "MetaCognitiveModule registered" << std::endl;
        
        // Связываем с EvolutionModule
        if (evolution) {
            metacog->setEvolutionModule(evolution);
        }
    }
    
    // 5. Загружаем остальные модули
    loadModulesForDevice();
    
    std::cout << "CoreSystem initialization complete!" << std::endl;
    return true;
}

void CoreSystem::activePerception() {
    // 1. Система решает, что исследовать
    // Вместо getCuriositySignal() используем энтропию системы
    double entropy = neuralSystem->computeSystemEntropy();
    
    // Нормализуем энтропию в диапазон [0,1] для сигнала любопытства
    float curiosity = static_cast<float>(std::min(1.0, entropy / 5.0));
    
    // 2. Активирует соответствующие сенсоры
    if (curiosity > 0.7f) {
        // activateCamera();
        // activateMicrophone();
        std::cout << "🔍 High curiosity (" << curiosity << "), activating sensors..." << std::endl;
    }
    
    // 3. Получает новые данные
    // auto new_data = sense();
    
    // 4. Обучается на них
    // neuralSystem->learn(new_data);
}

void CoreSystem::loadModulesForDevice() {
    // Загружаем модули на основе возможностей устройства
    for (const auto& cap : capabilities) {
        if (cap.name == "camera" && !getComponent<Component>("vision")) {
            // registerComponent<VisionModule>("vision", ...);
            std::cout << "📷 Загружен модуль vision для камеры" << std::endl;
        }
        if (cap.name == "microphone" && !getComponent<Component>("speech")) {
            // registerComponent<SpeechModule>("speech", ...);
            std::cout << "🎤 Загружен модуль speech для микрофона" << std::endl;
        }
        if (cap.name == "motor" && !getComponent<Component>("motion")) {
            // registerComponent<MotionModule>("motion", ...);
            std::cout << "🤖 Загружен модуль motion для мотора" << std::endl;
        }
    }
}

void CoreSystem::shutdown() {
    std::cout << "CoreSystem shutting down" << std::endl;
    
    // Сохраняем граф перед завершением
    semantic_graph.saveToFile("data/semantic_graph.bin");  // ← убрал if (!...)
    std::cout << "Semantic graph saved to data/semantic_graph.bin" << std::endl;
    
    for (auto& component : components) {
        component->shutdown();
    }
    components.clear();
    componentRegistry.clear();
}

void CoreSystem::update(float dt) {
    float cpu_usage = getCurrentCPUUsage();
    float ram_usage = getCurrentRAMUsage();

    // Обновляем нейросистему с передачей номера шага
    static int stepCounter = 0;
    neuralSystem->step(0.0f, stepCounter++);

    // Передаём "боль" в нейросистему
    float resource_penalty = -0.5f * (cpu_usage + ram_usage);
    neuralSystem->applyResourcePenalty(resource_penalty);
    
    // Мониторинг энтропии
    static int entropyCheckCounter = 0;
    if (++entropyCheckCounter % 100 == 0) {
        double entropy = neuralSystem->computeSystemEntropy();
        // std::cout << "System entropy: " << entropy << std::endl;
        
        // Сохраняем высокоэнтропийные состояния в память
        if (entropy > 3.0) {
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
    
    // Сохраняем семантический граф
    semantic_graph.saveToFile("data/semantic_graph.bin");
    
    // Передаем менеджер компонентам
    for (auto& component : components) {
        component->saveState(memory);
    }
    lineage->save(memory);
}

void CoreSystem::loadState() {
    memory.loadAll();
    
    // Загружаем семантический граф
    if (std::filesystem::exists("data/semantic_graph.bin")) {
        semantic_graph.loadFromFile("data/semantic_graph.bin");
    }
    
    lineage->load(memory);
    for (auto& component : components) {
        component->loadState(memory);
    }
}