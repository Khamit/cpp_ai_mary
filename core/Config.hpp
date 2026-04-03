#pragma once
#include <string>
#include <map>
#include <any>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include "DynamicParams.hpp"

class Config {
private:
    std::map<std::string, std::any> values;

public:
    template<typename T>
    T get(const std::string& key, T defaultValue = T()) const {
        auto it = values.find(key);
        if (it != values.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                std::cerr << "Config: Type mismatch for key " << key << std::endl;
            }
        }
        return defaultValue;
    }

    template<typename T>
    void set(const std::string& key, T value) {
        values[key] = value;
    }

    bool loadFromFile(const std::string& filename);
    bool saveToFile(const std::string& filename) const;

    double getEntropyTarget() const { return get<double>("entropy.target", 2.5); }
    double getEntropyLearningRate() const { return get<double>("entropy.learning_rate", 0.01); }
    double getMinEntropyThreshold() const { return get<double>("entropy.min_threshold", 0.5); }

    // ===== НОВАЯ СТРУКТУРА ДЛЯ ПАРАМЕТРОВ РЕЖИМОВ =====
    struct ModeLimits {
        double planck_mass;
        double schwarzschild_radius_factor;
        double max_temperature_mass;
        double evaporation_rate;
        double info_capacity_factor;
        double max_entropy_density;
        double binding_energy_factor;
        double saturation_mass;
        
        // Конструктор по умолчанию
        ModeLimits() : 
            planck_mass(1.0),
            schwarzschild_radius_factor(2.0),
            max_temperature_mass(3.0),
            evaporation_rate(0.01),
            info_capacity_factor(0.1),
            max_entropy_density(1.0),
            binding_energy_factor(0.3),
            saturation_mass(4.0) {}
    };
    
    ModeLimits getTrainingLimits() const {
        ModeLimits limits;
        limits.planck_mass = get<double>("mode.training.planck_mass", 1.0);
        limits.schwarzschild_radius_factor = get<double>("mode.training.schwarzschild_radius_factor", 2.0);
        limits.max_temperature_mass = get<double>("mode.training.max_temperature_mass", 4.5);
        limits.evaporation_rate = get<double>("mode.training.evaporation_rate", 0.01);
        limits.info_capacity_factor = get<double>("mode.training.info_capacity_factor", 0.1);
        limits.max_entropy_density = get<double>("mode.training.max_entropy_density", 1.0);
        limits.binding_energy_factor = get<double>("mode.training.binding_energy_factor", 0.3);
        limits.saturation_mass = get<double>("mode.training.saturation_mass", 5.0);
        return limits;
    }
    
    ModeLimits getNormalLimits() const {
        ModeLimits limits;
        limits.planck_mass = get<double>("mode.normal.planck_mass", 1.0);
        limits.schwarzschild_radius_factor = get<double>("mode.normal.schwarzschild_radius_factor", 2.0);
        limits.max_temperature_mass = get<double>("mode.normal.max_temperature_mass", 2.5);
        limits.evaporation_rate = get<double>("mode.normal.evaporation_rate", 0.005);
        limits.info_capacity_factor = get<double>("mode.normal.info_capacity_factor", 0.1);
        limits.max_entropy_density = get<double>("mode.normal.max_entropy_density", 1.0);
        limits.binding_energy_factor = get<double>("mode.normal.binding_energy_factor", 0.4);
        limits.saturation_mass = get<double>("mode.normal.saturation_mass", 3.0);
        return limits;
    }
    
    ModeLimits getIdleLimits() const {
        ModeLimits limits;
        limits.planck_mass = get<double>("mode.idle.planck_mass", 1.0);
        limits.schwarzschild_radius_factor = get<double>("mode.idle.schwarzschild_radius_factor", 2.0);
        limits.max_temperature_mass = get<double>("mode.idle.max_temperature_mass", 1.5);
        limits.evaporation_rate = get<double>("mode.idle.evaporation_rate", 0.001);
        limits.info_capacity_factor = get<double>("mode.idle.info_capacity_factor", 0.1);
        limits.max_entropy_density = get<double>("mode.idle.max_entropy_density", 1.0);
        limits.binding_energy_factor = get<double>("mode.idle.binding_energy_factor", 0.5);
        limits.saturation_mass = get<double>("mode.idle.saturation_mass", 2.0);
        return limits;
    }
    
    OperatingMode::Type getStartupMode() const {
        std::string mode = get<std::string>("mode.startup", "normal");
        if (mode == "training") return OperatingMode::TRAINING;
        if (mode == "idle") return OperatingMode::IDLE;
        if (mode == "sleep") return OperatingMode::SLEEP;
        return OperatingMode::NORMAL;
    }
    
    bool isButtonPressed() const {
        return get<bool>("ui.button_pressed", false);
    }

    // JSON сериализация
    static Config fromJSON(const std::string& json_str);
    std::string toJSON() const;
};