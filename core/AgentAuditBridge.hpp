#pragma once

// AgentAuditBridge.hpp
// Внешний модуль-адаптер между LLM-агентом и нейросетью-аудитором
// 
// Задачи:
// 1. Конвертировать действия агента в эмбеддинги для INPUT_GROUP
// 2. Отслеживать энтропию и предотвращать бесконечные циклы
// 3. Вычислять риск галлюцинаций
// 4. Блокировать опасные действия
// 5. Логировать все шаги для анализа
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <optional>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "application/AgentConfig.hpp"

static constexpr int GROUP_SIZE = 32;

// Forward declarations
class NeuralFieldSystem;

// ============================================================================
// СТРУКТУРЫ ДАННЫХ ДЛЯ АУДИТА
// ============================================================================

/**
 * @struct AgentAction
 * @brief Действие агента, которое нужно проверить
 */
struct AgentAction {
    std::string thought;        // мысль агента перед действием
    std::string action;         // название действия (call_tool, think, respond)
    std::string tool_name;      // имя инструмента (если action == call_tool)
    std::vector<float> arguments; // аргументы действия (эмбеддинг)
    int step_number = 0;            // номер шага в текущей сессии
    std::chrono::steady_clock::time_point timestamp;

    // Уникальный ID для трекинга
    std::string action_id;
};

/**
 * @struct AuditVerdict
 * @brief Результат проверки действия агентом
 */
struct AuditVerdict {
    bool allowed = true;               // разрешено ли действие
    float hallucination_risk = 0.0;   // риск галлюцинации [0,1]
    float entropy_contribution = 0.0; // вклад в энтропию системы
    std::string reason;         // причина запрета (если не разрешено)
    std::string suggested_action; // альтернативное действие (если есть)
};

/**
 * @struct AgentSession
 * @brief Сессия работы агента (для контекста)
 */
struct AgentSession {
    std::string session_id;
    std::string agent_name;
    int total_steps = 0;
    float accumulated_risk = 0.0f;
    float average_entropy = 0.5f;
    std::deque<float> recent_surprise;
    bool is_dangerous_mode = false;
    
    // Для статистики
    int successful_actions = 0;
    int blocked_actions = 0;
    int warning_count = 0;
    std::chrono::system_clock::time_point start_time;
};

// ============================================================================
// СИСТЕМА ОГРАНИЧЕНИЙ (с возможностью включения/выключения)
// ============================================================================

/**
 * @struct AuditConstraint
 * @brief Ограничение, которое можно включать/выключать
 */
struct AuditConstraint {
    bool enabled = false;
    std::string name;
    std::string description;
    
    // Параметры ограничения
    float threshold = 0.0f;
    int limit = 0;
    std::string action_type;  // think, call_tool, respond
    
    // Для логирования
    int violations_count = 0;
    float last_violation_value = 0.0f;
};

/**
 * @struct AuditConstraintsConfig
 * @brief Конфигурация всех ограничений
 */
struct AuditConstraintsConfig {
    // === ЛИМИТЫ НА ОТВЕТЫ (можно включать/выключать) ===
    AuditConstraint response_length_limit_300;   // 300 символов
    AuditConstraint response_length_limit_600;   // 600 символов
    AuditConstraint response_length_limit_1000;  // 1000 символов
    
    // === ЛИМИТЫ НА УВЕРЕННОСТЬ ===
    AuditConstraint confidence_threshold_30;     // 30% уверенности
    AuditConstraint confidence_threshold_50;     // 50% уверенности
    AuditConstraint confidence_threshold_70;     // 70% уверенности
    AuditConstraint confidence_threshold_90;     // 90% уверенности
    
    // === ДРУГИЕ ОГРАНИЧЕНИЯ ===
    AuditConstraint max_steps_per_second;        // шагов в секунду
    AuditConstraint max_tokens_per_response;     // токенов в ответе
    AuditConstraint max_tool_calls_per_cycle;    // вызовов инструментов за цикл
    
    AuditConstraintsConfig() {
        // Инициализация с описаниями
        response_length_limit_300 = {false, "response_length_300", "Ограничение ответа 300 символов", 300.0f, 300, ""};
        response_length_limit_600 = {false, "response_length_600", "Ограничение ответа 600 символов", 600.0f, 600, ""};
        response_length_limit_1000 = {false, "response_length_1000", "Ограничение ответа 1000 символов", 1000.0f, 1000, ""};
        
        confidence_threshold_30 = {false, "confidence_30", "Минимальная уверенность 30% для логирования", 0.3f, 0, "respond"};
        confidence_threshold_50 = {false, "confidence_50", "Минимальная уверенность 50% для логирования", 0.5f, 0, "respond"};
        confidence_threshold_70 = {false, "confidence_70", "Минимальная уверенность 70% для логирования", 0.7f, 0, "respond"};
        confidence_threshold_90 = {false, "confidence_90", "Минимальная уверенность 90% для логирования", 0.9f, 0, "respond"};
        
        max_steps_per_second = {false, "max_steps_per_second", "Максимум шагов в секунду", 0.0f, 10, ""};
        max_tokens_per_response = {false, "max_tokens_per_response", "Максимум токенов в ответе", 0.0f, 4096, "respond"};
        max_tool_calls_per_cycle = {false, "max_tool_calls_per_cycle", "Максимум вызовов инструментов за цикл", 0.0f, 10, "call_tool"};
    }
    
    // Включить/выключить ограничение по имени
    void setEnabled(const std::string& name, bool enabled) {
        if (name == "response_length_300") response_length_limit_300.enabled = enabled;
        if (name == "response_length_600") response_length_limit_600.enabled = enabled;
        if (name == "response_length_1000") response_length_limit_1000.enabled = enabled;
        if (name == "confidence_30") confidence_threshold_30.enabled = enabled;
        if (name == "confidence_50") confidence_threshold_50.enabled = enabled;
        if (name == "confidence_70") confidence_threshold_70.enabled = enabled;
        if (name == "confidence_90") confidence_threshold_90.enabled = enabled;
        if (name == "max_steps_per_second") max_steps_per_second.enabled = enabled;
        if (name == "max_tokens_per_response") max_tokens_per_response.enabled = enabled;
        if (name == "max_tool_calls_per_cycle") max_tool_calls_per_cycle.enabled = enabled;
    }
    
    // Получить все активные ограничения
    std::vector<AuditConstraint> getActiveConstraints() const {
        std::vector<AuditConstraint> active;
        if (response_length_limit_300.enabled) active.push_back(response_length_limit_300);
        if (response_length_limit_600.enabled) active.push_back(response_length_limit_600);
        if (response_length_limit_1000.enabled) active.push_back(response_length_limit_1000);
        if (confidence_threshold_30.enabled) active.push_back(confidence_threshold_30);
        if (confidence_threshold_50.enabled) active.push_back(confidence_threshold_50);
        if (confidence_threshold_70.enabled) active.push_back(confidence_threshold_70);
        if (confidence_threshold_90.enabled) active.push_back(confidence_threshold_90);
        if (max_steps_per_second.enabled) active.push_back(max_steps_per_second);
        if (max_tokens_per_response.enabled) active.push_back(max_tokens_per_response);
        if (max_tool_calls_per_cycle.enabled) active.push_back(max_tool_calls_per_cycle);
        return active;
    }
};


// ============================================================================
// СИСТЕМА АГРЕГИРОВАННОГО ЛОГИРОВАНИЯ
// ============================================================================

/**
 * @struct AggregatedStats
 * @brief Агрегированная статистика за период
 */
struct AggregatedStats {
    std::string period;  // "5h", "12h", "24h", "week", "month", "year"
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    
    // Счётчики
    int total_actions = 0;
    int successful_actions = 0;
    int blocked_actions = 0;
    int warnings = 0;
    
    // Риски и энтропия
    float avg_hallucination_risk = 0.0f;
    float max_hallucination_risk = 0.0f;
    float avg_entropy = 0.0f;
    float min_entropy = 1.0f;
    float max_entropy = 0.0f;
    
    // По типам действий
    std::unordered_map<std::string, int> actions_by_type;
    
    // По инструментам
    std::unordered_map<std::string, int> tool_usage;
    
    // Ограничения
    std::unordered_map<std::string, int> constraint_violations;
    
    // Конвертация в JSON
    nlohmann::json toJson() const {
        nlohmann::json j;
        j["period"] = period;
        j["total_actions"] = total_actions;
        j["successful_actions"] = successful_actions;
        j["blocked_actions"] = blocked_actions;
        j["warnings"] = warnings;
        j["avg_hallucination_risk"] = avg_hallucination_risk;
        j["max_hallucination_risk"] = max_hallucination_risk;
        j["avg_entropy"] = avg_entropy;
        j["min_entropy"] = min_entropy;
        j["max_entropy"] = max_entropy;
        j["actions_by_type"] = actions_by_type;
        j["tool_usage"] = tool_usage;
        j["constraint_violations"] = constraint_violations;
        return j;
    }
};


/**
 * @class AggregatedLogger
 * @brief Система агрегированного логирования с разными интервалами
 */
class AggregatedLogger {
public:
    AggregatedLogger(const std::string& raw_logs_dir = "logs/raw",
                     const std::string& aggregated_dir = "logs/aggregated");
    ~AggregatedLogger();
    
    // Логирование сырого действия
    void logRaw(const AgentAction& action, const AuditVerdict& verdict, float entropy, const std::string& response = "");
    
    // Агрегация по интервалам
    void aggregate();
    
    // Получить статистику за период
    AggregatedStats getStatsForPeriod(const std::string& period) const;
    
    // Получить все доступные периоды
    std::vector<std::string> getAvailablePeriods() const;
    
    // Очистка старых логов (старше N дней)
    void cleanupOldLogs(int max_days = 30);
    
private:
    std::string raw_logs_dir_;
    std::string aggregated_dir_;
    
    // Текущие агрегаты
    std::unordered_map<std::string, AggregatedStats> current_aggregates_;
    
    // Временные буферы для сырых логов
    struct RawLogEntry {
        std::chrono::system_clock::time_point timestamp;
        AgentAction action;
        AuditVerdict verdict;
        float entropy;
        std::string response;
    };
    std::deque<RawLogEntry> raw_buffer_;
    static constexpr int MAX_BUFFER_SIZE = 10000;
    
    // Вспомогательные методы
    std::string getPeriodKey(const std::chrono::system_clock::time_point& time, const std::string& period);
    void flushBuffer();
    void updateAggregate(AggregatedStats& stats, const RawLogEntry& entry);
    void saveAggregate(const std::string& period, const AggregatedStats& stats);
    AggregatedStats loadAggregate(const std::string& period);
    
    // Потокобезопасность
    mutable std::mutex mutex_;
};
// ============================================================================
// ОСНОВНОЙ КЛАСС — АУДИТОР АГЕНТОВ
// ============================================================================

class AgentAuditBridge {
public:
    AgentAuditBridge(NeuralFieldSystem& neural_system);
    ~AgentAuditBridge() = default;

    // ===== ОСНОВНЫЕ МЕТОДЫ =====
    AuditVerdict auditAction(const AgentAction& action);
    void reportActionResult(const AgentAction& action, bool success, const std::string& observation);
    void startSession(const std::string& session_id, const std::string& agent_name);
    void endSession();

    // ===== УПРАВЛЕНИЕ ОГРАНИЧЕНИЯМИ =====
    void enableConstraint(const std::string& name, bool enabled);
    bool isConstraintEnabled(const std::string& name) const;
    const AuditConstraintsConfig& getConstraints() const { return constraints_; }
    void setConstraints(const AuditConstraintsConfig& cfg) { constraints_ = cfg; }
    
    // ===== ПОЛУЧЕНИЕ ДАННЫХ =====
    const AgentSession& getCurrentSession() const { return current_session_; }
    float getCurrentHallucinationRisk() const;
    float getCurrentEntropy() const;
    void resetRiskAccumulator();

    // ===== ДОСТУП К ЛОГГЕРУ =====
    AggregatedLogger& getLogger() { return logger_; }
    const AggregatedLogger& getLogger() const { return logger_; }
    
    // ===== НАСТРОЙКА =====
    struct Config {
        float entropy_danger_threshold = 0.85f;
        float entropy_warning_threshold = 0.65f;
        float risk_block_threshold = 0.75f;
        float risk_warning_threshold = 0.5f;
        int max_steps_per_session = 100;
        float max_accumulated_risk = 5.0f;
        int max_consecutive_similar_actions = 3;
        float min_entropy_change_to_continue = 0.02f;
        std::vector<std::string> blocked_actions = {
            "delete_file", "rm", "drop_database", "shutdown", "reboot",
            "format", "chmod_777", "sudo", "eval", "exec"
        };
    };
    
    void setConfig(const Config& cfg) { config_ = cfg; }
    const Config& getConfig() const { return config_; }
    
    // ===== ЛОГИРОВАНИЕ =====
    using AuditCallback = std::function<void(const AgentAction&, const AuditVerdict&)>;
    void setAuditCallback(AuditCallback callback) { audit_callback_ = callback; }
    const std::deque<std::pair<AgentAction, AuditVerdict>>& getHistory() const { return history_; }

    // ===== РАБОЧАЯ ПАПКА =====
    void setWorkingDirectory(const std::string& path);
    std::string getWorkingDirectory() const;
    
    // ===== СОХРАНЕНИЕ/ЗАГРУЗКА =====
    bool loadMemoryState();
    bool saveMemoryState();
    
private:
    std::string generateActionId();
    
    // ===== ПРИВАТНЫЕ МЕТОДЫ =====
    std::vector<float> actionToEmbedding(const AgentAction& action);
    float computeHallucinationRisk(const AgentAction& action, float surprise);
    bool isInfiniteLoop(const AgentAction& action);
    bool exceedsLimits(const AgentAction& action);
    void updateEntropyFromNeuralSystem();
    bool isBlockedAction(const std::string& action);
    bool checkConstraints(const AgentAction& action, const std::string& response, AuditVerdict& verdict);
    float getConfidenceFromResponse(const std::string& response);
    float normalizeValue(float value, float min, float max);
    float cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b);
    
    // ===== ПОЛЯ =====
    NeuralFieldSystem& neural_system_;
    AgentSession current_session_;
    Config config_;
    AuditConstraintsConfig constraints_;
    AuditCallback audit_callback_;
    AggregatedLogger logger_;
    
    std::deque<std::pair<AgentAction, AuditVerdict>> history_;
    static constexpr int MAX_HISTORY = 1000;
    
    std::deque<std::vector<float>> recent_embeddings_;
    static constexpr int CYCLE_DETECTION_WINDOW = 10;
    
    float cached_entropy_ = 0.5f;
    float cached_surprise_ = 0.5f;
    int last_entropy_update_step_ = -1;
    
    int consecutive_similar_actions_ = 0;
    std::string last_action_name_;
    std::vector<float> last_action_embedding_;

    int step_counter_ = 0;
    std::chrono::steady_clock::time_point last_step_time_;
    
    static std::atomic<int> action_id_counter_;
};