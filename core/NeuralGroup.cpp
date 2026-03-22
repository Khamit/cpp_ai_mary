#include "NeuralGroup.hpp"
#include "DynamicParams.hpp"
#include <cmath>
#include <complex>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <numeric>
#include <random>
#include <set>

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
      activation_temp(0.8),
      // НОВЫЕ ИНИЦИАЛИЗАЦИИ
      wave_function(size, std::complex<double>(1.0, 0.0)),
      wave_phase(size, 0.0),
      wave_amplitude(size, 1.0)
{
    neuron_specialization.resize(size, "reserve");

    // Инициализация счетчиков использования
    connection_usage_count.resize(size, std::vector<int>(size, 0));
    connection_success_count.resize(size, std::vector<int>(size, 0));
    connection_last_used_step.resize(size, std::vector<int>(size, 0));
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

    // Инициализация волновой функции
    for (int i = 0; i < size; ++i) {
        double r = pos[i].norm();
        // Начальная амплитуда зависит от радиуса
        wave_amplitude[i] = std::exp(-r * 0.5);
        wave_phase[i] = 2.0 * M_PI * (rng() % 1000) / 1000.0;
        wave_function[i] = std::polar(wave_amplitude[i], wave_phase[i]);
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

    // Волна влияет на орбитальные уровни
    if (step_counter_ % 10 == 0) {
        for (int i = 0; i < size; i++) {
            // Квантовое туннелирование - шанс спонтанного повышения
            if (wave_amplitude[i] > 1.5 && orbit_level[i] < 4) {
                static std::mt19937 rng(std::random_device{}());
                if (std::uniform_real_distribution<>(0, 1)(rng) < 0.001) {  // 0.1% шанс
                    std::cout << "  Quantum tunneling! Neuron " << i 
                              << " jumps to orbit " << (orbit_level[i] + 1) << std::endl;
                    orbit_level[i]++;
                    target_radius[i] = getOrbitRadius(orbit_level[i]);
                }
            }
        }
    }

    // Обновляем волновую функцию (каждый шаг)
    updateWaveFunction();

    // Leapfrog integration
    for (int i = 0; i < size; ++i) {
        vel[i] += force[i] * (dt / (2.0 * mass[i]));
    }
    
    for (int i = 0; i < size; ++i) {
        pos[i] += vel[i] * dt;
    }
    
    // Пересчет сил
    computeDerivatives();
    
    for (int i = 0; i < size; ++i) {
        vel[i] += force[i] * (dt / (2.0 * mass[i]));
    }
    
    // Обновление орбитальных уровней (раз в 10 шагов)
    if (step_counter_ % 10 == 0) {
        updateOrbitLevels();
        // updateMetricTensor();
        updateHomeostasis();
    }

        // ===== НОВОЕ: очистка неиспользуемых связей =====
    if (step_counter_ % 50 == 0) {
        pruneConnectionsByOrbit();
    }
    
    // ===== НОВОЕ: любопытство — создание новых связей =====
    if (step_counter_ % 200 == 0) {
        generateCuriosityConnections();
    }

    // Метрический тензор обновляем реже (раз в 100 шагов)
    if (step_counter_ % 100 == 0) {
        updateMetricTensor();
    }
    
    // Лог массы (раз в 1000 шагов)
    if (step_counter_ % 1000 == 0) {
        logMassDistribution();
    }

    // НОВОЕ: единый генератор случайных чисел для всех проверок
    static std::mt19937 rng(std::random_device{}());
    
    // Проверка на побег и сингулярность (каждый шаг)
    int rebirth_this_step = 0;
    for (int i = 0; i < size; ++i) {
        // Сначала проверяем, не сбежал ли нейрон (более экстремальное условие)
        checkForEscape(i, rng);
        
        // Затем проверяем на падение в сингулярность
        bool was_reset = checkForSingularity(i);
        if (was_reset) {
            rebirth_this_step++;
        }
    }
    
    // Увеличиваем время на орбите для всех нейронов
    for (int i = 0; i < size; ++i) {
        time_on_orbit[i] += dt;
    }
    
    // Если слишком много перерождений за шаг - легкая стабилизация
    if (rebirth_this_step > size / 10) {  // больше 10% нейронов переродилось
        std::cout << "Mass rebirth detected (" << rebirth_this_step 
                  << " neurons), activating gentle stabilization" << std::endl;
        
        for (int i = 0; i < size; ++i) {
            if (orbit_level[i] == 0) {  // нейроны в сингулярности
                // Даем небольшой толчок, чтобы не застревали
                Vec3 random_boost = Vec3::randomOnSphere(rng) * 0.1;
                vel[i] += random_boost;
            }
        }
    }
    // НОВОЕ: периодический массовый выброс из сингулярности
    if (step_counter_ % 50 == 0) {  // чаще, каждые 50 шагов
        static std::mt19937 rng(std::random_device{}());
        int ejected = 0;
        
        for (int i = 0; i < size; i++) {
            if (orbit_level[i] == 0) {
                // Увеличиваем шанс выброса
                double time_factor = std::min(1.0, time_on_orbit[i] / 100.0);
                double chance = 0.05 + time_factor * 0.2;  // 5-25% шанс
                
                if (std::uniform_real_distribution<>(0, 1)(rng) < chance) {
                    ejectFromSingularity(i);
                    ejected++;
                }
            }
        }
        
        if (ejected > 5) {
            std::cout << "  Mass ejection: " << ejected << " neurons escaped singularity" << std::endl;
        }
    }
}

void NeuralGroup::checkForEscape(int i, std::mt19937& rng) {
    double r = pos[i].norm();
    
    // Определяем предел побега - немного больше OUTER_ORBIT
    const double ESCAPE_LIMIT = OrbitalParams::OUTER_ORBIT * 1.8;  // ~4.68
    
    // Если нейрон слишком далеко - он "сбежал"
    if (r > ESCAPE_LIMIT) {
        // УБИРАЕМ вывод для каждого нейрона, оставляем только для отладки
        // std::cout << "  Neuron " << i << " escaped to infinity at r=" << r 
        //          << "! Resetting to singularity..." << std::endl;
        
        // Отправляем в сингулярность (уровень 0)
        orbit_level[i] = 0;
        target_radius[i] = OrbitalParams::SINGULARITY_RADIUS;
        
        // Телепортируем в центр с маленьким шумом
        pos[i] = Vec3::randomOnSphere(rng) * OrbitalParams::SINGULARITY_RADIUS * 2.0;
        vel[i] = Vec3(0,0,0);
        
        // Обнуляем энергию
        orbital_energy[i] = 0.0;
        
        // Масса падает до минимума
        mass[i] = homeo.min_mass;
        
        // Сильно ослабляем все связи (забывает всё)
        for (int j = 0; j < size; j++) {
            if (i != j) {
                W_intra[i][j] *= 0.1;  // теряет 90% связей
                W_intra[j][i] *= 0.1;
            }
        }
        
        // Волновая функция тоже сбрасывается
        wave_amplitude[i] = 0.2;
        wave_phase[i] = 2.0 * M_PI * (rng() % 1000) / 1000.0;
        wave_function[i] = std::polar(wave_amplitude[i], wave_phase[i]);
        
        syncSynapsesFromWeights();
    }
}
// ============================================================================
// ОРБИТАЛЬНЫЕ СИЛЫ
// ============================================================================

// ============================================================================
// ОРБИТАЛЬНЫЕ СИЛЫ - ИСПРАВЛЕННАЯ ВЕРСИЯ
// ============================================================================

void NeuralGroup::computeDerivatives() {
    // УБИРАЕМ глобальное ограничение скорости
    // const double MAX_SPEED = 0.5; - УДАЛЯЕМ
    
    std::fill(force.begin(), force.end(), Vec3(0,0,0));
    
    double current_entropy = computeEntropy();
    double target_entropy = computeEntropyTarget();
    double entropy_error = target_entropy - current_entropy;
    
    // 1. ОРБИТАЛЬНЫЕ СИЛЫ
    applyOrbitalForces();
    
    // 2. КВАНТОВЫЙ БАРЬЕР
    for (int i = 0; i < size; i++) {
        double barrier = computeQuantumBarrier(i);
        if (barrier > 0) {
            Vec3 radial_dir = pos[i].normalized();
            double r = pos[i].norm();
            double r_target = target_radius[i];
            
            if (r > r_target) {
                force[i] -= radial_dir * barrier;
            } else {
                force[i] += radial_dir * barrier;
            }
        }
    }
    
    // 3. КОНКУРЕНЦИЯ
    applyCompetitionForces();
    
    // 4. ВЗАИМОДЕЙСТВИЕ ЮКАВА (без изменений)
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            if (i == j) continue;
            
            Vec3 diff = pos[i] - pos[j];
            double r_ij = diff.norm() + 1e-6;
            
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
    
    // 5. ИСПРАВЛЕННОЕ ТРЕНИЕ - уменьшаем вблизи центра
    for (int i = 0; i < size; ++i) {
        double r = pos[i].norm();
        double inertia_factor = 1.0 / (1.0 + mass[i] * 0.5);
        
        // УМЕНЬШАЕМ ТРЕНИЕ В ЦЕНТРЕ
        double damping_factor = damping[i];
        if (r < OrbitalParams::INNER_ORBIT) {
            damping_factor *= 0.1;  // в 10 раз меньше трения в центре
        }
        
        double orbital_drag = damping_factor * inertia_factor;
        force[i] += vel[i] * (-orbital_drag);
    }
    
    // 6. ТЕМПЕРАТУРНЫЙ ШУМ (без изменений)
    static std::mt19937 rng(std::random_device{}());
    
    for (int i = 0; i < size; ++i) {
        if (orbit_level[i] < 3) {
            Vec3 noise = Vec3::randomTangent(pos[i].normalized(), rng);
            double quantum_factor = 1.0 / (1.0 + mass[i] * 0.3);
            double T = 0.02 * quantum_factor;
            double gamma = damping[i] * 0.5;
            double sigma = std::sqrt(2.0 * gamma * T / dt);
            force[i] += noise * sigma * quantum_factor;
        }
    }
    
    // 7. ЦЕНТРОБЕЖНЫЙ ПОТЕНЦИАЛ (без изменений)
    for (int i = 0; i < size; ++i) {
        double r = pos[i].norm();
        double v_tang = getTangentialVelocity(i);
        
        double angular_momentum = mass[i] * r * v_tang;
        double centrifugal_quantum = (angular_momentum * angular_momentum) / 
                                    (2.0 * mass[i] * r * r * r);
        
        if (orbit_level[i] < 2) {
            centrifugal_quantum *= 0.3;
        }
        
        Vec3 radial_dir = pos[i].normalized();
        force[i] += radial_dir * centrifugal_quantum;
    }

    // 8. ИСПРАВЛЕННОЕ ВЫТАЛКИВАНИЕ ИЗ СИНГУЛЯРНОСТИ
    for (int i = 0; i < size; i++) {
        double r = pos[i].norm();
        
        // Критическое расстояние - когда применяем импульс, а не силу
        const double CRITICAL_RADIUS = OrbitalParams::INNER_ORBIT * 0.2;
        
        if (r < CRITICAL_RADIUS) {
            Vec3 radial;
            
            // ИСПРАВЛЕНИЕ: фиксируем направление при r→0
            if (r < 1e-4) {
                // Случайное направление для избежания "мертвой точки"
                static std::mt19937 rng(std::random_device{}());
                radial = Vec3::randomOnSphere(rng);
            } else {
                radial = pos[i].normalized();
            }
            
            // ИСПРАВЛЕНИЕ: убираем +0.001 из знаменателя
            double pauli_repulsion = 0.1 / (r * r * r + 1e-12);  // минимум, но не константа
            
            // Усиливаем для нейронов в сингулярности
            if (orbit_level[i] == 0) {
                pauli_repulsion *= 20.0;  // усилено с 5.0 до 20.0
            }
            
            // ПРИМЕНЯЕМ ИМПУЛЬС, а не силу (для преодоления диссипации)
            double impulse_strength = pauli_repulsion * dt * 10.0;
            vel[i] += radial * impulse_strength;
            
            // Добавляем случайную компоненту
            static std::mt19937 rng(std::random_device{}());
            Vec3 random_dir = Vec3::randomOnSphere(rng);
            vel[i] += random_dir * impulse_strength * 0.3;
            
            // Логируем редкие события
            static int log_counter = 0;
            if (orbit_level[i] == 0 && log_counter++ % 100 == 0) {
                std::cout << "  Impulse applied to neuron " << i 
                          << " at r=" << r << ", impulse=" << impulse_strength << std::endl;
            }
        }
    }
    
    // 9. ОГРАНИЧЕНИЕ РАДИУСА (без изменений)
    const double ABSOLUTE_LIMIT = OrbitalParams::OUTER_ORBIT * 2.0;
    for (int i = 0; i < size; i++) {
        double r = pos[i].norm();
        if (r > ABSOLUTE_LIMIT) {
            Vec3 radial_dir = pos[i].normalized();
            pos[i] = radial_dir * ABSOLUTE_LIMIT * 0.9;
            vel[i] = Vec3(0,0,0);
        }
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
        
        // Потенциальная яма на целевой орбите
        double well_depth = OrbitalParams::ORBITAL_WELL_DEPTH * (1.0 + orbit_level[i] * 0.5);
        double orbital_well = -well_depth * r_error * std::exp(-r_error * r_error);
        
        // Центробежная сила
        double v_tangential = getTangentialVelocity(i);
        double centrifugal = v_tangential * v_tangential / r;
        
        // УСИЛЕННАЯ ГРАВИТАЦИЯ К ЦЕНТРУ
        double gravity = 0.2 * (1.0 + r * 0.5) / (1.0 + orbit_level[i] * 0.3);
        
        // ВНЕШНИЙ ПОТЕНЦИАЛ (стенка на больших расстояниях)
        double outer_wall = 0.0;
        double outer_limit = OrbitalParams::OUTER_ORBIT * 1.5;  // предел: 3.9
        
        if (r > outer_limit) {
            // Экспоненциально растущая сила
            double wall_strength = 0.5 * (r - outer_limit) * (r - outer_limit);
            outer_wall = wall_strength;
            
            // ВАЖНО: ДОБАВЛЯЕМ ДИССИПАЦИЮ ЭНЕРГИИ ПРИ УДАРЕ
            // Проверяем, движется ли нейрон наружу
            double radial_velocity = Vec3::dot(vel[i], radial_dir);
            
            if (radial_velocity > 0) {
                // Нейрон движется наружу - тормозим сильнее
                double damping_factor = 0.5;  // теряем 50% радиальной скорости
                vel[i] -= radial_dir * radial_velocity * damping_factor;
                
                // Конвертируем часть кинетической энергии в орбитальную
                Vec3 tangent = Vec3::cross(radial_dir, Vec3(0,1,0));
                if (tangent.norm() < 0.1) tangent = Vec3::cross(radial_dir, Vec3(1,0,0));
                tangent = tangent.normalized();
                
                vel[i] += tangent * std::abs(radial_velocity) * 0.2;
            }
            
            // Уменьшаем вывод до 1 раза в 1000 ударов
            static int hit_counter = 0;
            hit_counter++;
            if (hit_counter % 1000 == 0) {  // было 100
                std::cout << " Neuron " << i << " at wall (r=" << r << ", hits=" << hit_counter << ")" << std::endl;
            }
        }
        
        // Суммарная радиальная сила
        Vec3 radial_force = radial_dir * (orbital_well - centrifugal - gravity * r - outer_wall);
        force[i] += radial_force;
        
        // Дополнительное затухание на больших орбитах
        if (r > OrbitalParams::OUTER_ORBIT) {
            double extra_damping = 0.05 * (r - OrbitalParams::OUTER_ORBIT);  // увеличил с 0.01 до 0.05
            vel[i] *= (1.0 - extra_damping);
        }
    }
    // ===== УСИЛЕННЫЙ Hebbian-компонент =====
    for (int i = 0; i < size; i++) {
        for (int j = i + 1; j < size; j++) {
            double act_i = pos[i].norm();
            double act_j = pos[j].norm();
            double correlation = act_i * act_j;
            
            // Если нейроны активны и на высоких орбитах
            if (correlation > 0.5 && orbit_level[i] >= 2 && orbit_level[j] >= 2) {
                // Сильная Hebbian-сила
                double hebbian_strength = 0.05 * correlation;  // было 0.01
                
                Vec3 diff = pos[i] - pos[j];
                double dist = diff.norm() + 1e-6;
                Vec3 hebbian_force = diff.normalized() * hebbian_strength / (dist + 0.1);
                
                // Положительные связи притягиваются
                if (W_intra[i][j] > 0) {
                    force[i] += hebbian_force;
                    force[j] -= hebbian_force;
                }
            }
        }
    }
}
// ----------------------------------------------------------------------------
// Конкуренция на орбитах
// ----------------------------------------------------------------------------

void NeuralGroup::applyCompetitionForces() {
    // Группируем нейроны по орбитам
    std::vector<std::vector<int>> orbit_groups(5);
    for (int i = 0; i < size; i++) {
        orbit_groups[orbit_level[i]].push_back(i);
    }
    
    // Конкуренция только внутри одной орбиты
    for (int level = 0; level < 5; level++) {
        const auto& group = orbit_groups[level];
        for (size_t a = 0; a < group.size(); a++) {
            for (size_t b = a + 1; b < group.size(); b++) {
                int i = group[a];
                int j = group[b];
                Vec3 diff = pos[i] - pos[j];
                double dist = diff.norm();
                
                if (dist < OrbitalParams::COMPETITION_DISTANCE) {
                    double competition = OrbitalParams::COMPETITION_STRENGTH * 
                                        (1.0 + orbit_level[i] * 0.8);
                    double weight_factor = 1.0 - std::min(0.9, std::abs(W_intra[i][j]) * 2.0);
                    Vec3 repel = diff.normalized() * competition * weight_factor / (dist + 0.1);
                    force[i] += repel;
                    force[j] -= repel;
                }
            }
        }
    }
}
// ----------------------------------------------------------------------------
// Обновление орбитальных уровней - конкурентная версия с функциональной специализацией
// ----------------------------------------------------------------------------

void NeuralGroup::updateOrbitLevels() {
        // Предварительно вычисляем все нормы позиций (O(N))
    std::vector<double> norms(size);
    for (int i = 0; i < size; i++) {
        norms[i] = pos[i].norm();
    }

        // Используем предвычисленные нормы
    for (int i = 0; i < size; i++) {
        double activity = norms[i];
        double energy = getNeuronEnergy(i);
        double stability = getLocalStability(i);
        int old_level = orbit_level[i];
        
        // ===== 1. ФУНКЦИОНАЛЬНАЯ ВАЖНОСТЬ НЕЙРОНА =====
        // Определяем, насколько нейрон полезен для системы
        
        // 1.1 Связность (сколько сильных связей имеет нейрон)
        double connectivity = 0.0;
        for (int j = 0; j < size; j++) {
            if (i != j && std::abs(W_intra[i][j]) > 0.3) {
                connectivity += std::abs(W_intra[i][j]);
            }
        }
        connectivity = std::min(1.0, connectivity / 10.0);
        
        // 1.2 Уникальность (насколько нейрон отличается от соседей)
        double uniqueness = 0.0;
        int neighbor_count = 0;
        // Для уникальности используем предвычисленные нормы
        for (int j = 0; j < size; j++) {
            if (i != j) {
                double diff = std::abs(norms[i] - norms[j]);
                uniqueness += diff;
                neighbor_count++;
            }
        }
        if (neighbor_count > 0) uniqueness /= neighbor_count;
        uniqueness = std::min(1.0, uniqueness * 2.0);
        
        // 1.3 Вклад в гомеостаз (помогает ли нейрон поддерживать баланс)
        double homeostatic_value = 1.0 - std::abs(computeLocalEntropy(i) - homeo.target_entropy) / homeo.target_entropy;
        
        // 1.4 Адаптивность (быстро ли меняется)
        double adaptability = std::min(1.0, std::abs(orbital_energy[i] - energy) / 2.0);
        
        // ИТОГОВАЯ ВАЖНОСТЬ
        double functional_importance = (connectivity * 0.4 + 
                                        uniqueness * 0.2 + 
                                        homeostatic_value * 0.2 + 
                                        adaptability * 0.2);
        
        // ===== 2. ВЫЧИСЛЯЕМ ЦЕЛЕВУЮ ОРБИТУ НА ОСНОВЕ ФУНКЦИОНАЛЬНОЙ ВАЖНОСТИ =====
        // Важные нейроны поднимаются выше
        double target_level_raw = 4.0 * functional_importance;

        double normalized_energy = std::tanh(energy / 5.0);  // энергия 5 -> 0.99
        
        // Энергия даёт бонус
        double energy_factor = normalized_energy * 1.5;  // макс 1.5
        target_level_raw += energy_factor * 1.0;
        
        // Стабильность уже в [0,1], но можно усилить эффект
        double stability_factor = (stability - 0.5) * 2.0;  // -1..1
        // Стабильность помогает удержаться
        target_level_raw += stability_factor * 0.8;
        
        target_level_raw = std::clamp(target_level_raw, 0.0, 4.0);
        int target_level = static_cast<int>(std::round(target_level_raw));
        target_level = std::clamp(target_level, 0, 4);
        
        // ===== 3. КОНКУРЕНЦИЯ: НА ВЫСОКИХ ОРБИТАХ МЕСТО ОГРАНИЧЕНО =====
        // Вычисляем, сколько нейронов уже на целевой орбите
        int capacity = getOrbitCapacity(target_level);
        
        int current_on_target = 0;
        for (int k = 0; k < size; k++) {
            if (orbit_level[k] == target_level) current_on_target++;
        }
        
        // Если место занято, нужно вытеснить кого-то
        bool can_enter = (current_on_target < capacity);
        bool is_currently_on_target = (orbit_level[i] == target_level);
        
        int current_on_old = 0;
        for (int k = 0; k < size; k++) {
            if (orbit_level[k] == old_level) current_on_old++;
        }
        int old_capacity = getOrbitCapacity(old_level);

        bool has_to_leave = (current_on_old > old_capacity);

        if (target_level > old_level && (can_enter || has_to_leave)) {
            // ПОВЫШЕНИЕ - нужно доказать свою важность
            double importance_threshold = 0.6;
            if (functional_importance > importance_threshold && 
                time_on_orbit[i] > 10.0) {
                
                orbit_level[i] = target_level;
                target_radius[i] = getOrbitRadius(orbit_level[i]);
                time_on_orbit[i] = 0;
                
                // Бонус к массе и энергии
                mass[i] = std::min(mass[i] * 1.2, homeo.max_mass);
                orbital_energy[i] += 1.0;
                
                std::cout << " ⬆ Neuron " << i << " PROMOTED to orbit " << orbit_level[i] 
                          << " (importance: " << std::fixed << std::setprecision(2) << functional_importance
                          << ", conn: " << connectivity << ")" << std::endl;
            }
        }
        else if (target_level < old_level) {
            // ПОНИЖЕНИЕ - если важность упала, освобождаем место
            double importance_threshold = 0.3;
            if (functional_importance < importance_threshold || 
                (stability < 0.3 && time_on_orbit[i] > 50.0)) {
                
                int new_level = std::max(0, old_level - 1);
                orbit_level[i] = new_level;
                target_radius[i] = getOrbitRadius(orbit_level[i]);
                time_on_orbit[i] = 0;
                
                // Штраф к массе
                mass[i] = std::max(mass[i] * 0.9, homeo.min_mass);
                orbital_energy[i] *= 0.8;
                
                std::cout << " ⬇ Neuron " << i << " DEMOTED to orbit " << orbit_level[i] 
                          << " (importance: " << std::fixed << std::setprecision(2) << functional_importance
                          << ")" << std::endl;
            }
        }
        
        // ===== 4. ДЕГРАДАЦИЯ: НА НИЗКИХ ОРБИТАХ - БЫСТРОЕ ОБНУЛЕНИЕ =====
        if (orbit_level[i] <= 1 && time_on_orbit[i] > 200.0 && functional_importance < 0.15) {
            // Деградация только если очень долго на низкой орбите И почти бесполезен
            // Вместо resetNeuron, пробуем сначала ejectFromSingularity
            if (orbit_level[i] == 0) {
                ejectFromSingularity(i);
            } else {
                // Понижаем ещё на уровень
                orbit_level[i] = 0;
                target_radius[i] = getOrbitRadius(0);
                time_on_orbit[i] = 0;
            }
                if (orbit_level[i] >= 3 && functional_importance < 0.4 && time_on_orbit[i] > 50.0) {
                    // Принудительное понижение для "зазнавшихся" нейронов
                    orbit_level[i]--;
                    target_radius[i] = getOrbitRadius(orbit_level[i]);
                    time_on_orbit[i] = 0;
                    mass[i] = std::max(mass[i] * 0.85, homeo.min_mass);
                    std::cout << " Force demotion of lazy elite neuron " << i << std::endl;
                }
        }
        // ===== 5. РАСПРЕДЕЛЕНИЕ ПО ФУНКЦИЯМ =====
        // На основе орбиты назначаем специализацию
        if (orbit_level[i] >= 3) {
            neuron_specialization[i] = "metacognition";
        } else if (orbit_level[i] == 2) {
            neuron_specialization[i] = "coordination";
        } else if (orbit_level[i] == 1) {
            neuron_specialization[i] = "fast_response";
        } else {
            neuron_specialization[i] = "reserve";
        }
        
        // Обновляем время на орбите
        if (old_level != orbit_level[i]) {
            time_on_orbit[i] = 0;
        } else {
            time_on_orbit[i] += dt;
        }
    }
    
    // ===== 7. ПЕРИОДИЧЕСКАЯ ПЕРЕБАЛАНСИРОВКА ПОПУЛЯЦИИ =====
    if (step_counter_ % 1000 == 0) {
        rebalanceOrbits(); 
    }
    // ===== НОВОЕ: заморозка стабильных кластеров =====
    for (int i = 0; i < size; i++) {
        if (orbit_level[i] >= 3 && time_on_orbit[i] > 200.0) {
            // Находим всех нейронов на этой орбите
            std::vector<int> cluster;
            for (int j = 0; j < size; j++) {
                if (orbit_level[j] == orbit_level[i] && 
                    (pos[i] - pos[j]).norm() < OrbitalParams::COMPETITION_DISTANCE * 2.0) {
                    cluster.push_back(j);
                }
            }
            
            // Если кластер стабилен (все связи сильные)
            bool stable = true;
            for (int a : cluster) {
                for (int b : cluster) {
                    if (a != b && std::abs(W_intra[a][b]) < 0.3) {
                        stable = false;
                        break;
                    }
                }
                if (!stable) break;
            }
            
            if (stable && cluster.size() >= 3) {
                // Замораживаем все связи в кластере
                for (int a : cluster) {
                    for (int b : cluster) {
                        if (a != b && W_intra[a][b] > 0) {
                            // Запрещаем изменение
                            W_intra[a][b] = std::min(1.0, W_intra[a][b] * 1.01);
                        }
                    }
                }
                
                // Также запрещаем туннелирование для этого нейрона
                wave_amplitude[i] = std::min<double>(wave_amplitude[i], 1.2);
            }
        }
    }
}

// ============================================================================
// Проверка падения в сингулярность - ИСПРАВЛЕННАЯ ВЕРСИЯ
// ============================================================================

bool NeuralGroup::checkForSingularity(int i) {
    double r = pos[i].norm();
    double energy = getNeuronEnergy(i);
    
    auto mode = getCurrentMode();
    const auto& survival = getSurvivalParams(mode);
    
    // Радиус сингулярности (зависит от режима)
    double singularity_radius = OrbitalParams::SINGULARITY_RADIUS;
    if (mode == OperatingMode::NORMAL) {
        singularity_radius *= 0.3;
    }
    
    // Определяем время коллапса в зависимости от режима ИСПОЛЬЗУЯ КОНСТАНТЫ ИЗ DynamicParams
    double collapse_time_seconds;
    if (mode == OperatingMode::TRAINING) {
        collapse_time_seconds = DynamicParams::TRAINING_COLLAPSE_TIME;
    } else if (mode == OperatingMode::IDLE) {
        collapse_time_seconds = DynamicParams::IDLE_COLLAPSE_TIME;
    } else { // NORMAL и SLEEP
        collapse_time_seconds = DynamicParams::NORMAL_COLLAPSE_TIME;
    }
    
    // Проверяем, не слишком ли низкая энергия и слишком ли близко к центру
    bool energy_collapse = (energy < OrbitalParams::COLLAPSE_ENERGY && 
                            time_on_orbit[i] > collapse_time_seconds);
    bool radius_collapse = (r < singularity_radius);
    
    // Если условия коллапса выполнены
    if (radius_collapse || energy_collapse) {
        // Увеличиваем счетчик выбросов
        ejection_count_++;
        
        // Если есть авто-оживление - просто выбрасываем
        if (survival.auto_revive) {
            ejectFromSingularity(i);
            return true;
        }
        
        // Иначе обычный ресет (но с выбрасыванием)
        static std::mt19937 rng(std::random_device{}());
        resetNeuron(i, rng);
        rebirth_count[i]++;
        return true;
    }
    
    return false;
}

// ----------------------------------------------------------------------------
// НОВЫЙ МЕТОД: Выбрасывание из сингулярности на 2-3 орбиты
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// УСИЛЕННОЕ выбрасывание из сингулярности на 2-3 орбиты
// ----------------------------------------------------------------------------

void NeuralGroup::ejectFromSingularity(int i) {
    static std::mt19937 rng(std::random_device{}());
    
    // Выбираем целевой уровень орбиты: 2 или 3
    std::uniform_int_distribution<int> orbit_dist(2, 3);
    int target_level = orbit_dist(rng);
    
    // Обновляем орбитальный уровень
    orbit_level[i] = target_level;
    target_radius[i] = getOrbitRadius(target_level);
    
    // УВЕЛИЧИВАЕМ радиус выброса (подальше от центра)
    double r_scale = 1.2 + 0.5 * (rng() % 100) / 100.0;  // было 0.9+0.2
    double new_radius = target_radius[i] * r_scale;
    
    // Новая позиция
    pos[i] = Vec3::randomOnSphere(rng) * new_radius;
    
    // УСИЛЕННАЯ скорость выброса
    Vec3 radial = pos[i].normalized();
    Vec3 tangent1 = Vec3::cross(radial, Vec3(0, 1, 0));
    if (tangent1.norm() < 0.1) {
        tangent1 = Vec3::cross(radial, Vec3(1, 0, 0));
    }
    Vec3 tangent2 = Vec3::cross(radial, tangent1);
    
    std::uniform_real_distribution<double> angle_dist(0, 2 * M_PI);
    double angle = angle_dist(rng);
    Vec3 direction = (tangent1 * std::cos(angle) + tangent2 * std::sin(angle)).normalized();
    
    // СИЛЬНЫЙ выброс
    double ejection_strength = 1.5 + target_level * 0.5;  // level 2: 2.5, level 3: 3.0
    vel[i] = direction * ejection_strength;
    
    // Добавляем радиальную компоненту для гарантированного удаления от центра
    vel[i] += radial * ejection_strength * 0.5;
    
    // Сбрасываем энергию и таймер
    orbital_energy[i] = getKineticEnergy(i) + getPotentialEnergy(i) + 2.0;
    time_on_orbit[i] = 0;
    
    // Масса восстанавливается
    mass[i] = 1.0 + target_level * 0.4;
    
    // Частично восстанавливаем связи
    for (int j = 0; j < size; j++) {
        if (i != j) {
            // Восстанавливаем примерно половину от среднего веса
            double avg_weight = 0.0;
            int count = 0;
            for (int k = 0; k < size; k++) {
                if (k != i && k != j && std::abs(W_intra[j][k]) > 0.01) {
                    avg_weight += std::abs(W_intra[j][k]);
                    count++;
                }
            }
            if (count > 0) {
                avg_weight /= count;
                W_intra[i][j] = avg_weight * 0.7;  // больше сохранение
                W_intra[j][i] = avg_weight * 0.7;
            }
        }
    }
    
    // Волновая функция
    wave_amplitude[i] = 2.0 + target_level * 0.5;
    wave_phase[i] = 2.0 * M_PI * (rng() % 1000) / 1000.0;
    wave_function[i] = std::polar(wave_amplitude[i], wave_phase[i]);
    
    syncSynapsesFromWeights();
    
    // После установки новой орбиты
    updateWaveAmplitudeFromState();
    
    std::cout << "FORCE EJECT: Neuron " << i << " from singularity to orbit " 
              << target_level << " (r=" << new_radius << ", v=" << ejection_strength 
              << ", energy=" << orbital_energy[i] << ")" << std::endl;
}

// ----------------------------------------------------------------------------
// Перерождение нейрона - ИСПРАВЛЕНО
// ----------------------------------------------------------------------------

void NeuralGroup::resetNeuron(int i, std::mt19937& rng) {
    // При ресете тоже выбрасываем на орбиту 2-3, но с меньшей силой
    std::uniform_int_distribution<int> orbit_dist(2, 3);
    int target_level = orbit_dist(rng);
    
    orbit_level[i] = target_level;
    target_radius[i] = getOrbitRadius(target_level);
    
    // Немного дальше от центра
    double r_scale = 1.1 + 0.3 * (rng() % 100 / 100.0);
    double new_radius = target_radius[i] * r_scale;
    pos[i] = Vec3::randomOnSphere(rng) * new_radius;
    
    // Меньшая начальная скорость чем при eject
    Vec3 radial = pos[i].normalized();
    Vec3 tangent = Vec3::cross(radial, Vec3(0, 1, 0));
    if (tangent.norm() < 0.1) {
        tangent = Vec3::cross(radial, Vec3(1, 0, 0));
    }
    tangent = tangent.normalized();
    
    vel[i] = tangent * (0.3 + target_level * 0.2);  // level 2: 0.7, level 3: 0.9
    
    orbital_energy[i] = 0.5 + target_level * 0.3;
    time_on_orbit[i] = 0;
    
    // Масса чуть ниже чем при eject
    mass[i] = 1.0 + target_level * 0.2;  // level 2: 1.4, level 3: 1.6
    
    // Меньше забывание
    for (int j = 0; j < size; j++) {
        if (i != j) {
            W_intra[i][j] *= 0.7;
            W_intra[j][i] *= 0.7;
        }
    }
    
    wave_amplitude[i] = 1.0;
    wave_phase[i] = 2.0 * M_PI * (rng() % 1000) / 1000.0;
    wave_function[i] = std::polar(wave_amplitude[i], wave_phase[i]);
    
    syncSynapsesFromWeights();
    
    std::cout << "Neuron " << i << " reset to orbit " << target_level << std::endl;
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
        
        // Базовые массы для разных орбит
        double base_mass_by_orbit[] = {0.5, 0.8, 1.2, 2.0, 3.5};
        double base_mass = base_mass_by_orbit[orbit];
        
        // ВКЛАД ЭНЕРГИИ (но с насыщением) - ОСТАВЛЯЕМ ОДНО ОПРЕДЕЛЕНИЕ
        double energy_contribution = 0.5 * std::tanh(energy);
        
        // ВКЛАД ПАМЯТИ (тоже с насыщением) - ОСТАВЛЯЕМ ОДНО ОПРЕДЕЛЕНИЕ
        double memory_contribution = 0.3 * std::tanh(memory);
        
        // Бонус за время на орбите
        double time_bonus = 0.2 * std::min(1.0, time_on_orbit[i] / 500.0);
        
        // ЦЕЛЕВАЯ МАССА
        double target_mass = base_mass + energy_contribution + memory_contribution + time_bonus;
        
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
        double inertia = 0.93;  // масса меняется медленно
        mass[i] = mass[i] * inertia + target_mass * (1.0 - inertia);
        
        // ФИНАЛЬНОЕ КЛИППИРОВАНИЕ
        mass[i] = std::clamp(mass[i], homeo.min_mass, mass_cap);
        
        // ЕСЛИ МАССА СЛИШКОМ БЛИЗКА К КРИТИЧЕСКОЙ - АКТИВИРУЕМ ЗАЩИТУ
        if (mass[i] > mass_cap * 0.98) {
            activateMassShedding(i);
        }
    }
}

// НОВЫЙ МЕТОД: сброс избыточной массы
void NeuralGroup::activateMassShedding(int i) {
    double excess = mass[i] - computeMassCap(i) * 0.99;  // было 0.98
    if (excess > 0) {
        // ЕЩЕ МЕНЬШЕЕ СНИЖЕНИЕ МАССЫ
        mass[i] -= excess * 0.01;  // было 0.1
        
        // Выводим только при значительных изменениях
        if (step_counter_ % 5000 == 0 && excess > 0.5) {
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

void NeuralGroup::updateWaveFunction() {
    static std::mt19937 wave_rng(std::random_device{}());
    
    // 1. Временная эволюция (каждый нейрон имеет свою частоту)
    for (int i = 0; i < size; i++) {
        // Частота зависит от массы, орбиты и активности
        double activity = pos[i].norm();
        double omega = wave_frequency * (1.0 + mass[i] * 0.1 + orbit_level[i] * 0.2);
        
        // Активные нейроны имеют более высокую частоту
        if (activity > 0.5) {
            omega *= (1.0 + activity * 0.3);
        }
        
        // Изменение фазы
        wave_phase[i] += omega * dt;
        
        // НОРМАЛИЗАЦИЯ ФАЗЫ
        while (wave_phase[i] > 2.0 * M_PI) wave_phase[i] -= 2.0 * M_PI;
        while (wave_phase[i] < 0) wave_phase[i] += 2.0 * M_PI;
        
        // ===== ОСНОВНОЕ ИСПРАВЛЕНИЕ: АМПЛИТУДА =====
        // Амплитуда зависит от:
        // 1. Активности нейрона (расстояние от центра)
        // 2. Орбитального уровня
        // 3. Стабильности
        
        double activity_factor = std::min(1.0, activity / OrbitalParams::BASE_ORBIT);
        double orbit_factor = 0.5 + orbit_level[i] * 0.25;  // орбита 4 -> 1.5
        double stability_factor = getLocalStability(i);
        
        // Целевая амплитуда
        double target_amplitude = 0.5 + activity_factor * 1.0 + orbit_factor * 0.3 + stability_factor * 0.2;
        target_amplitude = std::clamp(target_amplitude, 0.2, 2.5);
        
        // Плавная адаптация к целевой амплитуде
        wave_amplitude[i] = wave_amplitude[i] * 0.98 + target_amplitude * 0.02;
        
        // Защита от NaN
        if (!std::isfinite(wave_amplitude[i])) {
            wave_amplitude[i] = 0.5;
        }
        
        // ===== ВЛИЯНИЕ ВОЛНЫ НА ЭНЕРГИЮ =====
        // Сильная волна может помочь выброситься из сингулярности
        if (orbit_level[i] == 0 && wave_amplitude[i] > 1.2) {
            // Волна даёт энергию для выброса
            orbital_energy[i] += wave_amplitude[i] * 0.05;
        }
        
        // Обновляем волновую функцию
        wave_function[i] = std::polar(wave_amplitude[i], wave_phase[i]);
    }
    
    // 2. Интерференция между соседними нейронами
    for (int i = 0; i < size; i++) {
        for (int j = i + 1; j < size; j++) {
            double dist = (pos[i] - pos[j]).norm();
            
            // Интерференция только на близком расстоянии
            if (dist < OrbitalParams::COMPETITION_DISTANCE * 2.0) {
                // Квантовая интерференция
                std::complex<double> interference = wave_function[i] + wave_function[j];
                double interference_strength = 0.05 / (1.0 + dist);
                
                // Взаимное влияние фаз
                double phase_diff = std::arg(wave_function[j]) - std::arg(wave_function[i]);
                
                // Конструктивная/деструктивная интерференция
                if (std::abs(phase_diff) < M_PI / 4) {
                    // Синфазные нейроны усиливают друг друга
                    wave_amplitude[i] = std::min(2.5, wave_amplitude[i] + interference_strength * 0.1);
                    wave_amplitude[j] = std::min(2.5, wave_amplitude[j] + interference_strength * 0.1);
                } else if (std::abs(phase_diff) > 3 * M_PI / 4) {
                    // Противофазные нейроны ослабляют друг друга
                    wave_amplitude[i] = std::max(0.2, wave_amplitude[i] - interference_strength * 0.05);
                    wave_amplitude[j] = std::max(0.2, wave_amplitude[j] - interference_strength * 0.05);
                }
                
                // Фазовая синхронизация
                double sync_strength = interference_strength * 0.1;
                wave_phase[i] += sync_strength * std::sin(wave_phase[j] - wave_phase[i]);
                wave_phase[j] += sync_strength * std::sin(wave_phase[i] - wave_phase[j]);
            }
        }
    }
    
    // 3. Волна влияет на физику нейрона (пондеромоторная сила)
    for (int i = 0; i < size; i++) {
        // Вычисляем градиент амплитуды
        Vec3 gradient(0, 0, 0);
        int neighbors = 0;
        
        for (int j = 0; j < size; j++) {
            if (i != j) {
                Vec3 diff = pos[j] - pos[i];
                double dist = diff.norm();
                if (dist > 1e-6) {
                    double amp_diff = wave_amplitude[j] - wave_amplitude[i];
                    Vec3 dir = diff / dist;
                    gradient = gradient + dir * amp_diff / (dist * dist);
                    neighbors++;
                }
            }
        }
        
        if (neighbors > 0) {
            gradient = gradient / neighbors;
            // Сила пропорциональна градиенту амплитуды
            Vec3 wave_force = gradient * wave_amplitude[i] * 0.02;
            force[i] += wave_force;
        }
        
        // Волна выталкивает из сингулярности
        double r = pos[i].norm();
        if (r < OrbitalParams::INNER_ORBIT * 0.5) {
            Vec3 radial = pos[i].normalized();
            double push_strength = wave_amplitude[i] * 0.01 * (1.0 - r / OrbitalParams::INNER_ORBIT);
            force[i] += radial * push_strength;
        }
        
        // Волна может помочь преодолеть барьер между орбитами
        if (orbit_level[i] < 4 && wave_amplitude[i] > 1.0) {
            double r_target = target_radius[i];
            double r_current = r;
            if (r_current < r_target - 0.1) {
                // Волна толкает вверх
                Vec3 radial = pos[i].normalized();
                force[i] += radial * wave_amplitude[i] * 0.005;
            }
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

            float anti_hebb = -0.001f * activity_i * activity_j;  // было -0.01
            delta += anti_hebb;

            // Модификаторы на основе орбит
            // Увелил разницу между орбитальными факторами
            float orbit_factor = 1.0f;
            if (orbit_level[i] > 3 && orbit_level[j] > 3) {
                orbit_factor = 4.0f;  // элитные связи
            } else if (orbit_level[i] > 2 && orbit_level[j] > 2) {
                orbit_factor = 2.0f;  // хорошие связи
            } else if (orbit_level[i] < 2 || orbit_level[j] < 2) {
                orbit_factor = 0.2f;  // слабые связи
            } else {
                orbit_factor = 0.8f;  // обычные связи
            }
            
            float elevation_factor = 1.0f + elevation_;
            float entropy_factor = 1.0f + static_cast<float>(entropy_error * 0.1);
            
            // НОВОЕ: фактор массы - тяжелые нейроны меняются медленнее
            float mass_factor = 1.0f / (1.0f + static_cast<float>((mass[i] + mass[j]) * 0.1f));
            
            syn.eligibility = syn.eligibility * params.eligibilityDecay + delta;
            syn.eligibility = std::clamp(syn.eligibility, -0.1f, 0.1f);  // ← ДОБАВИТЬ
            
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

            // ===== НОВОЕ: запись использования связи =====
            if (isActive && std::abs(weight_change) > 0.0001f) {
                recordConnectionUsage(i, j, reward > 0);
            }
        }
    }
    
    // НОВОЕ: периодически применяем затухание памяти
    if (step_counter_ % 100 == 0) {
        decayMemory();
    }
    // ===== НОВОЕ: затухание слабых связей =====
    if (step_counter_ % 50 == 0) {  // раз в 50 шагов
        for (int i = 0; i < size; i++) {
            for (int j = i + 1; j < size; j++) {
                // Если связь слабая и оба нейрона неактивны
                if (std::abs(W_intra[i][j]) < 0.05 && 
                    pos[i].norm() < 0.1 && pos[j].norm() < 0.1) {
                    
                    W_intra[i][j] *= 0.95;  // затухание
                    W_intra[j][i] = W_intra[i][j];
                }
            }
        }
        // Normalization?
        if (step_counter_ % 500 == 0) {
            for (int i = 0; i < size; i++) {
                double norm = 0.0;
                for (int j = 0; j < size; j++) {
                    norm += W_intra[i][j] * W_intra[i][j];
                }
                norm = std::sqrt(norm + 1e-6);
                for (int j = 0; j < size; j++) {
                    W_intra[i][j] /= norm;
                }
            }
            syncSynapsesFromWeights();
        }
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
            }
        }
    }
    
    // Нормализация — вынести за пределы внутреннего цикла
    for (int i = 0; i < size; i++) {
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

    // Обновляем кривизну только раз в 10 шагов
    if (step_counter_ - curvature_step_ < 10) {
        return cached_curvature_;
    }
    
    curvature_step_ = step_counter_;

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
    // КЭШИРУЕМ
    cached_curvature_ = triangles > 0 ? total_curvature / triangles : 0.0;
    return cached_curvature_;  // <-- ВАЖНО: возвращаем закэшированное значение
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
    
    // ===== НОВОЕ: фактор орбиты =====
    for (int i = 0; i < size; i++) {
        for (int j = i + 1; j < size; j++) {
            // Прямой доступ к W_intra вместо getSynapseIndex
            // Если оба нейрона на высоких орбитах - консолидация сильнее
            float orbit_factor = 1.0f;
            if (orbit_level[i] >= 3 && orbit_level[j] >= 3) {
                orbit_factor = 2.0f;
            } else if (orbit_level[i] <= 1 || orbit_level[j] <= 1) {
                orbit_factor = 0.3f;
            }
            
            // Получаем eligibility из синапса
            int syn_idx = getSynapseIndex(i, j);
            if (syn_idx >= 0 && syn_idx < synapses.size()) {
                auto& syn = synapses[syn_idx];
                float consolidation_factor = params.consolidationRate * globalImportance * 
                                            orbit_factor * static_cast<float>(entropy_factor);
                
                syn.weight += consolidation_factor * syn.eligibility;
                syn.eligibility *= 0.5f;
                syn.weight = std::clamp(syn.weight, -params.maxWeight, params.maxWeight);
                
                W_intra[i][j] = syn.weight;
                W_intra[j][i] = syn.weight;
            }
        }
    }
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

// Для Орбит
double NeuralGroup::computeConnectivity(int i) const {
    double connectivity = 0.0;
    for (int j = 0; j < size; ++j) {
        if (i != j && std::abs(W_intra[i][j]) > 0.3) {
            connectivity += std::abs(W_intra[i][j]);
        }
    }
    // Нормализация: делим на 10 (эмпирически подобрано)
    return std::min(1.0, connectivity / 10.0);
}

double NeuralGroup::computeUniqueness(int i) const {
    double uniqueness = 0.0;
    int neighbor_count = 0;
    for (int j = 0; j < size; ++j) {
        if (i != j) {
            double diff = std::abs(pos[i].norm() - pos[j].norm());
            uniqueness += diff;
            neighbor_count++;
        }
    }
    if (neighbor_count > 0) uniqueness /= neighbor_count;
    return std::min(1.0, uniqueness * 2.0);  // масштабирование
}

double NeuralGroup::computeHomeostaticValue() const {
    double current_entropy = computeEntropy();
    double target_entropy = computeEntropyTarget();
    double error = std::abs(current_entropy - target_entropy);
    return 1.0 - std::min(1.0, error / target_entropy);
}

double NeuralGroup::computeAdaptability(int i) const {
    // Адаптивность = скорость изменения энергии
    double delta = std::abs(orbital_energy[i] - getNeuronEnergy(i));
    return std::min(1.0, delta / 2.0);
}

void NeuralGroup::rebalanceOrbits() {
    // Подсчитываем текущее распределение
    std::vector<int> distribution(5, 0);
    for (int i = 0; i < size; ++i) {
        distribution[orbit_level[i]]++;
    }
    
    // Идеальные доли (20%, 30%, 30%, 15%, 5%)
    double ideal[] = {0.20, 0.30, 0.30, 0.15, 0.05};
    bool need_rebalance = false;
    
    for (int level = 0; level < 5; ++level) {
        double actual = static_cast<double>(distribution[level]) / size;
        if (std::abs(actual - ideal[level]) > 0.1) {
            need_rebalance = true;
            break;
        }
    }
    
    if (!need_rebalance) return;
    
    std::cout << " Rebalancing orbits..." << std::endl;
    
    // Для каждого уровня (сверху вниз) корректируем численность
    for (int level = 4; level >= 0; --level) {
        double actual = static_cast<double>(distribution[level]) / size;
        double target = ideal[level];
        
        if (actual > target) {
            // Слишком много нейронов на этом уровне – понижаем слабейших
            int excess = static_cast<int>((actual - target) * size);
            std::vector<std::pair<double, int>> candidates;
            for (int i = 0; i < size; ++i) {
                if (orbit_level[i] == level) {
                    double weakness = 1.0 / (getNeuronEnergy(i) + 0.1);
                    candidates.push_back({weakness, i});
                }
            }
            std::sort(candidates.begin(), candidates.end(), std::greater<>());
            
            for (int k = 0; k < std::min(excess, (int)candidates.size()); ++k) {
                int idx = candidates[k].second;
                if (orbit_level[idx] > 0) {
                    orbit_level[idx]--;
                    target_radius[idx] = getOrbitRadius(orbit_level[idx]);
                    time_on_orbit[idx] = 0;
                }
            }
        }
        else if (actual < target) {
            // Недостаточно нейронов – повышаем сильнейших с нижнего уровня
            int deficit = static_cast<int>((target - actual) * size);
            std::vector<std::pair<double, int>> candidates;
            int lower_level = std::max(0, level - 1);
            for (int i = 0; i < size; ++i) {
                if (orbit_level[i] == lower_level) {
                    double strength = getNeuronEnergy(i) * (1.0 + getLocalStability(i));
                    candidates.push_back({strength, i});
                }
            }
            std::sort(candidates.begin(), candidates.end(), std::greater<>());
            
            for (int k = 0; k < std::min(deficit, (int)candidates.size()); ++k) {
                int idx = candidates[k].second;
                if (orbit_level[idx] < 4) {
                    orbit_level[idx]++;
                    target_radius[idx] = getOrbitRadius(orbit_level[idx]);
                    time_on_orbit[idx] = 0;
                }
            }
        }
        
        // После изменений обновляем distribution для следующих уровней
        distribution.assign(5, 0);
        for (int i = 0; i < size; ++i) distribution[orbit_level[i]]++;
    }
}

// Добавить метод для вычисления локальной энтропии нейрона
double NeuralGroup::computeLocalEntropy(int i) const {
    double local_entropy = 0.0;
    int neighbors = 0;
    
    for (int j = 0; j < size; ++j) {
        if (i != j && std::abs(W_intra[i][j]) > 0.1) {
            double w = std::abs(W_intra[i][j]);
            local_entropy -= w * std::log(w + 1e-10);
            neighbors++;
        }
    }
    
    if (neighbors > 0) {
        local_entropy /= neighbors;
    }
    
    // Нормализуем
    return std::min(1.0, local_entropy / 2.0);
}

void NeuralGroup::pruneConnectionsByOrbit() {
    // Выполняем раз в N шагов
    if (step_counter_ % 100 != 0) return;
    
    static std::mt19937 rng(std::random_device{}());
    
    for (int i = 0; i < size; i++) {
        int orbit_i = orbit_level[i];
        
        for (int j = i + 1; j < size; j++) {
            int orbit_j = orbit_level[j];
            int min_orbit = std::min(orbit_i, orbit_j);
            int max_orbit = std::max(orbit_i, orbit_j);
            
            float weight = std::abs(W_intra[i][j]);
            int last_used = connection_last_used_step[i][j];
            int age = step_counter_ - last_used;
            float efficiency = getConnectionEfficiency(i, j);
            
            // ===== ПРАВИЛА ПРИВАТИЗАЦИИ =====
            
            // 1. ЭЛИТА (орбита 4) - НЕ РАЗРЫВАЕМ
            if (min_orbit >= 3 && max_orbit >= 3) {
                // Элитные связи защищены
                continue;
            }
            
            // 2. СВЯЗИ С ЭЛИТОЙ (орбита 4 + любая другая)
            if (max_orbit == 4 && min_orbit >= 2) {
                // Связи с элитой важны, но могут быть понижены
                if (efficiency < 0.3f && age > 1000) {
                    // Неэффективная связь с элитой
                    W_intra[i][j] *= 0.5;
                    std::cout << " Weakening elite connection: " << i << "-" << j 
                              << " (eff=" << efficiency << ")" << std::endl;
                }
                continue;
            }
            
            // 3. МЕНЕДЖЕРЫ (орбита 3) - проверяем эффективность
            if (min_orbit >= 2 && max_orbit <= 3) {
                if (efficiency > 0.7f && weight > 0.5f) {
                    // Эффективная связь → кандидат на повышение
                    // Помечаем для возможного повышения орбиты
                    if (orbit_i < 4 && orbit_j < 4) {
                        // Будет обработано в updateOrbitLevels
                        std::cout << " Candidate for promotion: " << i << "-" << j 
                                  << " (eff=" << efficiency << ")" << std::endl;
                    }
                } else if (efficiency < 0.3f && age > 500) {
                    // Неэффективная связь → ослабляем
                    W_intra[i][j] *= 0.7;
                    std::cout << " Weakening manager connection: " << i << "-" << j << std::endl;
                }
                continue;
            }
            
            // 4. РАБОЧИЕ (орбита 2) - активное тестирование
            if (min_orbit >= 1 && max_orbit <= 2) {
                if (efficiency > 0.6f && weight > 0.3f) {
                    // Перспективная связь → усиливаем
                    W_intra[i][j] *= 1.1;
                    W_intra[i][j] = std::min(W_intra[i][j], static_cast<double>(params.maxWeight));
                } else if (efficiency < 0.4f && age > 300) {
                    // Слабая связь → ослабляем
                    W_intra[i][j] *= 0.8;
                } else if (age > 1000 && weight < 0.1f) {
                    // Давно не использовалась и слабая → удаляем
                    W_intra[i][j] = 0.0;
                    std::cout << " Removing weak connection: " << i << "-" << j << std::endl;
                }
                continue;
            }
            
            // 5. КАНДИДАТЫ И СИНГУЛЯРНОСТЬ (орбита 0-1) - агрессивная чистка
            if (max_orbit <= 1) {
                // Связи на низких орбитах — кандидаты на удаление
                if (age > 200 || efficiency < 0.3f) {
                    W_intra[i][j] *= 0.5;
                    if (W_intra[i][j] < 0.01f) {
                        W_intra[i][j] = 0.0;
                        std::cout << " Removing low-orbit connection: " << i << "-" << j << std::endl;
                    }
                }
                continue;
            }
        }
    }
    
    syncSynapsesFromWeights();
}

void NeuralGroup::generateCuriosityConnections() {
    // Раз в 200 шагов
    if (step_counter_ % 200 != 0) return;
    
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> neuron_dist(0, size - 1);
    std::uniform_real_distribution<float> prob_dist(0, 1);
    
    // Находим элитные нейроны (база знаний)
    std::vector<int> elite_neurons;
    std::vector<int> exploratory_neurons;
    
    for (int i = 0; i < size; i++) {
        if (orbit_level[i] >= 3) {
            elite_neurons.push_back(i);
        } else if (orbit_level[i] <= 2) {
            exploratory_neurons.push_back(i);
        }
    }
    
    if (elite_neurons.empty() || exploratory_neurons.empty()) return;
    
    // Создаем новые связи между элитой и исследовательскими нейронами
    int new_connections = 0;
    int max_new = size / 10;  // не более 10% нейронов
    
    for (int elite : elite_neurons) {
        for (int exp : exploratory_neurons) {
            if (new_connections >= max_new) break;
            
            // Если связи нет или она очень слабая
            if (std::abs(W_intra[elite][exp]) < 0.05f) {
                // 5% шанс создать новую связь
                if (prob_dist(rng) < 0.05f) {
                    float new_weight = 0.1f + (rng() % 100) / 1000.0f;
                    W_intra[elite][exp] = new_weight;
                    W_intra[exp][elite] = new_weight;
                    new_connections++;
                    std::cout << "  🔬 Curiosity: new connection created between elite " 
                              << elite << " and explorer " << exp << std::endl;
                }
            }
        }
        if (new_connections >= max_new) break;
    }
    
    syncSynapsesFromWeights();
}

void NeuralGroup::protectCoreConcepts() {
    // Получаем ID важных смыслов из SemanticGraphDatabase
    // Для этого нужно передать ссылку на граф в NeuralGroup
    // Или сделать через callback
    
    static std::set<uint32_t> protected_concepts = {
        597,  // Mary
        43,   // creator
        142,  // khamit
        145,  // name
        516,  // name (второй ID)
        1,    // system_identity
        2,    // system_capability
        38,   // good
        49    // greeting
    };
    
    // Здесь нужно сопоставить концепты с нейронами
    // Временно пропускаем — можно добавить позже
}