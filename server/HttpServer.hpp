#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <memory>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <nlohmann/json.hpp>

// Forward declarations
class NeuralFieldSystem;
class AgentAuditBridge;
class ApiHandlers;

class HttpServer {
public:
    HttpServer(int port, const std::string& workspace, NeuralFieldSystem* nfs, AgentAuditBridge* auditor);
    ~HttpServer();
    
    bool start();
    void stop();
    bool isRunning() const { return running_.load(); }
    
    // Установка указателя на глобальный флаг (теперь atomic)
    void setRunningFlag(std::atomic<bool>* flag) { running_flag_ = flag; }
    
private:
    void run();
    void handleClient(int client_fd);
    std::string loadHtmlFile(const std::string& path);
    
    // Обработчики запросов
    void handleGetRequest(const std::string& path, std::string& response);
    void handlePostRequest(const std::string& path, const std::string& body, std::string& response);
    
    int port_;
    std::string workspace_;
    int server_fd_;
    std::atomic<bool> running_;
    std::thread server_thread_;
    
    NeuralFieldSystem* nfs_;
    AgentAuditBridge* auditor_;
    std::unique_ptr<ApiHandlers> apiHandlers_;
    
    std::atomic<bool>* running_flag_ = nullptr;  // Теперь atomic!
    std::mutex data_mutex_;  // Мьютекс для защиты доступа к nfs_ и auditor_
};