// modules/hallucination/HNeuronDetector.hpp
#pragma once
#include "../../core/NeuralFieldSystem.hpp"
#include <vector>
#include <map>
#include <iostream>

struct HNeuronProfile {
    int groupIndex;
    int neuronIndex;
    float hallucinationScore;    // 0-1, насколько часто нейрон активен при галлюцинациях
    float activationHistory;      // средняя активность
    int sampleCount;              // количество наблюдений
    
    HNeuronProfile() : groupIndex(0), neuronIndex(0), 
                       hallucinationScore(0.0f), activationHistory(0.0f), 
                       sampleCount(0) {}
};

class HNeuronDetector {
public:
    // ЯВНЫЙ КОНСТРУКТОР ПО УМОЛЧАНИЮ
    HNeuronDetector() : system_(nullptr) {
        std::cout << "HNeuronDetector created" << std::endl;
    }
    
    // Привязка к нейросистеме
    void setSystem(NeuralFieldSystem* system) {
        system_ = system;
        std::cout << "HNeuronDetector connected to neural system" << std::endl;
    }
    
    // Запись фидбека
    void recordFeedback(int step, float reward, bool wasHallucination) {
        if (!system_) return;
        
        const auto& groups = system_->getGroups();
        
        for (int g = 0; g < groups.size(); ++g) {
            const auto& phi = groups[g].getPhi();
            
            for (int n = 0; n < phi.size(); ++n) {
                auto key = std::make_pair(g, n);
                auto& profile = neuronProfiles_[key];
                profile.groupIndex = g;
                profile.neuronIndex = n;
                
                if (wasHallucination) {
                    profile.hallucinationScore += static_cast<float>(phi[n]);
                    profile.sampleCount++;
                }
            }
        }
    }
    
    // Поиск подозрительных нейронов
    std::vector<HNeuronProfile> findHNeurons(float threshold = 0.8f) {
        std::vector<HNeuronProfile> suspects;
        
        for (auto& [key, profile] : neuronProfiles_) {
            if (profile.sampleCount > 10) {
                float avgActivation = profile.hallucinationScore / profile.sampleCount;
                profile.activationHistory = avgActivation;
                
                if (avgActivation > threshold) {
                    suspects.push_back(profile);
                }
            }
        }
        
        return suspects;
    }
    
    // Очистка статистики
    void reset() {
        neuronProfiles_.clear();
    }
    
private:
    NeuralFieldSystem* system_;
    std::map<std::pair<int, int>, HNeuronProfile> neuronProfiles_;
};