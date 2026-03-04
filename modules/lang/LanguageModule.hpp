#pragma once

#include <string>
#include <vector>
#include <cctype>
#include <iostream>
#include <algorithm>
#include <random>
#include <cmath>
#include <fstream>
#include <unordered_map>
#include <map>
#include <memory>
#include "../../core/NeuralFieldSystem.hpp"

// Структура для хранения слова с семантическим вектором
struct LearnedWord {
    std::string word;
    float frequency;
    float correctness;
    std::vector<float> semantic_vector;  // 32-мерный вектор (активности групп)
    std::vector<float> context_vector;    // контекст использования
    int times_rated;
    std::vector<std::string> common_phrases; // частые словосочетания
};

// Структура для биграммы с весом
struct Bigram {
    char first;
    char second;
    float probability;
    int occurrences;
};

// Структура для семантической связи между словами
struct SemanticLink {
    std::string word1;
    std::string word2;
    float strength;           // сила связи
    std::string relation;     // тип связи (синоним, антоним, часть речи и т.д.)
};

class LanguageModule {
public:
    explicit LanguageModule(NeuralFieldSystem& system);
    int getLearnedWordsCount() const { return learned_words_.size(); }
    // Основные методы
    std::string process(const std::string& input);
    void giveFeedback(float rating, bool autoFeedback = false);
    void setContext(const std::string& context);
    
    // Методы для автоматической оценки
    void autoEvaluateGeneratedWord(const std::string& word);
    void updateSemanticLinks(const std::string& word, float rating);
    
    // Сохранение/загрузка
    void saveAll();
    void loadAll();

    double getLanguageFitness() const {
        if (word_history_.empty()) return 0.5;
        
        float totalScore = 0;
        int count = 0;
        for (const auto& word : word_history_) {
            totalScore += autoEvaluateWord(word);
            count++;
        }
        return count > 0 ? totalScore / count : 0.5;
    }

private:
    NeuralFieldSystem& system_;
    std::mt19937 rng_;
    
    // ---- Группы и их специализация ----
    static constexpr int GROUP_PHONETICS_START = 0;      // 0-3
    static constexpr int GROUP_PHONETICS_END = 4;
    static constexpr int GROUP_BIGRAMS_START = 4;        // 4-7
    static constexpr int GROUP_BIGRAMS_END = 8;
    static constexpr int GROUP_MORPHEMES_START = 8;      // 8-15
    static constexpr int GROUP_MORPHEMES_END = 16;
    static constexpr int GROUP_SEMANTIC_START = 16;      // 16-23
    static constexpr int GROUP_SEMANTIC_END = 24;
    static constexpr int GROUP_SYNTAX_START = 24;        // 24-27
    static constexpr int GROUP_SYNTAX_END = 28;
    static constexpr int GROUP_CONTEXT_START = 28;       // 28-29
    static constexpr int GROUP_CONTEXT_END = 30;
    static constexpr int GROUP_OUTPUT = 30;               // выходная группа
    static constexpr int GROUP_META = 31;                  // мета-группа оценки
    
    // ---- Языковые данные ----
    std::vector<char> alphabet_;
    std::map<std::string, LearnedWord> learned_words_;
    std::vector<std::string> word_history_;
    std::vector<std::string> context_history_;
    
    // Биграммы
    std::vector<Bigram> common_bigrams_;
    
    // Семантические связи между словами
    std::vector<SemanticLink> semantic_links_;
    
    // Часто используемые словосочетания (коллокации)
    std::map<std::string, float> collocations_;
    
    // Текущее состояние
    std::string last_input_;
    std::string last_generated_word_;
    std::string current_context_;
    std::vector<int> active_groups_history_; // какие группы были активны
    
    // ---- Вспомогательные методы для работы с группами ----
    std::vector<double> getGroupActivities(int startGroup, int endGroup) const;
    void activateGroup(int groupIndex, double strength);
    void strengthenInterGroupConnection(int fromGroup, int toGroup, double delta);
    std::vector<int> findActiveGroups(double threshold = 0.6) const;
    
    // ---- Генерация слова через группы ----
    std::string generateWordFromGroups();
    char selectNextChar(char prevChar, const std::vector<int>& activeGroups);
    
    // ---- Обучение через группы ----
    void learnWordPattern(const std::string& word, float rating);
    void reinforceActiveGroups(float rating);
    void updateBigramGroups(const std::string& word, float rating);
    void updateSemanticGroups(const std::string& word, float rating);
    void updateContextGroups();  // ОДИН РАЗ
    
    // ---- Автоматическая оценка ----
    float autoEvaluateWord(const std::string& word) const;           
    float calculatePhoneticScore(const std::string& word) const;     
    float calculateBigramScore(const std::string& word) const;       
    float calculateSemanticCoherence(const std::string& word) const;
    
    // ---- Коллокации и частотность ----
    void updateCollocations(const std::string& word);
    std::string suggestNextWord(const std::string& currentWord);  // ОДИН РАЗ
    
    // ---- Сохранение/загрузка ----
    void saveLearnedWords();
    void saveSemanticLinks();
    void saveCollocations();
    
    // ---- Совместимость со старым кодом ----
    std::string getWordMeaning(const std::string& word);
    std::string getStats();
    std::string getRandomResponse();
    std::string decodeLastWord();
    void encodeToNeuralField(const std::string& text);
    
    // Вспомогательные методы
    std::vector<std::string> split(const std::string& text);
    std::string toLower(std::string text);
};