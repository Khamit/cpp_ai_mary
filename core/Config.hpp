#pragma once
#include <string>
#include <map>
#include <any>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

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

    // JSON сериализация
    static Config fromJSON(const std::string& json_str);
    std::string toJSON() const;
};