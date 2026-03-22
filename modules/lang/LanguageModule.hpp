// modules/lang/LanguageModule.hpp
#pragma once
#include <string>
#include <vector>
#include <map>
#include <random>
#include <memory>
#include <deque>
#include <thread>
#include <unordered_map>
#include <mutex>      
#include <chrono>     
#include <iostream>   
#include "../../core/Component.hpp"
#include "../../core/NeuralFieldSystem.hpp"
#include "../semantic/SemanticManager.hpp"
#include "../semantic/ThoughtPredictor.hpp"
#include "../../core/ImmutableCore.hpp"
#include "../../core/IAuthorization.hpp"
#include "../../data/SemanticGraphDatabase.hpp"
//#include "EffectiveLearning_fwd.hpp" 

// Forward declaration
class CuriosityDriver;

//class EffectiveLearning;
class LearningOrchestrator;  // forward declaration

// Структура для хранения состояния диалога
struct DialogueState {
    bool expecting_answer = false;
    std::string last_question;
    std::string last_response;
    std::chrono::system_clock::time_point question_time;
};


class LanguageModule : public Component {
private:
    NeuralFieldSystem& neural_system;
    ImmutableCore& immutable_core;           // техническая защита
    IAuthorization& auth;                     // социальная защита
    std::mt19937 rng_;

    DialogueState dialogue_state_;

    std::unique_ptr<SemanticManager> semantic_manager;
    std::unique_ptr<ThoughtPredictor> thought_predictor;

    SemanticGraphDatabase* semantic_graph_ = nullptr;

    int process_step_counter_ = 0;

    //std::unique_ptr<NeuralTrainer> neural_trainer_;

    std::thread learning_thread_;  // нужно join в деструкторе!
    std::atomic<bool> learning_active_{false};
    
    // НОВОЕ: драйвер любопытства
    std::shared_ptr<CuriosityDriver> curiosity_driver_;
    
    static constexpr int GROUP_SEMANTIC_START = 16;
    
    float external_feedback_sum_ = 0.0f;
    int external_feedback_count_ = 0;
    
    // Для совместимости со старым UI
    bool auto_learning_active_ = false;
    
    // Информация о режиме работы
    std::string system_mode_ = "personal";
    std::string default_user_ = "user";
    bool enforce_commands_ = false;
    bool emotional_responses_ = true;

    // в private секцию
    bool dictionary_initialized_ = false;
    std::mutex dict_mutex_;
    
    // История последних смыслов для анализа
    std::deque<uint32_t> recent_meanings_;
    static constexpr size_t MAX_RECENT_MEANINGS = 50;

    // словарь для быстрого поиска слов -> смыслы
    std::unordered_map<std::string, std::vector<uint32_t>> word_to_meaning;
    
    // Приватные методы
    std::vector<std::string> split(const std::string& text);
    std::string toLower(std::string text);
    std::string meaningsToText(const std::vector<uint32_t>& meanings);
    float calculateConfidence(const std::vector<uint32_t>& meanings);
    
    // Проверка, можно ли выполнить команду
    bool canExecuteCommand(const std::string& user_name, 
                          const std::string& command,
                          AccessLevel required_level);

    std::string buildFrameResponse(uint32_t frame_id, 
                    const std::vector<uint32_t>& meanings);
    
public:
    explicit LanguageModule(NeuralFieldSystem& system, 
                           ImmutableCore& core,
                           IAuthorization& auth, SemanticGraphDatabase* graph = nullptr);
    
    ~LanguageModule();

    SemanticManager& getSemanticManager() { 
        if (!semantic_manager) {
            throw std::runtime_error("semantic_manager is null in getSemanticManager()");
        }
        return *semantic_manager; 
    }


    std::string getName() const override { return "LanguageModule"; }
    bool initialize(const Config& config) override;
    void shutdown() override;
    void update(float dt) override;
    void saveState(MemoryManager& memory) override;
    void loadState(MemoryManager& memory) override;

    std::shared_ptr<CuriosityDriver> createCuriosityDriver();
    bool hasCuriosityDriver() const { return curiosity_driver_ != nullptr; }
    // Основной метод обработки с учетом пользователя (возвращает ответ)
    std::string process(const std::string& input, const std::string& user_name = "guest");
    // НОВЫЕ МЕТОДЫ ДЛЯ ДИАЛОГОВОГО ОБУЧЕНИЯ
    // капец их много - НЕОБХОДИМА РЕСТРУКТОРИЗАЦИЯ! СЛИШКОМ МНОГО КОДА В КЛАССЕ!!!
    // Проверить, является ли ввод специальной командой
    bool isSpecialCommand(const std::string& input);
    
    // Обработать специальную команду (возвращает true если команда обработана)
    bool handleSpecialCommand(const std::string& input, std::string& output);
    
    // Получить следующий вопрос от системы
    std::string getNextQuestion();
    
    // Обработать ответ на вопрос
    std::string processAnswer(const std::string& answer);
    
    // Получить статус обучения
    std::string getLearningStatus() const;
    
    // Ожидает ли система ответа?
    bool isExpectingAnswer() const { return dialogue_state_.expecting_answer; }
    
    // Сбросить состояние диалога
    void resetDialogue() { dialogue_state_.expecting_answer = false; }


    void giveFeedback(float rating, bool autoFeedback = false);
    double getLanguageFitness() const;

    // Методы для UI (совместимость)
    void runAutoLearning(int steps, LearningOrchestrator* orchestrator);

    void stopAutoLearning();
    bool isAutoLearningActive() const { return auto_learning_active_; }
    
    void saveAll() {}  // заглушка
    int getLearnedWordsCount() const { return word_to_meaning.size(); } 

    void addExternalFeedback(float rating) {
        external_feedback_sum_ += rating;
        external_feedback_count_++;
    }
    
    float getExternalFeedbackAvg() const {
        return external_feedback_count_ > 0 ? 
               external_feedback_sum_ / external_feedback_count_ : 0.5f;
    }

    void setSemanticGraph(SemanticGraphDatabase& graph) {
        semantic_graph_ = &graph;
        initializeWordDictionary();
    }

    // Установка режима работы
    void setSystemMode(const std::string& mode, const std::string& default_user = "user") {
        system_mode_ = mode;
        default_user_ = default_user;
        std::cout << "LanguageModule switched to " << mode << " mode" << std::endl;
    }

    void setCommandEnforcement(bool enforce) { enforce_commands_ = enforce; }
    void setEmotionalResponses(bool enable) { emotional_responses_ = enable; }
    
    // Установка драйвера любопытства
    void setCuriosityDriver(std::shared_ptr<CuriosityDriver> driver) {
        curiosity_driver_ = driver;
    }

    // Публичные методы для доступа к смыслам
    std::vector<uint32_t> textToMeanings(const std::string& text);
    
    // НОВЫЙ МЕТОД: получить недавние смыслы
    std::vector<uint32_t> getRecentMeanings() const {
        return std::vector<uint32_t>(recent_meanings_.begin(), recent_meanings_.end());
    }
    void initializeWordDictionary();

    int getProcessStepCounter() const { return process_step_counter_; }
    void incrementProcessStepCounter() { process_step_counter_++; }
};