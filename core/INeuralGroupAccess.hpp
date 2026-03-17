// core/INeuralGroupAccess.hpp - НОВЫЙ ФАЙЛ
#pragma once
#include <vector>

class INeuralGroupAccess {
public:
    virtual ~INeuralGroupAccess() = default;
    
    // Методы для доступа к группам-хабам
    virtual std::vector<class NeuralGroup*>& getHubGroups() = 0;
    virtual const std::vector<class NeuralGroup*>& getHubGroups() const = 0;
    
    // Метод для регистрации хаба
    virtual void registerHub(int groupIndex) = 0;
};