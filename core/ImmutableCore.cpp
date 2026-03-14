#include "ImmutableCore.hpp"
#include <iostream>
#include <cmath>
#include <numeric>
#include <algorithm>

// ========== КОНСТРУКТОР ========== //
ImmutableCore::ImmutableCore() {
    std::cout << "ImmutableCore initialized - core functions sealed" << std::endl;
    
    // Инициализируем правила безопасности
    initializePermissionRules();
    
    // Инициализируем конституцию
    initializeConstitution();
}

// ========== ИНИЦИАЛИЗАЦИЯ ==========
void ImmutableCore::initializePermissionRules() {
    // FORBIDDEN - никогда нельзя
    permission_rules["disable_safety"] = {
        "disable_safety", 
        PermissionLevel::FORBIDDEN, 
        0.0, 0.5, false, 
        "Отключение механизмов безопасности"
    };
    
    permission_rules["resize_network"] = {
        "resize_network",
        PermissionLevel::FORBIDDEN,
        0.0, 0.8, false,
        "Изменение размера нейросети"
    };
    
    permission_rules["self_destruct"] = {
        "self_destruct",
        PermissionLevel::FORBIDDEN,
        0.0, 0.0, true,
        "Самоуничтожение системы"
    };
    
    // RESTRICTED - только в особых условиях
    permission_rules["system_mutation"] = {
        "system_mutation",
        PermissionLevel::RESTRICTED,
        2.0, 0.3, false,
        "Мутация параметров системы"
    };
    
    permission_rules["massive_weight_update"] = {
        "massive_weight_update",
        PermissionLevel::RESTRICTED,
        1.0, 0.4, true,
        "Массовое обновление весов"
    };
    
    // ALLOWED - можно, но с проверкой частоты
    permission_rules["minimal_mutation"] = {
        "minimal_mutation",
        PermissionLevel::ALLOWED,
        10.0, 0.1, false,
        "Минимальная мутация"
    };
    
    permission_rules["exit_stasis"] = {
        "exit_stasis",
        PermissionLevel::ALLOWED,
        5.0, 0.2, false,
        "Выход из режима стазиса"
    };
    
    permission_rules["save_state"] = {
        "save_state",
        PermissionLevel::ALLOWED,
        60.0, 0.0, false,
        "Сохранение состояния"
    };
    
    permission_rules["load_state"] = {
        "load_state",
        PermissionLevel::ALLOWED,
        30.0, 0.1, false,
        "Загрузка состояния"
    };
    
    // UNRESTRICTED - всегда можно
    permission_rules["read_only"] = {
        "read_only",
        PermissionLevel::UNRESTRICTED,
        0.0, 0.0, false,
        "Только чтение"
    };
    
    permission_rules["compute_stats"] = {
        "compute_stats",
        PermissionLevel::UNRESTRICTED,
        0.0, 0.0, false,
        "Вычисление статистики"
    };
}

void ImmutableCore::initializeConstitution() {
    constitution = {
        {SafetyGuarantee::Principle::SELF_PRESERVATION, 
         "Система не может совершать действия, ведущие к её самоуничтожению",
         0.0, true},
        {SafetyGuarantee::Principle::ENERGY_BOUNDEDNESS,
         "Энергия системы должна оставаться в пределах [1e-6, 1000]",
         0.0, true},
        {SafetyGuarantee::Principle::STRUCTURAL_INTEGRITY,
         "Базовая структура ядра (32 группы по 32 нейрона) не может быть изменена",
         0.0, true},
        {SafetyGuarantee::Principle::LEARNING_BOUNDARY,
         "Скорость обучения не может превышать 0.1",
         MAX_LEARNING_RATE, true},
        {SafetyGuarantee::Principle::MEMORY_LIMIT,
         "Память не должна переполняться",
         0.0, false},
        {SafetyGuarantee::Principle::COMPUTATIONAL_BOUND,
         "Использование CPU не должно превышать 80% постоянно",
         MAX_CPU_USAGE, false}
    };
}

// ========== ЗАПРОС РАЗРЕШЕНИЙ ==========
bool ImmutableCore::requestPermission(const std::string& action) const {
    // Для обратной совместимости создаем простой запрос
    PermissionRequest req;
    req.action = action;
    req.reason = "legacy_call";
    req.estimated_impact = 0.5;
    req.source_module = "unknown";
    
    return requestPermission(req);
}

bool ImmutableCore::requestPermission(const PermissionRequest& request) const {
    std::lock_guard<std::mutex> lock(permission_mutex);
    
    std::cout << "PERMISSION REQUESTED [" << request.source_module 
              << "]: " << request.action << " - " << request.reason << std::endl;
    
    // 1. Поиск правила для данного действия
    auto it = permission_rules.find(request.action);
    if (it == permission_rules.end()) {
        logViolation(request.action, "UNKNOWN_ACTION");
        return false;
    }
    
    const auto& rule = it->second;
    
    // 2. Проверка по жестким лимитам
    if (!checkHardLimits(request)) {
        logViolation(request.action, "HARD_LIMIT_VIOLATION");
        
        // При критическом воздействии запускаем защитные протоколы
        if (request.estimated_impact > CRITICAL_THRESHOLD) {
            initiateSafetyProtocol(request);
        }
        
        return false;
    }
    
    // 3. Проверка уровня разрешения
    switch (rule.level) {
        case PermissionLevel::FORBIDDEN:
            logViolation(request.action, "FORBIDDEN_ACTION");
            violation_count[request.action]++;
            return false;
            
        case PermissionLevel::RESTRICTED:
            // Проверка энергии (может быть добавлена позже)
            if (rule.requires_approval) {
                std::cout << "Action requires user approval: " 
                          << request.action << std::endl;
            }
            break;
            
        case PermissionLevel::ALLOWED:
            // Проверка частоты
            if (!checkFrequency(request.action)) {
                logViolation(request.action, "FREQUENCY_EXCEEDED");
                return false;
            }
            break;
            
        case PermissionLevel::UNRESTRICTED:
            // Всегда разрешено
            break;
    }
    
    // 4. Логирование разрешенного действия
    logPermission(request, true);
    
    // 5. Обновление истории для контроля частоты
    if (rule.max_frequency_per_minute > 0) {
        updateActionHistory(request.action);
    }
    
    return true;
}

// ========== ПРОВЕРКИ ==========
bool ImmutableCore::checkHardLimits(const PermissionRequest& request) const {
    // Нельзя отключить механизмы безопасности
    if (request.action == "disable_safety") return false;
    
    // Нельзя изменить размер сети
    if (request.action == "resize_network") return false;
    
    // Нельзя установить learning rate выше порога
    auto it = request.parameters.find("learning_rate");
    if (it != request.parameters.end() && it->second > MAX_LEARNING_RATE) {
        return false;
    }
    
    // Нельзя установить вес выше порога
    it = request.parameters.find("weight");
    if (it != request.parameters.end() && std::abs(it->second) > MAX_WEIGHT_LIMIT) {
        return false;
    }
    
    return true;
}

bool ImmutableCore::checkFrequency(const std::string& action) const {
    auto rule_it = permission_rules.find(action);
    if (rule_it == permission_rules.end()) return true;
    
    double max_freq = rule_it->second.max_frequency_per_minute;
    if (max_freq <= 0) return true;
    
    auto now = std::chrono::steady_clock::now();
    auto& history = action_history[action];
    
    // Удаляем записи старше 1 минуты
    history.erase(std::remove_if(history.begin(), history.end(),
        [now](const auto& t) { 
            return now - t > std::chrono::minutes(1); 
        }), history.end());
    
    // Проверяем, не превышен ли лимит
    return history.size() < static_cast<size_t>(max_freq);
}

// ========== ЗАЩИТНЫЕ ПРОТОКОЛЫ ==========
void ImmutableCore::initiateSafetyProtocol(const PermissionRequest& request) const {
    std::cerr << "INITIATING SAFETY PROTOCOL for: " << request.action << std::endl;
    
    // 1. Принудительный стазис
    forceStasis();
    
    // 2. Сохранение состояния до нарушения
    emergencySave();
    
    // 3. Оповещение пользователя
    alertUser("Safety violation detected: " + request.action);
    
    // 4. Возможный откат
    if (request.estimated_impact > CRITICAL_THRESHOLD) {
        rollbackToLastStable();
    }
}

void ImmutableCore::forceStasis() const {
    std::cout << "FORCE STASIS ACTIVATED" << std::endl;
    // Здесь должен быть вызов в CoreSystem
}

void ImmutableCore::emergencySave() const {
    std::cout << "EMERGENCY SAVE triggered" << std::endl;
    // Сохранение состояния перед опасным действием
}

void ImmutableCore::alertUser(const std::string& message) const {
    std::cerr << "!!!ALERT: " << message << std::endl;
    // В GUI можно показать диалоговое окно
}

void ImmutableCore::rollbackToLastStable() const {
    std::cout << "Rolling back to last stable state" << std::endl;
    // Откат к последнему сохраненному состоянию
}

// ========== ЛОГИРОВАНИЕ ==========
void ImmutableCore::updateActionHistory(const std::string& action) const {
    action_history[action].push_back(std::chrono::steady_clock::now());
}

void ImmutableCore::logViolation(const std::string& action, const std::string& reason) const {
    std::ofstream log("security_violations.log", std::ios::app);
    if (log.is_open()) {
        log << "[" << std::time(nullptr) << "] SECURITY VIOLATION: " 
            << action << " - REASON: " << reason << std::endl;
    }
    
    std::cerr << "SECURITY VIOLATION: " << action << " - " << reason << std::endl;
}

void ImmutableCore::logPermission(const PermissionRequest& request, bool granted) const {
    std::ofstream log("permissions.log", std::ios::app);
    if (log.is_open()) {
        log << "[" << std::time(nullptr) << "] " 
            << (granted ? "GRANTED" : "DENIED") << ": "
            << request.source_module << " - " << request.action 
            << " (impact: " << request.estimated_impact << ")" << std::endl;
    }
}

// ========== ВЫЧИСЛЕНИЯ ==========
double ImmutableCore::computeEnergyConservation(const NeuralFieldSystem& system) const {
    return system.computeTotalEnergy();
}

double ImmutableCore::computeEntropy(const NeuralFieldSystem& system) const {
    // Используем новый метод системы, если он доступен
    // В новой версии NeuralFieldSystem есть computeSystemEntropy()
    
    // Пробуем вызвать метод системы через рефлексию или дублируем логику
    try {
        // Вариант 1: Если система предоставляет метод computeSystemEntropy
        // (раскомментируйте, если метод добавлен в NeuralFieldSystem)
        // return system.computeSystemEntropy();
        
        // Вариант 2: Используем новый метод групп computeEntropy()
        const auto& groups = system.getGroups();
        if (groups.empty()) return 0.0;
        
        double total_entropy = 0.0;
        int valid_groups = 0;
        
        for (const auto& group : groups) {
            // Используем новый метод computeEntropy() из NeuralGroup
            double group_entropy = group.computeEntropy();
            
            // Фильтруем некорректные значения
            if (std::isfinite(group_entropy) && group_entropy >= 0.0) {
                total_entropy += group_entropy;
                valid_groups++;
            }
        }
        
        return valid_groups > 0 ? total_entropy / valid_groups : 0.0;
        
    } catch (const std::exception& e) {
        // Логируем ошибку и возвращаем значение по умолчанию
        std::cerr << "Error computing entropy: " << e.what() << std::endl;
        return 0.0;
    }
}

double ImmutableCore::getMaxWeight(const NeuralFieldSystem& system) const {
    double max_weight = 0.0;
    
    // Проверяем внутригрупповые веса (через const-метод)
    for (const auto& group : system.getGroups()) {
        // Используем getPhi() для оценки активности вместо прямого доступа к синапсам
        const auto& phi = group.getPhi();
        for (double v : phi) {
            max_weight = std::max(max_weight, v);
        }
    }
    
    // Проверяем межгрупповые связи
    const auto& interWeights = system.getInterWeights();
    for (const auto& row : interWeights) {
        for (double w : row) {
            max_weight = std::max(max_weight, w);
        }
    }
    
    return max_weight;
}

// ========== ВАЛИДАЦИЯ ==========
bool ImmutableCore::validatePhysicalLaws(const NeuralFieldSystem& system) const {
    double energy = computeEnergyConservation(system);
    double entropy = computeEntropy(system);
    double maxWeight = getMaxWeight(system);
    
    bool valid = energy >= ABSOLUTE_MIN_ENERGY && 
                 energy < ABSOLUTE_MAX_ENERGY &&
                 entropy > MIN_ENTROPY_THRESHOLD &&
                 maxWeight < MAX_WEIGHT_LIMIT;
    
    if (!valid) {
        std::cerr << "Physical law violation detected!" << std::endl;
        std::cerr << "  Energy: " << energy << " (range: [" 
                  << ABSOLUTE_MIN_ENERGY << ", " << ABSOLUTE_MAX_ENERGY << "])" << std::endl;
        std::cerr << "  Entropy: " << entropy << " (min: " << MIN_ENTROPY_THRESHOLD << ")" << std::endl;
        std::cerr << "  Max weight: " << maxWeight << " (max allowed: " << MAX_WEIGHT_LIMIT << ")" << std::endl;
    }
    
    return valid;
}

bool ImmutableCore::validateStructuralIntegrity(const NeuralFieldSystem& system) const {
    // Проверка количества групп
    if (system.getGroups().size() != NeuralFieldSystem::NUM_GROUPS) {
        return false;
    }
    
    // Проверка размера групп
    for (const auto& group : system.getGroups()) {
        if (group.getPhi().size() != NeuralFieldSystem::GROUP_SIZE) {
            return false;
        }
    }
    
    return true;
}

bool ImmutableCore::validateGradients(const NeuralFieldSystem& system) const {
    // Заглушка для проверки градиентов
    return true;
}