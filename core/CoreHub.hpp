#include <iostream>
#include "NeuralGroup.hpp"

class CoreHub {
public:
    CoreHub(int numHubs);
    void connectToGroups(const std::vector<NeuralGroup>& groups, const std::vector<int>& hubIndices);
    void integrate(std::vector<NeuralGroup>& groups);
    void learnSTDP(float reward,int currentStep);
private:
    std::vector<NeuralGroup*> hubs; // указатели на группы-хабы
    std::vector<std::vector<float>> hubWeights; // веса между хабами
    std::vector<float> hubInputs; // временные входы
};