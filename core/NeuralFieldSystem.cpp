#include "NeuralFieldSystem.hpp"
#include "DynamicParams.hpp"
#include <cmath>
#include <iostream>
#include <numeric>
#include <algorithm>
#include <iomanip>

constexpr int CONSOLIDATION_INTERVAL = 100;   // Уровень 2: редко
constexpr int EVOLUTION_INTERVAL = 5000;      // Уровень 3: очень редко

NeuralFieldSystem::NeuralFieldSystem(double dt, EventSystem& events)
    : dt(dt), events(events),
      groups(),
      interWeights(NUM_GROUPS, std::vector<double>(NUM_GROUPS, 0.0)),
      flatPhi(TOTAL_NEURONS, 0.0),
      flatPi(TOTAL_NEURONS, 0.0),
      flatDirty(true)
{
    // Группы будут созданы позже в initializeRandom
}

void NeuralFieldSystem::initializeWithLimits(std::mt19937& rng, const MassLimits& limits) {
    groups.clear();
    groups.reserve(NUM_GROUPS);
    for (int g = 0; g < NUM_GROUPS; ++g) {
        // Передаем лимиты в конструктор NeuralGroup
        groups.emplace_back(GROUP_SIZE, dt, rng, limits);
    }
    
    // Проверка размера
    if (groups.size() != NUM_GROUPS) {
        std::cerr << "ERROR: Groups not properly initialized!" << std::endl;
        return;
    }

    for (auto& group : groups) {
        group.setMemoryManager(memory_manager);  // нужно добавить этот метод
    }
    
    // Инициализация межгрупповых связей
    interWeights.resize(NUM_GROUPS, std::vector<double>(NUM_GROUPS, 0.0));
    
    // Усиливаем связи от группы 0 к семантике
    for (int g = 16; g <= 21; g++) {
        interWeights[0][g] = 2.0;
        interWeights[g][0] = 0.5;
    }
    
    // Инициализация межгрупповых связей с геометрической структурой
    std::uniform_real_distribution<double> dist(-0.01, 0.01);
    for (int i = 0; i < NUM_GROUPS; ++i) {
        for (int j = 0; j < NUM_GROUPS; ++j) {
            if (i != j) {
                double angle = 2.0 * M_PI * std::abs(i - j) / NUM_GROUPS;
                double geometric_factor = std::cos(angle) * 0.5 + 0.5;
                interWeights[i][j] = dist(rng) * geometric_factor;
            } else {
                interWeights[i][j] = 0.0;
            }
        }
    }

    
    flatDirty = true;
}

void NeuralFieldSystem::rebuildFlatVectors() const {
    if (!flatDirty) return;
    int idx = 0;
    for (const auto& grp : groups) {
        const auto& phiGrp = grp.getPhi();
        const auto& piGrp = grp.getPi();
        for (int i = 0; i < GROUP_SIZE; ++i) {
            flatPhi[idx] = phiGrp[i];
            flatPi[idx] = piGrp[i];
            ++idx;
        }
    }
    flatDirty = false;
}

void NeuralFieldSystem::consolidateInterWeights() {
    double entropy = computeSystemEntropy();
    double entropy_factor = 1.0 / (1.0 + std::exp(-(entropy - 2.0)));
    
    // ИСПРАВЛЕНИЕ: используем геометрическое среднее вместо линейного
    double geometry_factor = 1.0;
    if (!groups.empty()) {
        double avg_curvature = 0.0;
        for (const auto& group : groups) {
            avg_curvature += group.scalarCurvature();  // ЗДЕСЬ ВЫЗОВ
        }
        avg_curvature /= groups.size();
        geometry_factor = 1.0 / (1.0 + avg_curvature);
    }
    
    for (auto& row : interWeights) {
        for (auto& w : row) {
            // Комбинируем энтропийный и геометрический факторы
            double combined = entropy_factor * geometry_factor;
            w *= (0.999 + 0.001 * combined);
        }
    }
}

double NeuralFieldSystem::computeSystemEntropy() const {
    rebuildFlatVectors();
    
    // Исправление 1: Используем тот же метод, что и в NeuralGroup
    const int BINS = 20;
    std::vector<int> hist(BINS, 0);
    
    for (double v : flatPhi) {
        // Значения phi уже в [0,1] после sigmoid
        int bin = static_cast<int>(v * BINS);
        bin = std::clamp(bin, 0, BINS - 1);
        hist[bin]++;
    }
    
    double entropy = 0.0;
    double total = static_cast<double>(TOTAL_NEURONS);
    const double LOG_BASE = std::log(2.0);  // для конвертации в log2
    
    for (int count : hist) {
        if (count > 0) {
            double p = static_cast<double>(count) / total;
            // Исправление 2: Используем ту же формулу, что в NeuralGroup
            // Но там используется std::log (натуральный), что математически верно
            // Главное - везде использовать одинаково - но почему не наследуем? 
            entropy -= p * std::log(p);
        }
    }
    
    // Исправление 3: Нормализуем, как в NeuralGroup (там нет нормировки)
    // Но для системы в целом имеем диапазон [0, log(BINS)]
    // Максимальная энтропия при равномерном распределении: log(BINS)
    
    return entropy;  // возвращаем в нат
}

void NeuralFieldSystem::applyReentry(int iterations) {
    std::vector<double> newGroupAvg(NUM_GROUPS, 0.0);
    std::vector<double> currAvg = getGroupAverages();

    for (int iter = 0; iter < iterations; ++iter) {
        attention.computeAttention(currAvg);
        
        // Используем и обычное softmax внимание, и сферическое для сравнения
        std::vector<double> spherical_attention = attention.computeSphericalAttention(currAvg);
        
        std::fill(newGroupAvg.begin(), newGroupAvg.end(), 0.0);
        
        for (int g = 0; g < NUM_GROUPS; ++g) {
            double input = 0.0;
            for (int h = 0; h < NUM_GROUPS; ++h) {
                // Комбинируем два типа внимания
                float att_soft = attention.attention_weights[h];
                double att_sphere = spherical_attention[h];
                double combined_att = 0.7 * att_soft + 0.3 * att_sphere;
                
                input += interWeights[g][h] * currAvg[h] * combined_att;
            }

            newGroupAvg[g] = currAvg[g] + dt * input;
            newGroupAvg[g] = std::clamp(newGroupAvg[g], 0.0, 1.0);
        }
        currAvg.swap(newGroupAvg);
    }

    // Применяем итоговые активности к группам
    for (int g = 0; g < NUM_GROUPS; ++g) {
        double targetAvg = currAvg[g];
        double currentAvg = groups[g].getAverageActivity();
        double diff = targetAvg - currentAvg;
        auto& phiGrp = groups[g].getPhiNonConst();
        for (int n = 0; n < GROUP_SIZE; ++n) {
            phiGrp[n] += dt * diff * 0.1;
            phiGrp[n] = std::clamp(phiGrp[n], 0.0, 1.0);
        }
    }
}

void NeuralFieldSystem::applyLateralInhibition() {
    // Торможение между семантическими группами (16-21)
    const int NUM_SEMANTIC_GROUPS = 6;
    const int SEMANTIC_START = 16;
    
    std::vector<double> group_activity(NUM_GROUPS, 0.0);
    
    // Вычисляем активность каждой группы
    for (int g = 0; g < NUM_GROUPS; g++) {
        double sum = 0.0;
        const auto& phi = groups[g].getPhi();
        for (int i = 0; i < GROUP_SIZE; i++) {
            sum += phi[i];
        }
        group_activity[g] = sum / GROUP_SIZE;
    }
    
    // Торможение: уменьшаем активность групп, которые конкурируют
    const double INHIBITION_STRENGTH = 0.1;
    
    for (int g = SEMANTIC_START; g < SEMANTIC_START + NUM_SEMANTIC_GROUPS; g++) {
        double total_inhibition = 0.0;
        for (int other = SEMANTIC_START; other < SEMANTIC_START + NUM_SEMANTIC_GROUPS; other++) {
            if (other != g) {
                total_inhibition += group_activity[other];
            }
        }
        
        // Применяем торможение
        double inhibition = INHIBITION_STRENGTH * total_inhibition;
        auto& phi = groups[g].getPhiNonConst();
        for (int i = 0; i < GROUP_SIZE; i++) {
            phi[i] = std::max(0.0, phi[i] - inhibition);
        }
    }
}

void NeuralFieldSystem::maintainCriticality() {
    double prediction_error = predictive_coder ? predictive_coder->getLastError() : 0.5;
    double energy = computeTotalEnergy();
    double current_entropy = getUnifiedEntropy();
    
    // 1. Вычисляем адаптивную целевую энтропию
    // target_entropy = 1.5 * (1 + error*2) * (1-energy/2) * mode
    // Может быть от 0.5 до 3.5

    free_energy_controller_.update(energy, current_entropy, prediction_error);
    double target_entropy = free_energy_controller_.getTargetEntropy(energy);

    // 2. Энтропийная ошибка
    double entropy_error = target_entropy - current_entropy;
    
    // 3. Адаптируем параметры системы
    if (std::abs(entropy_error) > 0.05) {   // 5% ошибки
        if (entropy_error > 0) {
            // Нужно увеличить энтропию (больше хаоса для исследования)
            attention.temperature *= 1.02;
            // Уменьшаем консолидацию (легче менять связи)
            for (auto& group : groups) {
                group.setConsolidationRate(0.003f);
                group.setStdpRate(0.6f);  // увеличиваем скорость обучения
            }
        } else {
            // Нужно уменьшить энтропию (больше порядка для предсказаний)
            attention.temperature *= 0.98;
            // Увеличиваем консолидацию (запоминаем паттерны)
            for (auto& group : groups) {
                group.setConsolidationRate(0.02f);
                group.setStdpRate(0.3f);  // уменьшаем скорость обучения
            }
        }
        
        // Ограничиваем температуру
        attention.temperature = std::clamp(attention.temperature, 0.1f, 5.0f);
    }
    
    // 4. Логируем состояние
    static int log_counter = 0;
    if (log_counter++ % 100 == 0) {
        std::cout << "Entropy: " << current_entropy 
                  << " (target: " << target_entropy << ")"
                  << ", Error: " << prediction_error
                  << ", Temp: " << attention.temperature 
                  << ", ΔE: " << entropy_error << std::endl;
    }
}

// Принцип максимума энтропии Больцмана
// Наиболее вероятное состояние = максимум энтропии при заданных ограничениях

double NeuralFieldSystem::computeOptimalStructure() {
    // Находим распределение, максимизирующее энтропию
    // при фиксированной средней энергии (канонический ансамбль)
    
    double avg_energy = computeTotalEnergy();
    double temperature = attention.temperature;
    
    // Распределение Гиббса: p_i = exp(-E_i/kT) / Z
    // Это даёт оптимальный баланс между порядком и хаосом!
    
    std::vector<double> probabilities;
    double Z = 0.0;
    
    // Используем getAverageEnergy() который мы добавили
    for (const auto& group : groups) {
        double group_energy = group.getAverageEnergy();
        // Защита от переполнения
        double exp_arg = -group_energy / (temperature + 1e-6);
        double p = std::exp(std::clamp(exp_arg, -50.0, 50.0));
        probabilities.push_back(p);
        Z += p;
    }
    
    // Защита от деления на ноль
    if (Z < 1e-12) return 0.0;
    
    // Нормализуем
    for (auto& p : probabilities) {
        p /= Z;
    }
    
    // Вычисляем информационную энтропию этого распределения
    double H_optimal = 0.0;
    for (double p : probabilities) {
        if (p > 1e-12) {
            H_optimal -= p * std::log2(p);
        }
    }
    
    return H_optimal;  // Это идеальная энтропия для системы!
}

void NeuralFieldSystem::diagnoseCriticality() {
    double entropy = computeSystemEntropy();
    double energy = computeTotalEnergy();
    double free_energy = energy - attention.temperature * entropy;
    
    // 1. Проверка на фазовый переход (критичность)
    static double last_entropy = entropy;
    double entropy_change = std::abs(entropy - last_entropy);
    
    if (entropy_change > 0.1) {
        std::cout << "⚠️ Phase transition detected! ΔS = " << entropy_change << std::endl;
    }
    
    // 2. Проверка на "застывание" (слишком низкая энтропия)
    if (entropy < 0.5) {
        std::cout << "❌ System frozen! Too ordered. Increasing exploration..." << std::endl;
        attention.temperature *= 1.5;
    }
    
    // 3. Проверка на "хаос" (слишком высокая энтропия)
    if (entropy > 3.0) {
        std::cout << "❌ System chaotic! Too disordered. Increasing consolidation..." << std::endl;
        attention.temperature *= 0.7;
    }
    
    // 4. Идеальное состояние
    if (entropy > 1.2 && entropy < 1.8 && entropy_change < 0.05) {
        static int stable_counter = 0;
        stable_counter++;
        if (stable_counter > 100) {
            std::cout << "✅ System at critical point! Optimal balance achieved." << std::endl;
        }
    }
    
    last_entropy = entropy;
}

void NeuralFieldSystem::setOperatingMode(OperatingMode::Type mode) {
    current_mode_ = mode;
    
    // Передаем режим во все группы
    for (auto& group : groups) {
        group.setCurrentMode(mode);
    }
}

void NeuralFieldSystem::step(float globalReward, int stepNumber) {
    stepCounter = stepNumber;
    
    // ===== 1. ЭВОЛЮЦИЯ (всегда) =====
    for (auto& group : groups) {
        group.evolve();
        group.maintainActivity(current_mode_);
    }
    
    // ===== 2. ГЕОМЕТРИЧЕСКИЕ ОПЕРАЦИИ (периодически) =====
    int ricci_interval = (current_mode_ == OperatingMode::TRAINING) ? 10 : 50;
    if (stepNumber % ricci_interval == 0) {
        applyRicciFlow();
    }
    
    // ===== 3. REENTRY (периодически) =====
    int reentry_iter = (current_mode_ == OperatingMode::TRAINING) ? 3 : 1;
    applyReentry(reentry_iter);
    
    // ===== 4. ОБУЧЕНИЕ С РАСПРЕДЕЛЁННЫМ REWARD =====
    // ВНИМАНИЕ: globalReward НЕ передаётся напрямую в learnSTDP!
    // Вместо этого каждая группа получает reward из своих источников
    
    // 4.1. Предсказательный кодер вычисляет ошибку для семантических групп
    float semantic_reward = 0.0f;
    if (predictive_coder) {
        float pred_error = predictive_coder->step(stepNumber);
        semantic_reward = 1.0f - std::tanh(pred_error);
        
        if (pred_error > 0.3) {
            Event anomaly_event;
            anomaly_event.type = EventType::ANOMALY_DETECTED;
            anomaly_event.source = "predictive_coder";
            anomaly_event.value = pred_error;
            anomaly_event.step = stepNumber;
            events.emit(anomaly_event);
        }
    }

    float base_survival_reward = 0.02f;
    
    // 4.2. Обучение для разных групп с разными reward
    for (int g = 0; g < NUM_GROUPS; ++g) {
        float group_reward = base_survival_reward;
        
        if (g >= 16 && g <= 21) {
            // Семантические группы — получают reward от предсказательного кодера
            // и от LanguageModule (через отдельный механизм)
            group_reward = semantic_reward;
            
            // Добавляем бонус от LanguageModule, если есть
            // (через отдельный метод, не глобальный reward)
        } else if (g == 0) {
            // Входная группа — получает reward от успешного распознавания
            // через LearningOrchestrator
            group_reward = 0.1f;  // базовая поддержка
        } else {
            // Сенсорные и моторные группы — минимальный reward
            group_reward = 0.05f;
        }
        
        // Только одно обучение за шаг!
        groups[g].learnSTDP(group_reward, stepNumber);
        groups[g].updateElevationFast(group_reward, groups[g].getAverageActivity());
    }
    
    // ===== 5. ЭСТАФЕТНОЕ ОБУЧЕНИЕ (отдельно) =====
    // НЕ зависит от globalReward!
    applyReentry(3);  // повторный reentry для закрепления
    
    // ===== 6. КОНСОЛИДАЦИЯ (периодически) =====
    if (stepNumber % CONSOLIDATION_INTERVAL == 0) {
        float globalImportance = computeGlobalImportance();
        
        if (predictive_coder) {
            globalImportance += predictive_coder->getLastError() * 2.0f;
        }
        
        for (auto& group : groups) {
            group.consolidateEligibility(globalImportance);
            group.consolidateElevation(globalImportance);
        }
        consolidateInterWeights();
        applyPruningByElevation();
    }
    
    // ===== 7. ЛОГИРОВАНИЕ =====
    if (stepNumber % 1000 == 0) {
        logOrbitalHealth();
        diagnoseCriticality(); 
    }
    
    if (stepNumber % 200 == 0) {
        double current_entropy = computeSystemEntropy();
        std::cout << "System entropy: " << current_entropy << std::endl;
    }
    
    // ===== 8. ЭВОЛЮЦИЯ (очень редко) =====
    if (stepNumber % EVOLUTION_INTERVAL == 0) {
        pendingEvolutionRequest_ = true;
    }

    applyLateralInhibition();
    
    flatDirty = true;
}

void NeuralFieldSystem::applyPruningByElevation() {
    const float PRUNING_THRESHOLD = -0.5f;  // повысили порог
    
    for (auto& group : groups) {
        if (group.getElevation() < PRUNING_THRESHOLD) {
            // Ослабляем, но не обрезаем полностью
            group.decayAllWeights(0.95f);  // мягче, чем 0.9
        }
    }
}

float NeuralFieldSystem::computeGlobalImportance() {
    // Важность пропорциональна скорости изменения энтропии
    if (entropy_history.size() < 10) return 0.5f;
    
    double current_entropy = entropy_history.back();
    double old_entropy = entropy_history[entropy_history.size() - 10];
    
    // Положительное изменение = система становится более хаотичной
    // Отрицательное изменение = система застывает (важно для консолидации)
    double entropy_change = current_entropy - old_entropy;
    
    // Если энтропия падает (система застывает), увеличиваем важность консолидации
    return std::max(0.0f, static_cast<float>(-entropy_change * 10.0f));
}

double NeuralFieldSystem::computeTotalEnergy() const {
    rebuildFlatVectors();
    double total = 0.0;
    
    // Кинетическая энергия (скорости)
    for (double v : flatPi) {
        total += v * v * 0.5;  // E_kin = 1/2 * m * v^2, m≈1
    }
    
    // Потенциальная энергия (расстояния от центра)
    for (double r : flatPhi) {
        total += r * r * 0.5;   // гармонический осциллятор
    }
    
    // Энергия связей
    for (int g = 0; g < NUM_GROUPS; g++) {
        const auto& weights = groups[g].getWeights();
        for (int i = 0; i < GROUP_SIZE; i++) {
            for (int j = i + 1; j < GROUP_SIZE; j++) {
                total += std::abs(weights[i][j]) * 0.1;
            }
        }
    }
    
    return total / TOTAL_NEURONS;
}

std::vector<double> NeuralFieldSystem::getGroupAverages() const {
    std::vector<double> avgs(NUM_GROUPS);
    for (int g = 0; g < NUM_GROUPS; ++g) {
        avgs[g] = groups[g].getAverageActivity();
    }
    return avgs;
}

void NeuralFieldSystem::strengthenInterConnection(int from, int to, double delta) {
    if (from >= 0 && from < NUM_GROUPS && to >= 0 && to < NUM_GROUPS && from != to) {
        interWeights[from][to] += delta;
        const double maxW = 0.5;
        if (interWeights[from][to] > maxW) interWeights[from][to] = maxW;
        if (interWeights[from][to] < -maxW) interWeights[from][to] = -maxW;
    }
}

void NeuralFieldSystem::weakenInterConnection(int from, int to, double delta) {
    strengthenInterConnection(from, to, -delta);
}

std::vector<float> NeuralFieldSystem::getFeatures() const {
    std::vector<float> features(64, 0.0f);
    auto avgs = getGroupAverages();

    // Первые 32 признака - средние активности групп
    for (int g = 0; g < NUM_GROUPS; ++g) {
        features[g] = static_cast<float>(avgs[g]);
    }

    // Следующие 32 признака - энтропия внутри группы
    for (int g = 0; g < NUM_GROUPS; ++g) {
        const auto& phi = groups[g].getPhi();
        
        // Вычисляем энтропию распределения активностей в группе
        const int BINS = 10;
        std::vector<int> hist(BINS, 0);
        for (double v : phi) {
            int bin = static_cast<int>(v * BINS);
            bin = std::clamp(bin, 0, BINS - 1);
            hist[bin]++;
        }
        
        double entropy = 0.0;
        for (int count : hist) {
            if (count > 0) {
                double p = static_cast<double>(count) / GROUP_SIZE;
                entropy -= p * std::log(p);
            }
        }
        
        features[NUM_GROUPS + g] = static_cast<float>(entropy);
    }

    return features;
}

// ---------- Заглушки для рефлексии ----------
NeuralFieldSystem::ReflectionState NeuralFieldSystem::getReflectionState() const {
    ReflectionState s;
    s.confidence = 0.5;
    s.curiosity = 0.5;
    s.satisfaction = 0.5;
    s.confusion = 0.5;
    s.attention_map.resize(4, 0.25);
    return s;
}

void NeuralFieldSystem::reflect() {}

void NeuralFieldSystem::setGoal(const std::string& goal) {
    current_goal = goal;
}

bool NeuralFieldSystem::evaluateProgress() {
    return false;
}

void NeuralFieldSystem::learnFromReflection(float outcome) {}

void NeuralFieldSystem::applyTargetedMutation(double strength, int targetType) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> groupDist(0, NUM_GROUPS - 1);
    std::uniform_real_distribution<> paramDist(-strength, strength);

    if (targetType == 0) {
        for (int i = 0; i < NUM_GROUPS; ++i) {
            for (int j = 0; j < NUM_GROUPS; ++j) {
                if (i != j) {
                    interWeights[i][j] += paramDist(gen) * 0.1;
                }
            }
        }
    } else if (targetType == 1) {
        int g = groupDist(gen);
        groups[g].setLearningRate(groups[g].getLearningRate() + paramDist(gen) * 0.01);
        groups[g].setThreshold(groups[g].getThreshold() + paramDist(gen) * 0.01);
    }
}

void NeuralFieldSystem::enterLowPowerMode() {
    // Вместо обнуления - мягкое снижение
    for (auto& group : groups) {
        double avg = group.getAverageActivity();
        if (avg > 0.3) {
            for (int i = 0; i < GROUP_SIZE; ++i) {
                // Снижаем, но не до нуля
                group.getPhiNonConst()[i] *= 0.7;
                // Гарантируем минимум
                if (group.getPhiNonConst()[i] < 0.2) {
                    group.getPhiNonConst()[i] = 0.2;
                }
            }
        }
    }
    std::cout << "Low power mode activated - activity reduced but preserved" << std::endl;
}

// Добавить в NeuralFieldSystem
void NeuralFieldSystem::applyRicciFlow() {
    if (entropy_history.size() < 10) return;
    
    // Вычисляем текущую кривизну системы
    double total_curvature = 0.0;
    int active_groups = 0;
    
    for (const auto& group : groups) {
        double curvature = group.scalarCurvature();  // теперь метод существует
        if (std::isfinite(curvature) && std::abs(curvature) < 10.0) {
            total_curvature += curvature;
            active_groups++;
        }
    }
    
    if (active_groups == 0) return;
    double avg_curvature = total_curvature / active_groups;
    
    // Поток Риччи на межгрупповых связях
    double flow_rate = 0.001;
    auto avgs = getGroupAverages();
    
    for (int i = 0; i < NUM_GROUPS; i++) {
        for (int j = i + 1; j < NUM_GROUPS; j++) {
            // Корреляция между группами
            double correlation = avgs[i] * avgs[j];
            
            // Тензор Риччи для межгрупповых связей
            double ricci = correlation - avg_curvature * interWeights[i][j];
            
            // Поток: ∂g/∂t = -2·Ric(g)
            double delta = -2.0 * flow_rate * ricci;
            
            interWeights[i][j] += delta;
            interWeights[j][i] = interWeights[i][j];
            
            // Ограничиваем веса
            interWeights[i][j] = std::clamp(interWeights[i][j], -0.5, 0.5);
            interWeights[j][i] = interWeights[i][j];
        }
    }
}

void NeuralFieldSystem::applyTargetPattern(const std::vector<float>& target_pattern) {
    // target_pattern размером 192 (6 групп × 32 нейрона)
    const int NUM_SEMANTIC_GROUPS = 6;
    const int SEMANTIC_START = 16;
    
    for (int g = 0; g < NUM_SEMANTIC_GROUPS; g++) {
        int group_idx = SEMANTIC_START + g;
        auto& phi = groups[group_idx].getPhiNonConst();
        
        for (int i = 0; i < GROUP_SIZE; i++) {
            float target = target_pattern[g * 32 + i];
            float diff = target - phi[i];
            // Добавляем силу, тянущую к цели (0.1 — сила притяжения)
            phi[i] += diff * 0.1;
            phi[i] = std::clamp(phi[i], 0.0, 1.0);
        }
    }
}

// НОВЫЙ МЕТОД: мониторинг орбитального здоровья системы
void NeuralFieldSystem::logOrbitalHealth() {
    if (stepCounter % 1000 != 0) return;  // раз в 1000 шагов
    
    std::vector<int> orbit_distribution(5, 0);
    int total_neurons = 0;
    
    for (const auto& group : groups) {
        for (int i = 0; i < GROUP_SIZE; i++) {
            int level = group.getOrbitLevel(i);
            orbit_distribution[level]++;
            total_neurons++;
        }
    }
    
    std::cout << "\n=== ORBITAL HEALTH ===\n";
    std::cout << "Orbit 4 (elite): " << orbit_distribution[4] 
              << " (" << (orbit_distribution[4] * 100 / total_neurons) << "%)\n";
    std::cout << "Orbit 3: " << orbit_distribution[3] 
              << " (" << (orbit_distribution[3] * 100 / total_neurons) << "%)\n";
    std::cout << "Orbit 2: " << orbit_distribution[2] 
              << " (" << (orbit_distribution[2] * 100 / total_neurons) << "%)\n";
    std::cout << "Orbit 1: " << orbit_distribution[1] 
              << " (" << (orbit_distribution[1] * 100 / total_neurons) << "%)\n";
    std::cout << "Orbit 0 (singularity): " << orbit_distribution[0] 
              << " (" << (orbit_distribution[0] * 100 / total_neurons) << "%)\n";
    std::cout << "====================\n";
}