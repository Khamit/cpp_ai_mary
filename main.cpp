#include <SFML/Graphics.hpp>
#include <iostream>
#include <fstream>
#include <random>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <filesystem>

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
//#include "core/MemoryManager.hpp"
//#include "core/Component.hpp"

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
    evolConfig.evolution_interval_steps = getIntValue(content, "\"evolution_interval_steps\"", 1000);
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

    // Параметры системы для новой архитектуры
    double dt = 0.001;
    unsigned int windowWidth = 800;
    unsigned int windowHeight = 700;

    std::cout << "! Initializing Neural Field System with " 
              << NeuralFieldSystem::NUM_GROUPS << " groups × " 
              << NeuralFieldSystem::GROUP_SIZE << " neurons = "
              << NeuralFieldSystem::TOTAL_NEURONS << " total neurons" << std::endl;
    
    // ===== СОЗДАЕМ И ИНИЦИАЛИЗИРУЕМ CORESYSTEM =====
    CoreSystem core;

    // Инициализируем с конфигурационным файлом
    if (!core.initialize("config/system_config.json")) {
        std::cerr << "Failed to initialize CoreSystem" << std::endl;
        return 1;
    }
    
    // Получаем доступ к компонентам ядра - ТОЛЬКО ОДИН РАЗ!
    NeuralFieldSystem& neuralSystem = core.getNeuralSystem();  // Это ссылка
    MemoryManager& memoryManager = core.getMemory();
    ImmutableCore& immutable_core = core.getImmutableCore();

    // Регистрируем модули в CoreSystem
    auto* evolution = core.registerComponent<EvolutionModule>(
        "evolution",
        immutable_core, 
        evolConfig, 
        memoryManager);

    // neuralSystem - это ссылка, поэтому передаем как есть
    auto* language = core.registerComponent<LanguageModule>(
        "language", 
        neuralSystem, 
        *evolution,      // EvolutionModule
        memoryManager     // MemoryManager - НОВЫЙ ПАРАМЕТР
    );
    auto* metacog = core.registerComponent<MetaCognitiveModule>("metacognition", neuralSystem);
    
    // Создаем остальные модули (они пока не Component)
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
    ui.setNeuralSystem(&neuralSystem);  // передаем указатель на neuralSystem
    
    VisualizationModule visualization(visConfig, NeuralFieldSystem::NUM_GROUPS, 
                                      NeuralFieldSystem::GROUP_SIZE, visWidth, visHeight);
    InteractionModule interaction(interConfig, NeuralFieldSystem::NUM_GROUPS, 
                                  visWidth / float(NeuralFieldSystem::NUM_GROUPS));
    StatisticsModule statistics;

    // Создание окна
    sf::RenderWindow window(sf::VideoMode({windowWidth, windowHeight}), 
                           "Advanced Neural Field System - MaryAI");

    // Переменные симуляции
    int step = 0;
    bool simulation_running = false;
    bool system_in_stasis = false;

    // Загружаем память если есть
    if (std::ifstream("dump/short_term.bin").good()) {
        memoryManager.loadAll();
        std::cout << "! Loaded memory from dump/" << std::endl;
    }
    // новый UI 
    // ========= Новый Основной Цикл =============== !!!!
        // Основной цикл
    while (window.isOpen()) {
        // ОБРАБОТКА СОБЫТИЙ
        while (auto eventOpt = window.pollEvent()) {
            const auto& event = *eventOpt;
            
            if (event.is<sf::Event::Closed>()) {
                window.close();
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
        
        if (!system_in_stasis && resources.checkAndTriggerOverload()) {
            std::cout << "⚠️ System overload detected! Reducing memory activity..." << std::endl;
        }

        if (simulation_running) {
            auto start_time = std::chrono::high_resolution_clock::now();

            // ===== ЕДИНСТВЕННЫЙ ВЫЗОВ НЕЙРОСИСТЕМЫ =====
            neuralSystem.step(lastReward, step);
            
            // Рефлексия (редко)
            if (step % 100 == 0) {
                neuralSystem.reflect();
                metacog->think();
                
                if (neuralSystem.evaluateProgress()) {
                    std::cout << "Goal achieved!" << std::endl;
                }
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            double step_time = std::chrono::duration<double>(end_time - start_time).count();

            // ЭВОЛЮЦИЯ И ОЦЕНКА
            evolution->evaluateFitness(neuralSystem, step_time, *language);
            
            // ========== РАБОТА С ПАМЯТЬЮ ==========
            if (!system_in_stasis) {
                std::vector<float> currentFeatures = neuralSystem.getFeatures();
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
                
                memoryManager.store("neural", "features", currentFeatures, utility, metadata);
                
                if (currentFitness > bestFitness) {
                    bestFitness = currentFitness;
                    
                    std::cout << "\n🏆 New best fitness: " << bestFitness 
                              << " | Records: " << memoryManager.getLongTermMemory().size() << std::endl;
                }
                
                if (step % 1000 == 0 && step > 0) {
                    memoryManager.saveAll();
                    std::cout << "\n💾 Checkpoint saved" << std::endl;
                }
                
                actionsTaken++;
                if (actionsTaken % 100 == 0) {
                    cumulativeReward *= 0.9f;
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
            if (step % 100 == 0) {
                const auto& stats = statistics.getCurrentStats();
                const auto& metrics = evolution->getCurrentMetrics();
                std::cout << "\rStep " << step 
                          << " | Energy: " << stats.total_energy
                          << " | Fitness: " << metrics.overall_fitness
                          << " | Best: " << bestFitness
                          << " | Memory: " << memoryManager.getLongTermMemory().size() 
                          << " | CPU: " << resources.getCurrentLoad() << "%   ";
                std::cout.flush();
            }
            step++;  // УВЕЛИЧИВАЕМ СЧЕТЧИК ПОСЛЕ ВСЕГО
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
    std::cout << "\n\n💾 Saving all data to dump/ folder..." << std::endl;
    
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
    
    // Завершаем работу CoreSystem
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