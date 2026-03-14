#pragma once
#include <vector>
#include <memory>
#include <string>
#include "Component.hpp"
#include "Config.hpp"
#include "DeviceProbe.hpp"
#include "MemoryManager.hpp"
#include "NeuralFieldSystem.hpp"
#include "ImmutableCore.hpp"
#include "MaryLineage.hpp"

class CoreSystem {
private:
    std::vector<std::unique_ptr<Component>> components;
    Config config;
    MemoryManager memory;
    ImmutableCore immutableCore;
    EventSystem eventSystem;
    DeviceDescriptor deviceInfo;
    
    // Ядро нейросети
    std::unique_ptr<NeuralFieldSystem> neuralSystem;
    
    // Реестр компонентов по имени
    std::map<std::string, Component*> componentRegistry;
    
    //  IdentityManager
    std::unique_ptr<MaryLineage> lineage;

    // Интеграция
    std::vector<Capability> capabilities;
    
public:
    CoreSystem();
    ~CoreSystem();
    
    // Инициализация
    bool initialize(const std::string& configFile);
    void shutdown();
    
    // Основной цикл
    void update(float dt);
    
    // Регистрация компонентов
    template<typename T, typename... Args>
    T* registerComponent(const std::string& name, Args&&... args) {
        // Статическая проверка, что T наследник Component
        static_assert(std::is_base_of<Component, T>::value, 
                    "T must inherit from Component");
        
        // Создаем компонент как unique_ptr<Component> для хранения в векторе
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = component.get();  // Сохраняем сырой указатель для возврата
        
        // Сохраняем в реестр как Component* - это работает благодаря полиморфизму
        componentRegistry[name] = component.get();
        
        // Перемещаем в вектор как unique_ptr<Component>
        components.push_back(std::move(component));
        return ptr;
    }
    
    // Получение компонентов
    template<typename T>
    T* getComponent(const std::string& name) {
        auto it = componentRegistry.find(name);
        if (it != componentRegistry.end()) {
            return dynamic_cast<T*>(it->second);
        }
        return nullptr;
    }
    
    // Доступ к ядру
    NeuralFieldSystem& getNeuralSystem() { return *neuralSystem; }
    MemoryManager& getMemory() { return memory; }
    ImmutableCore& getImmutableCore() { return immutableCore; }
    const Config& getConfig() const { return config; }
    
    // Сохранение состояния всей системы
    void saveState();
    void loadState();

    // id
    MaryLineage& getLineage() { return *lineage; }
    // Отчеты об аномалиях? хахаха))
    EventSystem& getEventSystem() { return eventSystem; }

    // Новые методы
    const DeviceDescriptor& getDeviceInfo() const { return deviceInfo; }
    const std::vector<Capability>& getCapabilities() const { return capabilities; }
    
    // Загрузить модули на основе устройства
    void loadModulesForDevice();
};