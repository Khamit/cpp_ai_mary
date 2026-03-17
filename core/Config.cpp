#include "Config.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <typeinfo>

using json = nlohmann::json;

// В Config::loadFromFile, добавьте обработку разных типов:
bool Config::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;
    
    std::string line;
    while (std::getline(file, line)) {
        // Пропускаем пустые строки и комментарии
        if (line.empty() || line[0] == '#') continue;
        
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // Очищаем от пробелов
            key.erase(remove_if(key.begin(), key.end(), ::isspace), key.end());
            value.erase(remove_if(value.begin(), value.end(), ::isspace), value.end());
            
            // Пробуем определить тип
            if (value == "true" || value == "false") {
                set(key, value == "true");
            }
            else if (value.find('.') != std::string::npos) {
                try {
                    set(key, std::stod(value));
                } catch (...) {
                    set(key, value);
                }
            }
            else {
                try {
                    set(key, std::stoi(value));
                } catch (...) {
                    set(key, value);
                }
            }
        }
    }
    return true;
}

bool Config::saveToFile(const std::string& filename) const {
    json j;
    for (const auto& [key, val] : values) {
        try {
            if (val.type() == typeid(int))
                j[key] = std::any_cast<int>(val);
            else if (val.type() == typeid(double))
                j[key] = std::any_cast<double>(val);
            else if (val.type() == typeid(std::string))
                j[key] = std::any_cast<std::string>(val);
            else if (val.type() == typeid(bool))
                j[key] = std::any_cast<bool>(val);
            else
                j[key] = nullptr;
        } catch (...) {
            j[key] = nullptr;
        }
    }

    std::ofstream file(filename);
    if (!file.is_open())
        return false;

    file << j.dump(4); // красочный формат
    return true;
}

Config Config::fromJSON(const std::string& json_str) {
    Config cfg;
    json j;
    try {
        j = json::parse(json_str);
    } catch (const std::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return cfg;
    }

    for (auto it = j.begin(); it != j.end(); ++it) {
        if (it.value().is_number_integer())
            cfg.set(it.key(), it.value().get<int>());
        else if (it.value().is_number_float())
            cfg.set(it.key(), it.value().get<double>());
        else if (it.value().is_string())
            cfg.set(it.key(), it.value().get<std::string>());
        else if (it.value().is_boolean())
            cfg.set(it.key(), it.value().get<bool>());
    }

    return cfg;
}

std::string Config::toJSON() const {
    json j;
    for (const auto& [key, val] : values) {
        try {
            if (val.type() == typeid(int))
                j[key] = std::any_cast<int>(val);
            else if (val.type() == typeid(double))
                j[key] = std::any_cast<double>(val);
            else if (val.type() == typeid(std::string))
                j[key] = std::any_cast<std::string>(val);
            else if (val.type() == typeid(bool))
                j[key] = std::any_cast<bool>(val);
            else
                j[key] = nullptr;
        } catch (...) {
            j[key] = nullptr;
        }
    }
    return j.dump(4);
}