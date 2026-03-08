// LanguageModule.hpp - ИСПРАВИТЬ:

#pragma once

#include <string>
#include <vector>
#include <map>
#include <random>
#include <memory>
#include "../../core/Component.hpp"
#include "../../core/NeuralFieldSystem.hpp"

class EvolutionModule; // forward declaration

// Statistics
struct NeuronStats {
    int active = 0;      // phi > 0.7
    int passive = 0;     // 0.1 < phi <= 0.7
    int inactive = 0;    // phi <= 0.1
    double avgActivity = 0.0;
};

// Структура для хранения выученного слова
struct LearnedWord {
    std::string word;
    float frequency = 1.0f;
    float correctness = 0.5f;
    int times_rated = 1;
    std::vector<float> semantic_vector;
    std::vector<float> context_vector;
};

// Структура для биграммы
struct Bigram {
    char first;
    char second;
    float probability;
    int occurrences = 0;
};

// Структура для семантической связи
struct SemanticLink {
    std::string word1;
    std::string word2;
    float strength;
    std::string relation;
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
    explicit LanguageModule(NeuralFieldSystem& system, EvolutionModule& evolution);
    // EvolutionModule& evolution_;  // ← ЭТО БЫЛО ПРОПУЩЕНО!
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

    //
    float getLearningRate() const;
    float calculateSurprise(const std::string& word) const;

    // Оценка
    void addExternalFeedback(float rating) {
        external_feedback_sum_ += rating;
        external_feedback_count_++;
    }
    
    float getExternalFeedbackAvg() const {
        return external_feedback_count_ > 0 ? 
               external_feedback_sum_ / external_feedback_count_ : 0.5f;
    }

    // эмбеддинг
    std::vector<float> getWordEmbedding(const std::string& word) const;

    // мутации
    void applyPendingMutations(); // Будет вызываться эволюционным модулем

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

    NeuronStats computeNeuronStats(double activeThreshold = 0.7, double passiveThreshold = 0.1) const;
    NeuronStats stats;

    // Core components
    NeuralFieldSystem& system_;
    EvolutionModule& evolution_;  // ← ЭТО БЫЛО ПРОПУЩЕНО!
    std::mt19937 rng_;

    // Language data
    std::vector<char> alphabet_;
    std::map<std::string, LearnedWord> learned_words_;
    std::vector<std::string> word_history_;
    std::vector<Bigram> common_bigrams_;
    std::vector<SemanticLink> semantic_links_;
    std::map<std::string, float> collocations_;

    // State
    std::string last_generated_word_;
    std::string current_context_;
    std::vector<int> active_groups_history_;
    
    // Буфер предложений мутаций - ТЕПЕРЬ ЭТО ЧЛЕН КЛАССА
    std::vector<PendingMutation> pending_mutations_;

    // Group operations
    std::vector<double> getGroupActivities(int start, int end) const;
    std::vector<int> findActiveGroups(double threshold = 0.6) const;

    // Word generation
    std::string generateWordFromGroups();
    char selectNextChar(char prevChar);

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
    float calculatePhoneticScore(const std::string& word) const;
    float calculateBigramScore(const std::string& word) const;
    float calculateSemanticCoherence(const std::string& word) const;

    // Utilities
    std::vector<std::string> split(const std::string& text);
    std::string toLower(std::string text);
    std::string getRandomResponse();
    std::string getStats();
};