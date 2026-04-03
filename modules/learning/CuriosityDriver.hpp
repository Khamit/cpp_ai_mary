// modules/learning/CuriosityDriver.hpp
#pragma once
#include <vector>
#include <deque>
#include <random>
#include <chrono>
#include <string>
#include <algorithm>
#include <iostream>
#include <memory>

// Forward declarations вместо includes
class NeuralFieldSystem;
class LanguageModule;
class SemanticGraphDatabase;
struct SemanticEdge;  // Добавляем forward declaration

struct CuriosityQuestion {
    std::string question;
    std::vector<uint32_t> target_meanings;
    std::vector<std::pair<int, int>> target_neurons;  // <-- ДОБАВИТЬ
    float importance;
    bool asked = false;
    std::string user_answer;
    float understanding_score = 0.0f;
};

class CuriosityDriver {
private:
    NeuralFieldSystem& neural_system;
    LanguageModule& language_module;
    SemanticGraphDatabase& semantic_graph;
    
    std::deque<CuriosityQuestion> question_queue;
    std::vector<std::string> asked_questions;
    
    float curiosity_level = 1.0f;
    float boredom_threshold = 0.3f;
    int questions_per_session = 3;
    
    std::mt19937 rng{std::random_device{}()};
    
public:
    std::shared_ptr<CuriosityDriver> createCuriosityDriver(
        NeuralFieldSystem& neural_system,
        LanguageModule& language,
        SemanticGraphDatabase& semantic_graph
    );
    
    CuriosityDriver(NeuralFieldSystem& ns, LanguageModule& lang, 
                    SemanticGraphDatabase& graph);
    
    std::vector<CuriosityQuestion> generateQuestions();
    void processAnswer(const std::string& question, const std::string& answer);
    std::string getNextQuestion();
    
private:
    std::vector<uint32_t> findUncertainMeanings();
    std::vector<std::pair<uint32_t, uint32_t>> explorePotentialRelations();
    float calculateUnderstandingReward(const std::vector<uint32_t>& answer,
                                       const std::vector<uint32_t>& target);
};