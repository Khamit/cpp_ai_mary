#pragma once
#include "../core/NeuralFieldSystem.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <random>

class MetaCognitiveModule {
private:
    NeuralFieldSystem& neural_system;
    std::vector<std::string> insights;
    std::vector<float> insight_quality;
    std::mt19937 rng;
    
public:
    MetaCognitiveModule(NeuralFieldSystem& ns) 
        : neural_system(ns), rng(std::random_device{}()) {}
    
    void think() {
        // Получаем состояние рефлексии
        auto state = neural_system.getReflectionState();
        
        // Если система в замешательстве
        if (state.confusion > 0.7) {
            tryNewStrategy();
        }
        
        // Если любопытство высокое
        if (state.curiosity > 0.6) {
            explore();
        }
        
        // Если удовлетворенность высокая
        if (state.satisfaction > 0.8) {
            std::string insight = generateInsight();
            insights.push_back(insight);
            insight_quality.push_back(state.satisfaction);
            
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
        // В новой архитектуре группы 16-23 - семантические
        for (int i = 16; i < 24; i += 2) {
            // Влияем через межгрупповые связи
            double new_value = (rng() % 100) / 100.0;
            neural_system.strengthenInterConnection(31, i, new_value * 0.1);
        }
    }
    
    void explore() {
        // Активируем случайные группы для исследования
        std::uniform_int_distribution<> dist(0, 31);
        for (int i = 0; i < 5; i++) {
            int idx = dist(rng);
            neural_system.strengthenInterConnection(31, idx, 0.05);
        }
    }
    
    std::string generateInsight() {
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
        std::filesystem::create_directories("insights");
        std::ofstream file("insights/insight_" + std::to_string(counter++) + ".txt");
        file << "Insight: " << insight << std::endl;
        file << "Quality: " << insight_quality.back() << std::endl;
        file.close();
    }
};