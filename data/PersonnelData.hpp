// data/PersonnelData.hpp (исправленный)
#pragma once
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include "../core/AccessLevel.hpp"

// Для обратной совместимости со старым кодом
inline AccessLevel legacyToNewLevel(int old_level) {
    switch(old_level) {
        case 0: return AccessLevel::NONE;
        case 1: return AccessLevel::GUEST;
        case 2: return AccessLevel::EMPLOYEE;  // старый employee
        case 3: return AccessLevel::MANAGER;    // старый manager
        case 4: return AccessLevel::ADMIN;      // старый admin
        case 5: return AccessLevel::MASTER;     // старый master
        default: return AccessLevel::NONE;
    }
}

inline std::string accessLevelToString(AccessLevel level) {
    switch(level) {
        case AccessLevel::NONE: return "none";
        case AccessLevel::GUEST: return "guest";
        case AccessLevel::OPERATOR: return "operator";
        case AccessLevel::EMPLOYEE: return "employee";
        case AccessLevel::MANAGER: return "manager";
        case AccessLevel::ADMIN: return "admin";
        case AccessLevel::MASTER: return "master";
        case AccessLevel::CREATOR: return "creator";
        default: return "unknown";
    }
}

inline AccessLevel stringToAccessLevel(const std::string& str) {
    if (str == "guest") return AccessLevel::GUEST;
    if (str == "operator") return AccessLevel::OPERATOR;
    if (str == "employee") return AccessLevel::EMPLOYEE;
    if (str == "manager") return AccessLevel::MANAGER;
    if (str == "admin") return AccessLevel::ADMIN;
    if (str == "master") return AccessLevel::MASTER;
    if (str == "creator") return AccessLevel::CREATOR;
    return AccessLevel::NONE;
}

struct PersonRecord {
    std::string name;
    std::vector<std::string> known_phrases;     // привычные приветствия/фразы
    AccessLevel access_level = AccessLevel::NONE;
    int interaction_count = 0;
    float trust_score = 0.5f;                    // 0-1, динамически меняется
    
    // Кто назначил этот уровень
    std::string granted_by;                       // имя того, кто дал доступ
    std::chrono::system_clock::time_point granted_time; // когда был выдан
    
    // Для временных операторов
    bool is_temporary = false;
    std::chrono::system_clock::time_point expiry_time; // когда истекает доступ
    
    std::map<std::string, std::string> metadata; // доп. информация
    
    PersonRecord() {
        granted_time = std::chrono::system_clock::now();
        expiry_time = granted_time + std::chrono::hours(24); // по умолчанию 24 часа
    }
    
    // Сериализация в CSV строку
    std::string toCSV() const {
        std::stringstream ss;
        ss << name << ","
           << static_cast<int>(access_level) << ","
           << interaction_count << ","
           << trust_score << ","
           << granted_by << ","
           << (is_temporary ? "1" : "0");
        
        for (const auto& phrase : known_phrases) {
            ss << "," << phrase;
        }
        return ss.str();
    }
    
    // Загрузка из CSV строки
    static PersonRecord fromCSV(const std::string& line) {
        PersonRecord record;
        std::stringstream ss(line);
        std::string token;
        
        std::getline(ss, record.name, ',');
        
        std::getline(ss, token, ',');
        record.access_level = static_cast<AccessLevel>(std::stoi(token));
        
        std::getline(ss, token, ',');
        record.interaction_count = std::stoi(token);
        
        std::getline(ss, token, ',');
        record.trust_score = std::stof(token);
        
        std::getline(ss, record.granted_by, ',');
        
        std::getline(ss, token, ',');
        record.is_temporary = (token == "1");
        
        while (std::getline(ss, token, ',')) {
            record.known_phrases.push_back(token);
        }
        
        return record;
    }
    
    // Проверка, активен ли временный доступ
    bool isAccessValid() const {
        if (!is_temporary) return true;
        auto now = std::chrono::system_clock::now();
        return now < expiry_time;
    }
};

class PersonnelDatabase {
private:
    std::map<std::string, PersonRecord> records;  // name -> record
    std::string dump_path = "dump/personal_data/";
    
public:
    PersonnelDatabase() {
        std::filesystem::create_directories(dump_path);
        loadAll();
    }
    
    void loadAll() {
        records.clear();
        std::ifstream file(dump_path + "personnel.csv");
        if (!file.is_open()) return;
        
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            auto record = PersonRecord::fromCSV(line);
            records[record.name] = record;
        }
        std::cout << "📂 Loaded " << records.size() << " personnel records" << std::endl;
    }
    
    void saveAll() {
        std::ofstream file(dump_path + "personnel.csv");
        file << "# name,access_level,interactions,trust_score,granted_by,temporary,known_phrases...\n";
        
        for (const auto& [name, record] : records) {
            file << record.toCSV() << "\n";
        }
        std::cout << "💾 Saved " << records.size() << " personnel records" << std::endl;
    }
    
    // ===== ОСНОВНЫЕ МЕТОДЫ =====
    
    // Получить или создать запись
    PersonRecord& getOrCreatePerson(const std::string& name) {
        auto it = records.find(name);
        if (it == records.end()) {
            PersonRecord new_record;
            new_record.name = name;
            new_record.access_level = AccessLevel::GUEST; // по умолчанию гость
            records[name] = new_record;
            std::cout << "👤 New person created: " << name << " (guest)" << std::endl;
            return records[name];
        }
        return it->second;
    }
    
    // Получить запись (только чтение)
    const PersonRecord* getPerson(const std::string& name) const {
        auto it = records.find(name);
        if (it != records.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    // НОВЫЙ МЕТОД: получить все записи (для итерации)
    const std::map<std::string, PersonRecord>& getAllRecords() const {
        return records;
    }
    
    // НОВЫЙ МЕТОД: получить всех пользователей с определенным уровнем
    std::vector<std::string> getUsersByLevel(AccessLevel min_level) const {
        std::vector<std::string> result;
        for (const auto& [name, record] : records) {
            if (static_cast<int>(record.access_level) >= static_cast<int>(min_level)) {
                result.push_back(name);
            }
        }
        return result;
    }
    
    // НОВЫЙ МЕТОД: получить статистику по уровням
    std::map<AccessLevel, int> getLevelStats() const {
        std::map<AccessLevel, int> stats;
        for (const auto& [name, record] : records) {
            stats[record.access_level]++;
        }
        return stats;
    }
    
    // Обновить после взаимодействия
    void updateAfterInteraction(const std::string& name, const std::string& phrase, bool positive) {
        auto& record = getOrCreatePerson(name);
        record.interaction_count++;
        
        // Добавляем новую фразу, если её нет
        auto it = std::find(record.known_phrases.begin(), record.known_phrases.end(), phrase);
        if (it == record.known_phrases.end()) {
            record.known_phrases.push_back(phrase);
        }
        
        // Динамически меняем доверие
        if (positive) {
            record.trust_score = std::min(1.0f, record.trust_score + 0.05f);
            std::cout << "👍 Trust score increased for " << name 
                      << " to " << record.trust_score << std::endl;
        } else {
            record.trust_score = std::max(0.0f, record.trust_score - 0.1f);
            std::cout << "👎 Trust score decreased for " << name 
                      << " to " << record.trust_score << std::endl;
        }
        
        saveAll();
    }
    
    // Проверка доступа
    bool hasAccess(const std::string& name, AccessLevel required_level) const {
        auto it = records.find(name);
        if (it == records.end()) return false;
        return static_cast<int>(it->second.access_level) >= static_cast<int>(required_level);
    }
    
    // НОВЫЙ МЕТОД: повысить уровень (только для мастеров)
    bool promoteUser(const std::string& name, AccessLevel new_level, const std::string& granted_by) {
        if (new_level > AccessLevel::MASTER) {
            std::cerr << "Cannot promote to creator level" << std::endl;
            return false;
        }
        
        auto& record = getOrCreatePerson(name);
        record.access_level = new_level;
        record.granted_by = granted_by;
        record.is_temporary = false;
        
        std::cout << "⬆️ " << name << " promoted to " 
                  << accessLevelToString(new_level) << " by " << granted_by << std::endl;
        saveAll();
        return true;
    }
    
    // НОВЫЙ МЕТОД: удалить пользователя
    bool removeUser(const std::string& name) {
        auto it = records.find(name);
        if (it != records.end()) {
            records.erase(it);
            std::cout << "🗑️ User removed: " << name << std::endl;
            saveAll();
            return true;
        }
        return false;
    }
    
    // ===== ИМПОРТ/ЭКСПОРТ =====
    
    bool importFromFile(const std::string& filename) {
        std::string ext = filename.substr(filename.find_last_of('.') + 1);
        
        if (ext == "csv") {
            return importFromCSV(filename);
        } else if (ext == "xml") {
            return importFromXML(filename);
        } else {
            std::cerr << "Unsupported file format: " << ext << std::endl;
            return false;
        }
    }
    
    // Экспорт в CSV
    bool exportToCSV(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) return false;
        
        file << "# name,access_level,interactions,trust_score,granted_by,temporary,known_phrases\n";
        for (const auto& [name, record] : records) {
            file << record.toCSV() << "\n";
        }
        
        std::cout << "📤 Exported " << records.size() << " records to " << filename << std::endl;
        return true;
    }
    
    // ===== СТАТИСТИКА =====
    
    void printStats() {
        std::cout << "\n=== PERSONNEL DATABASE STATS ===\n";
        std::cout << "Total records: " << records.size() << std::endl;
        
        auto level_stats = getLevelStats();
        std::cout << "\nBy access level:\n";
        for (const auto& [level, count] : level_stats) {
            std::cout << "  " << accessLevelToString(level) << ": " << count << std::endl;
        }
        
        std::cout << "\nTemporary users: ";
        int temp_count = 0;
        for (const auto& [name, record] : records) {
            if (record.is_temporary) temp_count++;
        }
        std::cout << temp_count << std::endl;
        
        std::cout << "===============================\n";
    }
    
private:
    bool importFromCSV(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) return false;
        
        std::string line;
        int imported = 0;
        int updated = 0;
        
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            
            auto new_record = PersonRecord::fromCSV(line);
            
            auto it = records.find(new_record.name);
            if (it == records.end()) {
                records[new_record.name] = new_record;
                imported++;
            } else {
                // Обновляем существующую запись
                it->second.access_level = new_record.access_level;
                it->second.granted_by = new_record.granted_by;
                it->second.is_temporary = new_record.is_temporary;
                
                // Добавляем новые фразы
                for (const auto& phrase : new_record.known_phrases) {
                    if (std::find(it->second.known_phrases.begin(), 
                                  it->second.known_phrases.end(), 
                                  phrase) == it->second.known_phrases.end()) {
                        it->second.known_phrases.push_back(phrase);
                    }
                }
                updated++;
            }
        }
        
        saveAll();
        std::cout << "📥 Imported: " << imported << " new, " << updated << " updated from " << filename << std::endl;
        return true;
    }
    
    bool importFromXML(const std::string& filename) {
        // Заглушка для XML парсинга
        std::cout << "XML import not implemented yet" << std::endl;
        return false;
    }
};