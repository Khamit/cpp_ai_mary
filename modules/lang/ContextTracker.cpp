#include "ContextTracker.hpp"
#include <sstream>
#include <chrono>

ContextTracker::ContextTracker(size_t max_history) : max_history_(max_history) {}

void ContextTracker::addTurn(const std::string& speaker, const std::string& text) {
    ConversationTurn turn;
    turn.speaker = speaker;
    turn.text = text;
    turn.topics = extractTopics(text);
    turn.timestamp = std::chrono::duration<float>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
    
    history_.push_back(turn);
    if (history_.size() > max_history_) {
        history_.pop_front();
    }
}

std::vector<std::string> ContextTracker::extractTopics(const std::string& text) {
    // Простое выделение ключевых слов (можно улучшить)
    std::vector<std::string> topics;
    std::istringstream iss(text);
    std::string word;
    
    std::vector<std::string> stop_words = {"the", "a", "an", "is", "are", "was", 
                                           "to", "and", "or", "but", "in", "on"};
    
    while (iss >> word) {
        // Приводим к нижнему регистру
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        
        // Удаляем пунктуацию
        word.erase(std::remove_if(word.begin(), word.end(), ::ispunct), word.end());
        
        // Пропускаем короткие слова и стоп-слова
        if (word.length() > 3 && 
            std::find(stop_words.begin(), stop_words.end(), word) == stop_words.end()) {
            topics.push_back(word);
        }
    }
    
    return topics;
}

std::string ContextTracker::getCurrentTopic() const {
    if (history_.empty()) return "";
    
    // Берем последние 3 слова из последнего сообщения пользователя
    const auto& last_turn = history_.back();
    if (last_turn.speaker != "user") return "";
    
    if (!last_turn.topics.empty()) {
        return last_turn.topics.back();
    }
    return "";
}

std::vector<std::string> ContextTracker::getRecentTopics() const {
    std::vector<std::string> all_topics;
    for (const auto& turn : history_) {
        all_topics.insert(all_topics.end(), turn.topics.begin(), turn.topics.end());
    }
    return all_topics;
}

std::string ContextTracker::getConversationSummary() const {
    if (history_.empty()) return "No conversation yet.";
    
    std::string summary;
    for (const auto& turn : history_) {
        summary += turn.speaker + ": " + turn.text + "\n";
        if (summary.length() > 200) {
            summary += "...";
            break;
        }
    }
    return summary;
}

bool ContextTracker::wasRecentlyMentioned(const std::string& term) const {
    std::string lower_term = term;
    std::transform(lower_term.begin(), lower_term.end(), lower_term.begin(), ::tolower);
    
    for (const auto& turn : history_) {
        if (turn.text.find(lower_term) != std::string::npos) {
            return true;
        }
    }
    return false;
}