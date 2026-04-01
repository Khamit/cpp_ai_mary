// modules/learning/LearningOrchestrator.hpp
#pragma once
#include <vector>
#include <deque>
#include <string>
#include <map>
#include <memory>
#include <random>
#include "../../core/Component.hpp" 
#include "../../data/MeaningTrainingExample_fwd.hpp"
#include "../learning/NeuralTrainer.hpp"        
#include "../learning/TrainingExampleManager.hpp"
#include "../learning/ConceptMasteryEvaluator.hpp" 
#include "../learning/CurriculumTrainer.hpp"       

class LanguageModule;
class SemanticManager;
class MemoryManager;
class IAuthorization;
struct DeviceDescriptor;
class NeuralFieldSystem;
class SemanticGraphDatabase;
class ConceptMasteryEvaluator;
class TrainingExampleManager;
class NeuralTrainer;
class CurriculumTrainer;

class LearningOrchestrator : public Component { 
private:
    std::unique_ptr<ConceptMasteryEvaluator> mastery_evaluator_;
    std::unique_ptr<TrainingExampleManager> example_manager_;
    std::unique_ptr<NeuralTrainer> neural_trainer_;
    std::unique_ptr<CurriculumTrainer> curriculum_trainer_;

    NeuralFieldSystem& neural_system_;
    LanguageModule& language_module_;
    SemanticManager& semantic_manager_;
    MemoryManager& memory_manager_;
    IAuthorization& auth_;
    const DeviceDescriptor& device_info_;
    SemanticGraphDatabase& semantic_graph_;
    
    std::mt19937 rng_;

    bool training_active_ = false;      // <-- ДОБАВИТЬ
    int total_steps_ = 0;               // <-- ДОБАВИТЬ

    void registerWithLanguageModule();
    
    void loadCurriculum();

        // Добавляем метод для определения группы по концепту
    int getGroupForConcept(uint32_t id) const {
        auto node = semantic_graph_.getNode(id);
        if (!node) return id % 6;
        
        const std::string& cat = node->primary_category;
        
        // Группа 0 (группа 16): Физические объекты и устройства
        if (cat == "device" || cat == "sensor" || cat == "object" || 
            cat == "tool" || cat == "file" || cat == "shape" || cat == "shape_3d") {
            return 0;
        }
        
        // Группа 1 (группа 17): Действия и процессы
        if (cat == "action" || cat == "action_file" || cat == "action_control" ||
            cat == "action_modify" || cat == "function" || cat == "process" ||
            cat == "operation" || cat == "command") {
            return 1;
        }
        
        // Группа 2 (группа 18): Состояния и результаты
        if (cat == "state" || cat == "result" || cat == "property" ||
            cat == "attribute" || cat == "measurement" || cat == "quantity" ||
            cat == "duration" || cat == "frequency" || cat == "time_unit") {
            return 2;
        }
        
        // Группа 3 (группа 19): Логика и абстракции
        if (cat == "logic" || cat == "causality" || cat == "modality" ||
            cat == "abstract" || cat == "mathematics" || cat == "statistics" ||
            cat == "quantifier" || cat == "comparative" || cat == "math_operation") {
            return 3;
        }
        
        // Группа 4 (группа 20): Коммуникация и социальное
        if (cat == "communication" || cat == "social" || cat == "linguistic" ||
            cat == "pronoun" || cat == "question_word" || cat == "rhetorical" ||
            cat == "greeting" || cat == "insult" || cat == "praise") {
            return 4;
        }
        
        // Группа 5 (группа 21): Метафизика и высшие абстракции
        if (cat == "metaphysical" || cat == "value" || cat == "ethics" ||
            cat == "identity" || cat == "metacognition" || cat == "semantic_role" ||
            cat == "visualization" || cat == "affect" || cat == "behavioral" ||
            cat == "physiological" || cat == "evaluation" || cat == "security" ||
            cat == "frame" || cat == "data_structure" || cat == "pattern") {
            return 5;
        }
        
        // Категории по умолчанию
        if (cat == "general" || cat == "temporal" || cat == "space" || cat == "time" ||
            cat == "place" || cat == "weather" || cat == "color" || cat == "light") {
            return 3; // абстракции по умолчанию
        }
        
        return id % 6; // fallback
    }

public:
    LearningOrchestrator(
        NeuralFieldSystem& ns,
        LanguageModule& lang,
        SemanticManager& sm,
        MemoryManager& mem,
        IAuthorization& auth,
        const DeviceDescriptor& dev,
        SemanticGraphDatabase& graph
    );
    
    ~LearningOrchestrator();

    // Реализация методов Component
    std::string getName() const override { return "LearningOrchestrator"; }

    bool initialize(const Config& config) override {
        std::cout << "LearningOrchestrator initialized" << std::endl;
        loadCurriculum();
        return true;
    }
    
    void shutdown() override {
        std::cout << "LearningOrchestrator shutting down" << std::endl;
        stopTraining();
    }

        void update(float dt) override {
        // Можно обрабатывать один пример за шаг, если нужно
        // processNextExample();
    }
    
    void saveState(MemoryManager& memory) override {
        // TODO: сохранять состояние обучения
    }
    
    void loadState(MemoryManager& memory) override {
        // TODO: загружать состояние обучения
    }

    // новый 
    void runExploratoryLearning(int steps);
    float detectNovelPatterns();
    
    // Высокоуровневое обучение
    void runTraining(int hours);
    void stopTraining();
    
    // Работа с примерами
    void addExample(const MeaningTrainingExample& example);
    void processNextExample();
    
    // Диалоговые методы
    std::string testResponse(const std::string& input, const std::string& user);
    void demonstrateTraining(const std::string& user);
    
    // Статистика
    std::string getStats() const;
    float getAverageAccuracy() const { return neural_trainer_->getAverageAccuracy(); }
    
    // Доступ к компонентам (для обратной совместимости)
    ConceptMasteryEvaluator& getMasteryEvaluator() { return *mastery_evaluator_; }
    TrainingExampleManager& getExampleManager() { return *example_manager_; }
    NeuralTrainer& getNeuralTrainer() { return *neural_trainer_; }
    CurriculumTrainer& getCurriculumTrainer() { return *curriculum_trainer_; }
};