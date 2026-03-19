// DynamicParams.hpp
#pragma once
#include <chrono>
#include <string>

struct OperatingMode {
    enum Type {
        TRAINING,        // режим обучения
        NORMAL,          // обычная работа
        IDLE,           // простой
        SLEEP           // сон
    };
    
    // ДОБАВЛЯЕМ МЕТОД toString
    static const char* toString(Type mode) {
        switch(mode) {
            case TRAINING: return "TRAINING";
            case NORMAL: return "NORMAL";
            case IDLE: return "IDLE";
            case SLEEP: return "SLEEP";
            default: return "UNKNOWN";
        }
    }
};

struct DynamicParams {
    // Таймауты для разных режимов
    // Таймауты для разных режимов (в секундах)
    static constexpr double TRAINING_COLLAPSE_TIME = 30.0 * 60.0;      // 30 минут
    static constexpr double NORMAL_COLLAPSE_TIME = 24.0 * 60.0 * 60.0; // 24 часа
    static constexpr double IDLE_COLLAPSE_TIME = 72.0 * 60.0 * 60.0;   // 72 часа
    
    // Параметры выживаемости
    struct SurvivalParams {
        double min_energy_boost = 0.1;      // минимальная подпитка
        double idle_activity = 0.3;          // активность в простое
        double noise_level = 0.05;            // фоновый шум
        bool auto_revive = true;               // авто-оживление
    };
    
    SurvivalParams training = {0.1, 0.5, 0.05, true};
    SurvivalParams normal = {0.3, 0.4, 0.02, true};    // более стабильно
    SurvivalParams idle = {0.2, 0.3, 0.01, true};
    SurvivalParams sleep = {0.05, 0.2, 0.001, false};
    
    // Метод для получения параметров по режиму
    static const SurvivalParams& getSurvivalParams(OperatingMode::Type mode) {
        static DynamicParams params;
        switch(mode) {
            case OperatingMode::TRAINING: return params.training;
            case OperatingMode::NORMAL: return params.normal;
            case OperatingMode::IDLE: return params.idle;
            case OperatingMode::SLEEP: return params.sleep;
            default: return params.normal;
        }
    }
};