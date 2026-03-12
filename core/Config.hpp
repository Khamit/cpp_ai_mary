#pragma once
#include <string>
#include <map>
#include <any>
#include <fstream>
#include <iostream>

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

    // Получить параметры энтропийной регуляризации
    inline double getEntropyTarget() const {
        return get<double>("entropy.target", 2.5);  // целевая энтропия
    }

    inline double getEntropyLearningRate() const {
        return get<double>("entropy.learning_rate", 0.01);
    }

    inline double getMinEntropyThreshold() const {
        return get<double>("entropy.min_threshold", 0.5);
    }
    
    // Вспомогательные методы для JSON парсинга
    static Config fromJSON(const std::string& json);
    std::string toJSON() const;
};