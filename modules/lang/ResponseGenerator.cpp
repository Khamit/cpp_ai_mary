#include "ResponseGenerator.hpp"
#include <random>
#include <iostream>

ResponseGenerator::ResponseGenerator() {
    templates_ = {
        {"greeting", {
            "Hello {name}!",
            "Hi {name}! How can I help you today?",
            "Hey {name}! Good to see you again."
        }, 1.0f},
        
        {"name_question", {
            "Your name is {name}.",
            "You told me your name is {name}."
        }, 0.9f},
        
        {"fact_question", {
            "I know that {fact}.",
            "Based on our conversation, {fact}."
        }, 0.8f},
        
        {"preference_question", {
            "You mentioned you like {preference}.",
            "I remember you enjoy {preference}."
        }, 0.8f},
        
        {"unknown", {
            "I'm not sure I understand. Could you rephrase that?",
            "I'm still learning. Can you tell me more?",
            "Interesting! Tell me more about that."
        }, 0.5f}
    };
}

std::string ResponseGenerator::detectIntent(const std::string& input) {
    std::string lower = input;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower.find("hello") != std::string::npos || 
        lower.find("hi") != std::string::npos ||
        lower.find("hey") != std::string::npos) {
        return "greeting";
    }
    
    if (lower.find("my name") != std::string::npos ||
        lower.find("what is my name") != std::string::npos) {
        return "name_question";
    }
    
    if (lower.find("do you know") != std::string::npos ||
        lower.find("remember") != std::string::npos) {
        return "fact_question";
    }
    
    if (lower.find("like") != std::string::npos ||
        lower.find("favorite") != std::string::npos) {
        return "preference_question";
    }
    
    return "unknown";
}

std::string ResponseGenerator::fillTemplate(const std::string& template_str,
                                           const UserProfile& profile,
                                           const ContextTracker& context) {
    std::string result = template_str;
    
    // Заменяем {name}
    auto facts = profile.getAllFacts();
    if (result.find("{name}") != std::string::npos) {
        auto it = facts.find("user_name");
        std::string name = (it != facts.end()) ? it->second : "there";
        result = std::regex_replace(result, std::regex("\\{name\\}"), name);
    }
    
    // Заменяем {fact}
    if (result.find("{fact}") != std::string::npos && !facts.empty()) {
        // Берем первый факт
        auto first = facts.begin();
        std::string fact = "you are " + first->second;
        result = std::regex_replace(result, std::regex("\\{fact\\}"), fact);
    }
    
    // Заменяем {preference}
    if (result.find("{preference}") != std::string::npos) {
        std::string pref = "something";
        for (const auto& [key, value] : facts) {
            if (key.find("likes_") == 0 || key.find("favorite_") == 0) {
                pref = value;
                break;
            }
        }
        result = std::regex_replace(result, std::regex("\\{preference\\}"), pref);
    }
    
    return result;
}

std::string ResponseGenerator::handleGreeting(const UserProfile& profile) {
    return profile.getPersonalizedGreeting();
}

std::string ResponseGenerator::handleQuestion(const std::string& input, 
                                             const UserProfile& profile) {
    // Пробуем ответить на основе фактов
    std::string fact_response = profile.getFactResponse(input);
    if (!fact_response.empty()) {
        return fact_response;
    }
    
    // Иначе generic ответ
    return "I don't know that yet. Can you tell me more?";
}

std::string ResponseGenerator::handleUnknown() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dist(0, 2);
    
    std::vector<std::string> unknown_responses = {
        "I'm not sure I understand. Could you rephrase that?",
        "I'm still learning. Can you tell me more?",
        "Interesting! Tell me more about that."
    };
    
    return unknown_responses[dist(gen)];
}

std::string ResponseGenerator::generateResponse(const std::string& user_input,
                                              const UserProfile& profile,
                                              const ContextTracker& context) {
    std::string intent = detectIntent(user_input);
    
    for (const auto& t : templates_) {
        if (t.intent == intent && !t.patterns.empty()) {
            std::uniform_int_distribution<> dist(0, t.patterns.size() - 1);
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::string response = fillTemplate(t.patterns[dist(gen)], profile, context);
            return response;
        }
    }
    
    return handleUnknown();
}