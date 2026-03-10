#pragma once
#include <string>
#include <map>
#include <vector>
#include "../../core/MemoryManager.hpp"
#include "FactExtractor.hpp"

class UserProfile {
public:
    UserProfile();
    
    // Загрузка профиля из памяти
    void loadFromMemory(MemoryManager& memory);
    
    // Обновление профиля на основе извлеченных фактов
    void updateFromFacts(const std::vector<ExtractedFact>& facts);
    
    // Получение информации для ответа
    std::string getPersonalizedGreeting() const;
    std::string getFactResponse(const std::string& query) const;
    
    // Проверка, знаем ли мы что-то о пользователе
    bool knowsUser() const { return !facts_.empty(); }
    
    // Получение всех фактов
    const std::map<std::string, std::string>& getAllFacts() const { return facts_; }

private:
    std::map<std::string, std::string> facts_;  // ключ -> значение
    
    // Специальные поля
    std::string user_name_;
    std::vector<std::string> preferences_;
    std::string location_;
    std::string job_;
    
    void updateSpecialFields();
};