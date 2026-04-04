// CuriosityDriver.cpp (исправленный)
#include "CuriosityDriver.hpp"
#include "../../core/NeuralFieldSystem.hpp"
#include "../lang/LanguageModule.hpp"
#include "../../data/DynamicSemanticMemory.hpp"
#include <cmath>
#include <algorithm>

std::vector<CuriosityQuestion> CuriosityDriver::generateQuestions() {
    std::vector<CuriosityQuestion> questions;
    
    if (!dynamic_memory_) return questions;
    
    auto& groups = neural_system.getGroups();
    
    // Находим элитные нейроны с высокой энергией
    for (int g = 16; g <= 21; g++) {
        for (int i = 0; i < 32; i++) {
            if (groups[g].getOrbitLevel(i) >= 3) {
                float energy = groups[g].getNeuronEnergy(i);
                
                if (energy > 1.5f) {
                    CuriosityQuestion q;
                    q.question = "What does this pattern mean? (neuron " 
                               + std::to_string(i) + " in group " + std::to_string(g) + ")";
                    q.target_neurons.push_back({g, i});
                    q.importance = std::min(1.0f, energy / 5.0f);
                    questions.push_back(q);
                }
            }
        }
    }
    
    return questions;
}

void CuriosityDriver::processAnswer(const std::string& question, const std::string& answer) {
    for (auto& q : question_queue) {
        if (q.question == question && !q.asked) {
            q.asked = true;
            q.user_answer = answer;
            
            // Обрабатываем ответ через динамическую память
            if (dynamic_memory_) {
                auto answer_meanings = dynamic_memory_->processText(answer, "curiosity");
                
                // Обработка target_neurons
                if (!q.target_neurons.empty()) {
                    auto& groups = neural_system.getGroupsNonConst();
                    
                    for (const auto& [group_idx, neuron_idx] : q.target_neurons) {
                        if (!answer_meanings.empty()) {
                            // Повышаем орбиту нейрона
                            if (groups[group_idx].getOrbitLevel(neuron_idx) < 4) {
                                groups[group_idx].publicPromoteToBaseOrbit(neuron_idx);
                                std::cout << "  Promoted neuron (" << group_idx << "," 
                                          << neuron_idx << ") based on user answer" << std::endl;
                            }
                            
                            // Усиливаем связи с семантическими группами
                            for (uint32_t meaning_id : answer_meanings) {
                                // Находим группу для этого смысла
                                int semantic_group = 16 + (meaning_id % 6);
                                neural_system.strengthenInterConnection(group_idx, semantic_group, 0.1f);
                            }
                        }
                    }
                }
                
                // Применяем обратную связь
                dynamic_memory_->applyUserFeedback("curiosity", q.importance, answer_meanings);
            }
            
            q.understanding_score = q.importance;
            std::cout << "Understanding score: " << q.understanding_score << std::endl;
            break;
        }
    }
}

std::string CuriosityDriver::getNextQuestion() {
    if (question_queue.empty()) {
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

float CuriosityDriver::calculateUnderstandingReward(const std::string& answer, 
                                                     const CuriosityQuestion& q) {
    // Упрощённая версия
    return q.importance;
}