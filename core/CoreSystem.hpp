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
#include "modules/lang/LanguageModule.hpp"
#include "data/PersonnelData.hpp"    
#include "data/MasterKeyManager.hpp" 
#include "data/AccessManager.hpp"    
#include "IAuthorization.hpp"
#include "EnterpriseAuth.hpp"
#include "PersonalAuth.hpp"
#include "data/SemanticGraphDatabase.hpp"
// ДОБАВЛЕНО: предварительные объявления
class EvolutionModule;
class LearningOrchestrator;
class MetaCognitiveModule;
/*
График загрузки нейронов
text
Категория          Занято нейронов
─────────────────────────────────────
Ядро системы       ████████████████░ 320
Language           ██████░░░░░░░░░░░░ 128
Эволюция           ███░░░░░░░░░░░░░░░ 64
Мета-познание      ███░░░░░░░░░░░░░░░ 64
Predictor          █░░░░░░░░░░░░░░░░░ 24
MaryLineage        ████░░░░░░░░░░░░░░ 96
Визуализация       ███░░░░░░░░░░░░░░░ 64
Память             ███░░░░░░░░░░░░░░░ 64
─────────────────────────────────────
ИТОГО: 824 нейрона (80% загрузки)
Резерв: 200 нейронов (20% свободно)
*/

class CoreSystem {

private:
    std::vector<std::unique_ptr<Component>> components;
    Config config;
    MemoryManager memory;
    ImmutableCore immutableCore; // ЗАЩИТА ЯДРА (техническая)
    EventSystem eventSystem;
    DeviceDescriptor deviceInfo;

    // система управления доступом (социальная)
    PersonnelDatabase personnel_db;
    MasterKeyManager master_key_mgr;
    AccessManager access_manager{personnel_db, master_key_mgr};

    std::unique_ptr<LanguageModule> language;
    
    // Ядро нейросети
    std::unique_ptr<NeuralFieldSystem> neuralSystem;
    
    // Реестр компонентов по имени
    std::map<std::string, Component*> componentRegistry;
    
    //  IdentityManager
    std::unique_ptr<MaryLineage> lineage;

    // Интеграция Устройства 
    std::vector<Capability> capabilities;
    
    // НОВОЕ: режим работы
    std::string system_mode = "personal";  // по умолчанию personal
    std::string default_user = "user";     // для personal режима

    // ВМЕСТО конкретного AccessManager:
    std::unique_ptr<IAuthorization> auth;

    // ДОБАВЛЕНО
    SemanticGraphDatabase semantic_graph;
public:
    CoreSystem();
    ~CoreSystem();
    IAuthorization& getAuth() { return *auth; }
    
    // Инициализация
    bool initialize(const std::string& configFile);
    void shutdown();
    
    // Основной цикл
    void update(float dt);

    // активное восприятие
    void activePerception();
    void initializeSemanticGraph();
    
    // Метод для проверки, инициализирован ли граф
    bool isSemanticGraphInitialized() const { 
        return !semantic_graph.getAllNodes().empty(); 
    }
    SemanticGraphDatabase& getSemanticGraph() { return semantic_graph; }
    const SemanticGraphDatabase& getSemanticGraph() const { return semantic_graph; }

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
    // Доступ к компонентам
    NeuralFieldSystem& getNeuralSystem() { return *neuralSystem; }
    MemoryManager& getMemory() { return memory; }
    ImmutableCore& getImmutableCore() { return immutableCore; }
    AccessManager& getAccessManager() { return access_manager; }  // НОВОЕ
    PersonnelDatabase& getPersonnelDB() { return personnel_db; }  // НОВОЕ
    
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

        // НОВЫЙ МЕТОД: безопасное выполнение действия
    template<typename Func>
    bool executeSecurely(const std::string& user_name, 
                        const std::string& action,
                        AccessLevel required_level,
                        Func&& func) {
        
        // 1. Проверяем социальный доступ (кто ты?)
        if (!access_manager.canPerform(user_name, action, required_level)) {
            std::cout << "(-)Access denied for " << user_name 
                      << " to perform " << action << std::endl;
            return false;
        }
        
        // 2. Проверяем технический доступ (можно ли сейчас?)
        PermissionRequest req;
        req.action = action;
        req.source_module = "user:" + user_name;
        req.estimated_impact = 0.5; // можно вычислять динамически
        
        if (!immutableCore.requestPermission(req)) {
            std::cout << "System prevents " << action << " at this moment" << std::endl;
            return false;
        }
        
        // 3. Выполняем действие
        func();
        
        // 4. Логируем успешное действие
        std::cout << "(+)" << user_name << " performed " << action << std::endl;
        return true;
    }

    // НОВЫЕ МЕТОДЫ
    const std::string& getMode() const { return system_mode; }
    bool isEnterpriseMode() const { return system_mode == "enterprise"; }
    bool isPersonalMode() const { return system_mode == "personal"; }
    const std::string& getDefaultUser() const { return default_user; }
};