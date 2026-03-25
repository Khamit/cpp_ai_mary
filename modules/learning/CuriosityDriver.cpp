// modules/learning/CuriosityDriver.cpp
#include "CuriosityDriver.hpp"
#include "../../core/NeuralFieldSystem.hpp"
#include "../../modules/lang/LanguageModule.hpp"
#include "../../data/SemanticGraphDatabase.hpp"
#include <cmath>

CuriosityDriver::CuriosityDriver(NeuralFieldSystem& ns, LanguageModule& lang, 
                                 SemanticGraphDatabase& graph)
    : neural_system(ns), language_module(lang), semantic_graph(graph) {}

    std::shared_ptr<CuriosityDriver> createCuriosityDriver(
    NeuralFieldSystem& neural_system,
    LanguageModule& language,
    SemanticGraphDatabase& semantic_graph
) {
    return std::make_shared<CuriosityDriver>(neural_system, language, semantic_graph);
}

std::vector<CuriosityQuestion> CuriosityDriver::generateQuestions() {
    std::vector<CuriosityQuestion> questions;
    
    auto uncertain_meanings = findUncertainMeanings();
    for (auto id : uncertain_meanings) {
        auto node = semantic_graph.getNode(id);
        if (node) {
            CuriosityQuestion q;
            q.question = "What does '" + node->canonical_form + "' mean?";
            q.target_meanings = {id};
            q.importance = 1.0f - node->base_importance;
            questions.push_back(q);
        }
    }
    
    auto potential_relations = explorePotentialRelations();
    for (const auto& [from_id, to_id] : potential_relations) {
        auto from_node = semantic_graph.getNode(from_id);
        auto to_node = semantic_graph.getNode(to_id);
        if (from_node && to_node) {
            CuriosityQuestion q;
            q.question = "Is " + from_node->canonical_form + 
                        " related to " + to_node->canonical_form + "?";
            q.target_meanings = {from_id, to_id};
            q.importance = 0.8f;
            questions.push_back(q);
        }
    }
    return questions;
}

void CuriosityDriver::processAnswer(const std::string& question, const std::string& answer) {
    for (auto& q : question_queue) {
        if (q.question == question && !q.asked) {
            q.asked = true;
            q.user_answer = answer;
            
            auto answer_meanings = language_module.textToMeanings(answer);
            
            for (uint32_t target : q.target_meanings) {
                for (uint32_t ans : answer_meanings) {
                    // Здесь нужно будет добавить правильный тип ребра
                    // semantic_graph.addEdge(target, ans, ...);
                }
            }
            
            float reward = calculateUnderstandingReward(answer_meanings, q.target_meanings);
            neural_system.step(reward, 0);
            
            q.understanding_score = reward;
            
            std::cout << "🎯 Understanding score: " << reward << std::endl;
            break;
        }
    }
}

std::string CuriosityDriver::getNextQuestion() {
    if (question_queue.empty() || curiosity_level < boredom_threshold) {
        question_queue.clear();
        auto new_questions = generateQuestions();
        for (auto& q : new_questions) {
            question_queue.push_back(q);
        }
    }
    
    if (!question_queue.empty()) {
        for (auto& q : question_queue) {
            if (!q.asked) {
                q.asked = true;
                asked_questions.push_back(q.question);
                return q.question;
            }
        }
    }
    
    return "What would you like to talk about?";
}

std::vector<uint32_t> CuriosityDriver::findUncertainMeanings() {
    std::vector<uint32_t> uncertain;
    auto features = neural_system.getFeatures();
    
    for (uint32_t id = 1; id <= 200; id++) {
        auto node = semantic_graph.getNode(id);
        if (!node) continue;
        
        int group = id % 32;
        float activation = 0.0f;
        for (int i = 0; i < 32; i++) {
            activation += features[group * 32 + i] * node->signature[i];
        }
        
        if (activation > 0.3f && activation < 0.7f) {
            uncertain.push_back(id);
        }
    }
    
    return uncertain;
}

std::vector<std::pair<uint32_t, uint32_t>> CuriosityDriver::explorePotentialRelations() {
    std::vector<std::pair<uint32_t, uint32_t>> potential;
    auto recent_meanings = language_module.getRecentMeanings();
    
    for (size_t i = 0; i < recent_meanings.size(); i++) {
        for (size_t j = i + 1; j < recent_meanings.size(); j++) {
            uint32_t id1 = recent_meanings[i];
            uint32_t id2 = recent_meanings[j];
            
            auto edges = semantic_graph.getEdgesFrom(id1);
            bool has_edge = false;
            for (const auto& e : edges) {
                if (e.to_id == id2) has_edge = true;
            }
            
            if (!has_edge) {
                potential.push_back({id1, id2});
            }
        }
    }
    
    return potential;
}

float CuriosityDriver::calculateUnderstandingReward(const std::vector<uint32_t>& answer,
                                                    const std::vector<uint32_t>& target) {
    int matches = 0;
    for (uint32_t a : answer) {
        for (uint32_t t : target) {
            if (a == t) matches++;
        }
    }
    
    float base_reward = static_cast<float>(matches) / target.size();
    float novelty_bonus = 0.2f;
    
    return std::min(1.0f, base_reward + novelty_bonus);
}