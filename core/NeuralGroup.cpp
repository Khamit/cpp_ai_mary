#include "NeuralGroup.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <numeric>

NeuralGroup::NeuralGroup(int size, double dt, std::mt19937& rng)
    : size(size), dt(dt), threshold(0.5),
      pos(size), vel(size), force(size),
      mass(size, 1.0),
      damping(size, 0.95),
      W_intra(size, std::vector<double>(size, 0.0)),
      metric_tensor(size, std::vector<double>(size, 0.0)),
      specialization("unspecialized"),
      activation_temp(0.8)
{
    // Инициализация позиций на сфере
    std::uniform_real_distribution<double> theta_dist(0, 2*M_PI);
    std::uniform_real_distribution<double> phi_dist(0, M_PI);
    
    for (int i = 0; i < size; ++i) {
        double theta = theta_dist(rng);
        double phi = phi_dist(rng);
        
        // Случайная точка на единичной сфере
        pos[i].x = std::sin(phi) * std::cos(theta);
        pos[i].y = std::sin(phi) * std::sin(theta);
        pos[i].z = std::cos(phi);
        
        vel[i] = Vec3(0,0,0);
        force[i] = Vec3(0,0,0);
        
        // Инициализация связей
        for (int j = i + 1; j < size; ++j) {
            double w = (theta_dist(rng) - 0.5) * 0.1;
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

void NeuralGroup::initializeRandom(std::mt19937& rng) {
    std::uniform_real_distribution<double> theta_dist(0, 2*M_PI);
    std::uniform_real_distribution<double> phi_dist(0, M_PI);
    std::uniform_real_distribution<double> weight_dist(-0.1, 0.1);
    
    for (int i = 0; i < size; ++i) {
        double theta = theta_dist(rng);
        double phi_angle = phi_dist(rng);
        
        // Случайная точка на единичной сфере
        pos[i].x = std::sin(phi_angle) * std::cos(theta);
        pos[i].y = std::sin(phi_angle) * std::sin(theta);
        pos[i].z = std::cos(phi_angle);
        
        vel[i] = Vec3(0,0,0);
        force[i] = Vec3(0,0,0);
    }
    
    // Реинициализация связей
    for (int i = 0; i < size; ++i) {
        for (int j = i + 1; j < size; ++j) {
            double w = weight_dist(rng);
            W_intra[i][j] = w;
            W_intra[j][i] = w;
        }
    }
    
    buildSynapsesFromWeights();
}

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

void NeuralGroup::updateHomeostasis() {
    // Правильная 3D энтропия на сфере
    double current_entropy = 0.0;
    
    const int THETA_BINS = 8;  // азимут (0-2π)
    const int PHI_BINS = 4;    // полярный угол (0-π)
    
    std::vector<std::vector<int>> hist(THETA_BINS, std::vector<int>(PHI_BINS, 0));
    
    for (int i = 0; i < size; ++i) {
        double theta = std::atan2(pos[i].y, pos[i].x);  // -π до π
        double z = std::clamp(pos[i].z, -1.0, 1.0);
        double phi = std::acos(z);              // 0 до π
        
        int theta_bin = static_cast<int>((theta + M_PI) / (2*M_PI) * THETA_BINS) % THETA_BINS;
        int phi_bin = static_cast<int>(phi / M_PI * PHI_BINS) % PHI_BINS;
        
        hist[theta_bin][phi_bin]++;
    }
    
    for (int t = 0; t < THETA_BINS; ++t) {
        for (int p = 0; p < PHI_BINS; ++p) {
            int count = hist[t][p];
            if (count > 0) {
                double p_val = static_cast<double>(count) / size;
                current_entropy -= p_val * std::log(p_val);
            }
        }
    }
    
    entropy_history.push_back(current_entropy);
    if (entropy_history.size() > homeo.history_size) {
        entropy_history.pop_front();
    }
    
    double entropy_error = homeo.target_entropy - current_entropy;
    
    // Адаптируем параметры - теперь только damping, масса фиксирована
    double adaptation = entropy_error * homeo.adaptation_rate;
    adaptation = std::clamp(adaptation, -0.1, 0.1);
    
    for (int i = 0; i < size; i++) {
        damping[i] -= adaptation * 0.2;
        damping[i] = std::clamp(damping[i], homeo.min_damping, homeo.max_damping);
        
        // Массу не меняем (она постоянна для стабильности)
        // mass[i] = std::clamp(mass[i], 0.5, 2.0);
    }
    
    // Проверяем, нужен ли burst - ЗАМЕДЛЯЕМ НАКОПЛЕНИЕ
    if (std::abs(entropy_error) > homeo.entropy_tolerance) {
        burst_potential += std::abs(entropy_error) * 0.02;
        
        // Добавляем проверку на cooldown
        if (burst_potential > homeo.burst_threshold && 
            step_counter_ - last_burst_step > BURST_COOLDOWN) {
            generateBurst();
            burst_potential = 0.0;
            last_burst_step = step_counter_;
        }
    }

    // Если entropy все еще низкая после burst'а, увеличиваем потенциал быстрее
    if (current_entropy < homeo.target_entropy * 0.5 && 
        step_counter_ - last_burst_step < 10) {
        // burst не помог - накапливаем потенциал быстрее
        burst_potential += homeo.entropy_tolerance * 0.1;
    }
}

void NeuralGroup::adaptParameters(double entropy_error) {
    // Увеличиваем адаптацию
    double adaptation = entropy_error * homeo.adaptation_rate;
    
    // Ограничиваем максимальное изменение за шаг
    adaptation = std::clamp(adaptation, -0.1, 0.1);
    
    for (int i = 0; i < size; i++) {
        // 1. Регулируем затухание - БОЛЕЕ АГРЕССИВНО
        damping[i] -= adaptation * 0.5;  // было 0.1
        damping[i] = std::clamp(damping[i], homeo.min_damping, homeo.max_damping);
        
        // 2. Регулируем массу - БОЛЕЕ АГРЕССИВНО
        mass[i] += adaptation * 0.3;  // было 0.05
        mass[i] = std::clamp(mass[i], 0.3, 3.0);
    }
    
    // 3. Регулируем температуру - БОЛЕЕ АГРЕССИВНО
    activation_temp -= adaptation * 1.0;  // было 0.2
    activation_temp = std::clamp(activation_temp, 
                                 homeo.min_temperature, 
                                 homeo.max_temperature);
    
    // 4. Шум в весах - только при сильном отклонении
    if (std::abs(entropy_error) > homeo.entropy_tolerance) {
        double noise_scale = std::abs(entropy_error) * 0.01;  // было 0.001
        
        for (auto& syn : synapses) {
            double noise = (static_cast<double>(rand()) / RAND_MAX - 0.5) * noise_scale;
            syn.weight += noise;
            syn.weight = std::clamp(syn.weight, -params.maxWeight, params.maxWeight);
        }
    }
}

void NeuralGroup::generateBurst() {
    std::cout << "  Burst generated to increase diversity" << std::endl;
    
    static std::mt19937 rng(std::random_device{}());
    static std::normal_distribution<double> dist(0.0, 1.0);
    
    // ЕЩЕ БОЛЕЕ СИЛЬНЫЙ burst
    double burst_strength = 3.0;  // увеличили с 2.0 до 3.0
    
    for (int i = 0; i < size; ++i) {
        // Создаем шум, касательный к сфере
        Vec3 noise(dist(rng), dist(rng), dist(rng));
        double dot = Vec3::dot(noise, pos[i]);
        noise = noise - pos[i] * dot;
        
        double noise_norm = noise.norm();
        if (noise_norm > 1e-6) {
            noise = noise / noise_norm;
        }
        
        vel[i] += noise * burst_strength;
        
        // ЕЩЕ СИЛЬНЕЕ уменьшаем массу
        mass[i] *= 0.6;  // было 0.8
        mass[i] = std::clamp(mass[i], 0.3, 3.0);
    }
    
    // Ограничение весов
    for (auto& syn : synapses) {
        syn.weight = std::clamp(syn.weight, -params.maxWeight, params.maxWeight);
    }
}

double NeuralGroup::getAdaptiveNoiseLevel() const {
    // Вычисляем адаптивный уровень шума на основе истории энтропии
    if (entropy_history.size() < 10) return homeo.base_noise_level;
    
    double recent_entropy = 0.0;
    for (size_t i = entropy_history.size() - 10; i < entropy_history.size(); i++) {
        recent_entropy += entropy_history[i];
    }
    recent_entropy /= 10;
    
    double entropy_error = homeo.target_entropy - recent_entropy;
    
    // Чем дальше от цели, тем больше шума
    double noise_level = homeo.base_noise_level + std::abs(entropy_error) * 0.1;
    return std::clamp(noise_level, 0.01, 0.5);
}

void NeuralGroup::evolve() {
    step_counter_++;
    
    // ОТЛАДКА: проверяем состояние
    if (step_counter_ % 100 == 0) {
        double avg_pos_x = 0, avg_pos_y = 0, avg_pos_z = 0;
        double min_r = 1e9, max_r = 0;
        
        for (int i = 0; i < size; ++i) {
            double r = pos[i].norm();
            min_r = std::min(min_r, r);
            max_r = std::max(max_r, r);
            avg_pos_x += pos[i].x;
            avg_pos_y += pos[i].y;
            avg_pos_z += pos[i].z;
        }
        
        std::cout << "  Group " << specialization 
                  << " r: [" << min_r << ", " << max_r << "]"
                  << " center: (" << avg_pos_x/size << "," 
                  << avg_pos_y/size << "," << avg_pos_z/size << ")" 
                  << std::endl;
    }

    // Шаг 1: половина шага скорости
    for (int i = 0; i < size; ++i) {
        vel[i] += force[i] * (dt / (2.0 * mass[i]));
    }
    
    // Шаг 2: полный шаг позиции
    for (int i = 0; i < size; ++i) {
        pos[i] += vel[i] * dt;
    }
    
    // Шаг 3: пересчитываем силы
    computeDerivatives();
    
    // Шаг 4: завершаем шаг скорости
    for (int i = 0; i < size; ++i) {
        vel[i] += force[i] * (dt / (2.0 * mass[i]));
    }
    
    // ===== ГЛАДКОЕ ОГРАНИЧЕНИЕ НА СФЕРЕ =====
    for (int i = 0; i < size; ++i) {
        double r = pos[i].norm();
        
        if (r > 1e-6) {  // защита от деления на ноль
            // Нормализуем до единичной сферы
            pos[i] = pos[i] / r;
            
            // Убираем радиальную компоненту скорости
            Vec3 r_hat = pos[i];
            double v_radial = Vec3::dot(vel[i], r_hat);
            vel[i] -= r_hat * v_radial;
        } else {
            // Если в центре - случайное направление
            static std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<double> theta_dist(0, 2*M_PI);
            std::uniform_real_distribution<double> phi_dist(0, M_PI);
            
            double theta = theta_dist(rng);
            double phi = phi_dist(rng);
            
            pos[i].x = std::sin(phi) * std::cos(theta);
            pos[i].y = std::sin(phi) * std::sin(theta);
            pos[i].z = std::cos(phi);
            vel[i] = Vec3(0,0,0);
        }
    }
    
    // Ограничение скорости
    double current_entropy = computeEntropy();
    double target_entropy = computeEntropyTarget();
    double entropy_error = target_entropy - current_entropy;
    
    for (int i = 0; i < size; ++i) {
        double v_norm = vel[i].norm();
        double v_max = 2.0 * (1.0 + std::abs(entropy_error));
        if (v_norm > v_max) {
            vel[i] = vel[i] * (v_max / v_norm);
        }
    }

    if (step_counter_ % 100 == 0) {
    // Проверяем, что векторы не пусты и имеют правильный размер
    if (pos.size() != size || vel.size() != size) {
        std::cerr << "CRITICAL: Vector size mismatch!" << std::endl;
        return;
        }
    }
    
    // Обновляем метрику
    updateMetricTensor();
    
    // Гомеостаз (адаптация параметров)
    if (step_counter_ % 10 == 0) {
        updateHomeostasis();
    }
}


// метод обновления массы 
void NeuralGroup::updateMass() {
    double entropy = computeEntropy();
    double target_entropy = computeEntropyTarget();
    
    for (int i = 0; i < size; ++i) {
        // Активность = расстояние от центра (теперь всегда 1 на сфере)
        // Используем скорость как меру активности
        double activity = vel[i].norm();
        
        // Уверенность
        double confidence = std::min(activity * 2.0, 1.0);
        
        // Вклад в энтропию
        double entropy_contribution = 4.0 * activity * (1.0 - activity);
        
        // Разность с целевой энтропией
        double entropy_error = std::abs(target_entropy - entropy);
        
        // Итоговая масса
        double target_mass = 0.5 + 0.5 * confidence + 0.2 * entropy_contribution;
        
        if (entropy_error > 1.0) {
            target_mass *= 0.7;
        }
        
        // МЕДЛЕННОЕ обновление массы
        mass[i] = mass[i] * 0.995 + target_mass * 0.005;
        mass[i] = std::clamp(mass[i], 0.3, 2.0);
    }
}

// обновление метрического тензора
void NeuralGroup::updateMetricTensor() {
    const double alpha = 0.0005;
    
    // ПРОВЕРКА: если много NaN, сбрасываем метрику
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
        std::cout << "  Metric tensor reset due to NaN" << std::endl;
        return;
    }
    
    for (int i = 0; i < size; i++) {
        for (int j = i; j < size; j++) {
            if (i == j) {
                metric_tensor[i][i] = 1.0;
            } else {
                // Расстояние между нейронами в 3D
                double dist = (pos[i] - pos[j]).norm();
                // Нормализованная корреляция (1 при dist=0, 0 при dist=большом)
                double correlation = std::exp(-dist * dist);
                correlation = std::clamp(correlation, -1.0, 1.0);
                
                metric_tensor[i][j] = metric_tensor[i][j] * (1.0 - alpha) + correlation * alpha;
                metric_tensor[j][i] = metric_tensor[i][j];
                
                // Нормализация строки
                double sum = 0;
                for (int k = 0; k < size; k++) {
                    if (std::isfinite(metric_tensor[i][k])) {
                        sum += metric_tensor[i][k];
                    } else {
                        metric_tensor[i][k] = (i == k) ? 1.0 : 0.0;
                    }
                }
                
                if (sum > 1.0 && std::isfinite(sum) && sum > 1e-8) {
                    for (int k = 0; k < size; k++) {
                        metric_tensor[i][k] /= sum;
                    }
                }
            }
        }
    }
}

void NeuralGroup::computeDerivatives() {
    static std::mt19937 rng(std::random_device{}());
    static std::normal_distribution<double> noise_dist(0.0, 1.0);
    
    std::fill(force.begin(), force.end(), Vec3(0,0,0));

    // Проверка на одинаковые позиции - убираем, это симптом, не решение
    // Вместо этого добавим анизотропный шум при низкой энтропии ниже
    
    double current_entropy = computeEntropy();
    double target_entropy = computeEntropyTarget();
    double entropy_error = target_entropy - current_entropy;
    double avg_activity = getAverageActivity();
    
    // Активность теперь = скорость, не радиус
    double activity = 0.0;
    for (int i = 0; i < size; ++i) {
        activity += vel[i].norm();
    }
    activity /= size;
    
    for (int i = 0; i < size; ++i) {
        // ВЗАИМОДЕЙСТВИЕ ЮКАВА (ОСЛАБЛЕННОЕ)
        for (int j = 0; j < size; ++j) {
            if (i == j) continue;
            
            Vec3 diff = pos[i] - pos[j];
            double r_ij = diff.norm() + 1e-6;
            
            double g_squared = 0.1 * mass[i] * mass[j];
            double w_factor = std::abs(W_intra[i][j]);
            double mu = 2.0 + entropy_error * 0.5;
            mu = std::clamp(mu, 1.0, 5.0);
            
            double exp_term = std::exp(-mu * r_ij);
            double potential_gradient = g_squared * exp_term * (1.0/(r_ij*r_ij) + mu/r_ij);
            Vec3 yukawa_force = -diff.normalized() * potential_gradient;
            
            double sign = (W_intra[i][j] > 0) ? 1.0 : -1.0;
            // ОСЛАБЛЯЕМ: было 5.0, стало 0.5
            force[i] += yukawa_force * w_factor * 0.5 * sign;
        }
        
        // ПРИНЦИП ПАУЛИ (отталкивание) - тоже ослабляем
        for (int j = 0; j < size; ++j) {
            if (i == j) continue;
            
            Vec3 diff = pos[i] - pos[j];
            double r_ij = diff.norm() + 1e-6;
            
            double pauli_strength = 0.2 + 0.2 * std::abs(entropy_error);
            if (r_ij < 0.5) {
                Vec3 pauli_force = diff.normalized() * (pauli_strength / (r_ij * r_ij + 0.1));
                force[i] += pauli_force;
            }
        }
        
        // ТРЕНИЕ
        double gamma = damping[i] * (0.8 + 0.4 * avg_activity);
        force[i] += vel[i] * (-gamma);
        
        // АНИЗОТРОПНЫЙ ШУМ (касательный к сфере)
        Vec3 noise_vec(noise_dist(rng), noise_dist(rng), noise_dist(rng));
        // Проецируем на касательную плоскость
        double dot = Vec3::dot(noise_vec, pos[i]);
        noise_vec = noise_vec - pos[i] * dot;
        
        // Нормируем шум
        double noise_norm = noise_vec.norm();
        if (noise_norm > 1e-6) {
            noise_vec = noise_vec / noise_norm;
        }
        
        // Интенсивность шума зависит от энтропии и температуры
        double T = 0.05 + 0.1 * std::abs(entropy_error);
        double sigma = std::sqrt(2.0 * gamma * T / dt);
        
        force[i] += noise_vec * sigma;
        
        // УБРАНА ТАНГЕНЦИАЛЬНАЯ СИЛА ПОЛНОСТЬЮ
        
        // СЛАБОЕ ПРИТЯЖЕНИЕ К ОПТИМАЛЬНОМУ РАДИУСУ (очень слабое)
        double r = 1.0;
        double r_error = r - 1.0;  // оптимальный радиус всегда 1.0
        if (std::abs(r_error) > 0.1) {
            Vec3 radial_force = -pos[i].normalized() * r_error * 0.01;
            force[i] += radial_force;
        }
    }
    
    // Добавляем дополнительный шум при низкой энтропии
    if (current_entropy < 0.5) {
        for (int i = 0; i < size; ++i) {
            Vec3 extra_noise(noise_dist(rng), noise_dist(rng), noise_dist(rng));
            double dot = Vec3::dot(extra_noise, pos[i]);
            extra_noise = extra_noise - pos[i] * dot;
            
            double extra_sigma = 0.05 * (0.5 - current_entropy);
            force[i] += extra_noise * extra_sigma;
        }
        std::cout << "  Low entropy (" << current_entropy << "), adding tangential noise" << std::endl;
    }
}

double NeuralGroup::computeEntropy() const {
    // В 3D энтропия может считаться по направлениям
    const int BINS = 8;
    std::vector<int> hist_azimuth(BINS, 0);  // по азимуту
    std::vector<int> hist_polar(BINS, 0);    // по полярному углу
    
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
    
    return entropy / 2.0;  // среднее по двум распределениям
}

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
            const bool preSpike  = (activity_i > threshold);
            const bool postSpike = (activity_j > threshold);

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

            float elevation_factor = 1.0f + elevation_;
            float entropy_factor = 1.0f + static_cast<float>(entropy_error * 0.1);
            
            syn.eligibility = syn.eligibility * params.eligibilityDecay + delta;
            
            if (isActive) {
                syn.weight += params.stdpRate * reward * syn.eligibility * 
                             elevation_factor * entropy_factor;
            } else {
                syn.weight *= 0.999f;
            }

            syn.weight = std::clamp(syn.weight, -params.maxWeight, params.maxWeight);

            W_intra[i][j] = syn.weight;
            W_intra[j][i] = syn.weight;
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
            // C++17 или новее, можно использовать std::clamp с разными типами
            // Явное приведение типов
            double maxWeight = static_cast<double>(params.maxWeight);
            double minWeight = static_cast<double>(-params.maxWeight);
            
            W_intra[i][j] = std::clamp(W_intra[i][j], minWeight, maxWeight);
            W_intra[j][i] = W_intra[i][j];
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
}

double NeuralGroup::getAverageActivity() const {
    // Активность = расстояние от центра (нормированное)
    double sum = 0.0;
    for (const auto& p : pos) {
        sum += p.norm();
    }
    return sum / size;
}

const std::vector<double>& NeuralGroup::getPhi() const {
    // Для обратной совместимости - конвертируем 3D в 1D (расстояние)
    static std::vector<double> phi_converted;
    phi_converted.resize(size);
    for (int i = 0; i < size; ++i) {
        phi_converted[i] = pos[i].norm();
    }
    return phi_converted;
}

std::vector<double>& NeuralGroup::getPhiNonConst() { 
    // Это костыль - лучше переписать код, использующий phi
    static std::vector<double> phi_converted;
    phi_converted.resize(size);
    for (int i = 0; i < size; ++i) {
        phi_converted[i] = pos[i].norm();
    }
    return phi_converted;
}


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
            gd_velocity[i][j] = gd_momentum * gd_velocity[i][j]  // ПЕРЕИМЕНОВАНО
                           - gd_learning_rate * weight_gradients[i][j];

            W_intra[i][j] += gd_velocity[i][j];  // ПЕРЕИМЕНОВАНО
            W_intra[j][i] = W_intra[i][j];

            W_intra[i][j] = std::clamp(
                W_intra[i][j],
                -static_cast<double>(params.maxWeight),
                static_cast<double>(params.maxWeight)
            );
            W_intra[j][i] = W_intra[i][j];
        }
    }
    
    int idx = 0;
    for (int i = 0; i < size; ++i) {
        for (int j = i + 1; j < size; ++j) {
            synapses[idx++].weight = static_cast<float>(W_intra[i][j]);
        }
    }
}

void NeuralGroup::updateElevationFast(float reward, float activity) {
    double entropy = computeEntropy();
    double target_entropy = computeEntropyTarget();
    double entropy_contribution = -entropy * std::log(entropy + 1e-12);
    
    if (activity > getEffectiveThreshold()) {
        // Высота растет, если нейрон вносит вклад в энтропию
        elevation_ += elevation_learning_rate_ * reward * entropy_contribution;
    }
    
    elevation_ *= elevation_decay_;
    elevation_ = std::clamp(elevation_, -1.0f, 1.0f);
}

void NeuralGroup::decayAllWeights(float factor) {
    for (auto& syn : synapses) {
        syn.weight *= factor;
    }
    
    int idx = 0;
    for (int i = 0; i < size; ++i) {
        for (int j = i + 1; j < size; ++j) {
            W_intra[i][j] = synapses[idx].weight;
            W_intra[j][i] = synapses[idx].weight;
            idx++;
        }
    }
}

void NeuralGroup::consolidateElevation(float globalImportance, float hallucinationPenalty) {
    if (activity_counter_ > 0) {
        float avg_importance = cumulative_importance_ / activity_counter_;
        
        // Учитываем энтропийный вклад
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
    // для обратной совместимости? - ДА, ЭТО НУЖНО!
    // W_intra используется в computeDerivatives() и evolve()
    int idx = 0;
    for (int i = 0; i < size; ++i) {
        for (int j = i + 1; j < size; ++j) {
            W_intra[i][j] = synapses[idx].weight;
            W_intra[j][i] = synapses[idx].weight;
            idx++;
        }
    }
}

double NeuralGroup::scalarCurvature() const {
    // ЗАЩИТА: проверяем метрический тензор на NaN
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            if (!std::isfinite(metric_tensor[i][j])) {
                return 0.0;  // Возвращаем 0 при проблемах
            }
        }
    }

    if (size < 3) return 0.0;
    
    double total_curvature = 0.0;
    int triangles = 0;
    
    for (int i = 0; i < size; i++) {
        for (int j = i + 1; j < size; j++) {
            for (int k = j + 1; k < size; k++) {
                double g_ij = std::isfinite(metric_tensor[i][j]) ? metric_tensor[i][j] : 1.0;
                double g_jk = std::isfinite(metric_tensor[j][k]) ? metric_tensor[j][k] : 1.0;
                double g_ki = std::isfinite(metric_tensor[k][i]) ? metric_tensor[k][i] : 1.0;
                
                // Ограничиваем значения
                g_ij = std::clamp(g_ij, 0.01, 10.0);
                g_jk = std::clamp(g_jk, 0.01, 10.0);
                g_ki = std::clamp(g_ki, 0.01, 10.0);
                
                double d_ij = std::sqrt(g_ij);
                double d_jk = std::sqrt(g_jk);
                double d_ki = std::sqrt(g_ki);
                
                double s = (d_ij + d_jk + d_ki) / 2.0;
                double area_sq = s * (s - d_ij) * (s - d_jk) * (s - d_ki);
                
                if (area_sq <= 1e-10) continue;  // Пропускаем вырожденные треугольники
                
                double area = std::sqrt(area_sq);
                
                // Вычисляем углы с защитой
                double cos_i = (d_ij*d_ij + d_ki*d_ki - d_jk*d_jk) / (2.0 * d_ij * d_ki + 1e-8);
                cos_i = std::clamp(cos_i, -0.999, 0.999);
                double angle_i = std::acos(cos_i);
                
                double cos_j = (d_ij*d_ij + d_jk*d_jk - d_ki*d_ki) / (2.0 * d_ij * d_jk + 1e-8);
                cos_j = std::clamp(cos_j, -0.999, 0.999);
                double angle_j = std::acos(cos_j);
                
                double angle_k = M_PI - (angle_i + angle_j);
                
                if (angle_k <= 0 || angle_k >= M_PI) continue;
                
                double angle_defect = M_PI - (angle_i + angle_j + angle_k);
                
                // Защита от деления на ноль
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
    // Целевая энтропия зависит от специализации группы
    // Для разных типов групп - разная оптимальная энтропия
    
    double target = 2.0; // базовое значение
    
    // Корректируем в зависимости от специализации
    if (specialization == "perception") {
        // Перцептивные группы должны быть более определенными (меньше энтропии)
        target = 1.5;
    } else if (specialization == "memory") {
        // Группы памяти - умеренная энтропия
        target = 2.0;
    } else if (specialization == "action") {
        // Моторные группы - низкая энтропия (точные действия)
        target = 1.2;
    } else if (specialization == "abstract") {
        // Абстрактные группы - высокая энтропия (гибкость)
        target = 2.5;
    } else if (specialization == "language") {
        // Языковые группы - средняя энтропия
        target = 2.2;
    } else if (specialization == "curiosity") {
        // Любопытство - высокая энтропия (исследование)
        target = 2.8;
    }
    
    // Динамическая регулировка на основе активности группы
    double avg_activity = getAverageActivity();
    if (avg_activity < 0.2) {
        // Слишком низкая активность - цель увеличить энтропию
        target *= 1.2;
    } else if (avg_activity > 0.8) {
        // Слишком высокая активность - цель уменьшить энтропию
        target *= 0.8;
    }
    
    // Учитываем влияние кривизны (чем выше кривизна, тем выше целевая энтропия)
    double curvature = scalarCurvature();
    if (std::isfinite(curvature) && std::abs(curvature) < 10.0) {
        target *= (1.0 + 0.1 * curvature);
    }
    
    return target;
}

const std::vector<double>& NeuralGroup::getPi() const {
    static std::vector<double> pi_converted;
    pi_converted.resize(size);
    for (int i = 0; i < size; ++i) {
        // Используем компоненту скорости как "импульс"
        pi_converted[i] = vel[i].norm();
    }
    return pi_converted;
}

std::vector<double>& NeuralGroup::getPiNonConst() {
    static std::vector<double> pi_converted;
    pi_converted.resize(size);
    for (int i = 0; i < size; ++i) {
        pi_converted[i] = vel[i].norm();
    }
    return pi_converted;
}

std::vector<double>& NeuralGroup::getNeuronVelocity() {
    static std::vector<double> vel_converted;
    vel_converted.resize(size);
    for (int i = 0; i < size; ++i) {
        vel_converted[i] = vel[i].norm();
    }
    return vel_converted;
}

const std::vector<double>& NeuralGroup::getNeuronVelocity() const {
    static std::vector<double> vel_converted;
    vel_converted.resize(size);
    for (int i = 0; i < size; ++i) {
        vel_converted[i] = vel[i].norm();
    }
    return vel_converted;
}