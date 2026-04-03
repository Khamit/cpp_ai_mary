#pragma once
#include "NeuralGroup.hpp"
#include <vector>
#include <random>
#include <string>
#include <deque>
#include <algorithm>
#include <cmath>
#include "EventSystem.hpp"
#include "core/INeuralGroupAccess.hpp"
#include "DynamicParams.hpp"
#include "core/MemoryManager.hpp"
#include <mutex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "PredictiveCoder.hpp"

// *Информационная свободная энергия* 
// адаптивная целевая энтропия - предсказываем по энтропии
/*
парадокс энтропии -
Высокая энтропия (хаос) ≠ хорошая система
Низкая энтропия (порядок) ≠ хорошая система
Нужен: Хаос выйти в порядок что равно компактность и эфективность
при малом обьеме информации - так система будет предсказывать - эффективность
*/
// FreeEnergyController должен БРАТЬ энергию извне, а не вычислять её сам
class FreeEnergyController {
private:
    double free_energy_;
    double target_free_energy_;
    double temperature_;
    
public:
    // Энергия передаётся извне!
    void update(double energy, double entropy, double prediction_error) {
        free_energy_ = energy - temperature_ * entropy;
        
        if (prediction_error > 0.3) {
            target_free_energy_ = free_energy_ * 1.1;
        } else {
            target_free_energy_ = free_energy_ * 0.95;
        }
        
        double free_energy_error = target_free_energy_ - free_energy_;
        temperature_ += 0.01 * free_energy_error;
        temperature_ = std::clamp(temperature_, 0.1, 2.0);
    }
    
    // Этот метод должен получать целевую энтропию ИЗВНЕ
    double getTargetEntropy(double energy) const {
        // S_target = (E - F_target) / T
        return (energy - target_free_energy_) / (temperature_ + 1e-6);
    }
    
    double getTemperature() const { return temperature_; }
};

/**
 * @class AttentionMechanism
 * @brief Механизм внимания с энтропийной регуляризацией и геометрической нормализацией
 * 
 * Логика: 
 * 1. Веса внимания нормализуются как распределение на гиперсфере (с использованием π)
 * 2. Энтропия распределения используется как мера неопределенности
 * 3. Температура регулирует компромисс между исследованием и эксплуатацией
 */
struct AttentionMechanism {
    std::vector<double> attention_weights;  // веса внимания для групп
    std::vector<float> context_vector;      // контекстный вектор
    float temperature = 1.0f;                // температура для softmax
    double entropy = 0.0;                    // текущая энтропия распределения
    
    /**
     * Вычисление весов внимания с softmax и расчет энтропии
     * @param groupActivities - вектор активностей групп
     */
    void computeAttention(const std::vector<double>& groupActivities) {
        const size_t n = groupActivities.size();
        attention_weights.resize(n);

        if (n == 0) return;

        float maxVal = static_cast<float>(*std::max_element(
            groupActivities.begin(), groupActivities.end()));

        const float temp = (temperature <= 1e-6f) ? 1e-6f : temperature;

        float sum = 0.0f;
        for (size_t i = 0; i < n; i++) {
            float v = std::exp((static_cast<float>(groupActivities[i]) - maxVal) / temp);
            attention_weights[i] = v;
            sum += v;
        }

        if (sum <= 1e-12f) {
            float uniform = 1.0f / static_cast<float>(n);
            for (auto& w : attention_weights) w = uniform;
            entropy = std::log(static_cast<double>(n)); // максимальная энтропия для равномерного
            return;
        }

        float invSum = 1.0f / sum;
        entropy = 0.0;
        for (auto& w : attention_weights) {
            w *= invSum;
            if (w > 1e-12) {
                entropy -= w * std::log(static_cast<double>(w));
            }
        }
    }
    
    /**
     * Вычисление геометрической нормализации на гиперсфере (с использованием π)
     * @param groupActivities - вектор активностей групп
     * @return нормированный вектор на сфере
     */
    std::vector<double> computeSphericalAttention(const std::vector<double>& groupActivities) {
        const size_t n = groupActivities.size();
        std::vector<double> result(n, 0.0);
        
        // Нормализация вектора как точки на сфере
        double norm = 0.0;
        for (double a : groupActivities) norm += a * a;
        if (norm < 1e-12) return result;
        
        norm = std::sqrt(norm);
        
        // Площадь поверхности единичной сферы в n-мерном пространстве
        // S(n) = 2 * π^(n/2) / Γ(n/2)
        double sphere_area = 2.0 * std::pow(M_PI, n / 2.0) / std::tgamma(n / 2.0 + 1);
        
        for (size_t i = 0; i < n; i++) {
            result[i] = (groupActivities[i] / norm) / sphere_area;
        }
        
        return result;
    }
};

/**
 * @class NeuralFieldSystem
 * @brief Контейнер групп нейронов с межгрупповыми связями и механизмом повторного входа
 * 
 * Архитектура:
 * - 32 группы по 32 нейрона (1024 нейрона)
 * - Трехуровневая система обучения (быстрое/медленное/эволюция)
 * - Энтропийная регуляризация для предотвращения коллапса режимов
 * - Геометрические ограничения связей через π-функции
 */
class NeuralFieldSystem : public INeuralGroupAccess { 
public:

    static constexpr int NUM_GROUPS = 32;       ///< 32 группы
    static constexpr int GROUP_SIZE = 32;       ///< 32 нейрона в группе
    static constexpr int TOTAL_NEURONS = NUM_GROUPS * GROUP_SIZE; // 1024
    static constexpr int SYSTEM_ENTROPY_BINS = 20;  // можно оставить 20 для системы
    
    /**
     * @param dt Глобальный шаг интегрирования
     */
    NeuralFieldSystem(double dt, EventSystem& events);
   
    // ИЗМЕНИТЬ существующий метод на вызов нового с параметрами по умолчанию
    void initializeRandom(std::mt19937& rng) {
        initializeWithLimits(rng, MassLimits());  // вызывает новую версию
    }
    
    // НОВЫЙ МЕТОД: инициализация с лимитами массы
    void initializeWithLimits(std::mt19937& rng, const MassLimits& limits);

    // Запрет копирования
    NeuralFieldSystem(const NeuralFieldSystem&) = delete;
    NeuralFieldSystem& operator=(const NeuralFieldSystem&) = delete;

    /**
     * Основной шаг симуляции с трехуровневой архитектурой
     * @param globalReward - глобальный сигнал награды
     * @param stepNumber - номер текущего шага
     */
    void step(float globalReward, int stepNumber);

    /** Вычислить общую энергию системы */
    double computeTotalEnergy() const;
    
    /** Вычислить энтропию всей системы */
    double computeSystemEntropy() const;

    // Entropy compute
    double computeOptimalStructure();
    void diagnoseCriticality();

    /// Штраф -
    /**
     * Применить штраф за потребление ресурсов
     * @param penalty - отрицательное значение при высоком потреблении
     */
    void applyResourcePenalty(float penalty) {
        // 1. Применяем штраф ко всем группам
        for (auto& group : groups) {
            group.applyGlobalPenalty(penalty);
        }
        
        // 2. При высоком штрафе (перегрузка) снижаем энтропию
        if (penalty < -0.3f) {
            attention.temperature *= 0.98f;
            attention.temperature = std::clamp(attention.temperature, 0.1f, 5.0f);
            
            static int log_counter = 0;
            if (log_counter++ % 100 == 0) {
                std::cout << "⚠️ High resource usage! Penalty: " << penalty 
                        << ", Temp reduced to: " << attention.temperature << std::endl;
            }
        }
        
        // 3. При экстремальной перегрузке вызываем принудительную деградацию
        if (penalty < -0.7f) {
            for (auto& group : groups) {
                group.checkForResourceExhaustion();
            }
        }
    }

    // НОВЫЕ МЕТОДЫ для свободной энергии
    void updateFreeEnergy(double prediction_error) {
        double energy = computeTotalEnergy();
        double entropy = computeSystemEntropy();
        free_energy_controller_.update(energy, entropy, prediction_error);
        
        // НЕ вызывайте getOptimalEntropy()! Вместо этого:
        double optimal_entropy = computeOptimalStructure();
        if (std::isfinite(optimal_entropy) && optimal_entropy > 0) {
            // Корректируем температуру для достижения оптимальной энтропии
            double entropy_error = optimal_entropy - entropy;
            attention.temperature *= (1.0 + 0.01 * entropy_error);
            attention.temperature = std::clamp(attention.temperature, 0.1f, 5.0f);
        }
    }

    // ЕДИНАЯ энтропия для всей системы (в БИТАХ)
    double getUnifiedEntropy() const {
        rebuildFlatVectors();
        
        const int BINS = 32;  // больше бинов для точности
        std::vector<int> hist(BINS, 0);
        
        // 1. Гистограмма активностей
        for (double v : flatPhi) {
            int bin = static_cast<int>(v * BINS);
            bin = std::clamp(bin, 0, BINS - 1);
            hist[bin]++;
        }
        
        // 2. Энтропия Шеннона в БИТАХ (log2)
        double H = 0.0;
        double total = static_cast<double>(TOTAL_NEURONS);
        
        for (int count : hist) {
            if (count > 0) {
                double p = static_cast<double>(count) / total;
                H -= p * std::log2(p);  // ← БИТЫ!
            }
        }
        
        // 3. Нормализация [0, 1]
        double max_entropy = std::log2(static_cast<double>(BINS));
        return std::clamp(H / max_entropy, 0.0, 1.0);
    }
    
    // Целевая энтропия (тоже в БИТАХ, нормализованная)
    double getTargetUnifiedEntropy() const {
        double prediction_error = predictive_coder ? predictive_coder->getLastError() : 0.5;
        double system_energy = computeTotalEnergy();
        
        // Базовая целевая энтропия (50% от максимума)
        double base_target = 0.5;
        
        // Модуляция ошибкой (высокая ошибка → выше энтропия)
        double error_factor = 1.0 + std::min(0.5, prediction_error);
        
        // Модуляция энергией (высокая энергия → ниже энтропия)
        double energy_factor = 1.0 / (1.0 + system_energy);
        
        // Режимная модуляция
        double mode_factor = 1.0;
        switch (current_mode_) {
            case OperatingMode::TRAINING: mode_factor = 1.3; break;
            case OperatingMode::NORMAL:   mode_factor = 1.0; break;
            case OperatingMode::IDLE:     mode_factor = 0.7; break;
            case OperatingMode::SLEEP:    mode_factor = 1.2; break;
        }
        
        double target = base_target * error_factor * energy_factor * mode_factor;
        return std::clamp(target, 0.2, 0.8);  // [20%..80%] от макс. энтропии
    }

    // ===============================================

    /** Возвращает вектор активностей всех нейронов */
    const std::vector<double>& getPhi() const { return flatPhi; }

    /** Возвращает вектор импульсов всех нейронов */
    const std::vector<double>& getPi() const { return flatPi; }

    /**
     * Для совместимости возвращает пустую матрицу
     */
    const std::vector<std::vector<double>> getWeights() const { return {}; }

    /** Получить средние активности групп */
    std::vector<double> getGroupAverages() const;

    /** Получить матрицу межгрупповых связей */
    const std::vector<std::vector<double>>& getInterWeights() const { return interWeights; }

    /** Укрепить связь от группы from к группе to */
    void strengthenInterConnection(int from, int to, double delta);

    /** Ослабить связь */
    void weakenInterConnection(int from, int to, double delta);

    /** Получить вектор признаков для памяти (64 значения) */
    std::vector<float> getFeatures() const;

    // Структура состояния рефлексии
    struct ReflectionState {
        double confidence, curiosity, satisfaction, confusion;
        std::vector<double> attention_map;
    };
    
    ReflectionState getReflectionState() const;
    void reflect();
    void setGoal(const std::string& goal);
    bool evaluateProgress();
    void learnFromReflection(float outcome);

    // Методы для эволюции
    void applyTargetedMutation(double strength, int targetType);
    void enterLowPowerMode();

    // удержания системы на границе хаоса
    void maintainCriticality();

    void applyRicciFlow();

    // Доступ к группам
    std::vector<NeuralGroup>& getGroupsNonConst() { return groups; }
    const std::vector<NeuralGroup>& getGroups() const { return groups; }

    // LOG
    void logOrbitalHealth();
    void setOperatingMode(OperatingMode::Type mode);

    // УДАЛЕНО: старые методы предсказателя
    // void initializePredictor(size_t input_dim, size_t latent_dim, std::mt19937& rng);
    // std::vector<double> predictNextState(const std::vector<double>& current_features);
    // double updatePredictor(...);
    // bool detectAnomaly(const std::vector<double>& features);
    // std::vector<double> getCompressedState(const std::vector<double>& features);
    // double getPredictionEntropy() const;
    
    bool isTrainingMode() const { return training_mode_; }
    void setTrainingMode(bool enabled) { training_mode_ = enabled; }
    
    void addExternalInput(const std::vector<float>& input) {
        external_inputs_ = input;
    }
    
    void clearExternalInputs() {
        external_inputs_.clear();
    }

    void applyLateralInhibition();
    void applyTargetPattern(const std::vector<float>& target_patter);

    void setMemoryManager(MemoryManager* mm) { memory_manager = mm; };


    // РЕАЛИЗАЦИЯ ИНТЕРФЕЙСА
    std::vector<NeuralGroup*>& getHubGroups() override {
        // Ленивое заполнение вектора указателей
        static std::vector<NeuralGroup*> hubPointers;
        hubPointers.clear();
        for (int idx : hubIndices) {
            if (idx >= 0 && idx < static_cast<int>(groups.size())) {
                hubPointers.push_back(&groups[idx]);
            }
        }
        return hubPointers;
    }
    
    const std::vector<NeuralGroup*>& getHubGroups() const override {
        static std::vector<NeuralGroup*> hubPointers;
        hubPointers.clear();
        for (int idx : hubIndices) {
            if (idx >= 0 && idx < static_cast<int>(groups.size())) {
                hubPointers.push_back(const_cast<NeuralGroup*>(&groups[idx]));  // const_cast здесь ОК, т.к. возвращаем const версию
            }
        }
        return hubPointers;
    }
    
    void registerHub(int groupIndex) override {
        if (groupIndex >= 0 && groupIndex < static_cast<int>(groups.size())) {
            hubIndices.push_back(groupIndex);
        }
    }

    // НОВЫЙ МЕТОД: инициализация предсказательного кодера
    void initializePredictiveCoder(MemoryManager& memory) {
        predictive_coder = std::make_unique<PredictiveCoder>(*this, memory);
    }
    // Временное решение для прямой активации семантических групп
    void setInputText(const std::vector<float>& input_signal) {
        // Группа 0 - входной буфер
        auto& group0 = groups[0].getPhiNonConst();
        for (int i = 0; i < 32; i++) {
            group0[i] = input_signal[i];
        }
        
        // Дополнительно: усиливаем связи от группы 0 к семантическим группам
        for (int g = 16; g <= 21; g++) {
            strengthenInterConnection(0, g, 0.1);  // временно усиливаем
        }
    }

    void lock() { system_mutex_.lock(); }
    void unlock() { system_mutex_.unlock(); }
    std::mutex& getMutex() { return system_mutex_; }
    
    // Или используйте RAII
    class ScopedLock {
        NeuralFieldSystem& system_;
    public:
        ScopedLock(NeuralFieldSystem& system) : system_(system) { system_.lock(); }
        ~ScopedLock() { system_.unlock(); }
    };

private:
    mutable std::mutex system_mutex_;  // добавить

    AttentionMechanism attention;
    EventSystem& events;
    OperatingMode::Type current_mode_ = OperatingMode::NORMAL;
    MemoryManager* memory_manager = nullptr;

    // НОВОЕ: вектор индексов групп-хабов
    std::vector<int> hubIndices;
    // НОВОЕ: добавить счетчик шагов
    int stepCounter = 0;  // или в конструкторе инициализировать
    
    double dt;
    std::vector<NeuralGroup> groups;                 // 32 группы
    std::vector<std::vector<double>> interWeights;   // межгрупповые связи 32x32

    FreeEnergyController free_energy_controller_;

    // Плоские векторы для обратной совместимости
    mutable std::vector<double> flatPhi;
    mutable std::vector<double> flatPi;
    mutable bool flatDirty;

    void rebuildFlatVectors() const;

    // Механизм повторного входа
    void applyReentry(int iterations = 3);

    // Вспомогательные методы
    std::deque<std::vector<double>> state_history;
    std::deque<double> energy_history;
    std::deque<double> entropy_history;              // история энтропии для анализа
    static constexpr int HISTORY_SIZE = 100;
    std::string current_goal;
    std::vector<double> goal_embedding;

    // Методы с энтропийной регуляризацией
    float computeGlobalImportance();                  // на основе изменения энтропии
    void applyPruningByElevation();                   
    void consolidateInterWeights();
    
    bool pendingEvolutionRequest_ = false;

    // НОВЫЕ ЧЛЕНЫ для определения режима
    std::vector<float> external_inputs_;  // внешние входы
    bool training_mode_ = false;           // флаг режима обучения

     // НОВОЕ: предсказательный кодер (вместо старого predictor)
    std::unique_ptr<PredictiveCoder> predictive_coder;

    // ИСПРАВЛЕНО: убран квалификатор NeuralFieldSystem::
    std::vector<double> getFeaturesDouble() const {
        auto float_features = getFeatures();
        return std::vector<double>(float_features.begin(), float_features.end());
    }
};
