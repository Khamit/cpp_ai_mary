//cpp_ai_test/modules/UIModule.cpp
#include "UIModule.hpp"
#include <sstream>
#include <iomanip>
#include <iostream>

UIModule::UIModule(const UIConfig& config, int windowWidth, int windowHeight)
    : config(config),
      windowWidth(windowWidth),
      windowHeight(windowHeight),
            // Инициализируем тексты через конструктор, принимающий шрифт
      startText(font),
      stopText(font),
      resetText(font),
      statsText(font)
{
    if (!font.openFromFile("SF-Pro-Display-Regular.otf")) {
        std::cerr << "Failed to load font!\n";
    }
    
    float buttonWidth = 180.0f;
    float buttonHeight = 40.0f;
    float startX = static_cast<float>(windowWidth - config.control_panel_width + 10);
    
    // Кнопки
    startButton.setSize(sf::Vector2f(buttonWidth, buttonHeight));
    startButton.setPosition(sf::Vector2f(startX, 50.0f));
    startButton.setFillColor(sf::Color(100, 200, 100));
    
    stopButton.setSize(sf::Vector2f(buttonWidth, buttonHeight));
    stopButton.setPosition(sf::Vector2f(startX, 100.0f));
    stopButton.setFillColor(sf::Color(200, 100, 100));
    
    resetButton.setSize(sf::Vector2f(buttonWidth, buttonHeight));
    resetButton.setPosition(sf::Vector2f(startX, 150.0f));
    resetButton.setFillColor(sf::Color(100, 100, 200));
    
    // Текст кнопок
    startText.setFont(font);
    startText.setString("Start Simulation");
    startText.setCharacterSize(16);
    startText.setFillColor(sf::Color::White);
    startText.setPosition(sf::Vector2f(startX + 20.0f, 60.0f));
    
    stopText.setFont(font);
    stopText.setString("Stop Simulation");
    stopText.setCharacterSize(16);
    stopText.setFillColor(sf::Color::White);
    stopText.setPosition(sf::Vector2f(startX + 20.0f, 110.0f));
    
    resetText.setFont(font);
    resetText.setString("Reset System");
    resetText.setCharacterSize(16);
    resetText.setFillColor(sf::Color::White);
    resetText.setPosition(sf::Vector2f(startX + 40.0f, 160.0f));
    
    statsText.setFont(font);
    statsText.setString("Statistics:\n\nLoading...");
    statsText.setCharacterSize(14);
    statsText.setFillColor(sf::Color::White);
    statsText.setPosition(sf::Vector2f(startX, 220.0f));
}

void UIModule::handleEvents(sf::RenderWindow& window, NeuralFieldSystem& system, 
                           bool& simulation_running, StatisticsModule& stats) 
{
    // Эта функция теперь обрабатывает только НЕ мышиные события
    if (auto eventOpt = window.pollEvent())
    {
        const sf::Event& event = *eventOpt;

        // Закрытие окна
        if (event.is<sf::Event::Closed>())
        {
            window.close();
        }
        // Другие события (клавиатура и т.д.) можно обрабатывать здесь
    }
}

void UIModule::handleMouseClick(const sf::Event::MouseButtonPressed& event, NeuralFieldSystem& system, 
                               bool& simulation_running, StatisticsModule& stats) {
    sf::Vector2f mousePos{
        static_cast<float>(event.position.x),
        static_cast<float>(event.position.y)
    };

    if (startButton.getGlobalBounds().contains(mousePos)) {
        simulation_running = true;
        stats.startTimer();
        std::cout << "Simulation STARTED" << std::endl;
    }
    else if (stopButton.getGlobalBounds().contains(mousePos)) {
        simulation_running = false;
        stats.stopTimer();
        std::cout << "Simulation STOPPED" << std::endl;
    }
    else if (resetButton.getGlobalBounds().contains(mousePos)) {
        simulation_running = false;
        stats.stopTimer();
        stats.reset();
        
        std::mt19937 rng(42);
        system.initializeRandom(rng, 0.1, 0.02);
        std::cout << "System RESET" << std::endl;
    }
}

void UIModule::draw(sf::RenderWindow& window, const NeuralFieldSystem& system, 
                   const StatisticsModule& stats, bool simulation_running) {
    
    if (!config.show_controls) return;

    sf::RectangleShape panel;
    panel.setSize(sf::Vector2f(
        static_cast<float>(config.control_panel_width),
        static_cast<float>(windowHeight)
    ));
    panel.setPosition(sf::Vector2f(
        static_cast<float>(windowWidth - config.control_panel_width),
        0.0f
    ));

    panel.setFillColor(sf::Color(50, 50, 50, 200));
    window.draw(panel);
    
    drawControlPanel(window, stats, simulation_running);
    
    if (config.show_stats) {
        drawStatistics(window, stats);
    }
}

void UIModule::drawControlPanel(sf::RenderWindow& window, const StatisticsModule& stats, bool simulation_running) {
    
    startButton.setFillColor(simulation_running ? sf::Color(70, 170, 70) 
                                                : sf::Color(100, 200, 100));
    stopButton.setFillColor(!simulation_running ? sf::Color(170, 70, 70)
                                                : sf::Color(200, 100, 100));
    
    window.draw(startButton);
    window.draw(stopButton);
    window.draw(resetButton);
    
    window.draw(startText);
    window.draw(stopText);
    window.draw(resetText);
    
    sf::Text statusText(font);
    statusText.setFont(font);
    statusText.setCharacterSize(16);
    statusText.setPosition(sf::Vector2f(
        static_cast<float>(windowWidth - config.control_panel_width + 10),
        10.0f
    ));
    
    if (simulation_running) {
        statusText.setString("Status: RUNNING");
        statusText.setFillColor(sf::Color::Green);
    } else {
        statusText.setString("Status: PAUSED");
        statusText.setFillColor(sf::Color::Yellow);
    }
    
    window.draw(statusText);
}

void UIModule::drawStatistics(sf::RenderWindow& window, const StatisticsModule& stats) {

    const auto& current = stats.getCurrentStats();
    
    std::stringstream ss;
    ss << "Statistics:\n\n"
       << "Step: " << current.step << "\n"
       << "Time: " << formatDouble(current.simulation_time) << "s\n"
       << "Energy: " << formatDouble(current.total_energy) << "\n"
       << "Avg Phi: " << formatDouble(current.avg_phi) << "\n"
       << "Avg Pi: " << formatDouble(current.avg_pi) << "\n"
       << "Min Phi: " << formatDouble(current.min_phi) << "\n"
       << "Max Phi: " << formatDouble(current.max_phi) << "\n"
       << "Std Phi: " << formatDouble(current.std_phi) << "\n"
       << "State: " << current.state << "\n"
       << "History: " << stats.getHistory().size() << " records";
    
    statsText.setString(ss.str());
    window.draw(statsText);
}

std::string UIModule::formatDouble(double value, int precision) const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(precision) << value;
    return ss.str();
}