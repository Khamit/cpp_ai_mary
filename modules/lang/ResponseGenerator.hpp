#pragma once
#include <string>
#include <vector>
#include <map>
#include "UserProfile.hpp"
#include "ContextTracker.hpp"
#include "../../core/MemoryManager.hpp"

class ResponseGenerator {
public:
    ResponseGenerator();
    
    std::string generateResponse(const std::string& user_input,
                             const UserProfile& profile,
                             const ContextTracker& context,
                             const std::vector<MemoryRecord>& similar);

    std::string getDirectResponse(const std::string& input);
    
private:
    // Шаблоны ответов
    struct ResponseTemplate {
        std::string intent;          // тип намерения
        std::vector<std::string> patterns;  // шаблоны для генерации
        float base_confidence;
    };
    
    std::vector<ResponseTemplate> templates_;
    
    std::string detectIntent(const std::string& input);
    std::string fillTemplate(const std::string& template_str,
                             const UserProfile& profile,
                             const ContextTracker& context);
    
    // Примитивные ответы для частых случаев
    std::string handleGreeting(const UserProfile& profile);
    std::string handleQuestion(const std::string& input, const UserProfile& profile);
    std::string handleUnknown();
};