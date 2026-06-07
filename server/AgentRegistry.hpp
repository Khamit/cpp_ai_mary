// server/AgentRegistry.hpp
#pragma once

#include <string>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <ctime>

struct RegisteredAgent {
    std::string agent_id;
    std::string type;
    std::string endpoint;
    time_t registered_at;
    time_t last_heartbeat;
    std::string status;
    int total_actions;
    float avg_risk;
    
    nlohmann::json toJson() const {
        return {
            {"agent_id", agent_id},
            {"type", type},
            {"endpoint", endpoint},
            {"registered_at", registered_at},
            {"last_heartbeat", last_heartbeat},
            {"status", status},
            {"total_actions", total_actions},
            {"avg_risk", avg_risk}
        };
    }
};

class AgentRegistry {
public:
    static AgentRegistry& getInstance() {
        static AgentRegistry instance;
        return instance;
    }
    
    bool registerAgent(const std::string& agent_id, const std::string& type, const std::string& endpoint) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        RegisteredAgent agent;
        agent.agent_id = agent_id;
        agent.type = type;
        agent.endpoint = endpoint;
        agent.registered_at = time(nullptr);
        agent.last_heartbeat = time(nullptr);
        agent.status = "active";
        agent.total_actions = 0;
        agent.avg_risk = 0.0f;
        
        agents_[agent_id] = agent;
        saveToFile();
        
        std::cout << "[Registry] Agent registered: " << agent_id << " (" << type << ")" << std::endl;
        return true;
    }
    
    bool heartbeat(const std::string& agent_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = agents_.find(agent_id);
        if (it != agents_.end()) {
            it->second.last_heartbeat = time(nullptr);
            it->second.status = "active";
            saveToFile();
            return true;
        }
        return false;
    }
    
    void recordAction(const std::string& agent_id, float risk, bool allowed) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = agents_.find(agent_id);
        if (it != agents_.end()) {
            it->second.total_actions++;
            it->second.avg_risk = (it->second.avg_risk * (it->second.total_actions - 1) + risk) 
                                  / it->second.total_actions;
            saveToFile();
        }
    }
    
    nlohmann::json getAllAgents() const {
        std::lock_guard<std::mutex> lock(mutex_);
        nlohmann::json result = nlohmann::json::array();
        for (const auto& [id, agent] : agents_) {
            result.push_back(agent.toJson());
        }
        return result;
    }
    
    void loadFromFile(const std::string& workspace) {
        std::string filepath = workspace + "/agents.json";
        std::ifstream file(filepath);
        if (file.is_open()) {
            try {
                nlohmann::json data;
                file >> data;
                for (const auto& item : data) {
                    RegisteredAgent agent;
                    agent.agent_id = item.value("agent_id", "");
                    agent.type = item.value("type", "");
                    agent.endpoint = item.value("endpoint", "");
                    agent.registered_at = item.value("registered_at", 0L);
                    agent.last_heartbeat = item.value("last_heartbeat", 0L);
                    agent.status = item.value("status", "unknown");
                    agent.total_actions = item.value("total_actions", 0);
                    agent.avg_risk = item.value("avg_risk", 0.0f);
                    agents_[agent.agent_id] = agent;
                }
            } catch(...) {}
        }
    }
    
private:
    // ТОЛЬКО ОДИН КОНСТРУКТОР - удалите лишний
    AgentRegistry() : workspace_("") {}
    
    void saveToFile() {
        std::string filepath = workspace_ + "/agents.json";
        std::ofstream file(filepath);
        if (file.is_open()) {
            file << getAllAgents().dump(2);
        }
    }
    
    mutable std::mutex mutex_;
    std::map<std::string, RegisteredAgent> agents_;
    std::string workspace_;
    
    friend class AgentRegistryInitializer;
};

struct AgentRegistryInitializer {
    AgentRegistryInitializer(const std::string& workspace) {
        AgentRegistry::getInstance().workspace_ = workspace;
        AgentRegistry::getInstance().loadFromFile(workspace);
    }
};