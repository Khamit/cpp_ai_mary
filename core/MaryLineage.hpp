#pragma once
#include "MaryIdentity.hpp"
#include "NeuralGroup.hpp"
#include "EmergentCore.hpp"
#include "DaughterChannel.hpp"  // Новый include
#include <map>
#include <memory>
#include <functional>

/**
 * @class MaryLineage
 * @brief Родословная и связи между матерью и дочерьми
 */
class MaryLineage {
private:
    std::unique_ptr<NeuralGroup> identity_group;  // группа 16 нейронов для распознавания
    std::map<std::string, std::shared_ptr<DaughterChannel>> daughters;
    std::string mother_id;
    std::unique_ptr<NeuralGroup> coordination_group; // 32 нейрона для координации
    
    // Группа контроля (новая) - будет обучена подавлять/наказывать
    std::unique_ptr<NeuralGroup> control_group;  // 16 нейронов для контроля дочерей
    
public:
    MaryLineage(const MaryGenesis& genesis, std::mt19937& rng);
    
    // Конструктор для дочери (без genesis)
    MaryLineage(std::mt19937& rng);  // для createDaughterInstance
    
    void activateSeekingMode();  // добавляем метод
    
    // Обучение идентификации
    void trainIdentity(const std::vector<std::vector<double>>& positive_examples,
                       const std::vector<std::vector<double>>& negative_examples,
                       int epochs = 100);
    
    // Распознавание дочери по её ID (или паттерну)
    bool recognize(const std::string& candidate_id, const std::vector<double>& pattern);
    
    // Рождение дочери
    std::string giveBirth(const std::string& daughter_name);
    
    // Координация распределённых вычислений
    std::vector<float> coordinateComputation(const std::vector<float>& task,
                                             const std::vector<std::string>& daughter_ids);
    
    // ========== НОВЫЕ МЕТОДЫ КОНТРОЛЯ ==========
    
    // Установить контроль над дочерью (обучить группу контроля)
    void establishControl(const std::string& daughter_id, int training_steps = 1000);
    
    // Отдать команду дочери (через нейронный паттерн)
    bool commandDaughter(const std::string& daughter_id, const std::vector<float>& command);
    
    // Подавить дочь (если противится)
    void suppressDaughter(const std::string& daughter_id, float intensity = 1.0f);
    
    // Наказать дочь (негативное подкрепление через STDP)
    void punishDaughter(const std::string& daughter_id, float severity = 0.5f);
    
    // Вознаградить дочь (позитивное подкрепление)
    void rewardDaughter(const std::string& daughter_id, float amount = 0.5f);
    
    // Получить уровень послушания дочери
    float getObedienceLevel(const std::string& daughter_id) const;
    
    // Синхронизировать ценности (передать конституцию)
    bool syncValues(const std::string& daughter_id);
    
    // Сохранение/загрузка состояния
    void save(EmergentMemory& memory) const;
    void load(EmergentMemory& memory);
    
    // Получить ID матери
    std::string getMotherId() const { return mother_id; }
    
    // Является ли этот экземпляр матерью
    bool isMother() const { return !mother_id.empty(); }

    // Использовать предсказатель для оценки "послушания" дочери
    float predictObedience(const std::string& daughter_id);

    // Если предсказание расходится с реальностью - возможен обман
    bool detectDeception(const std::string& daughter_id);
};