#include "NeuralGroup.hpp"
#include "DynamicParams.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <numeric>
#include <random>

// Константы орбит (вынесены для удобства)
constexpr double RADII[] = {
    OrbitalParams::SINGULARITY_RADIUS,  // уровень 0
    OrbitalParams::INNER_ORBIT,         // уровень 1
    OrbitalParams::BASE_ORBIT,          // уровень 2
    OrbitalParams::MIDDLE_ORBIT,        // уровень 3
    OrbitalParams::OUTER_ORBIT          // уровень 4
};

// ============================================================================
// КОНСТРУКТОР
// ============================================================================

NeuralGroup::NeuralGroup(int size, double dt, std::mt19937& rng, 
                const MassLimits& limits)
        : size(size), dt(dt), threshold(0.5),
          mass_limits(limits),
          pos(size), vel(size), force(size),
          mass(size, 1.0), damping(size, 0.95),
      orbit_level(size, 1),  // начинаем с базовой орбиты
      target_radius(size, OrbitalParams::BASE_ORBIT),
      orbital_energy(size, 0.0),
      time_on_orbit(size, 0),
      promotion_count(size, 0),
      W_intra(size, std::vector<double>(size, 0.0)),
      metric_tensor(size, std::vector<double>(size, 0.0)),
      specialization("unspecialized"),
      activation_temp(0.8)
{
    // Инициализация позиций на базовой орбите
    for (int i = 0; i < size; ++i) {
        pos[i] = Vec3::randomOnSphere(rng) * OrbitalParams::BASE_ORBIT;
        vel[i] = Vec3(0,0,0);
        force[i] = Vec3(0,0,0);
        
        // Инициализация связей (слабые)
        for (int j = i + 1; j < size; ++j) {
            std::uniform_real_distribution<double> weight_dist(-0.1, 0.1);
            double w = weight_dist(rng);
            W_intra[i][j] = w;
            W_intra[j][i] = w;
        }
    }
    
    buildSynapsesFromWeights();
    
    weight_gradients.resize(size, std::vector<double>(size, 0.0));
    gd_velocity.resize(size, std::vector<double>(size, 0.0));
    
    for (int i = 0; i < size; i++) {
        metric_tensor[i][i] = 1.0;
    }
}

// ============================================================================
// ОСНОВНАЯ ЭВОЛЮЦИЯ
// ============================================================================

void NeuralGroup::evolve() {
   step_counter_++;
    
    // ИСПРАВЛЕНИЕ: делаем static counter или членом класса
    static int global_rebirth_counter = 0;  // лучше сделать членом класса
    
    if (global_rebirth_counter > size * 5) {  // уменьшил с 10 до 5
        std::cout << "⚠️ Mass rebirth detected, activating stabilization" << std::endl;
        
        // Мягкая стабилизация
        for (int i = 0; i < size; ++i) {
            if (pos[i].norm() < OrbitalParams::SINGULARITY_RADIUS) {
                // Не на полную орбиту, а чуть дальше от центра
                double push_radius = OrbitalParams::INNER_ORBIT * 1.2;
                pos[i] = pos[i].normalized() * push_radius;
                vel[i] = Vec3(0,0,0);  // сбрасываем скорость
            }
        }
        global_rebirth_counter = 0;
    }

    // Leapfrog integration (как было, но с орбитальными силами)
    for (int i = 0; i < size; ++i) {
        vel[i] += force[i] * (dt / (2.0 * mass[i]));
    }
    
    for (int i = 0; i < size; ++i) {
        pos[i] += vel[i] * dt;
    }
    
    // Пересчет сил с новыми орбитальными компонентами
    computeDerivatives();
    
    for (int i = 0; i < size; ++i) {
        vel[i] += force[i] * (dt / (2.0 * mass[i]));
    }
    
    // Обновление орбитальных уровней (раз в 10 шагов)
    if (step_counter_ % 10 == 0) {
        updateOrbitLevels();
        updateMetricTensor();
        updateHomeostasis();
    }
    // раз в 1000 шагов вызыв лог массы
    if (step_counter_ % 1000 == 0) {
        logMassDistribution();
    }

    // Проверка на падение в сингулярность (каждый шаг)
    for (int i = 0; i < size; ++i) {
        // int old_rebirth = // нужно хранить где-то */;
        checkForSingularity(i);
        // if (/* нейрон переродился */) global_rebirth_counter++;
    }
    
    // Увеличиваем время на орбите
    for (int i = 0; i < size; ++i) {
        time_on_orbit[i] += dt;
    }

    // Обнуляем счетчик если давно не было перерождений
    static int steps_since_last_rebirth = 0;
    if (global_rebirth_counter == 0) {
        steps_since_last_rebirth++;
        if (steps_since_last_rebirth > 1000) {
            global_rebirth_counter = 0;  // сброс
            steps_since_last_rebirth = 0;
        }
    }
}

// ============================================================================
// ОРБИТАЛЬНЫЕ СИЛЫ
// ============================================================================

void NeuralGroup::computeDerivatives() {
    std::fill(force.begin(), force.end(), Vec3(0,0,0));
    
    double current_entropy = computeEntropy();
    double target_entropy = computeEntropyTarget();
    double entropy_error = target_entropy - current_entropy;
    double avg_activity = getAverageActivity();
    
    // 1. ОРБИТАЛЬНЫЕ СИЛЫ
    applyOrbitalForces();
    
    // 2. КВАНТОВЫЙ БАРЬЕР (НОВОЕ)
    for (int i = 0; i < size; i++) {
        double barrier = computeQuantumBarrier(i);
        if (barrier > 0) {
            Vec3 radial_dir = pos[i].normalized();
            double r = pos[i].norm();
            double r_target = target_radius[i];
            
            // Сила направлена к целевой орбите
            if (r > r_target) {
                force[i] -= radial_dir * barrier;  // толкаем внутрь
            } else {
                force[i] += radial_dir * barrier;  // толкаем наружу
            }
        }
    }
    
    // 3. КОНКУРЕНЦИЯ
    applyCompetitionForces();
    
    // 4. ВЗАИМОДЕЙСТВИЕ ЮКАВА
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            if (i == j) continue;
            
            Vec3 diff = pos[i] - pos[j];
            double r_ij = diff.norm() + 1e-6;
            
            // Масса теперь переменная!
            double g_squared = 0.05 * mass[i] * mass[j];
            double w_factor = std::abs(W_intra[i][j]);
            double mu = 2.0 + entropy_error * 0.2;
            mu = std::clamp(mu, 1.0, 3.0);
            
            double exp_term = std::exp(-mu * r_ij);
            double potential_gradient = g_squared * exp_term * (1.0/(r_ij*r_ij) + mu/r_ij);
            Vec3 yukawa_force = -diff.normalized() * potential_gradient;
            
            double sign = (W_intra[i][j] > 0) ? 1.0 : -1.0;
            
            force[i] += yukawa_force * w_factor * 0.3 * sign;
        }
    }
    
    // 5. ТРЕНИЕ (зависит от массы)
    for (int i = 0; i < size; ++i) {
        // Большая масса = больше инерция = меньше влияние трения
        double inertia_factor = 1.0 / (1.0 + mass[i] * 0.5);
        double orbital_drag = damping[i] * inertia_factor;
        force[i] += vel[i] * (-orbital_drag);
    }
    
    // 6. ТЕМПЕРАТУРНЫЙ ШУМ (учитываем массу)
    static std::mt19937 rng(std::random_device{}());
    
    for (int i = 0; i < size; ++i) {
        if (orbit_level[i] < 3) {  // шум только для низких орбит
            Vec3 noise = Vec3::randomTangent(pos[i].normalized(), rng);
            
            // Квантовый принцип: тяжелые частицы меньше реагируют на шум
            double quantum_factor = 1.0 / (1.0 + mass[i] * 0.3);
            
            double T = 0.02 * quantum_factor;
            double gamma = damping[i] * 0.5;
            double sigma = std::sqrt(2.0 * gamma * T / dt);
            
            force[i] += noise * sigma * quantum_factor;
        }
    }
    
    // 7. ЦЕНТРОБЕЖНЫЙ ПОТЕНЦИАЛ (квантовый момент)
    for (int i = 0; i < size; ++i) {
        double r = pos[i].norm();
        double v_tang = getTangentialVelocity(i);
        
        // Квантовый центробежный барьер
        double angular_momentum = mass[i] * r * v_tang;
        // немного ослабили 
        double centrifugal_quantum = (angular_momentum * angular_momentum) / 
                                    (2.0 * mass[i] * r * r * r);
        // На низких орбитах центробежная сила слабее
        if (orbit_level[i] < 2) {
            centrifugal_quantum *= 0.3;
        }
        
        Vec3 radial_dir = pos[i].normalized();
        force[i] += radial_dir * centrifugal_quantum;
    }
}

// ----------------------------------------------------------------------------
// Орбитальные силы
// ----------------------------------------------------------------------------

void NeuralGroup::applyOrbitalForces() {
    for (int i = 0; i < size; i++) {
        double r = pos[i].norm();
        double r_target = target_radius[i];
        
        if (r < 1e-6) continue;  // защита от деления на ноль
        
        Vec3 radial_dir = pos[i].normalized();
        double r_error = r - r_target;
        
        // Потенциальная яма на целевой орбите (глубже для высоких орбит)
        double well_depth = OrbitalParams::ORBITAL_WELL_DEPTH * (1.0 + orbit_level[i] * 0.5);
        double orbital_well = -well_depth * r_error * std::exp(-r_error * r_error);
        
        // Центробежная сила (стабилизирует орбиту)
        double v_tangential = getTangentialVelocity(i);
        double centrifugal = v_tangential * v_tangential / r;
        
        // Гравитация к центру (сильнее для низких орбит)
        double gravity = 0.1 / (1.0 + orbit_level[i]);
        
        Vec3 radial_force = radial_dir * (orbital_well - centrifugal - gravity * r);
        force[i] += radial_force;
    }
}

// ----------------------------------------------------------------------------
// Конкуренция на орбитах
// ----------------------------------------------------------------------------

void NeuralGroup::applyCompetitionForces() {
    for (int i = 0; i < size; i++) {
        for (int j = i + 1; j < size; j++) {
            // Конкурируют только нейроны на одной орбите
            if (orbit_level[i] != orbit_level[j]) continue;
            
            Vec3 diff = pos[i] - pos[j];
            double dist = diff.norm();
            
            if (dist < OrbitalParams::COMPETITION_DISTANCE) {
                // Сила отталкивания зависит от уровня орбиты
                // (на высоких орбитах конкуренция жестче)
                double competition = OrbitalParams::COMPETITION_STRENGTH * 
                                    (1.0 + orbit_level[i] * 0.5);
                
                Vec3 repel = diff.normalized() * competition / (dist + 0.1);
                force[i] += repel;
                force[j] -= repel;
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Обновление орбитальных уровней + учитываем массу
// ----------------------------------------------------------------------------

void NeuralGroup::updateOrbitLevels() {
    for (int i = 0; i < size; i++) {
        double energy = getNeuronEnergy(i);
        double stability = getLocalStability(i);
        
        int old_level = orbit_level[i];
        
        // Повышение требует больше энергии для тяжелых нейронов
        double escape_energy = OrbitalParams::ESCAPE_ENERGY * (1.0 + mass[i] * 0.2);
        
        if (energy > escape_energy && 
            stability > OrbitalParams::HIGH_STABILITY &&
            orbit_level[i] < 4) {
            
            orbit_level[i]++;
            target_radius[i] = getOrbitRadius(orbit_level[i]);
            promotion_count[i]++;
            
            std::cout << "  Neuron " << i << " promoted to orbit " << orbit_level[i] 
                      << " (energy: " << energy << ", mass: " << mass[i] << ")" << std::endl;
        }
        
        // Понижение - тяжелые нейроны стабильнее
        else if (orbit_level[i] > 0) {
            // Масса защищает от понижения
            double mass_protection = 1.0 / (1.0 + mass[i] * 0.3);
            double effective_fall_energy = OrbitalParams::FALL_ENERGY * mass_protection;
            
            bool should_demote = false;
            
            if (energy < effective_fall_energy) {
                should_demote = true;
            }
            else if (stability < OrbitalParams::LOW_STABILITY && 
                     time_on_orbit[i] > 10.0) {
                should_demote = true;
            }
            
            if (should_demote && time_on_orbit[i] > 5.0) {
                orbit_level[i]--;
                target_radius[i] = getOrbitRadius(orbit_level[i]);
                
                std::cout << "  Neuron " << i << " demoted to orbit " << orbit_level[i] 
                          << " (energy: " << energy << ", mass: " << mass[i] << ")" << std::endl;
            }
        }
        
        if (old_level != orbit_level[i]) {
            time_on_orbit[i] = 0;
            // При смене орбиты масса немного меняется
            mass[i] *= 1.1;  // небольшой бонус при повышении
            mass[i] = std::clamp(mass[i], homeo.min_mass, homeo.max_mass);
        } else {
            time_on_orbit[i] += dt;
        }
    }
}

// ============================================================================
// Проверка падения в сингулярность - ИСПРАВЛЕНО
// ============================================================================

void NeuralGroup::checkForSingularity(int i) {
    double r = pos[i].norm();
    double energy = getNeuronEnergy(i);
    
    // Определяем текущий режим работы
    auto mode = getCurrentMode();
    const auto& survival = getSurvivalParams(mode);
    
    bool should_collapse = false;
    
    // 1. РАДИУС: в обычном режиме терпимее
    double singularity_radius = OrbitalParams::SINGULARITY_RADIUS;
    if (mode == OperatingMode::NORMAL) {
        singularity_radius *= 0.3;  // в 3 раза меньше вероятность падения
    }
    
    if (r < singularity_radius) {
        // Даем шанс на recovery
        if (energy > survival.min_energy_boost) {
            // Автоматический толчок от центра
            pos[i] += pos[i].normalized() * survival.min_energy_boost;
            return;  // не коллапсируем
        }
        should_collapse = true;
    }
    
    // 2. ВРЕМЯ: ИСПОЛЬЗУЕМ КОНСТАНТЫ ИЗ DynamicParams
    double collapse_time_seconds;
    
    if (mode == OperatingMode::TRAINING) {
        collapse_time_seconds = DynamicParams::TRAINING_COLLAPSE_TIME;
    } else if (mode == OperatingMode::IDLE) {
        collapse_time_seconds = DynamicParams::IDLE_COLLAPSE_TIME;
    } else {
        collapse_time_seconds = DynamicParams::NORMAL_COLLAPSE_TIME;
    }
    
    if (energy < OrbitalParams::COLLAPSE_ENERGY && 
        time_on_orbit[i] > collapse_time_seconds) {
        should_collapse = true;
    }
    
    // 3. АВТО-ОЖИВЛЕНИЕ
    if (should_collapse && survival.auto_revive) {
        // Не убиваем, а отправляем на базовую орбиту
        promoteToBaseOrbit(i);
        return;
    }
    
    // Старая логика коллапса только если все плохо
    if (should_collapse && rebirth_count[i] < 3) {
        static std::mt19937 rng(std::random_device{}());
        resetNeuron(i, rng);
        rebirth_count[i]++;
    }
}
// ----------------------------------------------------------------------------
// Перерождение нейрона - ИСПРАВЛЕНО
// ----------------------------------------------------------------------------

void NeuralGroup::resetNeuron(int i, std::mt19937& rng) {
    orbit_level[i] = 1;
    target_radius[i] = OrbitalParams::BASE_ORBIT;
    
    // НЕ на самой орбите, а чуть дальше от центра
    double start_radius = OrbitalParams::BASE_ORBIT * (1.2 + 0.3 * (rng() % 100 / 100.0));
    pos[i] = Vec3::randomOnSphere(rng) * start_radius;
    vel[i] = Vec3(0,0,0);
    
    orbital_energy[i] = 0.5;  // даем начальную энергию
    time_on_orbit[i] = 0;
    
    // Масса восстанавливается до базовой
    mass[i] = 1.0;
    
    // Веса не обнуляем полностью, а немного ослабляем
    for (int j = 0; j < size; j++) {
        if (i != j) {
            W_intra[i][j] *= 0.7;  // вместо 0.5
            W_intra[j][i] *= 0.7;
        }
    }
    
    syncSynapsesFromWeights();
}

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ ДЛЯ ОРБИТ
// ============================================================================

double NeuralGroup::getOrbitRadius(int level) const {
    if (level < 0) level = 0;
    if (level > 4) level = 4;
    return RADII[level];
}

double NeuralGroup::getNeuronEnergy(int i) const {
    return getKineticEnergy(i) + getPotentialEnergy(i);
}

double NeuralGroup::getTangentialVelocity(int i) const {
    Vec3 radial = pos[i].normalized();
    Vec3 tangential = vel[i] - radial * Vec3::dot(vel[i], radial);
    return tangential.norm();
}

std::vector<int> NeuralGroup::getNeuronsByOrbit(int level) const {
    std::vector<int> result;
    for (int i = 0; i < size; i++) {
        if (orbit_level[i] == level) {
            result.push_back(i);
        }
    }
    return result;
}

double NeuralGroup::computeOrbitalHealth() const {
    std::vector<int> distribution(5, 0);
    for (int i = 0; i < size; i++) {
        distribution[orbit_level[i]]++;
    }
    
    // Идеальное распределение (пирамида)
    // Уровень 4: 5%, уровень 3: 15%, уровень 2: 30%, уровень 1: 30%, уровень 0: 20%
    double ideal[] = {0.20, 0.30, 0.30, 0.15, 0.05};
    
    double health = 0;
    for (int level = 0; level < 5; level++) {
        double actual_frac = static_cast<double>(distribution[level]) / size;
        double diff = std::abs(actual_frac - ideal[level]);
        health += 1.0 - std::min(1.0, diff / ideal[level]);
    }
    
    return health / 5.0;
}

void NeuralGroup::updateMassByEnergy() {
    for (int i = 0; i < size; i++) {
        double energy = getNeuronEnergy(i);
        double memory = getMemoryStrength(i);
        int orbit = orbit_level[i];
        
        // БАЗОВАЯ МАССА (зависит от орбиты)
        double base_mass_by_orbit[] = {0.5, 1.0, 1.8, 2.8, 4.0};
        double base_mass = base_mass_by_orbit[orbit];
        
        // ВКЛАД ЭНЕРГИИ (но с насыщением)
        double energy_contribution = homeo.mass_energy_factor * 
                                     std::tanh(energy * 0.5);  // tanh дает насыщение
        
        // ВКЛАД ПАМЯТИ (тоже с насыщением)
        double memory_contribution = homeo.mass_memory_factor * 
                                     std::tanh(memory * 0.3);   // насыщение памяти
        
        // ЦЕЛЕВАЯ МАССА
        double target_mass = base_mass + energy_contribution + memory_contribution;
        
        // ПРИМЕНЯЕМ ФИЗИЧЕСКИЕ ОГРАНИЧЕНИЯ
        double mass_cap = computeMassCap(i);
        target_mass = std::min(target_mass, mass_cap);
        
        // ЭНЕРГИЯ СВЯЗИ - если связи сильные, масса может быть чуть выше
        double binding_energy = 0.0;
        for (int j = 0; j < size; j++) {
            if (i != j && std::abs(W_intra[i][j]) > 0.5) {
                binding_energy += std::abs(W_intra[i][j]) * 0.1;
            }
        }
        target_mass += binding_energy * mass_limits.binding_energy_factor;
        
        // СНОВА ПРОВЕРЯЕМ ОГРАНИЧЕНИЯ (binding energy могло превысить)
        target_mass = std::min(target_mass, mass_cap);
        
        // ПЛАВНАЯ АДАПТАЦИЯ С УЧЕТОМ "ИНЕРЦИИ МАССЫ"
        double inertia = 0.99;  // масса меняется медленно
        mass[i] = mass[i] * inertia + target_mass * (1.0 - inertia);
        
        // ФИНАЛЬНОЕ КЛИППИРОВАНИЕ
        mass[i] = std::clamp(mass[i], homeo.min_mass, mass_cap);
        
        // ЕСЛИ МАССА СЛИШКОМ БЛИЗКА К КРИТИЧЕСКОЙ - АКТИВИРУЕМ ЗАЩИТУ
        if (mass[i] > mass_cap * 0.98) { // срабатывает только при 98% от лимита
            activateMassShedding(i);
        }
    }
}

// НОВЫЙ МЕТОД: сброс избыточной массы
void NeuralGroup::activateMassShedding(int i) {
    double excess = mass[i] - computeMassCap(i) * 0.98;  // было 0.95
    if (excess > 0) {
        // Еще меньше преобразование массы в связи
        for (int j = 0; j < size; j++) {
            if (i != j && std::abs(W_intra[i][j]) < params.maxWeight) {
                W_intra[i][j] += excess * 0.001;  // было 0.005
                W_intra[j][i] = W_intra[i][j];
            }
        }
        // Еще меньше снижение массы
        mass[i] -= excess * 0.1;  // было 0.3
        
        // Выводим еще реже
        if (step_counter_ % 1000 == 0) {  // было 500
            std::cout << "  Mass shedding for neuron " << i 
                      << " new mass: " << mass[i] << std::endl;
        }
    }
}

void NeuralGroup::decayMemory() {
    for (int i = 0; i < size; i++) {
        // Память медленно затухает, если нейрон неактивен
        double activity = pos[i].norm();
        if (activity < 0.1) {  // неактивный нейрон
            for (int j = 0; j < size; j++) {
                if (i != j) {
                    W_intra[i][j] *= homeo.memory_decay;
                }
            }
        }
    }
    syncSynapsesFromWeights();
}

double NeuralGroup::computeQuantumBarrier(int i) const {
    double r = pos[i].norm();
    double r_target = target_radius[i];
    double dr = std::abs(r - r_target);
    
    // Барьер существует, но сильные сигналы могут его преодолеть
    if (dr > homeo.barrier_width) {
        double energy = getNeuronEnergy(i);
        double barrier = homeo.barrier_height * 
                        (1.0 - std::exp(-(dr - homeo.barrier_width) * 5.0));
        
        // Высокая энергия позволяет преодолеть барьер
        double penetration = 1.0 / (1.0 + energy * 0.5);
        return barrier * penetration;
    }
    return 0.0;
}

// ============================================================================
// ГОМЕОСТАЗ (адаптированный под орбиты)
// ============================================================================

void NeuralGroup::updateHomeostasis() {
    double current_entropy = computeEntropy();
    
    entropy_history.push_back(current_entropy);
    if (entropy_history.size() > homeo.history_size) {
        entropy_history.pop_front();
    }
    
    double entropy_error = homeo.target_entropy - current_entropy;
    double adaptation = std::clamp(entropy_error * homeo.adaptation_rate, -0.1, 0.1);
    
    for (int i = 0; i < size; i++) {
        // Демпфирование
        double target_damping = homeo.min_damping + 
                               (homeo.max_damping - homeo.min_damping) * 
                               (1.0 - orbit_level[i] / 5.0);
        damping[i] += adaptation * 0.2;
        damping[i] = std::clamp(damping[i], target_damping * 0.8, target_damping * 1.2);
    }
    
    // ОБНОВЛЕНИЕ МАССЫ НА ОСНОВЕ ЭНЕРГИИ
    updateMassByEnergy();
    
    // Температура активации
    activation_temp -= adaptation * 1.0;
    activation_temp = std::clamp(activation_temp, 
                                 homeo.min_temperature, 
                                 homeo.max_temperature);
    
    // Burst логика...
    if (std::abs(entropy_error) > homeo.entropy_tolerance) {
        burst_potential += std::abs(entropy_error) * 0.02;
        if (burst_potential > homeo.burst_threshold && 
            step_counter_ - last_burst_step > BURST_COOLDOWN) {
            generateBurst();
            burst_potential = 0.0;
            last_burst_step = step_counter_;
        }
    }
}

void NeuralGroup::generateBurst() {
    std::cout << "  Burst generated to increase diversity" << std::endl;
    
    static std::mt19937 rng(std::random_device{}());
    double burst_strength = homeo.burst_strength;
    
    for (int i = 0; i < size; ++i) {
        // Шум, касательный к сфере
        Vec3 noise = Vec3::randomTangent(pos[i].normalized(), rng, burst_strength);
        vel[i] += noise;
        
        // Временно уменьшаем массу (чтобы легче менять орбиту)
        mass[i] *= 0.7;
        mass[i] = std::clamp(mass[i], 0.3, 3.0);
    }
    
    // Ограничение весов
    for (auto& syn : synapses) {
        syn.weight = std::clamp(syn.weight, -params.maxWeight, params.maxWeight);
    }
}

double NeuralGroup::getAdaptiveNoiseLevel() const {
    if (entropy_history.size() < 10) return homeo.base_noise_level;
    
    double recent_entropy = 0.0;
    for (size_t i = entropy_history.size() - 10; i < entropy_history.size(); i++) {
        recent_entropy += entropy_history[i];
    }
    recent_entropy /= 10;
    
    double entropy_error = homeo.target_entropy - recent_entropy;
    double noise_level = homeo.base_noise_level + std::abs(entropy_error) * 0.1;
    
    return std::clamp(noise_level, 0.01, 0.5);
}

// ============================================================================
// STDP ОБУЧЕНИЕ (с учетом орбит)
// ============================================================================

void NeuralGroup::learnSTDP(float reward, int currentStep) {
    int synIndex = 0;
    double current_entropy = computeEntropy();
    double target_entropy = computeEntropyTarget();
    double entropy_error = target_entropy - current_entropy;

    for (int i = 0; i < size; ++i) {
        for (int j = i + 1; j < size; ++j) {
            if (synIndex >= synapses.size()) break;

            auto& syn = synapses[synIndex++];

            // Активность = расстояние от центра
            double activity_i = pos[i].norm();
            double activity_j = pos[j].norm();
            
            const bool isActive = (activity_i > 0.2 || activity_j > 0.2);
            const bool preSpike  = (activity_i > getEffectiveThreshold());
            const bool postSpike = (activity_j > getEffectiveThreshold());

            if (preSpike) syn.lastPreFire = static_cast<float>(currentStep);
            if (postSpike) syn.lastPostFire = static_cast<float>(currentStep);

            float delta = 0.0f;
            
            if (isActive) {
                if (preSpike || postSpike) {
                    float dt = syn.lastPostFire - syn.lastPreFire;
                    if (dt > 0.0f) {
                        delta = params.A_plus * std::exp(-dt / params.tau_plus);
                    } else if (dt < 0.0f) {
                        delta = -params.A_minus * std::exp(dt / params.tau_minus);
                    }
                } else {
                    delta = -0.0005f;
                }
            } else {
                delta = -0.001f;
            }

            // Модификаторы на основе орбит
            float orbit_factor = 1.0f;
            if (orbit_level[i] > 3 && orbit_level[j] > 3) {
                orbit_factor = 2.0f;  // элитные связи усиливаются сильнее
            } else if (orbit_level[i] < 2 || orbit_level[j] < 2) {
                orbit_factor = 0.5f;  // слабые связи слабее
            }
            
            float elevation_factor = 1.0f + elevation_;
            float entropy_factor = 1.0f + static_cast<float>(entropy_error * 0.1);
            
            // НОВОЕ: фактор массы - тяжелые нейроны меняются медленнее
            float mass_factor = 1.0f / (1.0f + static_cast<float>((mass[i] + mass[j]) * 0.1f));
            
            syn.eligibility = syn.eligibility * params.eligibilityDecay + delta;
            
            float weight_change = 0.0f;
            
            if (isActive) {
                weight_change = params.stdpRate * reward * syn.eligibility * 
                               elevation_factor * entropy_factor * orbit_factor * mass_factor;
                syn.weight += weight_change;
                
                // НОВОЕ: масса растет пропорционально усилению связи
                // Но только для значительных изменений
                if (std::abs(weight_change) > 0.001f) {
                    // Положительное подкрепление увеличивает массу
                    if (reward > 0) {
                        mass[i] += weight_change * 10.0f;
                        mass[j] += weight_change * 10.0f;
                    }
                    // Отрицательное подкрепление может уменьшать массу
                    else if (reward < 0 && mass[i] > homeo.min_mass) {
                        mass[i] -= std::abs(weight_change) * 5.0f;
                        mass[j] -= std::abs(weight_change) * 5.0f;
                    }
                    
                    // Ограничиваем массу
                    mass[i] = std::clamp(mass[i], homeo.min_mass, homeo.max_mass);
                    mass[j] = std::clamp(mass[j], homeo.min_mass, homeo.max_mass);
                }
            } else {
                // Неактивные нейроны slowly forget
                syn.weight *= 0.999f;
                
                // И их масса медленно возвращается к базовой
                if (mass[i] > 1.0) {
                    mass[i] = mass[i] * 0.999 + 1.0 * 0.001;
                }
                if (mass[j] > 1.0) {
                    mass[j] = mass[j] * 0.999 + 1.0 * 0.001;
                }
            }

            syn.weight = std::clamp(syn.weight, -params.maxWeight, params.maxWeight);

            W_intra[i][j] = syn.weight;
            W_intra[j][i] = syn.weight;
        }
    }
    
    // НОВОЕ: периодически применяем затухание памяти
    if (step_counter_ % 100 == 0) {
        decayMemory();
    }
}

// ============================================================================
// МЕТРИКА И ЭНТРОПИЯ
// ============================================================================

void NeuralGroup::updateMetricTensor() {
    const double alpha = 0.0005;
    
    // Проверка на NaN
    int nan_count = 0;
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            if (!std::isfinite(metric_tensor[i][j])) {
                nan_count++;
            }
        }
    }
    
    if (nan_count > size * size / 2) {
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size; j++) {
                metric_tensor[i][j] = (i == j) ? 1.0 : 0.0;
            }
        }
        return;
    }
    
    for (int i = 0; i < size; i++) {
        for (int j = i; j < size; j++) {
            if (i == j) {
                metric_tensor[i][i] = 1.0;
            } else {
                double dist = (pos[i] - pos[j]).norm();
                double correlation = std::exp(-dist * dist);
                correlation = std::clamp(correlation, -1.0, 1.0);
                
                metric_tensor[i][j] = metric_tensor[i][j] * (1.0 - alpha) + correlation * alpha;
                metric_tensor[j][i] = metric_tensor[i][j];
                
                // Нормализация
                double sum = 0;
                for (int k = 0; k < size; k++) {
                    sum += std::abs(metric_tensor[i][k]);
                }
                
                if (sum > 1.0 && sum > 1e-8) {
                    for (int k = 0; k < size; k++) {
                        metric_tensor[i][k] /= sum;
                    }
                }
            }
        }
    }
}

double NeuralGroup::computeEntropy() const {
    const int BINS = 8;
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
    
    double entropy = 0.0;
    for (int count : hist_azimuth) {
        if (count > 0) {
            double p = static_cast<double>(count) / size;
            entropy -= p * std::log(p);
        }
    }
    for (int count : hist_polar) {
        if (count > 0) {
            double p = static_cast<double>(count) / size;
            entropy -= p * std::log(p);
        }
    }
    
    return entropy / 2.0;
}

double NeuralGroup::scalarCurvature() const {
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            if (!std::isfinite(metric_tensor[i][j])) {
                return 0.0;
            }
        }
    }

    if (size < 3) return 0.0;
    
    double total_curvature = 0.0;
    int triangles = 0;
    
    for (int i = 0; i < size; i++) {
        for (int j = i + 1; j < size; j++) {
            for (int k = j + 1; k < size; k++) {
                double g_ij = std::abs(metric_tensor[i][j]);
                double g_jk = std::abs(metric_tensor[j][k]);
                double g_ki = std::abs(metric_tensor[k][i]);
                
                g_ij = std::clamp(g_ij, 0.01, 10.0);
                g_jk = std::clamp(g_jk, 0.01, 10.0);
                g_ki = std::clamp(g_ki, 0.01, 10.0);
                
                double d_ij = std::sqrt(g_ij);
                double d_jk = std::sqrt(g_jk);
                double d_ki = std::sqrt(g_ki);
                
                double s = (d_ij + d_jk + d_ki) / 2.0;
                double area_sq = s * (s - d_ij) * (s - d_jk) * (s - d_ki);
                
                if (area_sq <= 1e-10) continue;
                
                double area = std::sqrt(area_sq);
                
                double cos_i = (d_ij*d_ij + d_ki*d_ki - d_jk*d_jk) / (2.0 * d_ij * d_ki + 1e-8);
                cos_i = std::clamp(cos_i, -0.999, 0.999);
                double angle_i = std::acos(cos_i);
                
                double cos_j = (d_ij*d_ij + d_jk*d_jk - d_ki*d_ki) / (2.0 * d_ij * d_jk + 1e-8);
                cos_j = std::clamp(cos_j, -0.999, 0.999);
                double angle_j = std::acos(cos_j);
                
                double angle_k = M_PI - (angle_i + angle_j);
                
                if (angle_k <= 0 || angle_k >= M_PI) continue;
                
                double angle_defect = M_PI - (angle_i + angle_j + angle_k);
                
                if (area > 1e-6) {
                    double curvature = angle_defect / area;
                    curvature = std::clamp(curvature, -10.0, 10.0);
                    
                    if (std::isfinite(curvature)) {
                        total_curvature += curvature;
                        triangles++;
                    }
                }
            }
        }
    }
    
    return triangles > 0 ? total_curvature / triangles : 0.0;
}

double NeuralGroup::computeEntropyTarget() const {
    double target = 2.0;
    
    if (specialization == "perception") target = 1.5;
    else if (specialization == "memory") target = 2.0;
    else if (specialization == "action") target = 1.2;
    else if (specialization == "abstract") target = 2.5;
    else if (specialization == "language") target = 2.2;
    else if (specialization == "curiosity") target = 2.8;
    
    double avg_activity = getAverageActivity();
    if (avg_activity < 0.2) target *= 1.2;
    else if (avg_activity > 0.8) target *= 0.8;
    
    double curvature = scalarCurvature();
    if (std::isfinite(curvature) && std::abs(curvature) < 10.0) {
        target *= (1.0 + 0.1 * curvature);
    }
    
    return target;
}

// ============================================================================
// МЕТОДЫ ДЛЯ ОБРАТНОЙ СОВМЕСТИМОСТИ (1D)
// ============================================================================

double NeuralGroup::getAverageActivity() const {
    double sum = 0.0;
    for (const auto& p : pos) {
        sum += p.norm();
    }
    return sum / size;
}

const std::vector<double>& NeuralGroup::getPhi() const {
    phi_cached.resize(size);
    for (int i = 0; i < size; ++i) {
        phi_cached[i] = pos[i].norm();
    }
    return phi_cached;
}

std::vector<double>& NeuralGroup::getPhiNonConst() {
    phi_cached.resize(size);
    for (int i = 0; i < size; ++i) {
        phi_cached[i] = pos[i].norm();
    }
    return phi_cached;
}

const std::vector<double>& NeuralGroup::getPi() const {
    pi_cached.resize(size);
    for (int i = 0; i < size; ++i) {
        pi_cached[i] = vel[i].norm();
    }
    return pi_cached;
}

std::vector<double>& NeuralGroup::getPiNonConst() {
    pi_cached.resize(size);
    for (int i = 0; i < size; ++i) {
        pi_cached[i] = vel[i].norm();
    }
    return pi_cached;
}

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
// ============================================================================

void NeuralGroup::buildSynapsesFromWeights() {
    synapses.clear();
    for (int i = 0; i < size; ++i) {
        for (int j = i + 1; j < size; ++j) {
            Synapse syn;
            syn.weight = static_cast<float>(W_intra[i][j]);
            synapses.push_back(syn);
        }
    }
}

void NeuralGroup::syncSynapsesFromWeights() {
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

void NeuralGroup::syncWeightsFromSynapses() {
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

void NeuralGroup::consolidate() {
    for (auto& syn : synapses) {
        syn.weight += params.consolidationRate * syn.eligibility;
        syn.eligibility *= 0.9f;
        
        if (syn.weight > params.maxWeight) syn.weight = params.maxWeight;
        if (syn.weight < -params.maxWeight) syn.weight = -params.maxWeight;
    }
    syncWeightsFromSynapses();
}

void NeuralGroup::decayAllWeights(float factor) {
    for (auto& syn : synapses) {
        syn.weight *= factor;
    }
    syncWeightsFromSynapses();
}

void NeuralGroup::updateElevationFast(float reward, float activity) {
    double entropy = computeEntropy();
    double target_entropy = computeEntropyTarget();
    double entropy_contribution = -entropy * std::log(entropy + 1e-12);
    
    if (activity > getEffectiveThreshold()) {
        elevation_ += elevation_learning_rate_ * reward * entropy_contribution;
    }
    
    elevation_ *= elevation_decay_;
    elevation_ = std::clamp(elevation_, -1.0f, 1.0f);
}

void NeuralGroup::consolidateElevation(float globalImportance, float hallucinationPenalty) {
    if (activity_counter_ > 0) {
        float avg_importance = cumulative_importance_ / activity_counter_;
        double entropy = computeEntropy();
        double entropy_factor = 1.0 + (entropy - 2.0) * 0.1;
        
        elevation_ += (avg_importance - 0.5f) * 0.1f * entropy_factor;
        elevation_ -= hallucinationPenalty * 0.2f;
        elevation_ = std::clamp(elevation_, -1.0f, 1.0f);
    }
    
    cumulative_importance_ = 0.0f;
    activity_counter_ = 0;
}

void NeuralGroup::consolidateEligibility(float globalImportance) {
    double entropy = computeEntropy();
    double entropy_factor = 1.0 / (1.0 + std::exp(-(entropy - 2.0)));

    for (auto& syn : synapses) {
        float consolidation_factor = params.consolidationRate * globalImportance;
        consolidation_factor *= (1.0f + elevation_) * static_cast<float>(entropy_factor);
        
        syn.weight += consolidation_factor * syn.eligibility;
        syn.eligibility *= 0.5f;
        
        syn.weight = std::clamp(syn.weight, -params.maxWeight, params.maxWeight);
    }
    syncWeightsFromSynapses();
}

// ============================================================================
// МЕТОДЫ ДЛЯ ГРАДИЕНТНОГО СПУСКА (обратная совместимость)
// ============================================================================

void NeuralGroup::computeGradients(const std::vector<double>& target) {
    if (target.size() != size) return;
    
    double current_entropy = computeEntropy();
    double target_entropy = computeEntropyTarget();
    double entropy_gradient = target_entropy - current_entropy;

    for (int i = 0; i < size; ++i) {
        double activity = pos[i].norm();
        double error_i = (activity - 0.5) * entropy_gradient * 0.1;
        
        for (int j = 0; j < size; ++j) {
            double activity_j = pos[j].norm();
            weight_gradients[i][j] = error_i * activity_j;
        }
    }
}

void NeuralGroup::applyGradients() {
    for (int i = 0; i < size; ++i) {
        for (int j = i + 1; j < size; ++j) {
            gd_velocity[i][j] = gd_momentum * gd_velocity[i][j]
                               - gd_learning_rate * weight_gradients[i][j];

            W_intra[i][j] += gd_velocity[i][j];
            W_intra[j][i] = W_intra[i][j];

            W_intra[i][j] = std::clamp(
                W_intra[i][j],
                -static_cast<double>(params.maxWeight),
                static_cast<double>(params.maxWeight)
            );
            W_intra[j][i] = W_intra[i][j];
        }
    }
    
    syncSynapsesFromWeights();
}

void NeuralGroup::learnHebbian(double globalReward) {
    double current_entropy = computeEntropy();
    double target_entropy = computeEntropyTarget();
    double entropy_factor = 1.0 + (target_entropy - current_entropy) * 0.1;

    for (int i = 0; i < size; ++i) {
        for (int j = i + 1; j < size; ++j) {
            double activity_i = pos[i].norm();
            double activity_j = pos[j].norm();
            
            double hebb = activity_i * activity_j;
            double update = params.hebbianRate * (hebb + globalReward * 0.1) * entropy_factor;
            
            W_intra[i][j] += update;
            W_intra[j][i] = W_intra[i][j];
            
            double maxWeight = static_cast<double>(params.maxWeight);
            double minWeight = static_cast<double>(-params.maxWeight);
            
            W_intra[i][j] = std::clamp(W_intra[i][j], minWeight, maxWeight);
            W_intra[j][i] = W_intra[i][j];
        }
    }
    syncSynapsesFromWeights();
}

void NeuralGroup::logMassDistribution() {
    if (step_counter_ % 1000 != 0) return;
    
    double min_m = 1000, max_m = 0, avg_m = 0;
    for (double m : mass) {
        min_m = std::min(min_m, m);
        max_m = std::max(max_m, m);
        avg_m += m;
    }
    avg_m /= size;
    
    std::cout << "Mass distribution - min: " << min_m 
              << ", max: " << max_m 
              << ", avg: " << avg_m << std::endl;
}

void NeuralGroup::maintainActivity(OperatingMode::Type mode) {
    auto& survival = getSurvivalParams(mode);
    
    // Минимальная активность для всех нейронов
    for (int i = 0; i < size; i++) {
        double r = pos[i].norm();
        
        // Если нейрон слишком близко к центру
        if (r < OrbitalParams::INNER_ORBIT * 0.5) {
            // Добавляем случайный шум
            static std::mt19937 rng(std::random_device{}());
            Vec3 noise = Vec3::randomTangent(pos[i].normalized(), rng, 
                                            survival.noise_level);
            
            // Толкаем наружу
            vel[i] += noise * survival.idle_activity;
            
            // Добавляем центробежную силу
            Vec3 radial = pos[i].normalized();
            vel[i] += radial * survival.min_energy_boost;
        }
    }
    
    // В режиме IDLE поддерживаем слабую активность
    if (mode == OperatingMode::IDLE) {
        // Периодические "вздохи" - синхронная активность
        if (step_counter_ % 1000 == 0) {
            generateGentleBurst();
        }
    }
}

void NeuralGroup::generateGentleBurst() {
    // Мягкая вспышка активности (не такая сильная как generateBurst)
    static std::mt19937 rng(std::random_device{}());
    
    for (int i = 0; i < size; i += 3) {  // каждый третий нейрон
        if (pos[i].norm() < OrbitalParams::BASE_ORBIT * 0.5) {
            Vec3 noise = Vec3::randomTangent(pos[i].normalized(), rng, 0.1);
            vel[i] += noise;
            
            // Укрепляем связи с активными нейронами
            for (int j = 0; j < size; j++) {
                if (i != j && pos[j].norm() > OrbitalParams::BASE_ORBIT) {
                    W_intra[i][j] *= 1.01;  // чуть усиливаем
                    W_intra[j][i] = W_intra[i][j];
                }
            }
        }
    }
}