// modules/learning/EffectiveLearning.cpp
#include "EffectiveLearning.hpp"
#include <cmath>
#include <iomanip>
#include <numeric>  // для std::accumulate
#include <thread>
#include "core/MemoryManager.hpp"

EffectiveLearning::EffectiveLearning(NeuralFieldSystem& ns, LanguageModule& lang, 
                                    SemanticManager& sm, MemoryManager& mem,
                                    PersonnelDatabase* db,  // указатель, а не ссылка!
                                    IAuthorization& auth,
                                    const DeviceDescriptor& dev,
                                    SemanticGraphDatabase& graph)
    : neural_system(ns)
    , language_module(lang)
    , semantic_manager(sm)
    , memory_manager(mem)
    , personnel_db(db)  // теперь указатель
    , auth(auth)
    , device_info(dev)
    , semantic_graph(graph)
    , current_learning_rate_(base_learning_rate_)
    , total_steps_(0)
    , epoch_(0)
    , training_active_(false) {
    
    loadMeaningCurriculum();
    std::cout << "EffectiveLearning (semantic) initialized" << std::endl;
}

void EffectiveLearning::loadMeaningCurriculum() {
    std::cout << "Loading semantic curriculum from graph..." << std::endl;
    
    auto examples = semantic_graph.generateTrainingExamples();
    for (const auto& ex : examples) {
        replay_buffer_.push_back(ex);
    }
    
    // Добавляем примеры с уровнями доступа
    // GUEST уровень (может только здороваться и спрашивать общее)
    addMeaningExample(
        {"hello", "hi", "hey"},
        {"greeting"},
        {},
        0.1f, "communication",
        AccessLevel::GUEST
    );
    
    addMeaningExample(
        {"who", "are", "you"},
        {"question", "system_identity"},
        {{43, 1}},
        0.2f, "device",
        AccessLevel::GUEST
    );
    
    // OPERATOR уровень (может управлять)
    if (device_info.actuators.motor) {
        addMeaningExample(
            {"move", "forward"},
            {"action_move"},
            {{81, 85}},
            0.4f, "action",
            AccessLevel::OPERATOR
        );
        
        addMeaningExample(
            {"stop", "halt"},
            {"action_stop"},
            {{83, 87}},
            0.3f, "action",
            AccessLevel::OPERATOR
        );
    }
    
    // EMPLOYEE уровень (доступ к данным)
    addMeaningExample(
        {"status", "report"},
        {"action_status"},
        {},
        0.4f, "system",
        AccessLevel::EMPLOYEE
    );
    
    // MANAGER уровень (управление сотрудниками)
    addMeaningExample(
        {"add", "user", "employee"},
        {"access_employee"},
        {},
        0.6f, "access",
        AccessLevel::MANAGER
    );
    
    // ADMIN уровень (системные команды)
    addMeaningExample(
        {"shutdown", "system"},
        {"action_stop"},
        {},
        0.7f, "system",
        AccessLevel::ADMIN
    );
    
    // MASTER уровень (всё можно)
    addMeaningExample(
        {"configure", "settings"},
        {"system_capability"},
        {},
        0.8f, "system",
        AccessLevel::MASTER
    );
    
    std::cout << "   Loaded " << replay_buffer_.size() << " training examples from graph" << std::endl;
}

void EffectiveLearning::addMeaningExample(const std::vector<std::string>& input_words,
                                         const std::vector<std::string>& expected_meanings,
                                         const std::vector<std::pair<uint32_t, uint32_t>>& cause_effect,
                                         float difficulty, const std::string& category,
                                         AccessLevel required) {
    
    MeaningTrainingExample example;
    example.input_words = input_words;
    example.difficulty = difficulty;
    example.category = category;
    example.priority = difficulty;
    example.required_level = required;
    
    for (const auto& meaning_name : expected_meanings) {
        uint32_t id = semantic_manager.getMeaningId(meaning_name);
        if (id > 0) {
            example.expected_meanings.push_back(id);
        }
    }
    
    example.cause_effect = cause_effect;
    
    replay_buffer_.push_back(example);
}

// ИСПРАВЛЕНИЕ 7: Улучшить testResponse с проверкой авторизации
std::string EffectiveLearning::testResponse(const std::string& input, const std::string& user_name) {
    // Проверяем авторизацию
    if (!auth.authorize(user_name)) {
        return "Access denied: user " + user_name + " not authorized";
    }
    
    // Проверяем уровень доступа для команды
    AccessLevel level = auth.getCurrentLevel();
    
    // Получаем ответ с учетом уровня
    std::string response = language_module.process(input, user_name);
    
    // Если ответ пустой, генерируем на основе нейросети
    if (response.empty()) {
        auto meanings = language_module.textToMeanings(input);
        if (!meanings.empty()) {
            response = "I understand: ";
            for (uint32_t mid : meanings) {
                auto node = semantic_graph.getNode(mid);
                if (node) {
                    response += node->name + " ";
                }
            }
        } else {
            response = "I don't understand yet";
        }
    }
    
    return response;
}

float EffectiveLearning::evaluateResponse(const std::string& input, const std::string& expected, 
                                         const std::string& user_name) {
    std::string response = language_module.process(input, user_name);
    
    auto response_meanings = language_module.textToMeanings(response);
    auto expected_meanings = language_module.textToMeanings(expected);
    
    if (response_meanings.empty() || expected_meanings.empty()) {
        return 0.0f;
    }
    
    int matches = 0;
    for (uint32_t exp : expected_meanings) {
        for (uint32_t resp : response_meanings) {
            if (exp == resp) {
                matches++;
                break;
            }
        }
    }
    
    float accuracy = static_cast<float>(matches) / expected_meanings.size();
    return accuracy * 100.0f;
}

void EffectiveLearning::demonstrateTraining(const std::string& user_name) {
    std::cout << "\n=== DEMONSTRATING TRAINING PROGRESS ===\n";
    std::cout << "User: " << user_name << " (" 
              << accessLevelToString(auth.getCurrentLevel()) << ")\n";
    
    std::vector<std::pair<std::string, std::string>> tests;
    
    // Базовые тесты для всех
    tests.push_back({"hello", "Hello"});
    tests.push_back({"who are you", "I'm Mary"});
    
    // Тесты в зависимости от уровня
    if (auth.canPerform("status", AccessLevel::EMPLOYEE)) {
        tests.push_back({"status", "System status: operational"});
    }
    
    if (device_info.actuators.motor && 
        auth.canPerform("move", AccessLevel::OPERATOR)) {
        tests.push_back({"move forward", "Moving forward"});
    }
    
    if (auth.canPerform("shutdown", AccessLevel::ADMIN)) {
        tests.push_back({"shutdown", "Shutting down..."});
    }
    
    int correct = 0;
    int total = 0;
    
    for (const auto& [input, expected] : tests) {
        std::string response = testResponse(input, user_name);
        float score = evaluateResponse(input, expected, user_name);
        
        if (score > 50.0f) correct++;
        total++;
        
        std::cout << "Input: \"" << input << "\"\n";
        std::cout << "Response: \"" << response << "\"\n";
        std::cout << "Expected: \"" << expected << "\"\n";
        std::cout << "Score: " << std::fixed << std::setprecision(1) << score << "%\n";
        std::cout << "---\n";
    }
    
    std::cout << "Overall accuracy: " << (correct * 100 / total) << "% (" 
              << correct << "/" << total << ")\n";
}

void EffectiveLearning::testAccessLevels() {
    if (!personnel_db) {
        std::cout << "Personal mode: access levels not applicable" << std::endl;
        return;
    }
    
    std::cout << "\n=== TESTING ACCESS LEVELS ===\n";
    
    std::vector<std::pair<std::string, AccessLevel>> test_users = {
        {"guest", AccessLevel::GUEST},
        {"operator", AccessLevel::OPERATOR},
        {"employee", AccessLevel::EMPLOYEE},
        {"manager", AccessLevel::MANAGER},
        {"admin", AccessLevel::ADMIN},
        {"master", AccessLevel::MASTER}
    };
    
    for (const auto& [name, level] : test_users) {
        // ТЕПЕРЬ БЕЗОПАСНО: используем оператор ->
        auto& record = personnel_db->getOrCreatePerson(name);
        record.access_level = level;
        personnel_db->saveAll();
        
        std::cout << "\n--- Testing " << name << " (" 
                  << accessLevelToString(level) << ") ---\n";
        
        demonstrateTraining(name);
    }
}

void EffectiveLearning::testUnderstanding(const std::string& user_name) {
    auth.authorize(user_name);
    
    std::cout << "\n=== TESTING SEMANTIC UNDERSTANDING ===\n";
    std::cout << "User: " << user_name << " (" 
              << accessLevelToString(auth.getCurrentLevel()) << ")\n";
    
    std::vector<std::string> test_words = {
        "hello", "robot", "camera", "move", "status", "shutdown", "master"
    };
    
    for (const auto& word : test_words) {
        auto meanings = language_module.textToMeanings(word);
        
        std::cout << "Word: \"" << word << "\" activates meanings: ";
        for (uint32_t mid : meanings) {
            auto node = semantic_graph.getNode(mid);
            if (node) {
                std::cout << node->name << "(" << mid << ") ";
            } else {
                std::cout << mid << " ";
            }
        }
        std::cout << std::endl;
    }
}

// НОВЫЙ МЕТОД: интеллектуальное планирование обучения
void EffectiveLearning::intelligentLearningPlan() {
    std::cout << "\n=== INTELLIGENT LEARNING PLAN ===\n";

    // НОВОЕ: Сначала принудительно проходим все концепты
    std::cout << "\nPhase 0: Initial exposure to all concepts\n";
    forceLearnAllConcepts();
    
    // Фаза 1: Базовые смыслы (конкретные объекты)
    std::cout << "\nPhase 1: Concrete concepts (abstraction 1-3)\n";
    trainAbstractionLevel(1, 3, 2000);
    
    // Фаза 2: Действия и отношения
    std::cout << "\nPhase 2: Actions and relations (abstraction 3-5)\n";
    trainAbstractionLevel(3, 5, 3000);
    
    // Фаза 3: Причинно-следственные связи
    std::cout << "\nPhase 3: Cause-effect chains\n";
    trainCauseEffectAdvanced(2000);
    
    // Фаза 4: Абстрактные понятия (с эмоциями)
    std::cout << "\nPhase 4: Abstract concepts (abstraction 6-8)\n";
    trainAbstractionLevel(6, 8, 2000);
    
    // Фаза 5: Контекстное понимание
    std::cout << "\nPhase 5: Contextual understanding\n";
    trainContextAwareness(2000);
    
    // Фаза 6: Мета-обучение (как учиться)
    std::cout << "\nPhase 6: Meta-learning\n";
    trainMetaLearning(1000);
}

// НОВЫЙ МЕТОД: обучение по уровню абстракции
void EffectiveLearning::trainAbstractionLevel(int min_abs, int max_abs, int steps) {
    auto nodes = semantic_graph.getNodesByAbstraction(min_abs, max_abs);
    std::vector<MeaningTrainingExample> level_examples;
    
    for (const auto& ex : replay_buffer_) {
        for (uint32_t mid : ex.expected_meanings) {
            auto node = semantic_graph.getNode(mid);
            if (node && node->abstraction_level >= min_abs && 
                node->abstraction_level <= max_abs) {
                level_examples.push_back(ex);
                break;
            }
        }
    }
    
    if (level_examples.empty()) return;
    
    for (int i = 0; i < steps && training_active_; i++) {
        int idx = rng_() % level_examples.size(); // = 0 
        trainOnExampleAdvanced(level_examples[idx], true);
        
        if (i % 100 == 0) {
            std::cout << "\r  Progress: " << (i * 100 / steps) << "%" << std::flush;
        }
    }
    std::cout << std::endl;
}

// НОВЫЙ МЕТОД: улучшенное обучение на одном примере
void EffectiveLearning::trainOnExampleAdvanced(const MeaningTrainingExample& example, bool use_curriculum) {
    if (is_processing_) {
        std::cout << "  Recursive call prevented" << std::endl;
        return;
    }
    is_processing_ = true;

    try {
        if (!training_active_) return;
        if (example.input_words.empty() || example.expected_meanings.empty()) return;
        
        auto& groups = neural_system.getGroups();

        // ===== 0. ПРОВЕРКА НА NAN =====
        bool has_nan = false;
        for (size_t g = 0; g < groups.size(); g++) {
            const auto& phi = groups[g].getPhi();
            for (size_t i = 0; i < phi.size(); i++) {
                if (!std::isfinite(phi[i])) {
                    has_nan = true;
                    break;
                }
            }
        }

        if (has_nan) {
            std::cout << "⚠️ NaN detected in neural groups, resetting..." << std::endl;
            std::uniform_real_distribution<double> dist(0.3, 0.7);
            for (int g = 0; g < 32; g++) {
                auto& phi = groups[g].getPhiNonConst();
                for (int i = 0; i < 32; i++) {
                    phi[i] = dist(rng_);
                }
            }
            is_processing_ = false;
            return;
        }
        
        // ===== НОВОЕ: Проверяем, не обрабатывали ли мы этот концепт недавно =====
        uint32_t primary_concept = example.expected_meanings.empty() ? 0 : example.expected_meanings[0];
        
        // Проверяем, есть ли концепт в недавних
        bool recently_processed = false;
        for (uint32_t recent_id : recent_concepts_) {
            if (recent_id == primary_concept) {
                recently_processed = true;
                break;
            }
        }
        
        // Если концепт недавно обрабатывался, пропускаем с вероятностью 80%
        if (recently_processed && rng_() % 100 < 80) {
            std::cout << "  Skipping recently processed concept" << std::endl;
            is_processing_ = false;
            return;
        }
        
        // Добавляем концепт в недавние
        if (primary_concept > 0) {
            recent_concepts_.push_back(primary_concept);
            if (recent_concepts_.size() > MAX_RECENT_CONCEPTS) {
                recent_concepts_.pop_front();
            }
        }
        
        // ===== НОВОЕ: Принудительный циклический режим =====
        if (force_cycle_mode_ && !all_concept_ids_.empty()) {
            // Периодически переключаемся на следующий концепт в цикле
            if (total_steps_ % 50 == 0) {
                current_concept_index_ = (current_concept_index_ + 1) % all_concept_ids_.size();
                
                // Находим пример для этого концепта
                uint32_t target_id = all_concept_ids_[current_concept_index_];
                bool found = false;
                
                for (const auto& ex : replay_buffer_) {
                    for (uint32_t mid : ex.expected_meanings) {
                        if (mid == target_id) {
                            // Вместо текущего примера используем найденный
                            const_cast<MeaningTrainingExample&>(example) = ex;
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                }
            }
        }
        
        // ===== 1. ПРОВЕРЯЕМ, ГОТОВ ЛИ МОЗГ К ЭТОМУ ПРИМЕРУ =====
        float brain_readiness = 1.0f;

        // Проверяем, достаточно ли активированы базовые концепты
        for (uint32_t exp_id : example.expected_meanings) {
            auto node = semantic_graph.getNode(exp_id);
            if (!node) continue;
            
            // Если концепт слишком абстрактный, проверяем его родителей
            if (node->abstraction_level > 5) {
                float parent_activation = 0.0f;
                int parent_count = 0;
                
                auto edges = semantic_graph.getEdgesFrom(exp_id);
                for (const auto& edge : edges) {
                    if (edge.type == SemanticEdge::Type::IS_A || 
                        edge.type == SemanticEdge::Type::PART_OF) {
                        
                        parent_activation += getConceptMastery(edge.to_id);
                        parent_count++;
                    }
                }
                
                if (parent_count > 0) {
                    float avg_parent = parent_activation / parent_count;
                    
                    if (avg_parent < 0.3f) {
                        brain_readiness *= 0.5f;
                        std::cout << "Low parent mastery (" << avg_parent 
                                << ") for abstract concept " << exp_id << std::endl;
                    }
                }
            }
        }

        // ===== 2. ЕСЛИ МОЗГ НЕ ГОТОВ, ОТКЛАДЫВАЕМ ПРИМЕР =====
        if (brain_readiness < 0.6f) {
            // Уменьшаем приоритет, но не убираем полностью
            const_cast<MeaningTrainingExample&>(example).priority *= 0.8f;
            
            std::cout << "⏳ Deferring example - brain not ready (readiness: " 
                    << brain_readiness << ")" << std::endl;
            
            // Вознаграждаем за попытку, но слабо
            for (int g = 0; g < 32; g++) {
                groups[g].learnSTDP(0.1f, total_steps_);
            }
            total_steps_++;
            is_processing_ = false;
            return;
        }
        
        // ===== 3. ВЫБИРАЕМ СЛОВО С УЛУЧШЕННЫМ МЕХАНИЗМОМ =====
        
        // Отдаем предпочтение словам, которые реже использовались
        std::vector<std::pair<int, int>> weighted_indices;
        
        for (size_t i = 0; i < example.input_words.size(); i++) {
            const std::string& word = example.input_words[i];
            
            // Базовая частота использования
            int attempts = word_total_attempts_[word];
            
            // Вес: меньше попыток = выше вес
            int weight = std::max(1, 100 - attempts * 10);
            
            // БОНУС ЗА НОВИЗНУ
            if (attempts == 0) {
                weight *= 5;  // увеличен бонус для новых слов
            }
            
            // Штраф за частые неудачи
            if (word_fail_count_[word] > 2) {
                weight /= 4;  // увеличен штраф
            }
            
            // Бонус за недавний успех
            if (word_success_count_[word] > 0 && word_fail_count_[word] == 0) {
                weight *= 2;
            }
            
            // НОВОЕ: Штраф за слишком частое использование
            if (attempts > 20) {
                weight /= (attempts / 10);
            }
            
            weighted_indices.push_back({weight, i});
        }
        
        // Добавляем случайный элемент для исследования
        if (rng_() % 100 < 30) {  // 30% шанс выбрать случайное слово
            int random_idx = rng_() % example.input_words.size();
            // Увеличиваем вес случайного слова
            for (auto& [w, idx] : weighted_indices) {
                if (idx == random_idx) {
                    w *= 3;
                    break;
                }
            }
        }
        
        // Выбираем по весу
        int total_weight = 0;
        for (const auto& [w, idx] : weighted_indices) total_weight += w;
        
        int rand_weight = rng_() % total_weight;
        int cum_weight = 0;
        int word_idx = 0;
        for (const auto& [w, idx] : weighted_indices) {
            cum_weight += w;
            if (rand_weight < cum_weight) {
                word_idx = idx;
                break;
            }
        }

        const std::string& input_word = example.input_words[word_idx];
        word_total_attempts_[input_word]++;
        
        std::cout << "\nProcessing: '" << input_word << "' (attempts: " 
                << word_total_attempts_[input_word] << ", success: " 
                << word_success_count_[input_word] << ")" << std::endl;
        
        // ===== 4. СОЗДАЕМ ВХОДНОЙ СИГНАЛ С ШУМОМ =====
        std::vector<float> input_signal(32, 0.0f);
        uint32_t word_hash = std::hash<std::string>{}(input_word);
        int neuron_idx = word_hash % 32;
        
        // Основной сигнал + шум для исследования
        input_signal[neuron_idx] = 1.0f;
        
        // Добавляем случайный шум для исследования
        std::uniform_real_distribution<float> noise_dist(-0.1f, 0.1f);  // увеличен шум
        for (int i = 0; i < 32; i++) {
            if (i != neuron_idx) {
                input_signal[i] += noise_dist(rng_);
            }
        }
        
        // Нормализуем
        float max_val = *std::max_element(input_signal.begin(), input_signal.end());
        if (max_val > 1.0f) {
            for (auto& v : input_signal) v /= max_val;
        }
        
        // ===== 5. СОХРАНЯЕМ СОСТОЯНИЕ ДО ОБУЧЕНИЯ =====
        std::vector<std::vector<double>> before_state;
        for (int g = 16; g <= 21; g++) {
            before_state.push_back(groups[g].getPhi());
        }
        
        // Подаем сигнал
        for (int i = 0; i < 32; i++) {
            groups[0].getPhiNonConst()[i] = input_signal[i];
        }
        
        // Динамическое время мышления
        int think_steps = std::min(10 + (brain_readiness < 0.8f ? 15 : 5), 20);  // было 30
        for (int think_step = 0; think_step < think_steps; think_step++) {
            neural_system.step(0.0f, total_steps_ + think_step);
        }
        
        // ===== 6. АНАЛИЗИРУЕМ РЕЗУЛЬТАТ =====
        float activation_score = 0.0f;
        std::map<uint32_t, float> meaning_activations;
        
        for (uint32_t exp_id : example.expected_meanings) {
            int group_idx = 16 + (exp_id % 6);
            if (group_idx <= 21) {
                float group_activation = 0.0f;
                for (int i = 0; i < 32; i++) {
                    group_activation += groups[group_idx].getPhi()[i];
                }
                meaning_activations[exp_id] = group_activation;
                activation_score += group_activation;
            }
        }
        
        // Сравниваем с состоянием до обучения
        float improvement = 0.0f;
        for (size_t g = 0; g < before_state.size(); g++) {
            float before = 0.0f, after = 0.0f;
            for (int i = 0; i < 32; i++) {
                before += before_state[g][i];
                after += groups[16 + g].getPhi()[i];
            }
            improvement += (after - before);
        }
        
        // ===== 7. ВЫЧИСЛЯЕМ НАГРАДУ =====
        float reward;
        bool is_success = false;

        // Порог успеха зависит от сложности концепта
        float success_threshold = 15.0f;

        // Если система в стазисе, снижаем порог
        if (neural_system.computeSystemEntropy() < 1.0f) {
            success_threshold = 12.0f;
            std::cout << "Lowering success threshold due to low entropy" << std::endl;
        }
        
        // НОВОЕ: Сохраняем историю освоения концепта
        if (primary_concept > 0) {
            float mastery = getConceptMastery(primary_concept);
            concept_mastery_history_[primary_concept] = mastery;
        }
        
        if (activation_score > success_threshold) {
            reward = 2.0f;
            is_success = true;
            word_success_count_[input_word]++;
            word_fail_count_[input_word] = 0;
            recent_successful_words_.push_back(input_word);
            if (recent_successful_words_.size() > 10) {
                recent_successful_words_.pop_front();
            }
            
            std::cout << "SUCCESS! Score: " << activation_score << std::endl;
            
            // НОВОЕ: Если концепт освоен, уменьшаем его приоритет сильнее
            if (primary_concept > 0 && getConceptMastery(primary_concept) > MASTERY_THRESHOLD) {
                const_cast<MeaningTrainingExample&>(example).priority *= 0.3f;
                std::cout << "  Concept mastered, priority reduced" << std::endl;
            }
        }
        else if (improvement > 1.0f) {
            reward = 0.8f;
            std::cout << "Progress! Improvement: " << improvement << std::endl;
        }
        else {
            reward = -0.1f;
            word_fail_count_[input_word]++;
            std::cout << "No improvement" << std::endl;
        }
        
        // ===== 8. ПРИМЕНЯЕМ НАГРАДУ =====
        for (size_t g = 16; g <= 21 && g < groups.size(); g++) {
            groups[g].learnSTDP(reward, total_steps_);
        }
        
        // ===== 9. ОБНОВЛЯЕМ ПРИОРИТЕТ ПРИМЕРА =====
        float& prio = const_cast<MeaningTrainingExample&>(example).priority;
        
        if (is_success) {
            // Успешные примеры - уменьшаем приоритет
            prio *= 0.7f;  // уменьшили с 0.8 до 0.7
        } else {
            // Неудачные - увеличиваем, но с защитой
            if (word_fail_count_[input_word] < 3) {
                prio *= 1.3f;  // увеличили с 1.2 до 1.3
            } else {
                // Слишком много неудач - временно игнорируем
                prio *= 0.2f;  // уменьшили с 0.3 до 0.2
                std::cout << "⚠️ Cooling down '" << input_word << "'" << std::endl;
            }
        }
        
        // Ограничиваем приоритет
        prio = std::clamp(prio, 0.1f, 2.0f);  // уменьшили максимум с 3.0 до 2.0
        
        // ===== 10. СОХРАНЯЕМ СТАТИСТИКУ =====
        float accuracy = is_success ? 100.0f : 
                        (improvement > 1.0f ? 50.0f : 0.0f);
        
        accuracy_history_.push_back(accuracy);
        if (accuracy_history_.size() > 100) {
            accuracy_history_.erase(accuracy_history_.begin());
        }
        
        total_steps_++;
        
        // Периодический вывод с информацией о прогрессе
        if (total_steps_ % 50 == 0) {
            float avg_acc = std::accumulate(accuracy_history_.begin(), 
                                            accuracy_history_.end(), 0.0f) / 
                        accuracy_history_.size();
            std::cout << "Step " << total_steps_ << ": Avg accuracy = " 
                    << std::fixed << std::setprecision(1) << avg_acc << "%" << std::endl;
            
            // Показываем статистику освоения
            int mastered = getMasteredConceptsCount(MASTERY_THRESHOLD);
            int learning = getLearningConceptsCount(0.3f, MASTERY_THRESHOLD);
            int untouched = 614 - mastered - learning;
            
            std::cout << "  Concepts: mastered=" << mastered 
                     << ", learning=" << learning 
                     << ", untouched=" << untouched << std::endl;
        }

        // Проверяем, не зависла ли система
        static int last_success_step = 0;
        static int steps_without_success = 0;

        if (is_success) {
            last_success_step = total_steps_;
            steps_without_success = 0;
        } else {
            steps_without_success++;
        }

        // Если долго без успеха, встряхиваем систему
        if (steps_without_success > 30) {  // уменьшили с 50 до 30
            std::cout << "30 steps without success, switching to cycle mode" << std::endl;
            force_cycle_mode_ = true;
            
            // Заполняем список всех концептов
            if (all_concept_ids_.empty()) {
                for (const auto& ex : replay_buffer_) {
                    for (uint32_t mid : ex.expected_meanings) {
                        if (std::find(all_concept_ids_.begin(), all_concept_ids_.end(), mid) == all_concept_ids_.end()) {
                            all_concept_ids_.push_back(mid);
                        }
                    }
                }
            }
            
            steps_without_success = 0;
        }
        
        // Каждые 500 шагов сбрасываем цикл
        if (total_steps_ % 500 == 0) {
            force_cycle_mode_ = false;
            recent_concepts_.clear();
            std::cout << "Resetting cycle mode" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in trainOnExampleAdvanced: " << e.what() << std::endl;
    }

    is_processing_ = false;
}

void EffectiveLearning::forceLearnAllConcepts() {
    std::cout << "\n=== FORCED LEARNING OF ALL CONCEPTS ===\n";
    
    // Собираем все уникальные концепты
    std::set<uint32_t> all_concepts;
    for (const auto& ex : replay_buffer_) {
        for (uint32_t mid : ex.expected_meanings) {
            all_concepts.insert(mid);
        }
    }
    
    std::cout << "Total concepts to learn: " << all_concepts.size() << std::endl;
    
    int learned = 0;
    for (uint32_t concept_id : all_concepts) {
        if (!training_active_) break;
        
        // Находим пример для этого концепта
        for (const auto& ex : replay_buffer_) {
            bool found = false;
            for (uint32_t mid : ex.expected_meanings) {
                if (mid == concept_id) {
                    trainOnExampleAdvanced(ex, false);
                    learned++;
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
        
        // Показываем прогресс
        if (learned % 10 == 0) {
            std::cout << "\rProgress: " << learned << "/" << all_concepts.size() 
                     << " (" << (learned * 100 / all_concepts.size()) << "%)" << std::flush;
        }
    }
    
    std::cout << "\nForced learning complete. Learned " << learned << " concepts." << std::endl;
}

float EffectiveLearning::getConceptMastery(uint32_t concept_id) {

    auto& groups = neural_system.getGroups();
    int group_idx = 16 + (concept_id % 6);  // concept_id % 6 может быть 0-5, значит group_idx 16-21
    if (group_idx > 21 || group_idx >= groups.size()) {  // ДОБАВИТЬ ПРОВЕРКУ
        return 0.0f;
    }
    
    float activation = 0.0f;
    const auto& phi = groups[group_idx].getPhi();
    
    // Проверяем на nan/inf
    bool has_valid = false;
    for (int i = 0; i < 32; i++) {
        if (std::isfinite(phi[i])) {
            activation += phi[i];
            has_valid = true;
        }
    }
    
    if (!has_valid) return 0.5f;  // если все nan, возвращаем среднее
    
    return std::min(1.0f, activation / 32.0f);
}

// НОВЫЙ МЕТОД: вычисление приоритета для replay buffer
float EffectiveLearning::calculateNewPriority(const MeaningTrainingExample& example, 
                                             float learning_rate) {
    float base = example.priority;
    
    // Повышаем приоритет для:
    // - Сложных примеров
    base += example.difficulty * 0.1f;
    
    // - Примеров с cause-effect
    base += example.cause_effect.size() * 0.05f;
    
    // - Примеров с эмоциональной окраской
    if (!example.expected_meanings.empty()) {
        auto node = semantic_graph.getNode(example.expected_meanings[0]);
        if (node && node->emotional_tone != EmotionalTone::NEUTRAL) {
            base += 0.1f;
        }
    }
    
    return std::clamp(base, 0.0f, 1.0f);
}

// НОВЫЙ МЕТОД: продвинутое обучение причинности
void EffectiveLearning::trainCauseEffectAdvanced(int steps) {
    std::vector<MeaningTrainingExample> cause_effect_examples;
    
    // Собираем примеры с cause-effect из графа
    for (const auto& ex : replay_buffer_) {
        if (!ex.cause_effect.empty()) {
            cause_effect_examples.push_back(ex);
        }
    }
    
    if (cause_effect_examples.empty()) return;
    
    for (int i = 0; i < steps && training_active_; i++) {
        int idx = rng_() % cause_effect_examples.size();
        const auto& ex = cause_effect_examples[idx];
        
        // Обучаем последовательности
        if (ex.cause_effect.size() >= 2) {
            // Создаем временную задержку между причиной и следствием
            auto [cause_id, effect_id] = ex.cause_effect[0];
            
            // Сначала активируем причину
            activateMeaning(cause_id, 1.0f);
            
            // Небольшая задержка (имитация времени)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            // Затем ожидаем следствие
            activateMeaning(effect_id, 0.8f);
            
            // Укрепляем связь
            int cause_group = cause_id % 32;
            int effect_group = effect_id % 32;
            neural_system.strengthenInterConnection(
                cause_group, effect_group, 
                0.01f * ex.difficulty
            );
        }
        
        if (i % 100 == 0) {
            std::cout << "\r  Cause-effect progress: " << (i * 100 / steps) << "%" << std::flush;
        }
    }
    std::cout << std::endl;
}

// НОВЫЙ МЕТОД: активация смысла в нейросети
void EffectiveLearning::activateMeaning(uint32_t meaning_id, float strength) {
    auto node = semantic_graph.getNode(meaning_id);
    if (!node) return;
    
    auto& groups = neural_system.getGroups();
    // тоже эвристическое распределение.
    int group_idx = meaning_id % 32;
    
    if (group_idx < groups.size()) {
        auto& phi = groups[group_idx].getPhiNonConst();
        for (int i = 0; i < 32; i++) {
            phi[i] += strength * 0.1f * node->signature[i];
            phi[i] = std::clamp<double>(phi[i], 0.0, 1.0);  // явно указываем тип double
        }
    }
}

// НОВЫЙ МЕТОД: обучение с учетом контекста
void EffectiveLearning::trainContextAwareness(int steps) {
    std::vector<std::string> contexts = {
        "conversation", "command", "query", "system", 
        "security", "movement", "perception"
    };
    
    for (int i = 0; i < steps && training_active_; i++) {
        // Выбираем случайный контекст
        std::string context = contexts[rng_() % contexts.size()];
        
        // Получаем релевантные узлы для этого контекста
        auto relevant_nodes = semantic_graph.getRelevantNodes(context, 5);
        
        if (relevant_nodes.empty()) continue;
        
        for (uint32_t node_id : relevant_nodes) {
            auto node = semantic_graph.getNode(node_id);
            if (!node) continue;
            
            // Создаем пример: контекстное слово + смысл
            MeaningTrainingExample ctx_ex;
            ctx_ex.input_words = {context};
            ctx_ex.expected_meanings = {node_id};
            ctx_ex.difficulty = 0.6f;
            ctx_ex.category = "contextual";
            ctx_ex.priority = 0.7f;
            
            trainOnExampleAdvanced(ctx_ex, false);
        }
        
        if (i % 200 == 0) {
            std::cout << "\r  Context progress: " << (i * 100 / steps) << "%" << std::flush;
        }
    }
    std::cout << std::endl;
}

// НОВЫЙ МЕТОД: мета-обучение
void EffectiveLearning::trainMetaLearning(int steps) {
    std::cout << "\nLearning how to learn...\n";
    
    for (int i = 0; i < steps && training_active_; i++) {
        // Анализируем историю обучения
        if (accuracy_history_.size() > 10) {
            float recent_accuracy = std::accumulate(
                accuracy_history_.end() - 10, 
                accuracy_history_.end(), 
                0.0f) / 10.0f;
            
            float old_accuracy = std::accumulate(
                accuracy_history_.begin(), 
                accuracy_history_.begin() + 10, 
                0.0f) / 10.0f;
            
            float learning_speed = recent_accuracy - old_accuracy;
            
            // Адаптируем скорость обучения
            if (learning_speed < 0.01f) {
                // Учимся слишком медленно - ускоряем
                current_learning_rate_ *= 1.05f;
            } else if (learning_speed > 0.1f) {
                // Учимся слишком быстро (может переобучаться) - замедляем
                current_learning_rate_ *= 0.95f;
            }
            
            // Ограничиваем
            current_learning_rate_ = std::clamp(
                current_learning_rate_, 
                0.001f,  // float
                0.1f     // float
            ); // OK - все float
        }
        
        // Выбираем примеры, где хуже всего получается
        if (!loss_history_.empty() && !replay_buffer_.empty()) {
            // Находим примеры с высокой ошибкой
            std::vector<int> high_error_indices;
            for (size_t j = 0; j < std::min(size_t(100), replay_buffer_.size()); j++) {
                int idx = rng_() % replay_buffer_.size();
                high_error_indices.push_back(idx);
            }
            
            // Обучаем на них с повышенным вниманием
            for (int idx : high_error_indices) {
                trainOnExampleAdvanced(replay_buffer_[idx], false);
            }
        }
        
        epoch_++;
    }
}

// НОВЫЙ МЕТОД: более точная оценка
// В EffectiveLearning.cpp, замените evaluateAccuracyAdvanced:

float EffectiveLearning::evaluateAccuracyAdvanced(int num_tests) {
    if (replay_buffer_.empty()) return 0.0f;
    
    // Берем примеры, которые уже учили (с высоким приоритетом)
    std::vector<const MeaningTrainingExample*> test_examples;
    
    // Сортируем буфер по приоритету (высокие приоритеты - те, что учили)
    std::vector<size_t> indices(replay_buffer_.size());
    std::iota(indices.begin(), indices.end(), 0);
    
    std::sort(indices.begin(), indices.end(), [this](size_t a, size_t b) {
        return replay_buffer_[a].priority > replay_buffer_[b].priority;
    });
    
    // Берем top-N примеров
    int test_count = std::min(num_tests, (int)replay_buffer_.size());
    int correct = 0;
    int total = 0;
    
    for (int i = 0; i < test_count; i++) {
        const auto& ex = replay_buffer_[indices[i]];
        if (ex.input_words.empty()) continue;
        
        for (const auto& word : ex.input_words) {
            auto response = language_module.process(word, "test_user");
            auto response_meanings = language_module.textToMeanings(response);
            
            // Проверяем все ожидаемые смыслы
            bool all_found = true;
            for (uint32_t expected : ex.expected_meanings) {
                bool found = false;
                for (uint32_t resp : response_meanings) {
                    if (expected == resp) {
                        found = true;
                        break;
                    }
                    // Проверяем синонимы через граф
                    auto edges = semantic_graph.getEdgesFrom(resp);
                    for (const auto& edge : edges) {
                        if (edge.type == SemanticEdge::Type::SIMILAR_TO && 
                            edge.to_id == expected) {
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                }
                if (!found) {
                    all_found = false;
                    break;
                }
            }
            
            if (all_found) correct++;
            total++;
        }
    }
    
    float raw_accuracy = total > 0 ? (correct * 100.0f / total) : 0.0f;
    
    // Экспоненциальное сглаживание для стабильности
    static float smoothed_accuracy = 0.0f;
    if (smoothed_accuracy == 0.0f) {
        smoothed_accuracy = raw_accuracy;
    } else {
        smoothed_accuracy = smoothed_accuracy * 0.7f + raw_accuracy * 0.3f;
    }
    
    return smoothed_accuracy;
}

// НОВЫЙ МЕТОД: полный цикл обучения
void EffectiveLearning::runFullTraining() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "STARTING FULL SEMANTIC TRAINING" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    training_active_ = true;
    auto start_time = std::chrono::steady_clock::now();
    
    // 1. Базовое обучение (конкретные понятия)
    std::cout << "\nPhase 1: Concrete concepts" << std::endl;
    trainAbstractionLevel(1, 3, 2000);
    
    // 2. Действия и отношения
    std::cout << "\nPhase 2: Actions and relations" << std::endl;
    trainAbstractionLevel(3, 5, 3000);
    
    // 3. Причинность
    std::cout << "\nPhase 3: Cause and effect" << std::endl;
    trainCauseEffectAdvanced(2000);
    
    // 4. Абстракции с эмоциями
    std::cout << "\nPhase 4: Abstract concepts" << std::endl;
    trainAbstractionLevel(6, 8, 2000);
    
    // 5. Контекст
    std::cout << "\nPhase 5: Contextual understanding" << std::endl;
    trainContextAwareness(2000);
    
    // 6. Мета-обучение
    std::cout << "\nPhase 6: Meta-learning" << std::endl;
    trainMetaLearning(1000);
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::minutes>(end_time - start_time);
    
    training_active_ = false;
    
    // Финальная оценка
    float final_accuracy = evaluateAccuracyAdvanced(100);
    
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "TRAINING COMPLETE!" << std::endl;
    std::cout << "   Total steps: " << total_steps_ << std::endl;
    std::cout << "   Epochs: " << epoch_ << std::endl;
    std::cout << "   Duration: " << duration.count() << " minutes" << std::endl;
    std::cout << "   Final accuracy: " << std::fixed << std::setprecision(1) 
              << final_accuracy << "%" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    // Сохраняем результаты
    saveCheckpoint("full_training_complete");
}

// НОВЫЙ МЕТОД: демонстрация понимания
void EffectiveLearning::demonstrateUnderstanding() {
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "DEMONSTRATING SEMANTIC UNDERSTANDING" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    
    // Тест 1: Базовые понятия
    std::cout << "\nBasic concepts:" << std::endl;
    testWordUnderstanding("robot");
    testWordUnderstanding("camera");
    testWordUnderstanding("move");
    
    // Тест 2: Отношения
    std::cout << "\nRelations:" << std::endl;
    testRelationUnderstanding("camera", "see");
    testRelationUnderstanding("robot", "move");
    
    // Тест 3: Абстракции
    std::cout << "\nAbstract concepts:" << std::endl;
    testWordUnderstanding("good");
    testWordUnderstanding("because");
    
    // Тест 4: Контекст
    std::cout << "\nContextual understanding:" << std::endl;
    testContextUnderstanding("command", "move");
    testContextUnderstanding("conversation", "hello");
}

void EffectiveLearning::testWordUnderstanding(const std::string& word) {
    auto response = testResponse(word, "test_user");
    auto meanings = language_module.textToMeanings(word);
    
    std::cout << "  \"" << word << "\" → ";
    for (uint32_t mid : meanings) {
        auto node = semantic_graph.getNode(mid);
        if (node) {
            std::cout << node->name << "(" << mid << ") ";
        }
    }
    std::cout << std::endl;
    std::cout << "     Response: \"" << response << "\"" << std::endl;
}

void EffectiveLearning::testRelationUnderstanding(const std::string& from, const std::string& to) {
    auto from_meanings = language_module.textToMeanings(from);
    auto to_meanings = language_module.textToMeanings(to);
    
    if (from_meanings.empty() || to_meanings.empty()) return;
    
    uint32_t from_id = from_meanings[0];
    uint32_t to_id = to_meanings[0];
    
    // Проверяем связь в графе
    auto edges = semantic_graph.getEdgesFrom(from_id);
    bool has_relation = false;
    for (const auto& edge : edges) {
        if (edge.to_id == to_id) {
            has_relation = true;
            std::cout << "  " << from << " → " << to 
                      << " (" << SemanticEdge::typeToString(edge.type) 
                      << ", trust: " << edge.trust_level << ")" << std::endl;
            break;
        }
    }
    
    if (!has_relation) {
        std::cout << "  No direct relation between " << from << " and " << to << std::endl;
    }
}

void EffectiveLearning::testContextUnderstanding(const std::string& context, const std::string& word) {
    auto relevant = semantic_graph.getRelevantNodes(context, 5);
    bool is_relevant = false;
    
    auto word_meanings = language_module.textToMeanings(word);
    if (!word_meanings.empty()) {
        uint32_t word_id = word_meanings[0];
        for (uint32_t rel_id : relevant) {
            if (rel_id == word_id) {
                is_relevant = true;
                break;
            }
        }
    }
    
    std::cout << "  Context \"" << context << "\" → \"" << word << "\": "
              << (is_relevant ? "relevant" : "not relevant") << std::endl;
}

// ИСПРАВЛЕНИЕ 4: Добавить реализацию saveCheckpoint
void EffectiveLearning::saveCheckpoint(const std::string& name) {
    std::filesystem::create_directories("dump/learning");
    std::ofstream file("dump/learning/" + name + ".bin", std::ios::binary);
    if (!file) {
        std::cerr << "Failed to save checkpoint: " << name << std::endl;
        return;
    }
    
    file.write(reinterpret_cast<const char*>(&total_steps_), sizeof(total_steps_));
    file.write(reinterpret_cast<const char*>(&epoch_), sizeof(epoch_));
    file.write(reinterpret_cast<const char*>(&current_learning_rate_), sizeof(current_learning_rate_));
    
    size_t hist_size = accuracy_history_.size();
    file.write(reinterpret_cast<const char*>(&hist_size), sizeof(hist_size));
    if (hist_size > 0) {
        file.write(reinterpret_cast<const char*>(accuracy_history_.data()), 
                   hist_size * sizeof(float));
    }
    
    std::cout << "Checkpoint saved: " << name << std::endl;
}

// ИСПРАВЛЕНИЕ 5: Добавить реализацию loadCheckpoint
void EffectiveLearning::loadCheckpoint(const std::string& name) {
    std::ifstream file("dump/learning/" + name + ".bin", std::ios::binary);
    if (!file) {
        std::cerr << "Failed to load checkpoint: " << name << std::endl;
        return;
    }
    
    file.read(reinterpret_cast<char*>(&total_steps_), sizeof(total_steps_));
    file.read(reinterpret_cast<char*>(&epoch_), sizeof(epoch_));
    file.read(reinterpret_cast<char*>(&current_learning_rate_), sizeof(current_learning_rate_));
    
    size_t hist_size;
    file.read(reinterpret_cast<char*>(&hist_size), sizeof(hist_size));
    accuracy_history_.resize(hist_size);
    if (hist_size > 0) {
        file.read(reinterpret_cast<char*>(accuracy_history_.data()), 
                  hist_size * sizeof(float));
    }
    
    std::cout << "Checkpoint loaded: " << name << std::endl;
}

// ИСПРАВЛЕНИЕ 6: Добавить реализацию getStats
std::string EffectiveLearning::getStats() {
    std::stringstream ss;
    ss << "=== Learning Statistics ===\n";
    ss << "Total steps: " << total_steps_ << "\n";
    ss << "Epoch: " << epoch_ << "\n";
    ss << "Learning rate: " << std::fixed << std::setprecision(4) << current_learning_rate_ << "\n";
    ss << "Buffer size: " << replay_buffer_.size() << "/" << MAX_BUFFER_SIZE << "\n";
    
    if (!accuracy_history_.empty()) {
        float avg_accuracy = std::accumulate(accuracy_history_.begin(), 
                                             accuracy_history_.end(), 0.0f) / 
                            accuracy_history_.size();
        ss << "Avg accuracy: " << std::setprecision(1) << avg_accuracy << "%\n";
    }
    
    return ss.str();
}

void EffectiveLearning::stopTraining() {
    training_active_ = false;
    std::cout << "Training stopped by user" << std::endl;
}

    // запуск обучения
void EffectiveLearning::runSemanticTraining(int hours) {
    std::cout << "=== SEMANTIC TRAINING STARTED ===" << std::endl;
    std::cout << "Target duration: " << hours << " hours" << std::endl;
    std::cout << "Replay buffer size: " << replay_buffer_.size() << std::endl;
    
    training_active_ = true;
    auto start_time = std::chrono::steady_clock::now();
    int target_steps = hours * 3600 * 10;
    int iteration = 0;
    int last_accuracy_step = 0;
    
    // Для отслеживания прогресса по концептам
    std::map<uint32_t, float> best_mastery;
    
    while (training_active_ && iteration < target_steps) {  // <-- ДОБАВИТЬ training_active_ В УСЛОВИЕ
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        auto elapsed_hours = std::chrono::duration_cast<std::chrono::hours>(elapsed).count();
        auto elapsed_minutes = std::chrono::duration_cast<std::chrono::minutes>(elapsed).count();
        
        if (elapsed_hours >= hours) break;
        
        // Обучаемся на примерах с учетом приоритета
        for (int i = 0; i < 20 && training_active_; i++) {  // было 10
            if (replay_buffer_.empty()) break;
            
            // Выбираем пример с учетом приоритета (выше приоритет - чаще)
            float total_priority = 0;
            for (const auto& ex : replay_buffer_) {
                total_priority += ex.priority;
            }
            
            float r = static_cast<float>(rng_()) / RAND_MAX * total_priority;
            float cumsum = 0;
            int idx = 0;
            
            for (size_t j = 0; j < replay_buffer_.size(); j++) {
                cumsum += replay_buffer_[j].priority;
                if (r <= cumsum) {
                    idx = j;
                    break;
                }
            }
            
            trainOnExampleAdvanced(replay_buffer_[idx]);
            iteration++;
            total_steps_++;
        }
        
        // Оценка прогресса
        if (iteration - last_accuracy_step >= 100) {
            float accuracy = evaluateAccuracyAdvanced(20);
            accuracy_history_.push_back(accuracy);
            
            // Показываем прогресс
            std::cout << "\rTraining: " << elapsed_minutes << " min, "
                      << "steps: " << iteration << ", "
                      << "accuracy: " << std::fixed << std::setprecision(1) << accuracy << "%"
                      << " | rate: " << std::setprecision(3) << current_learning_rate_
                      << std::flush;
            
            last_accuracy_step = iteration;
        }
        
        // Адаптация скорости обучения
        if (accuracy_history_.size() > 5) {
            float recent_avg = 0;
            for (size_t i = accuracy_history_.size() - 5; i < accuracy_history_.size(); i++) {
                recent_avg += accuracy_history_[i];
            }
            recent_avg /= 5;
            
            float older_avg = 0;
            for (size_t i = 0; i < 5 && i < accuracy_history_.size() - 5; i++) {
                older_avg += accuracy_history_[i];
            }
            if (accuracy_history_.size() >= 10) older_avg /= 5;
            
            if (recent_avg < older_avg * 0.9f) {
                // Точность падает - уменьшаем скорость
                current_learning_rate_ *= 0.95f;
            } else if (recent_avg > older_avg * 1.1f) {
                // Точность растет - можно увеличить скорость
                current_learning_rate_ *= 1.02f;
            }
            
            current_learning_rate_ = std::clamp(current_learning_rate_, 0.001f, 0.1f);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    training_active_ = false;
    std::cout << "\n=== SEMANTIC TRAINING FINISHED ===" << std::endl;
    std::cout << "Total steps: " << iteration << std::endl;
    std::cout << "Final accuracy: " << std::fixed << std::setprecision(1) 
              << getAverageAccuracy() << "%" << std::endl;
}

// NEW Cu
// НОВЫЙ МЕТОД: инициализация любопытства
void EffectiveLearning::initializeCuriosity() {
    curiosity = std::make_unique<CuriosityDriver>(
        neural_system, language_module, semantic_graph
    );
}

// НОВЫЙ МЕТОД: получить вопрос от системы
std::string EffectiveLearning::askQuestion() {
    if (!curiosity) initializeCuriosity();
    return curiosity->getNextQuestion();
}

// НОВЫЙ МЕТОД: обработать ответ пользователя
void EffectiveLearning::receiveAnswer(const std::string& question, const std::string& answer) {
    if (!curiosity) initializeCuriosity();
    curiosity->processAnswer(question, answer);
    
    // Награждаем систему за взаимодействие
    neural_system.step(0.5f, total_steps_++);
}

// НОВЫЙ МЕТОД: обучение через диалог
void EffectiveLearning::learnFromDialogue(const std::string& user_input, 
                      const std::string& user_name) {
    
    // 1. Обрабатываем ввод пользователя
    auto response = language_module.process(user_input, user_name);
    
    // 2. Извлекаем смыслы
    auto meanings = language_module.textToMeanings(user_input);
    
    // 3. Если есть неопределенность, генерируем вопрос
    if (meanings.empty() || response.find("not sure") != std::string::npos) {
        std::string question = askQuestion();
        std::cout << "?Mary: " << question << std::endl;
    } else {
        std::cout << "!Mary: " << response << std::endl;
    }
}

float EffectiveLearning::getAverageMastery() {
    float total = 0.0f;
    int count = 0;
    
    // Проверяем все концепты от 1 до 614 (максимальный ID в графе)
    for (uint32_t id = 1; id <= 614; id++) {
        float mastery = getConceptMastery(id);
        if (mastery > 0.01f) {  // только те, которые хоть как-то активированы
            total += mastery;
            count++;
        }
    }
    
    return count > 0 ? total / count : 0.0f;
}

int EffectiveLearning::getMasteredConceptsCount(float threshold) {
    int count = 0;
    for (uint32_t id = 1; id <= 614; id++) {
        if (getConceptMastery(id) >= threshold) {
            count++;
        }
    }
    return count;
}

int EffectiveLearning::getLearningConceptsCount(float min_threshold, float max_threshold) {
    int count = 0;
    for (uint32_t id = 1; id <= 614; id++) {
        float mastery = getConceptMastery(id);
        if (mastery >= min_threshold && mastery < max_threshold) {
            count++;
        }
    }
    return count;
}

std::string EffectiveLearning::getConceptsProgressBar(int width) {
    float avg = getAverageMastery();
    int filled = static_cast<int>(avg * width);
    
    std::string bar = "[";
    for (int i = 0; i < width; i++) {
        if (i < filled) {
            bar += "█";
        } else {
            bar += "░";
        }
    }
    bar += "] " + std::to_string(static_cast<int>(avg * 100)) + "%";
    
    return bar;
}

std::map<int, float> EffectiveLearning::getMasteryByAbstractionLevel() {
    std::map<int, float> level_mastery;
    std::map<int, int> level_count;
    
    for (uint32_t id = 1; id <= 614; id++) {
        auto node = semantic_graph.getNode(id);
        if (!node) continue;
        
        float mastery = getConceptMastery(id);
        int level = node->abstraction_level;
        
        level_mastery[level] += mastery;
        level_count[level]++;
    }
    
    // Усредняем
    for (auto& [level, total] : level_mastery) {
        if (level_count[level] > 0) {
            level_mastery[level] = total / level_count[level];
        }
    }
    
    return level_mastery;
}

std::map<uint32_t, float> EffectiveLearning::getAllConceptsMastery() {
    std::map<uint32_t, float> result;
    
    // Берем только концепты, которые есть в графе и имеют имя
    for (uint32_t id = 1; id <= 614; id++) {
        auto node = semantic_graph.getNode(id);
        if (node && !node->canonical_form.empty()) {
            float mastery = getConceptMastery(id);
            if (mastery > 0.01f) {  // только значимые
                result[id] = mastery;
            }
        }
    }
    
    return result;
}