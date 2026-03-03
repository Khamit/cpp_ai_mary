#include <SFML/Graphics.hpp>
#include <iostream>
#include <fstream>
#include <random>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <filesystem>

// Подключаем новую архитектуру
#include "core/NeuralFieldSystem.hpp"
#include "modules/LearningModule.hpp"
#include "modules/DynamicsModule.hpp"
#include "modules/VisualizationModule.hpp"
#include "modules/InteractionModule.hpp"
#include "modules/StatisticsModule.hpp"
#include "modules/UIModule.hpp"
#include "core/ImmutableCore.hpp"
#include "modules/EvolutionModule.hpp"
#include "modules/ResourceMonitor.hpp"
#include "modules/ConfigStructs.hpp"
#include "data/MemoryModule.hpp"
#include "modules/MetaCognitiveModule.hpp"
#include "modules/lang/LanguageModule.hpp"  // Добавлено!

class ConfigLoader {
public:
    static bool loadFromFile(const std::string& filename, 
                            LearningConfig& learnConfig,
                            DynamicsConfig& dynConfig,
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
                               LearningConfig& learnConfig,
                               DynamicsConfig& dynConfig,
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
    
    // Парсинг основных модулей
    learnConfig.enabled = getBoolValue(content, "\"enabled\"", true);
    learnConfig.learning_rate = getDoubleValue(content, "\"learning_rate\"", 0.001);
    learnConfig.weight_decay = getDoubleValue(content, "\"weight_decay\"", 0.999);
    learnConfig.max_weight = getDoubleValue(content, "\"max_weight\"", 0.1);
    learnConfig.min_weight = getDoubleValue(content, "\"min_weight\"", -0.1);
    learnConfig.rule = getStringValue(content, "\"rule\"", "hebbian");
    
    dynConfig.enabled = getBoolValue(content, "\"enabled\"", true);
    dynConfig.damping_enabled = getBoolValue(content, "\"damping_enabled\"", true);
    dynConfig.damping_factor = getDoubleValue(content, "\"damping_factor\"", 0.999);
    dynConfig.limits_enabled = getBoolValue(content, "\"limits_enabled\"", true);
    dynConfig.max_phi = getDoubleValue(content, "\"max_phi\"", 2.0);
    dynConfig.min_phi = getDoubleValue(content, "\"min_phi\"", -2.0);
    dynConfig.max_pi = getDoubleValue(content, "\"max_pi\"", 10.0);
    dynConfig.min_pi = getDoubleValue(content, "\"min_pi\"", -10.0);
    
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
    LearningConfig learnConfig;
    DynamicsConfig dynConfig;
    VisualizationConfig visConfig;
    InteractionConfig interConfig;
    UIConfig uiConfig;
    ResourceMonitorConfig resConfig;
    EvolutionConfig evolConfig;
    
    bool configLoaded = ConfigLoader::loadFromFile("config/system_config.json", 
                              learnConfig, dynConfig, visConfig, interConfig, uiConfig,
                              resConfig, evolConfig);

    // Параметры системы для новой архитектуры
    double dt = 0.001;  // шаг интегрирования
    unsigned int windowWidth = 800;
    unsigned int windowHeight = 700;

    std::cout << "! Initializing Neural Field System with " 
              << NeuralFieldSystem::NUM_GROUPS << " groups × " 
              << NeuralFieldSystem::GROUP_SIZE << " neurons = "
              << NeuralFieldSystem::TOTAL_NEURONS << " total neurons" << std::endl;
    
    // ИНИЦИАЛИЗАЦИЯ НОВОЙ АРХИТЕКТУРЫ
    NeuralFieldSystem neuralSystem(dt);
    
    std::mt19937 rng(42);
    neuralSystem.initializeRandom(rng);
    
    // Инициализация модулей
    ImmutableCore immutable_core;
    EvolutionModule evolution(immutable_core, evolConfig);
    ResourceMonitor resources(resConfig);
    MetaCognitiveModule metacog(neuralSystem);
    
    evolution.testEvolutionMethods();
    
    // Learning и Dynamics модули (адаптированы под новую архитектуру)
    LearningModule learning(learnConfig);
    DynamicsModule dynamics(dynConfig);

    // ИНИЦИАЛИЗАЦИЯ МОДУЛЯ ПАМЯТИ
    MemoryConfig memConfig;
    memConfig.max_records = 5000;
    memConfig.feature_dim = 64;  // 64 признака из getFeatures()
    memConfig.global_decay_factor = 0.995f;
    memConfig.similarity_threshold = 0.7f;
    
    MemoryController memory(memConfig);

    // ИНИЦИАЛИЗАЦИЯ ЯЗЫКОВОГО МОДУЛЯ (НОВАЯ ВЕРСИЯ)
    LanguageModule language(neuralSystem);
    
    // Переменные для работы с памятью
    float bestFitness = 0.0f;
    uint8_t lastAction = 0;
    float lastReward = 0.0f;
    float cumulativeReward = 0.0f;
    int actionsTaken = 0;
    
    std::cout << "! Memory module initialized:" << std::endl;
    std::cout << "   - Max records: " << memConfig.max_records << std::endl;
    std::cout << "   - Feature dim: " << memConfig.feature_dim << std::endl;
    std::cout << "   - Decay factor: " << memConfig.global_decay_factor << std::endl;

    // Инициализация UI и визуализации
    UIModule ui(uiConfig, windowWidth, windowHeight);
    ui.setLanguageModule(&language);
    int visWidth = ui.getVisualizationWidth();
    int visHeight = ui.getVisualizationHeight();
    ui.setNeuralSystem(&neuralSystem);
    
    // Для визуализации нам нужен доступ к плоским векторам
    // Визуализация должна быть адаптирована под новую архитектуру
    VisualizationModule visualization(visConfig, NeuralFieldSystem::NUM_GROUPS, 
                                      NeuralFieldSystem::GROUP_SIZE, visWidth, visHeight);
    InteractionModule interaction(interConfig, NeuralFieldSystem::NUM_GROUPS, 
                                  visWidth / float(NeuralFieldSystem::NUM_GROUPS));
    StatisticsModule statistics;

    // Создание окна
    sf::RenderWindow window(sf::VideoMode({windowWidth, windowHeight}), 
                           "Advanced Neural Field System - MaryAI (Group Architecture)");

    // Переменные симуляции
    int step = 0;
    bool simulation_running = false;
    bool system_in_stasis = false;

    // Загружаем предыдущую память если есть
    if (std::ifstream("dump/best_memory.dat").good()) {
        if (memory.loadFromFile("dump/best_memory.dat")) {
            std::cout << "! Loaded " << memory.size() << " memories from file" << std::endl;
        }
    }
    
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
                // Сначала обрабатываем UI
                ui.handleMouseClick(*mousePressed, neuralSystem, simulation_running, statistics);
                
                // Потом проверяем визуализацию
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

        // МОНИТОРИНГ РЕСУРСОВ
        resources.update();
        
        if (!system_in_stasis && resources.checkAndTriggerOverload()) {
            std::cout << "⚠️ System overload detected! Reducing memory activity..." << std::endl;
        }

        if (simulation_running) {
            auto start_time = std::chrono::high_resolution_clock::now();
            
            // ОСНОВНАЯ СИМУЛЯЦИЯ (НОВАЯ АРХИТЕКТУРА)
            neuralSystem.step();  // Один шаг эволюции всей системы
            
            // Применяем дополнительные модули если нужно
            if (dynConfig.enabled && !system_in_stasis) {
                // dynamics.applyDynamics(neuralSystem); // Нужно адаптировать
            }
                
            if (learnConfig.enabled && !system_in_stasis) {
                // learning.applyLearning(neuralSystem); // Нужно адаптировать
            }

            // Каждые 100 шагов - саморефлексия
            if (step % 100 == 0) {
                neuralSystem.reflect();
                metacog.think();
                
                if (neuralSystem.evaluateProgress()) {
                    std::cout << "🎯 Goal achieved!" << std::endl;
                }
            }
            
            step++;

            auto end_time = std::chrono::high_resolution_clock::now();
            double step_time = std::chrono::duration<double>(end_time - start_time).count();
            
            // ЭВОЛЮЦИЯ И ОЦЕНКА
            evolution.evaluateFitness(neuralSystem, step_time);
            
            // ========== РАБОТА С ПАМЯТЬЮ ==========
            if (!system_in_stasis) {
                std::vector<float> currentFeatures = neuralSystem.getFeatures();
                auto similarIndices = memory.findSimilar(currentFeatures, 3);
                
                if (!similarIndices.empty()) {
                    std::vector<int> actionVotes(10, 0);
                    for (size_t idx : similarIndices) {
                        const auto& past = memory.getRecords()[idx];
                        actionVotes[past.action % 10] += static_cast<int>(past.utility * 10);
                    }
                    lastAction = std::max_element(actionVotes.begin(), actionVotes.end()) 
                               - actionVotes.begin();
                } else {
                    lastAction = rand() % 5;
                }
                
                float currentFitness = evolution.getOverallFitness();
                float fitnessDelta = currentFitness - bestFitness;
                lastReward = fitnessDelta * 10.0f;
                
                float explorationBonus = ((rand() % 100) / 10000.0f) - 0.005f;
                lastReward += explorationBonus;
                
                float utility = (currentFitness * 0.5f) + (cumulativeReward * 0.3f) + 0.2f;
                
                MemoryRecord newRec(currentFeatures, lastAction, lastReward, utility);
                memory.addRecord(newRec);
                
                if (step % 200 == 0 && step > 0) {
                    memory.decayAll();
                }
                
                if (currentFitness > bestFitness) {
                    bestFitness = currentFitness;
                    memory.saveToFile("dump/best_memory.dat");
                    std::cout << "\n🏆 New best fitness: " << bestFitness 
                              << " | Records: " << memory.size() << std::endl;
                }
                
                if (step % 1000 == 0 && step > 0) {
                    std::string checkpointFile = "dump/memory_checkpoint_" + std::to_string(step) + ".dat";
                    memory.saveToFile(checkpointFile);
                    std::cout << "\nCheckpoint saved: " << checkpointFile << std::endl;
                }
                
                actionsTaken++;
                if (actionsTaken % 100 == 0) {
                    cumulativeReward *= 0.9f;
                }
            }

            // УМНЫЙ ВЫЗОВ ЭВОЛЮЦИИ
            if (step % evolConfig.evolution_interval_steps == 0 && !system_in_stasis) {
                if (evolution.getOverallFitness() < evolConfig.min_fitness_for_optimization) {
                    evolution.proposeMutation(neuralSystem);
                }
            }
            
            // ОБНОВЛЕНИЕ СТАТИСТИКИ
            statistics.update(neuralSystem, step, dt);

            // УПРАВЛЕНИЕ СТАЗИСОМ
            if (step % 1000 == 0 && system_in_stasis) {
                if (resources.getCurrentLoad() < 30.0) {
                    evolution.exitStasis();
                    system_in_stasis = false;
                    std::cout << "Auto-exited stasis" << std::endl;
                }
            }

            // ВЫВОД В КОНСОЛЬ
            if (step % 100 == 0) {
                const auto& stats = statistics.getCurrentStats();
                const auto& metrics = evolution.getCurrentMetrics();
                std::cout << "\rStep " << step 
                          << " | Energy: " << stats.total_energy
                          << " | Fitness: " << metrics.overall_fitness
                          << " | Best: " << bestFitness
                          << " | Memory: " << memory.size() << "/" << memConfig.max_records
                          << " | CPU: " << resources.getCurrentLoad() << "%   ";
                std::cout.flush();
            }
        }

        // ВИЗУАЛИЗАЦИЯ
        window.clear();

        if (visConfig.enabled && !system_in_stasis) {
            visualization.updateDynamicRange(neuralSystem);
            visualization.draw(window, neuralSystem);
        }

        ui.draw(window, neuralSystem, statistics, simulation_running && !system_in_stasis, 
                resources, evolution, memory, step);
        
        window.display();
    }

    // ФИНАЛЬНОЕ СОХРАНЕНИЕ
    std::cout << "\n\n💾 Saving all data to dump/ folder..." << std::endl;
    
    statistics.saveToFile("dump/simulation_statistics.csv");
    evolution.saveEvolutionState();
    memory.saveToFile("dump/final_memory.dat");
    language.saveAll();  // Сохраняем языковые данные
    
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
        meta << "Final memory records: " << memory.size() << "/" << memConfig.max_records << "\n";
        meta << "Actions taken: " << actionsTaken << "\n";
        meta << "Learned words: " << language.getLearnedWordsCount() << "\n";  // Потребуется метод getLearnedWordsCount()
        meta.close();
    }
    
    std::cout << "\n=== FINAL STATS ===" << std::endl;
    std::cout << "Total steps: " << step << std::endl;
    std::cout << "Best fitness: " << bestFitness << std::endl;
    std::cout << "Memory records: " << memory.size() << "/" << memConfig.max_records << std::endl;
    std::cout << "Actions taken: " << actionsTaken << std::endl;
    std::cout << "Neurons: " << NeuralFieldSystem::TOTAL_NEURONS << " ("
              << NeuralFieldSystem::NUM_GROUPS << "x" << NeuralFieldSystem::GROUP_SIZE << ")" << std::endl;
    std::cout << "All data saved to dump/ folder" << std::endl;

    return 0;
}