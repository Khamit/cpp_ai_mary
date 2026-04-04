// CuriosityDriver.hpp (исправленный)
#pragma once
#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <utility>
#include "../lang/LanguageModule.hpp"
#include "../../core/NeuralFieldSystem.hpp"
#include "../../data/DynamicSemanticMemory.hpp"

class NeuralFieldSystem;
class LanguageModule;

struct CuriosityQuestion {
    std::string question;
    std::vector<std::pair<int, int>> target_neurons;  // (group, neuron)
    float importance = 0.5f;
    bool asked = false;
    std::string user_answer;
    float understanding_score = 0.0f;
};

class CuriosityDriver {
private:
    NeuralFieldSystem& neural_system;
    LanguageModule& language_module;
    DynamicSemanticMemory* dynamic_memory_;
    
    std::deque<CuriosityQuestion> question_queue;
    std::vector<std::string> asked_questions;
    
    float curiosity_level = 0.7f;
    float boredom_threshold = 0.3f;
    
    std::vector<CuriosityQuestion> generateQuestions();
    
public:
    CuriosityDriver(NeuralFieldSystem& ns, LanguageModule& lang, DynamicSemanticMemory* memory)
        : neural_system(ns), language_module(lang), dynamic_memory_(memory) {}
    
    std::string getNextQuestion();
    void processAnswer(const std::string& question, const std::string& answer);
    float calculateUnderstandingReward(const std::string& answer, const CuriosityQuestion& q);
};