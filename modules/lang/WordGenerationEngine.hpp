#pragma once
#include <string>
#include <vector>
#include <map>
#include <random>
#include <memory>

class NeuralFieldSystem;
class EvolutionModule;

struct Bigram {
    char first;
    char second;
    float probability;
    int occurrences = 0;
    
    Bigram() : first(0), second(0), probability(0.0f), occurrences(0) {}
    Bigram(char f, char s, float p, int o = 0) : first(f), second(s), probability(p), occurrences(o) {}
};

struct SemanticLink {
    std::string word1;
    std::string word2;
    float strength;
    std::string relation;
    
    SemanticLink() : strength(0.0f) {}
    SemanticLink(const std::string& w1, const std::string& w2, float s, const std::string& r)
        : word1(w1), word2(w2), strength(s), relation(r) {}
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

class WordGenerationEngine {
public:
    WordGenerationEngine();

    // Генерация слова через группы нейронов
    std::string generateWordFromGroups(
        NeuralFieldSystem& system,
        std::mt19937& rng,
        std::vector<int>& activeGroupsHistory,
        std::vector<std::string>& wordHistory,
        std::string& lastWord,
        const std::map<std::string, LearnedWord>& learnedWords
    );

    // Обучение на слове
    void learnWordPattern(
        const std::string& word, 
        float rating,
        const std::vector<float>& semanticActivity,
        const std::vector<float>& contextActivity
    );

    // Оценка слова
    float autoEvaluateWord(
        const std::string& word,
        const std::map<std::string, LearnedWord>& learnedWords,
        const std::vector<std::string>& wordHistory
    ) const;

    // Константы групп (для совместимости)
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

private:
    std::vector<char> alphabet_;
    std::vector<Bigram> common_bigrams_;
    std::vector<SemanticLink> semantic_links_;
    std::map<std::string, float> collocations_;

    // Внутренние методы
    char selectNextChar(
        char prevChar, 
        std::mt19937& rng,
        const std::map<std::string, LearnedWord>& learnedWords
    ) const;
    
    std::vector<double> getGroupActivities(
        NeuralFieldSystem& system,
        int start, 
        int end
    ) const;
    
    std::vector<int> findActiveGroups(
        NeuralFieldSystem& system,
        double threshold = 0.6
    ) const;

    // Оценочные методы
    float calculatePhoneticScore(const std::string& word) const;
    float calculateBigramScore(const std::string& word) const;
    float calculateSemanticCoherence(
        const std::string& word,
        const std::vector<std::string>& history
    ) const;
};