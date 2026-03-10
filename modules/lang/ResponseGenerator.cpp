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
        }, 0.5f},

        {"tell_me", {
            "I'm still learning about that. What would you like to know?",
            "That's an interesting topic! Can you be more specific?",
            "I'd love to tell you something. What are you interested in?"
        }, 0.6f},
    };
}

std::string ResponseGenerator::detectIntent(const std::string& input) {
    std::string lower = input;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    // Приветствия и вопросы о состоянии
    std::vector<std::string> greetings = {
        "hello", "hi", "hey", "howdy", "greetings",
        "how are you", "how are you doing", "how's it going",
        "what's up", "sup", "good morning", "good afternoon", "good evening"
    };
    
    for (const auto& g : greetings) {
        if (lower.find(g) != std::string::npos) {
            return "greeting";
        }
    }
    
    // Вопросы об имени пользователя
    if (lower.find("my name") != std::string::npos || 
        lower.find("what is my name") != std::string::npos ||
        lower.find("do you know my name") != std::string::npos) {
        return "name_question";
    }
    
    // Вопросы о фактах
    if (lower.find("do you know") != std::string::npos ||
        lower.find("remember") != std::string::npos ||
        lower.find("what is") != std::string::npos ||
        lower.find("who is") != std::string::npos ||
        lower.find("tell me about") != std::string::npos) {
        return "fact_question";
    }
    
    // Вопросы о предпочтениях
    if (lower.find("like") != std::string::npos ||
        lower.find("love") != std::string::npos ||
        lower.find("favorite") != std::string::npos ||
        lower.find("enjoy") != std::string::npos) {
        return "preference_question";
    }
    
    // Просьбы рассказать что-то
    if (lower.find("tell me") != std::string::npos ||
        lower.find("say something") != std::string::npos) {
        return "tell_me";
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

std::string ResponseGenerator::generateResponse(
    const std::string& user_input,
    const UserProfile& profile,
    const ContextTracker& context, 
    const std::vector<MemoryRecord>& similar
) {
    std::string intent = detectIntent(user_input);
    std::cout << "Detected intent: " << intent << std::endl; // ОТЛАДКА
    
    // Проверяем, есть ли факты о пользователе
    auto facts = profile.getAllFacts();
    
    // Если это вопрос об имени и мы знаем имя
    if (intent == "name_question" && facts.count("user_name")) {
        return "Your name is " + facts.at("user_name") + "!";
    }
    
    // Если это приветствие
    if (intent == "greeting") {
        return profile.getPersonalizedGreeting();
    }
    
    // Если есть похожие разговоры в памяти
    if (!similar.empty()) {
        auto it = similar[0].metadata.find("response");
        if (it != similar[0].metadata.end()) {
            return "I remember we talked about this: " + it->second;
        }
    }
    
    // Используем шаблоны
    for (const auto& t : templates_) {
        if (t.intent == intent && !t.patterns.empty()) {
            std::uniform_int_distribution<> dist(0, t.patterns.size() - 1);
            static std::random_device rd;
            static std::mt19937 gen(rd());
            return fillTemplate(t.patterns[dist(gen)], profile, context);
        }
    }
    
    return handleUnknown();
}

std::string ResponseGenerator::getDirectResponse(const std::string& input) {
    std::string lower = input;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    // Приветствия
    if (lower == "hi" || lower == "hello" || lower == "hey") {
        return "Hello! How can I help you today?";
    }
    
    // Вопросы о личности
    if (lower == "who are you" || lower == "what are you") {
        return "I'm Mary, your AI assistant running on a neural field system!";
    }
    
    if (lower.find("your name") != std::string::npos) {
        return "My name is Mary!";
    }
    
    // Вопросы о самочувствии
    if (lower.find("how are you") != std::string::npos ||
        lower.find("how do you do") != std::string::npos ||
        lower == "are you ok" || lower == "are you okay") {
        return "I'm doing well, thanks for asking! How can I help you today?";
    }
    
    // Благодарности
    if (lower == "thanks" || lower == "thank you") {
        return "You're welcome! 😊";
    }
    
    // Прощания
    if (lower == "bye" || lower == "goodbye") {
        return "Goodbye! Talk to you soon!";
    }
    
    return ""; // пустой ответ = использовать обычную генерацию
}