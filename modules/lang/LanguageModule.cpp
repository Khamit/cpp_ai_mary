// modules/lang/LanguageModule.cpp
#include "LanguageModule.hpp"
#include "../../core/Config.hpp"
#include "../../core/MemoryManager.hpp"
#include "../learning/CuriosityDriver.hpp"
#include "../../core/AccessLevel.hpp"
#include "../../data/PersonnelData.hpp"
#include "../../data/DynamicSemanticMemory.hpp" 
#include <thread>
#include <numeric>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>

// Конструктор
LanguageModule::LanguageModule(NeuralFieldSystem& system, 
                             ImmutableCore& core,
                             IAuthorization& auth,
                             PersonnelDatabase& personnel_db,
                             MemoryManager& memory_manager)
    : neural_system(system)
    , immutable_core(core)
    , auth(auth)
    , personnel_db_(personnel_db)
    , memory_manager_(memory_manager)
    , rng_(std::random_device{}())
    , dialogue_state_()
    , orchestrator_(nullptr) 
{
    // СНАЧАЛА создаём динамическую семантическую память
    dynamic_memory_ = std::make_unique<DynamicSemanticMemory>(
        neural_system, memory_manager, personnel_db);
    
    // ПОТОМ инициализируем WebTrainer (он использует dynamic_memory_)
    initWebTrainer();
    
    system_mode_ = "personal";
    default_user_ = "user";
    process_step_counter_ = 0;
    
    std::cout << "LanguageModule initialized with DynamicSemanticMemory" << std::endl;
}

std::shared_ptr<CuriosityDriver> LanguageModule::createCuriosityDriver() {
    // CuriosityDriver теперь использует динамическую память
    return std::make_shared<CuriosityDriver>(neural_system, *this, dynamic_memory_.get());
}

// Деструктор
LanguageModule::~LanguageModule() {
    stopAutoLearning();
    if (dynamic_memory_) {
        dynamic_memory_->saveToMemory();
    }
}

bool LanguageModule::initialize(const Config& config) {
    std::cout << "LanguageModule initialized from config" << std::endl;
    return true;
}

void LanguageModule::shutdown() {
    if (dynamic_memory_) {
        dynamic_memory_->saveToMemory();
    }
    std::cout << "LanguageModule shutting down" << std::endl;
}

void LanguageModule::update(float dt) {
    static int cleanup_counter = 0;
    cleanup_counter++;
    static int dream_counter = 0;
    dream_counter++;
    
    // Каждые 1000 шагов сохраняем динамическую память
    if (cleanup_counter >= 1000 && dynamic_memory_) {
        cleanup_counter = 0;
        dynamic_memory_->saveToMemory();
    }
    
}

void LanguageModule::saveState(MemoryManager& memory) {
    std::vector<float> feedbackData = {
        external_feedback_sum_,
        static_cast<float>(external_feedback_count_)
    };
    
    std::map<std::string, std::string> metadata;
    metadata["module"] = "LanguageModule";
    
    memory.store("LanguageModule", "feedback", feedbackData, 0.5f, metadata);
    
    if (dynamic_memory_) {
        dynamic_memory_->saveToMemory();
    }
}

void LanguageModule::loadState(MemoryManager& memory) {
    auto records = memory.getRecords("LanguageModule");
    for (const auto& record : records) {
        if (record.type == "feedback" && record.data.size() >= 2) {
            external_feedback_sum_ = record.data[0];
            external_feedback_count_ = static_cast<int>(record.data[1]);
        }
    }
    
    if (dynamic_memory_) {
        dynamic_memory_->loadFromMemory();
    }
    
    std::cout << "LanguageModule state loaded" << std::endl;
}

void LanguageModule::runAutoLearning(int steps, LearningOrchestrator* orchestrator) {
    if (learning_active_) return;
    
    learning_active_ = true;
    auto_learning_active_ = true;

    if (orchestrator) {
        learning_thread_ = std::thread([this, orchestrator, steps]() {
            std::cout << "Auto-learning thread started" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            learning_active_ = false;
            auto_learning_active_ = false;
            std::cout << "Auto-learning thread finished" << std::endl;
        });
    }
}

void LanguageModule::stopAutoLearning() {
    learning_active_ = false;
    auto_learning_active_ = false;
    if (learning_thread_.joinable()) {
        learning_thread_.join();
    }
}

// ========== ОСНОВНОЙ МЕТОД ==========
std::string LanguageModule::process(const std::string& input, const std::string& user_name) {
    if (!dynamic_memory_) {
        return "Error: Dynamic memory not initialized";
    }
    // Добавить текущий step
    static int step_counter = 0;
    step_counter++;
    
    // 1. Измеряем состояние ДО
    float before_energy = neural_system.computeTotalEnergy();
    float before_entropy = neural_system.getUnifiedEntropy();
    
    // 2. Обработка через динамическую память
    auto meanings = dynamic_memory_->processText(input, user_name);
    
    // 3. Генерация ответа через динамическую память
    std::string response = dynamic_memory_->generateResponse(user_name, meanings);
    
    // 4. Измеряем состояние ПОСЛЕ
    float after_energy = neural_system.computeTotalEnergy();
    float after_entropy = neural_system.getUnifiedEntropy();
    
    // 5. Вычисляем награду
    float energy_delta = std::max(0.01f, after_energy - before_energy);
    float accuracy = evaluateResponse(input, response);
    
    float reward = (accuracy * accuracy) / energy_delta;
    reward *= (1.0f - 0.5f * after_entropy);
    reward = std::clamp(reward, 0.0f, 1.0f);

    // Передаём step_counter в applyUserFeedback (ОДИН РАЗ!)
    dynamic_memory_->applyUserFeedback(user_name, reward, meanings, step_counter);
    giveFeedback(reward);

    // Штраф за повторение
    static std::string last_response;
    static int repeat_count = 0;
    
    if (response == last_response) {
        repeat_count++;
        float repeat_penalty = std::min(0.5f, repeat_count * 0.1f);
        reward *= (1.0f - repeat_penalty);
    } else {
        repeat_count = 0;
        last_response = response;
    }
    
    // 7. Auth и команды
    std::string effective_user = (system_mode_ == "personal") ? default_user_ : user_name;
    if (!auth.authorize(effective_user)) return "";
    
    std::string special_output;
    if (isSpecialCommand(input) && handleSpecialCommand(input, special_output)) {
        return special_output;
    }
    
    // 8. Ответ
    return response;
}

bool LanguageModule::canExecuteCommand(const std::string& user_name, 
                                      const std::string& command,
                                      AccessLevel required_level) {
    
    if (system_mode_ == "personal") {
        std::cout << "Personal mode: command '" << command << "' auto-approved" << std::endl;
        return true;
    }
    
    if (!auth.canPerform(user_name, command, required_level)) {
        return false;
    }
    
    PermissionRequest req;
    req.action = "user_command_" + command;
    req.source_module = "LanguageModule";
    req.estimated_impact = 0.3;
    req.reason = "User " + user_name + " requested " + command;
    
    return immutable_core.requestPermission(req);
}

// ========== УТИЛИТЫ ==========
std::vector<std::string> LanguageModule::split(const std::string& text) {
    std::vector<std::string> tokens;
    std::stringstream ss(text);
    std::string token;
    while (std::getline(ss, token, ' ')) {
        if (!token.empty()) {
            token.erase(std::remove(token.begin(), token.end(), '\n'), token.end());
            token.erase(std::remove(token.begin(), token.end(), '\r'), token.end());
            token.erase(std::remove(token.begin(), token.end(), '\t'), token.end());
            
            if (!token.empty()) {
                std::transform(token.begin(), token.end(), token.begin(), ::tolower);
                tokens.push_back(token);
            }
        }
    }
    return tokens;
}

std::string LanguageModule::toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), ::tolower);
    return text;
}

bool LanguageModule::isSpecialCommand(const std::string& input) {
    return input == "?" || 
           input == "help" || 
           input == "status" || 
           input == "learn" ||
           input == "save" ||
           input == "stats" ||
           input.find("answer:") == 0;
}

bool LanguageModule::handleSpecialCommand(const std::string& input, std::string& output) {
    if (input == "?") {
        if (curiosity_driver_) {
            std::string question = getNextQuestion();
            dialogue_state_.last_question = question;
            dialogue_state_.expecting_answer = true;
            dialogue_state_.question_time = std::chrono::system_clock::now();
            output = "... " + question;
            return true;
        } else if (dynamic_memory_) {
            // Fallback: задаём простой вопрос
            output = "... What would you like to talk about?";
            return true;
        }
    }
    else if (input == "help") {
        std::stringstream ss;
        ss << "Available commands:\n";
        ss << "  ? - Ask a question\n";
        ss << "  answer: <text> - Answer a question\n";
        ss << "  learn - Start learning mode\n";
        ss << "  status - Show learning status\n";
        ss << "  stats - Show detailed statistics\n";
        ss << "  save - Save current memory\n";
        output = ss.str();
        return true;
    }
    else if (input == "status") {
        output = getLearningStatus();
        return true;
    }
    else if (input == "stats") {
        output = getDetailedStats();
        return true;
    }
    else if (input == "save") {
        if (dynamic_memory_) {
            dynamic_memory_->saveToMemory();
            output = "Memory saved successfully!";
        } else {
            output = "Dynamic memory not available";
        }
        return true;
    }
    else if (input.find("answer:") == 0) {
        if (dialogue_state_.expecting_answer) {
            std::string answer = input.substr(7);
            output = processAnswer(answer);
            return true;
        } else {
            output = "I wasn't expecting an answer right now. Type '?' if you want to ask something.";
            return true;
        }
    }
    return false;
}

std::string LanguageModule::getNextQuestion() {
    if (!curiosity_driver_ && !dynamic_memory_) {
        return "I'm not curious right now.";
    }
    
    if (curiosity_driver_) {
        return curiosity_driver_->getNextQuestion();
    }
    
    // Простые вопросы из динамической памяти
    static const std::vector<std::string> basic_questions = {
        "What do you think about?",
        "How are you feeling today?",
        "What would you like to learn?",
        "Tell me something interesting."
    };
    
    std::uniform_int_distribution<> dist(0, basic_questions.size() - 1);
    return basic_questions[dist(rng_)];
}

std::string LanguageModule::processAnswer(const std::string& answer) {
    if (!dynamic_memory_) {
        dialogue_state_.expecting_answer = false;
        return "I can't learn right now.";
    }
    
    if (!dialogue_state_.expecting_answer) {
        return "I wasn't expecting an answer.";
    }
    
    if (curiosity_driver_) {
        curiosity_driver_->processAnswer(dialogue_state_.last_question, answer);
    } else {
        // Обрабатываем ответ через динамическую память
        auto meanings = dynamic_memory_->processText(answer, default_user_);
        dynamic_memory_->applyUserFeedback(default_user_, 0.7f, meanings);
    }
    
    auto now = std::chrono::system_clock::now();
    auto response_time = std::chrono::duration_cast<std::chrono::seconds>(
        now - dialogue_state_.question_time).count();
    
    dialogue_state_.expecting_answer = false;
    
    return "Thanks for teaching me! (Response time: " + std::to_string(response_time) + "s)";
}

std::string LanguageModule::getLearningStatus() const {
    std::stringstream ss;
    ss << "Learning Status:\n";
    ss << "  Mode: " << system_mode_ << "\n";
    ss << "  Recent meanings: " << recent_meanings_.size() << "/" << MAX_RECENT_MEANINGS << "\n";
    ss << "  Expecting answer: " << (dialogue_state_.expecting_answer ? "yes" : "no") << "\n";
    
    if (dialogue_state_.expecting_answer && !dialogue_state_.last_question.empty()) {
        ss << "  Last question: \"" << dialogue_state_.last_question << "\"\n";
    }
    
    ss << "  External feedback: " << getExternalFeedbackAvg() << "\n";
    ss << "  Fitness: " << getLanguageFitness();
    
    if (dynamic_memory_) {
        ss << "\n  Dynamic memory active: YES\n";
        ss << "  User profiles: " << dynamic_memory_->getAllProfiles().size() << "\n";
    }
    
    return ss.str();
}

std::string LanguageModule::getDetailedStats() const {
    std::stringstream ss;
    ss << "\n=== DETAILED STATISTICS ===\n";
    ss << getLearningStatus() << "\n";
    
    if (dynamic_memory_) {
        ss << "\n--- Dynamic Memory Stats ---\n";
        ss << "Words learned: " << dynamic_memory_->getWordCount() << "\n";
        ss << "Patterns detected: " << dynamic_memory_->getPatternCount() << "\n";
        ss << "Meanings formed: " << dynamic_memory_->getMeaningCount() << "\n";
        
        ss << "\n--- User Profiles ---\n";
        for (const auto& [name, profile] : dynamic_memory_->getAllProfiles()) {
            ss << "  " << name << ": trust=" << std::fixed << std::setprecision(2) 
               << profile.trust_level << ", mood=" << profile.average_mood
               << ", interactions=" << profile.interaction_count << "\n";
        }
    }
    
    ss << "===========================\n";
    return ss.str();
}

void LanguageModule::giveFeedback(float rating, bool autoFeedback) {
    addExternalFeedback(rating);
    
    if (!autoFeedback) {
        std::cout << "User feedback: " << rating << std::endl;
    }
    
    // Динамическая память уже обработала фидбек в process()
    // Здесь только сохраняем статистику
    static int feedback_counter = 0;
    feedback_counter++;
    
    if (feedback_counter % 10 == 0 && dynamic_memory_) {
        dynamic_memory_->saveToMemory();
        std::cout << "Memory saved (feedback #" << feedback_counter << ")" << std::endl;
    }
}

double LanguageModule::getLanguageFitness() const {
    float externalScore = getExternalFeedbackAvg();
    float diversityScore = 0.5f;
    
    if (external_feedback_count_ < 10) {
        externalScore = 0.5f;
    }
    
    if (externalScore > 0.95f) {
        externalScore = 0.6f;
    }
    
    return 0.7f * externalScore + 0.3f * diversityScore;
}

float LanguageModule::evaluateResponse(const std::string& input, const std::string& response) {
    // Простая эвристическая оценка
    std::string input_lower = toLower(input);
    std::string response_lower = toLower(response);
    
    // Проверяем известные паттерны
    if (input_lower.find("your name") != std::string::npos) {
        if (response_lower.find("mary") != std::string::npos) {
            return 1.0f;
        }
        return 0.0f;
    }
    
    if (input_lower.find("hello") != std::string::npos ||
        input_lower.find("hi") != std::string::npos) {
        if (response_lower.find("hello") != std::string::npos ||
            response_lower.find("hi") != std::string::npos) {
            return 0.8f;
        }
        return 0.2f;
    }
    
    if (input_lower.find("how are you") != std::string::npos) {
        if (response_lower.find("good") != std::string::npos ||
            response_lower.find("fine") != std::string::npos ||
            response_lower.find("well") != std::string::npos) {
            return 0.9f;
        }
        return 0.3f;
    }
    
    // Если ответ не пустой и не "...", даём базовую оценку
    if (!response.empty() && response != "...") {
        return 0.6f;
    }
    
    return 0.5f;
}

void LanguageModule::addToRecentMeanings(uint32_t meaning_id) {
    recent_meanings_.push_back(meaning_id);
    if (recent_meanings_.size() > MAX_RECENT_MEANINGS) {
        recent_meanings_.pop_front();
    }
}