#pragma once
#include <string>
#include <vector>
#include <regex>
#include <map>
#include "../../core/MemoryManager.hpp"

struct ExtractedFact {
    std::string fact_type;      // "name", "preference", "location", etc.
    std::string key;            // например "user_name", "favorite_food"
    std::string value;          // значение факта
    float confidence;           // уверенность в факте (0-1)
    std::string source_text;    // исходный текст, откуда извлечен
};

class FactExtractor {
public:
    FactExtractor();
    
    // Извлечение фактов из сообщения пользователя
    std::vector<ExtractedFact> extractFacts(const std::string& user_message);
    
    // Сохранение факта в MemoryManager
    void storeFact(MemoryManager& memory, const ExtractedFact& fact);
    
    // Получение всех фактов о пользователе
    std::map<std::string, std::string> getUserFacts(MemoryManager& memory);
    
private:
    // Шаблоны для извлечения фактов
    struct FactPattern {
        std::regex pattern;
        std::string fact_type;
        std::string key_template;
        int value_group;
        float base_confidence;
    };
    
    std::vector<FactPattern> patterns_;
    
    void initializePatterns();
};