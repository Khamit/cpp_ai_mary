//cpp_ai_test/modules/UIModule.cpp
#include "UIModule.hpp"
#include <sstream>
#include <iomanip>
#include <iostream>
#include "core/MemoryManager.hpp" 

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
      inputText(font),
      chatHistoryText(font),
      sendText(font),
      likeText(font),
      dislikeText(font),
      autoLearnText(font),
      stopLearnText(font),
      modeText(font),
      stats_collector(nullptr),  // инициализируем nullptr
      current_display_mode(0),
      toggleChatText(font)
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

    // Кнопки автообучения
    autoLearnButton.setSize(sf::Vector2f(buttonWidth, buttonHeight));
    autoLearnButton.setPosition(sf::Vector2f(startX, 200.0f));  // под reset
    autoLearnButton.setFillColor(sf::Color(70, 120, 70));  // зеленоватый
    autoLearnButton.setOutlineColor(sf::Color(100, 200, 100));
    autoLearnButton.setOutlineThickness(1.0f);

    stopLearnButton = autoLearnButton;
    stopLearnButton.setPosition(sf::Vector2f(startX, 250.0f));
    stopLearnButton.setFillColor(sf::Color(120, 70, 70));  // красноватый
    stopLearnButton.setOutlineColor(sf::Color(200, 100, 100));

    autoLearnText.setFont(font);
    autoLearnText.setString("Start Auto-Learning");
    autoLearnText.setCharacterSize(16);
    autoLearnText.setFillColor(TEXT_MAIN);
    autoLearnText.setPosition(sf::Vector2f(startX + 20.0f, 210.0f));

    stopLearnText.setFont(font);
    stopLearnText.setString("Stop Auto-Learning");
    stopLearnText.setCharacterSize(16);
    stopLearnText.setFillColor(TEXT_MAIN);
    stopLearnText.setPosition(sf::Vector2f(startX + 20.0f, 260.0f));

    modeText.setFont(font);
    modeText.setCharacterSize(12);
    modeText.setFillColor(TEXT_INFO);
    modeText.setPosition(sf::Vector2f(startX, 300.0f)); // под статистикой

    // Кнопка переключения чата
    float toggleButtonX = 10.0f;  // слева, над чатом
    float toggleButtonY = chatY - 30.0f;  // прямо над чатом

    toggleChatButton.setSize(sf::Vector2f(100.0f, 25.0f));
    toggleChatButton.setPosition(sf::Vector2f(toggleButtonX, toggleButtonY));
    toggleChatButton.setFillColor(sf::Color(70, 70, 90));
    toggleChatButton.setOutlineColor(sf::Color(100, 100, 150));
    toggleChatButton.setOutlineThickness(1.0f);

    toggleChatText.setFont(font);
    toggleChatText.setString("▼ Hide Chat");  // изначально чат виден
    toggleChatText.setCharacterSize(12);
    toggleChatText.setFillColor(TEXT_MAIN);

    // Центрируем текст на кнопке
    sf::FloatRect textBounds = toggleChatText.getLocalBounds();
    toggleChatText.setPosition(sf::Vector2f(
        toggleButtonX + (100.0f - textBounds.size.x) / 2.0f,
        toggleButtonY + (25.0f - textBounds.size.y) / 2.0f - 2.0f
        )
    );
}

void UIModule::toggleChat() {
    chat_visible = !chat_visible;
    
    if (chat_visible) {
        toggleChatText.setString("▼ Hide Chat");
        std::cout << "Chat shown" << std::endl;
    } else {
        toggleChatText.setString("▶ Show Chat");
        std::cout << "Chat hidden" << std::endl;
    }
    
    // Центрируем текст заново после изменения
    sf::FloatRect textBounds = toggleChatText.getLocalBounds();
    sf::Vector2f btnPos = toggleChatButton.getPosition();
    toggleChatText.setPosition(sf::Vector2f(
        btnPos.x + (100.0f - textBounds.size.x) / 2.0f,
        btnPos.y + (25.0f - textBounds.size.y) / 2.0f - 2.0f)
    );
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
        // НЕ добавляем \n в currentInput
    }
    else if (event.unicode < 128 && event.unicode != 13) { // игнорируем enter
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

    // Очищаем ввод от служебных символов
    std::string cleanInput = currentInput;
    cleanInput.erase(std::remove(cleanInput.begin(), cleanInput.end(), '\n'), cleanInput.end());
    cleanInput.erase(std::remove(cleanInput.begin(), cleanInput.end(), '\r'), cleanInput.end());
    
    if (cleanInput.empty()) {
        currentInput.clear();
        inputText.setString("");
        return;
    }
    
    // ДОБАВИТЬ: проверка, запущена ли симуляция
    if (!simulation_running_) {
        chatHistory.push_back({currentInput, true});
        chatHistory.push_back({"Please start simulation first (click Start Simulation)", false});
        currentInput.clear();
        inputText.setString("");
        return;
    }
    
    // Проверяем, не занята ли нейросеть
    static std::atomic<bool> processing{false};
    if (processing.exchange(true)) {
        std::cout << "System is busy, please wait..." << std::endl;
        return;
    }
    
    std::cout << "Sending message: " << currentInput << std::endl;
    
    try {
        std::string response = languageModule->process(currentInput);
        chatHistory.push_back({currentInput, true});
        chatHistory.push_back({response, false});
    } catch (const std::exception& e) {
        std::cerr << "Error processing message: " << e.what() << std::endl;
        chatHistory.push_back({currentInput, true});
        chatHistory.push_back({"Sorry, I encountered an error", false});
    }
    
    processing = false;
    
    if (chatHistory.size() > 20) {
        chatHistory.erase(chatHistory.begin(), chatHistory.begin() + 2);
    }
    
    currentInput.clear();
    inputText.setString("");
    stickToBottom = true;
}

void UIModule::handleMouseClick(const sf::Event::MouseButtonPressed& event, NeuralFieldSystem& system, 
                               bool& simulation_running, StatisticsModule& stats) {
    sf::Vector2f mousePos{
        static_cast<float>(event.position.x),
        static_cast<float>(event.position.y)
    };
    
    // ===== НОВОЕ: Проверка клика по кнопке переключения чата =====
    if (toggleChatButton.getGlobalBounds().contains(mousePos)) {
        toggleChat();
        return;
    }
    
    // Проверяем кнопку Send (только если чат виден)
    if (chat_visible && sendButton.getGlobalBounds().contains(mousePos)) {
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
            languageModule->addExternalFeedback(1.0f);
            likePressed = true;
            dislikePressed = false;
            feedbackClock.restart();
            std::cout << "Good button clicked" << std::endl;
            return;
        }

        if (dislikeRect.contains(mousePos) && languageModule) {
            languageModule->giveFeedback(0.0f);
            languageModule->addExternalFeedback(0.0f);
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

    // Проверка кнопок визуализации
    float buttonY = 320.0f;
    float buttonWidth = 180.0f;
    float buttonHeight = 25.0f;
    float startX = windowWidth - config.control_panel_width + 10;
    
    // Орбиты
    sf::FloatRect orbitsRect(sf::Vector2f(startX, buttonY), sf::Vector2f(buttonWidth, buttonHeight));
    if (orbitsRect.contains(mousePos)) {
        toggleOrbits();
        return;
    }
    
    // Межгрупповые связи
    sf::FloatRect interRect(sf::Vector2f(startX, buttonY + 30), sf::Vector2f(buttonWidth, buttonHeight));
    if (interRect.contains(mousePos)) {
        toggleInterConnections();
        return;
    }
    
    // Внутригрупповые связи
    sf::FloatRect intraRect(sf::Vector2f(startX, buttonY + 60), sf::Vector2f(buttonWidth, buttonHeight));
    if (intraRect.contains(mousePos)) {
        toggleIntraConnections();
        return;
    }
    
    // Нейроны
    sf::FloatRect neuronsRect(sf::Vector2f(startX, buttonY + 90), sf::Vector2f(buttonWidth, buttonHeight));
    if (neuronsRect.contains(mousePos)) {
        toggleNeurons();
        return;
    }
    
    // НОВЫЕ 3D КНОПКИ
    
    // Авто-вращение
    sf::FloatRect autoRotateRect(sf::Vector2f(startX, buttonY + 120), sf::Vector2f(buttonWidth, buttonHeight));
    if (autoRotateRect.contains(mousePos)) {
        toggleAutoRotate();
        return;
    }
    
    // Сброс вида
    sf::FloatRect resetViewRect(sf::Vector2f(startX, buttonY + 150), sf::Vector2f(buttonWidth, buttonHeight));
    if (resetViewRect.contains(mousePos)) {
        resetView();
        return;
    }
    
    
    // Проверка клика по кнопке режима (ДОБАВИТЬ)
    sf::FloatRect modeButtonRect(
        sf::Vector2f(static_cast<float>(windowWidth - config.control_panel_width + 10), 290.0f),
        sf::Vector2f(180.0f, 25.0f)
    );
    
    if (modeButtonRect.contains(mousePos)) {
        cycleDisplayMode();
        std::cout << "Display mode: " << current_display_mode << std::endl;
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

    else if (autoLearnButton.getGlobalBounds().contains(mousePos)) {
        autoLearningActive = true;
        // ВАЖНО: автоматически запускаем симуляцию, если она остановлена
        if (!simulation_running) {
            simulation_running = true;
            stats.startTimer();
            std::cout << "Simulation AUTO-STARTED for learning" << std::endl;
        }
        std::cout << "AUTO-LEARNING STARTED" << std::endl;
        return;
    }

    else if (stopLearnButton.getGlobalBounds().contains(mousePos)) {
        autoLearningActive = false;
        // НЕ останавливаем симуляцию, только обучение
        std::cout << "AUTO-LEARNING STOPPED" << std::endl;
        return;
    }
}

void UIModule::draw(sf::RenderWindow& window, const NeuralFieldSystem& system, 
                   const StatisticsModule& stats, bool simulation_running,
                   const ResourceMonitor& resources, const EvolutionModule& evolution,
                   const MemoryManager& memory, int step) {
    
    // Рисуем нижнюю панель
    window.draw(bottomPanel);
    drawBottomPanel(window, resources, evolution);
    
    //CHAT
    drawChat(window);
    
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
        // Вместо старой статистики используем новую
        drawUnifiedStats(window);
        
        // Эволюцию и стазис оставляем для совместимости
        if (stats.getCurrentStats().total_energy < 0.001) {
            stasisText.setString("LOW ENERGY\nSTASIS MODE");
            stasisText.setFillColor(TEXT_WARNING);
            window.draw(stasisText);
        }
    }
}

void UIModule::drawChat(sf::RenderWindow& window)
{
    // ===== НОВОЕ: Всегда рисуем кнопку переключения =====
    window.draw(toggleChatButton);
    window.draw(toggleChatText);
    
    // Если чат скрыт - не рисуем остальное
    if (!chat_visible) return;
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

// Исправленный метод:
void UIModule::drawControlPanel(sf::RenderWindow& window, const StatisticsModule& stats, bool simulation_running) {
    
    startButton.setFillColor(simulation_running ? BTN_ACTIVE : BTN_BG);
    stopButton.setFillColor(!simulation_running ? BTN_ACTIVE : BTN_BG);
    resetButton.setFillColor(BTN_BG);
    
    // Рисуем кнопки автообучения
    window.draw(autoLearnButton);
    window.draw(stopLearnButton);
    window.draw(autoLearnText);
    window.draw(stopLearnText);
    
    window.draw(startButton);
    window.draw(stopButton);
    window.draw(resetButton);
    
    window.draw(startText);
    window.draw(stopText);
    window.draw(resetText);

    // Статус симуляции
    sf::Text statusText(font);
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

    // Статус автообучения
    if (autoLearningActive) {
        sf::Text autoStatusText(font);
        autoStatusText.setString("AUTO-LEARNING ACTIVE");
        autoStatusText.setCharacterSize(14);
        autoStatusText.setFillColor(sf::Color(100, 255, 100));
        autoStatusText.setPosition(sf::Vector2f(
            static_cast<float>(windowWidth - config.control_panel_width + 10),
            290.0f
        ));
        window.draw(autoStatusText);
        
        static sf::Clock blinkClock;
        if (static_cast<int>(blinkClock.getElapsedTime().asSeconds() * 2) % 2 == 0) {
            sf::CircleShape indicator(5);
            indicator.setFillColor(sf::Color(100, 255, 100));
            indicator.setPosition(sf::Vector2f(
                static_cast<float>(windowWidth - config.control_panel_width + 150),
                295.0f
            ));
            window.draw(indicator);
        }
    }

    // ===== КНОПКИ ВИЗУАЛИЗАЦИИ =====
    float buttonY = 320.0f;
    float buttonWidth = 180.0f;
    float buttonHeight = 25.0f;
    float startX = windowWidth - config.control_panel_width + 10;
    
    // Кнопка орбит
    sf::RectangleShape orbitsBtn(sf::Vector2f(buttonWidth, buttonHeight));
    orbitsBtn.setPosition(sf::Vector2f(startX, buttonY));
    orbitsBtn.setFillColor(visualizer && visualizer->getConfig().show_orbits ? BTN_ACTIVE : BTN_BG);
    window.draw(orbitsBtn);
    
    sf::Text orbitsText(font);
    orbitsText.setString("Orbits: " + std::string(visualizer && visualizer->getConfig().show_orbits ? "ON" : "OFF"));
    orbitsText.setCharacterSize(12);
    orbitsText.setFillColor(TEXT_MAIN);
    orbitsText.setPosition(sf::Vector2f(startX + 15, buttonY + 5));
    window.draw(orbitsText);
    
    // Кнопка межгрупповых связей
    sf::RectangleShape interBtn(sf::Vector2f(buttonWidth, buttonHeight));
    interBtn.setPosition(sf::Vector2f(startX, buttonY + 30));
    interBtn.setFillColor(visualizer && visualizer->getConfig().show_connections ? BTN_ACTIVE : BTN_BG);
    window.draw(interBtn);
    
    sf::Text interText(font);
    interText.setString("Inter: " + std::string(visualizer && visualizer->getConfig().show_connections ? "ON" : "OFF"));
    interText.setCharacterSize(12);
    interText.setFillColor(TEXT_MAIN);
    interText.setPosition(sf::Vector2f(startX + 15, buttonY + 35));
    window.draw(interText);
    
    // Кнопка внутригрупповых связей
    sf::RectangleShape intraBtn(sf::Vector2f(buttonWidth, buttonHeight));
    intraBtn.setPosition(sf::Vector2f(startX, buttonY + 60));
    intraBtn.setFillColor(visualizer && visualizer->getConfig().show_intra_connections ? BTN_ACTIVE : BTN_BG);
    window.draw(intraBtn);
    
    sf::Text intraText(font);
    intraText.setString("Intra: " + std::string(visualizer && visualizer->getConfig().show_intra_connections ? "ON" : "OFF"));
    intraText.setCharacterSize(12);
    intraText.setFillColor(TEXT_MAIN);
    intraText.setPosition(sf::Vector2f(startX + 15, buttonY + 65));
    window.draw(intraText);
    
    // Кнопка нейронов
    sf::RectangleShape neuronsBtn(sf::Vector2f(buttonWidth, buttonHeight));
    neuronsBtn.setPosition(sf::Vector2f(startX, buttonY + 90));
    neuronsBtn.setFillColor(visualizer && visualizer->getConfig().show_neurons ? BTN_ACTIVE : BTN_BG);
    window.draw(neuronsBtn);
    
    sf::Text neuronsText(font);
    neuronsText.setString("Neurons: " + std::string(visualizer && visualizer->getConfig().show_neurons ? "ON" : "OFF"));
    neuronsText.setCharacterSize(12);
    neuronsText.setFillColor(TEXT_MAIN);
    neuronsText.setPosition(sf::Vector2f(startX + 15, buttonY + 95));
    window.draw(neuronsText);
    
    // ===== НОВЫЕ 3D КНОПКИ =====
    // Кнопка авто-вращения
    sf::RectangleShape autoRotateBtn(sf::Vector2f(buttonWidth, buttonHeight));
    autoRotateBtn.setPosition(sf::Vector2f(startX, buttonY + 120));
    autoRotateBtn.setFillColor(visualizer && visualizer->getConfig().auto_rotate ? BTN_ACTIVE : BTN_BG);
    window.draw(autoRotateBtn);
    
    sf::Text autoRotateText(font);
    autoRotateText.setString("Auto Rotate: " + std::string(visualizer && visualizer->getConfig().auto_rotate ? "ON" : "OFF"));
    autoRotateText.setCharacterSize(12);
    autoRotateText.setFillColor(TEXT_MAIN);
    autoRotateText.setPosition(sf::Vector2f(startX + 15, buttonY + 125));
    window.draw(autoRotateText);
    
    // Кнопка сброса вида
    sf::RectangleShape resetViewBtn(sf::Vector2f(buttonWidth, buttonHeight));
    resetViewBtn.setPosition(sf::Vector2f(startX, buttonY + 150));
    resetViewBtn.setFillColor(BTN_BG);
    window.draw(resetViewBtn);
    
    sf::Text resetViewText(font);
    resetViewText.setString("Reset View");
    resetViewText.setCharacterSize(12);
    resetViewText.setFillColor(TEXT_MAIN);
    resetViewText.setPosition(sf::Vector2f(startX + 15, buttonY + 155));
    window.draw(resetViewText);
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

// modules/UIModule.cpp - в методе drawUnifiedStats()

void UIModule::drawUnifiedStats(sf::RenderWindow& window) {
    if (!stats_collector) {
        sf::Text errorText(font);
        errorText.setString("Stats collector not connected");
        errorText.setCharacterSize(12);
        errorText.setFillColor(TEXT_WARNING);
        errorText.setPosition(sf::Vector2f(
            windowWidth - config.control_panel_width + 10,
            320.0f
        ));
        window.draw(errorText);
        return;
    }
    
    const auto& all_stats = stats_collector->getAllStats();
    
    std::stringstream ss;
    ss << "SYSTEM DASHBOARD\n";
    ss << "--\n";
    
    if (current_display_mode == 0) {
        // COMPACT MODE
        ss << stats_collector->getCompactStats();
    } 
    else if (current_display_mode == 5) {  // CONCEPTS MODE
        ss << "CONCEPTS MASTERY\n\n";
        
        // Получаем LearningOrchestrator через stats_collector
        auto* learning = stats_collector->getLearning();
        if (learning) {
            // Получаем доступ к ConceptMasteryEvaluator
            auto& evaluator = learning->getMasteryEvaluator();
            
            // Общая статистика
            float avg_mastery = evaluator.getAverageMastery();
            int mastered = evaluator.getMasteredConceptsCount(0.7f);
            
            // Для "learning" концептов (30-70%) - нужно посчитать самим
            int learning_count = 0;
            for (uint32_t id = 1; id <= 614; id++) {
                float mastery = evaluator.getConceptMastery(id);
                if (mastery > 0.3f && mastery < 0.7f) {
                    learning_count++;
                }
            }
            int total_concepts = mastered + learning_count;
            
            // Прогресс-бар (создаем вручную)
            ss << "Overall Mastery: ";
            int filled = static_cast<int>(avg_mastery * 30);
            for (int i = 0; i < 30; i++) {
                ss << (i < filled ? "█" : "░");
            }
            ss << "\n";
            ss << "Mastered (≥70%): " << mastered << " concepts\n";
            ss << "Learning (30-70%): " << learning_count << " concepts\n";
            ss << "Total active: " << total_concepts << " concepts\n\n";
            
            // Статистика по уровням абстракции
            ss << "By Abstraction Level:\n";
            auto level_stats = evaluator.getMasteryByAbstractionLevel();
            for (int level = 1; level <= 10; level++) {
                if (level_stats.count(level)) {
                    int filled = static_cast<int>(level_stats[level] * 20);
                    ss << "  Level " << level << ": ";
                    for (int i = 0; i < 20; i++) {
                        ss << (i < filled ? "█" : "░");
                    }
                    ss << " " << std::fixed << std::setprecision(0) 
                       << (level_stats[level] * 100) << "%\n";
                }
            }
            
            // Топ-5 самых освоенных концептов
            ss << "\nTop Mastered Concepts:\n";
            auto all_mastery = evaluator.getAllConceptsMastery();
            
            std::vector<std::pair<uint32_t, float>> sorted(
                all_mastery.begin(), all_mastery.end());
            std::sort(sorted.begin(), sorted.end(),
                [](const auto& a, const auto& b) { return a.second > b.second; });
            
            int count = 0;
            for (const auto& [id, mastery] : sorted) {
                if (count++ >= 5) break;
                if (semantic_graph_) {
                    auto node = semantic_graph_->getNode(id);
                    if (node) {
                        ss << "  " << node->canonical_form << ": " 
                           << std::fixed << std::setprecision(0) << (mastery * 100) << "%\n";
                    }
                } else {
                    ss << "  Concept " << id << ": " 
                       << std::fixed << std::setprecision(0) << (mastery * 100) << "%\n";
                }
            }
            
            // Концепты, которые нужно подучить (30-70%)
            ss << "\nNeed Practice:\n";
            count = 0;
            for (const auto& [id, mastery] : sorted) {
                if (mastery > 0.3f && mastery < 0.7f) {
                    if (count++ >= 3) break;
                    if (semantic_graph_) {
                        auto node = semantic_graph_->getNode(id);
                        if (node) {
                            ss << "  " << node->canonical_form << ": " 
                               << std::fixed << std::setprecision(0) << (mastery * 100) << "%\n";
                        }
                    } else {
                        ss << "  Concept " << id << ": " 
                           << std::fixed << std::setprecision(0) << (mastery * 100) << "%\n";
                    }
                }
            }
        } else {
            ss << "Learning module not available\n";
        }
    } else {
        // DETAILED MODE
        std::string module_name;
        switch(current_display_mode) {
            case 1: module_name = "neural"; break;
            case 2: module_name = "evolution"; break;
            case 3: module_name = "language"; break;
            case 4: module_name = "memory"; break;
        }
        
        auto it = all_stats.find(module_name);
        if (it != all_stats.end()) {
            ss << "=== " << it->second.name << " ===\n";
            for (const auto& [key, val] : it->second.numeric_stats) {
                ss << "  " << key << ": ";
                if (val == 0 || val == 1) {
                    ss << (val == 1 ? "YES" : "NO");
                } else {
                    ss << std::fixed << std::setprecision(2) << val;
                }
                ss << "\n";
            }
        }
    }
    
    // Добавляем подсказку по переключению
    std::string modeNames[] = {"Compact", "Neural", "Evolution", "Language", "Memory", "Concepts"};
    ss << "\n[Click mode to cycle: " << modeNames[current_display_mode] << "]";
    
    sf::Text statsText(font);
    statsText.setString(ss.str());
    statsText.setCharacterSize(12);
    statsText.setFillColor(TEXT_MAIN);
    statsText.setPosition(sf::Vector2f(
        windowWidth - config.control_panel_width + 10,
        320.0f
    ));
    window.draw(statsText);
    
    // Рисуем кнопку переключения режима
    sf::RectangleShape modeButton(sf::Vector2f(180.0f, 25.0f));
    modeButton.setPosition(sf::Vector2f(
        windowWidth - config.control_panel_width + 10,
        290.0f
    ));
    modeButton.setFillColor(sf::Color(70, 70, 90));
    window.draw(modeButton);
    
    modeText.setString("Mode: " + modeNames[current_display_mode]);
    modeText.setPosition(sf::Vector2f(
        windowWidth - config.control_panel_width + 15,
        295.0f
    ));
    window.draw(modeText);
}

void UIModule::handleMouseWheel(const sf::Event::MouseWheelScrolled& event) {
    
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
            statusText.setString("Status: Confident");
        } else if (state.confusion > 0.6f) {
            statusText.setString("Status: Confused");
        } else {
            statusText.setString("Status: Learning");
        }
        window.draw(statusText);
        
    } catch (const std::exception& e) {
        std::cerr << "Error in drawReflectionPanel: " << e.what() << std::endl;
    }
}

void UIModule::drawStatistics(
    sf::RenderWindow& window, 
    const StatisticsModule& stats, 
    const EvolutionModule& evolution) {

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
                    << "Fitness: " << formatDouble(evolution.getOverallFitness()) << "\n"
                    << "Best: " << formatDouble(evolution.getBestFitness()) << "\n"
                 << "Generation: " << stats.getHistory().size() / 100 << "\n"
                 << "Mutations: " << (current.step / 1000);
    
    evolutionText.setString(evolution_ss.str());
    window.draw(evolutionText);
    
    // Стазис
    if (stats.getCurrentStats().total_energy < 0.001) {
        stasisText.setString("LOW ENERGY\nSTASIS MODE");
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
              //<< "Reduction Cooldown: " << evolution.getReductionCooldown() << "s\n"
              << "Overall Fitness: " << evolution.getOverallFitness() << "/min\n"
              << "Best Fitness: " << formatDouble(evolution.getBestFitness(), 2);
    
    configText.setString(config_ss.str());
    window.draw(configText);
}

std::string UIModule::formatDouble(double value, int precision) const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(precision) << value;
    return ss.str();
}

void UIModule::toggleOrbits() {
    if (visualizer) {
        auto& cfg = visualizer->getConfig();
        cfg.show_orbits = !cfg.show_orbits;
        std::cout << "Orbits: " << (cfg.show_orbits ? "ON" : "OFF") << std::endl;
    }
}

void UIModule::toggleInterConnections() {
    if (visualizer) {
        auto& cfg = visualizer->getConfig();
        cfg.show_connections = !cfg.show_connections;
        std::cout << "Inter-group connections: " << (cfg.show_connections ? "ON" : "OFF") << std::endl;
    }
}

void UIModule::toggleIntraConnections() {
    if (visualizer) {
        auto& cfg = visualizer->getConfig();
        cfg.show_intra_connections = !cfg.show_intra_connections;
        std::cout << "Intra-group connections: " << (cfg.show_intra_connections ? "ON" : "OFF") << std::endl;
    }
}

void UIModule::toggleNeurons() {
    if (visualizer) {
        auto& cfg = visualizer->getConfig();
        cfg.show_neurons = !cfg.show_neurons;
        std::cout << "Neurons: " << (cfg.show_neurons ? "ON" : "OFF") << std::endl;
    }
}

void UIModule::toggleAutoRotate() {
    if (visualizer) {
        auto& cfg = visualizer->getConfig();
        cfg.auto_rotate = !cfg.auto_rotate;
        std::cout << "Auto-rotate: " << (cfg.auto_rotate ? "ON" : "OFF") << std::endl;
    }
}

void UIModule::resetView() {
    if (visualizer) {
        visualizer->resetView();
        std::cout << "View reset" << std::endl;
    }
}

void UIModule::handleRotate(float delta) {
    if (visualizer) {
        visualizer->handleRotate(delta);
    }
}

void UIModule::handleTilt(float delta) {
    if (visualizer) {
        visualizer->handleTilt(delta);
    }
}