#pragma once
#include <vector>
#include <random>
#include <string>
#include <deque>
#include <cmath>
#include <algorithm>
#include <iostream>
#include "Synapse.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
        limits.saturation_mass = 8.0;
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
    static constexpr double OUTER_ORBIT = 2.0;              // элита
    
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
    double max_mass = 5.0;                 // максимальная масса
    double min_mass = 0.5;                  // минимальная масса
    
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
    
    // Запрет копирования
    NeuralGroup(const NeuralGroup&) = delete;
    NeuralGroup& operator=(const NeuralGroup&) = delete;
    
    // Разрешаем перемещение
    NeuralGroup(NeuralGroup&&) = default;
    NeuralGroup& operator=(NeuralGroup&&) = default;

    // Параметры гомеостаза (публичные для доступа)
    HomeostasisParams homeo;
    double activation_temp = 0.8;
    
    // ===== ОСНОВНЫЕ МЕТОДЫ =====
    void evolve();                                      // эволюция группы
    void learnSTDP(float reward, int currentStep);     // STDP обучение
    void consolidate();                                 // консолидация памяти
    
    // ===== НОВЫЕ ОРБИТАЛЬНЫЕ МЕТОДЫ =====
    void updateMassByEnergy();
    double computeQuantumBarrier(int i) const;

    int getOrbitLevel(int i) const { return orbit_level[i]; }
    double getOrbitRadius(int level) const;
    double getNeuronEnergy(int i) const;

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

    // MASS
    bool hasMass() const { return true; }  // всегда true, т.к. масса есть у всех
    bool hasTemperature() const { return true; }  // всегда true
    
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

    //
    void syncSynapsesFromWeights();
    void syncWeightsFromSynapses();
    void computeGradients(const std::vector<double>& target);
    void applyGradients();
    void learnHebbian(double globalReward);

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
        if (i < 0 || i >= size) return 0.0;
        double memory = 0.0;
        for (int j = 0; j < size; j++) {
            if (i != j) {
                memory += std::abs(W_intra[i][j]);
            }
        }
        return memory / (size - 1);
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
        // Этот метод должен получать режим от NeuralFieldSystem
        // Пока возвращаем NORMAL как значение по умолчанию
        return OperatingMode::NORMAL;
    }
    
    const DynamicParams::SurvivalParams& getSurvivalParams(OperatingMode::Type mode) const {
        return DynamicParams::getSurvivalParams(mode);
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

private:
    MassLimits mass_limits;
    // ===== ФУНДАМЕНТАЛЬНЫЕ ПАРАМЕТРЫ =====
    int size;                       // количество нейронов
    double dt;                      // шаг времени
    double threshold;                // порог активации
    int step_counter_ = 0;
    
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
    void checkForSingularity(int i);             // проверка падения в сингулярность
    void resetNeuron(int i, std::mt19937& rng);  // перерождение нейрона
    void buildSynapsesFromWeights();             // синхронизация весов
    void updateCachedPhi() const;                // обновление кэша
    
    // Вспомогательные
    double getTangentialVelocity(int i) const;

    double getLocalStabilityImpl(int i) const {
        double time_factor = std::min(1.0, time_on_orbit[i] / 1000.0);
        double energy_stability = 1.0 / (1.0 + std::abs(orbital_energy[i] - target_radius[i]));
        return 0.7 * time_factor + 0.3 * energy_stability;
    }
    
    // Для градиентного спуска
    std::vector<std::vector<double>> weight_gradients;
    std::vector<std::vector<double>> gd_velocity;
    double gd_learning_rate = 0.001;
    double gd_momentum = 0.9;

    // методы баланса масс
    double computeMassCap(int neuron_idx) {
        double r = pos[neuron_idx].norm();
        double energy = getNeuronEnergy(neuron_idx);
        double memory = getMemoryStrength(neuron_idx);
        int orbit = orbit_level[neuron_idx];
        
        // ЕЩЕ БОЛЕЕ МЯГКИЕ ЛИМИТЫ
        
        // Увеличиваем фактор Шварцшильда еще больше
        double schwarzschild_limit = r / (mass_limits.schwarzschild_radius_factor * 5.0);
        
        double temperature = getNeuronTemperature(neuron_idx);
        // Еще меньше влияния температуры
        double thermodynamic_limit = mass_limits.max_temperature_mass * 
                                    (1.0 - temperature * 0.02);  // было 0.05
        
        double entropy = getLocalEntropy(neuron_idx);
        // Увеличиваем информационный лимит еще больше
        double info_limit = entropy * 5.0;  // было 2.0
        
        // Увеличиваем орбитальные лимиты
        double orbit_limits[] = {2.0, 3.0, 5.0, 7.0, 10.0};  // было {1.0, 2.0, 3.5, 5.0, 7.0}
        double orbit_limit = orbit_limits[orbit];
        
        // Увеличиваем saturation_mass
        double saturation = mass_limits.saturation_mass * 2.0;  // было 1.5
        
        double mass_cap = std::min({
            schwarzschild_limit,
            thermodynamic_limit,
            info_limit,
            orbit_limit,
            saturation
        });
        
        // Увеличиваем минимальную массу
        return std::max(mass_cap, homeo.min_mass);  // было homeo.min_mass * 0.8
    }
};