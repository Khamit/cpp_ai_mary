// application/AgentConfig.hpp
#pragma once

#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>

class AgentConfig {
public:
    static AgentConfig& getInstance() {
        static AgentConfig instance;
        return instance;
    }
    
    // Установка рабочей папки
    void setWorkingDirectory(const std::string& path) {
        working_dir_ = path;
        ensureDirectories();
    }
    
    std::string getWorkingDirectory() const { return working_dir_; }
    
    // Пути к папкам (относительно рабочей)
    std::string getLogsRawDir() const { return working_dir_ + "/logs/raw"; }
    std::string getLogsAggregatedDir() const { return working_dir_ + "/logs/aggregated"; }
    std::string getMemoryDir() const { return working_dir_ + "/memory"; }
    std::string getModelsDir() const { return working_dir_ + "/models"; }
    std::string getConfigDir() const { return working_dir_ + "/config"; }
    
    // Путь к модели
    void setModelPath(const std::string& path) { model_path_ = path; }
    std::string getModelPath() const { return model_path_; }
    
    // Полный путь к файлам памяти
    std::string getSTMFilePath() const { return getMemoryDir() + "/stm.bin"; }
    std::string getLTMFilePath() const { return getMemoryDir() + "/ltm.bin"; }
    
    // Получить список активных ограничений из конфига
    std::vector<std::string> getConstraints() const {
        std::vector<std::string> active;
        for (const auto& [name, enabled] : constraints_) {
            if (enabled) active.push_back(name);
        }
        return active;
    }
    
    // Загрузка/сохранение конфигурации
    bool loadFromFile(const std::string& filename = "") {
        std::string path = filename.empty() ? getConfigDir() + "/config.json" : filename;
        std::ifstream file(path);
        if (!file.is_open()) return false;
        
        try {
            nlohmann::json j;
            file >> j;
            
            if (j.contains("working_directory")) working_dir_ = j["working_directory"];
            if (j.contains("model_path")) model_path_ = j["model_path"];
            if (j.contains("constraints")) {
                for (auto& [key, value] : j["constraints"].items()) {
                    constraints_[key] = value.get<bool>();
                }
            }
            ensureDirectories();
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error loading config: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool saveToFile() const {
        std::string path = getConfigDir() + "/config.json";
        std::filesystem::create_directories(getConfigDir());
        std::ofstream file(path);
        if (!file.is_open()) return false;
        
        nlohmann::json j;
        j["working_directory"] = working_dir_;
        j["model_path"] = model_path_;
        j["constraints"] = constraints_;
        
        file << j.dump(4);
        return true;
    }
    
    // Установить ограничение
    void setConstraint(const std::string& name, bool enabled) {
        constraints_[name] = enabled;
    }
    
    bool isConstraintEnabled(const std::string& name) const {
        auto it = constraints_.find(name);
        return it != constraints_.end() && it->second;
    }
    
    // Сброс к значениям по умолчанию
    void resetToDefault() {
        working_dir_ = "agent_workspace";
        model_path_ = "models/Phi-3-mini-4k-instruct-q4.gguf";
        constraints_.clear();
        constraints_["confidence_50"] = true;
        constraints_["max_steps_per_second"] = false;
        constraints_["response_length_300"] = false;
        constraints_["response_length_600"] = false;
    }
    
private:
    AgentConfig() { resetToDefault(); }
    
    void ensureDirectories() const {
        std::filesystem::create_directories(getLogsRawDir());
        std::filesystem::create_directories(getLogsAggregatedDir());
        std::filesystem::create_directories(getMemoryDir());
        std::filesystem::create_directories(getModelsDir());
        std::filesystem::create_directories(getConfigDir());
    }
    
    std::string working_dir_;
    std::string model_path_;
    std::unordered_map<std::string, bool> constraints_;
};