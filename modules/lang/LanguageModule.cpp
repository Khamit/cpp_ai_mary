// modules/lang/LanguageModule.cpp
#include "LanguageModule.hpp"
#include "../../core/Config.hpp"
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

LanguageModule::LanguageModule(NeuralFieldSystem& system, 
                             ImmutableCore& core,
                             IAuthorization& auth,
                             PersonnelDatabase& personnel_db)
    : neural_system(system)
    , immutable_core(core)
    , auth(auth)
    , personnel_db_(personnel_db)
    , rng_(std::random_device{}())
    , dialogue_state_()
    , orchestrator_(nullptr) 
{
    // Создаём динамическую семантическую память
    dynamic_memory_ = std::make_unique<DynamicSemanticMemory>(
        neural_system, personnel_db);

    initWebTrainer();

    dynamic_memory_->loadFromMemory();

    system_mode_ = "personal";
    default_user_ = "user";
    process_step_counter_ = 0;
    
    std::cout << "LanguageModule initialized with DynamicSemanticMemory (EmergentCore mode)" << std::endl;
}

std::shared_ptr<CuriosityDriver> LanguageModule::createCuriosityDriver() {
    return std::make_shared<CuriosityDriver>(neural_system, *this, dynamic_memory_.get());
}

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

void LanguageModule::applyEliteInstruction(const std::string& intent) {
    auto& groups = neural_system.getGroupsNonConst();
    
    if (intent == "focus_on_letters") {
        for (int g = 0; g < groups.size(); g++) {
            bool is_elite_group = (g == 4) || (g >= 16 && g <= 21);
            if (is_elite_group) {
                groups[g].eliteInstruct("letters", 0.3f);
            }
        }
    } 
    else if (intent == "focus_on_words") {
        for (int g = 0; g < groups.size(); g++) {
            bool is_elite_group = (g == 4) || (g >= 16 && g <= 21);
            if (is_elite_group) {
                groups[g].eliteInstruct("words", 0.5f);
            }
        }
    }
    else if (intent == "focus_on_patterns") {
        for (int g = 0; g < groups.size(); g++) {
            bool is_elite_group = (g == 4) || (g >= 16 && g <= 21);
            if (is_elite_group) {
                groups[g].eliteInstruct("patterns", 0.4f);
            }
        }
    }
}

void LanguageModule::update(float dt) {
    static int dream_counter = 0;
    dream_counter++;
    
    // Каждые 5000 шагов — фаза сна (консолидация через EmergentMemory)
    if (dream_counter >= 5000 && dynamic_memory_) {
        dream_counter = 0;
        
        // Принудительная консолидация STM → LTM
        auto& emergent = neural_system.emergentMutable();
        auto [cons, disc] = emergent.memory.step(process_step_counter_);
        
        if (cons > 0 || disc > 0) {
            std::cout << "💤 Dream phase: consolidated " << cons 
                      << " memories, discarded " << disc << std::endl;
        }
    }
}

void LanguageModule::runAutoLearning(int steps, LearningOrchestrator* orchestrator) {
    if (learning_active_) return;
    
    learning_active_ = true;
    auto_learning_active_ = true;

    if (orchestrator) {
        learning_thread_ = std::thread([this, orchestrator, steps]() {
            std::cout << "Auto-learning thread started" << std::endl;
            
            auto start_time = std::chrono::steady_clock::now();
            const auto timeout = std::chrono::seconds(30);
            
            while (auto_learning_active_) {
                auto now = std::chrono::steady_clock::now();
                if (now - start_time > timeout) {
                    std::cout << "Auto-learning timeout reached" << std::endl;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

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

// ========== ОСНОВНОЙ МЕТОД (исправленный) ==========
std::string LanguageModule::process(const std::string& input, const std::string& user_name) {
    if (!dynamic_memory_) {
        return "Error: Dynamic memory not initialized";
    }
    
    static int step_counter = 0;
    step_counter++;
    
    last_user_input_ = input;
    dialogue_state_.last_question = input;
    
    // 1. АКТИВАЦИЯ через динамическую память
    auto meanings = dynamic_memory_->processText(input, user_name);
    
    // 2. Эволюция нейросистемы с эмерджентным управлением
    for (int evolve_step = 0; evolve_step < 10; evolve_step++) {
        neural_system.step(0.0f, step_counter + evolve_step);
    }
    
    // 3. ПОЛУЧАЕМ ОТВЕТ ИЗ НЕЙРОСИСТЕМЫ
    std::string response = readResponseFromNeuralSystem();
    
    // 4. Если нет ответа — пробуем контекст из LTM
    if (response.empty() || response == "...") {
        response = contextualResponse(input, step_counter);
    }
    
    if (response.empty() || response == "...") {
        response = "... I'm still learning. Please continue teaching me.";
    }
    
    std::cout << "🧠 Neural response: '" << response << "'" << std::endl;
    
    // 5. ОЦЕНКА ответа
    float correctness = evaluateResponse(input, response);
    float reward = 0.0f;
    
    if (correctness >= 0.8f) {
        reward = 1.0f;
        std::cout << "🎯 PERFECT ANSWER! Reward: 1.0" << std::endl;
    } 
    else if (correctness >= 0.6f) {
        reward = 0.7f;
        std::cout << "👍 GOOD ANSWER! Reward: 0.7" << std::endl;
    }
    else if (correctness >= 0.4f) {
        reward = 0.3f;
        std::cout << "🤔 PARTIAL ANSWER! Reward: 0.3" << std::endl;
    }
    else {
        reward = 0.05f;
        std::cout << "❌ WRONG ANSWER! Base reward: 0.05" << std::endl;
    }
    
    // Бонус за исследование (surprise из эмерджентного сигнала)
    float surprise = neural_system.lastSignal().surprise;
    reward = std::clamp(reward + 0.05f * surprise, 0.f, 1.f);
    
    // 6. Финальный шаг с реальной наградой
    neural_system.step(reward, step_counter + 10);
    
    // 7. Инструкции элиты при хорошем ответе
    if (correctness > 0.8f && web_trainer_) {
        std::cout << "🎯 Excellent response! Directing elite attention..." << std::endl;
        applyEliteInstruction("focus_on_words");
        applyEliteInstruction("focus_on_patterns");
    }
    
    // 8. Обновляем профиль пользователя
    dynamic_memory_->applyUserFeedback(user_name, reward, meanings, step_counter);
    giveFeedback(reward, true);
    
    // 9. Добавляем вопрос в WebTrainer
    bool is_question = input.find('?') != std::string::npos ||
                       input.find("what") != std::string::npos ||
                       input.find("how") != std::string::npos ||
                       input.find("who") != std::string::npos;
    if (web_trainer_ && is_question && input.size() > 3) {
        web_trainer_->addQuestion(input);
        std::cout << "📚 Added to WebTrainer: " << input << std::endl;
    }
    
    return response;
}

// ========== КОНТЕКСТНЫЙ ОТВЕТ ИЗ LTM ==========
std::string LanguageModule::contextualResponse(const std::string& /*input*/, int step) {
    if (!dynamic_memory_) return "";
    
    // Текущее состояние групп
    auto avgs_d = neural_system.getGroupAverages();
    std::vector<float> avgs_f(avgs_d.begin(), avgs_d.end());
    
    // Запрос к LTM
    auto matches = neural_system.emergent().memory.query(avgs_f, 3, step);
    if (matches.empty()) return "";
    
    const auto* best = matches[0];
    if (best->pattern.size() < 32) return "";
    
    // Поиск слова по тегу
    if (!best->tag.empty() && best->tag != "state") {
        return best->tag;
    }
    
    return "";
}

// ========== ЧТЕНИЕ ОТВЕТА ИЗ НЕЙРОСЕТИ (ИСПРАВЛЕНО) ==========
std::string LanguageModule::readResponseFromNeuralSystem() {
    if (!dynamic_memory_) return "";
    
    const auto& groups = neural_system.getGroups();
    
    // Вспомогательная лямбда для поиска лучшего нейрона
    auto bestNeuron = [&](int g, float thresh) -> int {
        int best_n = -1;
        float best_v = thresh;
        const auto& phi = groups[g].getPhi();
        for (int i = 0; i < 32; ++i) {
            if (phi[i] > best_v) {
                best_v = phi[i];
                best_n = i;
            }
        }
        return best_n;
    };
    
    // ПРИОРИТЕТ 1: Группа 3 (смыслы/паттерны)
    int pn = bestNeuron(3, 0.4f);
    if (pn >= 0) {
        const auto& all_words = dynamic_memory_->getAllWordRecords();
        for (const auto& [word, rec] : all_words) {
            const auto* neurons = dynamic_memory_->getWordNeurons(word);
            if (!neurons) continue;
            for (int n : *neurons) {
                if (n == pn) return word;
            }
        }
    }
    
    // ПРИОРИТЕТ 2: Группа 1 (слова)
    int wn = bestNeuron(1, 0.45f);
    if (wn >= 0) {
        const auto& all_words = dynamic_memory_->getAllWordRecords();
        for (const auto& [word, rec] : all_words) {
            const auto* neurons = dynamic_memory_->getWordNeurons(word);
            if (!neurons) continue;
            for (int n : *neurons) {
                if (n == wn) return word;
            }
        }
    }
    
    // ПРИОРИТЕТ 3: Элитные семантические группы (16-21)
    for (int g = 16; g <= 21; ++g) {
        int en = bestNeuron(g, 0.55f);
        if (en < 0) continue;
        const auto& all_words = dynamic_memory_->getAllWordRecords();
        for (const auto& [word, rec] : all_words) {
            const auto* neurons = dynamic_memory_->getWordNeurons(word);
            if (!neurons) continue;
            for (int n : *neurons) {
                if (n == en) return word;
            }
        }
    }
    
    return "";
}


float LanguageModule::evaluateResponse(const std::string& input, const std::string& response) {
    std::string input_lower = toLower(input);
    std::string response_lower = toLower(response);
    
    if (response.empty() || response == "...") {
        return 0.0f;
    }
    
    float score = 0.0f;
    int total_checks = 0;
    
    if (input_lower.find("your name") != std::string::npos) {
        total_checks++;
        if (response_lower.find("mary") != std::string::npos) {
            score += 1.0f;
        } else if (response_lower.find("i am") != std::string::npos) {
            score += 0.5f;
        }
    }
    
    if (input_lower.find("hello") != std::string::npos ||
        input_lower.find("hi") != std::string::npos) {
        total_checks++;
        if (response_lower.find("hello") != std::string::npos ||
            response_lower.find("hi") != std::string::npos) {
            score += 1.0f;
        } else if (!response_lower.empty()) {
            score += 0.4f;
        }
    }
    
    if (input_lower.find("how are you") != std::string::npos) {
        total_checks++;
        if (response_lower.find("good") != std::string::npos ||
            response_lower.find("fine") != std::string::npos ||
            response_lower.find("well") != std::string::npos) {
            score += 1.0f;
        } else if (response_lower.find("i am") != std::string::npos) {
            score += 0.6f;
        }
    }
    
    if (total_checks == 0 && !response.empty() && response != "...") {
        return 0.5f;
    }
    
    return total_checks > 0 ? score / total_checks : 0.0f;
}

std::string LanguageModule::formatResponse(const std::string& base_word) {
    return base_word;
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
        
        ss << "\n--- Emergent Memory Stats ---\n";
        const auto& emergent = neural_system.emergent();
        ss << "STM size: " << emergent.memory.stmSize() << "\n";
        ss << "LTM size: " << emergent.memory.ltmSize() << "\n";
        ss << "Surprise: " << neural_system.lastSignal().surprise << "\n";
        ss << "Quality: " << neural_system.lastSignal().quality << "\n";
    }
    
    ss << "===========================\n";
    return ss.str();
}

void LanguageModule::giveFeedback(float rating, bool autoFeedback) {
    addExternalFeedback(rating);
    
    if (!autoFeedback) {
        std::cout << "User feedback: " << rating << std::endl;
    }
    
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

void LanguageModule::addToRecentMeanings(uint32_t meaning_id) {
    recent_meanings_.push_back(meaning_id);
    if (recent_meanings_.size() > MAX_RECENT_MEANINGS) {
        recent_meanings_.pop_front();
    }
}