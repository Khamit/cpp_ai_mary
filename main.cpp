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
// Подключаем CoreSystem вместо отдельных компонентов
#include "core/NeuralFieldSystem.hpp"
#include "modules/VisualizationModule.hpp"
#include "modules/InteractionModule.hpp"
#include "modules/StatisticsModule.hpp"
#include "modules/UIModule.hpp"
#include "core/ImmutableCore.hpp"
#include "modules/EvolutionModule.hpp"
#include "modules/ResourceMonitor.hpp"
#include "modules/ConfigStructs.hpp"
#include "modules/MetaCognitiveModule.hpp"
#include "modules/lang/LanguageModule.hpp"  // Добавлено!
#include "modules/lang/EffectiveLearning.hpp"
#include "core/CoreHub.hpp"
//#include "core/MemoryManager.hpp"
//#include "core/Component.hpp"
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>  // Добавьте этот заголовок для STDERR_FILENO
#include <cstring>
#include <atomic>

std::atomic<bool> handling_signal(false);

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
    evolConfig.evolution_interval_steps = getIntValue(content, "\"evolution_interval_steps\"", 10000);
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
    // Установить обработчик сигналов В САМОМ НАЧАЛЕ
    signal(SIGSEGV, handler);

    // Создаем папки если их нет
    std::filesystem::create_directories("dump");
    std::filesystem::create_directories("data");
    
    // Загрузка полной конфигурации
    VisualizationConfig visConfig;
    InteractionConfig interConfig;
    UIConfig uiConfig;
    ResourceMonitorConfig resConfig;
    EvolutionConfig evolConfig;
    
    bool configLoaded = ConfigLoader::loadFromFile("config/system_config.json", 
                               visConfig, interConfig, uiConfig,
                              resConfig, evolConfig);

    // Параметры системы
    double dt = 0.001;
    unsigned int windowWidth = 800;
    unsigned int windowHeight = 700;

    std::cout << "! Initializing Neural Field System with " 
              << NeuralFieldSystem::NUM_GROUPS << " groups × " 
              << NeuralFieldSystem::GROUP_SIZE << " neurons = "
              << NeuralFieldSystem::TOTAL_NEURONS << " total neurons" << std::endl;
    
    // ===== СОЗДАЕМ И ИНИЦИАЛИЗИРУЕМ CORESYSTEM =====
    CoreSystem core;
    // Инициализация автоматически определит устройство
    // Инициализируем с конфигурационным файлом
    if (!core.initialize("config/system_config.json")) {
        std::cerr << "Failed to initialize CoreSystem" << std::endl;
        return 1;
    }

    // Получаем информацию об устройстве
    const auto& deviceInfo = core.getDeviceInfo();
    std::cout << "Running on: " << deviceInfo.device_type << std::endl;

    // Запускаем предобучение (только при первом запуске)
    // core.runInitialTraining();
    
    // Получаем доступ к компонентам ядра
    NeuralFieldSystem& neuralSystem = core.getNeuralSystem();
    MemoryManager& memoryManager = core.getMemory();
    ImmutableCore& immutable_core = core.getImmutableCore();
    // Вместо конкретного AccessManager используем интерфейс
    IAuthorization& auth = core.getAuth();  // теперь интерфейс
    // PersonnelDatabase может не существовать в personal режиме
    // Поэтому будем использовать её только если нужно
    // ИСПРАВЛЕНИЕ: создаем персонал только для enterprise режима
    PersonnelDatabase* personnel_db = nullptr;
    if (core.isEnterpriseMode()) {
        static PersonnelDatabase enterprise_db;
        personnel_db = &enterprise_db;
    }
    // ИСПРАВЛЕНИЕ: создаем семантический граф один раз
    SemanticGraphDatabase semantic_graph;
    // Создаем менеджеры
    MasterKeyManager master_key_mgr;

    // Регистрируем хабы в нейросистеме
    neuralSystem.registerHub(0);   // группа 0 как хаб
    neuralSystem.registerHub(15);  // группа 15 как хаб
    neuralSystem.registerHub(31);  // группа 31 как хаб
    
    // Создаем CoreHub и подключаем через интерфейс
    CoreHub coreHub(3);
    coreHub.connectToGroups(neuralSystem);

    // Регистрируем модули в CoreSystem
    auto* evolution = core.registerComponent<EvolutionModule>(
        "evolution",
        immutable_core, 
        evolConfig, 
        memoryManager);

    auto* language = core.registerComponent<LanguageModule>(
        "language", 
        neuralSystem, 
        immutable_core,
        auth,
        &semantic_graph  // SemanticGraphDatabase* graph = nullptr
    );

    auto* metacog = core.registerComponent<MetaCognitiveModule>(
        "metacognition", neuralSystem
    );

    // ИСПРАВЛЕНИЕ: создаем EffectiveLearning с правильным порядком параметров
    auto semantic_learning = std::make_unique<EffectiveLearning>(
        neuralSystem,
        *language,
        language->getSemanticManager(),
        memoryManager,
        personnel_db,  // указатель (может быть nullptr)
        auth,
        deviceInfo,
        semantic_graph
    );
    // Связываем MetaCognitiveModule с EvolutionModule
    metacog->setEvolutionModule(evolution);

    // Инициализируем любопытство
    auto curiosity = std::make_shared<CuriosityDriver>(
        neuralSystem,
        *language, 
        semantic_graph
    );
    language->setCuriosityDriver(curiosity);
    semantic_learning->initializeCuriosity();

    // При первом запуске показываем мастер-ключ
    if (!std::filesystem::exists("dump/semantic_trained.bin")) {
        std::cout << "\n🔑 FIRST RUN - MASTER KEY GENERATED:\n";
        std::cout << "=====================================\n";
        // Мастер-ключ уже сгенерирован в MasterKeyManager
        std::cout << "=====================================\n\n";
    }
    
    std::mt19937 rng(std::random_device{}());
    neuralSystem.initializePredictiveCoder(memoryManager);
    
    // Создаем остальные модули
    ResourceMonitor resources(resConfig);

    // Подключаем детектор к нейросистеме
    evolution->connectToSystem(neuralSystem);
    
    // Переменные для работы
    float bestFitness = 0.0f;
    uint8_t lastAction = 0;
    float lastReward = 0.0f;
    float cumulativeReward = 0.0f;
    int actionsTaken = 0;
    
    // Инициализация UI и визуализации
    UIModule ui(uiConfig, windowWidth, windowHeight);
    ui.setLanguageModule(language);
    int visWidth = ui.getVisualizationWidth();
    int visHeight = ui.getVisualizationHeight();
    
    VisualizationModule visualization(visConfig, NeuralFieldSystem::NUM_GROUPS, 
                                    NeuralFieldSystem::GROUP_SIZE, visWidth, visHeight);
    ui.setVisualizer(&visualization);
    ui.setNeuralSystem(&neuralSystem);
    ui.setSemanticGraph(&semantic_graph);
    
    InteractionModule interaction(interConfig, NeuralFieldSystem::NUM_GROUPS, 
                                  visWidth / float(NeuralFieldSystem::NUM_GROUPS));
    StatisticsModule statistics;

    // Создание окна
    sf::RenderWindow window(sf::VideoMode({windowWidth, windowHeight}), 
                           "Advanced Neural Field System - MaryAI");

    // Создаем UnifiedStatsCollector
    UnifiedStatsCollector stats_collector;
    stats_collector.setNeuralSystem(&neuralSystem);
    stats_collector.setMemoryManager(&memoryManager);
    stats_collector.setEvolution(evolution);
    stats_collector.setLanguage(language);
    stats_collector.setMetaCognitive(metacog);
    stats_collector.setEffectiveLearning(semantic_learning.get());

    // Подключаем к StatisticsModule
    statistics.setStatsCollector(&stats_collector);

    // Подключаем к UIModule
    ui.setStatsCollector(&stats_collector);

    // Переменные симуляции
    int step = 0;
    bool simulation_running = false;
    bool system_in_stasis = false;

    // Загружаем память если есть
    if (std::ifstream("dump/short_term.bin").good()) {
        memoryManager.loadAll();
        std::cout << "! Loaded memory from dump/" << std::endl;
    }


    core.getEventSystem().subscribe(EventType::PREDICTION_HIGH_ERROR, 
    [&memoryManager](const Event& event) {
        // Сохраняем в память, но без вывода
        memoryManager.storeWithEntropy("predictor", event.data, event.value, 0.8f);
        // Убрали std::cout
    });

    // *ОСНОВНОЙ ЦИКЛ*
    while (window.isOpen()) {
        // ОБРАБОТКА СОБЫТИЙ
        while (auto eventOpt = window.pollEvent()) {
            const auto& event = *eventOpt;
            
            if (event.is<sf::Event::Closed>()) {
                window.close();
            }
            // Обработка событий клавиатуры:
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
                    visualization.handleRotate(-5.0f);  // вращение влево
                }
                else if (keyPressed->code == sf::Keyboard::Key::Right) {
                    visualization.handleRotate(5.0f);   // вращение вправо
                }
                else if (keyPressed->code == sf::Keyboard::Key::Up) {
                    visualization.handleTilt(5.0f);     // наклон вверх
                }
                else if (keyPressed->code == sf::Keyboard::Key::Down) {
                    visualization.handleTilt(-5.0f);    // наклон вниз
                }
            }
            // В обработке движения мыши (если зажата кнопка)
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
        /*
        if (!system_in_stasis && resources.checkAndTriggerOverload()) {
            std::cout << "⚠️ System overload detected! Reducing memory activity..." << std::endl;
        }
        */
        // Проверяем состояние автообучения
        static bool lastAutoLearningState = false;
        if (ui.isAutoLearningActive() != lastAutoLearningState) {
            if (ui.isAutoLearningActive()) {
                language->runAutoLearning(10000, semantic_learning.get());
                std::cout << "Auto-learning started" << std::endl;
            } else {
                language->stopAutoLearning();
                semantic_learning->stopTraining();  // останавливаем обучение
                std::cout << "Auto-learning stopped" << std::endl;
            }
            lastAutoLearningState = ui.isAutoLearningActive();
        }

        if (simulation_running) {

            // Интеграция хабов перед шагом нейросистемы
            coreHub.integrate(neuralSystem);

            auto start_time = std::chrono::high_resolution_clock::now();

            // ===== ЕДИНСТВЕННЫЙ ВЫЗОВ НЕЙРОСИСТЕМЫ =====
            neuralSystem.step(lastReward, step);
            // Передаем текущий шаг в обучение (если нужно)
            // semantic_learning->setCurrentStep(step);  // если добавите такой метод
            // Обучение хабов
            coreHub.learnSTDP(lastReward, step);
            
            // Рефлексия (редко)
            if (step % 100 == 0) {
                neuralSystem.reflect();
                metacog->think();
                
                if (neuralSystem.evaluateProgress()) {
                    std::cout << "Goal achieved!" << std::endl;
                }
            }

            // Обновляем unified статистику
            stats_collector.update(step);
            statistics.updateFromCollector();
            
            auto end_time = std::chrono::high_resolution_clock::now();
            double step_time = std::chrono::duration<double>(end_time - start_time).count();

            // ЭВОЛЮЦИЯ И ОЦЕНКА
            evolution->evaluateFitness(neuralSystem, step_time, *language);
            
            // ========== РАБОТА С ПАМЯТЬЮ ==========
            if (!system_in_stasis) {
                std::vector<float> currentFeatures = neuralSystem.getFeatures();
                
                // ИСПРАВЛЕНИЕ 2: Используем энтропийное сохранение
                double system_entropy = neuralSystem.computeSystemEntropy();
                
                auto similarIndices = memoryManager.findSimilar("neural", currentFeatures, 3);
                
                if (!similarIndices.empty()) {
                    std::vector<int> actionVotes(10, 0);
                    
                    const auto& neuralMemory = memoryManager.getLongTermMemory().at("neural");
                    
                    for (size_t idx : similarIndices) {
                        if (idx < neuralMemory.size()) {
                            const auto& past = neuralMemory[idx];
                            
                            auto actionIt = past.metadata.find("action");
                            auto utilityIt = past.metadata.find("utility");
                            
                            if (actionIt != past.metadata.end() && utilityIt != past.metadata.end()) {
                                int pastAction = std::stoi(actionIt->second);
                                float pastUtility = std::stof(utilityIt->second);
                                actionVotes[pastAction % 10] += static_cast<int>(pastUtility * 10);
                            }
                        }
                    }
                    
                    lastAction = std::max_element(actionVotes.begin(), actionVotes.end()) - actionVotes.begin();
                } else {
                    lastAction = rand() % 5;
                }
                
                float currentFitness = evolution->getOverallFitness();
                float fitnessDelta = currentFitness - bestFitness;
                lastReward = fitnessDelta * 10.0f;
                
                float explorationBonus = ((rand() % 100) / 10000.0f) - 0.005f;
                lastReward += explorationBonus;
                
                float utility = (currentFitness * 0.5f) + (cumulativeReward * 0.3f) + 0.2f;
                
                std::map<std::string, std::string> metadata;
                metadata["action"] = std::to_string(lastAction);
                metadata["reward"] = std::to_string(lastReward);
                metadata["utility"] = std::to_string(utility);
                metadata["step"] = std::to_string(step);
                metadata["entropy"] = std::to_string(system_entropy);  // добавили энтропию
                
                // ИСПРАВЛЕНИЕ 3: Используем storeWithEntropy для высокоэнтропийных состояний
                if (system_entropy > 2.5) {
                    memoryManager.storeWithEntropy("neural", currentFeatures, system_entropy, utility);
                } else {
                    memoryManager.store("neural", "features", currentFeatures, utility, metadata);
                }
                
                if (currentFitness > bestFitness) {
                    bestFitness = currentFitness;
                    
                    std::cout << "\nNew best fitness: " << bestFitness 
                              << " | Records: " << memoryManager.getLongTermMemory().size() << std::endl;
                }
                
                // ИСПРАВЛЕНИЕ 4: Сохраняем статистику периодически
                if (step % 10000 == 0 && step > 0) {
                    memoryManager.saveAll();
                    statistics.saveToFile("dump/simulation_statistics.csv");
                    std::cout << semantic_learning->getStats() << std::endl;
                    std::cout << "\nCheckpoint saved at step " << step << std::endl;
                }
                
                actionsTaken++;
                if (actionsTaken % 100 == 0) {
                    cumulativeReward *= 0.9f;
                }

                // Сохраняем память периодически (УБРАНО из store)
                if (step % 1000 == 0) {
                    memoryManager.saveAll();
                }
            }

            // УМНЫЙ ВЫЗОВ ЭВОЛЮЦИИ
            if (step % evolConfig.evolution_interval_steps == 0 && !system_in_stasis) {
                if (evolution->getOverallFitness() < evolConfig.min_fitness_for_optimization) {
                    evolution->proposeMutation(neuralSystem);
                }
            }
            
            statistics.update(neuralSystem, step, dt);

            // Проверка выхода из стазиса
            if (step % 1000 == 0 && system_in_stasis) {
                if (resources.getCurrentLoad() < 30.0) {
                    evolution->exitStasis();
                    system_in_stasis = false;
                    std::cout << "Auto-exited stasis" << std::endl;
                }
            }
            
            // Вывод статистики
                if (step % 200 == 0) {  // каждые 200 шагов вместо 100
                    const auto& stats = statistics.getCurrentStats();
                    const auto& metrics = evolution->getCurrentMetrics();
                    double entropy = neuralSystem.computeSystemEntropy();
                    std::cout << "\rStep " << step 
                            << " | Energy: " << std::fixed << std::setprecision(3) << stats.total_energy
                            << " | Entropy: " << std::setprecision(2) << entropy
                            << " | Fitness: " << std::setprecision(3) << metrics.overall_fitness
                            << " | Best: " << bestFitness
                            << " | Memory: " << memoryManager.getLongTermMemory().size() 
                            << " | CPU: " << std::setprecision(0) << resources.getCurrentLoad() << "%   ";
                    std::cout.flush();
                }
            step++;
        }

        // ВИЗУАЛИЗАЦИЯ
        window.clear();

        if (visConfig.enabled && !system_in_stasis) {
            visualization.updateDynamicRange(neuralSystem);
            visualization.draw(window, neuralSystem);
        }

        ui.draw(window, neuralSystem, statistics, simulation_running && !system_in_stasis, 
                resources, *evolution, memoryManager, step);
        
        window.display();
    }
    // ФИНАЛЬНОЕ СОХРАНЕНИЕ
    std::cout << "\n\nSaving all data to dump/ folder..." << std::endl;
    
    // ИСПРАВЛЕНИЕ 5: Финальное сохранение статистики
    statistics.saveToFile("dump/simulation_statistics.csv");
    
    evolution->saveEvolutionState();
    memoryManager.saveAll();
    language->saveAll();
    
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
        meta << "Memory records: " << memoryManager.getLongTermMemory().size() << "\n";
        meta << "Actions taken: " << actionsTaken << "\n";
        meta << "Learned words: " << language->getLearnedWordsCount() << "\n";
        meta.close();
    }
    
    // Завершаем работу CoreSystem (unique_ptr сам удалит effectiveLearning)
    core.shutdown();
    
    std::cout << "\n=== FINAL STATS ===" << std::endl;
    std::cout << "Total steps: " << step << std::endl;
    std::cout << "Best fitness: " << bestFitness << std::endl;
    std::cout << "Memory records: " << memoryManager.getLongTermMemory().size() << std::endl;
    std::cout << "Actions taken: " << actionsTaken << std::endl;
    std::cout << "Neurons: " << NeuralFieldSystem::TOTAL_NEURONS << " ("
              << NeuralFieldSystem::NUM_GROUPS << "x" << NeuralFieldSystem::GROUP_SIZE << ")" << std::endl;
    std::cout << "All data saved to dump/ folder" << std::endl;

    return 0;
}