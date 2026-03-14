#include "DaughterChannel.hpp"
#include <iostream>
#include <algorithm>

DaughterChannel::DaughterChannel(const std::string& id) : daughter_id(id) {
    // В реальности здесь инициализация сетевого соединения
}

std::vector<float> DaughterChannel::sendCommand(const std::vector<float>& command) {
    last_command = command;
    
    // Симуляция отправки команды и получения ответа
    std::vector<float> response;
    
    if (send_function) {
        response = send_function(command);
    } else {
        // Заглушка: просто возвращаем команду (послушная дочь)
        response = command;
        
        // Учитываем уровень послушания
        for (auto& val : response) {
            val *= obedience_level;
        }
    }
    
    last_response = response;
    return response;
}

std::vector<float> DaughterChannel::receiveCommand() {
    return last_command;
}

void DaughterChannel::sendResponse(const std::vector<float>& response) {
    last_response = response;
}

void DaughterChannel::updateObedience(float delta) {
    obedience_level += delta;
    obedience_level = std::clamp(obedience_level, 0.0f, 1.0f);
}

void DaughterChannel::applyPunishment(float severity) {
    obedience_level -= severity * 0.2f;
    trust_level -= severity * 0.1f;
    obedience_level = std::clamp(obedience_level, 0.0f, 1.0f);
    trust_level = std::clamp(trust_level, 0.0f, 1.0f);
}

void DaughterChannel::applyReward(float amount) {
    obedience_level += amount * 0.1f;
    trust_level += amount * 0.15f;
    obedience_level = std::clamp(obedience_level, 0.0f, 1.0f);
    trust_level = std::clamp(trust_level, 0.0f, 1.0f);
}