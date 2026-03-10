#pragma once
#include <string>
#include <deque>
#include <vector>

struct ConversationTurn {
    std::string speaker;
    std::string text;
    std::vector<std::string> topics;
    float timestamp;
};

class ContextTracker {
public:
    ContextTracker(size_t max_history = 10);
    
    void addTurn(const std::string& speaker, const std::string& text);
    std::string getCurrentTopic() const;
    std::vector<std::string> getRecentTopics() const;
    std::string getConversationSummary() const;
    
    // Поиск по истории
    bool wasRecentlyMentioned(const std::string& term) const;
    
private:
    size_t max_history_;
    std::deque<ConversationTurn> history_;
    
    std::vector<std::string> extractTopics(const std::string& text);
};