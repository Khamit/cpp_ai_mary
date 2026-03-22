// modules/learning/CurriculumTrainer.hpp
#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include "NeuralTrainer.hpp"
#include "TrainingExampleManager.hpp"
#include "ConceptMasteryEvaluator.hpp"

class NeuralFieldSystem;
class SemanticGraphDatabase;

class CurriculumTrainer {
private:
    NeuralTrainer& trainer;
    TrainingExampleManager& example_manager;
    ConceptMasteryEvaluator& mastery_evaluator;
    NeuralFieldSystem& neural_system_;
    SemanticGraphDatabase& semantic_graph_;
    
    bool training_active_ = false;
    int epoch_ = 0;
    
public:
    CurriculumTrainer(NeuralTrainer& t, TrainingExampleManager& em, 
                      ConceptMasteryEvaluator& me, NeuralFieldSystem& ns, SemanticGraphDatabase& graph)
        : trainer(t), example_manager(em), mastery_evaluator(me), neural_system_(ns), semantic_graph_(graph){}
    
void trainAbstractionLevel(int min_abs, int max_abs, int steps) {
    auto level_examples = example_manager.getExamplesByAbstraction(min_abs, max_abs);
    if (level_examples.empty()) return;
    
    auto& groups = trainer.getNeuralSystem().getGroups();
    
    float elite_readiness = 0.0f;
    int elite_count = 0;
    
    for (int g = 16; g <= 21 && g < groups.size(); g++) {
        for (int i = 0; i < 32; i++) {
            if (groups[g].getOrbitLevel(i) >= 3) {
                elite_count++;
                float stability = groups[g].getLocalStability(i);
                elite_readiness += stability;
            }
        }
    }
    
    if (elite_count > 0) {
        elite_readiness /= elite_count;
    } else {
        elite_readiness = 0.3f;
    }
    
    int actual_steps = steps;
    if (elite_readiness < 0.5f && min_abs > 5) {
        std::cout << "  Elite neurons not ready (" << std::fixed << std::setprecision(2)
                  << elite_readiness << "), simplifying curriculum\n";
        actual_steps = steps / 2;
    }
    
    std::mt19937 rng(std::random_device{}());
    
    for (int i = 0; i < actual_steps && training_active_; i++) {
        int idx = rng() % level_examples.size();
        const auto& example = level_examples[idx];
        
        // ===== ИСПРАВЛЕНО: вызываем реальное обучение =====
        bool success = trainer.trainOnExample(example, &example_manager, true);
        
        if (i % 50 == 0) {
            std::cout << "\r  Abstraction " << min_abs << "-" << max_abs 
                      << " progress: " << (i * 100 / actual_steps) << "%" 
                      << " (elite readiness: " << std::fixed << std::setprecision(2)
                      << elite_readiness << ")" << std::flush;
        }
    }
    std::cout << std::endl;
}
    
    void trainCauseEffectAdvanced(int steps) {
        auto cause_effect_examples = example_manager.getExamplesWithCauseEffect();
        if (cause_effect_examples.empty()) return;
        
        std::mt19937 rng(std::random_device{}());
        
        for (int i = 0; i < steps && training_active_; i++) {
            int idx = rng() % cause_effect_examples.size();
            const auto& ex = cause_effect_examples[idx];
            
            if (ex.cause_effect.size() >= 2) {
                auto [cause_id, effect_id] = ex.cause_effect[0];
                
                trainer.activateMeaning(cause_id, 1.0f);
                
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                
                trainer.activateMeaning(effect_id, 0.8f);
                
                trainer.trainRelation(cause_id, effect_id, SemanticEdge::Type::CAUSES);
            }
            
            if (i % 100 == 0) {
                std::cout << "\r  Cause-effect progress: " << (i * 100 / steps) << "%" << std::flush;
            }
        }
        std::cout << std::endl;
    }
    
    void trainContextAwareness(int steps) {
        std::vector<std::string> contexts = {
            "conversation", "command", "query", "system", 
            "security", "movement", "perception"
        };
        
        std::mt19937 rng(std::random_device{}());
        
        for (int i = 0; i < steps && training_active_; i++) {
            std::string context = contexts[rng() % contexts.size()];
            
            // Здесь должна быть логика обучения с контекстом
            (void)context; // подавление предупреждения
            
            if (i % 200 == 0) {
                std::cout << "\r  Context progress: " << (i * 100 / steps) << "%" << std::flush;
            }
        }
        std::cout << std::endl;
    }
    
    void trainMetaLearning(int steps) {
        std::cout << "\nLearning how to learn...\n";
        
        for (int i = 0; i < steps && training_active_; i++) {
            if (trainer.getAverageAccuracy() > 0.01f) {
                // Адаптация скорости обучения на основе точности
                float new_rate = trainer.getLearningRate();
                
                if (trainer.getAverageAccuracy() < 0.1f) {
                    new_rate *= 1.05f;
                } else {
                    new_rate *= 0.99f;
                }
                
                new_rate = std::clamp(new_rate, 0.001f, 0.1f);
                trainer.setLearningRate(new_rate);
            }
            
            epoch_++;
        }
    }
    
    void runFullTraining() {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "STARTING FULL SEMANTIC TRAINING" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        training_active_ = true;
        auto start_time = std::chrono::steady_clock::now();
        
        trainer.healStuckNeurons();
        trainer.preventStagnation();
        
        std::cout << "\nPhase 1: Concrete concepts (abstraction 1-3)" << std::endl;
        trainAbstractionLevel(1, 3, 2000);
        trainer.healStuckNeurons();
        
        std::cout << "\nPhase 2: Actions and relations (abstraction 3-5)" << std::endl;
        trainAbstractionLevel(3, 5, 3000);
        trainer.healStuckNeurons();
        
        std::cout << "\nPhase 3: Cause and effect" << std::endl;
        trainCauseEffectAdvanced(2000);
        trainer.healStuckNeurons();
        
        std::cout << "\nPhase 4: Abstract concepts (abstraction 6-8)" << std::endl;
        trainAbstractionLevel(6, 8, 2000);
        trainer.healStuckNeurons();
        
        std::cout << "\nPhase 5: Contextual understanding" << std::endl;
        trainContextAwareness(2000);
        trainer.healStuckNeurons();
        
        std::cout << "\nPhase 6: Meta-learning" << std::endl;
        trainMetaLearning(1000);
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::minutes>(end_time - start_time);
        
        training_active_ = false;
        
        float final_accuracy = trainer.getAverageAccuracy();
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "TRAINING COMPLETE!" << std::endl;
        std::cout << "   Total steps: " << trainer.getTotalSteps() << std::endl;
        std::cout << "   Epochs: " << epoch_ << std::endl;
        std::cout << "   Duration: " << duration.count() << " minutes" << std::endl;
        std::cout << "   Final accuracy: " << std::fixed << std::setprecision(1) 
                  << final_accuracy << "%" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
    }
    
    void stopTraining() { training_active_ = false; }
    bool isTrainingActive() const { return training_active_; }
    
    void analyzeWeaknesses() {
        std::map<std::string, float> category_accuracy;
        std::map<std::string, int> category_count;
        std::map<int, float> abstraction_accuracy;
        std::map<int, int> abstraction_count;
        
        for (uint32_t id = 1; id <= 614; id++) {
            auto node = semantic_graph_.getNode(id);
            if (!node) continue;
            
            float mastery = mastery_evaluator.getConceptMastery(id);
            
            if (mastery < 0.4f) {
                category_accuracy[node->primary_category] += mastery;
                category_count[node->primary_category]++;
                abstraction_accuracy[node->abstraction_level] += mastery;
                abstraction_count[node->abstraction_level]++;
            }
        }
        
        std::cout << "\n=== WEAKNESSES ANALYSIS ===\n";
        
        std::cout << "\nWeak categories:\n";
        for (const auto& [cat, total] : category_accuracy) {
            if (category_count[cat] > 0) {
                float avg = total / category_count[cat];
                std::cout << "  " << cat << ": " << std::fixed << std::setprecision(1) 
                          << (avg * 100) << "% (" << category_count[cat] << " concepts)\n";
            }
        }
        
        std::cout << "\nWeak abstraction levels:\n";
        for (int level = 1; level <= 10; level++) {
            if (abstraction_count[level] > 0) {
                float avg = abstraction_accuracy[level] / abstraction_count[level];
                std::cout << "  Level " << level << ": " << std::fixed << std::setprecision(1) 
                          << (avg * 100) << "%\n";
            }
        }
    }
};