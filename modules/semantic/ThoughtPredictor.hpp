// modules/semantic/ThoughtPredictor.hpp
#pragma once
#include "SemanticManager.hpp"
#include "../../core/NeuralFieldSystem.hpp"
#include <deque>

/**
 * @class ThoughtPredictor
 * @brief Предсказание последовательности мыслей (причина-следствие)
 */
class ThoughtPredictor {
private:
    SemanticManager& semantic_manager;
    NeuralFieldSystem& neural_system;
    
    std::deque<std::vector<uint32_t>> thought_history;
    static constexpr size_t MAX_HISTORY = 20;
    
public:
    ThoughtPredictor(SemanticManager& sm, NeuralFieldSystem& ns) 
        : semantic_manager(sm), neural_system(ns) {}
    
    // Основной цикл мышления
    std::vector<uint32_t> think(const std::vector<uint32_t>& input_meanings) {
        // 1. Проецируем входные смыслы в нейросеть
        semantic_manager.projectToSystem(input_meanings);
        
        // 2. НОВОЕ: применяем "интенцию" — bias в динамике
        if (!input_meanings.empty()) {
            // Вычисляем "целевой" вектор на основе входных смыслов
            std::vector<float> target_pattern(192, 0.0f); // 6 групп * 32 нейрона
            
            for (uint32_t mid : input_meanings) {
                auto node = semantic_manager.getGranule(mid);
                if (node) {
                    int group_idx = semantic_manager.getGroupForConcept(mid);
                    const auto& sig = node->getSignature();
                    for (int i = 0; i < 32; i++) {
                        target_pattern[group_idx * 32 + i] += sig[i];
                    }
                }
            }
            
            // Нормализуем target_pattern
            float max_val = *std::max_element(target_pattern.begin(), target_pattern.end());
            if (max_val > 0) {
                for (auto& v : target_pattern) v /= max_val;
            }
            
            // ПРИМЕНЯЕМ ЦЕЛЕВОЙ ПАТТЕРН К НЕЙРОСЕТИ
            neural_system.applyTargetPattern(target_pattern);
        }
        
        // 3. Даем нейросети время "подумать"
        for (int i = 0; i < 8; i++) {
            neural_system.step(0.0f, i);
        }
        
        // 4. Извлекаем результирующий путь
        auto output_path = semantic_manager.extractMeaningPath(5);
        
        return output_path;
    }
        
    // Обучение на правильной последовательности
    void learnFromSequence(const std::vector<std::vector<uint32_t>>& sequence) {
        for (const auto& step : sequence) {
            semantic_manager.trainOnThoughtSequence(step);
        }
    }
    
    // Проверка на нехватку ресурсов
    bool needsMoreNeurons() const {
        // Проверяем, слишком ли много смыслов активно одновременно
        if (!thought_history.empty()) {
            const auto& last = thought_history.back();
            if (last.size() > 8) { // больше 8 смыслов одновременно = перегрузка
                return true;
            }
        }
        return false;
    }
    
    // Получить мета-мысль о собственных возможностях
    std::vector<uint32_t> getSelfAssessment() {
        std::vector<uint32_t> assessment;
        
        if (needsMoreNeurons()) {
            // "мои нейроны недостаточны для подобных рассуждений"
            uint32_t insufficient_id = semantic_manager.getMeaningId("insufficient");
            uint32_t neurons_id = 3; // system_capability
            if (insufficient_id) assessment.push_back(insufficient_id);
            if (neurons_id) assessment.push_back(neurons_id);
        }
        
        return assessment;
    }
};