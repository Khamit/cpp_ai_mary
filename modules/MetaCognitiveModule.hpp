// modules/MetaCognitiveModule.hpp
#pragma once

#include "../core/NeuralFieldSystem.hpp"
#include "../core/Component.hpp" 
#include <string>
#include <vector>
#include <fstream>
#include <random>
#include <filesystem>
#include <memory>  // для std::unique_ptr

class MetaCognitiveModule : public Component {
private:
    NeuralFieldSystem& neural_system;
    std::vector<std::string> insights;
    std::vector<float> insight_quality;
    std::mt19937 rng;
    
public:
    MetaCognitiveModule(NeuralFieldSystem& ns) 
        : neural_system(ns), rng(std::random_device{}()) {}
    
    
    std::string getName() const override { return "MetaCognitiveModule"; }
    
    bool initialize(const Config& config) override {
        std::cout << "MetaCognitiveModule initialized" << std::endl;
        return true;
    }
    
    void shutdown() override {
        std::cout << "MetaCognitiveModule shutting down" << std::endl;
    }
    
    void update(float dt) override {
        think();
    }
    // ИСПРАВЛЕННЫЙ МЕТОД think
void think() {
    auto state = neural_system.getReflectionState();
    
        // Если система в замешательстве
        if (state.confusion > 0.7f) {
            tryNewStrategy();
        }
        
        // Если любопытство высокое
        if (state.curiosity > 0.6f) {
            explore();
        }
        
        // Если удовлетворенность высокая
        if (state.satisfaction > 0.8f) {
            std::string insight = generateInsight();
            insights.push_back(insight);
            insight_quality.push_back(state.satisfaction);
            
            if (insight_quality.back() > 0.9f) {
                saveInsight(insight);
            }
        }
        
        // Ставим новую цель на основе рефлексии
        if (state.confidence < 0.3f) {
            neural_system.setGoal("understand_self");
        } else if (state.curiosity > 0.5f) {
            neural_system.setGoal("explore_new");
        }
    }
    
private:
    void tryNewStrategy() {
        for (int i = 16; i < 24; i += 2) {
            double new_value = (rng() % 100) / 100.0;
            neural_system.strengthenInterConnection(31, i, new_value * 0.1);
        }
    }
    
    void explore() {
        std::uniform_int_distribution<> dist(0, 31);
        for (int i = 0; i < 5; i++) {
            int idx = dist(rng);
            neural_system.strengthenInterConnection(31, idx, 0.05);
        }
    }
    
    std::string generateInsight() {
        auto state = neural_system.getReflectionState();
        
        std::string insight = "I feel ";
        if (state.confidence > 0.7f) insight += "confident";
        else if (state.confidence > 0.4f) insight += "uncertain";
        else insight += "confused";
        
        insight += " and ";
        if (state.curiosity > 0.6f) insight += "curious";
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