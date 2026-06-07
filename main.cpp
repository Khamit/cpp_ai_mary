// main.cpp - ИСПРАВЛЕННАЯ ВЕРСИЯ
#include "core/NeuralFieldSystem.hpp"
#include "core/AgentAuditBridge.hpp"
#include "server/HttpServer.hpp"
#include "server/ApiHandlers.hpp"
#include "server/AgentRegistry.hpp"
#include "application/AgentConfig.hpp"
#include <iostream>
#include <signal.h>
#include <filesystem>
#include <atomic>
#include <thread>

NeuralFieldSystem* g_nfs = nullptr;
AgentAuditBridge* g_auditor = nullptr;
HttpServer* g_server = nullptr;
std::atomic<bool> g_running(true);
std::atomic<bool> g_shutdown_initiated(false);  // Защита от двойного завершения

void signalHandler(int signum) {
    // Защита от повторных вызовов
    static std::atomic_flag signal_handled = ATOMIC_FLAG_INIT;
    if (signal_handled.test_and_set()) {
        // Повторный сигнал - принудительный выход
        std::cout << "\n[Main] Force exit..." << std::endl;
        std::_Exit(1);
    }
    
    std::cout << "\n[Main] Received signal " << signum << ", shutting down..." << std::endl;
    g_running = false;
    
    // НЕ вызываем endSession и saveMemoryState здесь - они в основном потоке
    // Только устанавливаем флаг для остановки
    if (g_server) {
        g_server->stop();  // Это безопасно вызвать из сигнала
    }
}

void printBanner() {
    std::cout << R"(
╔══════════════════════════════════════════════════════════════════╗
║   Neural Agent Audit System v3.0                                 ║
║   Multi-Agent Audit & Orchestration Platform                     ║
║   API: http://localhost:8080                                     ║
╚══════════════════════════════════════════════════════════════════╝
    )" << std::endl;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGPIPE, SIG_IGN);  // Игнорируем broken pipe
    
    printBanner();
    
    // Парсинг аргументов
    std::string workspace = "agent_workspace";
    int web_port = 8080;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--workspace" && i + 1 < argc) workspace = argv[++i];
        else if (arg == "--port" && i + 1 < argc) web_port = std::stoi(argv[++i]);
        else if (arg == "--help") {
            std::cout << "Usage: ./advanced_neural_system --workspace <path> --port <port>" << std::endl;
            return 0;
        }
    }
    
    // Инициализация
    auto& config = AgentConfig::getInstance();
    config.setWorkingDirectory(workspace);
    config.loadFromFile();
    
    // Создаём директории
    std::filesystem::create_directories(workspace + "/logs/raw");
    std::filesystem::create_directories(workspace + "/logs/aggregated");
    std::filesystem::create_directories(workspace + "/memory");
    std::filesystem::create_directories("web");
    
    // Инициализация AgentRegistry
    AgentRegistryInitializer registryInit(workspace);
    
    // Инициализация нейросети
    NeuralFieldSystem nfs(0.01);
    g_nfs = &nfs;
    
    std::mt19937 rng(std::random_device{}());
    nfs.initialize(rng);
    nfs.setOperatingMode(OperatingMode::NORMAL);
    
    // Инициализация аудитора
    AgentAuditBridge auditor(nfs);
    g_auditor = &auditor;
    auditor.setWorkingDirectory(workspace);
    
    // Загрузка ограничений
    for (const auto& constraint : config.getConstraints()) {
        auditor.enableConstraint(constraint, true);
    }
    auditor.loadMemoryState();
    
    // Запуск HTTP сервера
    HttpServer server(web_port, workspace, &nfs, &auditor);
    g_server = &server;
    server.setRunningFlag(&g_running);
    
    if (!server.start()) {
        std::cerr << "[Main] Failed to start HTTP server" << std::endl;
        return 1;
    }
    
    // Запуск сессии
    std::string session_id = "session_" + std::to_string(std::time(nullptr));
    auditor.startSession(session_id, "neural_audit");
    
    std::cout << "\n[Main] System ready!" << std::endl;
    std::cout << "[Main] Admin panel: http://localhost:" << web_port << std::endl;
    std::cout << "[Main] API: http://localhost:" << web_port << "/api/" << std::endl;
    std::cout << "[Main] Type 'quit' to exit\n" << std::endl;
    
    // REPL для ручных команд - ПОЛНОСТЬЮ ИСПРАВЛЕННЫЙ ЦИКЛ
    std::string command;
    while (g_running.load()) {
        std::cout << "[Console] > ";
        std::cout.flush();
        
        // Используем неблокирующую проверку для std::cin
        if (!std::getline(std::cin, command)) {
            // EOF или ошибка ввода
            if (std::cin.eof()) {
                // Ctrl+D - выходим
                break;
            }
            std::cin.clear();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // Удаляем лишние пробелы более эффективно
        size_t start = command.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            continue;  // Пустая строка
        }
        size_t end = command.find_last_not_of(" \t\r\n");
        command = command.substr(start, end - start + 1);
        
        if (command == "quit" || command == "exit") {
            break;
        } else if (command == "status") {
            auto& session = auditor.getCurrentSession();
            std::cout << "Session: " << session.session_id << std::endl;
            std::cout << "Steps: " << session.total_steps << std::endl;
            std::cout << "Risk: " << auditor.getCurrentHallucinationRisk() << std::endl;
            std::cout << "Entropy: " << auditor.getCurrentEntropy() << std::endl;
        } else if (command == "agents") {
            std::cout << AgentRegistry::getInstance().getAllAgents().dump(2) << std::endl;
        } else if (command == "help") {
            std::cout << "Available commands: quit, exit, status, agents, help" << std::endl;
        } else if (!command.empty()) {
            std::cout << "Unknown command: '" << command << "'. Type 'help' for available commands." << std::endl;
        }
    }
    
    // Завершение - только один раз
    if (g_shutdown_initiated.exchange(true)) {
        // Уже завершаемся
        return 0;
    }
    
    std::cout << "\n[Main] Shutting down gracefully..." << std::endl;
    
    // Останавливаем HTTP сервер (если ещё не остановлен)
    if (g_server) {
        g_server->stop();
    }
    
    // Завершаем сессию аудитора и сохраняем состояние
    if (g_auditor) {
        g_auditor->endSession();
        g_auditor->saveMemoryState();
    }
    
    std::cout << "[Main] Goodbye!" << std::endl;
    return 0;
}