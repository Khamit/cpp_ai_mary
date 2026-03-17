#pragma once
#include <vector>
#include <random>
#include <string>
#include <deque>
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
    
    // Унарный минус (отрицание) - НУЖНО ДОБАВИТЬ
    Vec3 operator-() const {
        return Vec3(-x, -y, -z);
    }
    
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
};

/**
 * @class NeuralGroup
 * @brief Функциональная группа нейронов с энтропийной регуляризацией в 3D пространстве
 */
class NeuralGroup {
public:
    NeuralGroup(int size, double dt, std::mt19937& rng);

    void computeGradients(const std::vector<double>& target);
    void applyGradients();

    static constexpr int ENTROPY_BINS = 10;
    
    // Запрет копирования
    NeuralGroup(const NeuralGroup&) = delete;
    NeuralGroup& operator=(const NeuralGroup&) = delete;
    
    // Разрешаем перемещение
    NeuralGroup(NeuralGroup&&) = default;
    NeuralGroup& operator=(NeuralGroup&&) = default;
    
    // Основные методы
    void evolve();
    void learnSTDP(float reward, int currentStep);
    void learnHebbian(double globalReward);
    void consolidate();
    
    // Геттеры/сеттеры для обратной совместимости (конвертируют 3D в 1D)
    double getAverageActivity() const;
    const std::vector<double>& getPhi() const;
    std::vector<double>& getPhiNonConst();
    const std::vector<double>& getPi() const;
    std::vector<double>& getPiNonConst();
    std::vector<double>& getNeuronVelocity();
    const std::vector<double>& getNeuronVelocity() const;
    
    // Новые 3D геттеры
    const std::vector<Vec3>& getPositions() const { return pos; }
    const std::vector<Vec3>& getVelocities() const { return vel; }
    const std::vector<Vec3>& getForces() const { return force; }
    
    void setLearningRate(double lr) { params.hebbianRate = lr; }
    double getLearningRate() const { return params.hebbianRate; }
    
    void setThreshold(double thr) { threshold = thr; }
    double getThreshold() const { return threshold; }
    
    std::string specialization;

    // Доступ к синапсам
    std::vector<Synapse>& getSynapses() { return synapses; }
    const std::vector<Synapse>& getSynapsesConst() const { return synapses; }
    std::vector<Synapse>& getSynapsesNonConst() { return synapses; }
    
    // Методы для работы с высотой
    void setElevation(float elev) { elevation_ = elev; }
    float getElevation() const { return elevation_; }
    
    void updateElevationFast(float reward, float activity);
    void consolidateElevation(float globalImportance, float hallucinationPenalty = 0.0f);
    void consolidateEligibility(float globalImportance);
    
    // Для совместимости
    void updateEligibilityTraces(float reward, int currentStep) {
        learnSTDP(reward, currentStep);
    }
    
    double getEffectiveThreshold() const {
        return threshold - elevation_ * 0.05; 
    }

    void decayAllWeights(float factor);
    void updateMetricTensor();
    double computeEntropy() const;
    double scalarCurvature() const;

    // НОВЫЕ МЕТОДЫ ДЛЯ ГОМЕОСТАЗА
    void updateHomeostasis();                    // главный метод гомеостаза
    void adaptParameters(double entropy_error);  // адаптация параметров
    void generateBurst();                         // генерация всплесков активности
    double getAdaptiveNoiseLevel() const;         // получение уровня шума
    
    // Доступ к параметрам гомеостаза
    HomeostasisParams& getHomeoParams() { return homeo; }
    const HomeostasisParams& getHomeoParams() const { return homeo; }
    
    // НОВЫЕ МЕТОДЫ ДЛЯ ДОСТУПА К ВНУТРЕННИМ ДАННЫМ
    int getSize() const { return size; }
    
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
    
    void syncSynapsesFromWeights() {
        int idx = 0;
        for (int i = 0; i < size; ++i) {
            for (int j = i + 1; j < size; ++j) {
                if (idx < synapses.size()) {
                    synapses[idx].weight = static_cast<float>(W_intra[i][j]);
                    idx++;
                }
            }
        }
    }
    
    void syncWeightsFromSynapses() {
        int idx = 0;
        for (int i = 0; i < size; ++i) {
            for (int j = i + 1; j < size; ++j) {
                if (idx < synapses.size()) {
                    W_intra[i][j] = synapses[idx].weight;
                    W_intra[j][i] = synapses[idx].weight;
                    idx++;
                }
            }
        }
    }

    void setLearningThreshold(float thr) { learning_threshold_ = thr; }
    float getLearningThreshold() const { return learning_threshold_; }

    double computeEntropyTarget() const;

    // Методы для орбитальной визуализации (теперь работают с 3D)
    std::vector<double> getOrbitalPositions() const {
        std::vector<double> positions(size);
        for (int i = 0; i < size; ++i) {
            positions[i] = pos[i].norm(); // расстояние от центра
        }
        return positions;
    }

    double getDistanceToSingularity(int i) const {
        return pos[i].norm();
    }

    bool isInOrbitalZone(int i) const {
        double dist = getDistanceToSingularity(i);
        double inner = orbital_radius_ - orbital_width_/2;
        double outer = orbital_radius_ + orbital_width_/2;
        return (dist >= inner && dist <= outer);
    }
    
    void updateMass();

    // НОВОЕ: доступ к температуре активации
    double getActivationTemperature() const { return activation_temp; }
    void setActivationTemperature(double temp) { 
        activation_temp = std::clamp(temp, homeo.min_temperature, homeo.max_temperature); 
    }

    int getLastBurstStep() const { return last_burst_step; }

private:
    // Gradient Descent learning (оставляем для обратной совместимости)
    std::vector<std::vector<double>> weight_gradients;
    std::vector<std::vector<double>> gd_velocity;  
    
    double gd_learning_rate = 0.001;
    double gd_momentum = 0.9;

    // Параметры орбитальной модели (теперь в 3D)
    double singularity_strength_ = 1.0;
    double orbital_radius_ = 1.0;        // радиус сферы
    double orbital_width_ = 0.3;          // ширина орбитальной зоны
    std::vector<double> orbital_phi;      // больше не используется, оставлено для совместимости

    // Поля для массы и инерции
    std::vector<double> mass;
    std::vector<double> damping;
    double mass_learning_rate = 0.01;

    int size;
    double dt;
    double threshold;
    float learning_threshold_ = 0.3f;
    int step_counter_ = 0;
    
    // Матрица весов внутри группы
    std::vector<std::vector<double>> W_intra;
    
    // 3D состояние
    std::vector<Vec3> pos;   // позиция в 3D
    std::vector<Vec3> vel;   // скорость в 3D
    std::vector<Vec3> force; // сила в 3D
    
    // Для обратной совместимости - кэшированные 1D представления
    mutable std::vector<double> phi_cached;
    mutable std::vector<double> pi_cached;
    mutable std::vector<double> vel_cached;
    
    // Синапсы и параметры пластичности
    std::vector<Synapse> synapses;
    PlasticityParams params;

    // Метрика геометрии активности (расстояния между нейронами)
    std::vector<std::vector<double>> metric_tensor;
    
    // Параметры гомеостаза
    HomeostasisParams homeo;
    std::deque<double> entropy_history;        // история энтропии
    double activation_temp = 0.8;               // температура активации
    double burst_potential = 0.0;                // потенциал для всплеска

    int last_burst_step = 0;
    int BURST_COOLDOWN = 50;  // минимум шагов между burst'ами
    
    // Вспомогательные методы
    void computeDerivatives();
    double activationFunction(double x) const;
    void initializeRandom(std::mt19937& rng);
    void buildSynapsesFromWeights();

    // Высота нейрона
    float elevation_ = 0.0f;  
    float elevation_learning_rate_ = 0.001f;
    float elevation_decay_ = 0.999f;
    float cumulative_importance_ = 0.0f;
    int activity_counter_ = 0;
    
    // Обновление кэшей
    void updateCachedPhi() const;
};