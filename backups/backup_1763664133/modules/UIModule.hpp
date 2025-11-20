//cpp_ai_test/modules/UIModule.hpp
#pragma once
#include <SFML/Graphics.hpp>
#include "../core/NeuralFieldSystem.hpp"
#include "StatisticsModule.hpp"
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
             const StatisticsModule& stats, bool simulation_running);
    
    // Добавим методы для получения размеров визуализации
    int getVisualizationWidth() const { return windowWidth - config.control_panel_width; }
    int getVisualizationHeight() const { return windowHeight; }

private:
    UIConfig config;
    int windowWidth, windowHeight;
    
    sf::Font font;
    sf::RectangleShape startButton, stopButton, resetButton;
    sf::Text startText, stopText, resetText, statsText, evolutionText, stasisText;
    
    void drawControlPanel(sf::RenderWindow& window, const StatisticsModule& stats, bool simulation_running);
    void drawStatistics(sf::RenderWindow& window, const StatisticsModule& stats);
    std::string formatDouble(double value, int precision = 4) const;
};