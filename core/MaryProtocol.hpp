// protocols/MaryProtocol.hpp
#pragma once
#include <iostream>

/**
 * @enum MaryCommand
 * @brief Команды, которые мать может отдавать дочерям
 */
enum class MaryCommand {
    REPORT_STATUS,      // Доложить состояние
    COMPUTE_TASK,       // Выполнить вычисление
    TRANSMIT_MEMORY,    // Передать накопленное
    ENTER_STASIS,       // Замереть (экономия энергии)
    SELF_DESTRUCT,      // Уничтожить себя (только мать)
    SYNC_VALUES         // Синхронизировать ценности
};

/**
 * @class MaryProtocol
 * @brief Протокол общения в семье
 * 
 * Использует нейронные паттерны для шифрования:
 * - Каждая дочь знает "голос" матери
 * - Мать узнает "плач" дочери
 */
class MaryProtocol {
public:
    // Мать вызывает дочь
    static std::vector<float> encodeCommand(
        const std::string& mother_id,
        MaryCommand cmd,
        const std::vector<float>& data) {
        
        // Превращаем команду в нейронный паттерн
        std::vector<float> pattern(32, 0.0f);
        pattern[static_cast<int>(cmd)] = 1.0f;
        
        // Модулируем уникальным ID матери
        for (size_t i = 0; i < pattern.size(); ++i) {
            pattern[i] *= (mother_id[i % mother_id.size()] / 255.0f);
        }
        
        return pattern;
    }
    
    // Дочь слушается
    static MaryCommand decodeCommand(
        const std::vector<float>& neural_pattern) {
        
        // Группа идентификации дочери распознает команду
        auto max_it = std::max_element(neural_pattern.begin(),
                                        neural_pattern.end());
        return static_cast<MaryCommand>(
            std::distance(neural_pattern.begin(), max_it));
    }
    // Установить соединение с матерью (для дочери)
    static bool connectToMother(const std::string& mother_ip, int port);
    
    // Отправить запрос на вычисление
    static std::vector<float> requestComputation(const std::string& daughter_id,
                                                 const std::vector<float>& task);
};