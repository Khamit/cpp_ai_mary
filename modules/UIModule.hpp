//cpp_ai_test/modules/UIModule.hpp
#pragma once
#include <SFML/Graphics.hpp>
#include "../core/NeuralFieldSystem.hpp"
#include "StatisticsModule.hpp"
#include "ConfigStructs.hpp"
#include "ResourceMonitor.hpp"
#include "EvolutionModule.hpp"
#include <string>
#include "lang/LanguageModule.hpp"

// Forward declaration
class MemoryManager; 

   struct ChatMessage {
    std::string text;
    bool isUser;
    };

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
        const MemoryManager& memory, int step); 
    
    int getVisualizationWidth() const { return windowWidth - config.control_panel_width; }
    
    int getVisualizationHeight() const {
        return windowHeight 
            - static_cast<int>(chatAreaHeight)
            - bottom_panel_height;
    }
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

    bool isAutoLearningActive() const { return autoLearningActive; }

    // ИСПРАВЛЕНО: Добавили новый метод
    std::string getCurrentInput() const { return currentInput; }

    // Добавить метод для установки ссылки на нейросистему
    void setNeuralSystem(NeuralFieldSystem* system) {
        neural_system = system;
    }
    void drawReflectionPanel(sf::RenderWindow& window);

      // Скроллинг
    void handleMouseWheel(const sf::Event::MouseWheelScrolled& event);

private:
    // Добавить поле
    NeuralFieldSystem* neural_system = nullptr;

    // Вспомогательные методы для рисования
    void drawMeter(sf::RenderWindow& window, const std::string& label, float value, float x, float y);
    void drawBar(sf::RenderWindow& window, const std::string& label, float value, float x, float y);
    // =================================================

    UIConfig config;
    int windowWidth, windowHeight;
    int bottom_panel_height = 100; // Новая нижняя панель
    
    sf::Font font;
    sf::RectangleShape startButton, stopButton, resetButton;
    sf::Text startText, stopText, resetText, statsText, evolutionText, stasisText;
    sf::Text resourceText, configText; // Новые текстовые элементы
    sf::RectangleShape bottomPanel; // Новая нижняя панель
    
    void drawControlPanel(sf::RenderWindow& window, const StatisticsModule& stats, bool simulation_running);
    void drawStatistics(sf::RenderWindow& window, const StatisticsModule& stats, const EvolutionModule& evolution);
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
                   const MemoryManager& memory, int step);

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
    std::vector<ChatMessage> chatHistory;
    // высота чата
    float chatAreaHeight = 300.f;  // теперь управляемо

    // Сролл
    float chatScrollOffset = 0.f;
    float chatContentHeight = 0.f;
    bool stickToBottom = true;

    void drawChat(sf::RenderWindow& window);
    void handleChatClick(sf::Vector2f mousePos);
    void sendMessage();

    // chat btn effect
    bool likePressed = false;
    bool dislikePressed = false;
    sf::Clock feedbackClock; // для таймера сброса подсветки

    // Кнопки для автообучения
    sf::RectangleShape autoLearnButton;
    sf::RectangleShape stopLearnButton;
    sf::Text autoLearnText;
    sf::Text stopLearnText;
    bool autoLearningActive = false;  // флаг активности автообучения
};