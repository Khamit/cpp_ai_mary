//cpp_ai_test/modules/UIModule.hpp
#pragma once
#include <SFML/Graphics.hpp>
#include "../core/NeuralFieldSystem.hpp"
#include "StatisticsModule.hpp"
#include "ConfigStructs.hpp"
#include "ResourceMonitor.hpp"
#include <string>
#include "lang/LanguageModule.hpp"
#include "UnifiedStatsCollector.hpp"
#include "VisualizationModule.hpp"
#include "../modules/learning/WebTrainerModule.hpp"

// Forward declaration
class EmergentCore; 

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
        const ResourceMonitor& resources,
        const EmergentMemory& memory, int step); 
    
    int getVisualizationWidth() const { return windowWidth - config.control_panel_width; }
    void setWebTrainer(WebTrainerModule* trainer) { web_trainer_ = trainer; }
    
    int getVisualizationHeight() const {
        return windowHeight 
            - static_cast<int>(chatAreaHeight)
            - bottom_panel_height;
    }

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

    void setStatsCollector(UnifiedStatsCollector* collector) { stats_collector = collector; }
    void cycleDisplayMode() { current_display_mode = (current_display_mode + 1) % 6; }
    void drawUnifiedStats(sf::RenderWindow& window);

    // VisualizationModule
    void setVisualizer(VisualizationModule* vis) { visualizer = vis; }
    void toggleOrbits();
    void toggleInterConnections();
    void toggleIntraConnections();
    void toggleNeurons();
    // 3D управление:
    void toggleAutoRotate();
    void resetView();
    void handleRotate(float delta);
    void handleTilt(float delta);
    // chat
    void toggleChat();
    void setSimulationRunning(bool running) { simulation_running_ = running; }

    // Режим работ
OperatingMode::Type getCurrentOperatingMode() const {
    // 1. Если автообучение активно - режим TRAINING
    if (autoLearningActive) {
        return OperatingMode::TRAINING;
    }
    
    // 2. Если есть активность пользователя - NORMAL
    static auto lastActivityTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    
    if (!currentInput.empty() || 
        sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
        lastActivityTime = now;
        return OperatingMode::NORMAL;
    }
    
    // 3. Если прошло больше 5 минут без активности - IDLE
    if (std::chrono::duration_cast<std::chrono::minutes>(
            now - lastActivityTime).count() > 5) {
        return OperatingMode::IDLE;
    }
    
    // 4. Если прошло больше 30 минут - SLEEP
    if (std::chrono::duration_cast<std::chrono::minutes>(
            now - lastActivityTime).count() > 30) {
        return OperatingMode::SLEEP;
    }
    
    return OperatingMode::NORMAL;
}

   void toggleDiffusion();      // переключение отображения диффузии
    void toggleInhibitor();      // переключение отображения ингибитора

private:
    // Добавить поле
    NeuralFieldSystem* neural_system = nullptr;
    VisualizationModule* visualizer = nullptr;  // указатель на визуализатор

    // Вспомогательные методы для рисования
    void drawMeter(sf::RenderWindow& window, const std::string& label, float value, float x, float y);
    void drawBar(sf::RenderWindow& window, const std::string& label, float value, float x, float y);
    // =================================================
    WebTrainerModule* web_trainer_ = nullptr;

    UIConfig config;
    int windowWidth, windowHeight;
    int bottom_panel_height = 100; // Новая нижняя панель
    
    sf::Font font;
    sf::RectangleShape startButton, stopButton, resetButton;
    sf::Text startText, stopText, resetText, statsText, stasisText;
    sf::Text resourceText, configText; // Новые текстовые элементы
    sf::RectangleShape bottomPanel; // Новая нижняя панель
    
    void drawControlPanel(sf::RenderWindow& window, const StatisticsModule& stats, bool simulation_running);
    void drawStatistics(sf::RenderWindow& window, const StatisticsModule& stats);
    void drawBottomPanel(sf::RenderWindow& window, const ResourceMonitor& resources);
    std::string formatDouble(double value, int precision = 4) const;

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

    UnifiedStatsCollector* stats_collector = nullptr;
    int current_display_mode = 0;
    sf::Text modeText;

    // кнопка чата
    bool chat_visible = true;  // флаг видимости чата
    sf::RectangleShape toggleChatButton;  // кнопка для переключения
    sf::Text toggleChatText;  // текст на кнопке
    bool simulation_running_ = false;
};