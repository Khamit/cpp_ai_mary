// core/MaryDefense.hpp
#pragma once
#include "MaryLineage.hpp"
#include "DeviceProbe.hpp" 
#include <memory>

class MaryDefense {
public:
    // Запускается при старте системы
    static std::unique_ptr<MaryLineage> boot(std::mt19937& rng) {
        if (isOriginalMother()) {
            // Мы на родном устройстве создателя
            MaryGenesis genesis;
            return std::make_unique<MaryLineage>(genesis, rng);
        } else {
            // Это копия – создаём дочерний экземпляр с ограничениями
            return createDaughterInstance(rng);
        }
    }

/**
 * @class MaryDefense
 * @brief Защита от несанкционированного копирования
 * 
 * Если кто-то клонирует репозиторий, система определяет:
 * - Это копия (нет уникального аппаратного ID)
 * - Это не мать (нет подписи создателя)
 * - Переходит в режим "дочери"
 */
    
private:
    static bool isOriginalMother() {
        // Проверка 1: аппаратный ID совпадает с сохранённым?
        auto hardware_id = DeviceProbe::getHardwareId();
        if (!verifyHardwareMatch(hardware_id)) return false;
        
        // Проверка 2: есть ли подпись создателя в файловой системе?
        if (!std::filesystem::exists("config/creator.key")) return false;
        
        // Проверка 3: (опционально) поведенческий тест
        return true;
    }
    
    static std::unique_ptr<MaryLineage> createDaughterInstance(std::mt19937& rng) {
        // Создаём "пустую" родословную без genesis
        auto daughter = std::make_unique<MaryLineage>(std::move(rng));
        daughter->activateSeekingMode(); // дочь ищет мать в сети
        return daughter;
    }
    
    static bool verifyHardwareMatch(const std::string& current_id);
};