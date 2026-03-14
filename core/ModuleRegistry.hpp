// ModuleRegistry.hpp
#pragma once
#include "DeviceDescriptor.hpp"
#include <vector>
#include <string>
#include <memory>

enum class DeviceType
{
    GENERIC,
    ROBOT,
    DRONE,
    WEARABLE,
    COMPUTER,
    EMBEDDED
};
// Добавить Self Description API. 
// what can you do?

struct ModuleInfo
{
    std::string name;
    std::vector<std::string> actions;
    std::vector<std::string> inputs;
};

struct ResourcePolicy
{
    size_t max_memory;
    float max_cpu;
    bool allow_heavy_modules;
};
// Mary выбирает модули исходя из ресурсов.
/*
Стартовый протокол Mary
Я бы сделал при запуске такой pipeline:
boot
 ↓
DeviceProbe
 ↓
DeviceDescriptor
 ↓
CapabilityGraph
 ↓
ModuleRegistry
 ↓
NeuralCore start
*/

class ModuleRegistry {
private:
    std::vector<ModuleInfo> availableModules;
    
public:
    ModuleRegistry();
    
    // Загрузить модули подходящие для устройства
    void loadModules(const DeviceDescriptor& device);
    
    // Получить список доступных модулей
    std::vector<ModuleInfo> getAvailableModules() const;
    
    // Получить политику ресурсов для устройства
    ResourcePolicy getResourcePolicy(const DeviceDescriptor& device) const;
};
/*
Логика:

if camera → load VisionModule
if microphone → load SpeechModule
if motor → load MotionModule
======================================
Чтобы нейросистема могла работать с этим, 
нужно преобразовать устройство в вектор состояния.

Например:
[ram, cpu, camera, mic, motor, display, network]
----------------------------
Система должна различать:
smart_home
wearable
robot
drone
computer
phone
prosthetic
------------------------
Не давать ядру напрямую доступ к устройству.

Всегда через:

DeviceDescriptor
CapabilityGraph
ModuleInterface
*/