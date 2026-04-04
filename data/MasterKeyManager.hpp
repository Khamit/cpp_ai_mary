// data/MasterKeyManager.hpp
#pragma once

#include <string>
#include <fstream>
#include <chrono>
#include <random>
#include <filesystem>
#include "PersonnelData.hpp"

class MasterKeyManager {
private:
    std::string master_key;
    std::chrono::system_clock::time_point last_seen;
    std::string dump_path = "dump/security/";
    
    // Время бездействия до смены мастера (по умолчанию 30 дней)
    const int MASTER_TIMEOUT_DAYS = 30;
    
public:
    MasterKeyManager() {
        std::filesystem::create_directories(dump_path);
        loadKey();
    }
    
    // Генерация нового мастер-ключа при первом запуске
    std::string generateMasterKey() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        
        const char* hex_chars = "0123456789ABCDEF";
        std::string key = "MARY-MASTER-";
        
        for (int i = 0; i < 32; i++) {
            key += hex_chars[dis(gen)];
            if (i % 8 == 7 && i < 31) key += '-';
        }
        
        master_key = key;
        last_seen = std::chrono::system_clock::now();
        saveKey();
        
        std::cout << " New master key generated. Store it safely!\n";
        std::cout << "   Master Key: " << master_key << std::endl;
        
        return master_key;
    }
    
    // Проверка мастер-ключа
    bool verifyMasterKey(const std::string& key) {
        if (key == master_key) {
            last_seen = std::chrono::system_clock::now();
            saveKey();
            return true;
        }
        return false;
    }
    
    // Проверка, активен ли мастер
    bool isMasterActive() {
        auto now = std::chrono::system_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::hours>(
            now - last_seen).count();
        
        return diff < MASTER_TIMEOUT_DAYS * 24;
    }
    
    // Если мастер неактивен, найти нового из списка
    std::string findNewMaster(PersonnelDatabase& personnel_db) {
        // Ищем среди ADMIN с highest trust_score
        std::string new_master;
        float best_score = 0.0f;
        
        for (const auto& [name, record] : personnel_db.getAllRecords()) {
            if (record.access_level >= AccessLevel::ADMIN) {
                if (record.trust_score > best_score) {
                    best_score = record.trust_score;
                    new_master = name;
                }
            }
        }
        
        if (!new_master.empty()) {
            std::cout << "New master elected: " << new_master << std::endl;
            auto& record = personnel_db.getOrCreatePerson(new_master);
            record.access_level = AccessLevel::MASTER;
            record.granted_by = "SYSTEM_AUTO";
            personnel_db.saveAll();
        }
        
        return new_master;
    }
    
private:
    void saveKey() {
        std::ofstream file(dump_path + "master_key.dat");
        file << master_key << std::endl;
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        file << time_t << std::endl;
    }
    
    void loadKey() {
        std::ifstream file(dump_path + "master_key.dat");
        if (file.is_open()) {
            std::getline(file, master_key);
            
            std::string time_str;
            std::getline(file, time_str);
            if (!time_str.empty()) {
                std::time_t time_t = std::stoll(time_str);
                last_seen = std::chrono::system_clock::from_time_t(time_t);
            }
        } else {
            generateMasterKey(); // первый запуск
        }
    }
};