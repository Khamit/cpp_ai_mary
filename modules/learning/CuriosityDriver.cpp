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

// В CuriosityDriver.cpp
std::vector<CuriosityQuestion> CuriosityDriver::generateQuestions() {
    std::vector<CuriosityQuestion> questions;
    
    // НЕ спрашиваем про uncertain meanings из графа
    // Вместо этого спрашиваем про паттерны, которые система сама нашла
    
    auto& groups = neural_system.getGroups();
    
    // Находим элитные нейроны с высокой энергией, но без привязки к графу
    for (int g = 16; g <= 21; g++) {
        for (int i = 0; i < 32; i++) {
            if (groups[g].getOrbitLevel(i) >= 3) {
                float energy = groups[g].getNeuronEnergy(i);
                
                // Проверяем, связан ли этот нейрон с известным концептом
                bool known = false;
                for (uint32_t concept_id = 1; concept_id <= 614; concept_id++) {
                    auto node = semantic_graph.getNode(concept_id);
                    if (node) {
                        float affinity = 0.0f;
                        int group_idx = (concept_id % 6);
                        if (group_idx == (g - 16)) {
                            for (int j = 0; j < 32; j++) {
                                affinity += node->signature[j + group_idx * 32] * 
                                           groups[g].getPhi()[j];
                            }
                        }
                        if (affinity > 0.7f) {
                            known = true;
                            break;
                        }
                    }
                }
                
                if (!known && energy > 1.0f) {
                    CuriosityQuestion q;
                    q.question = "What pattern is this neuron representing? (neuron " 
                               + std::to_string(i) + " in group " + std::to_string(g) + ")";
                    q.target_neurons.push_back({g, i});
                    q.importance = energy / 5.0f;
                    questions.push_back(q);
                }
            }
        }
    }
    
    return questions;
}

// обработка target_neurons
void CuriosityDriver::processAnswer(const std::string& question, const std::string& answer) {
    for (auto& q : question_queue) {
        if (q.question == question && !q.asked) {
            q.asked = true;
            q.user_answer = answer;
            
            // Обработка target_meanings (существующая логика)
            auto answer_meanings = language_module.textToMeanings(answer);
            
            for (uint32_t target : q.target_meanings) {
                for (uint32_t ans : answer_meanings) {
                    // Добавляем связь в граф
                    semantic_graph.addEdge(target, ans, SemanticEdge::Type::RELATED_TO, 0.5f);
                }
            }
            
            // НОВОЕ: обработка target_neurons (для эмерджентных паттернов)
            if (!q.target_neurons.empty()) {
                auto& groups = neural_system.getGroupsNonConst();
                
                for (const auto& [group_idx, neuron_idx] : q.target_neurons) {
                    // Повышаем орбиту нейрона, если ответ был полезным
                    if (!answer_meanings.empty()) {
                        // Повышаем орбиту
                        if (groups[group_idx].getOrbitLevel(neuron_idx) < 4) {
                            groups[group_idx].publicPromoteToBaseOrbit(neuron_idx);
                            std::cout << "  Promoted neuron (" << group_idx << "," 
                                      << neuron_idx << ") based on user answer" << std::endl;
                        }
                        
                        // Усиливаем связи с семантическими группами
                        for (uint32_t meaning_id : answer_meanings) {
                            int semantic_group = 16 + (meaning_id % 6);
                            neural_system.strengthenInterConnection(group_idx, semantic_group, 0.1f);
                        }
                    }
                }
            }
            
            float reward = calculateUnderstandingReward(answer_meanings, q.target_meanings);
            neural_system.step(reward, 0);
            
            q.understanding_score = reward;
            
            std::cout << "Understanding score: " << reward << std::endl;
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