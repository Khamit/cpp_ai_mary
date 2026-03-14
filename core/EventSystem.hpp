#pragma once
#include <functional>
#include <map>
#include <vector>
#include <string>

enum class AnomalyLevel {
    INFO,      // легкое отклонение
    WARNING,   // заметная аномалия
    CRITICAL   // критическое расхождение
};

enum class EventType {
    ANOMALY_DETECTED,
    PREDICTION_HIGH_ERROR,
    STATE_CHANGED,
    MEMORY_CONSOLIDATION_NEEDED
};

struct Event {
    EventType type;
    std::string source;
    std::vector<float> data;
    double value;
    int step;
};

class EventSystem {
private:
    std::multimap<EventType, std::function<void(const Event&)>> listeners;
    
public:
    AnomalyLevel level;
    void subscribe(EventType type, std::function<void(const Event&)> callback) {
        listeners.insert({type, callback});
    }
    
    void emit(const Event& event) {
        auto range = listeners.equal_range(event.type);
        for (auto it = range.first; it != range.second; ++it) {
            it->second(event);
        }
    }
};