// modules/learning/LearningOrchestrator.cpp - обновлённая версия
#include "LearningOrchestrator.hpp"
#include "../../modules/learning/NeuralTrainer.hpp"
#include "../../modules/learning/TrainingExampleManager.hpp"
#include "../../modules/learning/ConceptMasteryEvaluator.hpp"
#include "../../modules/learning/CurriculumTrainer.hpp"
#include "../../core/DeviceProbe.hpp"
#include "../../data/PersonnelData.hpp"
#include "../../core/IAuthorization.hpp"  
#include "../../modules/lang/LanguageModule.hpp" 

LearningOrchestrator::LearningOrchestrator(
    NeuralFieldSystem& ns,
    LanguageModule& lang,
    SemanticManager& sm,
    MemoryManager& mem,
    IAuthorization& auth,
    const DeviceDescriptor& dev,
    SemanticGraphDatabase& graph
) : neural_system_(ns)
    , language_module_(lang)
    , semantic_manager_(sm)
    , memory_manager_(mem)
    , auth_(auth)
    , device_info_(dev)
    , semantic_graph_(graph)
    , rng_(std::random_device{}()) {
    
    // Инициализируем компоненты
    mastery_evaluator_ = std::make_unique<ConceptMasteryEvaluator>(ns, graph);
    example_manager_ = std::make_unique<TrainingExampleManager>(graph, sm);
    neural_trainer_ = std::make_unique<NeuralTrainer>(ns, graph, *mastery_evaluator_);
    
    // ИСПРАВЛЕНО: передаем все 5 аргументов
    curriculum_trainer_ = std::make_unique<CurriculumTrainer>(
        *neural_trainer_, 
        *example_manager_, 
        *mastery_evaluator_,
        neural_system_,      // NeuralFieldSystem& ns
        semantic_graph_      // SemanticGraphDatabase& graph
    );
    
    loadCurriculum();
}
LearningOrchestrator::~LearningOrchestrator() = default;

void LearningOrchestrator::loadCurriculum() {
    std::cout << "Loading semantic curriculum from graph..." << std::endl;
    
    // ИСПРАВЛЕНИЕ: используем semantic_graph_ вместо неопределённой переменной
    // Вместо вызова несуществующего метода getNodes() используем реальные методы
    
    // Вариант 1: если есть метод для получения всех узлов
    auto examples = semantic_graph_.getAllNodes(); 
    
    // Вариант 2: проходим по всем ID от 1 до максимального
    std::cout << "  Generating examples from existing nodes..." << std::endl;
    
    // Проходим по ID от 1 до 614 (максимальный ID в графе)
    for (uint32_t id = 1; id <= 614; id++) {
        auto node = semantic_graph_.getNode(id);
        if (!node || node->canonical_form.empty()) continue;
        
        MeaningTrainingExample ex;
        ex.difficulty = node->abstraction_level / 10.0f;
        ex.category = node->primary_category;
        ex.priority = ex.difficulty;
        ex.expected_meanings = {node->id};
        ex.required_level = AccessLevel::GUEST;
        
        // Добавляем входные слова (каноническая форма + алиасы)
        ex.input_words.push_back(node->canonical_form);
        for (const auto& alias : node->aliases) {
            if (ex.input_words.size() < 5) { // ограничим количество
                ex.input_words.push_back(alias);
            }
        }
        
        example_manager_->addExample(ex);
    }
    
    // Добавляем примеры с уровнями доступа
    example_manager_->addMeaningExample(
        {"hello", "hi", "hey"},
        {"greeting"},
        {},
        0.1f, "communication", AccessLevel::GUEST);
    
    example_manager_->addMeaningExample(
        {"what", "is", "your", "name"},
        {"question", "my", "name", "mary"},
        {{43, 1}},  // question -> system_identity
        0.3f, "identity", AccessLevel::GUEST);
    
    if (device_info_.actuators.motor) {
        example_manager_->addMeaningExample(
            {"move", "forward"},
            {"action_move"},
            {{81, 85}},
            0.4f, "action", AccessLevel::OPERATOR);
        
        example_manager_->addMeaningExample(
            {"stop", "halt"},
            {"action_stop"},
            {{83, 87}},
            0.3f, "action", AccessLevel::OPERATOR);
    }
    
    example_manager_->addMeaningExample(
        {"status", "report"},
        {"action_status"},
        {},
        0.4f, "system", AccessLevel::EMPLOYEE);
    
    example_manager_->addMeaningExample(
        {"add", "user", "employee"},
        {"access_employee"},
        {},
        0.6f, "access", AccessLevel::MANAGER);
    
    example_manager_->addMeaningExample(
        {"shutdown", "system"},
        {"action_stop"},
        {},
        0.7f, "system", AccessLevel::ADMIN);
    
    example_manager_->addMeaningExample(
        {"configure", "settings"},
        {"system_capability"},
        {},
        0.8f, "system", AccessLevel::MASTER);
    
    std::cout << "   Loaded " << example_manager_->getBufferSize() 
              << " training examples" << std::endl;
}

void LearningOrchestrator::runTraining(int hours) {
    std::cout << "=== SEMANTIC TRAINING STARTED ===" << std::endl;
    std::cout << "Target duration: " << hours << " hours" << std::endl;
    
    // Запускаем в отдельном потоке с блокировкой
    std::thread training_thread([this]() {
        // Блокируем нейросеть на всё время обучения
        std::lock_guard<std::mutex> lock(neural_system_.getMutex());
        
        neural_trainer_->startTraining();
        curriculum_trainer_->runFullTraining();
        neural_trainer_->stopTraining();
    });
    
    training_thread.detach();  // или join, в зависимости от логики
}

void LearningOrchestrator::stopTraining() {
    neural_trainer_->stopTraining();
    curriculum_trainer_->stopTraining();
}

void LearningOrchestrator::addExample(const MeaningTrainingExample& example) {
    example_manager_->addExample(example);
}

void LearningOrchestrator::processNextExample() {
    const auto* example = example_manager_->selectExampleByPriority();
    if (!example) return;
    
    if (neural_trainer_->isProcessing()) return;
    neural_trainer_->setProcessing(true);
    
    bool success = false;
    float reward = 0.0f;
    std::vector<uint32_t> output_meanings;
    
    try {
        auto& groups = neural_system_.getGroupsNonConst();
        
        std::string input_word = example->input_words[0];
        auto input_signal = neural_trainer_->createInputSignal(input_word);
        
        // Подаем сигнал
        for (int i = 0; i < 32; i++) {
            groups[0].getPhiNonConst()[i] = input_signal[i];
        }
        
        // Эволюция нейросети
        for (int step = 0; step < 4; step++) {
            neural_system_.step(0.0f, neural_trainer_->getTotalSteps() + step);
        }
        
        // Измеряем результат
        float activation = neural_trainer_->measureActivationForMeanings(
            example->expected_meanings);
        
        float success_threshold = 5.3f;
        uint32_t primary_concept = example->expected_meanings.empty() ? 
            0 : example->expected_meanings[0];
        
        reward = neural_trainer_->computeReward(activation, success_threshold, primary_concept);
        success = activation > success_threshold;
        
        // Применяем награду
        neural_trainer_->applySTDPReward(reward);
        
        // ===== ИСПРАВЛЕНО: получаем output_meanings напрямую =====
        // Используем semantic_manager для извлечения смыслов
        output_meanings = semantic_manager_.extractMeaningsFromSystem();
        
        // Обновляем статистику
        example_manager_->recordWordAttempt(input_word, success);
        
        float new_priority = example->priority;
        if (success) {
            new_priority *= 0.7f;
        } else {
            int fails = example_manager_->getWordFailCount(input_word);
            new_priority *= (fails < 3) ? 1.3f : 0.2f;
        }
        example_manager_->updatePriority(example, new_priority);
        
        float accuracy = success ? 100.0f : 0.0f;
        neural_trainer_->recordAccuracy(accuracy);
        neural_trainer_->incrementSteps();
        
    } catch (const std::exception& e) {
        std::cerr << "Error in processNextExample: " << e.what() << std::endl;
    }
    
    // ===== ИСПРАВЛЕНО: награда за последовательность (используем output_meanings) =====
    if (!output_meanings.empty() && example) {
        const auto& expected = example->expected_meanings;
        
        float similarity = 0.0f;
        for (uint32_t out_id : output_meanings) {
            for (uint32_t exp_id : expected) {
                if (out_id == exp_id) {
                    similarity += 1.0f;
                    break;
                }
            }
        }
        if (expected.size() > 0) {
            similarity /= expected.size();
        }
        
        if (similarity > 0.7f) {
            float seq_reward = 5.0f * similarity;
            
            auto& groups = neural_system_.getGroupsNonConst();
            for (int g = 16; g <= 21; g++) {
                groups[g].learnSTDP(seq_reward, neural_trainer_->getTotalSteps());
            }
            
            // Усиливаем связи между активными группами
            std::set<int> active_groups;
            for (uint32_t mid : output_meanings) {
                int group_idx = 16 + getGroupForConcept(mid);
                active_groups.insert(group_idx);
            }

            for (int from : active_groups) {
                for (int to : active_groups) {
                    if (from != to) {
                        neural_system_.strengthenInterConnection(from, to, 0.03f);
                    }
                }
            }
        }
    }
    
    neural_trainer_->setProcessing(false);
}

std::string LearningOrchestrator::testResponse(const std::string& input, 
                                               const std::string& user) {
    if (!auth_.authorize(user)) {
        return "Access denied: user " + user + " not authorized";
    }
    
    std::string response = language_module_.process(input, user);
    
    if (response.empty()) {
        auto meanings = language_module_.textToMeanings(input);
        if (!meanings.empty()) {
            response = "I understand: ";
            for (uint32_t mid : meanings) {
                // ИСПРАВЛЕНИЕ: используем semantic_graph_
                auto node = semantic_graph_.getNode(mid);
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
void LearningOrchestrator::demonstrateTraining(const std::string& user) {
    std::cout << "\n=== DEMONSTRATING TRAINING PROGRESS ===\n";
    std::cout << "User: " << user << " (" 
              << accessLevelToString(auth_.getCurrentLevel()) << ")\n";
    
    std::vector<std::pair<std::string, std::string>> tests;
    tests.push_back({"hello", "Hello"});
    tests.push_back({"who are you", "I'm Mary"});
    
    if (auth_.canPerform("status", AccessLevel::EMPLOYEE)) {
        tests.push_back({"status", "System status: operational"});
    }
    
    int correct = 0;
    int total = 0;
    
    for (const auto& [input, expected] : tests) {
        std::string response = testResponse(input, user);
        
        // Простая оценка
        bool is_correct = response.find(expected) != std::string::npos;
        if (is_correct) correct++;
        total++;
        
        std::cout << "Input: \"" << input << "\"\n";
        std::cout << "Response: \"" << response << "\"\n";
        std::cout << "Expected: \"" << expected << "\"\n";
        std::cout << "Result: " << (is_correct ? "✓" : "✗") << "\n---\n";
    }
    
    std::cout << "Overall accuracy: " << (correct * 100 / total) << "% (" 
              << correct << "/" << total << ")\n";
}

std::string LearningOrchestrator::getStats() const {
    std::stringstream ss;
    ss << "=== Learning Statistics ===\n";
    ss << "Total steps: " << neural_trainer_->getTotalSteps() << "\n";
    ss << "Learning rate: " << std::fixed << std::setprecision(4) 
       << neural_trainer_->getLearningRate() << "\n";
    ss << "Buffer size: " << example_manager_->getBufferSize() << "\n";
    ss << "Avg accuracy: " << std::setprecision(1) 
       << neural_trainer_->getAverageAccuracy() << "%\n";
    ss << "Average mastery: " << std::setprecision(2) 
       << mastery_evaluator_->getAverageMastery() << "\n";
    ss << "Mastered concepts: " << mastery_evaluator_->getMasteredConceptsCount() << "\n";
    return ss.str();
}