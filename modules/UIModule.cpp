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
    // Загрузка шрифта
    if (!font.openFromFile("SF-Pro-Display-Regular.otf")) {
        std::cerr << "Failed to load font 'SF-Pro-Display-Regular.otf'!" << std::endl;
        if (!font.openFromFile("/System/Library/Fonts/Helvetica.ttc")) {
            std::cerr << "Failed to load fallback font too!" << std::endl;
        }
    }
    
    bottom_panel_height = 120.0f;
    float chatAreaHeight = 300.0f;
    float buttonWidth = 180.0f;
    float buttonHeight = 42.0f;
    float startX = static_cast<float>(windowWidth - config.control_panel_width + 10);
    stickToBottom = true;
    float chatWidth = windowWidth * 0.5f; // 70% ширины окна
    // Размеры кнопок под Send
    float feedbackButtonHeight = sendButton.getSize().y; // высота как Send
    float feedbackButtonWidth = chatPanel.getSize().x / 2.0f; // половина ширины чата
        // ===== ПОДНИМАЕМ ЧАТ ВЫШЕ =====
    // Уменьшаем chatY, чтобы чат был выше
    // Было: windowHeight - bottom_panel_height - chatAreaHeight
    // Теперь: windowHeight - bottom_panel_height - chatAreaHeight - 100.f (поднимаем на 100 пикселей)
    float chatY = windowHeight - bottom_panel_height - chatAreaHeight - 100.f;

    // ===== НОВЫЕ ПОЗИЦИИ =====
    
    // Визуализация теперь будет занимать верхнюю часть,
    // но она рисуется отдельно, не в UI
    
    // Панель отладки - под визуализацией слева
    debugPanel.setSize(sf::Vector2f(200.0f, 250.0f));
    debugPanel.setPosition(sf::Vector2f(10.0f, 500.0f));  // y = 500 (под визуализацией)
    debugPanel.setFillColor(sf::Color(20, 20, 25, 220));
    debugPanel.setOutlineColor(sf::Color(100, 100, 150));
    debugPanel.setOutlineThickness(1.0f);
    
    debugInfoText.setFont(font);
    debugInfoText.setCharacterSize(12);
    debugInfoText.setFillColor(sf::Color(180, 255, 180));
    debugInfoText.setPosition(sf::Vector2f(20.0f, 510.0f));  // внутри панели

    // Чат - теперь выше
    chatPanel.setSize(sf::Vector2f(chatWidth, chatAreaHeight - 50.f));
    chatPanel.setPosition(sf::Vector2f(10.f, chatY));
    chatPanel.setFillColor(sf::Color(20, 20, 25, 230));
    
    // Input box под чатом
    inputBox.setSize(sf::Vector2f(chatPanel.getSize().x - 20.f, 35.f));
    inputBox.setPosition(sf::Vector2f(10.f, chatY + chatAreaHeight - 45.f));
    inputBox.setFillColor(sf::Color(40,40,45));
    
    // Кнопка Send
    sendButton.setSize(sf::Vector2f(80.f, 35.f));
    sendButton.setPosition(sf::Vector2f(inputBox.getPosition().x + inputBox.getSize().x + 10.f,
                                    chatY + chatAreaHeight - 45.f));
    sendButton.setFillColor(sf::Color(70,70,90));
    
    // Кнопки лайк/дизлайк (размеры, но позиция будет в drawChat)
    likeButton.setSize(sf::Vector2f(chatWidth / 2.0f - 2.f, 35.f));
    likeButton.setFillColor(sf::Color(60, 90, 60));

    dislikeButton.setSize(sf::Vector2f(chatWidth / 2.0f - 2.f, 35.f));
    dislikeButton.setFillColor(sf::Color(120, 60, 60));
    
    // Текстовые поля чата
    inputText.setFont(font);
    inputText.setCharacterSize(16);
    inputText.setFillColor(TEXT_MAIN);
    inputText.setPosition(sf::Vector2f(25.f, chatY + chatAreaHeight - 40.f));
    
    chatHistoryText.setFont(font); // Мы больше не используем chatHistoryText для отрисовки, но оставим на всякий случай
    chatHistoryText.setCharacterSize(14);
    chatHistoryText.setFillColor(TEXT_INFO);
    chatHistoryText.setPosition(sf::Vector2f(20.f, chatY + 10.f));
    
    sendText.setFont(font);
    sendText.setString("Send");
    sendText.setCharacterSize(14);
    sendText.setPosition(sf::Vector2f(sendButton.getPosition().x + 15.f,
                        sendButton.getPosition().y + 8.f));
    
    likeText.setFont(font);
    likeText.setString("Good");
    likeText.setCharacterSize(12);
    likeText.setFillColor(TEXT_MAIN);
    
    dislikeText.setFont(font);
    dislikeText.setString("Bad");
    dislikeText.setCharacterSize(12);
    dislikeText.setFillColor(TEXT_MAIN);
    
    // ===== ПРАВАЯ ПАНЕЛЬ УПРАВЛЕНИЯ =====
    
    // Кнопки управления
    startButton.setSize(sf::Vector2f(buttonWidth, buttonHeight));
    startButton.setPosition(sf::Vector2f(startX, 50.0f));
    startButton.setFillColor(BTN_BG);
    startButton.setOutlineColor(sf::Color(80, 80, 85));
    startButton.setOutlineThickness(1.0f);
    
    stopButton = startButton;
    stopButton.setPosition(sf::Vector2f(startX, 100.0f));
    
    resetButton = startButton;
    resetButton.setPosition(sf::Vector2f(startX, 150.0f));
    
    // Кнопка Debug
    debugButton.setSize(sf::Vector2f(buttonWidth, buttonHeight));
    debugButton.setPosition(sf::Vector2f(startX, windowHeight - bottom_panel_height - 60));
    debugButton.setFillColor(sf::Color(70, 70, 80));
    debugButton.setOutlineColor(sf::Color(120, 120, 130));
    debugButton.setOutlineThickness(1.0f);
    
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
    
    debugButtonText.setFont(font);
    debugButtonText.setString("Show Debug");
    debugButtonText.setCharacterSize(16);
    debugButtonText.setFillColor(sf::Color(200, 200, 220));
    debugButtonText.setPosition(sf::Vector2f(startX + 20.0f, windowHeight - bottom_panel_height - 50));
    
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
    bottomPanel.setFillColor(BOTTOM_PANEL_BG);
    
    // Текст для нижней панели
    resourceText.setFont(font);
    resourceText.setCharacterSize(14);
    resourceText.setFillColor(TEXT_BOTTOM);
    resourceText.setPosition(sf::Vector2f(10.0f, static_cast<float>(windowHeight - bottom_panel_height + 15)));
    
    configText.setFont(font);
    configText.setCharacterSize(14);
    configText.setFillColor(TEXT_BOTTOM);
    configText.setPosition(sf::Vector2f(200.0f, static_cast<float>(windowHeight - bottom_panel_height + 15)));
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
    // Проверяем клик по кнопке Send
    if (sendButton.getGlobalBounds().contains(mousePos)) {
        sendMessage();
        return;
    }
    
    // Проверяем клик по кнопкам лайк/дизлайк
    if (!chatHistory.empty() && !chatHistory.back().isUser) {
        float buttonY = chatPanel.getPosition().y + chatPanel.getSize().y - 40.f;
        
        // Вычисляем актуальные позиции кнопок
        sf::FloatRect likeRect(
            sf::Vector2f(chatPanel.getPosition().x + 20.f, buttonY),
            likeButton.getSize()
        );
        
        sf::FloatRect dislikeRect(
            sf::Vector2f(chatPanel.getPosition().x + 80.f, buttonY),
            dislikeButton.getSize()
        );
        
        if (likeRect.contains(mousePos) && languageModule) {
            languageModule->giveFeedback(1.0f);
            std::cout << "Good button clicked" << std::endl;
            return;
        }
        
        if (dislikeRect.contains(mousePos) && languageModule) {
            languageModule->giveFeedback(0.0f);
            std::cout << "Bad button clicked" << std::endl;
            return;
        }
    }
}

void UIModule::sendMessage() {
    if (!languageModule || currentInput.empty())
        return;
    
    std::cout << "Sending message: " << currentInput << std::endl;
    std::string response = languageModule->process(currentInput);
    
    chatHistory.push_back({currentInput, true});
    chatHistory.push_back({response, false});
    
    // Ограничиваем историю
    if (chatHistory.size() > 20) {
        chatHistory.erase(chatHistory.begin(), chatHistory.begin() + 2);
    }
    
    // Обновляем текст для отображения (опционально, если используете chatHistoryText)
    std::stringstream ss;
    for (const auto& msg : chatHistory) {
        ss << (msg.isUser ? "You: " : "AI: ") << msg.text << "\n";
    }
    chatHistoryText.setString(ss.str());
    
    currentInput.clear();
    inputText.setString("");
    stickToBottom = true; // Прокручиваем вниз при новом сообщении
    
    std::cout << "Chat history size: " << chatHistory.size() << std::endl;
}

void UIModule::handleMouseClick(const sf::Event::MouseButtonPressed& event, NeuralFieldSystem& system, 
                               bool& simulation_running, StatisticsModule& stats) {
    sf::Vector2f mousePos{
        static_cast<float>(event.position.x),
        static_cast<float>(event.position.y)
    };
    
    // Проверяем кнопку Send
    if (sendButton.getGlobalBounds().contains(mousePos)) {
        sendMessage();
        std::cout << "Send button clicked" << std::endl;
        return;
    }
    
    // Проверяем кнопки лайк/дизлайк (с учетом их актуальной позиции под чатом)
    if (!chatHistory.empty() && !chatHistory.back().isUser) {
        float feedbackButtonHeight = 40.f;
        float feedbackButtonWidth = chatPanel.getSize().x / 2.0f - 2.f;
        float feedbackY = chatPanel.getPosition().y + chatPanel.getSize().y + 5.f;

        sf::FloatRect likeRect(
            sf::Vector2f(chatPanel.getPosition().x, feedbackY),
            sf::Vector2f(feedbackButtonWidth, feedbackButtonHeight)
        );

        sf::FloatRect dislikeRect(
            sf::Vector2f(chatPanel.getPosition().x + feedbackButtonWidth + 4.f, feedbackY),
            sf::Vector2f(feedbackButtonWidth, feedbackButtonHeight)
        );

        if (likeRect.contains(mousePos) && languageModule) {
            languageModule->giveFeedback(1.0f);
            likePressed = true;
            dislikePressed = false;
            feedbackClock.restart();
            std::cout << "Good button clicked" << std::endl;
            return;
        }

        if (dislikeRect.contains(mousePos) && languageModule) {
            languageModule->giveFeedback(0.0f);
            dislikePressed = true;
            likePressed = false;
            feedbackClock.restart();
            std::cout << "Bad button clicked" << std::endl;
            return;
        }
    }
    
    // Проверяем input box
    if (inputBox.getGlobalBounds().contains(mousePos)) {
        std::cout << "Input box clicked" << std::endl;
        return;
    }
    
    // Остальные проверки...
    // Проверяем кнопку Debug
    if (debugButton.getGlobalBounds().contains(mousePos)) {
        toggleDebug();
        debugButtonText.setString(show_debug ? "🔍 Hide Debug" : "🔍 Show Debug");
        std::cout << "Debug panel " << (show_debug ? "shown" : "hidden") << std::endl;
        return;
    }

    // Проверяем нижнюю панель
    if (mousePos.y > windowHeight - bottom_panel_height) {
        return;
    }

    // Кнопки управления
    if (startButton.getGlobalBounds().contains(mousePos)) {
        simulation_running = true;
        stats.startTimer();
        std::cout << "Simulation STARTED" << std::endl;
        return;
    }
    else if (stopButton.getGlobalBounds().contains(mousePos)) {
        simulation_running = false;
        stats.stopTimer();
        std::cout << "Simulation STOPPED" << std::endl;
        return;
    }
    else if (resetButton.getGlobalBounds().contains(mousePos)) {
        simulation_running = false;
        stats.stopTimer();
        stats.reset();
        
        std::mt19937 rng(42);
        system.initializeRandom(rng);
        std::cout << "System RESET" << std::endl;
        return;
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
        drawReflectionPanel(window); // ref-panel
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

void UIModule::drawChat(sf::RenderWindow& window)
{
    // --- 1. Рисуем фон панели чата ---
    window.draw(chatPanel);

    // Получаем размер окна для расчета Viewport
    auto winSize = window.getSize();
    float windowWidthF = static_cast<float>(winSize.x);
    float windowHeightF = static_cast<float>(winSize.y);

    // --- 2. НАСТРАИВАЕМ VIEW для области чата ---
    sf::View chatView;
    chatView.setSize(chatPanel.getSize());
    chatView.setCenter(chatPanel.getSize() / 2.f);

    chatView.setViewport(sf::FloatRect(
        sf::Vector2f(chatPanel.getPosition().x / windowWidthF, chatPanel.getPosition().y / windowHeightF),
        sf::Vector2f(chatPanel.getSize().x / windowWidthF, chatPanel.getSize().y / windowHeightF)
    ));

    window.setView(chatView);

    // --- 3. Расчет высоты контента и скролла ---
    float contentHeight = 10.f;
    for (const auto& msg : chatHistory) {
        sf::Text tempText(font);
        tempText.setString((msg.isUser ? "You: " : "AI: ") + msg.text);
        tempText.setCharacterSize(14);
        sf::FloatRect bounds = tempText.getLocalBounds();
        contentHeight += bounds.size.y + 20.f + 10.f;
    }
    chatContentHeight = contentHeight;

    if (stickToBottom) {
        chatScrollOffset = std::max(0.f, chatContentHeight - chatPanel.getSize().y);
    }

    // --- 4. Отрисовка сообщений ---
    float yOffset = 10.f - chatScrollOffset;

    for (const auto& msg : chatHistory) {
        sf::Text text(font);
        text.setString((msg.isUser ? "You: " : "AI: ") + msg.text);
        text.setCharacterSize(14);
        text.setFillColor(msg.isUser ? sf::Color(230,230,235) : sf::Color(180,255,180));

        sf::FloatRect bounds = text.getLocalBounds();

        sf::RectangleShape bubble(sf::Vector2f(bounds.size.x + 20.f, bounds.size.y + 20.f));

        float bubbleX = msg.isUser ?
            chatPanel.getSize().x - bubble.getSize().x - 10.f :
            10.f;

        bubble.setPosition(sf::Vector2f(bubbleX, yOffset));
        bubble.setFillColor(msg.isUser ? sf::Color(70,70,90) : sf::Color(50,80,60));

        text.setPosition(sf::Vector2f(
            bubble.getPosition().x + 10.f,
            bubble.getPosition().y + 10.f
        ));

        window.draw(bubble);
        window.draw(text);

        yOffset += bubble.getSize().y + 10.f;
    }

    // --- 5. Возвращаем стандартный view ---
    window.setView(window.getDefaultView());

    // --- 6. Рисуем поле ввода и кнопку Send ---
    window.draw(inputBox);
    window.draw(inputText);
    window.draw(sendButton);
    window.draw(sendText);

    // --- 7. Рисуем кнопки фидбэка ПОД полем ввода ---
    if (!chatHistory.empty() && !chatHistory.back().isUser) {
        float feedbackButtonHeight = 35.f; // Такая же высота как у sendButton
        float feedbackButtonWidth = chatPanel.getSize().x / 2.0f - 2.f;

        // Новая Y-позиция: сразу под sendButton
        float feedbackY = sendButton.getPosition().y + sendButton.getSize().y + 5.f;

        // Проверка на выход за нижнюю панель
        float bottomPanelTop = static_cast<float>(windowHeight) - bottom_panel_height;
        if (feedbackY + feedbackButtonHeight > bottomPanelTop - 5.f) {
            feedbackY = bottomPanelTop - feedbackButtonHeight - 5.f;
        }

        // Устанавливаем позиции кнопок
        likeButton.setPosition(sf::Vector2f(chatPanel.getPosition().x, feedbackY));
        likeButton.setSize(sf::Vector2f(feedbackButtonWidth, feedbackButtonHeight));

        dislikeButton.setPosition(sf::Vector2f(chatPanel.getPosition().x + feedbackButtonWidth + 4.f, feedbackY));
        dislikeButton.setSize(sf::Vector2f(feedbackButtonWidth, feedbackButtonHeight));

        // Центрируем текст
        sf::FloatRect likeBounds = likeText.getLocalBounds();
        sf::FloatRect dislikeBounds = dislikeText.getLocalBounds();

        likeText.setPosition(sf::Vector2f(
            likeButton.getPosition().x + (likeButton.getSize().x - likeBounds.size.x) / 2.f,
            feedbackY + (feedbackButtonHeight - likeText.getCharacterSize()) / 2.f
        ));

        dislikeText.setPosition(sf::Vector2f(
            dislikeButton.getPosition().x + (dislikeButton.getSize().x - dislikeBounds.size.x) / 2.f,
            feedbackY + (feedbackButtonHeight - dislikeText.getCharacterSize()) / 2.f
        ));

        // Эффект нажатия
        if (likePressed) {
            likeButton.setFillColor(sf::Color(100, 200, 100));
        } else {
            likeButton.setFillColor(sf::Color(60, 90, 60));
        }

        if (dislikePressed) {
            dislikeButton.setFillColor(sf::Color(200, 100, 100));
        } else {
            dislikeButton.setFillColor(sf::Color(120, 60, 60));
        }

        if (feedbackClock.getElapsedTime().asSeconds() > 0.5f) {
            likePressed = false;
            dislikePressed = false;
        }

        window.draw(likeButton);
        window.draw(dislikeButton);
        window.draw(likeText);
        window.draw(dislikeText);
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

// Reflection draw
// ИСПРАВЛЕНО: добавляем параметр sf::RenderWindow& window
void UIModule::drawMeter(sf::RenderWindow& window, const std::string& label, float value, float x, float y) {
    // Рисуем текст метки
    sf::Text labelText(font);
    labelText.setString(label);
    labelText.setCharacterSize(12);
    labelText.setFillColor(TEXT_MAIN);
    labelText.setPosition(sf::Vector2f(x, y));
    
    // Рисуем фон метра (серый прямоугольник)
    sf::RectangleShape meterBg(sf::Vector2f(100.0f, 15.0f));
    meterBg.setPosition(sf::Vector2f(x + 80.0f, y));
    meterBg.setFillColor(sf::Color(50, 50, 50));
    
    // Рисуем значение метра (цветной прямоугольник)
    sf::RectangleShape meterValue(sf::Vector2f(100.0f * value, 15.0f));
    meterValue.setPosition(sf::Vector2f(x + 80.0f, y));
    
    // Цвет в зависимости от значения
    if (value > 0.7f) {
        meterValue.setFillColor(sf::Color(100, 255, 100)); // зеленый
    } else if (value > 0.4f) {
        meterValue.setFillColor(sf::Color(255, 255, 100)); // желтый
    } else {
        meterValue.setFillColor(sf::Color(255, 100, 100)); // красный
    }
    
    // Текст значения
    sf::Text valueText(font);
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << value;
    valueText.setString(ss.str());
    valueText.setCharacterSize(12);
    valueText.setFillColor(TEXT_MAIN);
    valueText.setPosition(sf::Vector2f(x + 190.0f, y));
    
    // Рисуем все элементы
    window.draw(labelText);
    window.draw(meterBg);
    window.draw(meterValue);
    window.draw(valueText);
}

// ИСПРАВЛЕНО: добавляем параметр sf::RenderWindow& window
void UIModule::drawBar(sf::RenderWindow& window, const std::string& label, float value, float x, float y) {
    // Рисуем текст метки
    sf::Text labelText(font);
    labelText.setString(label);
    labelText.setCharacterSize(12);
    labelText.setFillColor(TEXT_MAIN);
    labelText.setPosition(sf::Vector2f(x, y));
    
    // Рисуем фон бара (серый прямоугольник)
    sf::RectangleShape barBg(sf::Vector2f(150.0f, 20.0f));
    barBg.setPosition(sf::Vector2f(x + 100.0f, y - 5.0f));
    barBg.setFillColor(sf::Color(50, 50, 50));
    
    // Рисуем значение бара (цветной прямоугольник)
    sf::RectangleShape barValue(sf::Vector2f(150.0f * value, 20.0f));
    barValue.setPosition(sf::Vector2f(x + 100.0f, y - 5.0f));
    barValue.setFillColor(sf::Color(100, 180, 255)); // голубой
    
    // Текст значения
    sf::Text valueText(font);
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << value;
    valueText.setString(ss.str());
    valueText.setCharacterSize(12);
    valueText.setFillColor(TEXT_MAIN);
    valueText.setPosition(sf::Vector2f(x + 260.0f, y));
    
    window.draw(labelText);
    window.draw(barBg);
    window.draw(barValue);
    window.draw(valueText);
}

void UIModule::handleMouseWheel(const sf::Event::MouseWheelScrolled& event)
    {
        sf::Vector2f mousePos(
            static_cast<float>(event.position.x),
            static_cast<float>(event.position.y)
        );

        if (!chatPanel.getGlobalBounds().contains(mousePos))
            return;

        stickToBottom = false;

        chatScrollOffset -= event.delta * 30.f;

        if (chatScrollOffset < 0.f)
            chatScrollOffset = 0.f;

        float maxScroll = std::max(0.f, chatContentHeight - chatPanel.getSize().y);

        if (chatScrollOffset > maxScroll)
            chatScrollOffset = maxScroll;
    }
// Исправленный метод drawReflectionPanel
void UIModule::drawReflectionPanel(sf::RenderWindow& window) {
    if (!neural_system) return; // защита от нулевого указателя
    
    try {
        auto state = neural_system->getReflectionState();
        
        // Рисуем панель
        sf::RectangleShape panel(sf::Vector2f(350.0f, 400.0f));
        panel.setPosition(sf::Vector2f(400.0f, 50.0f)); // справа от debug панели
        panel.setFillColor(sf::Color(30, 30, 40, 200));
        panel.setOutlineColor(sf::Color(100, 100, 150));
        panel.setOutlineThickness(1.0f);
        window.draw(panel);
        
        // Заголовок
        sf::Text title(font);
        title.setString("SELF REFLECTION");
        title.setCharacterSize(16);
        title.setFillColor(sf::Color(180, 220, 255));
        title.setPosition(sf::Vector2f(410.0f, 60.0f));
        window.draw(title);
        
        // Рисуем метрики рефлексии - ПЕРЕДАЕМ window
        drawMeter(window, "Confidence", state.confidence, 410, 100);
        drawMeter(window, "Curiosity", state.curiosity, 410, 130);
        drawMeter(window, "Satisfaction", state.satisfaction, 410, 160);
        drawMeter(window, "Confusion", state.confusion, 410, 190);
        
        // Рисуем карту внимания
        sf::Text attentionTitle(font);
        attentionTitle.setString("Attention Map:");
        attentionTitle.setCharacterSize(14);
        attentionTitle.setFillColor(TEXT_INFO);
        attentionTitle.setPosition(sf::Vector2f(410.0f, 230.0f));
        window.draw(attentionTitle);
        
        for (int i = 0; i < 4 && i < state.attention_map.size(); i++) {
            float y = 260 + i * 25;
            std::string groupName;
            switch(i) {
                case 0: groupName = "Language"; break;
                case 1: groupName = "Code Gen"; break;
                case 2: groupName = "Executor"; break;
                case 3: groupName = "Evolution"; break;
                default: groupName = "Group " + std::to_string(i);
            }
            // ПЕРЕДАЕМ window
            drawBar(window, groupName, state.attention_map[i], 410, y);
        }
        
        // Статус системы
        sf::Text statusText(font);
        statusText.setCharacterSize(12);
        statusText.setFillColor(TEXT_INFO);
        statusText.setPosition(sf::Vector2f(410.0f, 370.0f));
        
        if (state.confidence > 0.7f) {
            statusText.setString("Status: 🟢 Confident");
        } else if (state.confusion > 0.6f) {
            statusText.setString("Status: 🟡 Confused");
        } else {
            statusText.setString("Status: 🟠 Learning");
        }
        window.draw(statusText);
        
    } catch (const std::exception& e) {
        std::cerr << "Error in drawReflectionPanel: " << e.what() << std::endl;
    }
}
void UIModule::drawStatistics(sf::RenderWindow& window, const StatisticsModule& stats) {
    const auto& current = stats.getCurrentStats();
    
    std::stringstream ss;
    ss << "Statistics:\n\n"
       << "Step: " << current.step << "\n"
       << "Time: " << formatDouble(current.dt) << "s\n"
       << "Energy: " << formatDouble(current.total_energy) << "\n"
       << "Avg Phi: " << formatDouble(current.avg_phi) << "\n"
       << "State/AVG_Pi: " << current.avg_pi << "\n"
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