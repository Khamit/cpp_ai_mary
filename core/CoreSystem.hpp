#pragma once
#include <vector>
#include <memory>
#include <string>
#include "Component.hpp"
#include "Config.hpp"
#include "DeviceProbe.hpp"
#include "EmergentCore.hpp"
//#include "MemoryManager.hpp"
#include "NeuralFieldSystem.hpp"
#include "ImmutableCore.hpp"
#include "MaryLineage.hpp"
#include "modules/lang/LanguageModule.hpp"
#include "data/PersonnelData.hpp"    
#include "data/MasterKeyManager.hpp" 
#include "data/AccessManager.hpp"    
#include "IAuthorization.hpp"
#include "EnterpriseAuth.hpp"
#include "PersonalAuth.hpp"

class MetaCognitiveModule;

class CoreSystem {
private:
    std::vector<std::unique_ptr<Component>> components;
    std::map<std::string, Component*> componentRegistry;
    
    // Ядро системы
    Config config;
    EmergentMemory memory;
    ImmutableCore immutableCore;
    EventSystem eventSystem;
    std::unique_ptr<NeuralFieldSystem> neuralSystem;
    
    // Управление доступом
    PersonnelDatabase personnel_db;
    MasterKeyManager master_key_mgr;
    AccessManager access_manager{personnel_db, master_key_mgr};
    std::unique_ptr<IAuthorization> auth;
    
    // Идентичность
    std::unique_ptr<MaryLineage> lineage;
    
    // Режим работы
    std::string system_mode = "personal";
    std::string default_user = "user";
    
    // Кэшированные данные устройства (получаем один раз)
    DeviceDescriptor deviceInfo;
    std::vector<Capability> capabilities;
    
public:
    CoreSystem();
    ~CoreSystem();
    
    IAuthorization& getAuth() { return *auth; }
    
    bool initialize(const std::string& configFile);
    void shutdown();
    void update(float dt);
    
    // Доступ к ядру
    NeuralFieldSystem& getNeuralSystem() { return *neuralSystem; }
    EmergentMemory& getMemory() { return memory; }
    ImmutableCore& getImmutableCore() { return immutableCore; }
    PersonnelDatabase& getPersonnelDB() { return personnel_db; }
    EventSystem& getEventSystem() { return eventSystem; }
    MaryLineage& getLineage() { return *lineage; }
    
    // Информация об устройстве (из DeviceProbe)
    const DeviceDescriptor& getDeviceInfo() const { return deviceInfo; }
    const std::vector<Capability>& getCapabilities() const { return capabilities; }
    
    // Состояние системы
    void saveState();
    void loadState();
    
    // Режим работы
    const std::string& getMode() const { return system_mode; }
    bool isEnterpriseMode() const { return system_mode == "enterprise"; }
    bool isPersonalMode() const { return system_mode == "personal"; }
    const std::string& getDefaultUser() const { return default_user; }
    
    // Регистрация компонентов
    template<typename T, typename... Args>
    T* registerComponent(const std::string& name, Args&&... args) {
        static_assert(std::is_base_of<Component, T>::value, 
                    "T must inherit from Component");
        
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = component.get();
        
        componentRegistry[name] = component.get();
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

        // Добавить метод для доступа к EmergentController
    EmergentController& getEmergentController() { 
        return neuralSystem->emergentMutable(); 
    }
    
    const EmergentController& getEmergentController() const { 
        return neuralSystem->emergent(); 
    }

    
private:
    void loadModulesForDevice();
    void loadConfig(const std::string& configFile);
};