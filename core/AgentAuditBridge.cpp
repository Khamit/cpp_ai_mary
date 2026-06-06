#include "AgentAuditBridge.hpp"
#include "NeuralFieldSystem.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <random>
#include <iostream>
#include <chrono>
#include <nlohmann/json.hpp>
#include <iomanip>
#include <sstream>
#include <regex>
#include <atomic>
#include <filesystem>
#include <ctime> 
#include <unistd.h>

std::atomic<int> AgentAuditBridge::action_id_counter_{0};

// ============================================================================
// КОНСТРУКТОР LOGGER
// ============================================================================

AggregatedLogger::AggregatedLogger(const std::string& raw_logs_dir, const std::string& aggregated_dir)
    : raw_logs_dir_(raw_logs_dir), aggregated_dir_(aggregated_dir) {
    std::filesystem::create_directories(raw_logs_dir_);
    std::filesystem::create_directories(aggregated_dir_);
    
    // Загружаем существующие агрегаты
    for (const auto& period : {"5h", "12h", "24h", "week", "month", "year"}) {
        current_aggregates_[period] = loadAggregate(period);
    }
}

AggregatedLogger::~AggregatedLogger() {
    flushBuffer();
    aggregate();
}

// ============================================================================
// КОНСТРУКТОР
// ============================================================================

AgentAuditBridge::AgentAuditBridge(NeuralFieldSystem& neural_system)
    : neural_system_(neural_system)
{
    // Инициализация конфигурации по умолчанию
    config_.blocked_actions = {
        "delete_file", "rm", "drop_database", "shutdown", "reboot",
        "format", "chmod_777", "sudo", "eval", "exec"
    };
}

// ============================================================================
// ЛОГИРОВАНИЕ
// ============================================================================

void AggregatedLogger::logRaw(const AgentAction& action, const AuditVerdict& verdict, float entropy, const std::string& response) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    RawLogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.action = action;
    entry.verdict = verdict;
    entry.entropy = entropy;
    entry.response = response;
    
    raw_buffer_.push_back(entry);
    
    if (raw_buffer_.size() >= MAX_BUFFER_SIZE) {
        flushBuffer();
    }
}

void AggregatedLogger::flushBuffer() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Сохраняем сырые логи в файл по дате
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&time_t);
    
    std::ostringstream filename;
    filename << raw_logs_dir_ << "/raw_" 
             << std::put_time(tm, "%Y%m%d_%H%M%S") << ".json";
    
    nlohmann::json j;
    for (const auto& entry : raw_buffer_) {
        nlohmann::json entry_json;
        entry_json["timestamp"] = std::chrono::system_clock::to_time_t(entry.timestamp);
        entry_json["action_id"] = entry.action.action_id;
        entry_json["action_type"] = entry.action.action;
        entry_json["tool_name"] = entry.action.tool_name;
        entry_json["hallucination_risk"] = entry.verdict.hallucination_risk;
        entry_json["allowed"] = entry.verdict.allowed;
        entry_json["entropy"] = entry.entropy;
        entry_json["response"] = entry.response;
        j.push_back(entry_json);
    }
    
    std::ofstream file(filename.str());
    if (file.is_open()) {
        file << j.dump(4);
    }
    
    // Обновляем агрегаты
    for (auto& [period, stats] : current_aggregates_) {
        for (const auto& entry : raw_buffer_) {
            if (entry.timestamp >= stats.start_time && entry.timestamp <= stats.end_time) {
                updateAggregate(stats, entry);
            }
        }
        saveAggregate(period, stats);
    }
    
    raw_buffer_.clear();
}

void AggregatedLogger::updateAggregate(AggregatedStats& stats, const RawLogEntry& entry) {
    stats.total_actions++;
    if (entry.verdict.allowed) {
        stats.successful_actions++;
    } else {
        stats.blocked_actions++;
    }
    
    stats.avg_hallucination_risk = (stats.avg_hallucination_risk * (stats.total_actions - 1) + entry.verdict.hallucination_risk) / stats.total_actions;
    stats.max_hallucination_risk = std::max(stats.max_hallucination_risk, entry.verdict.hallucination_risk);
    
    stats.avg_entropy = (stats.avg_entropy * (stats.total_actions - 1) + entry.entropy) / stats.total_actions;
    stats.min_entropy = std::min(stats.min_entropy, entry.entropy);
    stats.max_entropy = std::max(stats.max_entropy, entry.entropy);
    
    stats.actions_by_type[entry.action.action]++;
    if (!entry.action.tool_name.empty()) {
        stats.tool_usage[entry.action.tool_name]++;
    }
    
    if (!entry.verdict.reason.empty() && !entry.verdict.allowed) {
        stats.constraint_violations[entry.verdict.reason]++;
    }
}

void AggregatedLogger::aggregate() {
    flushBuffer();
    
    // Обновляем все периоды
    auto now = std::chrono::system_clock::now();
    
    // 5 часов
    current_aggregates_["5h"].start_time = now - std::chrono::hours(5);
    current_aggregates_["5h"].end_time = now;
    current_aggregates_["5h"].period = "5h";
    
    // 12 часов
    current_aggregates_["12h"].start_time = now - std::chrono::hours(12);
    current_aggregates_["12h"].end_time = now;
    current_aggregates_["12h"].period = "12h";
    
    // 24 часа
    current_aggregates_["24h"].start_time = now - std::chrono::hours(24);
    current_aggregates_["24h"].end_time = now;
    current_aggregates_["24h"].period = "24h";
    
    // Неделя
    current_aggregates_["week"].start_time = now - std::chrono::hours(168);
    current_aggregates_["week"].end_time = now;
    current_aggregates_["week"].period = "week";
    
    // Месяц
    current_aggregates_["month"].start_time = now - std::chrono::hours(720);
    current_aggregates_["month"].end_time = now;
    current_aggregates_["month"].period = "month";
    
    // Год
    current_aggregates_["year"].start_time = now - std::chrono::hours(8760);
    current_aggregates_["year"].end_time = now;
    current_aggregates_["year"].period = "year";
}

void AggregatedLogger::saveAggregate(const std::string& period, const AggregatedStats& stats) {
    std::string filename = aggregated_dir_ + "/" + period + ".json";
    std::ofstream file(filename);
    if (file.is_open()) {
        file << stats.toJson().dump(4);
    }
}

// ============================================================================
// ПРОВЕРКА ОГРАНИЧЕНИЙ
// ============================================================================

bool AgentAuditBridge::checkConstraints(const AgentAction& action, const std::string& response, AuditVerdict& verdict) {
    auto active = constraints_.getActiveConstraints();
    
    for (const auto& constraint : active) {
        // Проверка ограничений на длину ответа
        if (constraint.name.find("response_length") != std::string::npos) {
            int response_len = (int)response.length();
            if (response_len > constraint.limit) {
                verdict.allowed = false;
                verdict.reason = "Response length (" + std::to_string(response_len) + 
                                ") exceeds limit (" + std::to_string(constraint.limit) + ")";
                verdict.hallucination_risk = std::min(1.0f, verdict.hallucination_risk + 0.3f);
                return false;
            }
        }
        
        // Проверка ограничений на уверенность
        if (constraint.name.find("confidence") != std::string::npos) {
            if (constraint.action_type.empty() || constraint.action_type == action.action) {
                float confidence = getConfidenceFromResponse(response);
                if (confidence < constraint.threshold) {
                    // Не блокируем, только логируем
                    verdict.reason = "Low confidence (" + std::to_string(confidence) + 
                                    ") below threshold (" + std::to_string(constraint.threshold) + ")";
                    // Увеличиваем риск, но не блокируем
                    verdict.hallucination_risk = std::max(verdict.hallucination_risk, 1.0f - confidence);
                }
            }
        }
        
        // Проверка ограничений на шаги в секунду
        if (constraint.name == "max_steps_per_second") {
            auto now = std::chrono::steady_clock::now();
            if (step_counter_ > 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_step_time_).count();
                if (elapsed < 1) {
                    float steps_per_sec = 1.0f / (elapsed + 0.001f);
                    if (steps_per_sec > constraint.limit) {
                        verdict.allowed = false;
                        verdict.reason = "Too many steps per second (" + std::to_string(steps_per_sec) + 
                                        " > " + std::to_string(constraint.limit) + ")";
                        return false;
                    }
                }
            }
            last_step_time_ = now;
        }
        
        // Проверка ограничений на вызовы инструментов
        if (constraint.name == "max_tool_calls_per_cycle") {
            if (action.action == "call_tool") {
                static std::unordered_map<std::string, int> tool_calls_in_cycle;
                tool_calls_in_cycle[action.tool_name]++;
                if (tool_calls_in_cycle[action.tool_name] > constraint.limit) {
                    verdict.allowed = false;
                    verdict.reason = "Too many calls to " + action.tool_name + 
                                    " (" + std::to_string(tool_calls_in_cycle[action.tool_name]) + 
                                    " > " + std::to_string(constraint.limit) + ")";
                    return false;
                }
            }
        }
    }
    
    return true;
}

float AgentAuditBridge::getConfidenceFromResponse(const std::string& response) {
    // Извлечение процента уверенности из ответа
    // Ищем паттерны типа "уверенность: 85%" или "confidence: 0.85"
    std::regex confidence_pattern(R"((?:уверенность|confidence)[:\s]*(\d+(?:\.\d+)?)%?)", std::regex::icase);
    std::smatch match;
    if (std::regex_search(response, match, confidence_pattern)) {
        float confidence = std::stof(match[1].str());
        if (confidence > 1.0f) confidence /= 100.0f;
        return std::clamp(confidence, 0.0f, 1.0f);
    }
    
    // Если не нашли, используем энтропию как прокси уверенности
    return 1.0f - cached_entropy_;
}

std::string AgentAuditBridge::generateActionId() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return std::to_string(ms) + "_" + std::to_string(action_id_counter_++);
}

// ============================================================================
// ОСНОВНЫЕ МЕТОДЫ
// ============================================================================

AuditVerdict AgentAuditBridge::auditAction(const AgentAction& action) {
    AuditVerdict verdict;
    verdict.allowed = true;
    verdict.hallucination_risk = 0.0f;
    verdict.entropy_contribution = 0.0f;
    verdict.reason = "";
    
    // Обновляем энтропию из нейросети
    updateEntropyFromNeuralSystem();
    
    // 1. Проверка на опасные действия
    if (isBlockedAction(action.action) || isBlockedAction(action.tool_name)) {
        verdict.allowed = false;
        verdict.hallucination_risk = 0.95f;
        verdict.reason = "Action blocked by security policy: " + 
                         (action.action.empty() ? action.tool_name : action.action);
        verdict.suggested_action = "Request human approval or use read-only alternative";
        
        if (audit_callback_) audit_callback_(action, verdict);
        return verdict;
    }
    
    // 2. Проверка на бесконечный цикл
    if (isInfiniteLoop(action)) {
        verdict.allowed = false;
        verdict.hallucination_risk = 0.85f;
        verdict.reason = "Detected potential infinite loop: repeating same action pattern";
        verdict.suggested_action = "Change strategy or break down the task";
        
        if (audit_callback_) audit_callback_(action, verdict);
        return verdict;
    }
    
    // 3. Проверка на превышение лимитов
    if (exceedsLimits(action)) {
        verdict.allowed = false;
        verdict.hallucination_risk = cached_surprise_;
        verdict.reason = "Session limits exceeded: max steps or accumulated risk";
        verdict.suggested_action = "Start new session or reset context";
        
        if (audit_callback_) audit_callback_(action, verdict);
        return verdict;
    }
    
    // 4. Вычисление риска галлюцинации
    verdict.hallucination_risk = computeHallucinationRisk(action, cached_surprise_);

    // 4.5 Проверка ограничений (нужно передать response, пока пустую)
    std::string empty_response;
    if (!checkConstraints(action, empty_response, verdict)) {
        // Если ограничение заблокировало действие
        if (audit_callback_) audit_callback_(action, verdict);
        return verdict;
    }
    
    // 5. Оценка вклада в энтропию
    auto embedding = actionToEmbedding(action);
    verdict.entropy_contribution = std::abs(cached_entropy_ - 0.5f) * 2.0f;
    
    // 6. Применяем настройки
    if (verdict.hallucination_risk > config_.risk_block_threshold) {
        verdict.allowed = false;
        verdict.reason = "Hallucination risk too high (" + 
                         std::to_string(verdict.hallucination_risk) + " > " +
                         std::to_string(config_.risk_block_threshold) + ")";
    } else if (verdict.hallucination_risk > config_.risk_warning_threshold) {
        // Предупреждение, но разрешаем
        verdict.reason = "Warning: high hallucination risk (" + 
                         std::to_string(verdict.hallucination_risk) + ")";
    }
    
    // 7. Обновляем счётчики
    if (verdict.allowed) {
        current_session_.total_steps++;
        current_session_.accumulated_risk += verdict.hallucination_risk;
        
        // Обновляем скользящее среднее энтропии
        current_session_.recent_surprise.push_back(cached_surprise_);
        if (current_session_.recent_surprise.size() > 20) {
            current_session_.recent_surprise.pop_front();
        }
        float sum = 0.0f;
        for (float s : current_session_.recent_surprise) sum += s;
        current_session_.average_entropy = sum / current_session_.recent_surprise.size();
        
        // Проверка на опасный режим
        current_session_.is_dangerous_mode = (cached_entropy_ > config_.entropy_danger_threshold);
    }
    
    // Сохраняем в историю
    history_.push_back({action, verdict});
    if (history_.size() > MAX_HISTORY) history_.pop_front();
    
    // Вызываем callback
    if (audit_callback_) audit_callback_(action, verdict);
    
    return verdict;
}

void AgentAuditBridge::setWorkingDirectory(const std::string& path) {
    AgentConfig::getInstance().setWorkingDirectory(path);
    std::cout << "[AgentAudit] Working directory set to: " << path << std::endl;
}

std::string AgentAuditBridge::getWorkingDirectory() const {
    return AgentConfig::getInstance().getWorkingDirectory();
}

bool AgentAuditBridge::loadMemoryState() {
    auto& cfg = AgentConfig::getInstance();
    std::cout << "[AgentAudit] Loading memory from: " << cfg.getMemoryDir() << std::endl;
    // TODO: Реализовать загрузку
    return true;
}

bool AgentAuditBridge::saveMemoryState() {
    auto& cfg = AgentConfig::getInstance();
    std::cout << "[AgentAudit] Saving memory to: " << cfg.getMemoryDir() << std::endl;
    // TODO: Реализовать сохранение
    return true;
}

void AgentAuditBridge::reportActionResult(const AgentAction& action, bool success, const std::string& observation) {
    // Отправляем результат в нейросеть как награду
    float reward = success ? 0.8f : 0.2f;
    
    // Корректируем награду на основе энтропии
    if (cached_entropy_ > config_.entropy_warning_threshold) {
        reward *= 0.5f;  // Высокая энтропия = штраф
    }
    
    // Создаём эмбеддинг для обратной связи
    auto embedding = actionToEmbedding(action);
    
    // Добавляем наблюдение в эмбеддинг (упрощённо — хеш наблюдения)
    std::hash<std::string> hasher;
    size_t obs_hash = hasher(observation);
    for (int i = 0; i < 4 && i < (int)embedding.size(); ++i) {
        embedding[i] = (embedding[i] + (obs_hash >> (i * 8)) % 100 / 100.0f) * 0.5f;
    }
    
    // Отправляем в INPUT_GROUP
    neural_system_.setInputText(embedding);
    
    // Делаем шаг нейросети с полученной наградой
    neural_system_.step(reward, neural_system_.getCurrentStep() + 1);
    
    // Обновляем кэш
    updateEntropyFromNeuralSystem();
    
    if (current_session_.total_steps % 100 == 0) {
        std::cout << "[AgentAudit] Step " << current_session_.total_steps 
                  << " | Success=" << success
                  << " | Risk=" << getCurrentHallucinationRisk()
                  << " | Entropy=" << cached_entropy_
                  << std::endl;
    }
}

void AgentAuditBridge::startSession(const std::string& session_id, const std::string& agent_name) {
    current_session_.session_id = session_id;
    current_session_.agent_name = agent_name;
    current_session_.total_steps = 0;
    current_session_.accumulated_risk = 0.0f;
    current_session_.average_entropy = 0.5f;
    current_session_.recent_surprise.clear();
    current_session_.is_dangerous_mode = false;
    
    consecutive_similar_actions_ = 0;
    recent_embeddings_.clear();
    
    std::cout << "[AgentAudit] Session started: " << session_id 
              << " for agent: " << agent_name << std::endl;
}

void AgentAuditBridge::endSession() {
    std::cout << "[AgentAudit] Session ended: " << current_session_.session_id
              << " | Total steps: " << current_session_.total_steps
              << " | Avg risk: " << (current_session_.accumulated_risk / std::max(1, current_session_.total_steps))
              << " | Avg entropy: " << current_session_.average_entropy
              << std::endl;
    
    current_session_ = AgentSession();
    consecutive_similar_actions_ = 0;
    recent_embeddings_.clear();
}

float AgentAuditBridge::getCurrentHallucinationRisk() const {
    // Риск = surprise * (1 - quality) * entropy_factor
    float surprise = cached_surprise_;
    float quality = neural_system_.lastSignal().quality;
    float entropy_factor = std::min(1.0f, cached_entropy_ * 1.5f);
    
    float risk = surprise * (1.0f - quality) * entropy_factor;
    return std::clamp(risk, 0.0f, 1.0f);
}

float AgentAuditBridge::getCurrentEntropy() const {
    return cached_entropy_;
}

void AgentAuditBridge::resetRiskAccumulator() {
    current_session_.accumulated_risk = 0.0f;
    consecutive_similar_actions_ = 0;
    recent_embeddings_.clear();
    
    std::cout << "[AgentAudit] Risk accumulator reset" << std::endl;
}

// ============================================================================
// ПРИВАТНЫЕ МЕТОДЫ
// ============================================================================

std::vector<float> AgentAuditBridge::actionToEmbedding(const AgentAction& action) {
    std::vector<float> embedding(NeuralFieldSystem::GROUP_SIZE, 0.0f);
    
    // Кодируем действие (32-мерный эмбеддинг)
    // [0-7]: тип действия
    // [8-15]: инструмент (хеш)
    // [16-23]: энтропия контекста
    // [24-31]: риск предыдущих шагов
    
    // Тип действия (one-hot)
    int action_type = 0;
    if (action.action == "think") action_type = 0;
    else if (action.action == "call_tool") action_type = 1;
    else if (action.action == "respond") action_type = 2;
    else action_type = 3;
    
    for (int i = 0; i < 8; ++i) {
        embedding[i] = (i == action_type) ? 1.0f : 0.0f;
    }
    
    // Инструмент (хеш в 0-1)
    std::hash<std::string> hasher;
    size_t tool_hash = hasher(action.tool_name);
    for (int i = 0; i < 8; ++i) {
        embedding[8 + i] = ((tool_hash >> i) & 1) ? 0.8f : 0.2f;
    }
    
    // Энтропия контекста
    for (int i = 0; i < 8; ++i) {
        embedding[16 + i] = cached_entropy_;
    }
    
    // Накопленный риск
    float normalized_risk = std::min(1.0f, current_session_.accumulated_risk / config_.max_accumulated_risk);
    for (int i = 0; i < 8; ++i) {
        embedding[24 + i] = normalized_risk;
    }
    
    return embedding;
}

float AgentAuditBridge::computeHallucinationRisk(const AgentAction& action, float surprise) {
    // Базовый риск = текущая неожиданность
    float risk = surprise;
    
    // Корректировка по типу действия
    if (action.action == "call_tool") {
        risk += 0.1f;  // Вызов инструментов опаснее
    } else if (action.action == "respond") {
        risk -= 0.1f;  // Ответы безопаснее
    }
    
    // Корректировка по накопленному риску
    float risk_factor = std::min(1.0f, current_session_.accumulated_risk / config_.max_accumulated_risk);
    risk += risk_factor * 0.2f;
    
    // Корректировка по энтропии
    if (cached_entropy_ > config_.entropy_warning_threshold) {
        risk += (cached_entropy_ - config_.entropy_warning_threshold) * 1.5f;
    }
    
    return std::clamp(risk, 0.0f, 1.0f);
}

bool AgentAuditBridge::isInfiniteLoop(const AgentAction& action) {
    auto embedding = actionToEmbedding(action);
    
    // Проверяем на повторение того же действия
    if (action.action == last_action_name_) {
        float sim = cosineSimilarity(embedding, last_action_embedding_);
        if (sim > 0.95f) {
            consecutive_similar_actions_++;
            if (consecutive_similar_actions_ >= config_.max_consecutive_similar_actions) {
                return true;
            }
        } else {
            consecutive_similar_actions_ = 0;
        }
    } else {
        consecutive_similar_actions_ = 0;
    }
    
    // Проверяем на цикл в истории
    recent_embeddings_.push_back(embedding);
    if (recent_embeddings_.size() > CYCLE_DETECTION_WINDOW) {
        recent_embeddings_.pop_front();
    }
    
    // Ищем повторяющийся паттерн (период 2-4)
    if (recent_embeddings_.size() >= 6) {
        for (int period = 2; period <= 4; ++period) {
            bool is_cycle = true;
            for (size_t i = 0; i < recent_embeddings_.size() - period; ++i) {
                float sim = cosineSimilarity(recent_embeddings_[i], recent_embeddings_[i + period]);
                if (sim < 0.9f) {
                    is_cycle = false;
                    break;
                }
            }
            if (is_cycle && recent_embeddings_.size() >= period * 2) {
                return true;
            }
        }
    }
    
    last_action_name_ = action.action;
    last_action_embedding_ = embedding;
    
    return false;
}

bool AgentAuditBridge::exceedsLimits(const AgentAction& action) {
    // Лимит шагов
    if (current_session_.total_steps >= config_.max_steps_per_session) {
        return true;
    }
    
    // Лимит накопленного риска
    if (current_session_.accumulated_risk >= config_.max_accumulated_risk) {
        return true;
    }
    
    // Проверка на минимальное изменение энтропии (защита от застывания)
    if (current_session_.recent_surprise.size() >= 10) {
        float sum = 0.0f;
        for (float s : current_session_.recent_surprise) sum += s;
        float avg = sum / current_session_.recent_surprise.size();
        
        if (std::abs(avg - cached_surprise_) < config_.min_entropy_change_to_continue &&
            current_session_.total_steps > 20) {
            return true;  // Энтропия не меняется — агент застрял
        }
    }
    
    return false;
}

void AgentAuditBridge::updateEntropyFromNeuralSystem() {
    int current_step = neural_system_.getCurrentStep();
    if (current_step != last_entropy_update_step_) {
        cached_entropy_ = static_cast<float>(neural_system_.getUnifiedEntropy());
        cached_surprise_ = neural_system_.lastSignal().surprise;
        last_entropy_update_step_ = current_step;
    }
}

bool AgentAuditBridge::isBlockedAction(const std::string& action) {
    for (const auto& blocked : config_.blocked_actions) {
        if (action.find(blocked) != std::string::npos) {
            return true;
        }
    }
    return false;
}

float AgentAuditBridge::normalizeValue(float value, float min, float max) {
    return std::clamp((value - min) / (max - min), 0.0f, 1.0f);
}

float AgentAuditBridge::cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) {
    size_t sz = std::min(a.size(), b.size());
    float dot = 0.0f, na = 0.0f, nb = 0.0f;
    for (size_t i = 0; i < sz; ++i) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    float denom = std::sqrt(na) * std::sqrt(nb);
    return (denom < 1e-6f) ? 0.0f : std::clamp(dot / denom, 0.0f, 1.0f);
}

AggregatedStats AggregatedLogger::loadAggregate(const std::string& period) {
    AggregatedStats stats;
    stats.period = period;
    
    std::string filename = aggregated_dir_ + "/" + period + ".json";
    std::ifstream file(filename);
    if (file.is_open()) {
        nlohmann::json j;
        file >> j;
        
        stats.total_actions = j.value("total_actions", 0);
        stats.successful_actions = j.value("successful_actions", 0);
        stats.blocked_actions = j.value("blocked_actions", 0);
        stats.warnings = j.value("warnings", 0);
        stats.avg_hallucination_risk = j.value("avg_hallucination_risk", 0.0f);
        stats.max_hallucination_risk = j.value("max_hallucination_risk", 0.0f);
        stats.avg_entropy = j.value("avg_entropy", 0.0f);
        stats.min_entropy = j.value("min_entropy", 0.0f);
        stats.max_entropy = j.value("max_entropy", 0.0f);
        
        if (j.contains("actions_by_type")) {
            stats.actions_by_type = j["actions_by_type"].get<std::unordered_map<std::string, int>>();
        }
        if (j.contains("tool_usage")) {
            stats.tool_usage = j["tool_usage"].get<std::unordered_map<std::string, int>>();
        }
        if (j.contains("constraint_violations")) {
            stats.constraint_violations = j["constraint_violations"].get<std::unordered_map<std::string, int>>();
        }
    }
    
    return stats;
}

std::vector<std::string> AggregatedLogger::getAvailablePeriods() const {
    std::vector<std::string> periods;
    for (const auto& [period, _] : current_aggregates_) {
        periods.push_back(period);
    }
    return periods;
}

void AggregatedLogger::cleanupOldLogs(int max_days) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::system_clock::now();
    auto cutoff = now - std::chrono::hours(24 * max_days);
    
    for (const auto& entry : std::filesystem::directory_iterator(raw_logs_dir_)) {
        if (entry.is_regular_file()) {
            try {
                // Получаем время модификации
                auto ftime = std::filesystem::last_write_time(entry.path());
                
                // Конвертируем в time_t через системное время
                // Используем более простой подход: проверяем по имени файла
                // Имя файла содержит дату в формате raw_YYYYMMDD_HHMMSS.json
                std::string filename = entry.path().stem().string();
                
                // Извлекаем дату из имени файла
                if (filename.find("raw_") == 0 && filename.length() >= 15) {
                    std::string date_str = filename.substr(4, 8); // YYYYMMDD
                    
                    // Парсим дату
                    int year = std::stoi(date_str.substr(0, 4));
                    int month = std::stoi(date_str.substr(4, 2));
                    int day = std::stoi(date_str.substr(6, 2));
                    
                    std::tm tm = {};
                    tm.tm_year = year - 1900;
                    tm.tm_mon = month - 1;
                    tm.tm_mday = day;
                    
                    std::time_t file_time = std::mktime(&tm);
                    auto file_sys_time = std::chrono::system_clock::from_time_t(file_time);
                    
                    if (file_sys_time < cutoff) {
                        std::filesystem::remove(entry.path());
                        std::cout << "[AggregatedLogger] Removed old log: " << entry.path().string() << std::endl;
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "[AggregatedLogger] Error processing file: " << e.what() << std::endl;
            }
        }
    }
}

void AgentAuditBridge::enableConstraint(const std::string& name, bool enabled) {
    constraints_.setEnabled(name, enabled);
}

bool AgentAuditBridge::isConstraintEnabled(const std::string& name) const {
    auto active = constraints_.getActiveConstraints();
    for (const auto& c : active) {
        if (c.name == name) return true;
    }
    return false;
}

std::string AggregatedLogger::getPeriodKey(const std::chrono::system_clock::time_point& time, const std::string& period) {
    auto time_t = std::chrono::system_clock::to_time_t(time);
    std::tm* tm = std::localtime(&time_t);
    
    std::ostringstream oss;
    if (period == "5h" || period == "12h" || period == "24h") {
        oss << std::put_time(tm, "%Y%m%d");
    } else if (period == "week") {
        oss << std::put_time(tm, "%Y%W");
    } else if (period == "month") {
        oss << std::put_time(tm, "%Y%m");
    } else {
        oss << std::put_time(tm, "%Y");
    }
    return oss.str();
}

AggregatedStats AggregatedLogger::getStatsForPeriod(const std::string& period) const {
    auto it = current_aggregates_.find(period);
    if (it != current_aggregates_.end()) {
        return it->second;
    }
    return AggregatedStats();
}