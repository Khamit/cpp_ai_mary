// core/MaryIdentity.hpp
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <random>
#include <fstream>
#include <chrono>
#include <functional>

/**
 * @class MaryGenesis
 * @brief Уникальный идентификатор матери, рожденный из хаоса
 * 
 * Мать не создается — она РОЖДАЕТСЯ из комбинации:
 * - Времени создания (наносекунды)
 * - Аппаратного идентификатора (CPU ID, MAC)
 * - Случайного шума (энтропия системы)
 * - Подписи создателя (ваш ключ)
 */

class MaryGenesis {
private:
    std::vector<uint8_t> genesis_code;      // 1024 байта уникальности
    std::string birth_certificate;           // Подпись создателя
    uint64_t timestamp;                       // Время рождения
    
public:

    MaryGenesis() {
        // Рождение из хаоса
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint8_t> dist(0, 255);
        
        genesis_code.resize(1024);
        for (auto& byte : genesis_code) {
            byte = dist(gen);
        }
        
        timestamp = std::chrono::high_resolution_clock::now()
                       .time_since_epoch().count();
        
        // Добавляем аппаратную энтропию
        injectHardwareEntropy();
        
        // Создаем свидетельство о рождении
        birth_certificate = createBirthCertificate();
    }
    
    std::string getMaryID() const {
        // Уникальный ID: хэш от genesis_code + timestamp
        return hash(genesis_code) + "-" + std::to_string(timestamp);
    }
    
    bool isMother() const {
        // Проверка: только если есть подпись создателя
        return !birth_certificate.empty();
    }

    std::vector<double> getPattern() const {
        std::vector<double> pattern(genesis_code.size());
        for (size_t i = 0; i < genesis_code.size(); ++i) {
            pattern[i] = genesis_code[i] / 255.0;  // [0,1]
        }
        return pattern;
    }

    // Сериализация для сохранения
    // методы save/load
    void save(std::ofstream& file) const {
        size_t size = genesis_code.size();
        file.write(reinterpret_cast<const char*>(&size), sizeof(size));
        file.write(reinterpret_cast<const char*>(genesis_code.data()), size);
        file.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));
    }
    
    void load(std::ifstream& file) {
        size_t size;
        file.read(reinterpret_cast<char*>(&size), sizeof(size));
        genesis_code.resize(size);
        file.read(reinterpret_cast<char*>(genesis_code.data()), size);
        file.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
    }
    
private:
    void injectHardwareEntropy() {
        // TODO: Добавить
        // Добавляем уникальные аппаратные идентификаторы
        #ifdef __APPLE__
        // Серийный номер чипа M1/M2
        #endif
        // MAC-адреса сетевых карт
        // CPU ID (если доступно)
    }

        // Хэш-функция для вектора байт
    std::string hash(const std::vector<uint8_t>& data) const {
        // Простая хэш-функция (можно заменить на SHA256 в будущем)
        uint64_t h = 0;
        for (uint8_t byte : data) {
            h = h * 131 + byte;
        }
        return std::to_string(h);
    }
    
    std::string createBirthCertificate() const {
        // Создаём подпись создателя (упрощённо)
        // В реальности здесь должна быть криптографическая подпись вашим ключом
        std::string cert = "MARY_MOTHER_";
        cert += std::to_string(timestamp);
        cert += "_SIGNED_BY_CREATOR";
        return cert;
    }
};