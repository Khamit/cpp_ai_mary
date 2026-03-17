// CoreHub.hpp - ИСПРАВЛЕННЫЙ
#pragma once
#include <vector>
#include "core/INeuralGroupAccess.hpp"

class CoreHub {
public:
    CoreHub(int numHubs);
    
    // Теперь принимает интерфейс, а не константные группы
    void connectToGroups(INeuralGroupAccess& system);
    void integrate(INeuralGroupAccess& system);
    void learnSTDP(float reward, int currentStep);
    
private:
    std::vector<NeuralGroup*> hubs;  // неконстантные указатели
    std::vector<std::vector<float>> hubWeights;
    std::vector<float> hubInputs;
};