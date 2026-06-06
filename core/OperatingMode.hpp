#pragma once

struct OperatingMode {
    enum Type {
        TRAINING,   // режим обучения — высокая пластичность
        NORMAL,     // обычная работа — сбалансированная
        IDLE,       // простой — низкая активность, экономия
        SLEEP       // сон — консолидация памяти
    };
    
    static const char* toString(Type mode) {
        switch(mode) {
            case TRAINING: return "TRAINING";
            case NORMAL:   return "NORMAL";
            case IDLE:     return "IDLE";
            case SLEEP:    return "SLEEP";
            default:       return "UNKNOWN";
        }
    }
    
    // Параметры режимов для новой архитектуры
    struct ModeParams {
        float stdp_rate;           // скорость STDP
        float consolidation_rate;  // скорость консолидации
        float exploration_bias;    // склонность к исследованию
        float trophic_decay;       // затухание трофинов
        float apoptosis_threshold; // порог апоптоза
    };
    
    static ModeParams getParams(Type mode) {
        switch(mode) {
            case TRAINING:
                return {0.8f, 0.01f, 0.7f, 0.99f, 0.03f};
            case NORMAL:
                return {0.5f, 0.05f, 0.3f, 0.99f, 0.05f};
            case IDLE:
                return {0.2f, 0.10f, 0.1f, 0.999f, 0.10f};
            case SLEEP:
                return {0.1f, 0.20f, 0.0f, 0.999f, 0.15f};
            default:
                return {0.5f, 0.05f, 0.3f, 0.99f, 0.05f};
        }
    }
};