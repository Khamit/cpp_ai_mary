#include "NeuralFieldSystem.hpp"
#include <cmath>
#include <iostream>
#include <numeric>
#include <algorithm>

constexpr int CONSOLIDATION_INTERVAL = 100;   // Уровень 2: редко
constexpr int EVOLUTION_INTERVAL = 5000;      // Уровень 3: очень редко

NeuralFieldSystem::NeuralFieldSystem(double dt)
    : dt(dt),
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
    // Консолидация межгрупповых связей с учетом энтропии
    double entropy = computeSystemEntropy();
    double entropy_factor = 1.0 / (1.0 + std::exp(-(entropy - 2.0))); // сигмоида
    
    for (auto& row : interWeights) {
        for (auto& w : row) {
            // Если энтропия высокая (хаос), забывание замедляется
            // Если энтропия низкая (порядок), забывание ускоряется
            w *= (0.999f + 0.001f * entropy_factor);
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
            // Главное - везде использовать одинаково
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

void NeuralFieldSystem::step(float globalReward, int stepNumber) {
    // УРОВЕНЬ 1: каждый шаг
    for (auto& group : groups) group.evolve();
    applyReentry(3);
    
    for (auto& group : groups) {
        group.learnSTDP(globalReward, stepNumber);
        group.updateElevationFast(globalReward, group.getAverageActivity());
    }
    
    // Сохраняем историю энтропии
    double current_entropy = computeSystemEntropy();
    entropy_history.push_back(current_entropy);
    if (entropy_history.size() > HISTORY_SIZE) {
        entropy_history.pop_front();
    }
    
    // УРОВЕНЬ 2: редко (раз в 100 шагов)
    if (stepNumber % CONSOLIDATION_INTERVAL == 0) {
        float globalImportance = computeGlobalImportance();
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
    // Уменьшаем активность групп
    for (auto& group : groups) {
        for (int i = 0; i < GROUP_SIZE; ++i) {
            // Можно обнулить малые значения
        }
    }
}