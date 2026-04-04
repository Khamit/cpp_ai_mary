#pragma once
#include <vector>
#include <complex>
#include <random>
#include <string>
#include <deque>
#include <cmath>
#include <algorithm>
#include <iostream>
#include "Synapse.hpp"
#include "DynamicParams.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "Component.hpp"
#include "core/MemoryManager.hpp"

class SemanticGraphDatabase;
enum class EmotionalTone;

// Структура 3D вектора
struct Vec3 {
    double x, y, z;
    
    Vec3() : x(0), y(0), z(0) {}
    Vec3(double x, double y, double z) : x(x), y(y), z(z) {}
    
    double norm() const { return std::sqrt(x*x + y*y + z*z); }
    double normSquared() const { return x*x + y*y + z*z; }
    
    Vec3 normalized() const { 
        double n = norm();
        if (n < 1e-12) return Vec3(0,0,0);
        return Vec3(x/n, y/n, z/n);
    }
    
    Vec3 operator-() const { return Vec3(-x, -y, -z); }
    
    Vec3 operator+(const Vec3& other) const { return Vec3(x + other.x, y + other.y, z + other.z); }
    Vec3 operator-(const Vec3& other) const { return Vec3(x - other.x, y - other.y, z - other.z); }
    Vec3 operator*(double s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator/(double s) const { return Vec3(x / s, y / s, z / s); }
    
    Vec3& operator+=(const Vec3& other) { x += other.x; y += other.y; z += other.z; return *this; }
    Vec3& operator-=(const Vec3& other) { x -= other.x; y -= other.y; z -= other.z; return *this; }
    Vec3& operator*=(double s) { x *= s; y *= s; z *= s; return *this; }
    Vec3& operator/=(double s) { x /= s; y /= s; z /= s; return *this; }
    
    static double dot(const Vec3& a, const Vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    
    static Vec3 cross(const Vec3& a, const Vec3& b) {
        return Vec3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        );
    }
    
    // Случайный вектор на единичной сфере
    static Vec3 randomOnSphere(std::mt19937& rng) {
        std::uniform_real_distribution<double> theta_dist(0, 2*M_PI);
        std::uniform_real_distribution<double> phi_dist(0, M_PI);
        
        double theta = theta_dist(rng);
        double phi = phi_dist(rng);
        
        return Vec3(
            std::sin(phi) * std::cos(theta),
            std::sin(phi) * std::sin(theta),
            std::cos(phi)
        );
    }
    
    // Случайный касательный вектор
    static Vec3 randomTangent(const Vec3& normal, std::mt19937& rng, double scale = 1.0) {
        std::normal_distribution<double> dist(0.0, scale);
        Vec3 noise(dist(rng), dist(rng), dist(rng));
        double dot = Vec3::dot(noise, normal);
        noise = noise - normal * dot;
        double norm = noise.norm();
        if (norm > 1e-6) {
            noise = noise / norm;
        }
        return noise;
    }
};

// ===== СНАЧАЛА ОПРЕДЕЛЯЕМ СТРУКТУРУ MassLimits =====
struct MassLimits {
    // КВАНТОВЫЕ ОГРАНИЧЕНИЯ
    double planck_mass = 1.0;           // базовая единица
    double schwarzschild_radius_factor = 2.0; // когда масса создает черную дыру
    
    // ТЕРМОДИНАМИЧЕСКИЕ ОГРАНИЧЕНИЯ
    double max_temperature_mass = 3.0;   // масса, при которой начинается "испарение"
    double evaporation_rate = 0.01;      // скорость "испарения" при перегреве
    
    // ИНФОРМАЦИОННЫЕ ОГРАНИЧЕНИЯ
    double info_capacity_factor = 0.1;   // сколько информации может нести единица массы
    double max_entropy_density = 1.0;    // максимальная энтропия на единицу массы
    
    // ЭНЕРГЕТИЧЕСКИЕ ОГРАНИЧЕНИЯ
    double binding_energy_factor = 0.3;  // энергия связи удерживает массу
    double saturation_mass = 4.0;        // масса насыщения
};

/*

*/
// Оптимальные значения для разных сценариев
// ===== ТЕПЕРЬ ОПРЕДЕЛЯЕМ СТРУКТУРУ С РЕКОМЕНДАЦИЯМИ =====
struct MassRecommendations {
    // Raspberry Pi 512MB (экономичный режим)
    static MassLimits forLowMemory() {
        MassLimits limits;
        limits.planck_mass = 1.0;
        limits.saturation_mass = 2.0;
        limits.max_temperature_mass = 1.8;
        limits.evaporation_rate = 0.05;
        limits.binding_energy_factor = 0.2;
        return limits;
    }
    
    // Raspberry Pi 1GB (сбалансированный режим)
    static MassLimits forMediumMemory() {
        MassLimits limits;
        limits.planck_mass = 1.0;
        limits.saturation_mass = 3.5;
        limits.max_temperature_mass = 3.0;
        limits.evaporation_rate = 0.03;
        limits.binding_energy_factor = 0.25;
        return limits;
    }
    
    // M1 / Desktop (производительный режим)
    static MassLimits forHighMemory() {
        MassLimits limits;
        limits.planck_mass = 1.0;
        limits.saturation_mass = 5.0;
        limits.max_temperature_mass = 4.5;
        limits.evaporation_rate = 0.01;
        limits.binding_energy_factor = 0.3;
        return limits;
    }
    
    // Сервер (очень мощный)
    static MassLimits forServer() {
        MassLimits limits;
        limits.planck_mass = 1.0;
        limits.saturation_mass = 12.0;
        limits.max_temperature_mass = 7.0;
        limits.evaporation_rate = 0.005;
        limits.binding_energy_factor = 0.4;
        return limits;
    }
    
    // ДИНАМИЧЕСКОЕ ОГРАНИЧЕНИЕ
    static MassLimits forRAM(size_t ram_mb, int total_neurons) {
        MassLimits limits;
        
        double bytes_per_mass_unit = 100.0;
        double max_total_mass = (ram_mb * 1024 * 1024 * 0.3) / bytes_per_mass_unit;
        double max_mass_per_neuron = max_total_mass / total_neurons;
        
        limits.saturation_mass = std::min(5.0, max_mass_per_neuron);
        limits.max_temperature_mass = limits.saturation_mass * 0.9;
        limits.evaporation_rate = 0.02 * (5.0 / limits.saturation_mass);
        
        return limits;
    }
};

// Параметры орбитальной структуры
struct OrbitalParams {
    // Радиусы орбит (в порядке возрастания)
    static constexpr double SINGULARITY_RADIUS = 0.01;      // черная дыра
    static constexpr double INNER_ORBIT = 0.6;             // кандидаты
    static constexpr double BASE_ORBIT = 1.0;              // рабочие
    static constexpr double MIDDLE_ORBIT = 1.5;            // менеджеры
    static constexpr double OUTER_ORBIT = 2.6;              // элита

    // НОВЫЙ ПАРАМЕТР - НЕ УБЕГАЙТЕ ДЖЕКИЧАНЫ!
    static constexpr double ESCAPE_VELOCITY = 3.0;  // скорость убегания
    
    // Энергетические пороги
    static constexpr double ESCAPE_ENERGY = 2.7;            // энергия для повышения
    static constexpr double FALL_ENERGY = 0.8;              // энергия для понижения
    static constexpr double COLLAPSE_ENERGY = 0.07;          // энергия для обнуления
    
    // Стабильность
    static constexpr double HIGH_STABILITY = 0.8;           // высокая стабильность
    static constexpr double LOW_STABILITY = 0.3;            // низкая стабильность
    
    // Конкуренция
    static constexpr double COMPETITION_DISTANCE = 0.3;     // дистанция конкуренции
    static constexpr double COMPETITION_STRENGTH = 1.0;     // сила конкуренции
    
    // Орбитальные силы
    static constexpr double ORBITAL_WELL_DEPTH = 4.0;       // глубина потенциальной ямы
    static constexpr double ORBITAL_WELL_WIDTH = 1.0;       // ширина потенциальной ямы

    // НОВЫЕ ПАРАМЕТРЫ ДЛЯ ВЫБРОСА ИЗ СИНГУЛЯРНОСТИ
    static constexpr double EJECTION_MIN_ORBIT = 2;      // минимальная орбита для выброса
    static constexpr double EJECTION_MAX_ORBIT = 3;      // максимальная орбита для выброса
    static constexpr double EJECTION_FORCE_FACTOR = 1.2; // множитель силы выброса
    static constexpr double EJECTION_RANDOMNESS = 0.3;   // случайность в направлении
    
};

struct KnowledgeRelay {
    int source_neuron;        // от кого учимся
    int target_neuron;        // кто учится
    double confidence;        // уверенность источника
    double timestamp;         // время обучения
};

/**
 * @struct HomeostasisParams
 * @brief Параметры гомеостатической регуляции энтропии
 */
struct HomeostasisParams {
    // Целевые значения
    double target_entropy = 2.0;           // желаемая энтропия
    double entropy_tolerance = 0.5;         // допустимое отклонение
    double adaptation_rate = 0.05;         // скорость адаптации
    
    // Регулируемые параметры
    double base_noise_level = 0.05;          // базовый уровень шума
    double min_damping = 0.5;                 // мин. затухание
    double max_damping = 0.99;                // макс. затухание
    double min_temperature = 0.5;              // мин. температура активации
    double max_temperature = 2.0;              // макс. температура активации
    
    // Параметры для всплесков (bursts)
    double burst_threshold = 2.3;              // порог для генерации всплеска
    double burst_strength = 1.5;                // сила всплеска
    double burst_decay = 0.95;                   // затухание всплеска
    
    // История для анализа
    int history_size = 100;                      // размер истории энтропии

    // НОВЫЕ ПАРАМЕТРЫ ДЛЯ КВАНТОВОЙ СТАБИЛИЗАЦИИ
    double mass_energy_factor = 0.3;      // как сильно масса зависит от энергии
    double mass_orbit_factor = 0.5;       // как сильно масса зависит от орбиты
    double min_mass = 0.3;                  // минимальная масса
    double max_mass = 12.0;                 // максимальная масса
    
    // Потенциальный барьер
    double barrier_height = 2.0;            // высота барьера на границах орбит
    double barrier_width = 0.2;              // ширина барьера

    // НОВЫЕ ПАРАМЕТРЫ ДЛЯ ПАМЯТИ
    double mass_memory_factor = 0.5;      // как сильно память влияет на массу
    double memory_decay = 0.999;           // затухание "старой" памяти
    double max_memory_strength = 10.0;     // максимальная сила памяти
};

/**
 * @class NeuralGroup
 * @brief Функциональная группа нейронов с орбитальной конкуренцией в 3D пространстве
 * 
 * Ключевая идея: нейроны конкурируют за орбиты (энергетические уровни).
 * Высокие орбиты = стабильные, важные концепты.
 * Низкие орбиты = временные, экспериментальные.
 * Сингулярность = смерть и перерождение.
 */
class NeuralGroup {
public:
    NeuralGroup(int size, double dt, std::mt19937& rng, const MassLimits& limits = MassLimits());
    std::vector<std::string> neuron_specialization;
    // Запрет копирования
    NeuralGroup(const NeuralGroup&) = delete;
    NeuralGroup& operator=(const NeuralGroup&) = delete;
    
    // Разрешаем перемещение
    NeuralGroup(NeuralGroup&&) = default;
    NeuralGroup& operator=(NeuralGroup&&) = default;
    void setMemoryManager(MemoryManager* mm) { memory_manager = mm; }
    // Параметры гомеостаза (публичные для доступа)
    HomeostasisParams homeo;
    double activation_temp = 0.8;
    double computeLocalEntropy(int i) const;

    int getOrbitCapacity(int level) const {
        switch(level) {
            case 4: return std::max(1, (int)(size * 0.05));  // минимум 1
            case 3: return std::max(1, (int)(size * 0.15));
            case 2: return std::max(1, (int)(size * 0.30));
            case 1: return std::max(1, (int)(size * 0.30));
            default: return std::max(1, (int)(size * 0.20));
        }
    }

    // ===== НОВЫЕ ПОЛЯ ДЛЯ МЕТОДА ТЬЮРИНГА =====
    std::vector<double> inhibitor;          // Ингибиторное поле (второй компонент)
    double diffusion_strength = 0.02;       // Коэффициент диффузии (D)
    double inhibitor_decay = 0.995;         // Затухание ингибитора
    double inhibitor_influence = 0.5;       // Влияние ингибитора на активность
    double inhibitor_production = 0.05;     // Скорость производства ингибитора активностью

    const std::vector<double>& getInhibitor() const { return inhibitor; }
    
    // ===== ОСНОВНЫЕ МЕТОДЫ =====
    void evolve();                                      // эволюция группы
    void learnSTDP(float reward, int currentStep);     // STDP обучение
    void consolidate();                                 // консолидация памяти
    
    void saveBestPatternToMemory(float reward);
    
    // ===== НОВЫЕ ОРБИТАЛЬНЫЕ МЕТОДЫ =====
    void updateMassByEnergy();
    double computeQuantumBarrier(int i) const;

    int getOrbitLevel(int i) const { return orbit_level[i]; }
    double getOrbitRadius(int level) const;
    double getNeuronEnergy(int i) const;

    // Новый метод:
    void relayLearning(int learner_idx, float reward, int step);
    void propagateKnowledgeFromHigherOrbits();
    void promoteNovelPatternsFromLowerOrbits();
    double computeNovelty(int i) const;  // вычисление новизны нейрона
    int selectBestNovelPattern(const std::vector<int>& novel_neurons);  // выбор лучшего паттерна
    void applyEliteCoordination();
    void eliteCommandLowerOrbits();
    void performExperiments();

    // Новые методы для вычисления компонентов важности
    double computeConnectivity(int i) const;    // связность нейрона i
    double computeUniqueness(int i) const;      // уникальность
    double computeHomeostaticValue() const;     // гомеостатическая ценность всей группы
    double computeAdaptability(int i) const;    // адаптивность

    // Вес и энергия
    const std::vector<std::vector<double>>& getWeights() const { return W_intra; }
    const std::vector<double>& getOrbitalEnergy() const { return orbital_energy; }

    // Получить массу нейрона
    double getMass(int i) const { 
        if (i < 0 || i >= size) return 1.0;
        return mass[i]; 
    }

    // Получить время на орбите
    double getTimeOnOrbit(int i) const {
        if (i < 0 || i >= size) return 0.0;
        return time_on_orbit[i];
    }
    double getNeuronStability(int i) const;
    bool isInOrbit(int i, int level) const;
    std::vector<int> getNeuronsByOrbit(int level) const;
    double computeOrbitalHealth() const;                // распределение по орбитам
    
    // ===== ГОМЕОСТАЗ =====
    void updateHomeostasis();
    void generateBurst();
    double getAdaptiveNoiseLevel() const;
    
    // ===== ГЕОМЕТРИЯ =====
    void updateMetricTensor();
    double computeEntropy() const;
    double scalarCurvature() const;
    double computeEntropyTarget() const;
    
    // ===== АКТИВНОСТЬ (для обратной совместимости) =====
    double getAverageActivity() const;
    const std::vector<double>& getPhi() const;
    std::vector<double>& getPhiNonConst();
    const std::vector<double>& getPi() const;
    std::vector<double>& getPiNonConst();
    
    // ===== 3D ГЕТТЕРЫ =====
    const std::vector<Vec3>& getPositions() const { return pos; }
    const std::vector<Vec3>& getVelocities() const { return vel; }
    
    // ===== СИНАПСЫ =====
    std::vector<Synapse>& getSynapses() { return synapses; }
    const std::vector<Synapse>& getSynapsesConst() const { return synapses; }
    
    // ===== ПАРАМЕТРЫ =====
    void setLearningRate(double lr) { params.hebbianRate = lr; }
    double getLearningRate() const { return params.hebbianRate; }
    void setThreshold(double thr) { threshold = thr; }
    double getThreshold() const { return threshold; }
    double getEffectiveThreshold() const { return threshold - elevation_ * 0.05; }
    
    // ===== ВЫСОТА (ELEVATION) =====
    void setElevation(float elev) { elevation_ = elev; }
    float getElevation() const { return elevation_; }
    void updateElevationFast(float reward, float activity);
    void consolidateElevation(float globalImportance, float hallucinationPenalty = 0.0f);
    void consolidateEligibility(float globalImportance);

    // Глобальные Штрафы 
    void applyGlobalPenalty(float penalty) {
        // Штраф уменьшает элевацию (важность) нейронов
        elevation_ += penalty * 0.01f;
        elevation_ = std::clamp(elevation_, -1.0f, 1.0f);
    }

    void publicPromoteToEliteOrbit(int i) {
        if (i >= 0 && i < size) {
            promoteToEliteOrbit(i);
        }
    }

    // ===== Entropy ==============

    // MASS
    bool hasMass() const { return true; }  // всегда true, т.к. масса есть у всех
    bool hasTemperature() const { return true; }  // всегда true
        // НОВЫЙ МЕТОД: установить массу нейрона
    void setMass(int i, double new_mass) {
        if (i >= 0 && i < size) {
            mass[i] = std::clamp(new_mass, homeo.min_mass, homeo.max_mass);
        }
    }
    
    // ===== ВЕСА =====
    void setWeight(int i, int j, double weight) {
        if (i >= 0 && i < size && j >= 0 && j < size) {
            W_intra[i][j] = weight;
        }
    }
    double getWeight(int i, int j) const {
        if (i >= 0 && i < size && j >= 0 && j < size) {
            return W_intra[i][j];
        }
        return 0.0;
    }
    void decayAllWeights(float factor);

    // НОВЫЕ МЕТОДЫ ДЛЯ СИНГУЛЯРНОСТИ
    void ejectFromSingularity(int i);
    
    // Для диагностики
    bool isInSingularity(int i) const {
        if (i < 0 || i >= size) return false;
        return pos[i].norm() < OrbitalParams::SINGULARITY_RADIUS * 2.0;
    }
    
    // Получить статистику по выбросам
    int getEjectionCount() const { return ejection_count_; }
    
    // ===== ВСПОМОГАТЕЛЬНОЕ =====
    int getSize() const { return size; }
    int getLastBurstStep() const { return last_burst_step; }
    std::string specialization;

    void decayMemory();

    double getLocalStability(int i) const { 
        if (i < 0 || i >= size) return 0.0;
        return getLocalStabilityImpl(i); 
    }

    // сброс избыточной массы
    void activateMassShedding(int i);

    // Synapses
    void syncSynapsesFromWeights();
    void syncWeightsFromSynapses();
    void learnHebbian(double globalReward);

    // НОВЫЙ МЕТОД
    void updateWaveFunction();
        // Нейроны при ЦПУ
    bool checkForResourceExhaustion();

    // ===== ЕДИНАЯ энтропия для всей системы (в БИТАХ) ======
    // Исправленный метод — возвращает энтропию в БИТАХ!
    double computeUnifiedEntropy() const {
        const int BINS = 16;
        std::vector<int> hist_azimuth(BINS, 0);
        std::vector<int> hist_polar(BINS, 0);
        
        for (int i = 0; i < size; ++i) {
            double azimuth = std::atan2(pos[i].y, pos[i].x);
            double polar = std::acos(pos[i].z / (pos[i].norm() + 1e-12));
            
            int bin_az = static_cast<int>((azimuth + M_PI) / (2*M_PI) * BINS) % BINS;
            int bin_pol = static_cast<int>(polar / M_PI * BINS) % BINS;
            
            hist_azimuth[bin_az]++;
            hist_polar[bin_pol]++;
        }
        
        double H = 0.0;
        double max_entropy = std::log2(static_cast<double>(BINS));
        
        for (int count : hist_azimuth) {
            if (count > 0) {
                double p = static_cast<double>(count) / size;
                H -= p * std::log2(p);  // ← БИТЫ!
            }
        }
        for (int count : hist_polar) {
            if (count > 0) {
                double p = static_cast<double>(count) / size;
                H -= p * std::log2(p);  // ← БИТЫ!
            }
        }
        
        // Нормализация и усреднение
        return std::clamp((H / max_entropy) / 2.0, 0.0, 1.0);
    }

    // Исправленный computeOptimalEntropy — тоже в БИТАХ
    double computeOptimalEntropyBits() const {
        double avg_energy = getAverageEnergy();
        double temperature = activation_temp;
        
        if (temperature < 0.01) return 0.5;
        
        double Z = 0.0;
        std::vector<double> probabilities(size);
        
        for (int i = 0; i < size; ++i) {
            double energy = getNeuronEnergy(i);
            double p = std::exp(-energy / temperature);
            probabilities[i] = p;
            Z += p;
        }
        
        if (Z < 1e-12) return 0.5;
        
        // Энтропия Шеннона в БИТАХ
        double H = 0.0;
        for (double p : probabilities) {
            p /= Z;
            if (p > 1e-12) {
                H -= p * std::log2(p);  // ← БИТЫ!
            }
        }
        
        // Максимальная энтропия для size состояний = log2(size)
        double max_H = std::log2(static_cast<double>(size));
        return std::clamp(H / max_H, 0.0, 1.0);
    }

    // ===== МЕТОДЫ ДЛЯ ВИЗУАЛИЗАЦИИ =====
    double getNeuronTemperature(int i) const {
        if (i < 0 || i >= size) return 0.0;
        double kinetic = getKineticEnergy(i);
        double potential = getPotentialEnergy(i);
        return (kinetic + potential * 0.5) * (1.0 + pos[i].norm());
    }
    
    double getLocalEntropy(int i) const {
        if (i < 0 || i >= size) return 0.0;
        double r = pos[i].norm();
        double entropy = 0.0;
        
        for (int j = 0; j < size; j++) {
            if (i != j) {
                double w = std::abs(W_intra[i][j]);
                if (w > 0.01) {
                    entropy -= w * std::log(w + 1e-10);
                }
            }
        }
        entropy += std::log(r + 1.0) * 0.1;
        return entropy;
    }
    
    double getMemoryStrength(int i) const {
        double memory = 0.0;
        for (int j = 0; j < size; j++) {
            if (i != j) {
                memory += std::abs(W_intra[i][j]);
            }
        }
        // УБРАТЬ ДЕЛЕНИЕ или уменьшить знаменатель
        return memory / (size / 4);  // было size - 1
    }

    std::complex<double> getWaveFunction(int i) const { 
    if (i < 0 || i >= size) return std::complex<double>(0,0);
    return wave_function[i]; 
    }

    void updateWaveAmplitudeFromState() {
    for (int i = 0; i < size; i++) {
            double activity = pos[i].norm();
            double activity_factor = std::min(1.0, activity / OrbitalParams::BASE_ORBIT);
            double orbit_factor = 0.5 + orbit_level[i] * 0.25;
            double stability = getLocalStability(i);
            
            double new_amplitude = 0.5 + activity_factor * 0.8 + orbit_factor * 0.3 + stability * 0.2;
            wave_amplitude[i] = std::clamp(new_amplitude, 0.2, 2.5);
            wave_function[i] = std::polar(wave_amplitude[i], wave_phase[i]);
        }
    }

    int getSynapseIndex(int i, int j) const {
        if (i == j) return -1;
        if (i > j) std::swap(i, j);
        // Индекс в линейном массиве synapses для пары (i, j)
        return i * size - (i * (i + 1)) / 2 + (j - i - 1);
    }

    double getWavePhase(int i) const { 
        if (i < 0 || i >= size) return 0.0;
        return wave_phase[i]; 
    }
    
    // ВАЖНО: ЭТИ МЕТОДЫ ОПРЕДЕЛЕНЫ ТОЛЬКО ЗДЕСЬ!
    double getKineticEnergy(int i) const { 
        if (i < 0 || i >= size) return 0.0;
        return 0.5 * mass[i] * vel[i].normSquared(); 
    }
    
    double getPotentialEnergy(int i) const { 
        if (i < 0 || i >= size) return 0.0;
        double r = pos[i].norm();
        double r_target = target_radius[i];
        double r_error = r - r_target;
        return OrbitalParams::ORBITAL_WELL_DEPTH * (1.0 - std::exp(-r_error * r_error));
    }
    // LOG
    void logMassDistribution();

    // NEw
    void generateGentleBurst();
    void maintainActivity(OperatingMode::Type mode);

    // new
    void pruneConnectionsByOrbit();
    void generateCuriosityConnections();
    void protectCoreConcepts();

        // НОВЫЕ ПУБЛИЧНЫЕ МЕТОДЫ для доступа извне
    void publicResetNeuron(int i, std::mt19937& rng) {
        if (i >= 0 && i < size) {
            resetNeuron(i, rng);
        }
    }
    
    void publicPromoteToBaseOrbit(int i) {
        if (i >= 0 && i < size) {
            promoteToBaseOrbit(i);
        }
    }
    
    OperatingMode::Type getCurrentMode() const {
        // Этот метод должен быть связан с NeuralFieldSystem
        // Пока оставляем заглушку, но в реальности нужно передавать
        return current_mode_;  // добавить поле
    }
    
    const DynamicParams::SurvivalParams& getSurvivalParams(OperatingMode::Type mode) const {
        return DynamicParams::getSurvivalParams(mode);
    }

    // Добавить поле и метод установки
    void setCurrentMode(OperatingMode::Type mode) {
        current_mode_ = mode;
    }

    
    
    // Добавить метод promoteToBaseOrbit если его нет
    void promoteToBaseOrbit(int i) {
        if (i < 0 || i >= size) return;
        
        orbit_level[i] = 2;  // базовая орбита
        target_radius[i] = OrbitalParams::BASE_ORBIT;
        
        // Помещаем на базовую орбиту с небольшим смещением
        static std::mt19937 rng(std::random_device{}());
        double start_radius = OrbitalParams::BASE_ORBIT * (0.9 + 0.2 * (rng() % 100 / 100.0));
        pos[i] = Vec3::randomOnSphere(rng) * start_radius;
        vel[i] = Vec3(0,0,0);
        
        orbital_energy[i] = 1.0;  // даем базовую энергию
        time_on_orbit[i] = 0;
        
        std::cout << "  Neuron " << i << " promoted to base orbit" << std::endl;
    }

     // Запись использования связи
    void recordConnectionUsage(int i, int j, bool success = false) {
        if (i < 0 || i >= size || j < 0 || j >= size) return;
        if (i == j) return;
        
        connection_usage_count[i][j]++;
        connection_usage_count[j][i]++;
        connection_last_used_step[i][j] = step_counter_;
        connection_last_used_step[j][i] = step_counter_;
        
        if (success) {
            connection_success_count[i][j]++;
            connection_success_count[j][i]++;
        }
    }
    
    // Получить эффективность связи
    float getConnectionEfficiency(int i, int j) const {
        if (i < 0 || i >= size || j < 0 || j >= size) return 0.5f;
        int total = connection_usage_count[i][j];
        if (total == 0) return 0.5f;
        return static_cast<float>(connection_success_count[i][j]) / total;
    }

    // ENTROPY
        // ===== НОВЫЕ МЕТОДЫ ДЛЯ УПРАВЛЕНИЯ КОНСОЛИДАЦИЕЙ =====
    void setConsolidationRate(float rate) {
        params.consolidationRate = rate;
    }
    
    float getConsolidationRate() const {
        return params.consolidationRate;
    }
    
    // ===== НОВЫЙ МЕТОД ДЛЯ ПОЛУЧЕНИЯ СРЕДНЕЙ ЭНЕРГИИ =====
    double getAverageEnergy() const {
        double total_energy = 0.0;
        for (int i = 0; i < size; ++i) {
            total_energy += getNeuronEnergy(i);
        }
        return total_energy / size;
    }
    
    // ===== УПРАВЛЕНИЕ СКОРОСТЬЮ STDP =====
    void setStdpRate(float rate) {
        params.stdpRate = rate;
    }
    
    float getStdpRate() const {
        return params.stdpRate;
    }
    
    // ===== УПРАВЛЕНИЕ ТЕМПЕРАТУРОЙ =====
    void setTemperature(float temp) {
        activation_temp = temp;
    }
    
    float getTemperature() const {
        return activation_temp;
    }
    
    // ===== АЛИАСЫ ДЛЯ ЭНТРОПИИ =====
    double getCurrentEntropy() const {
        return computeEntropy();
    }
    
    double getTargetEntropy() const {
        return computeEntropyTarget();
    }

private:
    MassLimits mass_limits;
    OperatingMode::Type current_mode_ = OperatingMode::NORMAL;
    MemoryManager* memory_manager = nullptr;

    // ===== ФУНДАМЕНТАЛЬНЫЕ ПАРАМЕТРЫ =====
    int size;                       // количество нейронов
    double dt;                      // шаг времени
    double threshold;                // порог активации
    int step_counter_ = 0;
    // счетчик выбросов
    int ejection_count_ = 0;
    // Счетчик связей 
    // Счетчики использования связей
    std::vector<std::vector<int>> connection_usage_count;
    std::vector<std::vector<int>> connection_success_count;

    // ===== НОВЫЕ ПЕРЕМЕННЫЕ ДЛЯ БАЛАНСИРОВКИ ЭНТРОПИИ =====
    double promotion_threshold_ = 0.6;
    double demotion_threshold_ = 0.4;
    
    // Время последнего использования
    std::vector<std::vector<int>> connection_last_used_step;

    mutable double cached_curvature_ = 0.0;
    mutable int curvature_step_ = -1;

    // ВОЛНОВАЯ ФУНКЦИЯ
    std::vector<std::complex<double>> wave_function;  // волновая функция ψ
    std::vector<double> wave_phase;                   // фаза волны
    std::vector<double> wave_amplitude;               // амплитуда волны
    double wave_frequency = 0.1;                       // базовая частота
    double wave_damping = 0.999;                        // затухание волны
    
    // ===== ОРБИТАЛЬНАЯ СТРУКТУРА =====
    std::vector<Vec3> pos;           // позиция в 3D
    std::vector<Vec3> vel;           // скорость в 3D
    std::vector<Vec3> force;         // сила в 3D
    std::vector<double> mass;        // масса (инерция)
    std::vector<double> damping;      // затухание
    
    std::vector<int> orbit_level;     // текущий орбитальный уровень (0-4)
    std::vector<double> target_radius; // целевой радиус для каждого нейрона
    std::vector<double> orbital_energy; // энергия орбиты
    std::vector<double> time_on_orbit;  // время на текущей орбите
    std::vector<int> promotion_count;   // сколько раз повышался
    // Для перебалансировки понадобятся вспомогательные методы
    void rebalanceOrbits();   // вызовется периодически
    
    // ===== СВЯЗИ =====
    std::vector<std::vector<double>> W_intra;      // внутригрупповые веса
    std::vector<Synapse> synapses;                  // синапсы для STDP
    PlasticityParams params;                         // параметры пластичности
    
    // ===== МЕТРИКА =====
    std::vector<std::vector<double>> metric_tensor; // метрический тензор
    std::vector<int> rebirth_count;
    
    // ===== ГОМЕОСТАЗ =====
    std::deque<double> entropy_history;              // история энтропии
    double burst_potential = 0.0;
    int last_burst_step = 0;
    static constexpr int BURST_COOLDOWN = 50;
    
    // ===== ВЫСОТА (ELEVATION) =====
    float elevation_ = 0.0f;
    float elevation_learning_rate_ = 0.001f;
    float elevation_decay_ = 0.999f;
    float cumulative_importance_ = 0.0f;
    int activity_counter_ = 0;
    
    // ===== КЭШИ =====
    mutable std::vector<double> phi_cached;
    mutable std::vector<double> pi_cached;
    
    // ===== ПРИВАТНЫЕ МЕТОДЫ =====
    void computeDerivatives();                   // вычисление сил
    void applyOrbitalForces();                   // орбитальные силы
    void applyCompetitionForces();               // конкуренция на орбитах
    void updateOrbitLevels();                    // обновление уровней
    bool checkForSingularity(int i);             // проверка падения в сингулярность
    void resetNeuron(int i, std::mt19937& rng);  // перерождение нейрона
    void buildSynapsesFromWeights();             // синхронизация весов
    void updateCachedPhi() const;                // обновление кэша

    // НОВЫЙ МЕТОД: проверка на побег за пределы
    void checkForEscape(int i, std::mt19937& rng);
    
    // Вспомогательные
    double getTangentialVelocity(int i) const;

    double getLocalStabilityImpl(int i) const {
        double time_factor = std::min(1.0, time_on_orbit[i] / 1000.0);
        double energy_stability = 1.0 / (1.0 + std::abs(orbital_energy[i] - target_radius[i]));
        return 0.7 * time_factor + 0.3 * energy_stability;
    }

    // методы баланса масс
    double computeMassCap(int neuron_idx) {
        double r = pos[neuron_idx].norm();
        double energy = getNeuronEnergy(neuron_idx);
        double memory = getMemoryStrength(neuron_idx);
        int orbit = orbit_level[neuron_idx];
        
        // Квантовые ограничения
        // double schwarzschild_limit = r / (mass_limits.schwarzschild_radius_factor * 6.0);
        
        // Термодинамические ограничения
        double temperature = getNeuronTemperature(neuron_idx);
        double thermodynamic_limit = mass_limits.max_temperature_mass * 5.0;
        
        // Информационные ограничения
        double entropy = getLocalEntropy(neuron_idx);
        double info_limit = entropy * 20.0;
        
        // Орбитальные лимиты
        double orbit_limits[] = {5.0, 8.0, 12.0, 15.0, 20.0};
        double orbit_limit = orbit_limits[orbit];
        
        // Масса насыщения
        double saturation = mass_limits.saturation_mass;
        
        // Минимум из орбитального лимита и насыщения
        double mass_cap = std::min(orbit_limit, saturation);
        
        return std::max(mass_cap, homeo.min_mass);
    }
    
    void promoteToEliteOrbit(int i) {
        if (i < 0 || i >= size) return;
        
        int target_level = 4;
        orbit_level[i] = target_level;
        target_radius[i] = OrbitalParams::OUTER_ORBIT;
        
        // Помещаем на элитную орбиту
        static std::mt19937 rng(std::random_device{}());
        double start_radius = OrbitalParams::OUTER_ORBIT * (0.9 + 0.2 * (rng() % 100 / 100.0));
        pos[i] = Vec3::randomOnSphere(rng) * start_radius;
        vel[i] = Vec3(0, 0, 0);
        
        orbital_energy[i] = 3.0;  // высокая энергия для элиты
        time_on_orbit[i] = 0;
        mass[i] = std::min(mass[i] * 1.5, homeo.max_mass);
        
        std::cout << "  Neuron " << i << " promoted to ELITE orbit (level 4)" << std::endl;
    }
};