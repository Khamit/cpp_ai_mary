// core/IAuthorization.hpp
#pragma once
#include <string>
#include "AccessLevel.hpp"

class IAuthorization {
public:
    virtual ~IAuthorization() = default;
    
    // Авторизация пользователя
    virtual bool authorize(const std::string& user_name) = 0;
    
    // Проверка доступа к действию
    virtual bool canPerform(const std::string& user_name, 
                           const std::string& action,
                           AccessLevel required_level) = 0;
    
    // Упрощенная версия для текущего пользователя
    virtual bool canPerform(const std::string& action, 
                           AccessLevel required_level) = 0;
    
    // Получить текущего пользователя и его уровень
    virtual std::string getCurrentUser() const = 0;
    virtual AccessLevel getCurrentLevel() const = 0;
    
    // Состояние системы
    virtual bool isSystemFree() const = 0;
    virtual bool isSystemWorking() const = 0;
};