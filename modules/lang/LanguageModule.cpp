// modules/lang/LanguageModule.cpp
#include "LanguageModule.hpp"
#include "../../core/Config.hpp"
#include "../../core/MemoryManager.hpp"
#include "../learning/CuriosityDriver.hpp"
#include "LearningOrchestrator.hpp" 
#include "../../core/AccessLevel.hpp"
#include "../../data/PersonnelData.hpp"
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
    , dialogue_state_()
    , orchestrator_(nullptr) 
     {

        // ===== НОВОЕ: ЗАГРУЖАЕМ СОХРАНЕННЫЙ ГРАФ =====
    if (semantic_graph_) {
        // Пробуем загрузить сохраненный граф
        semantic_graph_->loadFromFile("semantic_graph.bin");
        std::cout << "Loaded semantic graph from previous session" << std::endl;
    }
    
    // ===== ИСПРАВЛЕНО: передаём граф в SemanticManager =====
    semantic_manager = std::make_unique<SemanticManager>(system, graph);
    thought_predictor = std::make_unique<ThoughtPredictor>(*semantic_manager, system);
    
    system_mode_ = "personal";
    default_user_ = "user";
    process_step_counter_ = 0;
    
    std::cout << "LanguageModule initialized (default mode: personal)" << std::endl;
    
    if (semantic_graph_) {
        initializeWordDictionary();
    } else {
        std::cerr << "WARNING: semantic_graph_ is null in LanguageModule constructor!" << std::endl;
    }
}

std::shared_ptr<CuriosityDriver> LanguageModule::createCuriosityDriver() {
    if (!semantic_graph_) {
        std::cerr << "ERROR: Cannot create CuriosityDriver - semantic_graph_ is null!" << std::endl;
        return nullptr;
    }
    return std::make_shared<CuriosityDriver>(neural_system, *this, *semantic_graph_);
}

// Деструктор
LanguageModule::~LanguageModule() {
    stopAutoLearning();
}

bool LanguageModule::initialize(const Config& config) {
    std::cout << "LanguageModule initialized from config" << std::endl;
    return true;
}

void LanguageModule::shutdown() {
    std::cout << "LanguageModule shutting down" << std::endl;
}

void LanguageModule::update(float dt) {
    static int cleanup_counter = 0;
    cleanup_counter++;
    static int dream_counter = 0;
    dream_counter++;
    
    // Каждые 1000 шагов очищаем старые концепты
    if (cleanup_counter >= 1000 && semantic_graph_) {
        cleanup_counter = 0;
        semantic_graph_->pruneAgedConcepts(process_step_counter_, 5000, 0.15f);
    }
        // Каждые 5000 шагов — сессия "мечтательности"
    if (dream_counter > 5050 && orchestrator_) {
        dream_counter = 0;
        std::cout << "Dreaming session..." << std::endl;
        orchestrator_->runExploratoryLearning(200);
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

void LanguageModule::runAutoLearning(int steps, LearningOrchestrator* orchestrator) {
    if (learning_active_) return;
    
    learning_active_ = true;
    auto_learning_active_ = true;

    if (orchestrator) {
        learning_thread_ = std::thread([this, orchestrator, steps]() {
            std::cout << "Auto-learning thread started" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            orchestrator->runTraining(steps / 3600);
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
    if (input.empty()) {
        return "";  // Пустой ввод → пустой ответ (ничего не говорим)
    }
    
    std::string effective_user = (system_mode_ == "personal") ? default_user_ : user_name;
    
    if (!auth.authorize(effective_user)) {
        return "";  // Нет доступа → молчим, никаких "Access denied"
    }

    // Обработка специальных команд (только если есть драйвер)
    std::string special_output;
    if (isSpecialCommand(input) && handleSpecialCommand(input, special_output)) {
        return special_output;  // команды оставляем, они нужны для управления
    }
    
    if (!semantic_graph_) {
        std::cerr << "ERROR: semantic_graph_ is null" << std::endl;
        return "";  // Ошибка → молчим
    }
    
    // Преобразуем в смыслы
    auto input_meanings = textToMeanings(input);

    for (uint32_t mid : input_meanings) {
        semantic_graph_->markConceptUsed(mid, process_step_counter_);
    }
    
    // Определяем эмоциональный тон входа (только для внутреннего использования)
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
    
    // ===== ВАЖНО: если смыслов нет, НЕ используем шаблоны =====
    if (input_meanings.empty()) {
        return "...";  // ← молчание
    }

    // Проверка команд (оставляем, это не шаблоны, а системные действия)
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
            if (lower_input.find(cmd.keyword) != std::string::npos) {
                if (!canExecuteCommand(effective_user, cmd.keyword, cmd.required_level)) {
                    return "";  // Нет прав — молчим, без "Access denied"
                }
                break;
            }
        }
    }
    
    // Генерируем ответ через нейросеть
    std::vector<uint32_t> output_meanings;
    
    if (thought_predictor) {
        output_meanings = thought_predictor->think(input_meanings);
        
        // Даём системе время на стабилизацию
        for (int i = 0; i < 5; i++) {
            neural_system.step(0.0f, process_step_counter_++);
        }
    }
    
    // Если нет выходных смыслов, извлекаем из текущего состояния
    if (output_meanings.empty() && semantic_manager) {
        output_meanings = semantic_manager->extractMeaningPath(5);
    }
    
    // Добавляем эмоциональный отклик (только если есть соответствующие смыслы)
    if (input_tone != EmotionalTone::NEUTRAL && semantic_graph_) {
        auto emotion_nodes = semantic_graph_->getNodesByEmotion(input_tone);
        if (!emotion_nodes.empty()) {
            // Проверяем, не дублируем ли уже существующие смыслы
            bool already_has = false;
            for (uint32_t existing : output_meanings) {
                if (existing == emotion_nodes[0]) {
                    already_has = true;
                    break;
                }
            }
            if (!already_has) {
                output_meanings.push_back(emotion_nodes[0]);
            }
        }
    }
    
    // Эмоциональные маркеры УБРАНЫ — никаких (+2), (=), (...)
    // Только чистый ответ
    
    std::string response;
    if (semantic_manager && !output_meanings.empty()) {
        response = semantic_manager->meaningsToText(output_meanings);
        
        // ДОБАВИТЬ ОТЛАДКУ
        std::cout << "output_meanings: ";
        for (uint32_t id : output_meanings) {
            std::cout << id << " ";
        }
        std::cout << std::endl;
        std::cout << "response before: \"" << response << "\"" << std::endl;
    }
    
    if (response.empty()) {
        std::cout << "Empty response generated. output_meanings size: " << output_meanings.size() << std::endl;
        return "";
    }
    
    std::cout << "Response: \"" << response << "\"" << std::endl;
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

void LanguageModule::initializeWordDictionary() {
    std::lock_guard<std::mutex> lock(dict_mutex_);
    
    if (dictionary_initialized_) return;
    if (!semantic_graph_) return;
    if (!semantic_manager) return;
    
    std::cout << "Initializing word dictionary from semantic graph..." << std::endl;
    
    int granules_created = 0;
    
    // Проходим по всем узлам графа
    for (uint32_t id = 1; id <= 614; id++) {
        auto node = semantic_graph_->getNode(id);
        if (!node || node->canonical_form.empty()) continue;
        
        std::string word = toLower(node->canonical_form);
        word_to_meaning[word].push_back(id);
        
        // Добавляем алиасы
        for (const auto& alias : node->aliases) {
            std::string alias_lower = toLower(alias);
            if (!alias_lower.empty() && alias_lower != word) {
                word_to_meaning[alias_lower].push_back(id);
            }
        }
        
        // ===== КРИТИЧЕСКИ ВАЖНО: создаём гранулу для SemanticManager =====
        if (!semantic_manager->hasGranule(id)) {
            // Собираем все выражения для этого смысла
            std::vector<std::string> expressions;
            expressions.push_back(node->canonical_form);
            for (const auto& alias : node->aliases) {
                expressions.push_back(alias);
            }
            
            // Добавляем концепт в SemanticManager с правильным ID
            semantic_manager->addConcept(id, node->canonical_form, expressions, node->canonical_form);
            granules_created++;
            
            if (granules_created % 100 == 0) {
                std::cout << "  Created " << granules_created << " granules..." << std::endl;
            }
        }
    }
    
    dictionary_initialized_ = true;
    std::cout << "Dictionary initialized: " << word_to_meaning.size() << " unique words" << std::endl;
    std::cout << "Granules created: " << semantic_manager->getGranuleCount() << std::endl;
    
    // Устанавливаем отношения между смыслами
    semantic_manager->establishRelationships();
}

// ========== КОНВЕРТАЦИЯ ТЕКСТ -> СМЫСЛЫ ==========
std::vector<uint32_t> LanguageModule::textToMeanings(const std::string& text) {
    // ← ДОБАВЛЕНО: защита от nullptr
    if (!semantic_graph_) {
        std::cerr << "ERROR: semantic_graph_ is null in textToMeanings!" << std::endl;
        return {};
    }
    
    std::string cleanText = text;
    cleanText.erase(std::remove(cleanText.begin(), cleanText.end(), '\n'), cleanText.end());
    cleanText.erase(std::remove(cleanText.begin(), cleanText.end(), '\r'), cleanText.end());
    
    if (cleanText.empty()) {
        return {};
    }
    
    std::lock_guard<std::mutex> lock(neural_system.getMutex());
    
    auto words = split(cleanText);
    std::cout << "Processing text: '" << cleanText << "', words: ";
    for (const auto& w : words) std::cout << w << " ";
    std::cout << std::endl;
    
    // Проверяем, инициализирован ли словарь
    if (word_to_meaning.empty()) {
        std::cout << "  Word dictionary not initialized, initializing..." << std::endl;
        initializeWordDictionary();
    }
    
    // Сначала ищем в word_to_meaning
    std::vector<uint32_t> result;
    for (const auto& word : words) {
        auto it = word_to_meaning.find(word);
        if (it != word_to_meaning.end()) {
            for (uint32_t mid : it->second) {
                result.push_back(mid);
                std::cout << "  Word '" << word << "' -> meaning " << mid << std::endl;
            }
        } else {
            std::cout << "  Unknown word: '" << word << "'" << std::endl;
        }
    }
    
    // Если нашли слова в словаре, возвращаем их
    if (!result.empty()) {
        std::cout << "  Found " << result.size() << " meanings from dictionary" << std::endl;
        return result;
    }
    
    // Если не нашли, используем нейросеть
    std::cout << "  No dictionary matches, using neural network" << std::endl;
    
    std::vector<float> input_signal(32, 0.0f);
    
    for (const auto& word : words) {
        uint32_t word_hash = std::hash<std::string>{}(word);
        int neuron_idx = word_hash % 32;
        input_signal[neuron_idx] = 1.0f;
        std::cout << "  Word '" << word << "' -> neuron " << neuron_idx << std::endl;
    }
    
    std::cout << "Active neurons: ";
    for (int i = 0; i < 32; i++) {
        if (input_signal[i] > 0) {
            std::cout << i << " ";
        }
    }
    std::cout << std::endl;
    
    // Подаем сигнал в группу 0
    auto& groups = neural_system.getGroupsNonConst();
    if (groups.empty()) {
        std::cerr << "ERROR: No neural groups available!" << std::endl;
        return {};
    }
    
    auto& group0 = groups[0].getPhiNonConst();
    for (int i = 0; i < 32 && i < group0.size(); i++) {
        group0[i] = input_signal[i];
    }

    std::cout << "Group0 after input: ";
    const auto& group0_check = groups[0].getPhi();
    for (int i = 0; i < 5 && i < group0_check.size(); i++) {
        std::cout << group0_check[i] << " ";
    }
    std::cout << std::endl;
    
    static int local_step_counter = 0;
    
    int think_steps = 10;
    for (int i = 0; i < think_steps; i++) {
        neural_system.step(0.0f, local_step_counter++);
        
        if (i > 5 && neural_system.computeSystemEntropy() < 0.1f) {
            std::cout << "System collapse detected, stopping early" << std::endl;
            break;
        }
    }
    
    // ← ИСПРАВЛЕНО: проверка, что semantic_manager существует
    std::vector<uint32_t> meanings;
    if (semantic_manager) {
        meanings = semantic_manager->extractMeaningsFromSystem();
    } else {
        std::cerr << "ERROR: semantic_manager is null in textToMeanings!" << std::endl;
        return {};
    }
    
    std::cout << "=== ACTIVATION CHECK ===" << std::endl;
    if (groups.size() > 16) {
        const auto& group16 = groups[16].getPhi();
        std::cout << "Group16 pattern: ";
        for (int i = 0; i < 5 && i < group16.size(); i++) {
            std::cout << std::fixed << std::setprecision(2) << group16[i] << " ";
        }
        std::cout << std::endl;
    }
    
    return meanings;
}

// ========== КОНВЕРТАЦИЯ СМЫСЛЫ -> ТЕКСТ ==========
std::string LanguageModule::meaningsToText(const std::vector<uint32_t>& meanings) {
    if (meanings.empty()) {
        return "";  // ← УБРАТЬ все шаблоны
    }
    
    std::string response;
    if (semantic_manager) {
        response = semantic_manager->meaningsToText(meanings);
    }
    
    // ← УБРАТЬ добавление точки в конце
    return response;
}

std::string LanguageModule::buildFrameResponse(uint32_t frame_id, 
                                              const std::vector<uint32_t>& meanings) {
    // ← ДОБАВЛЕНО: проверка на nullptr
    if (!semantic_graph_) return "";
    
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
    
    // ← ИСПРАВЛЕНО: проверка на nullptr
    if (!semantic_manager) return 0.5f;
    
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

void LanguageModule::giveFeedback(float rating, bool autoFeedback) {
    addExternalFeedback(rating);
    
    if (!autoFeedback) {
        std::cout << "User feedback: " << rating << std::endl;
    }
    
    // ===== ИСПРАВЛЕНИЕ: Отрицательный фидбек =====
    if (rating < 0.3f && semantic_manager && semantic_graph_) {
        std::vector<uint32_t> recent_meanings_vec = getRecentMeanings();
        
        if (!recent_meanings_vec.empty()) {
            // Ослабляем только последнюю связь (не всю цепочку)
            if (recent_meanings_vec.size() >= 2) {
                uint32_t last = recent_meanings_vec.back();
                uint32_t prev = recent_meanings_vec[recent_meanings_vec.size() - 2];
                
                // Ослабляем только если это связь из словаря
                auto edges = semantic_graph_->getEdgesFrom(prev);
                bool has_edge = false;
                for (const auto& edge : edges) {
                    if (edge.to_id == last) {
                        has_edge = true;
                        break;
                    }
                }
                
                if (has_edge) {
                    // Ослабляем один раз, не многократно
                    semantic_graph_->addEdge(prev, last, 
                        SemanticEdge::Type::OPPOSITE_OF, -0.2f);
                    std::cout << "  Weakened ONE connection: " << prev << " -> " << last << std::endl;
                }
            }
            
            // НЕ ослабляем все связи подряд
        }
    }
    
    // НОВОЕ: передаём reward только в семантические группы
    if (rating > 0.5f) {
        // Положительный фидбек — усиливаем последнюю мысль
        auto last_meanings = getRecentMeanings();
        if (!last_meanings.empty()) {
            uint32_t last_id = last_meanings.back();
            int group_idx = 16 + (last_id % 6);
            
            // Усиливаем связь между семантическими группами
            for (int g = 16; g <= 21; g++) {
                if (g != group_idx) {
                    neural_system.strengthenInterConnection(group_idx, g, 0.05f);
                }
            }
            
            // Высокая награда для нейронов, представляющих этот смысл
            float semantic_reward = 1.0f;
            for (int g = 16; g <= 21; g++) {
                neural_system.getGroupsNonConst()[g].learnSTDP(semantic_reward, 0);
            }
        }
    } else {
        // Отрицательный фидбек — уменьшаем активность
        float negative_reward = -0.1f;
        for (int g = 16; g <= 21; g++) {
            neural_system.getGroupsNonConst()[g].learnSTDP(negative_reward, 0);
        }
    }

    // ===== УДАЛЯЕМ или сильно ограничиваем pattern_frequency =====
    /*
    static std::map<std::vector<uint32_t>, int> pattern_frequency;
    static std::map<std::vector<uint32_t>, int> pattern_feedback_count;
    */
    
    // ✅ НОВОЕ: вместо наказания паттернов - усиляем альтернативы
    if (rating < 0.3f && semantic_manager && semantic_graph_) {
        std::vector<uint32_t> recent_meanings_vec = getRecentMeanings();
        
        if (!recent_meanings_vec.empty() && recent_meanings_vec.size() >= 2) {
            // Вместо наказания паттерна - предлагаем альтернативу
            uint32_t last_meaning = recent_meanings_vec.back();
            
            // Ищем альтернативные связи для последнего смысла
            auto alternatives = semantic_graph_->getSimilarConcepts(last_meaning, 3);
            
            if (!alternatives.empty()) {
                uint32_t alternative = alternatives[0];
                std::cout << "  💡 Suggestion: try alternative concept " << alternative << std::endl;
                
                // Усиливаем альтернативную связь
                for (uint32_t prev : recent_meanings_vec) {
                    if (prev != last_meaning) {
                        semantic_graph_->addEdge(prev, alternative, 
                            SemanticEdge::Type::LEADS_TO, 0.3f);
                    }
                }
            }
        }
    }
    
    // ===== СОХРАНЯЕМ ГРАФ РЕЖЕ =====
    static int feedback_counter = 0;
    feedback_counter++;
    
    // Сохраняем только каждый 5-й фидбек (вместо каждого)
    if (feedback_counter % 5 == 0 && semantic_graph_) {
        semantic_graph_->saveToFile("semantic_graph.bin");
        std::cout << "Graph saved (feedback #" << feedback_counter << ")" << std::endl;
    }
    
    // ===== УМЕНЬШАЕМ DECAY =====
    if (feedback_counter % 50 == 0 && semantic_graph_) {  // реже, 50 вместо 10
        auto all_nodes = semantic_graph_->getAllNodeIds();
        int weakened = 0;
        
        for (uint32_t from : all_nodes) {
            auto edges = semantic_graph_->getEdgesFrom(from);
            for (const auto& edge : edges) {
                // Ослабляем только очень старые и очень слабые связи
                if (edge.weight < 0.1f && edge.weight > 0.0f) {
                    semantic_graph_->addEdge(from, edge.to_id, 
                        SemanticEdge::Type::OPPOSITE_OF, -0.005f);  // очень слабое ослабление
                    weakened++;
                }
            }
        }
        
        if (weakened > 0) {
            std::cout << "Applied gentle decay to " << weakened << " weak connections" << std::endl;
        }
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
                tokens.push_back(toLower(token));
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
        std::stringstream ss;
        // ← ИСПРАВЛЕНО: защита от nullptr
        if (semantic_graph_) {
            auto help_id = semantic_graph_->getNodeId("help");
            auto question_id = semantic_graph_->getNodeId("question");
            auto learn_id = semantic_graph_->getNodeId("learn");
            
            ss << "Available commands:\n";
            if (help_id && semantic_manager) ss << "  ? - " << semantic_manager->meaningsToText({help_id}) << "\n";
            if (question_id && semantic_manager) ss << "  answer: <text> - " << semantic_manager->meaningsToText({question_id}) << "\n";
            if (learn_id && semantic_manager) ss << "  learn - " << semantic_manager->meaningsToText({learn_id}) << "\n";
        } else {
            ss << "Available commands:\n";
            ss << "  ? - Ask a question\n";
            ss << "  answer: <text> - Answer a question\n";
            ss << "  learn - Start learning\n";
            ss << "  status - Show learning status\n";
        }
        output = ss.str();
        return true;
    }
    else if (input == "status") {
        output = getLearningStatus();
        return true;
    }
    else if (input == "learn") {
        // Запускаем исследовательское обучение
        if (orchestrator_) {
            output = "Starting exploratory learning mode...\n";
            orchestrator_->runExploratoryLearning(500);  // 500 шагов
            output += "Exploratory learning completed!";
        } else {
            output = "Learning mode activated. I'll be more curious!\n";
            output += "(Note: Orchestrator not available for deep learning)";
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
    
    curiosity_driver_->processAnswer(dialogue_state_.last_question, answer);
    
    auto now = std::chrono::system_clock::now();
    auto response_time = std::chrono::duration_cast<std::chrono::seconds>(
        now - dialogue_state_.question_time).count();
    
    dialogue_state_.expecting_answer = false;
    
    // ← ИСПРАВЛЕНО: защита от nullptr
    if (semantic_graph_ && semantic_manager) {
        auto thank_id = semantic_graph_->getNodeId("thank");
        auto learn_id = semantic_graph_->getNodeId("learn");
        if (thank_id != 0 && learn_id != 0) {
            return semantic_manager->meaningsToText({thank_id, learn_id}) + 
                " (Response time: " + std::to_string(response_time) + "s)";
        }
    }
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
    
    return ss.str();
}