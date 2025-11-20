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
// new one 
#include "core/ImmutableCore.hpp"
#include "modules/EvolutionModule.hpp"
#include "modules/ResourceMonitor.hpp"
//
#include <algorithm> // для std::remove и std::remove_if
#include <cctype>    // для ::isspace

// Простой загрузчик JSON (упрощенный)
// Улучшенный загрузчик JSON (упрощенный, но с поддержкой вашей конфигурации)
class ConfigLoader {
public:
    static bool loadFromFile(const std::string& filename, 
                            LearningConfig& learnConfig,
                            DynamicsConfig& dynConfig,
                            VisualizationConfig& visConfig,
                            InteractionConfig& interConfig,
                            UIConfig& uiConfig);
    
    // ДЕЛАЕМ МЕТОДЫ ПУБЛИЧНЫМИ
    static double getDoubleValue(const std::string& content, const std::string& key, double defaultValue);
    static int getIntValue(const std::string& content, const std::string& key, int defaultValue);
    static bool getBoolValue(const std::string& content, const std::string& key, bool defaultValue);
    static std::string getStringValue(const std::string& content, const std::string& key, const std::string& defaultValue);

private:
    // Вспомогательные методы для парсинга
    static std::string extractValueString(const std::string& content, const std::string& key);
};

// Реализации методов ConfigLoader ВНЕ класса
bool ConfigLoader::loadFromFile(const std::string& filename, 
                               LearningConfig& learnConfig,
                               DynamicsConfig& dynConfig,
                               VisualizationConfig& visConfig,
                               InteractionConfig& interConfig,
                               UIConfig& uiConfig) {
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "⚠️ Config file not found, using defaults: " << filename << std::endl;
        return false;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)), 
                       std::istreambuf_iterator<char>());
    file.close();
    
    // Простой парсинг ключевых параметров
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
    
    std::cout << "✅ Configuration loaded from: " << filename << std::endl;
    return true;
}

// Вспомогательный метод для извлечения строки значения
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
        // Удаляем пробелы и кавычки
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
    
    // Удаляем пробелы и кавычки
    valueStr.erase(std::remove_if(valueStr.begin(), valueStr.end(), ::isspace), valueStr.end());
    valueStr.erase(std::remove(valueStr.begin(), valueStr.end(), '\"'), valueStr.end());
    
    return valueStr == "true";
}

std::string ConfigLoader::getStringValue(const std::string& content, const std::string& key, const std::string& defaultValue) {
    std::string valueStr = extractValueString(content, key);
    if (valueStr.empty()) return defaultValue;
    
    // Удаляем пробелы и кавычки
    valueStr.erase(std::remove_if(valueStr.begin(), valueStr.end(), ::isspace), valueStr.end());
    valueStr.erase(std::remove(valueStr.begin(), valueStr.end(), '\"'), valueStr.end());
    
    return valueStr.empty() ? defaultValue : valueStr;
}
// configLoader

// main
int main() {
    // Загрузка конфигурации ПЕРВОЙ
    LearningConfig learnConfig;
    DynamicsConfig dynConfig;
    VisualizationConfig visConfig;
    InteractionConfig interConfig;
    UIConfig uiConfig;
    
    bool configLoaded = ConfigLoader::loadFromFile("config/system_config.json", 
                              learnConfig, dynConfig, visConfig, interConfig, uiConfig);

    // Параметры системы из конфигурации
    int Nside = 20;
    double dt = 0.001;
    double m = 1.0;
    double lam = 0.5;
    unsigned int windowWidth = 600;
    unsigned int windowHeight = 500;
    
    // Если конфиг загружен, пытаемся прочитать системные параметры
    if (configLoaded) {
        std::ifstream config_file("config/system_config.json");
        if (config_file.is_open()) {
            std::string content((std::istreambuf_iterator<char>(config_file)), 
                               std::istreambuf_iterator<char>());
            config_file.close();
            
            Nside = ConfigLoader::getIntValue(content, "\"Nside\"", 20);
            dt = ConfigLoader::getDoubleValue(content, "\"dt\"", 0.001);
            m = ConfigLoader::getDoubleValue(content, "\"m\"", 1.0);
            lam = ConfigLoader::getDoubleValue(content, "\"lam\"", 0.5);
            windowWidth = ConfigLoader::getIntValue(content, "\"windowWidth\"", 600);
            windowHeight = ConfigLoader::getIntValue(content, "\"windowHeight\"", 500);
            /* убираем многократный вызов - а зачем он нужен - зря засоряет лог
            std::cout << "System parameters: Nside=" << Nside 
                      << ", dt=" << dt << ", m=" << m 
                      << ", lam=" << lam << std::endl;
                      */
        }
    }

    // Инициализация системы и модулей
    NeuralFieldSystem neuralSystem(Nside, dt, m, lam);
    
    std::mt19937 rng(42);
    neuralSystem.initializeRandom(rng, 0.1, 0.02);
    
    // НОВЫЕ МОДУЛИ - инициализируем ПОСЛЕ создания neuralSystem
    ImmutableCore immutable_core;
    EvolutionModule evolution(immutable_core);
    ResourceMonitor resources;
    
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
        
        // ПРОВЕРКА ПЕРЕГРУЗКИ
        if (resources.isOverloaded() && !system_in_stasis) {
            std::cout << "⚠️ System overload detected!" << std::endl;
            if (!immutable_core.requestPermission("reduce_complexity")) {
                evolution.enterStasis(neuralSystem);
                system_in_stasis = true;
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
            
            // Временно для отладки - запускать эволюцию чаще
            if (step % 100 == 0 && !system_in_stasis) {  // было 1000
                evolution.proposeMutation(neuralSystem);
            }

            // И принудительно запускать оптимизацию
            if (step % 200 == 0 && !system_in_stasis) {  // было 1000
                std::cout << "🔄 FORCING evolution check at step " << step << std::endl;
                // Здесь можно принудительно вызвать методы эволюции
            }
            
            // ОБНОВЛЕНИЕ СТАТИСТИКИ
            statistics.update(neuralSystem, step, dt);

            // УПРАВЛЕНИЕ СТАЗИСОМ И ОПТИМИЗАЦИЯ КАЖДЫЕ 1000 ШАГОВ
            if (step % 1000 == 0) {
                if (system_in_stasis) {
                    // Автоматический выход из стазиса при хороших условиях
                    if (resources.getCurrentLoad() < 0.3) {
                        evolution.exitStasis();
                        system_in_stasis = false;
                        std::cout << "✅ Auto-exited stasis - good conditions" << std::endl;
                    }
                } else {
                    // Периодическая оптимизация при низкой приспособленности
                    if (evolution.getOverallFitness() < 0.6) {
                        std::cout << "🔄 Triggering optimization due to low fitness..." << std::endl;
                        evolution.proposeMutation(neuralSystem);
                    }
                }
            }

                // Если конфиг загружен, пытаемся прочитать системные параметры
            if (configLoaded) {
                std::ifstream config_file("config/system_config.json");
                if (config_file.is_open()) {
                    std::string content((std::istreambuf_iterator<char>(config_file)), 
                                    std::istreambuf_iterator<char>());
                    config_file.close();
                    
                    // Теперь методы публичные и доступны
                    Nside = ConfigLoader::getIntValue(content, "\"Nside\"", 20);
                    dt = ConfigLoader::getDoubleValue(content, "\"dt\"", 0.001);
                    m = ConfigLoader::getDoubleValue(content, "\"m\"", 1.0);
                    lam = ConfigLoader::getDoubleValue(content, "\"lam\"", 0.5);
                    windowWidth = ConfigLoader::getIntValue(content, "\"windowWidth\"", 600);
                    windowHeight = ConfigLoader::getIntValue(content, "\"windowHeight\"", 500);
                    
                    std::cout << "System parameters: Nside=" << Nside 
                            << ", dt=" << dt << ", m=" << m 
                            << ", lam=" << lam << std::endl;
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
        
        // ИНТЕРФЕЙС
        ui.draw(window, neuralSystem, statistics, simulation_running && !system_in_stasis);
        
        window.display();
    }

    // СОХРАНЕНИЕ РЕЗУЛЬТАТОВ ЭВОЛЮЦИИ
    statistics.saveToFile("simulation_statistics.csv");
    
    const auto& final_stats = statistics.getCurrentStats();
    const auto& final_metrics = evolution.getCurrentMetrics();
    
    std::cout << "\n\n=== EVOLUTION COMPLETE ===" << std::endl;
    std::cout << "Total steps: " << final_stats.step << std::endl;
    std::cout << "Final fitness: " << final_metrics.overall_fitness << std::endl;
    std::cout << "Final energy: " << final_stats.total_energy << std::endl;
    std::cout << "Time in stasis: " << (system_in_stasis ? "Yes" : "No") << std::endl;
    std::cout << "Configuration used: " << (configLoaded ? "config/system_config.json" : "defaults") << std::endl;

    return 0;
}