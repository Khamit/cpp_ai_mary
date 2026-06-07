// server/ApiHandlers.hpp
#pragma once

#include <string>
#include <nlohmann/json.hpp>

class NeuralFieldSystem;
class AgentAuditBridge;

class ApiHandlers {
public:
    ApiHandlers(NeuralFieldSystem* nfs, AgentAuditBridge* auditor, const std::string& workspace);
    
    // GET handlers
    std::string handleStatus();
    std::string handleConstraints();
    std::string handleAuditorStatus();
    std::string handleAnalytics();
    std::string handleAgentsList();
    std::string handleDemoStatus();
    std::string handleLastAction();
    std::string handleStats(const std::string& period);
    std::string handleMetrics();
    
    // POST handlers
    std::string handleRegisterAgent(const nlohmann::json& body);
    std::string handleHeartbeat(const nlohmann::json& body);
    std::string handleAuditAction(const nlohmann::json& body);
    std::string handleConstraintToggle(const nlohmann::json& body);
    std::string handleSessionStart(const nlohmann::json& body);
    std::string handleSessionEnd();
    std::string handleSaveState();
    std::string handleResetRisk();
    std::string handleAuditorToggle(const nlohmann::json& body);
    std::string handleAuditorReset();
    std::string handleAuditorConfigSave(const nlohmann::json& body);
    std::string handleDemoStart();
    std::string handleDemoStop();
    std::string handleDemoScenario();
    
    // Event data
    nlohmann::json getEventData();
    
private:
    NeuralFieldSystem* nfs_;
    AgentAuditBridge* auditor_;
    std::string workspace_;
};