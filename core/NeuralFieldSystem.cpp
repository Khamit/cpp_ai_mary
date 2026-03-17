#include "NeuralFieldSystem.hpp"
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

void NeuralFieldSystem::initializeRandom(std::mt19937& rng) {
    groups.clear();
    groups.reserve(NUM_GROUPS);
    for (int g = 0; g < NUM_GROUPS; ++g) {
        groups.emplace_back(GROUP_SIZE, dt, rng);
    }
    
    // Убедиться, что groups.size() == NUM_GROUPS
    if (groups.size() != NUM_GROUPS) {
        std::cerr << "ERROR: Groups not properly initialized!" << std::endl;
        return;
    }
    
    // Проверить размер interWeights
    interWeights.resize(NUM_GROUPS, std::vector<double>(NUM_GROUPS, 0.0));
    
    // УСИЛИВАЕМ СВЯЗИ ОТ ГРУППЫ 0 К СЕМАНТИКЕ
    for (int g = 16; g <= 21; g++) {
        interWeights[0][g] = 2.0;  // было 0.8, увеличили до 2.0
        interWeights[g][0] = 0.5;  // обратная связь тоже усилили
    }

    // Инициализация межгрупповых связей с геометрической структурой
    // Используем синусоидальную зависимость от расстояния между группами
    std::uniform_real_distribution<double> dist(-0.01, 0.01);
    for (int i = 0; i < NUM_GROUPS; ++i) {
        for (int j = 0; j < NUM_GROUPS; ++j) {
            if (i != j) {
                // Геометрический фактор: связь сильнее между близкими группами
                double angle = 2.0 * M_PI * std::abs(i - j) / NUM_GROUPS;
                double geometric_factor = std::cos(angle) * 0.5 + 0.5; // от 0 до 1
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

void NeuralFieldSystem::maintainCriticality() {
    double entropy = computeSystemEntropy();
    double target = 2.5;
    
    // Используем кривизну для регуляции
    double total_curvature = 0.0;
    int count = 0;
    for (const auto& group : groups) {
        double curv = group.scalarCurvature();
        if (std::isfinite(curv)) {
            total_curvature += curv;
            count++;
        }
    }
    
    if (count > 0) {
        double avg_curvature = total_curvature / count;
        
        // Если кривизна высокая (сингулярность), увеличиваем температуру
        if (avg_curvature > 1.0) {
            attention.temperature *= 1.1f;
        }
        // Если кривизна низкая (плоское пространство), уменьшаем
        else if (avg_curvature < 0.1) {
            attention.temperature *= 0.95f;
        }
    }
    
    // Ограничиваем температуру
    attention.temperature = std::clamp(attention.temperature, 0.1f, 10.0f);
}

void NeuralFieldSystem::step(float globalReward, int stepNumber) {
    stepCounter = stepNumber;
    
    // Сохраняем состояние ДО обновления (для будущего использования)
    std::vector<double> state_before = getFeaturesDouble();
    
    // УРОВЕНЬ 1: каждый шаг
    for (auto& group : groups) group.evolve();
    
    // Поток Риччи каждые 10 шагов
    if (stepNumber % 10 == 0) {
        applyRicciFlow();
    }
    
    // Детектор сингулярностей - ТОЛЬКО РЕДКО (например, раз в 100 шагов)
    if (stepNumber % 100 == 0) {
        detectAndHandleSingularities();
    }
    
    
    applyReentry(3);
    
    for (auto& group : groups) {
        group.learnSTDP(globalReward, stepNumber);
        group.updateElevationFast(globalReward, group.getAverageActivity());
    }

    // Получаем состояние ПОСЛЕ обновления
    std::vector<double> state_after = getFeaturesDouble();
    
    // НОВОЕ: вызываем предсказательный кодер (вместо старого predictor)
    float modulated_reward = globalReward;
    if (predictive_coder) {
        float pred_error = predictive_coder->step(stepNumber);
        
        // Модулируем глобальную награду ошибкой предсказания
        modulated_reward = globalReward * (1.0f - std::tanh(pred_error));
        
        // Если ошибка высокая, генерируем событие
        if (pred_error > 0.3) {
            Event anomaly_event;
            anomaly_event.type = EventType::ANOMALY_DETECTED;
            anomaly_event.source = "predictive_coder";
            anomaly_event.value = pred_error;
            anomaly_event.step = stepNumber;
            events.emit(anomaly_event);
        }
    }
    
    // Сохраняем историю энтропии
    double current_entropy = computeSystemEntropy();
    entropy_history.push_back(current_entropy);
    if (entropy_history.size() > HISTORY_SIZE) {
        entropy_history.pop_front();
    }
    // частоту вывода энтропии
    static int last_entropy_step = 0;
    if (stepNumber - last_entropy_step > 200) {  // каждые 200 шагов
        std::cout << "System entropy: " << current_entropy << std::endl;
        last_entropy_step = stepNumber;
    }
        
    // УРОВЕНЬ 2: редко (раз в 100 шагов)
    if (stepNumber % CONSOLIDATION_INTERVAL == 0) {
        float globalImportance = computeGlobalImportance();
        
        // Добавляем важность на основе ошибки предсказания
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

    // УРОВЕНЬ 3: очень редко (раз в 5000 шагов)
    if (stepNumber % EVOLUTION_INTERVAL == 0) {
        pendingEvolutionRequest_ = true;
    }
    
    flatDirty = true;
}

void NeuralFieldSystem::applyPruningByElevation() {
    const float PRUNING_THRESHOLD = -0.3f;
    
    for (auto& group : groups) {
        if (group.getElevation() < PRUNING_THRESHOLD) {
            group.decayAllWeights(0.9f);
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
    for (double v : flatPhi) total += v * v;
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

void NeuralFieldSystem::performSurgery(NeuralGroup& group, double curvature, double entropy) {
    auto& phi = group.getPhiNonConst();
    auto& synapses = group.getSynapses();
    
    // 1. Сбрасываем активности с квантовыми флуктуациями
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> noise(0.0, 0.05);
    
    for (size_t i = 0; i < phi.size(); i++) {
        // Гауссово распределение вокруг 0.5
        phi[i] = 0.5 + noise(gen);
        phi[i] = std::clamp(phi[i], 0.0, 1.0);
    }
    
    // 2. Ослабляем слишком сильные связи
    for (auto& syn : synapses) {
        if (std::abs(syn.weight) > 0.8) {
            syn.weight *= 0.5;  // хирургическое уменьшение
        }
    }
    
    // ИСПРАВЛЕНИЕ: Используем новые методы вместо прямого доступа к private полям
    int group_size = group.getSize();  // используем геттер
    
    // 3. Обновляем W_intra из синапсов
    int idx = 0;
    for (int i = 0; i < group_size; i++) {
        for (int j = i + 1; j < group_size; j++) {
            if (idx < synapses.size()) {
                // Используем setWeight вместо прямого доступа
                group.setWeight(i, j, synapses[idx].weight);
                group.setWeight(j, i, synapses[idx].weight);
                idx++;
            }
        }
    }
    
    // 4. Генерируем событие для системы
    Event surgery_event;
    surgery_event.type = EventType::SURGERY_PERFORMED;
    surgery_event.source = "ricci_flow";
    surgery_event.value = curvature;
    surgery_event.step = stepCounter;
    events.emit(surgery_event);
}

// singularity
void NeuralFieldSystem::detectAndHandleSingularities() {
    const double SINGULARITY_THRESHOLD = 5.0;
    const double ENTROPY_COLLAPSE = 1.5;  // Порог коллапса энтропии
    
    static std::mt19937 rng(std::random_device{}());
    static int reset_count = 0;
    static int last_reset_step = 0;
    
    // Сбрасываем счетчик каждые 1000 шагов
    if (stepCounter % 1000 == 0) {
        reset_count = 0;
    }
    
    for (auto& group : groups) {
        double curvature = group.scalarCurvature();
        double entropy = group.computeEntropy();
        
        // ПРОВЕРКА 1: NaN или бесконечность
        if (!std::isfinite(curvature) || !std::isfinite(entropy)) {
            // Мягкое восстановление - только испорченные нейроны
            auto& phi = group.getPhiNonConst();
            auto& pi = group.getPiNonConst();
            
            for (int i = 0; i < GROUP_SIZE; i++) {
                if (!std::isfinite(phi[i])) {
                    phi[i] = 0.5 + 0.1 * std::sin(i * 0.5);  // Паттерн, а не случайный
                }
                if (!std::isfinite(pi[i])) {
                    pi[i] = 0.0;
                }
            }
            continue;  // Не делаем дополнительных сбросов
        }

        // ПРОВЕРКА НА НЕДАВНИЙ BURST
        if (stepCounter - group.getLastBurstStep() < 50) {
            continue;  // пропускаем группу, если был недавний burst
        }
        
        // ПРОВЕРКА 2: Полный ноль (все нейроны мертвы)
        bool all_zero = true;
        double avg = 0.0;
        for (double v : group.getPhi()) {
            avg += v;
            if (v > 0.05) {  // Понизили порог с 0.01 до 0.05
                all_zero = false;
                break;
            }
        }
        avg /= GROUP_SIZE;
        
        // ПРОВЕРКА 3: Очень низкая энтропия (застой)
        bool low_entropy = (entropy < ENTROPY_COLLAPSE);  // УБРАЛИ avg < 0.3
        
        // Применяем лечение ТОЛЬКО если действительно нужно
        if (all_zero || low_entropy) {
            if (reset_count < 10 && stepCounter - last_reset_step > 100) {  // УВЕЛИЧИЛИ reset_count
                
                std::cout << "Resetting group: entropy=" << entropy 
                        << ", avg=" << avg << std::endl;
                
                auto& phi = group.getPhiNonConst();
                auto& vel = group.getNeuronVelocity();
                auto& pi = group.getPiNonConst();
                if (vel.size() == GROUP_SIZE && pi.size() == GROUP_SIZE) {
                    for (int i = 0; i < GROUP_SIZE; i++) {
                        vel[i] = pi[i] * 2.0;
                    }
                }
                
                // БОЛЕЕ РАЗНООБРАЗНЫЙ ПАТТЕРН с разными фазами
                for (int i = 0; i < GROUP_SIZE; i++) {
                    // Используем разные частоты для каждого нейрона
                    double freq1 = 0.3 + 0.1 * (i % 5);
                    double freq2 = 0.7 + 0.1 * ((i + 3) % 7);
                    double pattern = 0.3 + 0.25 * std::sin(i * freq1 + stepCounter * 0.05) 
                                        + 0.2 * std::cos(i * freq2 + stepCounter * 0.03);
                    phi[i] = std::clamp(pattern, 0.2, 0.8);
                    
                    // Разные скорости для каждого нейрона
                    pi[i] = 0.05 * std::sin(i * 1.1) + 0.03 * std::cos(i * 0.9);
                    if (!vel.empty()) vel[i] = pi[i] * 2.0;  // УВЕЛИЧИЛИ скорость
                }
                
                reset_count++;
                last_reset_step = stepCounter;
            }
        }

        
        // ПРОВЕРКА 4: Слишком высокая кривизна (сингулярность)
        if (curvature > SINGULARITY_THRESHOLD && reset_count < 10) {
            // Ослабляем слишком сильные связи
            auto& synapses = group.getSynapsesNonConst();
            for (auto& syn : synapses) {
                if (std::abs(syn.weight) > 0.8) {
                    syn.weight *= 0.7;  // Уменьшаем, но не обнуляем
                }
            }
            
            // Обновляем веса в матрице
            int idx = 0;
            for (int i = 0; i < GROUP_SIZE; i++) {
                for (int j = i + 1; j < GROUP_SIZE; j++) {
                    if (idx < synapses.size()) {
                        group.setWeight(i, j, synapses[idx].weight);
                        group.setWeight(j, i, synapses[idx].weight);
                        idx++;
                    }
                }
            }
        }
    }
}