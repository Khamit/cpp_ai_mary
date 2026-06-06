// main.cpp - с встроенным HTTP сервером для админ панели

#include "core/NeuralFieldSystem.hpp"
#include "core/AgentAuditBridge.hpp"
#include "application/AgentConfig.hpp"
#include "core/OperatingMode.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <unistd.h>
#include <filesystem>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// Глобальные указатели
NeuralFieldSystem* g_nfs = nullptr;
AgentAuditBridge* g_auditor = nullptr;
bool g_running = true;

// HTTP сервер в отдельном потоке
class HttpServer {
public:
    HttpServer(int port, const std::string& workspace) : port_(port), workspace_(workspace), running_(false) {}
    
    bool start() {
        server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd_ < 0) return false;
        
        int opt = 1;
        setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port_);
        
        if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "[HTTP] Bind failed on port " << port_ << std::endl;
            return false;
        }
        if (listen(server_fd_, 3) < 0) return false;
        
        running_ = true;
        server_thread_ = std::thread(&HttpServer::run, this);
        std::cout << "[HTTP] Server started on port " << port_ << std::endl;
        return true;
    }
    
    void stop() {
        running_ = false;
        close(server_fd_);
        if (server_thread_.joinable()) server_thread_.join();
    }
    
private:
    void run() {
        while (running_) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd < 0) continue;
            
            std::thread(&HttpServer::handleClient, this, client_fd).detach();
        }
    }
    
    std::string loadHtmlTemplate() {
        std::string html_path = "web/admin_panel.html";
        std::ifstream html_file(html_path);
        
        if (html_file.is_open()) {
            std::stringstream buffer;
            buffer << html_file.rdbuf();
            std::cout << "[HTTP] Loaded HTML from " << html_path << std::endl;
            return buffer.str();
        }
        
        std::cerr << "[HTTP] ERROR: Could not find " << html_path << std::endl;
        std::cerr << "[HTTP] Please create the file web/admin_panel.html" << std::endl;
        return "<html><body><h1>Error</h1><p>Admin panel not found. Please create web/admin_panel.html</p></body></html>";
    }
    
    void handleClient(int client_fd) {
        char buffer[4096] = {0};
        read(client_fd, buffer, sizeof(buffer) - 1);
        
        std::string request(buffer);
        std::string method, path, version;
        std::istringstream iss(request);
        iss >> method >> path >> version;
        
        std::string response;
        
        if (path == "/" || path == "/index.html") {
            std::string html = loadHtmlTemplate();
            response = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n" + html;
        }
        else if (path == "/api/status") {
            response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n" + getStatus();
        }
        else if (path == "/api/stats/24h") {
            response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n" + getStats24h();
        }

        else if (path == "/api/agent-status") {
            std::string status_file = workspace_ + "/agent_status.json";
            std::ifstream file(status_file);
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n" + buffer.str();
            } else {
                response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{}";
            }
        }

        // после обработки путей:
        else if (path == "/api/events") {
            response = "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/event-stream\r\n"
                    "Cache-Control: no-cache\r\n"
                    "Connection: keep-alive\r\n"
                    "Access-Control-Allow-Origin: *\r\n\r\n";
            send(client_fd, response.c_str(), response.length(), 0);
            
            // Отправляем события
            while (g_running) {
                if (g_auditor) {
                    auto& session = g_auditor->getCurrentSession();
                    nlohmann::json event;
                    event["type"] = "status";
                    event["total_steps"] = session.total_steps;
                    event["risk"] = g_auditor->getCurrentHallucinationRisk();
                    event["entropy"] = g_auditor->getCurrentEntropy();
                    event["successful_actions"] = session.successful_actions;
                    event["blocked_actions"] = session.blocked_actions;
                    event["accumulated_risk"] = session.accumulated_risk;
                    
                    std::string data = "data: " + event.dump() + "\n\n";
                    send(client_fd, data.c_str(), data.length(), 0);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            close(client_fd);
            return;
        }

        else if (method == "POST" && path == "/api/constraint") {
            size_t body_pos = request.find("\r\n\r\n");
            if (body_pos != std::string::npos) {
                std::string body = request.substr(body_pos + 4);
                try {
                    auto json = nlohmann::json::parse(body);
                    std::string name = json.value("name", "");
                    bool enabled = json.value("enabled", false);
                    if (g_auditor) {
                        g_auditor->enableConstraint(name, enabled);
                        std::cout << "[API] Constraint " << name << " " << (enabled ? "enabled" : "disabled") << std::endl;
                    }
                } catch(...) {}
            }
            response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"ok\"}";
        }
        else if (method == "POST" && path == "/api/session/start") {
            size_t body_pos = request.find("\r\n\r\n");
            if (body_pos != std::string::npos) {
                std::string body = request.substr(body_pos + 4);
                try {
                    auto json = nlohmann::json::parse(body);
                    std::string session_id = json.value("session_id", "");
                    std::string agent_name = json.value("agent_name", "");
                    if (g_auditor) {
                        g_auditor->startSession(session_id, agent_name);
                        std::cout << "[API] Session started: " << session_id << std::endl;
                    }
                } catch(...) {}
            }
            response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"ok\"}";
        }
        else if (method == "POST" && path == "/api/session/end") {
            if (g_auditor) {
                g_auditor->endSession();
                std::cout << "[API] Session ended" << std::endl;
            }
            response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"ok\"}";
        }
        else if (method == "POST" && path == "/api/save") {
            if (g_auditor) {
                g_auditor->saveMemoryState();
                std::cout << "[API] State saved" << std::endl;
            }
            response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"ok\"}";
        }
        else if (method == "POST" && path == "/api/reset_risk") {
            if (g_auditor) {
                g_auditor->resetRiskAccumulator();
                std::cout << "[API] Risk reset" << std::endl;
            }
            response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"ok\"}";
        }
        else {
            response = "HTTP/1.1 404 Not Found\r\n\r\n";
        }
        
        send(client_fd, response.c_str(), response.length(), 0);
        close(client_fd);
    }
    
    std::string getStatus() {
    // Читаем из файла agent_status.json
    std::string status_file = workspace_ + "/agent_status.json";
    std::ifstream file(status_file);
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    
        if (!g_auditor) {
            return "{\"status\":\"waiting\",\"risk\":0,\"entropy\":0.5,\"quality\":0.5,\"surprise\":0}";
        }
        
        auto& session = g_auditor->getCurrentSession();
        nlohmann::json j;
        j["status"] = "active";
        j["session_id"] = session.session_id;
        j["total_steps"] = session.total_steps;
        j["accumulated_risk"] = session.accumulated_risk;
        j["avg_entropy"] = session.average_entropy;
        j["risk"] = g_auditor->getCurrentHallucinationRisk();
        j["entropy"] = g_auditor->getCurrentEntropy();
        j["quality"] = 0.7 - j["risk"].get<float>();
        j["surprise"] = j["risk"].get<float>() * 0.8;
        j["stm_size"] = 0;
        j["ltm_size"] = 0;
        j["ltm_avg_importance"] = 0.8;
        j["is_dangerous_mode"] = j["risk"].get<float>() > 0.7;
        j["successful_actions"] = session.successful_actions;
        j["blocked_actions"] = session.blocked_actions;
        return j.dump();
    }
    
    std::string getStats24h() {
        if (!g_auditor) return "{}";
        auto stats = g_auditor->getLogger().getStatsForPeriod("24h");
        nlohmann::json j;
        j["total_actions"] = stats.total_actions;
        j["successful_actions"] = stats.successful_actions;
        j["blocked_actions"] = stats.blocked_actions;
        j["avg_hallucination_risk"] = stats.avg_hallucination_risk;
        return j.dump();
    }
    
    int port_;
    std::string workspace_;
    int server_fd_;
    bool running_;
    std::thread server_thread_;
};

void signalHandler(int signum) {
    std::cout << "\n[Main] Received signal " << signum << ", shutting down..." << std::endl;
    g_running = false;
    if (g_auditor) {
        g_auditor->endSession();
        g_auditor->saveMemoryState();
    }
    exit(0);
}

void printBanner() {
    std::cout << R"(
╔══════════════════════════════════════════════════════════════════╗
║   Neural Agent Audit System v2.0                                 ║
║   Built-in Admin Panel at http://localhost:8080                  ║
╚══════════════════════════════════════════════════════════════════╝
    )" << std::endl;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    printBanner();
    
    // Парсинг аргументов
    std::string workspace = "agent_workspace";
    std::string model_path = "models/Phi-3-mini-4k-instruct-q4.gguf";
    int web_port = 8080;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--workspace" && i + 1 < argc) workspace = argv[++i];
        else if (arg == "--model" && i + 1 < argc) model_path = argv[++i];
        else if (arg == "--port" && i + 1 < argc) web_port = std::stoi(argv[++i]);
        else if (arg == "--help") {
            std::cout << "Usage: ./advanced_neural_system --workspace <path> --port <port>" << std::endl;
            return 0;
        }
    }
    
    // Инициализация конфигурации
    auto& config = AgentConfig::getInstance();
    config.setWorkingDirectory(workspace);
    config.setModelPath(model_path);
    config.loadFromFile();
    config.saveToFile();
    
    std::cout << "[Main] Working directory: " << config.getWorkingDirectory() << std::endl;
    std::cout << "[Main] Admin panel: http://localhost:" << web_port << std::endl;
    
    // Создаём директории
    std::filesystem::create_directories(config.getLogsRawDir());
    std::filesystem::create_directories(config.getLogsAggregatedDir());
    std::filesystem::create_directories(config.getMemoryDir());
    
    // Создаём директорию для web файлов
    std::filesystem::create_directories("web");
    
    // Инициализация нейросети
    NeuralFieldSystem nfs(0.01);
    g_nfs = &nfs;
    
    std::mt19937 rng(std::random_device{}());
    nfs.initialize(rng);
    nfs.setOperatingMode(OperatingMode::NORMAL);
    
    std::cout << "[Main] Neural network initialized" << std::endl;
    
    // Инициализация аудитора
    AgentAuditBridge auditor(nfs);
    g_auditor = &auditor;
    auditor.setWorkingDirectory(workspace);
    
    // Настройка ограничений
    auto constraints = config.getConstraints();
    for (const auto& constraint : constraints) {
        auditor.enableConstraint(constraint, true);
        std::cout << "[Main] Enabled constraint: " << constraint << std::endl;
    }
    auditor.loadMemoryState();
    
    // Запуск HTTP сервера для админ панели
    HttpServer http_server(web_port, workspace);
    if (!http_server.start()) {
        std::cerr << "[Main] Failed to start HTTP server on port " << web_port << std::endl;
        return 1;
    }
    
    // Запуск сессии
    std::string session_id = "session_" + std::to_string(std::time(nullptr));
    auditor.startSession(session_id, "phi3_mini");
    
    std::cout << "\n[Main] Agent session started: " << session_id << std::endl;
    std::cout << "[Main] Type 'quit' to exit, 'status' for stats\n" << std::endl;
    
    // Основной цикл
    int step = 0;
    std::string command;
    
    while (g_running) {
        std::cout << "\n[Agent] > ";
        std::getline(std::cin, command);
        
        if (command == "quit" || command == "exit") {
            g_running = false;
            break;
        } else if (command == "status") {
            auto& session = auditor.getCurrentSession();
            std::cout << "Session ID: " << session.session_id << std::endl;
            std::cout << "Total steps: " << session.total_steps << std::endl;
            std::cout << "Risk: " << auditor.getCurrentHallucinationRisk() << std::endl;
            std::cout << "Entropy: " << auditor.getCurrentEntropy() << std::endl;
            std::cout << "Accumulated risk: " << session.accumulated_risk << std::endl;
        } else if (command == "save") {
            auditor.saveMemoryState();
            config.saveToFile();
            std::cout << "State saved" << std::endl;
        } else if (!command.empty()) {
            AgentAction action;
            action.action = "respond";
            action.action_id = "manual_" + std::to_string(step++);
            action.thought = command;
            action.step_number = step;
            
            auto verdict = auditor.auditAction(action);
            if (verdict.allowed) {
                std::cout << "[Agent] Action allowed (risk: " << verdict.hallucination_risk << ")" << std::endl;
                auditor.reportActionResult(action, true, "Command executed");
            } else {
                std::cout << "[Agent] Action BLOCKED: " << verdict.reason << std::endl;
            }
        }
    }
    
    // Завершение
    std::cout << "\n[Main] Shutting down..." << std::endl;
    auditor.endSession();
    auditor.saveMemoryState();
    config.saveToFile();
    http_server.stop();
    
    std::cout << "[Main] Goodbye!" << std::endl;
    return 0;
}