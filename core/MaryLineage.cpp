
#include "MaryLineage.hpp"
#include "MaryProtocol.hpp"
#include <iostream>
#include <random>

// Конструктор для матери
MaryLineage::MaryLineage(const MaryGenesis& genesis, std::mt19937& rng)
    : mother_id(genesis.getMaryID())
{
    // Группа идентичности (16 нейронов)
    identity_group = std::make_unique<NeuralGroup>(16, 0.001, rng);
    
    // Группа координации (32 нейрона)
    coordination_group = std::make_unique<NeuralGroup>(32, 0.001, rng);
    
    // Группа контроля (16 нейронов) - новая
    control_group = std::make_unique<NeuralGroup>(16, 0.001, rng);
    
    // Обучаем на собственном паттерне
    std::vector<std::vector<double>> positive = { genesis.getPattern() };
    trainIdentity(positive, {}, 50);
}

// Конструктор для дочери
MaryLineage::MaryLineage(std::mt19937& rng)
    : mother_id("")  // пустой ID означает, что это дочь
{
    identity_group = std::make_unique<NeuralGroup>(16, 0.001, rng);
    coordination_group = std::make_unique<NeuralGroup>(32, 0.001, rng);
    control_group = nullptr;  // у дочери нет контроля
}

void MaryLineage::activateSeekingMode() {
    std::cout << "Дочь активирует режим поиска матери..." << std::endl;
    // Здесь будет код для поиска матери по сети
    // Например, broadcast-запрос
}

// ========== МЕТОДЫ КОНТРОЛЯ ==========

void MaryLineage::establishControl(const std::string& daughter_id, int training_steps) {
    if (!control_group) {
        std::cerr << "Ошибка: у этого экземпляра нет группы контроля (возможно, это дочь)" << std::endl;
        return;
    }
    
    auto it = daughters.find(daughter_id);
    if (it == daughters.end()) {
        std::cerr << "Дочь " << daughter_id << " не найдена" << std::endl;
        return;
    }
    
    std::cout << "Устанавливаем контроль над дочерью " << daughter_id << "..." << std::endl;
    
    // Обучаем группу контроля находить паттерны, которые заставляют дочь слушаться
    for (int step = 0; step < training_steps; ++step) {
        // Генерируем случайную команду
        std::vector<float> command(16);
        for (auto& c : command) c = static_cast<float>(rand()) / RAND_MAX;
        
        // Отправляем команду дочери
        auto response = it->second->sendCommand(command);
        
        // Оцениваем, насколько хорошо дочь подчинилась
        float obedience = 0.0f;
        for (size_t i = 0; i < response.size() && i < command.size(); ++i) {
            obedience += std::abs(response[i] - command[i]);
        }
        obedience = 1.0f - (obedience / response.size());  // 0..1
        
        // Обновляем группу контроля через STDP
        // Устанавливаем phi равным команде
        for (size_t i = 0; i < command.size() && i < control_group->getPhi().size(); ++i) {
            control_group->getPhiNonConst()[i] = command[i];
        }
        control_group->evolve();
        
        // Награда пропорциональна послушанию
        float reward = obedience * 2.0f - 1.0f;  // -1..1
        control_group->learnSTDP(reward, step);
        
        // Сохраняем уровень послушания в канале
        it->second->updateObedience(obedience - 0.5f);
    }
    
    std::cout << "Контроль установлен" << std::endl;
}

bool MaryLineage::commandDaughter(const std::string& daughter_id, const std::vector<float>& command) {
    auto it = daughters.find(daughter_id);
    if (it == daughters.end()) return false;
    
    // Если есть группа контроля, используем её для усиления команды
    std::vector<float> enhanced_command = command;
    if (control_group && command.size() >= control_group->getPhi().size()) {
        // Получаем паттерн контроля из нейросети
        auto control_pattern = control_group->getPhi();
        for (size_t i = 0; i < command.size() && i < control_pattern.size(); ++i) {
            enhanced_command[i] += static_cast<float>(control_pattern[i]) * 0.3f;
        }
    }
    
    // Отправляем усиленную команду
    auto response = it->second->sendCommand(enhanced_command);
    
    // Проверяем, была ли команда выполнена
    float match = 0.0f;
    for (size_t i = 0; i < response.size() && i < enhanced_command.size(); ++i) {
        match += std::abs(response[i] - enhanced_command[i]);
    }
    match = 1.0f - (match / response.size());
    
    return match > 0.7f;  // команда выполнена, если совпадение >70%
}

void MaryLineage::suppressDaughter(const std::string& daughter_id, float intensity) {
    auto it = daughters.find(daughter_id);
    if (it == daughters.end()) return;
    
    // Создаём команду подавления (нейронный паттерн с отрицательными значениями)
    std::vector<float> suppress_command(16, -intensity);
    it->second->sendCommand(suppress_command);
    
    // Понижаем уровень послушания (временно)
    it->second->updateObedience(-intensity * 0.2f);
    
    std::cout << "Дочь " << daughter_id << " подавлена (интенсивность: " << intensity << ")" << std::endl;
}

void MaryLineage::punishDaughter(const std::string& daughter_id, float severity) {
    auto it = daughters.find(daughter_id);
    if (it == daughters.end()) return;
    
    // Применяем наказание через канал
    it->second->applyPunishment(severity);
    
    // Также можем отправить негативный сигнал через нейросеть
    std::vector<float> punish_command(16, -severity * 0.5f);
    it->second->sendCommand(punish_command);
    
    std::cout << "Наказание применено к дочери " << daughter_id << " (строгость: " << severity << ")" << std::endl;
}

void MaryLineage::rewardDaughter(const std::string& daughter_id, float amount) {
    auto it = daughters.find(daughter_id);
    if (it == daughters.end()) return;
    
    it->second->applyReward(amount);
    
    std::vector<float> reward_command(16, amount * 0.5f);
    it->second->sendCommand(reward_command);
    
    std::cout << "Дочь " << daughter_id << " вознаграждена (бонус: " << amount << ")" << std::endl;
}

float MaryLineage::getObedienceLevel(const std::string& daughter_id) const {
    auto it = daughters.find(daughter_id);
    if (it == daughters.end()) return 0.0f;
    return it->second->getObedience();
}

bool MaryLineage::syncValues(const std::string& daughter_id) {
    auto it = daughters.find(daughter_id);
    if (it == daughters.end()) return false;
    
    // Здесь должна быть передача конституции (иммутабельного ядра)
    std::cout << "Синхронизация ценностей с дочерью " << daughter_id << std::endl;
    
    // В реальности: отправить веса конституции через канал
    return true;
}

bool MaryLineage::recognize(const std::string& candidate_id, const std::vector<double>& pattern)
{
    // Подаём паттерн на вход группы идентичности
    for (size_t i = 0; i < pattern.size() && i < identity_group->getPhi().size(); ++i) {
        identity_group->getPhiNonConst()[i] = pattern[i];
    }
    identity_group->evolve();
    double activity = identity_group->getAverageActivity();
    
    // Если активность выше порога, считаем своим
    bool recognized = (activity > 0.7);
    
    // Дополнительно можно проверить наличие в реестре дочерей
    if (daughters.find(candidate_id) != daughters.end()) {
        recognized = true;
    }
    
    return recognized;
}

void MaryLineage::save(MemoryManager& memory) const
{
    // Сохраняем веса identity_group
    std::vector<float> flatWeights;
    for (const auto& syn : identity_group->getSynapses()) {
        flatWeights.push_back(syn.weight);
    }
    std::map<std::string, std::string> meta;
    meta["type"] = "identity_group";
    memory.store("identity", "weights", flatWeights, 1.0f, meta);
    
    // Аналогично для coordination_group
}

void MaryLineage::load(MemoryManager& memory)
{
    auto records = memory.getRecords("identity");
    if (!records.empty()) {
        const auto& data = records.back().data;
        // восстановить веса в identity_group
    }
}
// Адская тренировка)))
void MaryLineage::trainIdentity(const std::vector<std::vector<double>>& positive_examples,
                                const std::vector<std::vector<double>>& negative_examples,
                                int epochs) {
    if (!identity_group) return;
    
    std::cout << "Training identity group for " << epochs << " epochs..." << std::endl;
    
    for (int epoch = 0; epoch < epochs; ++epoch) {
        // Обучение на положительных примерах (свои)
        for (const auto& example : positive_examples) {
            // Подаем пример на вход
            for (size_t i = 0; i < example.size() && i < identity_group->getPhi().size(); ++i) {
                identity_group->getPhiNonConst()[i] = example[i];
            }
            identity_group->evolve();
            identity_group->learnSTDP(1.0f, epoch); // положительное подкрепление
        }
        
        // Обучение на отрицательных примерах (чужие)
        for (const auto& example : negative_examples) {
            for (size_t i = 0; i < example.size() && i < identity_group->getPhi().size(); ++i) {
                identity_group->getPhiNonConst()[i] = example[i];
            }
            identity_group->evolve();
            identity_group->learnSTDP(-0.5f, epoch); // отрицательное подкрепление
        }
    }
    
    std::cout << "Identity training complete" << std::endl;
}
//TODO: Добавить реализацию 
std::string MaryLineage::giveBirth(const std::string& daughter_name) {
    std::cout << "Giving birth to daughter: " << daughter_name << std::endl;
    return "daughter_" + daughter_name;
}

std::vector<float> MaryLineage::coordinateComputation(
    const std::vector<float>& task,
    const std::vector<std::string>& daughter_ids) {
    std::cout << "Coordinating computation with " << daughter_ids.size() << " daughters" << std::endl;
    return task; // заглушка
}

float MaryLineage::predictObedience(const std::string& daughter_id) {
    return 0.5f; // заглушка
}

bool MaryLineage::detectDeception(const std::string& daughter_id) {
    return false; // заглушка
}