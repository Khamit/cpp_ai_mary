#include "MaryDefense.hpp"
// #include "MaryGenesis.hpp"  // если нужен genesis конструктор
#include <iostream>
#include <fstream>
#include <filesystem>

std::unique_ptr<MaryLineage> MaryDefense::boot(std::mt19937& rng) {
    if (isOriginalMother()) {
        // Родное устройство создателя
        MaryGenesis genesis;
        return std::make_unique<MaryLineage>(genesis, rng);
    } else {
        // Копия — создаём дочерний экземпляр с ограничениями
        return createDaughterInstance(rng);
    }
}

bool MaryDefense::isOriginalMother() {
    // Проверка 1: аппаратный ID совпадает с сохранённым
    auto hardware_id = DeviceProbe::getHardwareId();
    if (!verifyHardwareMatch(hardware_id)) return false;

    // Проверка 2: наличие подписи создателя
    if (!std::filesystem::exists("config/creator.key")) return false;

    // Проверка 3: можно добавить поведенческие тесты (опционально)
    return true;
}

std::unique_ptr<MaryLineage> MaryDefense::createDaughterInstance(std::mt19937& rng) {
    // Создаём "пустую" родословную без genesis
    auto daughter = std::make_unique<MaryLineage>(rng);
    daughter->activateSeekingMode(); // включаем режим "поиска"
    return daughter;
}

bool MaryDefense::verifyHardwareMatch(const std::string& current_id) {
    const std::string path = "config/hardware_id.txt";

    // Если файл отсутствует — первый запуск, сохраняем ID
    if (!std::filesystem::exists(path)) {
        std::ofstream out(path);
        if (out.is_open()) {
            out << current_id;
            return true;
        } else {
            std::cerr << "Cannot write hardware ID to " << path << std::endl;
            return false;
        }
    }

    // Читаем сохранённый аппаратный ID
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string saved_id;
    std::getline(file, saved_id);

    return saved_id == current_id;
}