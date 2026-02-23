// modules/MetaCognitiveModule.hpp
#pragma once
#include "../core/NeuralFieldSystem.hpp"
#include <string>
#include <vector>


class MetaCognitiveModule {
private:
    NeuralFieldSystem& neural_system;
    std::vector<std::string> insights;  // "озарения"
    std::vector<float> insight_quality;
    
public:
    MetaCognitiveModule(NeuralFieldSystem& ns) : neural_system(ns) {}
    
    void think() {
        // Получаем состояние рефлексии
        auto state = neural_system.getReflectionState();
        
        // Если система в замешательстве
        if (state.confusion > 0.7) {
            // Пробуем новую стратегию
            tryNewStrategy();
        }
        
        // Если любопытство высокое
        if (state.curiosity > 0.6) {
            // Исследуем новые области
            explore();
        }
        
        // Если удовлетворенность высокая
        if (state.satisfaction > 0.8) {
            // Формируем "озарение"
            std::string insight = generateInsight();
            insights.push_back(insight);
            insight_quality.push_back(state.satisfaction);
            
            // Сохраняем важные инсайты
            if (insight_quality.back() > 0.9) {
                saveInsight(insight);
            }
        }
        
        // Ставим новую цель на основе рефлексии
        if (state.confidence < 0.3) {
            neural_system.setGoal("understand_self");
        } else if (state.curiosity > 0.5) {
            neural_system.setGoal("explore_new");
        }
    }
    
private:
    void tryNewStrategy() {
        // Изменяем паттерны активности в группе метапознания
        for (int i = NeuralFieldSystem::METACOGNITION_START; 
             i < NeuralFieldSystem::METACOGNITION_END; i += 10) {
            
            double new_value = (rand() % 100) / 100.0;
            neural_system.setPhi(i, new_value);
        }
    }
    
    void explore() {
        // Активируем случайные нейроны для исследования
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, 1023);
        
        for (int i = 0; i < 10; i++) {
            int idx = dist(gen);
            double current = neural_system.getPhi()[idx];
            neural_system.setPhi(idx, current + 0.1);
        }
    }
    
    std::string generateInsight() {
        // Генерируем текстовое описание текущего состояния
        auto state = neural_system.getReflectionState();
        
        std::string insight = "I feel ";
        if (state.confidence > 0.7) insight += "confident";
        else if (state.confidence > 0.4) insight += "uncertain";
        else insight += "confused";
        
        insight += " and ";
        if (state.curiosity > 0.6) insight += "curious";
        else insight += "satisfied";
        
        return insight;
    }
    
    void saveInsight(const std::string& insight) {
        static int counter = 0;
        std::ofstream file("insights/insight_" + std::to_string(counter++) + ".txt");
        file << "Insight: " << insight << std::endl;
        file << "Quality: " << insight_quality.back() << std::endl;
        file.close();
    }
};