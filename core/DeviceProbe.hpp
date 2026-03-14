#pragma once
#include <string>
#include <vector>
#include <sstream>
#include "DeviceDescriptor.hpp"

class DeviceProbe {
public:
    static DeviceDescriptor detect();
    static std::string getHardwareId(); // уникальный ID устройства
    
    // Методы детекции отдельных компонентов
    static size_t detectRAM();
    static int detectCPU();
    static float detectCPUFrequency();
    static bool detectGPU();
    
    static bool detectCamera();
    static bool detectMicrophone();
    static bool detectIMU();
    static bool detectGPS();
    static bool detectTouch();
    
    static bool detectDisplay();
    static bool detectSpeaker();
    static bool detectMotor();
    static bool detectNetwork();
    
    static bool detectBluetooth();
    static bool detectWifi();
    
    // Определение типа устройства
    static std::string detectDeviceType(const DeviceDescriptor& d);
    
    // Построение графа возможностей
    static std::vector<Capability> buildCapabilities(const DeviceDescriptor& d);
    
    // Self-description API
    static std::string describe(const DeviceDescriptor& d);
    
    // Преобразование в вектор для нейросети
    static std::vector<float> encodeDevice(const DeviceDescriptor& d);
    
    // Утилиты
    static bool hasCamera() { return detectCamera(); }
    static bool hasMicrophone() { return detectMicrophone(); }
    static size_t getTotalRAM() { return detectRAM(); }
    static int getCpuCores() { return detectCPU(); }
};