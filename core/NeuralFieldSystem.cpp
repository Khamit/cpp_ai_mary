#include "NeuralFieldSystem.hpp"
#include <cmath>
#include <iostream>
#include <numeric>
#include <algorithm>

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

    // Инициализация межгрупповых связей случайно
    std::uniform_real_distribution<double> dist(-0.01, 0.01);
    for (int i = 0; i < NUM_GROUPS; ++i) {
        for (int j = 0; j < NUM_GROUPS; ++j) {
            if (i != j) {
                interWeights[i][j] = dist(rng);
            } else {
                interWeights[i][j] = 0.0; // нет самосвязи
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

void NeuralFieldSystem::applyReentry(int iterations) {
    // Временные векторы для новых активностей групп
    std::vector<double> newGroupAvg(NUM_GROUPS, 0.0);
    std::vector<double> currAvg = getGroupAverages();
    // attention.computeAttention(currAvg);

for (int iter = 0; iter < iterations; ++iter) {

    attention.computeAttention(currAvg);  // ← СНАЧАЛА внимание
    // Очистка
    std::fill(newGroupAvg.begin(), newGroupAvg.end(), 0.0);
    // Вычисление вход групп
    for (int g = 0; g < NUM_GROUPS; ++g) {
        double input = 0.0;
        for (int h = 0; h < NUM_GROUPS; ++h) {
            float att = attention.attention_weights[h];
            input += interWeights[g][h] * currAvg[h] * att;
        }
            // Лишнее получение phiGrp
            // Ты его не используешь в этом месте.
            // auto& phiGrp = groups[g].getPhiNonConst();
            /*
            Было так:
            // Преобразуем вход в изменение активности групп (простая модель)
            newGroupAvg[g] = currAvg[g] + dt * input;
            // Ограничение
            if (newGroupAvg[g] > 1.0) newGroupAvg[g] = 1.0;
            if (newGroupAvg[g] < 0.0) newGroupAvg[g] = 0.0;
            */

            newGroupAvg[g] = currAvg[g] + dt * input;
            newGroupAvg[g] = std::clamp(newGroupAvg[g], 0.0, 1.0);
        }
        currAvg.swap(newGroupAvg);
    }

    // Применяем итоговые активности к группам (влияем на phi)
    for (int g = 0; g < NUM_GROUPS; ++g) {
        double targetAvg = currAvg[g];
        double currentAvg = groups[g].getAverageActivity();
        double diff = targetAvg - currentAvg;
        auto& phiGrp = groups[g].getPhiNonConst();
        for (int n = 0; n < GROUP_SIZE; ++n) {
            phiGrp[n] += dt * diff * 0.1; // коэффициент влияния
            phiGrp[n] = std::clamp(phiGrp[n], 0.0, 1.0);
        }
    }
}

void NeuralFieldSystem::step(float globalReward, bool useSTDP) {
    // 1. Эволюция
    for (auto& group : groups) group.evolve();
    
    // 2. Повторный вход
    applyReentry(3);
    
    // 3. Обучение с выбором метода
    static int stepCounter = 0; // или передавать из main
    for (auto& group : groups) {
        if (useSTDP) {
            group.learnSTDP(globalReward, stepCounter);
        } else {
            group.learnHebbian(globalReward);
        }
    }

    // 4. Градиентное обучение (если есть цель)
    for (auto& group : groups) {
        std::vector<double> target(GROUP_SIZE, 0.5); // пример цели
        group.computeGradients(target);
        group.applyGradients();
    }
    
    stepCounter++;
    flatDirty = true;
}

double NeuralFieldSystem::computeTotalEnergy() const {
    rebuildFlatVectors();
    double total = 0.0;
    for (double v : flatPhi) total += v * v;
    return total / TOTAL_NEURONS; // упрощённо
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
        // ограничение
        const double maxW = 0.5;
        if (interWeights[from][to] > maxW) interWeights[from][to] = maxW;
        if (interWeights[from][to] < -maxW) interWeights[from][to] = -maxW;
    }
}

void NeuralFieldSystem::weakenInterConnection(int from, int to, double delta) {
    strengthenInterConnection(from, to, -delta);
}

std::vector<float> NeuralFieldSystem::getFeatures() const {
    // Формируем 64 признака из средних активностей групп и их дисперсий
    std::vector<float> features(64, 0.0f);
    auto avgs = getGroupAverages();

    // Первые 32 признака - средние активности групп
    for (int g = 0; g < NUM_GROUPS; ++g) {
        features[g] = static_cast<float>(avgs[g]);
    }

    // Следующие 32 признака - среднеквадратичное отклонение внутри группы (или другая статистика)
    for (int g = 0; g < NUM_GROUPS; ++g) {
        const auto& phi = groups[g].getPhi();
        double sumSq = 0.0;
        for (double v : phi) sumSq += v * v;
        double rms = std::sqrt(sumSq / GROUP_SIZE);
        features[NUM_GROUPS + g] = static_cast<float>(rms);
    }

    return features;
}

// ---------- Заглушки для рефлексии (можно будет развить позже) ----------
NeuralFieldSystem::ReflectionState NeuralFieldSystem::getReflectionState() const {
    ReflectionState s;
    s.confidence = 0.5;
    s.curiosity = 0.5;
    s.satisfaction = 0.5;
    s.confusion = 0.5;
    s.attention_map.resize(4, 0.25);
    return s;
}

void NeuralFieldSystem::reflect() {
    // Здесь можно реализовать анализ состояний групп, но пока пусто
}

void NeuralFieldSystem::setGoal(const std::string& goal) {
    current_goal = goal;
}

bool NeuralFieldSystem::evaluateProgress() {
    return false; // заглушка
}

void NeuralFieldSystem::learnFromReflection(float outcome) {
    // заглушка
}

void NeuralFieldSystem::applyTargetedMutation(double strength, int targetType) {
    // Мутации на уровне групп
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> groupDist(0, NUM_GROUPS - 1);
    std::uniform_real_distribution<> paramDist(-strength, strength);

    if (targetType == 0) { // мутация межгрупповых связей
        for (int i = 0; i < NUM_GROUPS; ++i) {
            for (int j = 0; j < NUM_GROUPS; ++j) {
                if (i != j) {
                    interWeights[i][j] += paramDist(gen) * 0.1;
                }
            }
        }
    } else if (targetType == 1) { // мутация параметров группы
        int g = groupDist(gen);
        groups[g].setLearningRate(groups[g].getLearningRate() + paramDist(gen) * 0.01);
        groups[g].setThreshold(groups[g].getThreshold() + paramDist(gen) * 0.01);
    }
}

void NeuralFieldSystem::enterLowPowerMode() {
    // Уменьшаем активность групп
    for (auto& group : groups) {
        for (int i = 0; i < GROUP_SIZE; ++i) {
            // можно обнулить малые значения
        }
    }
}