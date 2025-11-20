//cpp_ai_test/modules/UIModule.hpp
#pragma once
#include <SFML/Graphics.hpp>
#include "../core/NeuralFieldSystem.hpp"
#include "StatisticsModule.hpp"
#include "ConfigStructs.hpp"
#include "ResourceMonitor.hpp"
#include "EvolutionModule.hpp"
#include <string>

struct UIConfig {
    bool show_controls = true;
    bool show_stats = true;
    int control_panel_width = 200;
};

class UIModule {
public:
    UIModule(const UIConfig& config, int windowWidth, int windowHeight);
    
    void handleEvents(sf::RenderWindow& window, NeuralFieldSystem& system, 
                     bool& simulation_running, StatisticsModule& stats);
    void handleMouseClick(const sf::Event::MouseButtonPressed& event, NeuralFieldSystem& system, 
                        bool& simulation_running, StatisticsModule& stats);
    void draw(sf::RenderWindow& window, const NeuralFieldSystem& system, 
             const StatisticsModule& stats, bool simulation_running,
             const ResourceMonitor& resources, const EvolutionModule& evolution);
    
    int getVisualizationWidth() const { return windowWidth - config.control_panel_width; }
    int getVisualizationHeight() const { return windowHeight - bottom_panel_height; }

private:
    UIConfig config;
    int windowWidth, windowHeight;
    int bottom_panel_height = 100; // Новая нижняя панель
    
    sf::Font font;
    sf::RectangleShape startButton, stopButton, resetButton;
    sf::Text startText, stopText, resetText, statsText, evolutionText, stasisText;
    sf::Text resourceText, configText; // Новые текстовые элементы
    sf::RectangleShape bottomPanel; // Новая нижняя панель
    
    void drawControlPanel(sf::RenderWindow& window, const StatisticsModule& stats, bool simulation_running);
    void drawStatistics(sf::RenderWindow& window, const StatisticsModule& stats);
    void drawBottomPanel(sf::RenderWindow& window, const ResourceMonitor& resources, const EvolutionModule& evolution);
    std::string formatDouble(double value, int precision = 4) const;
};