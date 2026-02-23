// modules/lang/LanguageModule.hpp
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
#include <set>
#include <map>
#include "../../core/NeuralFieldSystem.hpp"

// Структура для хранения выученного слова
struct LearnedWord {
    std::string word;
    float frequency;                 // как часто используется
    float correctness;                // насколько правильное (0-1)
    std::vector<float> neural_pattern; // паттерн в нейронах
    int times_rated;                   // сколько раз оценивали
    float letter_probabilities[26];    // вероятности букв для этого слова
};

class LanguageModule {
public:
    explicit LanguageModule(NeuralFieldSystem& system);
    std::string process(const std::string& input);
    void normalizeBigrams();
    void saveBigrams();
    void loadBigrams();
    void giveFeedback(float rating);

private:
    NeuralFieldSystem& system_;
    std::mt19937 rng_;
    std::uniform_int_distribution<> word_length_dist_;
    std::uniform_int_distribution<> letter_dist_;
    std::vector<char> alphabet_;
    float base_letter_probs_[26];

    std::string last_input_;
    std::unordered_map<std::string, LearnedWord> learned_words_;
    std::vector<std::string> word_history_;

    float bigram_matrix_[26][26];

    void initializeEnglishBigrams();
    std::string generateWordFromNeurons();
    void enhanceNeuralPattern(const std::string& word);
    void weakenWordPattern(const std::string& word);
    void updateLetterProbabilities(const std::string& word, float rating);
    void saveProbabilities();
    void learnFromFeedback(const std::string& word, float rating);
    void saveWord(const std::string& word, float rating);
    void saveLearnedWords();
    void saveNeuralPatterns();
    void loadLearnedWords();
    void loadProbabilities();
    void loadNeuralPatterns();
    std::string getWordMeaning(const std::string& word);
    std::string getStats();
    std::string getRandomResponse();
    std::string decodeLastWord();
    void encodeToNeuralField(const std::string& text);
};