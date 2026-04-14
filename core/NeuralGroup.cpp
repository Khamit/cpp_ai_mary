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
    // Инициализация ингибиторного поля
    inhibitor.resize(size, 0.0);
    // Небольшой случайный шум для инициализации паттернов
    for (int i = 0; i < size; ++i) {
        // цель: задать начальное нарушение симметрии
        inhibitor[i] = 0.3 + 0.4 * ((rng() % 100) / 100.0);
    }

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
            // Только туннелирование ВВЕРХ, и только когда энергия позволяет
            if (wave_amplitude[i] > 1.5 && orbit_level[i] < 4) {
                double energy = getNeuronEnergy(i);
                double required_energy = getOrbitRadius(orbit_level[i] + 1) * 0.5;
                if (energy > required_energy) {  // ← проверка энергии
                    static std::mt19937 rng(std::random_device{}());
                    if (std::uniform_real_distribution<>(0, 1)(rng) < 0.002) {  // 0.002 вместо 0.02
                        std::cout << "  Quantum tunneling! Neuron " << i 
                                << " jumps to orbit " << (orbit_level[i] + 1) << std::endl;
                        orbit_level[i]++;
                        target_radius[i] = getOrbitRadius(orbit_level[i]);
                    }
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
        updateHomeostasis();
    }

    if (step_counter_ % 50 == 0) {
        promoteNovelPatternsFromLowerOrbits();
    }

    if (step_counter_ % 50 == 0) {
        pruneConnectionsByOrbit();
    }

    if (step_counter_ % 50 == 0) {
        eliteToLettersFeedback();  // ← ДОБАВИТЬ
    }
    
    if (step_counter_ % 200 == 0) {
        generateCuriosityConnections();
    }

    if (step_counter_ % 100 == 0) {
        updateMetricTensor();
    }

    if (step_counter_ % 100 == 0) {
        checkForResourceExhaustion();
    }

    if (step_counter_ % 100 == 0) {
        propagateKnowledgeFromHigherOrbits();
    }
    
    if (step_counter_ % 1000 == 0) {
        logMassDistribution();
    }

    static std::mt19937 rng(std::random_device{}());
    
    int rebirth_this_step = 0;
    
    // ===== ОСНОВНОЙ ЦИКЛ ПО НЕЙРОНАМ =====
    for (int i = 0; i < size; ++i) {
        // Проверка на побег
        checkForEscape(i, rng);
        
        // Проверка на сингулярность
        bool was_reset = checkForSingularity(i);
        if (was_reset) {
            rebirth_this_step++;
        }
        
        // ===== НОВЫЙ БЛОК ЭКСПЕРИМЕНТОВ ДЛЯ НИЖНИХ ОРБИТ =====
        // !!! ВАЖНО: этот блок должен быть ВНУТРИ цикла for (int i = 0; i < size; ++i)
        if (step_counter_ % 50 == 0 && orbit_level[i] <= 1) {
            double novelty = computeNovelty(i);
            if (novelty > 0.7f) {
                learnSTDP(0.3f, step_counter_);
            }
        }
        
        // ===== ДОБАВЛЯЕМ ИССЛЕДОВАТЕЛЬСКИЙ ШУМ =====
        if (orbit_level[i] <= 2) {
            static std::mt19937 local_rng(std::random_device{}());
            std::uniform_real_distribution<double> prob(0, 1);
            if (prob(local_rng) < 0.01) {  // 1% шанс
                pos[i] += Vec3::randomOnSphere(local_rng) * 0.05;
            }
        }
    }  // ← ЗАКРЫВАЕМ ЦИКЛ for (int i = 0; i < size; ++i)
    
    // Увеличиваем время на орбите для всех нейронов
    for (int i = 0; i < size; ++i) {
        time_on_orbit[i] += dt;
    }
    
    // Стабилизация при массовом перерождении
    if (rebirth_this_step > size / 10) {
        std::cout << "Mass rebirth detected (" << rebirth_this_step 
                  << " neurons), activating gentle stabilization" << std::endl;
        
        for (int i = 0; i < size; ++i) {
            if (orbit_level[i] == 0) {
                Vec3 random_boost = Vec3::randomOnSphere(rng) * 0.1;
                vel[i] += random_boost;
            }
        }
    }
    
    // Массовый выброс из сингулярности
    if (step_counter_ % 50 == 0) {
        int ejected = 0;
        for (int i = 0; i < size; i++) {
            if (orbit_level[i] == 0) {
                double time_factor = std::min(1.0, time_on_orbit[i] / 100.0);
                double chance = 0.05 + time_factor * 0.2;
                std::uniform_real_distribution<> dis(0, 1);
                if (dis(rng) < chance) {
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

void NeuralGroup::saveBestPatternToMemory(float reward) {
    if (!memory_manager) return;
    if (reward < 0.7f) return;
    
    int best_neuron = -1;
    float best_activity = 0.0f;
    for (int i = 0; i < size; i++) {
        float activity = static_cast<float>(pos[i].norm());
        if (activity > best_activity) {
            best_activity = activity;
            best_neuron = i;
        }
    }
    
    if (best_neuron == -1) return;
    
    std::vector<float> pattern;
    for (int j = 0; j < size; j++) {
        if (j != best_neuron) {
            pattern.push_back(static_cast<float>(std::abs(W_intra[best_neuron][j])));
        }
    }
    
    // Исправлено: используем writeSTM вместо storeWithEntropy
    memory_manager->writeSTM(
        pattern,
        std::min(1.0f, reward),
        0.5f,  // entropy
        "best_pattern",
        step_counter_
    );
    
    std::cout << "Saved best pattern from neuron " << best_neuron 
              << " (reward: " << reward << ")" << std::endl;
}

void NeuralGroup::checkForEscape(int i, std::mt19937& rng) {
    double r = pos[i].norm();
    
    // НОВОЕ: мягкое ограничение вместо жёсткого
    const double SOFT_LIMIT = OrbitalParams::OUTER_ORBIT * 1.2;  // 5.4
    const double HARD_LIMIT = OrbitalParams::OUTER_ORBIT * 1.5;  // 6.75

    if (r > SOFT_LIMIT) {
        // Тормозим, но не сбрасываем
        double brake = 0.95;
        vel[i] *= brake;
        
        // Тянем обратно к центру
        Vec3 radial = pos[i].normalized();
        double pull = 0.01 * (r - SOFT_LIMIT);
        vel[i] -= radial * pull;
    }
    
    // Если нейрон слишком далеко - он "сбежал"
    if (r > HARD_LIMIT) {
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
// ОРБИТАЛЬНЫЕ СИЛЫ - ИСПРАВЛЕННАЯ ВЕРСИЯ
// ============================================================================

void NeuralGroup::computeDerivatives() {
    static constexpr int SEMANTIC_GROUPS_START = 16;
    static constexpr int NUM_SEMANTIC_GROUPS = 6;
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

    // внутригрупповая координация элиты:
    applyEliteCoordination();
    
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

        // ===== НОВОЕ: ДИФФУЗИЯ ПО ГРАФУ =====
    // Вычисляем диффузионный вклад для каждого нейрона
    std::vector<Vec3> diffusion_forces(size, Vec3(0,0,0));
    for (int i = 0; i < size; ++i) {
        Vec3 diffusion_sum(0,0,0);
        int neighbor_count = 0;
        
        // Ищем соседей на основе синаптических связей (граф)
        for (int j = 0; j < size; ++j) {
            if (i != j && std::abs(W_intra[i][j]) > 0.05) { // Если есть значимая связь
                // Диффузия: стремление к среднему положению соседей
                diffusion_sum += pos[j];
                neighbor_count++;
            }
        }
        
        if (neighbor_count > 0) {
            Vec3 target_center = diffusion_sum / static_cast<double>(neighbor_count);
            // Сила, тянущая нейрон к центру масс его соседей
            Vec3 diffusion_force = (target_center - pos[i]) * diffusion_strength;
            diffusion_forces[i] = diffusion_force;
        }
    }

    // ===== НОВОЕ: ИНГИБИТОРНОЕ ПОЛЕ =====
    // Обновляем ингибиторное поле и добавляем его влияние на силу
    for (int i = 0; i < size; ++i) {
        // 1. Ингибитор затухает
        inhibitor[i] *= inhibitor_decay;
        
        // 2. Ингибитор производится активностью нейрона (расстояние от центра)
        double activity = pos[i].norm();
        inhibitor[i] += activity * inhibitor_production * dt;
        
        // 3. Ограничиваем ингибитор разумными пределами
        inhibitor[i] = std::clamp(inhibitor[i], 0.0, 2.0);
        
        // 4. Сила отталкивания от ингибитора (радиально наружу, если ингибитор велик)
        Vec3 radial_dir = pos[i].normalized();
        // Защита от нулевого вектора
        if (pos[i].norm() < 1e-6) {
            static std::mt19937 local_rng(std::random_device{}());  // ← создаем локальный rng
            radial_dir = Vec3::randomOnSphere(local_rng);
        }
        
        double inhibition_strength = inhibitor[i] * inhibitor_influence;
        
        // Сила, выталкивающая нейрон наружу, пропорциональная ингибитору
        // Это создает конкуренцию и предотвращает скопление всех в одном месте
        Vec3 inhibition_force = radial_dir * inhibition_strength;
        
        // Добавляем обе новые силы
        force[i] += diffusion_forces[i] + inhibition_force;
    }
    
    
    // 5. ИСПРАВЛЕННОЕ ТРЕНИЕ - уменьшаем вблизи центра
    for (int i = 0; i < size; ++i) {
        double r = pos[i].norm();
        double inertia_factor = 1.0 / (1.0 + mass[i] * 0.5);
        
        // УМЕНЬШАЕМ ТРЕНИЕ В ЦЕНТРЕ
        // НИЗКИЕ ОРБИТЫ: минимальное трение = максимальная скорость
        double damping_factor = damping[i];
        if (r < OrbitalParams::INNER_ORBIT) {
            damping_factor *= 0.01;  // в 10 раз меньше трения в центре
        }
        // ВЫСОКИЕ ОРБИТЫ: высокое трение = стабильность
        else if (r > OrbitalParams::MIDDLE_ORBIT) {
            damping_factor *= 3.2;   // дополнительное торможение для элиты
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
                pauli_repulsion *= 16.0;  // усилено с 5.0 до 20.0
            }
            
            // ПРИМЕНЯЕМ ИМПУЛЬС, а не силу (для преодоления диссипации)
            double impulse_strength = pauli_repulsion * dt * 7.0;
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

// В NeuralGroup::computeDerivatives() - добавить
void NeuralGroup::applyEliteCoordination() {
    // Только для элитных нейронов (орбита 4)
    std::vector<int> elite = getNeuronsByOrbit(4);
    if (elite.size() < 2) return;
    
    // Элитные нейроны координируют свои "команды"
    for (size_t a = 0; a < elite.size(); a++) {
        for (size_t b = a + 1; b < elite.size(); b++) {
            int i = elite[a];
            int j = elite[b];
            
            double correlation = pos[i].norm() * pos[j].norm();
            
            // Сильная положительная связь = синхронизация
            if (W_intra[i][j] > 0.7 && correlation > 0.5) {
                // Тянутся друг к другу для координации
                Vec3 diff = pos[i] - pos[j];
                double dist = diff.norm();
                Vec3 coord_force = diff.normalized() * 0.05 * correlation / (dist + 0.1);
                force[i] -= coord_force;  // притягиваются
                force[j] += coord_force;
            }
            // Отрицательная связь = разделение ролей
            else if (W_intra[i][j] < -0.5) {
                // Отталкиваются, чтобы не мешать друг другу
                Vec3 diff = pos[i] - pos[j];
                double dist = diff.norm();
                if (dist < 0.5) {
                    Vec3 repel = diff.normalized() * 0.1 / (dist + 0.1);
                    force[i] += repel;
                    force[j] -= repel;
                }
            }
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
        static int wall_hits[32] = {0};

        if (r > outer_limit) {
            wall_hits[i]++;
            if (wall_hits[i] > 500) {  // слишком долго у стенки
                ejectFromSingularity(i);
                wall_hits[i] = 0;
            }
        } else {
            wall_hits[i] = 0;
        }
        
        if (r > outer_limit) {
            // Экспоненциально растущая сила
            double wall_strength = 2.4 * (r - outer_limit) * (r - outer_limit);
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
    // латеральное торможение между разными смыслами
    // Если два нейрона активируют разные смыслы, они тормозят друг друга
    for (int i = 0; i < size; i++) {
        for (int j = i + 1; j < size; j++) {
            // Если оба на высокой орбите и имеют разные специализации
            if (orbit_level[i] >= 2 && orbit_level[j] >= 2 &&
                neuron_specialization[i] != neuron_specialization[j]) {
                
                double dist = (pos[i] - pos[j]).norm();
                if (dist < OrbitalParams::COMPETITION_DISTANCE * 2.0) {
                    // Сильное отталкивание
                    Vec3 repel = (pos[i] - pos[j]).normalized() * 0.2 / (dist + 0.1);
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

        if (functional_importance < 0.01) {
            functional_importance = 0.05;  // минимальная базовая важность
        }
        
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

        // при повышении до орбиты 4
        if (target_level == 4 && old_level < 4 && functional_importance > 0.7) {
            // НЕ сохраняем в граф статически
            // Вместо этого проверяем, есть ли уже такой паттерн
            
            std::vector<float> pattern;
            for (int j = 0; j < size; j++) {
                if (std::abs(W_intra[i][j]) > 0.1) {
                    pattern.push_back(static_cast<float>(W_intra[i][j]));
                }
            }
            
            // Ищем похожий паттерн в существующих концептах
            bool similar_exists = false;
            bool matches_known = false;  // <-- ДОБАВИТЬ ЭТУ ПЕРЕМЕННУЮ
            
            if (memory_manager) {
                // Используем query вместо findSimilar
                auto matches = memory_manager->query(pattern, 1, step_counter_);
                bool similar_exists = !matches.empty();
                
                // Для поиска по энтропии — проходим по LTM вручную
                const auto& ltm = memory_manager->getLTM();
                std::vector<MemoryRecord> high_entropy_patterns;
                for (const auto& record : ltm) {
                    if (record.entropy > 1.5f) {
                        high_entropy_patterns.push_back(record);
                    }
                

                    // Используем high_entropy_patterns вместо findHighEntropyRecords
                    for (const auto& p : high_entropy_patterns) {
                        if (p.pattern.size() != static_cast<size_t>(size)) continue;
                        
                        // Сравниваем текущий паттерн нейрона i с сохранённым паттерном
                        double similarity = 0.0;
                        int matched = 0;
                        
                        for (int j = 0; j < size; j++) {
                            double weight = std::abs(W_intra[i][j]);
                            double pattern_val = std::abs(p.pattern[j]);
                            
                            // Евклидово расстояние для похожести
                            double diff = weight - pattern_val;
                            if (std::abs(diff) < 0.1) {  // порог схожести
                                similarity += 1.0 - std::abs(diff);
                                matched++;
                            }
                        }
                        
                        if (matched > 0) {
                            similarity /= matched;
                        }
                        
                        // Если паттерн похож (более 70% схожести) — подкрепляем
                        if (similarity > 0.7f && matched > size / 4) {
                            for (int j = 0; j < size; j++) {
                                double weight = std::abs(W_intra[i][j]);
                                double pattern_val = std::abs(p.pattern[j]);
                                double diff = weight - pattern_val;
                                
                                if (std::abs(diff) < 0.1) {
                                    // Усиливаем связь в направлении сохранённого паттерна
                                    W_intra[i][j] += (pattern_val - weight) * 0.05;
                                    W_intra[j][i] = W_intra[i][j];
                                }
                            }
                            
                            // Ограничиваем веса
                            double maxW = static_cast<double>(params.maxWeight);
                            for (int j = 0; j < size; j++) {
                                W_intra[i][j] = std::clamp(W_intra[i][j], -maxW, maxW);
                                W_intra[j][i] = W_intra[i][j];
                            }
                            
                            std::cout << "  Reinforced pattern from memory for neuron " << i 
                                    << " (similarity: " << similarity << ")" << std::endl;
                        }
                    }
                }
            
                if (!similar_exists) {
                    // Сохраняем через writeSTM вместо storeWithEntropy
                    memory_manager->writeSTM(
                        pattern,
                        0.7f,
                        static_cast<float>(computeLocalEntropy(i)),
                        "emergent_pattern",
                        step_counter_
                    );
                }
            }
            // ===== БАЛАНСИРОВКА ЭНТРОПИИ (внутри цикла) =====
            double current_entropy = computeEntropy();
            double target_entropy = computeOptimalEntropyBits();
            double entropy_ratio = current_entropy / (target_entropy + 1e-6);
            
            // Корректируем пороги на основе энтропии
            if (entropy_ratio > 1.2) {
                // Слишком хаотично → стимулируем понижение
                promotion_threshold_ = 0.8;
                demotion_threshold_ = 0.2;
            } else if (entropy_ratio < 0.8) {
                // Слишком упорядочено → стимулируем повышение
                promotion_threshold_ = 0.4;
                demotion_threshold_ = 0.6;
            } else {
                // Золотая середина
                promotion_threshold_ = 0.6;
                demotion_threshold_ = 0.4;
            }
            
            // Используем пороги при принятии решения о повышении/понижении
            if (target_level > old_level && functional_importance > promotion_threshold_) {
                // Повышение (ваш существующий код)
            }
            else if (target_level < old_level && functional_importance < demotion_threshold_) {
                // Понижение (ваш существующий код)
            }
        }
                        
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
    // ===== НОВОЕ: "ПРИВЫЧКА" ДЛЯ ЭЛИТЫ =====
    // ДОБАВИТЬ ЭТОТ БЛОК ЗДЕСЬ, ПОСЛЕ ЗАМОРОЗКИ КЛАСТЕРОВ
    for (int i = 0; i < size; i++) {
        if (orbit_level[i] >= 3 && time_on_orbit[i] > 500.0) {
            // Вычисляем functional_importance для нейрона i
            // (нужно переиспользовать логику из начала метода)
            double connectivity = 0.0;
            for (int j = 0; j < size; j++) {
                if (i != j && std::abs(W_intra[i][j]) > 0.3) {
                    connectivity += std::abs(W_intra[i][j]);
                }
            }
            connectivity = std::min(1.0, connectivity / 10.0);
            
            double uniqueness = 0.0;
            int neighbor_count = 0;
            for (int j = 0; j < size; j++) {
                if (i != j) {
                    double diff = std::abs(pos[i].norm() - pos[j].norm());
                    uniqueness += diff;
                    neighbor_count++;
                }
            }
            if (neighbor_count > 0) uniqueness /= neighbor_count;
            uniqueness = std::min(1.0, uniqueness * 2.0);
            
            double homeostatic_value = 1.0 - std::abs(computeLocalEntropy(i) - homeo.target_entropy) / homeo.target_entropy;
            double adaptability = std::min(1.0, std::abs(orbital_energy[i] - getNeuronEnergy(i)) / 2.0);
            
            double functional_importance = (connectivity * 0.4 + uniqueness * 0.2 + 
                                            homeostatic_value * 0.2 + adaptability * 0.2);
            
            if (functional_importance > 0.6) {
                // Нейрон на высокой орбите долго и стабилен — формируем "привычку"
                // Увеличиваем инерцию (массу) чтобы не падал
                mass[i] = std::min(mass[i] * 1.05, homeo.max_mass);
                
                // Усиливаем связи с нейронами на орбите 2 (исполнители)
                for (int j = 0; j < size; j++) {
                    if (orbit_level[j] == 2 && std::abs(W_intra[i][j]) < 0.5) {
                        W_intra[i][j] += 0.01;
                        W_intra[j][i] = W_intra[i][j];
                        
                        // Ограничиваем веса
                        double maxW = static_cast<double>(params.maxWeight);
                        W_intra[i][j] = std::clamp(W_intra[i][j], -maxW, maxW);
                        W_intra[j][i] = W_intra[i][j];
                    }
                }
                
                if (step_counter_ % 1000 == 0) {
                    std::cout << "  Neuron " << i << " developing HABIT on orbit " 
                              << orbit_level[i] << " (importance: " << functional_importance << ")" << std::endl;
                }
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
// УСИЛЕННОЕ выбрасывание из сингулярности на 2-3 орбиты
// ----------------------------------------------------------------------------

void NeuralGroup::ejectFromSingularity(int i) {
    static std::mt19937 rng(std::random_device{}());
    
    std::uniform_int_distribution<int> orbit_dist(2, 3);
    int target_level = orbit_dist(rng);
    
    orbit_level[i] = target_level;
    target_radius[i] = getOrbitRadius(target_level);
    
    // УМЕНЬШИТЬ радиус выброса
    double r_scale = 0.7 + 0.2 * (rng() % 100) / 100.0;  // было 0.9+0.2
    double new_radius = target_radius[i] * r_scale;       // будет 0.7-0.9 от целевой орбиты
    
    pos[i] = Vec3::randomOnSphere(rng) * new_radius;
    
    // УМЕНЬШИТЬ скорость выброса
    Vec3 radial = pos[i].normalized();
    Vec3 tangent1 = Vec3::cross(radial, Vec3(0, 1, 0));
    if (tangent1.norm() < 0.1) tangent1 = Vec3::cross(radial, Vec3(1, 0, 0));
    Vec3 tangent2 = Vec3::cross(radial, tangent1);
    
    std::uniform_real_distribution<double> angle_dist(0, 2 * M_PI);
    double angle = angle_dist(rng);
    Vec3 direction = (tangent1 * std::cos(angle) + tangent2 * std::sin(angle)).normalized();
    
    // УМЕНЬШИТЬ ejection_strength
    double ejection_strength = 0.5 + target_level * 0.2;
    vel[i] = direction * ejection_strength;
    vel[i] += radial * ejection_strength * 0.2;  // было 0.5
    
    orbital_energy[i] = getKineticEnergy(i) + getPotentialEnergy(i) + 1.8;
    time_on_orbit[i] = 0;
    mass[i] = 0.8 + target_level * 0.2;  // было 1.0 + target_level * 0.4
    
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
        double base_mass_by_orbit[] = {0.8, 1.2, 2.0, 3.0, 5.0};
        double base_mass = base_mass_by_orbit[orbit];
        
        // ВКЛАД ПАМЯТИ (увеличиваем!)
        double memory_contribution = 2.0 * std::tanh(memory);  // было 0.3
        
        // ВКЛАД ЭНЕРГИИ
        double energy_contribution = 0.8 * std::tanh(energy);
        
        // Бонус за время на орбите
        double time_bonus = 0.5 * std::min(1.0, time_on_orbit[i] / 500.0);
        
        // ЦЕЛЕВАЯ МАССА
        double target_mass = base_mass + energy_contribution + memory_contribution + time_bonus;
        
        // ПРИМЕНЯЕМ ФИЗИЧЕСКИЕ ОГРАНИЧЕНИЯ
        double mass_cap = computeMassCap(i);
        target_mass = std::min(target_mass, mass_cap);

        // Отладка
        if (i == 0 && step_counter_ % 1000 == 0) {
            std::cout << "Neuron " << i << ": memory=" << memory 
                      << ", contrib=" << memory_contribution << std::endl;
        }
        
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
        // Плавная адаптация
        double inertia = 0.97;  // было 0.93 — медленнее меняем
        mass[i] = mass[i] * inertia + target_mass * (1.0 - inertia);
        
        // Мягкое клиппирование
        if (mass[i] > mass_cap) {
            mass[i] = mass_cap * 0.95 + mass[i] * 0.05;  // плавное приближение
        }
        // финальное
        mass[i] = std::clamp(mass[i], homeo.min_mass, mass_cap);
        
        // ЕСЛИ МАССА СЛИШКОМ БЛИЗКА К КРИТИЧЕСКОЙ - АКТИВИРУЕМ ЗАЩИТУ
        if (mass[i] > mass_cap * 0.98) {
            activateMassShedding(i);
        }
        // ОТЛАДКА: выводим каждые 1000 шагов для нейрона 0
        if (i == 0 && step_counter_ % 1000 == 0) {
            std::cout << "Neuron 0: memory=" << memory 
                    << ", contrib=" << memory_contribution 
                    << ", target_mass=" << target_mass
                    << ", mass_cap=" << mass_cap
                    << ", final_mass=" << mass[i] << std::endl;
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

// Знания передача по орбите 

void NeuralGroup::relayLearning(int learner_idx, float reward, int step) {
    if (learner_idx < 0 || learner_idx >= size) return;
    
    // 1. Проверяем, нуждается ли нейрон в обучении
    double learner_activity = pos[learner_idx].norm();
    double learner_confidence = getNeuronEnergy(learner_idx);
    
    if (orbit_level[learner_idx] >= 2 && learner_confidence > 1.0) {
        return;
    }
    
    // 2. Ищем "учителей" на более высоких орбитах
    std::vector<std::pair<int, double>> teachers;
    
    for (int teacher = 0; teacher < size; ++teacher) {
        if (teacher == learner_idx) continue;
        if (orbit_level[teacher] <= orbit_level[learner_idx]) continue;
        
        double teacher_activity = pos[teacher].norm();
        double teacher_confidence = getNeuronEnergy(teacher);
        
        if (teacher_activity > 0.5 && teacher_confidence > 0.8) {
            double quality = (orbit_level[teacher] + 1) * teacher_activity * teacher_confidence;
            teachers.push_back({teacher, quality});
        }
    }
    
    if (teachers.empty()) return;
    
    // 3. Выбираем лучшего учителя
    auto best_teacher = std::max_element(teachers.begin(), teachers.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    
    int teacher_idx = best_teacher->first;
    
    // 4. Копируем "знания" — усиливаем связь с учителем
    double current_weight = std::abs(W_intra[learner_idx][teacher_idx]);
    double target_weight = std::min(1.0, current_weight + 0.1 * reward);
    double orbit_gap = orbit_level[teacher_idx] - orbit_level[learner_idx];
    double plasticity = std::min(1.0, orbit_gap * 0.3);
    double weight_change = (target_weight - current_weight) * plasticity;
    
    W_intra[learner_idx][teacher_idx] += weight_change;
    W_intra[teacher_idx][learner_idx] += weight_change;
    
    // 5. Ограничиваем веса (ИСПРАВЛЕНО)
    double maxW = static_cast<double>(params.maxWeight);
    W_intra[learner_idx][teacher_idx] = std::clamp(W_intra[learner_idx][teacher_idx], -maxW, maxW);
    W_intra[teacher_idx][learner_idx] = W_intra[learner_idx][teacher_idx];
    
    // 6. Если обучение успешно — повышаем шанс на повышение орбиты
    if (reward > 0.5f && orbit_level[learner_idx] < 4) {
        promotion_count[learner_idx]++;
        
        if (promotion_count[learner_idx] > 10 && time_on_orbit[learner_idx] > 20.0) {
            orbit_level[learner_idx]++;
            target_radius[learner_idx] = getOrbitRadius(orbit_level[learner_idx]);
            time_on_orbit[learner_idx] = 0;
            promotion_count[learner_idx] = 0;
        }
    }
    
    recordConnectionUsage(learner_idx, teacher_idx, reward > 0);
    syncSynapsesFromWeights();
}

void NeuralGroup::promoteNovelPatternsFromLowerOrbits() {
    // Собираем "интересные" паттерны с орбит 0-2
    std::vector<int> novel_neurons;
    
    for (int i = 0; i < size; i++) {
        if (orbit_level[i] <= 2) {
            double novelty = computeNovelty(i);
            double activity = pos[i].norm();
            
            // Если паттерн интересный и стабильный
            if (novelty > 0.7f && activity > 0.3f && time_on_orbit[i] > 50) {
                novel_neurons.push_back(i);
            }
        }
    }
    
    // Находим "элитные" нейроны, которые могут принять новое знание
    for (int elite : getNeuronsByOrbit(4)) {
        if (novel_neurons.empty()) break;
        
        // Элита может "впитать" новый паттерн
        int best_novel = selectBestNovelPattern(novel_neurons);
        
        // Создаем сильную связь между элитой и новым паттерном
        W_intra[elite][best_novel] = std::min(1.0, W_intra[elite][best_novel] + 0.3);
        
    }
}

// COMMAND
void NeuralGroup::eliteCommandLowerOrbits() {
    if (step_counter_ % 20 != 0) return;  // каждые 20 шагов
    
    std::vector<int> elite = getNeuronsByOrbit(4);
    if (elite.empty()) return;
    
    // Элита "голосует" за направление развития
    Vec3 collective_direction(0,0,0);
    for (int e : elite) {
        collective_direction += pos[e].normalized() * pos[e].norm();
    }
    collective_direction = collective_direction.normalized();
    
    // Применяем команду к нейронам на орбитах 1-2
    for (int i = 0; i < size; i++) {
        if (orbit_level[i] <= 2) {
            // Элита направляет развитие
            double influence = 0.02 * (1.0 - pos[i].norm() / OrbitalParams::OUTER_ORBIT);
            force[i] += collective_direction * influence;
        }
    }
}

// Эстафета
void NeuralGroup::propagateKnowledgeFromHigherOrbits() {
    if (step_counter_ % 50 != 0) return;  // чаще, было 100
    
    // Собираем "знания" с высоких орбит (3-4)
    std::vector<int> high_orbit_neurons;
    std::vector<double> high_orbit_activities;
    
    for (int i = 0; i < size; ++i) {
        if (orbit_level[i] >= 3 && getNeuronEnergy(i) > 0.5) {
            high_orbit_neurons.push_back(i);
            high_orbit_activities.push_back(pos[i].norm());
        }
    }
    
    if (high_orbit_neurons.empty()) return;
    
    // Передаём знания на нижние орбиты (1-2)
    for (int learner = 0; learner < size; ++learner) {
        if (orbit_level[learner] >= 2) continue;
        
        // Находим ближайшего учителя
        int best_teacher = -1;
        double best_similarity = -1.0;
        
        for (size_t t = 0; t < high_orbit_neurons.size(); ++t) {
            int teacher = high_orbit_neurons[t];
            double similarity = 1.0 - std::abs(pos[learner].norm() - high_orbit_activities[t]);
            
            if (similarity > best_similarity) {
                best_similarity = similarity;
                best_teacher = teacher;
            }
        }
        
        if (best_teacher != -1 && best_similarity > 0.3) {  // снижено с 0.2
            // УСИЛЕННАЯ передача знаний
            double boost = 0.1;  // было 0.03
            W_intra[learner][best_teacher] = std::min(1.0, 
                W_intra[learner][best_teacher] + boost);
            W_intra[best_teacher][learner] = W_intra[learner][best_teacher];
            
            // Также влияем на позицию learner'а
            Vec3 direction = pos[best_teacher].normalized();
            force[learner] += direction * 0.05;
        }
    }
    
    syncSynapsesFromWeights();
}

// Нейроны падают в сингулярность только при низкой энергии/радиусе, но НЕ при перерасходе памяти/CPU.
bool NeuralGroup::checkForResourceExhaustion() {
    // ВАЖНО: functional_importance должен вычисляться для каждого нейрона
    // Это временное решение — нужно передавать извне
    static int check_counter = 0;
    check_counter++;
    
    if (check_counter % 100 == 0) {  // проверяем раз в 100 шагов
        // Считаем среднюю активность
        double avg_activity = getAverageActivity();
        
        // Если система перегружена (высокая активность)
        if (avg_activity > 0.9f) {
            // Принудительно снижаем активность малозначимых нейронов
            int degraded = 0;
            for (int i = 0; i < size; i++) {
                // Нейроны на низких орбитах и с низкой связностью
                if (orbit_level[i] <= 1 && getMemoryStrength(i) < 0.3) {
                    orbit_level[i] = 0;  // в сингулярность
                    degraded++;
                }
            }
            if (degraded > 0) {
                std::cout << "  Resource exhaustion: degraded " << degraded << " neurons" << std::endl;
            }
            return true;
        }
    }
    return false;
}
// Элита Обучает
// Добавьте этот метод в NeuralGroup.hpp
void NeuralGroup::eliteToLettersFeedback() {
    if (step_counter_ % 30 != 0) return;  // каждые 30 шагов
    
    std::vector<int> elite = getNeuronsByOrbit(4);
    if (elite.empty()) return;
    
    // Элита вычисляет "что важно" для текущего контекста
    Vec3 collective_intent(0, 0, 0);
    double collective_energy = 0.0;
    
    for (int e : elite) {
        collective_intent += pos[e].normalized();
        collective_energy += getNeuronEnergy(e);
    }
    collective_intent = collective_intent.normalized();
    float intent_strength = std::min(1.0f, static_cast<float>(collective_energy) / 20.0f);
    
    // ===== 1. ВЛИЯНИЕ НА БУКВЫ (орбита 0-1) =====
    for (int i = 0; i < size; i++) {
        if (orbit_level[i] <= 1) {  // буквенные/сигнальные нейроны
            // Вычисляем, насколько этот нейрон релевантен текущему намерению элиты
            double alignment = std::abs(Vec3::dot(pos[i].normalized(), collective_intent));
            
            if (alignment > 0.7) {
                // Элита усиливает релевантные буквенные нейроны
                double boost = 0.05 * intent_strength * alignment;
                force[i] += collective_intent * boost;
                
                // Укрепляем связь элита→буква
                for (int e : elite) {
                    if (std::abs(W_intra[e][i]) < 0.8) {
                        W_intra[e][i] += boost * 0.1;
                        W_intra[i][e] = W_intra[e][i];
                    }
                }
                
                if (step_counter_ % 500 == 0 && boost > 0.05) {  // было 0.01
                    std::cout << "  Elite→Letter feedback: neuron " << i 
                            << " boosted by " << boost << std::endl;
                }
            }
        }
    }
    
    // ===== 2. ВЛИЯНИЕ НА СЛОВА (орбита 2) =====
    for (int i = 0; i < size; i++) {
        if (orbit_level[i] == 2) {  // слова
            double alignment = std::abs(Vec3::dot(pos[i].normalized(), collective_intent));
            
            // Элита предпочитает определённые слова
            if (alignment > 0.5) {
                // Увеличиваем энергию "предпочтительных" слов
                orbital_energy[i] += 0.02 * intent_strength * alignment;
                
                // Укрепляем связь с элитой
                for (int e : elite) {
                    if (W_intra[e][i] < 0.6) {
                        W_intra[e][i] += 0.02 * intent_strength;
                        W_intra[i][e] = W_intra[e][i];
                    }
                }
            }
        }
    }
    
    // ===== 3. АКТИВАЦИЯ ПАТТЕРНОВ КОТОРЫЕ ЭЛИТА "ХОЧЕТ" ВИДЕТЬ =====
    for (int i = 0; i < size; i++) {
        if (orbit_level[i] == 3) {  // координация/паттерны
            double relevance = computeRelevanceToElite(i, elite, collective_intent);
            
            if (relevance > 0.6) {
                // Элита "одобряет" этот паттерн
                double approval = 0.03 * relevance;
                mass[i] = std::min(mass[i] * (1.0 + approval), homeo.max_mass);
                
                // Повышаем шанс на promotion
                promotion_count[i]++;
                if (promotion_count[i] > 15 && orbit_level[i] < 4) {
                    orbit_level[i]++;
                    target_radius[i] = getOrbitRadius(orbit_level[i]);
                    std::cout << "  Elite approved pattern " << i 
                              << " promoted to orbit " << orbit_level[i] << std::endl;
                }
            }
        }
    }
    
    syncSynapsesFromWeights();
}

// Вспомогательный метод для вычисления релевантности паттерна элите
double NeuralGroup::computeRelevanceToElite(int neuron_idx, 
                                             const std::vector<int>& elite,
                                             const Vec3& collective_intent) const {
    if (neuron_idx < 0 || neuron_idx >= size) return 0.0;
    
    double relevance = 0.0;
    int connections_to_elite = 0;
    
    // 1. Насколько сильно связан с элитой
    for (int e : elite) {
        double weight = std::abs(W_intra[neuron_idx][e]);
        if (weight > 0.1) {
            relevance += weight;
            connections_to_elite++;
        }
    }
    
    if (connections_to_elite > 0) {
        relevance /= connections_to_elite;
    }
    
    // 2. Насколько его позиция совпадает с коллективным намерением
    double alignment = std::abs(Vec3::dot(pos[neuron_idx].normalized(), collective_intent));
    relevance = relevance * 0.6 + alignment * 0.4;
    
    return std::min(1.0, relevance);
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
            float normalized_reward = std::min(1.0f, reward);
            // Базовая награда за активность (чтобы не умирали)
            if (isActive) {
                //float survival_reward = 0.01f;  // маленькая, но постоянная
                float energy_penalty = -0.1f * current_entropy;
                float final_reward = normalized_reward + energy_penalty;
                weight_change = params.stdpRate * final_reward * syn.eligibility * 
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
    // НОВОЕ: эстафетное обучение для слабых нейронов
    if (reward > 0.2f) {  // только при положительном подкреплении
        for (int i = 0; i < size; ++i) {
            double activity = pos[i].norm();
            double energy = getNeuronEnergy(i);
            
            // Нейроны на низких орбитах с низкой уверенностью
            if (orbit_level[i] <= 1 && energy < 0.5 && activity < 0.3) {
                relayLearning(i, reward, currentStep);
            }
            
            // Также учим нейроны, которые недавно переродились
            if (time_on_orbit[i] < 50 && orbit_level[i] <= 2) {
                relayLearning(i, reward, currentStep);
            }
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
    
    double H = 0.0;
    double max_entropy = std::log2(static_cast<double>(BINS));  // ← log2 для битов
    
    for (int count : hist_azimuth) {
        if (count > 0) {
            double p = static_cast<double>(count) / size;
            H -= p * std::log2(p);  // ← биты!
        }
    }
    for (int count : hist_polar) {
        if (count > 0) {
            double p = static_cast<double>(count) / size;
            H -= p * std::log2(p);  // ← биты!
        }
    }
    
    // Нормализация [0, 1]
    return std::clamp(H / (2.0 * max_entropy), 0.0, 1.0);
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
    static int rebalance_counter = 0;
    if (need_rebalance && ++rebalance_counter % 10 == 0) {
        std::cout << " Rebalancing orbits (distribution changed)" << std::endl;
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
    
    // === НОВОЕ: флаг для разреженного логирования ===
    static int log_counter = 0;
    bool should_log = (++log_counter % 10 == 0); // Логируем только каждый 10-й раз
    
    static std::mt19937 rng(std::random_device{}());
    
    // Счётчики для итогового отчёта
    int candidates = 0;
    int weakened_elite = 0;
    int weakened_manager = 0;
    int removed_weak = 0;
    int removed_low = 0;
    
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
                continue;
            }
            
            // 2. СВЯЗИ С ЭЛИТОЙ (орбита 4 + любая другая)
            if (max_orbit == 4 && min_orbit >= 2) {
                if (efficiency < 0.3f && age > 1000) {
                    W_intra[i][j] *= 0.5;
                    weakened_elite++;
                    if (should_log) {
                        std::cout << " Weakening elite connection: " << i << "-" << j << std::endl;
                    }
                }
                continue;
            }
            
            // 3. МЕНЕДЖЕРЫ (орбита 3) - проверяем эффективность
            if (min_orbit >= 2 && max_orbit <= 3) {
                if (efficiency > 0.7f && weight > 0.5f) {
                    candidates++;
                    if (should_log && candidates <= 10) { // Только первые 10 кандидатов
                        std::cout << " Candidate for promotion: " << i << "-" << j << std::endl;
                    }
                } else if (efficiency < 0.3f && age > 500) {
                    W_intra[i][j] *= 0.7;
                    weakened_manager++;
                    if (should_log) {
                        std::cout << " Weakening manager connection: " << i << "-" << j << std::endl;
                    }
                }
                continue;
            }
            
            // 4. РАБОЧИЕ (орбита 2) - активное тестирование
            if (min_orbit >= 1 && max_orbit <= 2) {
                if (efficiency > 0.6f && weight > 0.3f) {
                    W_intra[i][j] *= 1.1;
                    W_intra[i][j] = std::min(W_intra[i][j], static_cast<double>(params.maxWeight));
                } else if (efficiency < 0.4f && age > 300) {
                    W_intra[i][j] *= 0.8;
                } else if (age > 400 && weight < 0.03f) {
                    W_intra[i][j] = 0.0;
                    removed_weak++;
                    if (should_log) {
                        std::cout << " Removing weak connection: " << i << "-" << j << std::endl;
                    }
                }
                continue;
            }
            
            // 5. КАНДИДАТЫ И СИНГУЛЯРНОСТЬ (орбита 0-1) - агрессивная чистка
            if (max_orbit <= 1) {
                if (age > 200 || efficiency < 0.3f) {
                    W_intra[i][j] *= 0.5;
                    if (W_intra[i][j] < 0.01f) {
                        W_intra[i][j] = 0.0;
                        removed_low++;
                        if (should_log) {
                            std::cout << " Removing low-orbit connection: " << i << "-" << j << std::endl;
                        }
                    }
                }
                continue;
            }
        }
    }
    
    // === НОВОЕ: итоговый отчёт раз в 1000 шагов ===
    if (step_counter_ % 1000 == 0 && (candidates > 0 || weakened_elite > 0 || removed_weak > 0)) {
        std::cout << " [Prune] Candidates: " << candidates 
                  << ", Elite weakened: " << weakened_elite
                  << ", Manager weakened: " << weakened_manager
                  << ", Weak removed: " << removed_weak
                  << ", Low removed: " << removed_low << std::endl;
    }
    
    syncSynapsesFromWeights();
}

void NeuralGroup::performExperiments() {
    if (step_counter_ % 300 != 0) return;
    
    // Нейроны на низких орбитах "экспериментируют"
    for (int i = 0; i < size; i++) {
        if (orbit_level[i] <= 1) {
            // Случайно изменяем позицию (эксперимент)
            static std::mt19937 rng(std::random_device{}());
            Vec3 random_shift = Vec3::randomOnSphere(rng) * 0.1;
            pos[i] += random_shift;
            
            // Логируем успех/неудачу эксперимента
            double old_energy = orbital_energy[i];
            double new_energy = getNeuronEnergy(i);
            
            if (new_energy > old_energy) {
                
                float reward = (new_energy - old_energy) / old_energy;
                learnSTDP(reward * 2.0f, step_counter_);  // было просто 1.05
                std::cout << "Successful experiment by neuron " << i << std::endl;
            }
            if (orbit_level[i] <= 2 && random() < 0.01) {
                pos[i] += Vec3::randomOnSphere(rng) * 0.05;
            }
        }
    }
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
    int max_new = size / 23;  // не более 10% нейронов
    
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
                }
            }
        }
        if (new_connections >= max_new) break;
    }
    // Один раз в 1000 шагов
    if (step_counter_ % 1000 == 0 && new_connections > 0) {
        std::cout << " Curiosity: created " << new_connections << " new connections" << std::endl;
    }

    performExperiments();
    
    syncSynapsesFromWeights();
}

// ============================================================================
// ВЫЧИСЛЕНИЕ НОВИЗНЫ НЕЙРОНА
// ============================================================================

double NeuralGroup::computeNovelty(int i) const {
    if (i < 0 || i >= size) return 0.0;
    
    double novelty = 0.0;
    
    // 1. Насколько позиция отличается от среднего положения нейронов на той же орбите
    std::vector<int> same_orbit = getNeuronsByOrbit(orbit_level[i]);
    if (same_orbit.size() > 1) {
        Vec3 avg_pos(0,0,0);
        for (int idx : same_orbit) {
            avg_pos += pos[idx];
        }
        avg_pos = avg_pos / same_orbit.size();
        
        double distance_to_avg = (pos[i] - avg_pos).norm();
        novelty += distance_to_avg / OrbitalParams::OUTER_ORBIT;
    }
    
    // 2. Насколько уникальны связи этого нейрона
    double connection_uniqueness = 0.0;
    int unique_connections = 0;
    
    for (int j = 0; j < size; j++) {
        if (i != j && std::abs(W_intra[i][j]) > 0.1) {
            // Проверяем, есть ли у других нейронов похожие связи
            bool found_similar = false;
            for (int k = 0; k < size; k++) {
                if (k != i && k != j && std::abs(W_intra[k][j]) > 0.1) {
                    double similarity = 1.0 - std::abs(W_intra[i][j] - W_intra[k][j]);
                    if (similarity > 0.8) {
                        found_similar = true;
                        break;
                    }
                }
            }
            if (!found_similar) {
                unique_connections++;
                connection_uniqueness += std::abs(W_intra[i][j]);
            }
        }
    }
    
    if (unique_connections > 0) {
        novelty += connection_uniqueness / unique_connections * 0.3;
    }
    
    // 3. Новизна энергии (нестандартная энергия для своей орбиты)
    double expected_energy = 1.0 + orbit_level[i] * 0.5;
    double energy_deviation = std::abs(orbital_energy[i] - expected_energy) / expected_energy;
    novelty += std::min(1.0, energy_deviation) * 0.2;
    
    // 4. Новизна волновой функции
    if (orbit_level[i] < 3) {
        double avg_amplitude = 0.0;
        int count = 0;
        for (int j : same_orbit) {
            if (j != i) {
                avg_amplitude += wave_amplitude[j];
                count++;
            }
        }
        if (count > 0) {
            avg_amplitude /= count;
            double amp_deviation = std::abs(wave_amplitude[i] - avg_amplitude) / (avg_amplitude + 0.1);
            novelty += std::min(1.0, amp_deviation) * 0.2;
        }
    }
    
    // Нормализуем новизну
    return std::min(1.0, novelty);
}

// ============================================================================
// ВЫБОР ЛУЧШЕГО НОВОГО ПАТТЕРНА
// ============================================================================

int NeuralGroup::selectBestNovelPattern(const std::vector<int>& novel_neurons) {
    if (novel_neurons.empty()) return -1;
    
    int best_idx = novel_neurons[0];
    double best_score = -1.0;
    
    for (int neuron : novel_neurons) {
        double score = 0.0;
        
        // 1. Новизна
        score += computeNovelty(neuron) * 0.4;
        
        // 2. Активность
        double activity = pos[neuron].norm();
        score += activity * 0.3;
        
        // 3. Связность (сколько уже есть связей)
        double connectivity = 0.0;
        for (int j = 0; j < size; j++) {
            if (neuron != j && std::abs(W_intra[neuron][j]) > 0.1) {
                connectivity++;
            }
        }
        score += (connectivity / size) * 0.2;
        
        // 4. Стабильность (долго ли на орбите)
        score += std::min(1.0, time_on_orbit[neuron] / 200.0) * 0.1;
        
        if (score > best_score) {
            best_score = score;
            best_idx = neuron;
        }
    }
    
    return best_idx;
}