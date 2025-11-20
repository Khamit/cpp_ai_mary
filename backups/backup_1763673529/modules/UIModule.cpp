//cpp_ai_test/modules/UIModule.cpp
#include "UIModule.hpp"
#include <sstream>
#include <iomanip>
#include <iostream>

// Apple-like dark UI palette
static const sf::Color PANEL_BG(30, 32, 34, 180);
static const sf::Color BOTTOM_PANEL_BG(25, 27, 29, 220);
static const sf::Color BTN_BG(50, 52, 55);
static const sf::Color BTN_HOVER(65, 68, 72);
static const sf::Color BTN_ACTIVE(90, 95, 100);
static const sf::Color TEXT_MAIN(230, 230, 235);
static const sf::Color TEXT_ACCENT(100, 180, 255);
static const sf::Color TEXT_WARNING(255, 180, 100);
static const sf::Color TEXT_SUCCESS(100, 255, 180);
static const sf::Color TEXT_INFO(180, 180, 200);
static const sf::Color TEXT_BOTTOM(245, 245, 245); // белый


UIModule::UIModule(const UIConfig& config, int windowWidth, int windowHeight)
    : config(config),
      windowWidth(windowWidth),
      windowHeight(windowHeight),
      startText(font),
      stopText(font),
      resetText(font),
      statsText(font),
      evolutionText(font),
      stasisText(font),
      resourceText(font),
      configText(font)
{
    if (!font.openFromFile("SF-Pro-Display-Regular.otf")) {
        std::cerr << "Failed to load font!\n";
    }
    bottom_panel_height = 120.0f; // увеличиваем пространство снизу
    float buttonWidth = 180.0f;
    float buttonHeight = 42.0f;
    float startX = static_cast<float>(windowWidth - config.control_panel_width + 10);
    
    // Кнопки
    startButton.setSize(sf::Vector2f(buttonWidth, buttonHeight));
    startButton.setPosition(sf::Vector2f(startX, 50.0f));
    startButton.setFillColor(BTN_BG);
    startButton.setOutlineColor(sf::Color(80, 80, 85));
    startButton.setOutlineThickness(1.0f);
    
    stopButton = startButton;
    stopButton.setPosition(sf::Vector2f(startX, 100.0f));
    
    resetButton = startButton;
    resetButton.setPosition(sf::Vector2f(startX, 150.0f));
    
    // Текст кнопок
    startText.setFont(font);
    startText.setString("Start Simulation");
    startText.setCharacterSize(16);
    startText.setFillColor(TEXT_MAIN);
    startText.setPosition(sf::Vector2f(startX + 20.0f, 60.0f));
    
    stopText.setFont(font);
    stopText.setString("Stop Simulation");
    stopText.setCharacterSize(16);
    stopText.setFillColor(TEXT_MAIN);
    stopText.setPosition(sf::Vector2f(startX + 20.0f, 110.0f));
    
    resetText.setFont(font);
    resetText.setString("Reset System");
    resetText.setCharacterSize(16);
    resetText.setFillColor(TEXT_MAIN);
    resetText.setPosition(sf::Vector2f(startX + 40.0f, 160.0f));
    
    // Статистика
    statsText.setFont(font);
    statsText.setString("Statistics:\n\nLoading...");
    statsText.setCharacterSize(14);
    statsText.setFillColor(TEXT_MAIN);
    statsText.setPosition(sf::Vector2f(startX, 220.0f));
    
    // Эволюция
    evolutionText.setFont(font);
    evolutionText.setString("Evolution:\n\nLoading...");
    evolutionText.setCharacterSize(14);
    evolutionText.setFillColor(TEXT_MAIN);
    evolutionText.setPosition(sf::Vector2f(startX, 350.0f));
    
    // Стазис
    stasisText.setFont(font);
    stasisText.setString("");
    stasisText.setCharacterSize(16);
    stasisText.setFillColor(TEXT_WARNING);
    stasisText.setPosition(sf::Vector2f(startX, 500.0f));
    
    // Нижняя панель
    bottomPanel.setSize(sf::Vector2f(static_cast<float>(windowWidth), bottom_panel_height));
    bottomPanel.setPosition(sf::Vector2f(0, static_cast<float>(windowHeight - bottom_panel_height)));
    bottomPanel.setFillColor(BOTTOM_PANEL_BG); // фон панели тёмный

    // Текст для нижней панели
    resourceText.setFont(font);
    resourceText.setCharacterSize(14);
    resourceText.setFillColor(TEXT_BOTTOM); // белый
    resourceText.setPosition(sf::Vector2f(10.0f, static_cast<float>(windowHeight - bottom_panel_height + 15)));

    configText.setFont(font);
    configText.setCharacterSize(14);
    configText.setFillColor(TEXT_BOTTOM); // белый
    configText.setPosition(sf::Vector2f(static_cast<float>(windowWidth / 2), static_cast<float>(windowHeight - bottom_panel_height + 15)));
}

void UIModule::handleEvents(sf::RenderWindow& window, NeuralFieldSystem& system, 
                           bool& simulation_running, StatisticsModule& stats) 
{
    if (auto eventOpt = window.pollEvent())
    {
        const sf::Event& event = *eventOpt;

        if (event.is<sf::Event::Closed>())
        {
            window.close();
        }
    }
}

void UIModule::handleMouseClick(const sf::Event::MouseButtonPressed& event, NeuralFieldSystem& system, 
                               bool& simulation_running, StatisticsModule& stats) {
    sf::Vector2f mousePos{
        static_cast<float>(event.position.x),
        static_cast<float>(event.position.y)
    };

    // Проверяем, что клик не в нижней панели
    if (mousePos.y > windowHeight - bottom_panel_height) {
        return; // Игнорируем клики в нижней панели
    }

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
                   const StatisticsModule& stats, bool simulation_running,
                   const ResourceMonitor& resources, const EvolutionModule& evolution) {
    
    // Рисуем нижнюю панель
    window.draw(bottomPanel);
    drawBottomPanel(window, resources, evolution);
    
    if (!config.show_controls) return;

    sf::RectangleShape panel;
    panel.setSize(sf::Vector2f(
        static_cast<float>(config.control_panel_width),
        static_cast<float>(windowHeight - bottom_panel_height) // Учитываем нижнюю панель
    ));
    panel.setPosition(sf::Vector2f(
        static_cast<float>(windowWidth - config.control_panel_width),
        0.0f
    ));
    panel.setFillColor(PANEL_BG);
    window.draw(panel);
    
    drawControlPanel(window, stats, simulation_running);
    
    if (config.show_stats) {
        drawStatistics(window, stats);
    }
}

void UIModule::drawControlPanel(sf::RenderWindow& window, const StatisticsModule& stats, bool simulation_running) {
    
    startButton.setFillColor(simulation_running ? BTN_ACTIVE : BTN_BG);
    stopButton.setFillColor(!simulation_running ? BTN_ACTIVE : BTN_BG);
    resetButton.setFillColor(BTN_BG);
    
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
        statusText.setString("* Running");
        statusText.setFillColor(TEXT_ACCENT);
    } else {
        statusText.setString("# Paused");
        statusText.setFillColor(TEXT_MAIN);
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
       << "State: " << current.state << "\n"
       << "History: " << stats.getHistory().size() << " records";
    
    statsText.setString(ss.str());
    window.draw(statsText);
    
    // Эволюция
    std::stringstream evolution_ss;
    evolution_ss << "Evolution:\n\n"
                 << "Fitness: " << formatDouble(0.83) << "\n"
                 << "Best: " << formatDouble(0.91) << "\n"
                 << "Generation: " << stats.getHistory().size() / 100 << "\n"
                 << "Mutations: " << (current.step / 1000);
    
    evolutionText.setString(evolution_ss.str());
    window.draw(evolutionText);
    
    // Стазис
    if (stats.getCurrentStats().total_energy < 0.001) {
        stasisText.setString("⚠️ LOW ENERGY\nSTASIS MODE");
        stasisText.setFillColor(TEXT_WARNING);
        window.draw(stasisText);
    }
}

void UIModule::drawBottomPanel(sf::RenderWindow& window, const ResourceMonitor& resources, const EvolutionModule& evolution) {
    // Ресурсы
    std::stringstream resource_ss;
    resource_ss << "RESOURCES:\n"
                << "CPU: " << formatDouble(resources.getCurrentLoad() * 100, 1) << "%\n"
                << "Memory: " << formatDouble(resources.getMemoryUsage() * 100, 1) << "%\n"
                << "Overload: " << (resources.isOverloaded() ? "YES" : "NO") << "\n"
                << "Performance: " << formatDouble(resources.getPerformanceFactor() * 100, 1) << "%";
    
    resourceText.setString(resource_ss.str());
    window.draw(resourceText);
    
    // Конфигурация
    std::stringstream config_ss;
    config_ss << "CONFIGURATION:\n"
              << "CPU Threshold: " << formatDouble(resources.getCPUThreshold(), 1) << "%\n"
              << "Reduction Cooldown: " << evolution.getReductionCooldown() << "s\n"
              << "Max Reductions: " << evolution.getMaxReductionsPerMinute() << "/min\n"
              << "Min Fitness: " << formatDouble(evolution.getMinFitnessForOptimization(), 2);
    
    configText.setString(config_ss.str());
    window.draw(configText);
}

std::string UIModule::formatDouble(double value, int precision) const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(precision) << value;
    return ss.str();
}