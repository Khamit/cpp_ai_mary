// data/AccessManager.hpp
#pragma once
#include "PersonnelData.hpp"
#include "MasterKeyManager.hpp"

class AccessManager {
private:
    PersonnelDatabase& personnel_db;
    MasterKeyManager& master_key_mgr;
    
    // Текущий пользователь
    std::string current_user;
    AccessLevel current_level = AccessLevel::NONE;
    
public:
    AccessManager(PersonnelDatabase& db, MasterKeyManager& mgr)
        : personnel_db(db), master_key_mgr(mgr) {}
    
    // Авторизация по имени
    bool authorize(const std::string& name) {
        auto& record = personnel_db.getOrCreatePerson(name);
        
        // Проверяем временный доступ
        if (record.is_temporary && !record.isAccessValid()) {
            std::cout << " Temporary access expired for " << name << std::endl;
            record.access_level = AccessLevel::GUEST;
            personnel_db.saveAll();
        }
        
        current_user = name;
        current_level = record.access_level;
        
        std::cout << " Authorized: " << name << " as " 
                  << accessLevelToString(current_level) << std::endl;
        return true;
    }
    
    // Авторизация по мастер-ключу
    bool authorizeMaster(const std::string& key) {
        if (master_key_mgr.verifyMasterKey(key)) {
            current_user = "MASTER";
            current_level = AccessLevel::MASTER;
            std::cout << " Master access granted" << std::endl;
            return true;
        }
        return false;
    }
    
    // Проверка доступа к действию
    // ИСПРАВЛЕННЫЙ МЕТОД: проверка доступа к действию
    bool canPerform(const std::string& user_name, 
                    const std::string& action,
                    AccessLevel required_level) {
        
        // Сначала авторизуем пользователя, если это другой пользователь
        if (user_name != current_user) {
            authorize(user_name);
        }
        
        // Проверяем мастер-ключ (мастер может всё)
        if (current_level >= AccessLevel::MASTER) return true;
        
        // Проверяем уровень пользователя
        if (static_cast<int>(current_level) >= static_cast<int>(required_level)) {
            std::cout << "(+) " << user_name << " can perform " << action 
                      << " (level: " << accessLevelToString(current_level) 
                      << " >= " << accessLevelToString(required_level) << ")" << std::endl;
            return true;
        }
        
        std::cout << "(-) Access denied: " << user_name 
                  << " needs " << accessLevelToString(required_level) 
                  << " for " << action 
                  << " (current: " << accessLevelToString(current_level) << ")" << std::endl;
        return false;
    }

        // Упрощенная версия для текущего пользователя
    bool canPerform(const std::string& action, AccessLevel required_level) {
        return canPerform(current_user, action, required_level);
    }
    
    // Дать временный доступ оператору
    void grantTemporaryAccess(const std::string& name, int hours = 24) {
        if (current_level < AccessLevel::ADMIN) {
            std::cout << " Only admin can grant temporary access" << std::endl;
            return;
        }
        
        auto& record = personnel_db.getOrCreatePerson(name);
        record.access_level = AccessLevel::OPERATOR;
        record.is_temporary = true;
        record.expiry_time = std::chrono::system_clock::now() + 
                             std::chrono::hours(hours);
        record.granted_by = current_user;
        personnel_db.saveAll();
        
        std::cout << " Temporary operator access granted to " << name 
                  << " for " << hours << " hours" << std::endl;
    }
    
    // Повысить уровень (только мастер)
    void promoteUser(const std::string& name, AccessLevel new_level) {
        if (current_level < AccessLevel::MASTER) {
            std::cout << " Only master can promote users" << std::endl;
            return;
        }
        
        auto& record = personnel_db.getOrCreatePerson(name);
        record.access_level = new_level;
        record.granted_by = current_user;
        record.is_temporary = false;
        personnel_db.saveAll();
        
        std::cout << "(^)" << name << " promoted to " 
                  << accessLevelToString(new_level) << std::endl;
    }
    
    // Получить уровень текущего пользователя
    AccessLevel getCurrentLevel() const { return current_level; }
    std::string getCurrentUser() const { return current_user; }
    
    // Состояние системы (свободна или нет)
    bool isSystemFree() const {
        return current_level <= AccessLevel::GUEST;
    }
    
    // Система в рабочем режиме
    bool isSystemWorking() const {
        return current_level >= AccessLevel::OPERATOR;
    }
};