#include "LanguageModule.hpp"
#include "../../core/Config.hpp"
#include "../../core/MemoryManager.hpp"
#include "../learning/CuriosityDriver.hpp"
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
                             SemanticGraphDatabase* graph)
    : neural_system(system)
    , immutable_core(core)
    , auth(auth)
    , semantic_graph_(graph)
    , rng_(std::random_device{}())
    , dialogue_state_() {
    
    semantic_manager = std::make_unique<SemanticManager>(system);
    thought_predictor = std::make_unique<ThoughtPredictor>(*semantic_manager, system);
    
    system_mode_ = "personal";
    default_user_ = "user";
    
    std::cout << "LanguageModule initialized (default mode: personal)" << std::endl;
}

// Деструктор
LanguageModule::~LanguageModule() {
    stopAutoLearning(); // гарантируем завершение потока
}

bool LanguageModule::initialize(const Config& config) {
    std::cout << "LanguageModule initialized from config" << std::endl;
    return true;
}

void LanguageModule::shutdown() {
    std::cout << "LanguageModule shutting down" << std::endl;
}

void LanguageModule::update(float dt) {
    // Ничего не делаем в каждом тике - мышление происходит в process
}

void LanguageModule::saveState(MemoryManager& memory) {
    std::vector<float> feedbackData = {
        external_feedback_sum_,
        static_cast<float>(external_feedback_count_)
    };
    
    std::map<std::string, std::string> metadata;
    metadata["module"] = "LanguageModule";
    
    memory.store("LanguageModule", "feedback", feedbackData, 0.5f, metadata);
}

void LanguageModule::loadState(MemoryManager& memory) {
    auto records = memory.getRecords("LanguageModule");
    for (const auto& record : records) {
        if (record.type == "feedback" && record.data.size() >= 2) {
            external_feedback_sum_ = record.data[0];
            external_feedback_count_ = static_cast<int>(record.data[1]);
        }
    }
    std::cout << "LanguageModule state loaded" << std::endl;
}

// Методы для совместимости с UI
void LanguageModule::runAutoLearning(int steps, EffectiveLearning* learning) {
    if (learning_active_) return;
    
    learning_active_ = true;
    auto_learning_active_ = true;
    
    if (learning) {
        learning_thread_ = std::thread([this, learning, steps]() {
            std::cout << "Auto-learning thread started" << std::endl;
            
            // Ждем немного, чтобы симуляция точно запустилась
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            learning->runSemanticTraining(steps / 3600);
            
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
        learning_thread_.join(); // правильно ждем завершения
    }
}

// ========== ОСНОВНОЙ МЕТОД ==========
std::string LanguageModule::process(const std::string& input, const std::string& user_name) {
    
    std::string effective_user = (system_mode_ == "personal") ? default_user_ : user_name;
    
    if (!auth.authorize(effective_user)) {
        return "Access denied";
    }

    // Обработка специальных команд
    std::string special_output;
    if (isSpecialCommand(input) && handleSpecialCommand(input, special_output)) {
        return special_output;
    }
    
    // Преобразуем в смыслы
    auto input_meanings = textToMeanings(input);
    
    // ИСПРАВЛЕНИЕ: Определяем эмоциональный тон входа
    EmotionalTone input_tone = EmotionalTone::NEUTRAL;
    if (semantic_graph_) {
        float emotional_sum = 0.0f;
        int count = 0;
        for (uint32_t mid : input_meanings) {
            auto node = semantic_graph_->getNode(mid);
            if (node) {
                emotional_sum += static_cast<float>(node->emotional_tone);
                count++;
            }
        }
        if (count > 0) {
            float avg_tone = emotional_sum / count;
            // явно добавлены быстро и поверх системы.
            // Эмоции через среднее значение
            // 0.5f Это очень простая эвристика.
            if (avg_tone > 0.5f) input_tone = EmotionalTone::VERY_POSITIVE;
            else if (avg_tone > 0.1f) input_tone = EmotionalTone::POSITIVE;
            else if (avg_tone < -0.5f) input_tone = EmotionalTone::VERY_NEGATIVE;
            else if (avg_tone < -0.1f) input_tone = EmotionalTone::NEGATIVE;
        }
    }
    
    // Сохраняем в историю
    for (uint32_t m : input_meanings) {
        recent_meanings_.push_back(m);
        if (recent_meanings_.size() > MAX_RECENT_MEANINGS) {
            recent_meanings_.pop_front();
        }
    }
    
    // Если смыслов нет - система не понимает
    // Стало:
    if (input_meanings.empty()) {
        if (curiosity_driver_) {
            return "I don't understand. " + curiosity_driver_->getNextQuestion();
        }
        // Генерируем ответ из семантического графа
        auto confused_id = semantic_graph_->getNodeId("confused");
        if (confused_id != 0) {
            return semantic_manager->meaningsToText({confused_id});
        }
        return "I'm not sure I understand.";
    }

    // Проверка команд только в enterprise режиме
    if (system_mode_ == "enterprise") {
        std::string lower_input = input;
        std::transform(lower_input.begin(), lower_input.end(), lower_input.begin(), ::tolower);
        
        struct CommandRule {
            std::string keyword;
            AccessLevel required_level;
            std::string action_name;
        };
        
        std::vector<CommandRule> commands = {
            {"shutdown", AccessLevel::ADMIN, "system_shutdown"},
            {"restart", AccessLevel::ADMIN, "system_restart"},
            {"status", AccessLevel::EMPLOYEE, "system_status"},
            {"configure", AccessLevel::MASTER, "system_configure"},
            {"move", AccessLevel::OPERATOR, "motor_move"},
            {"stop", AccessLevel::OPERATOR, "motor_stop"},
            {"photo", AccessLevel::OPERATOR, "camera_capture"},
            {"record", AccessLevel::OPERATOR, "microphone_record"}
        };
        
        for (const auto& cmd : commands) {
            // Это примитивный парсинг.
            if (lower_input.find(cmd.keyword) != std::string::npos) {
                if (!canExecuteCommand(effective_user, cmd.keyword, cmd.required_level)) {
                    // Должно быть (исправлено):
                    std::string result = "Access denied. This command requires ";
                    result += accessLevelToString(cmd.required_level);
                    result += " level.";
                    return result;
                }
                break;
            }
        }
    } else {
        // Personal mode: просто логируем
        std::cout << "Personal mode: commands are informational only" << std::endl;
    }
    
    // Думаем (предсказываем следующие смыслы) с учётом эмоционального контекста
    auto output_meanings = thought_predictor->think(input_meanings);

    // ИСПРАВЛЕНИЕ: Добавляем эмоциональный отклик
    // ИСПРАВЛЕНИЕ: Добавить проверку на semantic_graph_
    if (input_tone != EmotionalTone::NEUTRAL && semantic_graph_) {  // проверка!
        auto emotion_nodes = semantic_graph_->getNodesByEmotion(input_tone);
        if (!emotion_nodes.empty()) {
            output_meanings.push_back(emotion_nodes[0]);
        }
    }
    
    // Проверяем самооценку
    auto self_assessment = thought_predictor->getSelfAssessment();
    if (!self_assessment.empty()) {
        output_meanings.insert(output_meanings.end(), 
                              self_assessment.begin(), 
                              self_assessment.end());
    }
    
    // Проверяем уверенность
    float confidence = calculateConfidence(output_meanings);
    
    // Если низкая уверенность и есть драйвер любопытства - задаем вопрос
    if (confidence < 0.5f && curiosity_driver_) {
        auto thinking_id = semantic_graph_->getNodeId("think");
        auto question = curiosity_driver_->getNextQuestion();
        if (thinking_id != 0) {
            return semantic_manager->meaningsToText({thinking_id}) + " " + question;
        }
        return "Let me ask: " + question;
    }
    
    // Преобразуем в ответ
    std::string response = meaningsToText(output_meanings);
    
    // ИСПРАВЛЕНИЕ: Эмоциональные эмодзи на основе тона
    if (emotional_responses_ && !response.empty()) {
        if (input_tone == EmotionalTone::VERY_POSITIVE) {
            response += " (+2)";
        } else if (input_tone == EmotionalTone::POSITIVE) {
            response += " (+1)";
        } else if (input_tone == EmotionalTone::VERY_NEGATIVE) {
            response += " (-2)";
        } else if (input_tone == EmotionalTone::NEGATIVE) {
            response += " (-1)";
        } else if (confidence > 0.8f) {
            response += " (=)";
        } else if (confidence < 0.3f) {
            response += " (...)";
        }
    }
    
    std::cout << "Response: \"" << response << "\" (confidence: " << confidence 
              << ", tone: " << emotionalToneToString(input_tone) << ")" << std::endl;
    return response;
}

bool LanguageModule::canExecuteCommand(const std::string& user_name, 
                                      const std::string& command,
                                      AccessLevel required_level) {
    
    // В personal mode все команды разрешены
    if (system_mode_ == "personal") {
        std::cout << "Personal mode: command '" << command << "' auto-approved" << std::endl;
        return true;
    }
    
    // Enterprise mode: полная проверка
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

// ========== КОНВЕРТАЦИЯ ТЕКСТ -> СМЫСЛЫ ==========
std::vector<uint32_t> LanguageModule::textToMeanings(const std::string& text) {
    auto words = split(text);
    std::cout << "Processing text: '" << text << "', words: ";
    for (const auto& w : words) std::cout << w << " ";
    std::cout << std::endl;
    
    std::vector<float> input_signal(32, 0.0f);
    
    for (const auto& word : words) {
        uint32_t word_hash = std::hash<std::string>{}(word);
        int neuron_idx = word_hash % 32;
        input_signal[neuron_idx] = 1.0f;
        std::cout << "  Word '" << word << "' -> neuron " << neuron_idx << std::endl;
    }
    
    // ПОКАЗЫВАЕМ ВСЕ АКТИВНЫЕ НЕЙРОНЫ
    std::cout << "Active neurons: ";
    for (int i = 0; i < 32; i++) {
        if (input_signal[i] > 0) {
            std::cout << i << " ";
        }
    }
    std::cout << std::endl;
    
    // Подаем сигнал в группу 0
    auto& group0 = neural_system.getGroups()[0].getPhiNonConst();
    for (int i = 0; i < 32; i++) {
        group0[i] = input_signal[i];
    }

    // После подачи сигнала
    std::cout << "Group0 after input: ";
    const auto& group0_check = neural_system.getGroups()[0].getPhi();
    for (int i = 0; i < 5; i++) {
        std::cout << group0_check[i] << " ";
    }
    std::cout << std::endl;
    
     // Даем системе ПОДОЛЬШЕ подумать (50 шагов вместо 10)
    for (int i = 0; i < 50; i++) {
        neural_system.step(0.0f, i);
    }
    
    auto result = semantic_manager->extractMeaningsFromSystem();
    
    std::cout << "=== ACTIVATION CHECK ===" << std::endl;
    const auto& group16 = neural_system.getGroups()[16].getPhi();
    std::cout << "Group16 pattern: ";
    for (int i = 0; i < 5; i++) {
        std::cout << std::fixed << std::setprecision(2) << group16[i] << " ";
    }
    std::cout << std::endl;
    
    return result;
}

// ========== КОНВЕРТАЦИЯ СМЫСЛЫ -> ТЕКСТ ==========
std::string LanguageModule::meaningsToText(const std::vector<uint32_t>& meanings) {
   
    if (meanings.empty()) {
        return (system_mode_ == "personal") ? "Hmm, let me think..." : "I'm thinking...";
    }

        // Если слишком много смыслов, берем только первые 3
    std::vector<uint32_t> top_meanings = meanings;
    if (top_meanings.size() > 3) {
        top_meanings.resize(3);
    }
    
    std::string response = semantic_manager->meaningsToText(meanings);
    
    // ИСПРАВЛЕНИЕ: Добавить проверку
    if (semantic_graph_ && meanings.size() >= 2) {  // проверка!
        for (uint32_t mid : meanings) {
            auto node = semantic_graph_->getNode(mid);
            if (node && !node->frame_roles.empty()) {
                std::string frame_response = buildFrameResponse(mid, meanings);
                if (!frame_response.empty()) {
                    response = frame_response;
                    break;
                }
            }
        }
    }
    
    if (system_mode_ == "personal" && !response.empty()) {
        if (response.back() != '.' && response.back() != '!' && response.back() != '?') {
            response += ".";
        }
    }
    
    return response;
}

// НОВЫЙ ВСПОМОГАТЕЛЬНЫЙ МЕТОД
// ЭТО ОСТАВИТЬ (метод используется в meaningsToText)
std::string LanguageModule::buildFrameResponse(uint32_t frame_id, 
                                              const std::vector<uint32_t>& meanings) {
    auto frame_node = semantic_graph_->getNode(frame_id);
    if (!frame_node) return "";
    
    std::map<std::string, std::string> role_values;
    
    for (uint32_t mid : meanings) {
        if (mid == frame_id) continue;
        
        auto node = semantic_graph_->getNode(mid);
        if (!node) continue;
        
        for (const auto& [role, role_id] : frame_node->frame_roles) {
            if (role_id == mid) {
                role_values[role] = node->canonical_form;
                break;
            }
        }
    }
    // Хардкод правил
    // Это явный rule-based patch.
    if (frame_id == semantic_graph_->getNodeId("capture_frame")) {
        return "The " + role_values["agent"] + " uses " + 
               role_values["instrument"] + " to capture " + 
               role_values["result"];
    }
    else if (frame_id == semantic_graph_->getNodeId("affect_analysis")) {
        return "Detecting " + role_values["result"] + " from " + 
               role_values["instrument"] + " data";
    }
    
    return "";
}

// ========== ВЫЧИСЛЕНИЕ УВЕРЕННОСТИ ==========
float LanguageModule::calculateConfidence(const std::vector<uint32_t>& meanings) {
    if (meanings.empty()) return 0.0f;
    
    auto features = neural_system.getFeatures();
    float total_activation = 0.0f;
    int count = 0;
    
    for (uint32_t id : meanings) {
        auto node = semantic_manager->getGranule(id);
        if (!node) continue;
        
        int group = id % 32;
        float activation = 0.0f;
        float norm_features = 0.0f;
        float norm_sig = 0.0f;
        
        for (int i = 0; i < 32; i++) {
            float f = features[group * 32 + i];
            float s = node->getSignature()[i];
            activation += f * s;
            norm_features += f * f;
            norm_sig += s * s;
        }
        
        if (norm_features > 0 && norm_sig > 0) {
            activation /= (std::sqrt(norm_features) * std::sqrt(norm_sig));
            total_activation += activation;
            count++;
        }
    }
    
    return count > 0 ? total_activation / count : 0.0f;
}

// ========== ОБРАТНАЯ СВЯЗЬ ==========
void LanguageModule::giveFeedback(float rating, bool autoFeedback) {
    addExternalFeedback(rating);
    
    if (!autoFeedback) {
        std::cout << "User feedback: " << rating << std::endl;
    }
}

// ========== ОЦЕНКА ПРИСПОСОБЛЕННОСТИ ==========
double LanguageModule::getLanguageFitness() const {
    float externalScore = getExternalFeedbackAvg();
    float diversityScore = 0.5f;
    
    // Если фидбек был только от нескольких пользователей
    if (external_feedback_count_ < 10) {
        externalScore = 0.5f;  // не доверяем малой статистике
    }
    
    // Защита от аномалий
    if (externalScore > 0.95f) {
        externalScore = 0.6f;
    }
    
    return 0.7f * externalScore + 0.3f * diversityScore;
}
// ========== УТИЛИТЫ ==========
std::vector<std::string> LanguageModule::split(const std::string& text) {
    std::vector<std::string> tokens;
    std::stringstream ss(text);
    std::string token;
    while (std::getline(ss, token, ' ')) {
        if (!token.empty()) {
            tokens.push_back(toLower(token));
        }
    }
    return tokens;
}

std::string LanguageModule::toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), ::tolower);
    return text;
}

// New models
bool LanguageModule::isSpecialCommand(const std::string& input) {
    return input == "?" || 
           input == "help" || 
           input == "status" || 
           input == "learn" ||
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
        }
    }
    else if (input == "help") {
        // Генерируем help из семантического графа
        std::stringstream ss;
        auto help_id = semantic_graph_->getNodeId("help");
        auto question_id = semantic_graph_->getNodeId("question");
        auto learn_id = semantic_graph_->getNodeId("learn");
        
        ss << "Available commands:\n";
        if (help_id) ss << "  ? - " << semantic_manager->meaningsToText({help_id}) << "\n";
        if (question_id) ss << "  answer: <text> - " << semantic_manager->meaningsToText({question_id}) << "\n";
        if (learn_id) ss << "  learn - " << semantic_manager->meaningsToText({learn_id}) << "\n";
        
        output = ss.str();
        return true;
    }

    else if (input == "status") {
        output = getLearningStatus();
        return true;
    }
    else if (input == "learn") {
        output = "📚 Learning mode activated. I'll be more curious!";
        // Можно увеличить параметры любопытства
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
    if (!curiosity_driver_) {
        return "I'm not curious right now. Something's wrong with my curiosity driver.";
    }
    
    std::string question = curiosity_driver_->getNextQuestion();
    dialogue_state_.last_question = question;
    dialogue_state_.expecting_answer = true;
    dialogue_state_.question_time = std::chrono::system_clock::now();
    
    return question;
}

std::string LanguageModule::processAnswer(const std::string& answer) {
    if (!curiosity_driver_) {
        dialogue_state_.expecting_answer = false;
        return "I can't learn right now. Curiosity driver not available.";
    }
    
    if (!dialogue_state_.expecting_answer) {
        return "I wasn't expecting an answer.";
    }
    
    // Передаем ответ в драйвер любопытства
    curiosity_driver_->processAnswer(dialogue_state_.last_question, answer);
    
    // Вычисляем время ответа (для статистики)
    auto now = std::chrono::system_clock::now();
    auto response_time = std::chrono::duration_cast<std::chrono::seconds>(
        now - dialogue_state_.question_time).count();
    
    // Сбрасываем состояние
    dialogue_state_.expecting_answer = false;
    
    // Стало:
    auto thank_id = semantic_graph_->getNodeId("thank");
    auto learn_id = semantic_graph_->getNodeId("learn");
    if (thank_id != 0 && learn_id != 0) {
        return semantic_manager->meaningsToText({thank_id, learn_id}) + 
            " (Response time: " + std::to_string(response_time) + "s)";
    }
    return "Thanks for teaching me!";
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
    
    return ss.str();
}