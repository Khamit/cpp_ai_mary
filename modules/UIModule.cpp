//cpp_ai_test/modules/UIModule.cpp
#include "UIModule.hpp"
#include <sstream>
#include <iomanip>
#include <iostream>
#include "../data/MemoryModule.hpp"

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
      configText(font),
      debugButtonText(font),
      debugInfoText(font),
      inputText(font),
      chatHistoryText(font),
      sendText(font),
      likeText(font),
      dislikeText(font)
{
    // В конструкторе UIModule добавьте более информативное сообщение
    if (!font.openFromFile("SF-Pro-Display-Regular.otf")) {
        std::cerr << "Failed to load font 'SF-Pro-Display-Regular.otf'!" << std::endl;
        // Попробуйте загрузить системный шрифт как запасной вариант
        if (!font.openFromFile("/System/Library/Fonts/Helvetica.ttc")) {
            std::cerr << "Failed to load fallback font too!" << std::endl;
        }
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

    // ОТЛАДКА!!!
        // Добавить кнопку Debug в нижней части панели
    debugButton.setSize(sf::Vector2f(buttonWidth, buttonHeight));
    debugButton.setPosition(sf::Vector2f(startX, static_cast<float>(windowHeight - bottom_panel_height - 60)));
    debugButton.setFillColor(sf::Color(70, 70, 80));
    debugButton.setOutlineColor(sf::Color(120, 120, 130));
    debugButton.setOutlineThickness(1.0f);
    
    debugButtonText.setFont(font);
    debugButtonText.setString("Show Debug");
    debugButtonText.setCharacterSize(16);
    debugButtonText.setFillColor(sf::Color(200, 200, 220));
    debugButtonText.setPosition(sf::Vector2f(startX + 20.0f, static_cast<float>(windowHeight - bottom_panel_height - 50)));
    
    // Панель отладки (появляется слева)
    debugPanel.setSize(sf::Vector2f(300.0f, 400.0f));
    debugPanel.setPosition(sf::Vector2f(10.0f, 10.0f));
    debugPanel.setFillColor(sf::Color(20, 20, 25, 220));
    debugPanel.setOutlineColor(sf::Color(100, 100, 150));
    debugPanel.setOutlineThickness(1.0f);
    
    debugInfoText.setFont(font);
    debugInfoText.setCharacterSize(12);
    debugInfoText.setFillColor(sf::Color(180, 255, 180)); // Салатовый для отладки
    debugInfoText.setPosition(sf::Vector2f(20.0f, 20.0f));
    
    // Нижняя панель
    bottomPanel.setSize(sf::Vector2f(static_cast<float>(windowWidth), bottom_panel_height));
    bottomPanel.setPosition(sf::Vector2f(0, static_cast<float>(windowHeight - bottom_panel_height)));
    bottomPanel.setFillColor(BOTTOM_PANEL_BG); // фон панели тёмный

    // Текст для нижней панели
    resourceText.setFont(font);
    resourceText.setCharacterSize(14);
    resourceText.setFillColor(TEXT_BOTTOM); // белый
    resourceText.setPosition(sf::Vector2f(10.0f, static_cast<float>(windowHeight - bottom_panel_height + 15)));

    // В конструкторе
    configText.setFont(font);
    configText.setCharacterSize(14);
    configText.setFillColor(TEXT_BOTTOM); // белый
    // Сдвигаем под resourceText, с небольшим левым отступом
    configText.setPosition(sf::Vector2f(200.0f, static_cast<float>(windowHeight - bottom_panel_height + 15)));

    // === CHAT PANEL ===
    chatPanel.setSize(sf::Vector2f(400.f, 250.f));
    chatPanel.setPosition(sf::Vector2f(10.f, windowHeight - bottom_panel_height - 260.f));
    chatPanel.setFillColor(sf::Color(20, 20, 25, 230));

    inputBox.setSize(sf::Vector2f(300.f, 35.f));
    inputBox.setPosition(sf::Vector2f(20.f, windowHeight - bottom_panel_height - 40.f));
    inputBox.setFillColor(sf::Color(40,40,45));

    sendButton.setSize(sf::Vector2f(80.f, 35.f));
    sendButton.setPosition(sf::Vector2f(330.f, windowHeight - bottom_panel_height - 40.f));
    sendButton.setFillColor(sf::Color(70,70,90));

    likeButton.setSize(sf::Vector2f(40.f, 30.f));
    likeButton.setPosition(sf::Vector2f(420.f, windowHeight - bottom_panel_height - 90.f));
    likeButton.setFillColor(sf::Color(60,90,60));

    dislikeButton = likeButton;
    dislikeButton.setPosition(sf::Vector2f(470.f, windowHeight - bottom_panel_height - 90.f));
    dislikeButton.setFillColor(sf::Color(120,60,60));

    inputText.setFont(font);
    inputText.setCharacterSize(16);
    inputText.setFillColor(TEXT_MAIN);
    inputText.setPosition(sf::Vector2f(25.f, windowHeight - bottom_panel_height - 35.f));

    chatHistoryText.setFont(font);
    chatHistoryText.setCharacterSize(14);
    chatHistoryText.setFillColor(TEXT_INFO);
    chatHistoryText.setPosition(sf::Vector2f(20.f, windowHeight - bottom_panel_height - 240.f));

    sendText.setFont(font);
    sendText.setString("Send");
    sendText.setCharacterSize(14);
    sendText.setPosition(sf::Vector2f(sendButton.getPosition().x + 15.f,
                        sendButton.getPosition().y + 8.f));

    likeText.setFont(font);
    likeText.setString("👍");
    likeText.setCharacterSize(16);
    likeText.setPosition(sf::Vector2f(likeButton.getPosition().x + 10.f,
                        likeButton.getPosition().y + 4.f));

    dislikeText.setFont(font);
    dislikeText.setString("👎");
    dislikeText.setCharacterSize(16);
    dislikeText.setPosition(sf::Vector2f(dislikeButton.getPosition().x + 10.f,
                            dislikeButton.getPosition().y + 4.f));

}

// modules/UIModule.cpp - исправьте метод handleEvents
void UIModule::handleEvents(sf::RenderWindow& window, NeuralFieldSystem& system, 
                           bool& simulation_running, StatisticsModule& stats) 
{
    // Этот метод должен обрабатывать события, которые не были обработаны в main
    // Но сейчас он пустой и использует pollEvent - это неправильно
    // Оставляем пустым или удаляем
}

// language modeule type text
void UIModule::handleTextEntered(const sf::Event::TextEntered& event) {
    if (event.unicode == 8) { // backspace
        if (!currentInput.empty())
            currentInput.pop_back();
    }
    else if (event.unicode == 13) { // enter
        sendMessage();
    }
    else if (event.unicode < 128) {
        currentInput += static_cast<char>(event.unicode);
    }

    inputText.setString(currentInput);
}

void UIModule::handleChatClick(sf::Vector2f mousePos) {

    if (sendButton.getGlobalBounds().contains(mousePos)) {
        sendMessage();
    }

    if (likeButton.getGlobalBounds().contains(mousePos) && languageModule) {
        languageModule->giveFeedback(1.0f);
    }

    if (dislikeButton.getGlobalBounds().contains(mousePos) && languageModule) {
        languageModule->giveFeedback(0.0f);
    }
}

void UIModule::sendMessage() {

    if (!languageModule || currentInput.empty())
        return;

    std::string response = languageModule->process(currentInput);

    chatHistory.push_back("You: " + currentInput);
    chatHistory.push_back("AI: " + response);

    if (chatHistory.size() > 20)
        chatHistory.erase(chatHistory.begin(), chatHistory.begin() + 2);

    std::stringstream ss;
    for (const auto& line : chatHistory)
        ss << line << "\n";

    chatHistoryText.setString(ss.str());

    currentInput.clear();
    inputText.setString("");
}

void UIModule::handleMouseClick(const sf::Event::MouseButtonPressed& event, NeuralFieldSystem& system, 
                               bool& simulation_running, StatisticsModule& stats) {
    sf::Vector2f mousePos{
        static_cast<float>(event.position.x),
        static_cast<float>(event.position.y)
    };
    handleChatClick(mousePos);
        // Проверяем клик по кнопке Debug
    if (debugButton.getGlobalBounds().contains(mousePos)) {
        toggleDebug();
        debugButtonText.setString(show_debug ? "🔍 Hide Debug" : "🔍 Show Debug");
        std::cout << "Debug panel " << (show_debug ? "shown" : "hidden") << std::endl;
        return;
    }

    // Проверяем, что клик не в нижней панели
    if (mousePos.y > windowHeight - bottom_panel_height) {
        return;
    }

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
                   const ResourceMonitor& resources, const EvolutionModule& evolution,
                   const MemoryController& memory, int step) {
    
    // Рисуем нижнюю панель
    window.draw(bottomPanel);
    drawBottomPanel(window, resources, evolution);
    
    //CHAT
    drawChat(window);

    // Рисуем кнопку Debug
    window.draw(debugButton);
    window.draw(debugButtonText);
    
    // Рисуем панель отладки если включена
    if (show_debug) {
        drawDebugPanel(window, evolution, memory, step);
    }
    
    if (!config.show_controls) return;

    sf::RectangleShape panel;
    panel.setSize(sf::Vector2f(
        static_cast<float>(config.control_panel_width),
        static_cast<float>(windowHeight - bottom_panel_height)
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
void UIModule::drawChat(sf::RenderWindow& window) {

    window.draw(chatPanel);

    window.draw(chatHistoryText);

    window.draw(inputBox);
    window.draw(inputText);

    window.draw(sendButton);
    window.draw(sendText);

    window.draw(likeButton);
    window.draw(dislikeButton);
    window.draw(likeText);
    window.draw(dislikeText);
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
    // не понял? sf::Text?
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
// ДЭБАГ!!!
void UIModule::drawDebugButton(sf::RenderWindow& window) {
    window.draw(debugButton);
    window.draw(debugButtonText);
}

void UIModule::drawDebugPanel(sf::RenderWindow& window, const EvolutionModule& evolution, 
                             const MemoryController& memory, int step) {
    window.draw(debugPanel);
    
    const auto& metrics = evolution.getCurrentMetrics();
    
    std::stringstream ss;
    ss << "  DEBUG INFO\n"
       << "═══════════════\n\n"
       << "  EVOLUTION:\n"
       << "  Fitness: " << std::fixed << std::setprecision(4) 
       << metrics.overall_fitness << "\n"
       << "  Best: " << evolution.getBestFitness() << "\n"
       << "  Code Score: " << metrics.code_size_score << "\n"
       << "  Perf Score: " << metrics.performance_score << "\n"
       << "  Energy Score: " << metrics.energy_score << "\n\n"
       << "  MEMORY:\n"
       << "  Records: " << memory.size() << "/5000\n"
       << "  Feature Dim: 64\n"
       << "  Decay: 0.995/step\n\n"
       << "  SYSTEM:\n"
       << "  Step: " << step << "\n"
       << "  Stasis: " << (evolution.isInStasis() ? "YES" : "NO") << "\n"
       << "  Cooldown: " << evolution.getReductionCooldown() << "s\n"
       << "  Max Reductions: " << evolution.getMaxReductionsPerMinute() << "/min\n"
       << "  Min Fitness: " << evolution.getMinFitnessForOptimization();
    
    debugInfoText.setString(ss.str());
    window.draw(debugInfoText);
}