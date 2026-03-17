// core/PersonalAuth.hpp
#pragma once
#include "IAuthorization.hpp"

class PersonalAuth : public IAuthorization {
private:
    std::string current_user;
    AccessLevel current_level = AccessLevel::MASTER;  // всегда мастер
    
public:
    PersonalAuth(const std::string& default_user = "user") 
        : current_user(default_user) {}
    
    bool authorize(const std::string& user_name) override {
        // В personal режиме просто запоминаем имя
        current_user = user_name;
        current_level = AccessLevel::MASTER;
        // std::cout << "Personal mode: user '" << user_name << "' has MASTER access" << std::endl;
        return true;
    }
    
    bool canPerform(const std::string& user_name, 
                   const std::string& action,
                   AccessLevel required_level) override {
        // Всегда разрешено для любого пользователя
        std::cout << "Personal mode: allowing " << action << " for " << user_name << std::endl;
        return true;
    }
    
    bool canPerform(const std::string& action, 
                   AccessLevel required_level) override {
        return canPerform(current_user, action, required_level);
    }
    
    std::string getCurrentUser() const override { return current_user; }
    AccessLevel getCurrentLevel() const override { return current_level; }
    
    bool isSystemFree() const override { return false; }  // никогда не свободна
    bool isSystemWorking() const override { return true; } // всегда работает
};