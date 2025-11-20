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
    
    std::cout << "✅ Full configuration loaded from: " << filename << std::endl;
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
    int Nside = 20;
    double dt = 0.001;
    double m = 1.0;
    double lam = 0.5;
    unsigned int windowWidth = 600;
    unsigned int windowHeight = 500;
    
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
        
        // УМНАЯ ПРОВЕРКА ПЕРЕГРУЗКИ - только если не в стазисе
        if (!system_in_stasis && resources.checkAndTriggerOverload()) {
            std::cout << "⚠️ System overload detected! Requesting complexity reduction..." << std::endl;
            
            // ПРЕДЛАГАЕМ УПРОЩЕНИЕ, НО НЕ ФОРСИРУЕМ СТАЗИС
            if (immutable_core.requestPermission("reduce_complexity")) {
                evolution.proposeMutation(neuralSystem);
            } else {
                std::cout << "🚫 Permission denied for complexity reduction" << std::endl;
            }
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
            
            // УМНЫЙ ВЫЗОВ ЭВОЛЮЦИИ - только при необходимости
            if (step % evolConfig.evolution_interval_steps == 0 && !system_in_stasis) {
                std::cout << "🔄 Evolution check at step " << step << std::endl;
                if (evolution.getOverallFitness() < evolConfig.min_fitness_for_optimization) {
                    evolution.proposeMutation(neuralSystem);
                } else {
                    std::cout << "✅ Fitness good, skipping evolution" << std::endl;
                }
            }
            
            // ОБНОВЛЕНИЕ СТАТИСТИКИ
            statistics.update(neuralSystem, step, dt);

            // УПРАВЛЕНИЕ СТАЗИСОМ
            if (step % 1000 == 0) {
                if (system_in_stasis) {
                    // Автоматический выход из стазиса при хороших условиях
                    if (resources.getCurrentLoad() < 30.0) {  // 30% вместо 0.3
                        evolution.exitStasis();
                        system_in_stasis = false;
                        std::cout << "✅ Auto-exited stasis - good conditions" << std::endl;
                    }
                }
            }

            // Обновление системных параметров из конфига если нужно
            if (configLoaded && step % 500 == 0) {
                std::ifstream config_file("config/system_config.json");
                if (config_file.is_open()) {
                    std::string content((std::istreambuf_iterator<char>(config_file)), 
                                    std::istreambuf_iterator<char>());
                    config_file.close();
                    
                    // Обновляем параметры если они изменились
                    int newNside = ConfigLoader::getIntValue(content, "\"Nside\"", 20);
                    double newDt = ConfigLoader::getDoubleValue(content, "\"dt\"", 0.001);
                    double newM = ConfigLoader::getDoubleValue(content, "\"m\"", 1.0);
                    double newLam = ConfigLoader::getDoubleValue(content, "\"lam\"", 0.5);
                    
                    if (newNside != Nside || newDt != dt || newM != m || newLam != lam) {
                        std::cout << "🔄 Updating system parameters from config..." << std::endl;
                        // Здесь можно переинициализировать систему с новыми параметрами
                    }
                }
            }

            // ВЫВОД В КОНСОЛЬ
            if (step % 100 == 0) {
                const auto& stats = statistics.getCurrentStats();
                const auto& metrics = evolution.getCurrentMetrics();
                std::cout << "Step " << step 
                          << " | Energy: " << stats.total_energy
                          << " | Fitness: " << metrics.overall_fitness
                          << " | Stasis: " << (system_in_stasis ? "YES" : "NO")
                          << " | CPU: " << resources.getCurrentLoad() << "%"
                          << " | Time: " << stats.simulation_time << "s\r";
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
        
        // ИНТЕРФЕЙС с передачей дополнительных данных
        ui.draw(window, neuralSystem, statistics, simulation_running && !system_in_stasis, resources, evolution);
        
        window.display();
    }

    // СОХРАНЕНИЕ РЕЗУЛЬТАТОВ ЭВОЛЮЦИИ
    statistics.saveToFile("simulation_statistics.csv");
    evolution.saveEvolutionState();
    
    const auto& final_stats = statistics.getCurrentStats();
    const auto& final_metrics = evolution.getCurrentMetrics();
    
    std::cout << "\n\n=== EVOLUTION COMPLETE ===" << std::endl;
    std::cout << "Total steps: " << final_stats.step << std::endl;
    std::cout << "Final fitness: " << final_metrics.overall_fitness << std::endl;
    std::cout << "Final energy: " << final_stats.total_energy << std::endl;
    std::cout << "Best fitness: " << evolution.getBestFitness() << std::endl;
    std::cout << "Time in stasis: " << (system_in_stasis ? "Yes" : "No") << std::endl;
    std::cout << "Configuration used: " << (configLoaded ? "config/system_config.json" : "defaults") << std::endl;

    return 0;
}