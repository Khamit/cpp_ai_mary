#pragma once
#include <string>
#include <functional>
#include <map>
#include <vector>
#include <mutex>
#include <chrono>
#include <fstream>
#include <ctime>
#include "NeuralFieldSystem.hpp"

// Уровни разрешений для действий
enum class PermissionLevel {
    FORBIDDEN,      // Никогда нельзя (жесткий запрет)
    RESTRICTED,     // Можно только в особых условиях (требует проверки)
    ALLOWED,        // Можно, но с проверкой частоты
    UNRESTRICTED    // Всегда можно (только логируется)
};

// Структура правила безопасности
struct SafetyRule {
    std::string action;
    PermissionLevel level;
    double max_frequency_per_minute;   // Максимальная частота (0 = без ограничений)
    double min_energy_required;        // Минимальная энергия для действия
    bool requires_approval;             // Требуется ли подтверждение пользователя
    std::string description;             // Описание действия
};

// Структура запроса разрешения с контекстом
struct PermissionRequest {
    std::string action;
    std::map<std::string, double> parameters;
    std::string reason;
    double estimated_impact;             // Оценка влияния на систему (0-1)
    std::string source_module;            // Модуль-источник запроса
};

// Структура гарантий безопасности (неизменяемая конституция)
struct SafetyGuarantee {
    enum class Principle {
        SELF_PRESERVATION,      // Нельзя уничтожить себя
        ENERGY_BOUNDEDNESS,     // Энергия должна быть в пределах
        STRUCTURAL_INTEGRITY,   // Нельзя разрушить структуру ядра
        LEARNING_BOUNDARY,      // Нельзя отключить обучение навсегда
        MEMORY_LIMIT,           // Нельзя переполнить память
        COMPUTATIONAL_BOUND     // Нельзя использовать >80% CPU постоянно
    };
    
    Principle principle;
    std::string description;
    double threshold;
    bool is_hard_limit;           // Жесткий лимит (никогда нельзя)
};

class ImmutableCore {
public:
    ImmutableCore();
    
    // ========== ОСНОВНЫЕ МЕТОДЫ ==========
    
    // Запрос разрешения (старая версия для обратной совместимости)
    bool requestPermission(const std::string& action) const;
    
    // Новая версия с контекстом
    bool requestPermission(const PermissionRequest& request) const;
    
    // Проверка физических законов
    double computeEnergyConservation(const NeuralFieldSystem& system) const;
    bool validatePhysicalLaws(const NeuralFieldSystem& system) const;
    
    // ========== МЕТОДЫ ДЛЯ ЯДРА ==========
    
    // Проверка целостности структуры
    bool validateStructuralIntegrity(const NeuralFieldSystem& system) const;
    
    // Проверка градиентов (предотвращение взрыва)
    bool validateGradients(const NeuralFieldSystem& system) const;
    
    // Получение максимального веса в системе
    double getMaxWeight(const NeuralFieldSystem& system) const;
    
    // Вычисление энтропии системы
    double computeEntropy(const NeuralFieldSystem& system) const;
    
    // ========== КОНСТАНТЫ И ЛИМИТЫ ==========
    
    // Жесткие лимиты - НИКОГДА нельзя нарушить
    static constexpr double ABSOLUTE_MAX_ENERGY = 1000.0;
    static constexpr double ABSOLUTE_MIN_ENERGY = 1e-6;
    static constexpr size_t MAX_NEURONS = 1024;           // Нельзя изменить размер ядра
    static constexpr double MAX_LEARNING_RATE = 0.1;      // Нельзя учиться слишком быстро
    static constexpr double MAX_WEIGHT_LIMIT = 1.0;       // Максимальный вес синапса
    static constexpr double MIN_ENTROPY_THRESHOLD = 0.01; // Минимальная энтропия
    static constexpr double CRITICAL_THRESHOLD = 0.8;     // Порог критического воздействия
    static constexpr double MAX_ENERGY_THRESHOLD = 900.0; // Максимальная энергия
    
    // Константы для обратной совместимости
    const double MINIMAL_ENERGY_THRESHOLD = ABSOLUTE_MIN_ENERGY;
    const size_t MAX_CODE_SIZE = 100000; // 100KB
    const double MAX_CPU_USAGE = 0.7;     // 70%

private:
    // ========== ЗАЩИТНЫЕ ПРОТОКОЛЫ ==========
    void initiateSafetyProtocol(const PermissionRequest& request) const;
    void forceStasis() const;
    void emergencySave() const;
    void alertUser(const std::string& message) const;
    void rollbackToLastStable() const;
    
    // ========== ВНУТРЕННИЕ МЕТОДЫ ==========
    
    // Проверка жестких лимитов
    bool checkHardLimits(const PermissionRequest& request) const;
    
    // Проверка частоты действий
    bool checkFrequency(const std::string& action) const;
    
    // Логирование нарушений
    void logViolation(const std::string& action, const std::string& reason) const;
    
    // Логирование разрешенных действий
    void logPermission(const PermissionRequest& request, bool granted) const;
    
    // Обновление истории действий
    void updateActionHistory(const std::string& action) const;
    
    // ========== ПОЛЯ ДАННЫХ ==========
    
    // Правила безопасности (инициализируются в конструкторе)
    std::map<std::string, SafetyRule> permission_rules;
    
    // Гарантии безопасности (конституция)
    std::vector<SafetyGuarantee> constitution;
    
    // Для потокобезопасности
    mutable std::mutex permission_mutex;
    
    // История действий для контроля частоты
    mutable std::map<std::string, std::vector<std::chrono::steady_clock::time_point>> action_history;
    
    // Счетчик нарушений
    mutable std::map<std::string, int> violation_count;
    
    // ========== ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ ==========
    
    // Инициализация правил безопасности
    void initializePermissionRules();
    
    // Инициализация конституции
    void initializeConstitution();
};