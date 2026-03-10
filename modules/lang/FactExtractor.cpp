#include "FactExtractor.hpp"
#include <iostream>

FactExtractor::FactExtractor() {
    initializePatterns();
}

void FactExtractor::initializePatterns() {
    // Шаблоны для извлечения фактов (можно расширять)
    patterns_ = {
        // Имя: "my name is X", "I'm X", "call me X"
        {std::regex("(?:my name is|I am|I'm|call me) ([A-Za-z]+)", std::regex::icase),
         "identity", "user_name", 1, 0.9f},
        
        // Возраст: "I am X years old", "age X"
        {std::regex("(?:I am|I'm) ([0-9]+) (?:years old|y/o)", std::regex::icase),
         "demographics", "user_age", 1, 0.8f},
        
        // Предпочтения: "I like X", "I love X", "my favorite X is Y"
        {std::regex("I (?:like|love) ([A-Za-z]+)", std::regex::icase),
         "preference", "likes_{value}", 1, 0.7f},
        
        {std::regex("my favorite ([A-Za-z]+) is ([A-Za-z]+)", std::regex::icase),
         "preference", "favorite_{1}", 2, 0.85f},
        
        // Местоположение: "I live in X", "from X"
        {std::regex("I live in ([A-Za-z ]+)", std::regex::icase),
         "location", "user_location", 1, 0.8f},
        
        {std::regex("I am from ([A-Za-z ]+)", std::regex::icase),
         "location", "user_origin", 1, 0.8f},
        
        // Работа: "I work as X", "I am a X"
        {std::regex("I work as (?:a|an) ([A-Za-z ]+)", std::regex::icase),
         "profession", "user_job", 1, 0.75f},
        
        {std::regex("I am (?:a|an) ([A-Za-z ]+)", std::regex::icase),
         "profession", "user_role", 1, 0.6f},
        
        // Хобби: "I enjoy X", "my hobby is X"
        {std::regex("I enjoy ([A-Za-z ]+)", std::regex::icase),
         "interest", "hobby", 1, 0.7f},
        
        {std::regex("my hobby is ([A-Za-z ]+)", std::regex::icase),
         "interest", "hobby", 1, 0.8f}
    };
}

std::vector<ExtractedFact> FactExtractor::extractFacts(const std::string& user_message) {
    std::vector<ExtractedFact> facts;
    
    for (const auto& pattern : patterns_) {
        std::smatch matches;
        if (std::regex_search(user_message, matches, pattern.pattern)) {
            ExtractedFact fact;
            fact.fact_type = pattern.fact_type;
            fact.value = matches[pattern.value_group].str();
            fact.confidence = pattern.base_confidence;
            fact.source_text = user_message;
            
            // Формируем ключ
            fact.key = pattern.key_template;
            if (fact.key.find("{value}") != std::string::npos) {
                fact.key = std::regex_replace(fact.key, std::regex("\\{value\\}"), fact.value);
            }
            if (fact.key.find("{1}") != std::string::npos && matches.size() > 1) {
                fact.key = std::regex_replace(fact.key, std::regex("\\{1\\}"), matches[1].str());
            }
            
            facts.push_back(fact);
            
            std::cout << "🔍 Extracted fact: " << fact.fact_type 
                      << " [" << fact.key << " = " << fact.value 
                      << "] confidence: " << fact.confidence << std::endl;
        }
    }
    
    return facts;
}

void FactExtractor::storeFact(MemoryManager& memory, const ExtractedFact& fact) {
    // Кодируем факт в вектор для хранения
    std::vector<float> fact_data;
    fact_data.push_back(fact.confidence);
    
    // Добавляем символьное представление (упрощенно)
    for (char c : fact.key + "=" + fact.value) {
        fact_data.push_back(static_cast<float>(c) / 255.0f);
    }
    
    // Метаданные
    std::map<std::string, std::string> metadata;
    metadata["fact_type"] = fact.fact_type;
    metadata["key"] = fact.key;
    metadata["value"] = fact.value;
    metadata["source"] = fact.source_text;
    
    // Сохраняем с высокой важностью (0.9)
    memory.store("UserFacts", fact.key, fact_data, 0.9f, metadata);
}

std::map<std::string, std::string> FactExtractor::getUserFacts(MemoryManager& memory) {
    std::map<std::string, std::string> facts;
    
    auto records = memory.getRecords("UserFacts");
    for (const auto& record : records) {
        auto key_it = record.metadata.find("key");
        auto value_it = record.metadata.find("value");
        
        if (key_it != record.metadata.end() && value_it != record.metadata.end()) {
            facts[key_it->second] = value_it->second;
        }
    }
    
    return facts;
}