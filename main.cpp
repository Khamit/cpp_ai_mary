#include <SFML/Graphics.hpp>
#include <iostream>
#include <fstream>
#include <random>
#include "core/NeuralFieldSystem.hpp"
#include "modules/LearningModule.hpp"
#include "modules/DynamicsModule.hpp"
#include "modules/VisualizationModule.hpp"
#include "modules/InteractionModule.hpp"
#include "modules/StatisticsModule.hpp"
#include "modules/UIModule.hpp"

// Простой загрузчик JSON (упрощенный)
class ConfigLoader {
public:
    static bool loadFromFile(const std::string& filename, 
                            LearningConfig& learnConfig,
                            DynamicsConfig& dynConfig,
                            VisualizationConfig& visConfig,
                            InteractionConfig& interConfig,
                            UIConfig& uiConfig) {
        // Здесь будет парсинг JSON файла
        // Для простоты используем значения по умолчанию
        return true;
    }
};

int main() {
    // Загрузка конфигурации
    LearningConfig learnConfig;
    DynamicsConfig dynConfig;
    VisualizationConfig visConfig;
    InteractionConfig interConfig;
    UIConfig uiConfig;
    
    ConfigLoader::loadFromFile("config/system_config.json", 
                              learnConfig, dynConfig, visConfig, interConfig, uiConfig);

    // Параметры системы
    const int Nside = 20;
    const double dt = 0.001;
    const double m = 1.0;
    const double lam = 0.5;
    const unsigned int windowWidth = 600;
    const unsigned int windowHeight = 500;

    // Инициализация системы и модулей
    NeuralFieldSystem system(Nside, dt, m, lam);
    
    std::mt19937 rng(42);
    system.initializeRandom(rng, 0.1, 0.02);
    
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
                           "Advanced Neural Field System");

    int step = 0;
    bool simulation_running = false;

    while (window.isOpen()) {
        // Обработка событий - УПРОЩЕННАЯ ВЕРСИЯ
        while (auto eventOpt = window.pollEvent()) {
            const auto& event = *eventOpt;
            
            // Обработка закрытия окна
            if (event.is<sf::Event::Closed>()) {
                window.close();
            }
            
            // Обработка нажатий мыши
            else if (const auto* mousePressed = event.getIf<sf::Event::MouseButtonPressed>()) {
                // Определяем область клика
                int mouseX = mousePressed->position.x;
                
                if (mouseX < visWidth) {
                    // Клик в области визуализации - обрабатываем взаимодействие
                    interaction.handleMouseClick(*mousePressed, system);
                    std::cout << "Interaction at (" << mousePressed->position.x << ", " 
                              << mousePressed->position.y << ")" << std::endl;
                } else {
                    // Клик в области UI - передаем в UI модуль
                    ui.handleMouseClick(*mousePressed, system, simulation_running, statistics);
                }
            }
            
            // Обрабатываем другие события (не мышь) через UI модуль
            else {
                ui.handleEvents(window, system, simulation_running, statistics);
            }
        }

        if (simulation_running) {
            // Основная симуляция
            system.symplecticEvolution();
            
            if (dynConfig.enabled)
                dynamics.applyDynamics(system);
                
            if (learnConfig.enabled)
                learning.applyLearning(system);

            // Обновление статистики
            statistics.update(system, step, dt);

            // Вывод в консоль каждые 100 шагов
            if (step % 100 == 0) {
                const auto& stats = statistics.getCurrentStats();
                std::cout << "Step " << step 
                          << " | Energy: " << stats.total_energy
                          << " | Avg Phi: " << stats.avg_phi
                          << " | State: " << stats.state
                          << " | Time: " << stats.simulation_time << "s\r";
                std::cout.flush();
            }

            step++;
        }

        // Визуализация
        window.clear();
        
        if (visConfig.enabled) {
            visualization.updateDynamicRange(system);
            visualization.draw(window, system);
        }
        
        // Интерфейс
        ui.draw(window, system, statistics, simulation_running);
        
        window.display();
    }

    // Сохранение статистики при завершении
    statistics.saveToFile("simulation_statistics.csv");
    
    const auto& final_stats = statistics.getCurrentStats();
    std::cout << "\n\n=== SIMULATION COMPLETE ===" << std::endl;
    std::cout << "Total steps: " << final_stats.step << std::endl;
    std::cout << "Simulation time: " << final_stats.simulation_time << "s" << std::endl;
    std::cout << "Final energy: " << final_stats.total_energy << std::endl;
    std::cout << "Final state: " << final_stats.state << std::endl;
    std::cout << "Statistics saved to: simulation_statistics.csv" << std::endl;

    return 0;
}