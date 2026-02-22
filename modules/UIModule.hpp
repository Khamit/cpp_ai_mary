//cpp_ai_test/modules/UIModule.hpp
#pragma once
#include <SFML/Graphics.hpp>
#include "../core/NeuralFieldSystem.hpp"
#include "StatisticsModule.hpp"
#include "ConfigStructs.hpp"
#include "ResourceMonitor.hpp"
#include "EvolutionModule.hpp"
#include <string>
// Включить
#include "lang/LanguageModule.hpp"

// Forward declaration вместо include (чтобы избежать циклических зависимостей)
class MemoryController;  // Добавлено!

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
            const ResourceMonitor& resources, const EvolutionModule& evolution,
            const MemoryController& memory, int step);  // Добавлено
    
    int getVisualizationWidth() const { return windowWidth - config.control_panel_width; }
    int getVisualizationHeight() const { return windowHeight - bottom_panel_height; }
    // !!!
    // Добавить метод для переключения отладки
    void toggleDebug() { show_debug = !show_debug; }
    bool isDebugVisible() const { return show_debug; }

    // вызываю в мэйн по тому должно быть публичным
    void handleTextEntered(const sf::Event::TextEntered& event);
    
    // ИСПРАВЛЕНО: Закрыли метод setLanguageModule правильно
    void setLanguageModule(LanguageModule* module) {
        languageModule = module;
    } // <- Здесь была закрывающая скобка метода

    // ИСПРАВЛЕНО: Добавили новый метод
    std::string getCurrentInput() const { return currentInput; }

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
    //!!! Добавить новые поля
    bool show_debug = false;  // По умолчанию скрыто
    sf::RectangleShape debugButton;
    sf::Text debugButtonText;
    sf::Text debugInfoText;
    sf::RectangleShape debugPanel;
    
    // Новые методы
    void drawDebugPanel(sf::RenderWindow& window, const EvolutionModule& evolution, 
                       const MemoryController& memory, int step);
    void drawDebugButton(sf::RenderWindow& window);

    // === AI CHAT ===
    LanguageModule* languageModule = nullptr;

    sf::RectangleShape chatPanel;
    sf::RectangleShape inputBox;
    sf::RectangleShape sendButton;
    sf::RectangleShape likeButton;
    sf::RectangleShape dislikeButton;

    sf::Text inputText;
    sf::Text chatHistoryText;
    sf::Text sendText;
    sf::Text likeText;
    sf::Text dislikeText;

    std::string currentInput;
    std::vector<std::string> chatHistory;

    void drawChat(sf::RenderWindow& window);
    
    void handleChatClick(sf::Vector2f mousePos);
    void sendMessage();
};