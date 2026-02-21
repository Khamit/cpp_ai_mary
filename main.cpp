#include <SFML/Graphics.hpp>
#include <iostream>
#include <fstream>
#include <random>
#include <chrono>
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
#include <algorithm>
#include <cctype>
#include "data/MemoryModule.hpp"

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

// Реализации методов ConfigLoader
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
        std::cout << "⚠️ Config file not found, using defaults: " << filename << std::endl;
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
    // Создаем папку dump если её нет
    std::filesystem::create_directories("dump");
    
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

    // Параметры системы из конфигурации
    // ========== ИСПРАВЛЕНО: Используем 1024 нейрона (32x32) ==========
    int Nside = 32;  // Было 20, теперь 32 для 1024 нейронов
    double dt = 0.001;
    double m = 1.0;
    double lam = 0.5;
    unsigned int windowWidth = 800;  // Увеличим окно для лучшего отображения
    unsigned int windowHeight = 700;

    // Проверка: действительно ли 32*32 = 1024?
    std::cout << "! Neural field size: " << Nside << "x" << Nside 
              << " = " << (Nside * Nside) << " neurons" << std::endl;
    
    // Инициализация системы и модулей
    NeuralFieldSystem neuralSystem(Nside, dt, m, lam);
    
    std::mt19937 rng(42);
    neuralSystem.initializeRandom(rng, 0.1, 0.02);
    
    // Инициализация новых модулей с конфигурацией
    ImmutableCore immutable_core;
    EvolutionModule evolution(immutable_core, evolConfig);
    ResourceMonitor resources(resConfig);  // Теперь должно работать

    evolution.testEvolutionMethods();
    
    LearningModule learning(learnConfig);
    DynamicsModule dynamics(dynConfig);

    // ========== ИНИЦИАЛИЗАЦИЯ МОДУЛЯ ПАМЯТИ ==========
    // ========== ИСПРАВЛЕНО: Модуль памяти с правильной размерностью ==========
    MemoryConfig memConfig;
    memConfig.max_records = 5000;      // Максимум 5000 воспоминаний
    memConfig.feature_dim = 64;         // 64 признака (совпадает с getFeatures)
    memConfig.global_decay_factor = 0.995f; // Забывание 0.5% за шаг
    memConfig.similarity_threshold = 0.7f;  // Порог схожести
    
    MemoryController memory(memConfig);
    
    // Переменные для работы с памятью
    float bestFitness = 0.0f;
    uint8_t lastAction = 0;
    float lastReward = 0.0f;
    float cumulativeReward = 0.0f;
    int actionsTaken = 0;
    
    std::cout << "🧠 Memory module initialized:" << std::endl;
    std::cout << "   - Max records: " << memConfig.max_records << std::endl;
    std::cout << "   - Feature dim: " << memConfig.feature_dim << std::endl;
    std::cout << "   - Decay factor: " << memConfig.global_decay_factor << std::endl;
    // ====================================================================

    // Используем методы из UIModule для получения размеров визуализации
    UIModule ui(uiConfig, windowWidth, windowHeight);
    int visWidth = ui.getVisualizationWidth();
    int visHeight = ui.getVisualizationHeight();
    
    VisualizationModule visualization(visConfig, Nside, visWidth, visHeight);
    InteractionModule interaction(interConfig, Nside, visWidth / float(Nside));
    StatisticsModule statistics;

    // Создание окна
    sf::RenderWindow window(sf::VideoMode({windowWidth, windowHeight}), 
                           "Advanced Neural Field System with Evolution");

    // Переменные симуляции
    int step = 0;
    bool simulation_running = false;
    bool system_in_stasis = false;

    // Загружаем предыдущую память если есть
    if (std::ifstream("best_memory.dat").good()) {
        if (memory.loadFromFile("best_memory.dat")) {
            std::cout << "📀 Loaded " << memory.size() << " memories from file" << std::endl;
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
            else if (const auto* mousePressed = event.getIf<sf::Event::MouseButtonPressed>()) {
                int mouseX = mousePressed->position.x;
                int currentVisWidth = ui.getVisualizationWidth();
                
                if (mouseX < currentVisWidth && !system_in_stasis) {
                    interaction.handleMouseClick(*mousePressed, neuralSystem);
                    // Считаем клик пользователя как положительное подкрепление
                    lastReward += 0.1f;
                    cumulativeReward += 0.1f;
                } else {
                    ui.handleMouseClick(*mousePressed, neuralSystem, simulation_running, statistics);
                }
            }
            else {
                ui.handleEvents(window, neuralSystem, simulation_running, statistics);
            }
        }

        // МОНИТОРИНГ РЕСУРСОВ
        resources.update();
        
        // УМНАЯ ПРОВЕРКА ПЕРЕГРУЗКИ
        if (!system_in_stasis && resources.checkAndTriggerOverload()) {
            std::cout << "⚠️ System overload detected! Reducing memory activity..." << std::endl;
            // Временно уменьшаем использование памяти
        }

        if (simulation_running) {
            auto start_time = std::chrono::high_resolution_clock::now();
            
            // ОСНОВНАЯ СИМУЛЯЦИЯ
            neuralSystem.symplecticEvolution();
            
            if (dynConfig.enabled && !system_in_stasis)
                dynamics.applyDynamics(neuralSystem);
                
            if (learnConfig.enabled && !system_in_stasis)
                learning.applyLearning(neuralSystem);

            auto end_time = std::chrono::high_resolution_clock::now();
            double step_time = std::chrono::duration<double>(end_time - start_time).count();
            
            // ЭВОЛЮЦИЯ И ОЦЕНКА
            evolution.evaluateFitness(neuralSystem, step_time);
            
            // ========== РАБОТА С ПАМЯТЬЮ (ИСПРАВЛЕНО) ==========
            if (!system_in_stasis) {
                // 1. Получаем текущие признаки из нейронов
                std::vector<float> currentFeatures = neuralSystem.getFeatures();
                
                // 2. Ищем похожие ситуации
                auto similarIndices = memory.findSimilar(currentFeatures, 3);
                
                // 3. Принимаем решение на основе прошлого опыта
                if (!similarIndices.empty()) {
                    // Взвешенное голосование по похожим воспоминаниям
                    std::vector<int> actionVotes(10, 0); // максимум 10 действий
                    
                    for (size_t idx : similarIndices) {
                        const auto& past = memory.getRecords()[idx];
                        actionVotes[past.action % 10] += static_cast<int>(past.utility * 10);
                    }
                    
                    // Выбираем действие с наибольшим количеством голосов
                    lastAction = std::max_element(actionVotes.begin(), actionVotes.end()) 
                               - actionVotes.begin();
                } else {
                    // Случайное исследование, если нет опыта
                    lastAction = rand() % 5;
                }
                
                // 4. Вычисляем награду на основе fitness и взаимодействий
                float currentFitness = evolution.getOverallFitness();
                float fitnessDelta = currentFitness - bestFitness;
                
                // Базовая награда от улучшения fitness
                lastReward = fitnessDelta * 10.0f;
                
                // Добавляем небольшую случайность для исследования
                float explorationBonus = ((rand() % 100) / 10000.0f) - 0.005f;
                lastReward += explorationBonus;
                
                // 5. Вычисляем полезность (utility)
                float utility = (currentFitness * 0.5f) + (cumulativeReward * 0.3f) + 0.2f;
                
                // 6. Создаём запись в памяти
                MemoryRecord newRec(currentFeatures, lastAction, lastReward, utility);
                memory.addRecord(newRec);
                
                // 7. Периодическое забывание
                if (step % 200 == 0 && step > 0) {
                    memory.decayAll();
                }
                
                // Сохраняем лучшую память
                if (currentFitness > bestFitness) {
                    bestFitness = currentFitness;
                    memory.saveToFile("dump/best_memory.dat");
                    std::cout << "\n💾 New best fitness: " << bestFitness 
                              << " | Records: " << memory.size() << std::endl;
                }
                
                // Автосохранение каждые 1000 шагов
                if (step % 1000 == 0 && step > 0) {
                    std::string checkpointFile = "dump/memory_checkpoint_" + std::to_string(step) + ".dat";
                    memory.saveToFile(checkpointFile);
                    std::cout << "\n💾 Checkpoint saved: " << checkpointFile << std::endl;
                }
                
                actionsTaken++;
                if (actionsTaken % 100 == 0) {
                    cumulativeReward *= 0.9f;
                }
            }
            // ====================================================

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
                    std::cout << "✅ Auto-exited stasis" << std::endl;
                }
            }

            // ВЫВОД В КОНСОЛЬ (исправлено для 1024 нейронов)
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

            step++;
        }

    // ВИЗУАЛИЗАЦИЯ
    window.clear();

    if (visConfig.enabled && !system_in_stasis) {
        visualization.updateDynamicRange(neuralSystem);
        visualization.draw(window, neuralSystem);
    }

        // Передаем дополнительные параметры в UI
        ui.draw(window, neuralSystem, statistics, simulation_running && !system_in_stasis, 
                resources, evolution, memory, step);  // Добавили memory и step

        window.display();
    }

    // ФИНАЛЬНОЕ СОХРАНЕНИЕ ВСЕГО В ПАПКУ DUMP
    std::cout << "\n\n💾 Saving all data to dump/ folder..." << std::endl;
    
    // Сохраняем статистику
    statistics.saveToFile("dump/simulation_statistics.csv");
    
    // Сохраняем состояние эволюции
    evolution.saveEvolutionState(); // Убедитесь что этот метод сохраняет в dump/
    
    // Сохраняем финальную память
    memory.saveToFile("dump/final_memory.dat");
    
    // Сохраняем конфигурацию для истории
    std::filesystem::copy_file("config/system_config.json", 
                               "dump/last_config.json", 
                               std::filesystem::copy_options::overwrite_existing);
    
    // Сохраняем метаданные
    std::ofstream meta("dump/meta.txt");
    if (meta.is_open()) {
        meta << "=== SIMULATION METADATA ===\n";
        meta << "Date: " << __DATE__ << " " << __TIME__ << "\n";
        meta << "Total steps: " << step << "\n";
        meta << "Best fitness: " << bestFitness << "\n";
        meta << "Final memory records: " << memory.size() << "/" << memConfig.max_records << "\n";
        meta << "Actions taken: " << actionsTaken << "\n";
        meta << "Neurons: " << (Nside * Nside) << " (" << Nside << "x" << Nside << ")\n";
        meta << "CPU threshold: " << resConfig.cpu_threshold << "%\n";
        meta.close();
    }
    
    std::cout << "\n=== FINAL STATS ===" << std::endl;
    std::cout << "Total steps: " << step << std::endl;
    std::cout << "Best fitness: " << bestFitness << std::endl;
    std::cout << "Memory records: " << memory.size() << "/" << memConfig.max_records << std::endl;
    std::cout << "Actions taken: " << actionsTaken << std::endl;
    std::cout << "Neurons: " << (Nside * Nside) << " (32x32)" << std::endl;
    std::cout << "All data saved to dump/ folder" << std::endl;

    return 0;
}