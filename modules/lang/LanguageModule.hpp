#pragma once

#include <string>
#include <vector>
#include <map>
#include <random>
#include <memory>
#include <thread>
#include "WordGenerationEngine.hpp"
#include "../../core/Component.hpp"
#include "../../core/NeuralFieldSystem.hpp"

// НОВЫЕ МОДУЛИ - ДОБАВИТЬ ЭТИ INCLUDE
#include "FactExtractor.hpp"
#include "UserProfile.hpp"
#include "ContextTracker.hpp"
#include "ResponseGenerator.hpp"
#include <learning/EffectiveLearning.hpp>

class EvolutionModule;
class MemoryManager;
// система оценки уверенности
struct ConfidenceScore {
    float overall;           // общая уверенность (0-1)
    float factual;           // уверенность в фактах
    float contextual;        // привязка к контексту
    float linguistic;        // лингвистическая правильность
    bool hasKnowledge;       // есть ли знания по теме
    std::string fallbackReason; // причина низкой уверенности
};


// Statistics
struct NeuronStats {
    int active = 0;      // phi > 0.7
    int passive = 0;     // 0.1 < phi <= 0.7
    int inactive = 0;    // phi <= 0.1
    double avgActivity = 0.0;
};

class LanguageModule : public Component {
public:
    // Структура для буфера предложений мутаций
    struct PendingMutation {
        int from;
        int to;
        double delta;
        std::string reason;
    };
    
    // ИЗМЕНЕНО: добавили MemoryManager в конструктор
    explicit LanguageModule(NeuralFieldSystem& system, EvolutionModule& evolution, MemoryManager& memory);

    // Component interface
    std::string getName() const override { return "LanguageModule"; }
    bool initialize(const Config& config) override;
    void shutdown() override;
    void update(float dt) override;
    void saveState(MemoryManager& memory) override;
    void loadState(MemoryManager& memory) override;

    // Public API
    std::string process(const std::string& input);
    void giveFeedback(float rating, bool autoFeedback = false);
    void setContext(const std::string& context);
    double getLanguageFitness() const;
    int getLearnedWordsCount() const { return learned_words_.size(); }

    // AUTO LEARNING
    // Запуск автоматического обучения
    void runAutoLearning(int steps, EffectiveLearning* effectiveLearning = nullptr);
    void stopAutoLearning();

    // Получить статус обучения
    bool isLearning() const { return autoLearningActive_; }

    // Оценка
    void addExternalFeedback(float rating) {
        external_feedback_sum_ += rating;
        external_feedback_count_++;
    }
    
    float getExternalFeedbackAvg() const {
        return external_feedback_count_ > 0 ? 
               external_feedback_sum_ / external_feedback_count_ : 0.5f;
    }

        // Новые методы
    ConfidenceScore evaluateConfidence(const std::string& input, const std::string& response);
    bool shouldRespond(const ConfidenceScore& confidence);
    std::string getFallbackResponse(const ConfidenceScore& confidence);
    

    // мутации
    void applyPendingMutations();

    // Persistence
    void saveAll();
    void loadAll();

private:
    // Constants
    static constexpr int GROUP_PHONETICS_START = 0;
    static constexpr int GROUP_PHONETICS_END = 4;
    static constexpr int GROUP_BIGRAMS_START = 4;
    static constexpr int GROUP_BIGRAMS_END = 8;
    static constexpr int GROUP_SEMANTIC_START = 16;
    static constexpr int GROUP_SEMANTIC_END = 24;
    static constexpr int GROUP_CONTEXT_START = 28;
    static constexpr int GROUP_CONTEXT_END = 30;
    static constexpr int GROUP_OUTPUT = 30;
    static constexpr int GROUP_META = 31;

    float external_feedback_sum_ = 0.0f;
    int external_feedback_count_ = 0;

    // Core components
    NeuralFieldSystem& system_;
    EvolutionModule& evolution_;
    MemoryManager& memory_;  // ДОБАВЛЕНО
    std::mt19937 rng_;
    std::unique_ptr<WordGenerationEngine> wordEngine_;

    // НОВЫЕ МОДУЛИ ДЛЯ КОНТЕКСТА - ДОБАВЛЕНО
    FactExtractor fact_extractor_;
    UserProfile user_profile_;
    ContextTracker context_tracker_;
    ResponseGenerator response_generator_;

    // Language data
    std::map<std::string, LearnedWord> learned_words_;
    std::vector<std::string> word_history_;
    std::string last_generated_word_;
    std::string current_context_;
    std::vector<int> active_groups_history_;
    
    // Буфер предложений мутаций
    std::vector<PendingMutation> pending_mutations_;

    // Group operations
    std::vector<double> getGroupActivities(int start, int end) const;
    std::vector<int> findActiveGroups(double threshold = 0.6) const;

    // Word generation (теперь через WordGenerationEngine)
    std::string generateWordFromGroups();

    // auto learn
    bool autoLearningActive_ = false;
    std::thread autoLearningThread_;

    // Эволюция
    void requestConnectionMutation(int from, int to, double delta, const std::string& reason);

    // Learning
    void learnWordPattern(const std::string& word, float rating);
    void reinforceActiveGroups(float rating);
    void updateBigramGroups(const std::string& word, float rating);
    void updateSemanticGroups(const std::string& word, float rating);
    void updateContextGroups();

    // Evaluation
    void autoEvaluateGeneratedWord(const std::string& word);
    float autoEvaluateWord(const std::string& word) const;

    // Utilities
    std::vector<std::string> split(const std::string& text);
    std::string toLower(std::string text);
    std::string getRandomResponse();
    std::string getStats();
    NeuronStats computeNeuronStats(double activeThreshold = 0.7, double passiveThreshold = 0.1) const;

    float confidenceThreshold_ = 0.6f;  // порог уверенности
    float factualThreshold_ = 0.5f;      // порог для фактов

        // НОВЫЕ МЕТОДЫ
    std::string extractTopicFromQuestion(const std::string& input);
    std::vector<float> embedText(const std::string& text);
    void saveConversation(const std::string& input, const std::string& response, float confidence);
};