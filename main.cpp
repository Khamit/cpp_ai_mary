#include <SFML/Graphics.hpp>
#include <iostream>
#include <fstream>
#include <random>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iomanip>
// Подключаем новую архитектуру
// nen
#include "core/CoreSystem.hpp"
// Подключаем CoreSystem вместо отдельных компонентов? - yes we make central system to launch
#include "core/NeuralFieldSystem.hpp"
#include "modules/VisualizationModule.hpp"
#include "modules/InteractionModule.hpp"
#include "modules/StatisticsModule.hpp"
#include "modules/UIModule.hpp"
#include "core/ImmutableCore.hpp"
#include "modules/ResourceMonitor.hpp"
#include "modules/ConfigStructs.hpp"
#include "modules/MetaCognitiveModule.hpp"
#include "modules/lang/LanguageModule.hpp" 
#include "core/CoreHub.hpp"
#include "modules/learning/CuriosityDriver.hpp" 
//#include "core/MemoryManager.hpp" //- now they launch by CoreSystem
//#include "core/Component.hpp"
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>  // заголовок для STDERR_FILENO
#include <cstring>
#include <atomic>

std::atomic<bool> handling_signal(false);
std::atomic<bool> system_busy{false};

void handler(int sig) {
    // Защита от рекурсивных вызовов
    if (handling_signal.exchange(true)) {
        return;
    }
    
    void *array[20];
    size_t size;
    
    // Получить void* указатели для всех записей в стеке
    size = backtrace(array, 20);
    
    // Распечатать все фреймы в stderr
    fprintf(stderr, "\n\n=== CRASH DETECTED: signal %d (%s) ===\n", 
            sig, strsignal(sig));
    fprintf(stderr, "Stack trace:\n");
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    fprintf(stderr, "=====================================\n\n");
    
    // Попытаться сохранить состояние перед выходом
    // (но будьте осторожны - многое может быть небезопасно в обработчике сигнала)
    // warning!
    handling_signal = false;
    exit(1);
}



class ConfigLoader {
public:
    static bool loadFromFile(const std::string& filename, 
                            VisualizationConfig& visConfig,
                            InteractionConfig& interConfig,
                            UIConfig& uiConfig,
                            ResourceMonitorConfig& resConfig,
                            EvolutionConfig& evolConfig);
    
    static double getDoubleValue(const std::string& content, const std::string& key, double defaultValue);
    static int getIntValue(const std::string& content, const std::string& key, int defaultValue);
    static bool getBoolValue(const std::string& content, const std::string& key, bool defaultValue);
    static std::string getStringValue(const std::string& content, const std::string& key, const std::string& defaultValue);

private:
    static std::string extractValueString(const std::string& content, const std::string& key);
};

// Реализации методов ConfigLoader (остаются без изменений)
bool ConfigLoader::loadFromFile(const std::string& filename, 
                               VisualizationConfig& visConfig,
                               InteractionConfig& interConfig,
                               UIConfig& uiConfig,
                               ResourceMonitorConfig& resConfig,
                               EvolutionConfig& evolConfig) {
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "! Config file not found, using defaults: " << filename << std::endl;
        return false;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)), 
                       std::istreambuf_iterator<char>());
    file.close();
    
    
    visConfig.enabled = getBoolValue(content, "\"enabled\"", true);
    visConfig.dynamic_normalization = getBoolValue(content, "\"dynamic_normalization\"", true);
    visConfig.color_scheme = getStringValue(content, "\"color_scheme\"", "blue_red");
    visConfig.min_range = getDoubleValue(content, "\"min_range\"", 0.1);
    
    interConfig.enabled = getBoolValue(content, "\"enabled\"", true);
    interConfig.mouse_impact = getDoubleValue(content, "\"mouse_impact\"", 0.1);
    interConfig.local_spread = getBoolValue(content, "\"local_spread\"", true);
    interConfig.spread_radius = getIntValue(content, "\"spread_radius\"", 2);
    
    uiConfig.show_controls = getBoolValue(content, "\"show_controls\"", true);
    uiConfig.show_stats = getBoolValue(content, "\"show_stats\"", true);
    uiConfig.control_panel_width = getIntValue(content, "\"control_panel_width\"", 200);
    
    // Парсинг новых конфигураций
    resConfig.cpu_threshold = getDoubleValue(content, "\"cpu_threshold\"", 85.0);
    resConfig.memory_threshold = getDoubleValue(content, "\"memory_threshold\"", 90.0);
    resConfig.overload_debounce_seconds = getIntValue(content, "\"overload_debounce_seconds\"", 5);
    resConfig.max_overloads_per_minute = getIntValue(content, "\"max_overloads_per_minute\"", 2);
    resConfig.adaptive_thresholds = getBoolValue(content, "\"adaptive_thresholds\"", true);
    
    evolConfig.reduction_cooldown_seconds = getIntValue(content, "\"reduction_cooldown_seconds\"", 30);
    evolConfig.max_reductions_per_minute = getIntValue(content, "\"max_reductions_per_minute\"", 2);
    evolConfig.min_fitness_for_optimization = getDoubleValue(content, "\"min_fitness_for_optimization\"", 0.8);
    evolConfig.backup_on_improvement = getBoolValue(content, "\"backup_on_improvement\"", true);
    evolConfig.enable_adaptive_mutations = getBoolValue(content, "\"enable_adaptive_mutations\"", true);
    
    std::cout << " Full configuration loaded from: " << filename << std::endl;
    return true;
}

std::string ConfigLoader::extractValueString(const std::string& content, const std::string& key) {
    size_t pos = content.find(key);
    if (pos == std::string::npos) return "";
    
    size_t valueStart = content.find(':', pos) + 1;
    size_t valueEnd = content.find_first_of(",}\n", valueStart);
    if (valueEnd == std::string::npos) return "";
    
    return content.substr(valueStart, valueEnd - valueStart);
}

double ConfigLoader::getDoubleValue(const std::string& content, const std::string& key, double defaultValue) {
    std::string valueStr = extractValueString(content, key);
    if (valueStr.empty()) return defaultValue;
    
    try {
        valueStr.erase(std::remove_if(valueStr.begin(), valueStr.end(), ::isspace), valueStr.end());
        valueStr.erase(std::remove(valueStr.begin(), valueStr.end(), '\"'), valueStr.end());
        return std::stod(valueStr);
    } catch (...) {
        return defaultValue;
    }
}

int ConfigLoader::getIntValue(const std::string& content, const std::string& key, int defaultValue) {
    return static_cast<int>(getDoubleValue(content, key, defaultValue));
}

bool ConfigLoader::getBoolValue(const std::string& content, const std::string& key, bool defaultValue) {
    std::string valueStr = extractValueString(content, key);
    if (valueStr.empty()) return defaultValue;
    
    valueStr.erase(std::remove_if(valueStr.begin(), valueStr.end(), ::isspace), valueStr.end());
    valueStr.erase(std::remove(valueStr.begin(), valueStr.end(), '\"'), valueStr.end());
    
    return valueStr == "true";
}

std::string ConfigLoader::getStringValue(const std::string& content, const std::string& key, const std::string& defaultValue) {
    std::string valueStr = extractValueString(content, key);
    if (valueStr.empty()) return defaultValue;
    
    valueStr.erase(std::remove_if(valueStr.begin(), valueStr.end(), ::isspace), valueStr.end());
    valueStr.erase(std::remove(valueStr.begin(), valueStr.end(), '\"'), valueStr.end());
    
    return valueStr.empty() ? defaultValue : valueStr;
}

int main() {

    std::cout << "=== PROGRAM START ===" << std::endl;
    // Установить обработчик сигналов В САМОМ НАЧАЛЕ
    signal(SIGSEGV, handler);

    // Создаем папки если их нет
    std::filesystem::create_directories("dump");
    std::filesystem::create_directories("data");


    std::cout << "Step 1: Loading config..." << std::endl;
    
    // Загрузка полной конфигурации
    VisualizationConfig visConfig;
    InteractionConfig interConfig;
    UIConfig uiConfig;
    ResourceMonitorConfig resConfig;
    EvolutionConfig evolConfig;
    
    bool configLoaded = ConfigLoader::loadFromFile("config/system_config.json", 
                               visConfig, interConfig, uiConfig,
                              resConfig, evolConfig);

    // СОЗДАЕМ КОНФИГУРАЦИЮ ДЛЯ ПАРАМЕТРОВ
    Config systemConfig;
    if (configLoaded) {
        // Загружаем JSON в Config
        std::ifstream configFile("config/system_config.json");
        if (configFile.is_open()) {
            std::string content((std::istreambuf_iterator<char>(configFile)), 
                               std::istreambuf_iterator<char>());
            systemConfig = Config::fromJSON(content);
        }
    }

    // ОПРЕДЕЛЯЕМ РЕЖИМ ЗАПУСКА
    OperatingMode::Type startupMode = systemConfig.getStartupMode();
    std::cout << "Startup mode: " << OperatingMode::toString(startupMode) << std::endl;

    // ВЫБИРАЕМ ПАРАМЕТРЫ В ЗАВИСИМОСТИ ОТ РЕЖИМА
    Config::ModeLimits selectedLimits;  // ИСПРАВЛЕНО: используем Config::ModeLimits
    switch(startupMode) {
        case OperatingMode::TRAINING:
            selectedLimits = systemConfig.getTrainingLimits();
            break;
        case OperatingMode::NORMAL:
            selectedLimits = systemConfig.getNormalLimits();
            break;
        case OperatingMode::IDLE:
        case OperatingMode::SLEEP:
            selectedLimits = systemConfig.getIdleLimits();
            break;
    }
    // this made for develop - then we should create backend
    // Параметры системы
    double dt = 0.001;
    unsigned int windowWidth = 800;
    unsigned int windowHeight = 700;

    std::cout << "! Initializing Neural Field System with " 
              << NeuralFieldSystem::NUM_GROUPS << " groups × " 
              << NeuralFieldSystem::GROUP_SIZE << " neurons = "
              << NeuralFieldSystem::TOTAL_NEURONS << " total neurons" << std::endl;
    
  // ===== СОЗДАЕМ И ИНИЦИАЛИЗИРУЕМ CORESYSTEM =====
    std::cout << "Step 2: Creating CoreSystem..." << std::endl;
    CoreSystem core;
    std::cout << "Step 3: Initializing CoreSystem..." << std::endl;
    if (!core.initialize("config/system_config.json")) {
        std::cerr << "Failed to initialize CoreSystem" << std::endl;
        return 1;
    }
    std::cout << "Step 4: Getting components..." << std::endl;
    const auto& deviceInfo = core.getDeviceInfo();
    std::cout << "Running on: " << deviceInfo.device_type << std::endl;

    // Получаем доступ к компонентам
    LanguageModule* language = core.getComponent<LanguageModule>("language");
    MetaCognitiveModule* metacog = core.getComponent<MetaCognitiveModule>("metacognition");
    // LearningOrchestrator больше не используется с новым DynamicSemanticMemory
    // auto* learning_orchestrator = core.getComponent<LearningOrchestrator>("learning");
    NeuralFieldSystem& neuralSystem = core.getNeuralSystem();
    EmergentMemory& memoryManager = core.getMemory();
    ImmutableCore& immutable_core = core.getImmutableCore();
    IAuthorization& auth = core.getAuth();
    std::cout << "Step 5: Creating UI..." << std::endl;
    // Регистрируем хабы
    neuralSystem.registerHub(0);
    neuralSystem.registerHub(15);
    neuralSystem.registerHub(31);
    
    CoreHub coreHub(3);
    coreHub.connectToGroups(neuralSystem);

    // Конвертируем Config::ModeLimits в MassLimits
    MassLimits initialLimits;
    initialLimits.planck_mass = selectedLimits.planck_mass;
    initialLimits.schwarzschild_radius_factor = selectedLimits.schwarzschild_radius_factor;
    initialLimits.max_temperature_mass = selectedLimits.max_temperature_mass;
    initialLimits.evaporation_rate = selectedLimits.evaporation_rate;
    initialLimits.info_capacity_factor = selectedLimits.info_capacity_factor;
    initialLimits.max_entropy_density = selectedLimits.max_entropy_density;
    initialLimits.binding_energy_factor = selectedLimits.binding_energy_factor;
    initialLimits.saturation_mass = selectedLimits.saturation_mass;

    std::mt19937 rng(std::random_device{}());
    neuralSystem.initializeWithLimits(rng, initialLimits);
    neuralSystem.initializePredictiveCoder(memoryManager);
    neuralSystem.setOperatingMode(startupMode);
    std::cout << "Initial operating mode: " << OperatingMode::toString(startupMode) << std::endl;
    
    ResourceMonitor resources(resConfig);


    float bestFitness = 0.0f;
    uint8_t lastAction = 0;
    float lastReward = 0.0f;
    float cumulativeReward = 0.0f;
    int actionsTaken = 0;
    
    // Инициализация UI
    UIModule ui(uiConfig, windowWidth, windowHeight);
    ui.setLanguageModule(language);
    std::cout << "Step 6: Creating window..." << std::endl;
    int visWidth = ui.getVisualizationWidth();
    int visHeight = ui.getVisualizationHeight();
    auto* trainer = language->getWebTrainer();
    if (trainer) {
        std::cout << "WebTrainer is active" << std::endl;
        std::cout << trainer->getStats() << std::endl;
        
        // ПЕРЕДАЁМ WebTrainer в UI
        ui.setWebTrainer(trainer);
    } else {
        std::cout << "WARNING: WebTrainer is NULL!" << std::endl;
    }
    VisualizationModule visualization(visConfig, NeuralFieldSystem::NUM_GROUPS, 
                                    NeuralFieldSystem::GROUP_SIZE, visWidth, visHeight);
    ui.setVisualizer(&visualization);
    ui.setNeuralSystem(&neuralSystem);
    
    InteractionModule interaction(interConfig, NeuralFieldSystem::NUM_GROUPS, 
                                  visWidth / float(NeuralFieldSystem::NUM_GROUPS));
    StatisticsModule statistics;

    sf::RenderWindow window(sf::VideoMode({windowWidth, windowHeight}), 
                           "Advanced Neural Field System - MaryAI");
    std::cout << "Step 7: Window created, entering main loop..." << std::endl;
    UnifiedStatsCollector stats_collector;
    stats_collector.setNeuralSystem(&neuralSystem);
    stats_collector.setMemoryManager(&memoryManager);
    stats_collector.setLanguage(language);
    stats_collector.setMetaCognitive(metacog);
    // stats_collector.setLearning(nullptr);  // LearningOrchestrator удалён
    visualization.updateOrbitRadiiFromSystem(neuralSystem);

    statistics.setStatsCollector(&stats_collector);
    ui.setStatsCollector(&stats_collector);

    int step = 0;
    bool simulation_running = false;
    bool system_in_stasis = false;

    // ========== ОСНОВНОЙ ЦИКЛ ==========
    while (window.isOpen()) {
        // Обработка событий (без изменений)
        while (auto eventOpt = window.pollEvent()) {
            const auto& event = *eventOpt;
            
            if (event.is<sf::Event::Closed>()) {
                window.close();
            }
            else if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->code == sf::Keyboard::Key::Equal) {
                    visualization.handleZoom(0.1f);
                }
                else if (keyPressed->code == sf::Keyboard::Key::Hyphen) {
                    visualization.handleZoom(-0.1f);
                }
                else if (keyPressed->code == sf::Keyboard::Key::R) {
                    visualization.resetView();
                }
                else if (keyPressed->code == sf::Keyboard::Key::Left) {
                    visualization.handleRotate(-5.0f);
                }
                else if (keyPressed->code == sf::Keyboard::Key::Right) {
                    visualization.handleRotate(5.0f);
                }
                else if (keyPressed->code == sf::Keyboard::Key::Up) {
                    visualization.handleTilt(5.0f);
                }
                else if (keyPressed->code == sf::Keyboard::Key::Down) {
                    visualization.handleTilt(-5.0f);
                }
            }
            else if (const auto* mouseMoved = event.getIf<sf::Event::MouseMoved>()) {
                static sf::Vector2i lastMousePos;
                if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Middle)) {
                    sf::Vector2i delta = sf::Mouse::getPosition(window) - lastMousePos;
                    visualization.handlePan(delta.x, delta.y);
                }
                lastMousePos = sf::Mouse::getPosition(window);
            }
            else if (const auto* mouseWheel = event.getIf<sf::Event::MouseWheelScrolled>()) {
                ui.handleMouseWheel(*mouseWheel);
            }
            else if (const auto* mousePressed = event.getIf<sf::Event::MouseButtonPressed>()) {
                ui.handleMouseClick(*mousePressed, neuralSystem, simulation_running, statistics);
                
                int mouseX = mousePressed->position.x;
                int currentVisWidth = ui.getVisualizationWidth();
                
                if (mouseX < currentVisWidth && !system_in_stasis) {
                    interaction.handleMouseClick(*mousePressed, neuralSystem);
                    lastReward += 0.1f;
                    cumulativeReward += 0.1f;
                }
            }
            else if (const auto* textEntered = event.getIf<sf::Event::TextEntered>()) {
                ui.handleTextEntered(*textEntered);
                std::cout << "Text entered: " << static_cast<char>(textEntered->unicode) 
                          << " (current input: '" << ui.getCurrentInput() << "')" << std::endl;
            }
        }
        
        resources.update();

        if (simulation_running) {
            ui.setSimulationRunning(true);
            coreHub.integrate(neuralSystem);

            auto start_time = std::chrono::high_resolution_clock::now();

            OperatingMode::Type current_mode = ui.getCurrentOperatingMode();
            neuralSystem.setOperatingMode(current_mode);
            coreHub.integrate(neuralSystem);
            neuralSystem.step(lastReward, step);
            coreHub.learnSTDP(lastReward, step);
            
            if (step % 100 == 0) {
                neuralSystem.reflect();
                if (metacog) metacog->think();
                
                if (neuralSystem.evaluateProgress()) {
                    std::cout << "Goal achieved!" << std::endl;
                }
            }

            stats_collector.update(step);
            statistics.updateFromCollector();
            
            auto end_time = std::chrono::high_resolution_clock::now();
            double step_time = std::chrono::duration<double>(end_time - start_time).count();

            
            statistics.update(neuralSystem, step, dt);

            if (step % 200 == 0) {
                const auto& stats = statistics.getCurrentStats();
                double entropy = neuralSystem.computeSystemEntropy();
                std::cout << "\rStep " << step 
                          << " | Energy: " << std::fixed << std::setprecision(3) << stats.total_energy
                          << " | Entropy: " << std::setprecision(2) << entropy
                          << " | Best: " << bestFitness
                          << " | CPU: " << std::setprecision(0) << resources.getCurrentLoad() << "%   ";
                std::cout.flush();
            }
            // В основном цикле, внутри if (simulation_running) после step++
            if (step % 1000 == 0 && step > 0) {
                if (trainer) {
                    trainer->logTrainingStatus();
                }
                
                // Дополнительная статистика обучения
                std::cout << "\n=== LEARNING STATS ===" << std::endl;
                std::cout << "Step: " << step << std::endl;
                std::cout << "Best fitness: " << bestFitness << std::endl;
                
                if (language) {
                    auto stats = language->getLanguageStats();
                    std::cout << "Words learned: " << stats.words_learned << std::endl;
                    std::cout << "Patterns detected: " << stats.patterns_detected << std::endl;
                    std::cout << "Meanings formed: " << stats.meanings_formed << std::endl;
                    std::cout << "User profiles: " << stats.user_profiles << std::endl;
                    std::cout << "External feedback avg: " << language->getExternalFeedbackAvg() << std::endl;
                }
                
                
                std::cout << "=====================\n" << std::endl;
            }
            step++;
        } else {
            ui.setSimulationRunning(false);
        }

        // Визуализация
        window.clear();

        if (visConfig.enabled && !system_in_stasis) {
            visualization.updateDynamicRange(neuralSystem);
            visualization.draw(window, neuralSystem);
        }

        ui.draw(window, neuralSystem, statistics, simulation_running && !system_in_stasis, 
                resources, 
                memoryManager, step);
        
        window.display();
    }
    
    // Финальное сохранение
    std::cout << "\n\nSaving all data to dump/ folder..." << std::endl;
    
    statistics.saveToFile("dump/simulation_statistics.csv");
    
    if (language) language->saveAll();
    
    if (std::filesystem::exists("config/system_config.json")) {
        std::filesystem::copy_file("config/system_config.json", 
                                   "dump/last_config.json", 
                                   std::filesystem::copy_options::overwrite_existing);
    }
    
    std::ofstream meta("dump/meta.txt");
    if (meta.is_open()) {
        meta << "=== SIMULATION METADATA ===\n";
        meta << "Date: " << __DATE__ << " " << __TIME__ << "\n";
        meta << "Architecture: " << NeuralFieldSystem::NUM_GROUPS << " groups × " 
             << NeuralFieldSystem::GROUP_SIZE << " neurons\n";
        meta << "Total neurons: " << NeuralFieldSystem::TOTAL_NEURONS << "\n";
        meta << "Total steps: " << step << "\n";
        meta << "Best fitness: " << bestFitness << "\n";
        meta << "Actions taken: " << actionsTaken << "\n";
        if (language) meta << "Learned words: " << language->getLearnedWordsCount() << "\n";
        meta.close();
    }
    
    core.shutdown();
    
    std::cout << "\n=== FINAL STATS ===" << std::endl;
    std::cout << "Total steps: " << step << std::endl;
    std::cout << "Best fitness: " << bestFitness << std::endl;
    std::cout << "Actions taken: " << actionsTaken << std::endl;
    std::cout << "Neurons: " << NeuralFieldSystem::TOTAL_NEURONS << " ("
              << NeuralFieldSystem::NUM_GROUPS << "x" << NeuralFieldSystem::GROUP_SIZE << ")" << std::endl;
    std::cout << "All data saved to dump/ folder" << std::endl;

    return 0;
}