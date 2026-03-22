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
        
        // 2. Даем нейросети время "подумать"
        for (int i = 0; i < 8; i++) {  // увеличил количество шагов для стабилизации
            neural_system.step(0.0f, i);
        }
        
        // 3. Извлекаем результирующий ПУТЬ (а не набор)
        auto output_path = semantic_manager.extractMeaningPath(5);  // максимум 5 смыслов в пути
        
        // 4. Предсказываем следующий смысл (причина -> следствие)
        auto predicted = semantic_manager.predictNextMeanings(output_path);
        
        // 5. Сохраняем в историю
        thought_history.push_back(output_path);
        if (thought_history.size() > MAX_HISTORY) {
            thought_history.pop_front();
        }
        
        // 6. Если предсказание сильное, добавляем его в конец пути
        if (!predicted.empty()) {
            // Добавляем только если предсказание связано с последним элементом пути
            uint32_t last = output_path.back();
            for (uint32_t pred : predicted) {
                // Проверяем связь через граф
                auto edges = semantic_manager.getSemanticGraph()->getEdgesFrom(last);
                bool has_relation = false;
                for (const auto& edge : edges) {
                    if (edge.to_id == pred) {
                        has_relation = true;
                        break;
                    }
                }
                if (has_relation) {
                    output_path.push_back(pred);
                    break;  // добавляем только одно предсказание
                }
            }
        }
        
        // Очищаем кэш отношений пути
        semantic_manager.clearPathRelations();
        
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