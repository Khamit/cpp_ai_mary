#include "UserProfile.hpp"
#include <iostream>

UserProfile::UserProfile() {}

void UserProfile::loadFromMemory(MemoryManager& memory) {
    facts_.clear();
    
    auto records = memory.getRecords("UserFacts");
    for (const auto& record : records) {
        auto key_it = record.metadata.find("key");
        auto value_it = record.metadata.find("value");
        
        if (key_it != record.metadata.end() && value_it != record.metadata.end()) {
            facts_[key_it->second] = value_it->second;
        }
    }
    
    updateSpecialFields();
    std::cout << "👤 Loaded " << facts_.size() << " facts about user" << std::endl;
}

void UserProfile::updateFromFacts(const std::vector<ExtractedFact>& facts) {
    for (const auto& fact : facts) {
        facts_[fact.key] = fact.value;
    }
    updateSpecialFields();
}

void UserProfile::updateSpecialFields() {
    user_name_ = facts_["user_name"];
    location_ = facts_["user_location"];
    job_ = facts_["user_job"];
    
    preferences_.clear();
    for (const auto& [key, value] : facts_) {
        if (key.find("likes_") == 0 || key.find("favorite_") == 0) {
            preferences_.push_back(value);
        }
    }
}

std::string UserProfile::getPersonalizedGreeting() const {
    if (!user_name_.empty()) {
        return "Hello again, " + user_name_ + "!";
    }
    return "Hello! I'm Mary, your personal assistant.";
}

std::string UserProfile::getFactResponse(const std::string& query) const {
    // Простые шаблоны ответов на основе фактов
    if (query.find("my name") != std::string::npos && !user_name_.empty()) {
        return "Your name is " + user_name_ + ".";
    }
    if (query.find("where do I live") != std::string::npos && !location_.empty()) {
        return "You live in " + location_ + ".";
    }
    if (query.find("what do I like") != std::string::npos && !preferences_.empty()) {
        std::string response = "You like ";
        for (size_t i = 0; i < preferences_.size(); ++i) {
            if (i > 0) response += ", ";
            response += preferences_[i];
        }
        return response + ".";
    }
    return "";
}