#pragma once

#include <atomic>
#include <vector>
#include <deque>
#include <string>
#include <random>
#include <fstream>
#include <sstream>
#include <chrono>
#include <numeric>
#include <memory>
#include <cstdint>
#include <filesystem>
// Project includes
#include "../../core/AccessLevel.hpp"
#include "modules/learning/CuriosityDriver.hpp"
#include "../../data/SemanticGraphDatabase.hpp"
#include "../../core/IAuthorization.hpp"
#include "../lang/LanguageModule.hpp"
#include "../semantic/SemanticManager.hpp"  
#include "../../core/DeviceProbe.hpp"
#include "../../data/PersonnelData.hpp"
#include "../../data/MeaningTrainingExample_fwd.hpp"

class NeuralFieldSystem;
class MemoryManager;

class EffectiveLearning {
private:

    NeuralFieldSystem& neural_system;
    LanguageModule& language_module;
    SemanticManager& semantic_manager;
    MemoryManager& memory_manager;

    PersonnelDatabase* personnel_db;

    IAuthorization& auth;
    const DeviceDescriptor& device_info;
    SemanticGraphDatabase& semantic_graph;

    std::deque<MeaningTrainingExample> replay_buffer_;
    static constexpr size_t MAX_BUFFER_SIZE = 10000;

    float base_learning_rate_ = 0.01f;
    float current_learning_rate_;

    int total_steps_ = 0;
    int epoch_ = 0;

    std::atomic<bool> training_active_{false};  // вместо bool training_active_ = false;

    std::mt19937 rng_{std::random_device{}()};

    std::vector<float> loss_history_;
    std::vector<float> accuracy_history_;

    std::unique_ptr<CuriosityDriver> curiosity;

    // Для интеллектуального обучения
    std::map<std::string, int> word_fail_count_;
    std::map<std::string, int> word_success_count_;
    std::deque<std::string> recent_successful_words_;
    std::map<uint32_t, float> concept_mastery_;  // уровень владения концептом
    
    // Приоритетная очередь для отложенных примеров
    struct DeferredExample {
        MeaningTrainingExample example;
        float defer_until;  // шаг, когда можно вернуться
        int defer_count;
    };
    std::vector<DeferredExample> deferred_examples_;

    std::map<std::string, int> word_total_attempts_;  // добавить

    bool is_processing_ = false;  // флаг, что мы уже обрабатываем пример

        // Новые поля для отслеживания прогресса
    std::map<uint32_t, float> concept_mastery_history_;  // история освоения концептов
    std::deque<uint32_t> recent_concepts_;  // недавние концепты (чтобы избежать повторов)
    int concepts_learning_cycle_ = 0;  // текущий цикл обучения
    std::vector<uint32_t> all_concept_ids_;  // все ID концептов для обучения
    size_t current_concept_index_ = 0;  // текущая позиция в цикле
    bool force_cycle_mode_ = false;  // принудительный циклический режим
    
    // Новые константы
    static constexpr int MAX_RECENT_CONCEPTS = 20;  // сколько концептов помнить
    static constexpr float MASTERY_THRESHOLD = 0.7f;  // порог освоения

public:

    EffectiveLearning(
        NeuralFieldSystem& ns,
        LanguageModule& lang,
        SemanticManager& sm,
        MemoryManager& mem,
        PersonnelDatabase* db,
        IAuthorization& auth,
        const DeviceDescriptor& dev,
        SemanticGraphDatabase& graph
    );

    // Initialization
    void loadMeaningCurriculum();

    // метод для проверки мастерства концепта
    float getConceptMastery(uint32_t concept_id);

    void addMeaningExample(
        const std::vector<std::string>& input_words,
        const std::vector<std::string>& expected_meanings,
        const std::vector<std::pair<uint32_t, uint32_t>>& cause_effect,
        float difficulty,
        const std::string& category,
        AccessLevel required = AccessLevel::GUEST
    );

    // Training control
    void runSemanticTraining(int hours = 1);
    void runFullTraining();

    void stopTraining();
    bool isTrainingActive() const { return training_active_; }

    // Training strategies
    void intelligentLearningPlan();
    
    void forceLearnAllConcepts();

    void trainAbstractionLevel(
        int min_abs,
        int max_abs,
        int steps
    );

    void trainCauseEffectAdvanced(int steps);

    void trainContextAwareness(int steps);

    void trainMetaLearning(int steps);

    // Core training
    void trainOnExampleAdvanced(
        const MeaningTrainingExample& example,
        bool use_curriculum = true
    );

    void activateMeaning(
        uint32_t meaning_id,
        float strength
    );

    // Evaluation
    float evaluateAccuracyAdvanced(int num_tests = 100);

    void saveCheckpoint(const std::string& name);

    void loadCheckpoint(const std::string& name);

    std::string getStats();

    // Dialogue testing
    std::string testResponse(
        const std::string& input,
        const std::string& user_name = "guest"
    );

    float evaluateResponse(
        const std::string& input,
        const std::string& expected,
        const std::string& user_name = "guest"
    );

    void demonstrateTraining(
        const std::string& user_name = "master"
    );

    void testUnderstanding(
        const std::string& user_name = "master"
    );

    void testAccessLevels();

    // Understanding diagnostics
    void demonstrateUnderstanding();

    void testWordUnderstanding(
        const std::string& word
    );

    void testRelationUnderstanding(
        const std::string& from,
        const std::string& to
    );

    void testContextUnderstanding(
        const std::string& context,
        const std::string& word
    );

    // Curiosity system
    void initializeCuriosity();

    std::string askQuestion();

    void receiveAnswer(
        const std::string& question,
        const std::string& answer
    );

    void learnFromDialogue(
        const std::string& user_input,
        const std::string& user_name
    );

    // Replay priority update
    float calculateNewPriority(
        const MeaningTrainingExample& example,
        float learning_rate
    );

    // Statistics
    int getTotalSteps() const { return total_steps_; }

    int getEpoch() const { return epoch_; }

    float getCurrentLearningRate() const { return current_learning_rate_; }

    size_t getBufferSize() const { return replay_buffer_.size(); }

    float getAverageAccuracy() const
    {
        if (accuracy_history_.empty()) return 0.0f;

        return std::accumulate(
            accuracy_history_.begin(),
            accuracy_history_.end(),
            0.0f
        ) / accuracy_history_.size();
    }
    // БоБо для экрана! БобоБОБО!!!
    // Новые методы для UI
    std::map<uint32_t, float> getAllConceptsMastery();
    float getAverageMastery();
    int getMasteredConceptsCount(float threshold = 0.7f);
    int getLearningConceptsCount(float min_threshold = 0.3f, float max_threshold = 0.7f);
    std::string getConceptsProgressBar(int width = 20);
    
    // Статистика по уровням абстракции
    std::map<int, float> getMasteryByAbstractionLevel();
};