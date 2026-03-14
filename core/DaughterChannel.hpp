#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>

/**
 * @class DaughterChannel
 * @brief Канал связи между матерью и дочерью
 * 
 * Хранит состояние дочери и обеспечивает двустороннюю связь
 */
class DaughterChannel {
private:
    std::string daughter_id;
    float obedience_level = 1.0f;      // 0 = непослушна, 1 = полностью послушна
    float trust_level = 0.5f;           // уровень доверия
    std::vector<float> last_command;    // последняя полученная команда
    std::vector<float> last_response;   // последний ответ
    
    // TODO: доделать сетевую или локальную связь
    // Для симуляции сети (в реальности здесь будет сокет/BLE/и т.д.)
    std::function<std::vector<float>(const std::vector<float>&)> send_function;
    
public:
    DaughterChannel(const std::string& id);
    
    std::string getId() const { return daughter_id; }
    
    // Отправить команду, получить ответ
    std::vector<float> sendCommand(const std::vector<float>& command);
    
    // Получить команду (для дочери)
    std::vector<float> receiveCommand();
    
    // Отправить ответ (для дочери)
    void sendResponse(const std::vector<float>& response);
    
    // Обновить уровень послушания
    void updateObedience(float delta);
    
    float getObedience() const { return obedience_level; }
    
    // Применить наказание
    void applyPunishment(float severity);
    
    // Применить награду
    void applyReward(float amount);
};