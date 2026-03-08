#pragma once
#include <string>
#include "Config.hpp"

// Forward declaration
class MemoryManager;

class Component {
public:
    virtual ~Component() = default;
    
    // Базовые методы жизненного цикла
    virtual std::string getName() const = 0;
    virtual bool initialize(const Config& config) = 0;
    virtual void shutdown() = 0;
    virtual void update(float dt) = 0;
    
    // Сохранение/загрузка состояния через MemoryManager
    virtual void saveState(MemoryManager& memory) = 0;
    virtual void loadState(MemoryManager& memory) = 0;
    
    // Опциональные методы
    virtual void onRegister() {}  // вызывается при регистрации в CoreSystem
    virtual void onUnregister() {} // вызывается при удалении
};