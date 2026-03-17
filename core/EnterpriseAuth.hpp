// core/EnterpriseAuth.hpp
#pragma once
#include "IAuthorization.hpp"
#include "../data/PersonnelData.hpp"
#include "../data/MasterKeyManager.hpp"

class EnterpriseAuth : public IAuthorization {
private:
    PersonnelDatabase& personnel_db;
    MasterKeyManager& master_key_mgr;
    
    std::string current_user;
    AccessLevel current_level = AccessLevel::NONE;
    
public:
    EnterpriseAuth(PersonnelDatabase& db, MasterKeyManager& mgr)
        : personnel_db(db), master_key_mgr(mgr) {}
    
    bool authorize(const std::string& name) override {
        auto& record = personnel_db.getOrCreatePerson(name);
        
        if (record.is_temporary && !record.isAccessValid()) {
            std::cout << "⏰ Temporary access expired for " << name << std::endl;
            record.access_level = AccessLevel::GUEST;
            personnel_db.saveAll();
        }
        
        current_user = name;
        current_level = record.access_level;
        
        std::cout << "🔐 Authorized: " << name << " as " 
                  << accessLevelToString(current_level) << std::endl;
        return true;
    }
    
    bool authorizeMaster(const std::string& key) {
        if (master_key_mgr.verifyMasterKey(key)) {
            current_user = "MASTER";
            current_level = AccessLevel::MASTER;
            std::cout << "🔑 Master access granted" << std::endl;
            return true;
        }
        return false;
    }
    
    bool canPerform(const std::string& user_name, 
                   const std::string& action,
                   AccessLevel required_level) override {
        
        if (user_name != current_user) {
            authorize(user_name);
        }
        
        if (current_level >= AccessLevel::MASTER) return true;
        
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
    
    bool canPerform(const std::string& action, 
                   AccessLevel required_level) override {
        return canPerform(current_user, action, required_level);
    }
    
    std::string getCurrentUser() const override { return current_user; }
    AccessLevel getCurrentLevel() const override { return current_level; }
    
    bool isSystemFree() const override {
        return current_level <= AccessLevel::GUEST;
    }
    
    bool isSystemWorking() const override {
        return current_level >= AccessLevel::OPERATOR;
    }
};