#pragma once
#include "MaryLineage.hpp"
#include "DeviceProbe.hpp"
#include <memory>
#include <random>
#include <filesystem>

/**
 * @class MaryDefense
 * @brief Защита от несанкционированного копирования системы
 *
 * Система проверяет:
 * - Уникальный аппаратный ID совпадает с оригиналом
 * - Наличие подписи создателя
 * Если проверки не пройдены, создаётся "дочерняя" версия с ограничениями.
 */
class MaryDefense {
public:
    // Запуск системы — возвращает корректный экземпляр MaryLineage
    static std::unique_ptr<MaryLineage> boot(std::mt19937& rng);

private:
    static bool isOriginalMother();
    static std::unique_ptr<MaryLineage> createDaughterInstance(std::mt19937& rng);
    static bool verifyHardwareMatch(const std::string& current_id);
};