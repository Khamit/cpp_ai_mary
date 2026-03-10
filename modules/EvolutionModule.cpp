// modules/EvolutionModule.cpp
#include "EvolutionModule.hpp"
#include <iostream>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <random>
#include <chrono>
#include "lang/LanguageModule.hpp"
#include "../core/MemoryManager.hpp"

using namespace std::filesystem;

// Конструкторы
EvolutionModule::EvolutionModule(ImmutableCore& core, const EvolutionConfig& config, MemoryManager& memory)
    : immutable_core(core), 
    memoryManager(memory),
      total_energy_consumed(0.0),
      total_steps(0),
      in_stasis(false),
      best_fitness(0.0),
      backup_dir("backups"),
      last_evaluation(std::chrono::steady_clock::now()),
      REDUCTION_COOLDOWN(std::chrono::seconds(config.reduction_cooldown_seconds)),
      MAX_REDUCTIONS_PER_MINUTE(config.max_reductions_per_minute),
      min_fitness_for_optimization(config.min_fitness_for_optimization),
      mutation_rate(0.01),
      last_reduction_time(std::chrono::steady_clock::now() - REDUCTION_COOLDOWN),
      reductions_this_minute(0)
{
    // init emb
    const size_t EMB_SIZE = 128; // размер embedding
    projectionMatrix.resize(NeuralFieldSystem::NUM_GROUPS);

    std::mt19937 gen(std::random_device{}());
    std::normal_distribution<float> dist(0.0f, 0.1f);

    for (auto& row : projectionMatrix) {
        row.resize(EMB_SIZE);
        for (auto& v : row) v = dist(gen);
    }
    // stat
    std::cout << "EvolutionModule initialized with config:" << std::endl;
    std::cout << "  - Reduction cooldown: " << REDUCTION_COOLDOWN.count() << "s" << std::endl;
    std::cout << "  - Max reductions: " << MAX_REDUCTIONS_PER_MINUTE << "/min" << std::endl;
    std::cout << "  - Min fitness for optimization: " << min_fitness_for_optimization << std::endl;
    
    std::filesystem::create_directories(backup_dir);
}

// Проверка возможности мутации
bool EvolutionModule::canReduceComplexity() {
    auto current_time = std::chrono::steady_clock::now();
    auto time_since_last = current_time - last_reduction_time;
    
    if (time_since_last < REDUCTION_COOLDOWN) {
        return false;
    }
    
    if (reductions_this_minute >= MAX_REDUCTIONS_PER_MINUTE) {
        if (time_since_last < std::chrono::minutes(1)) {
            return false;
        } else {
            reductions_this_minute = 0;
        }
    }
    
    return true;
}

std::vector<float> EvolutionModule::projectEmbeddingToGroups(const std::vector<float>& emb) {
    std::vector<float> groups(NeuralFieldSystem::NUM_GROUPS, 0.0f);

    for (size_t g = 0; g < groups.size(); ++g) {
    size_t dim = std::min(emb.size(), projectionMatrix[g].size());
    for (size_t i = 0; i < dim; ++i)
        groups[g] += emb[i] * projectionMatrix[g][i];
        groups[g] = std::tanh(groups[g]);
    }
    return groups;
}

void EvolutionModule::recordReduction() {
    last_reduction_time = std::chrono::steady_clock::now();
    reductions_this_minute++;
    
    std::cout << "Mutation called (" 
              << reductions_this_minute << "/" << MAX_REDUCTIONS_PER_MINUTE 
              << " this minute)" << std::endl;
}

// Оценка приспособленности - ТОЛЬКО языковая и энергетическая
void EvolutionModule::evaluateFitness(
    const NeuralFieldSystem& system,
    double step_time,
    LanguageModule& lang)
{
    // ===== 1. Языковая метрика =====
    double langFitness = std::clamp(lang.getLanguageFitness(), 0.0, 1.0);

    // ===== 2. Энергия (нормализованная) =====
    double energy = system.computeTotalEnergy();

    // Предполагаем нормальный диапазон энергии ~ [0, 2]
    double normalizedEnergy = std::clamp(energy / 2.0, 0.0, 1.0);

    // Чем меньше энергия — тем лучше
    double energyFitness = 1.0 - normalizedEnergy;

    // ===== 3. Стабильность (дисперсия групп) =====
    auto avgs = system.getGroupAverages();
    double mean = 0.0;
    for (double v : avgs) mean += v;
    mean /= avgs.size();

    double variance = 0.0;
    for (double v : avgs)
        variance += (v - mean) * (v - mean);

    variance /= avgs.size();

    double stabilityFitness = 1.0 - std::clamp(variance * 5.0, 0.0, 1.0);

    // ===== 4. Временная эффективность =====
    // нормируем относительно 5 мс
    double timePenalty = std::clamp(step_time / 0.005, 0.0, 1.0);
    double timeFitness = 1.0 - timePenalty;

    // ===== 5. Комбинирование (веса можно вынести в config) =====
    double rawFitness =
        0.55 * langFitness +
        0.20 * energyFitness +
        0.15 * stabilityFitness +
        0.10 * timeFitness;

    rawFitness = std::clamp(rawFitness, 0.0, 1.0);

    // ===== 6. EMA-сглаживание =====
    const double alpha = 0.1;
    if (total_steps == 0)
        current_metrics.overall_fitness = rawFitness;
    else
        current_metrics.overall_fitness =
            alpha * rawFitness +
            (1.0 - alpha) * current_metrics.overall_fitness;

    current_metrics.energy_score = energyFitness;
    current_metrics.performance_score = timeFitness;
    current_metrics.code_size_score = stabilityFitness;

    // ===== 7. Лучшее значение =====
    if (current_metrics.overall_fitness > best_fitness)
    {
        best_fitness = current_metrics.overall_fitness;

        std::cout << "\nNew best fitness: "
                  << best_fitness
                  << " | Lang: " << langFitness
                  << " | Energy: " << energyFitness
                  << " | Stability: " << stabilityFitness
                  << " | Time: " << timeFitness
                  << std::endl;

        createBackup();
    }

    history.push_back(current_metrics);
    total_steps++;

    // ===== 8. Проверка деградации =====
    if (total_steps % 500 == 0)
    {
        if (checkForDegradation())
        {
            std::cout << "⚠️ Degradation detected\n";
            if (current_metrics.overall_fitness < best_fitness * 0.65)
            {
                rollbackToBestVersion();
            }
        }
    }

    // ===== 9. Мутации =====
    if (total_steps % 1000 == 0 && !in_stasis)
    {
        if (current_metrics.overall_fitness < min_fitness_for_optimization)
        {
            mutateParameters(const_cast<NeuralFieldSystem&>(system));
        }
    }

    // ===== 10. Периодический вывод =====
    if (total_steps % 500 == 0)
    {
        std::cout << "Fitness Report | "
                  << "Lang: " << langFitness
                  << " | Energy: " << energyFitness
                  << " | Stability: " << stabilityFitness
                  << " | Time: " << timeFitness
                  << " | Overall: "
                  << current_metrics.overall_fitness
                  << std::endl;
    }
}
/*
// Устаревшие методы оценки - оставляем заглушки
double EvolutionModule::calculateCodeSizeScore() const { return 0.5; }
double EvolutionModule::calculatePerformanceScore(double) const { return 0.5; }
double EvolutionModule::calculateEnergyScore(const NeuralFieldSystem& system) const {
    double energy = system.computeTotalEnergy();
    return 1.0 / (energy + 0.001);
}
*/
// Предложение мутации
bool EvolutionModule::proposeMutation(NeuralFieldSystem& system) {
    /*
    // Новое
    // Только если система давно не улучшалась
    // Мутация архитектурных параметров (Уровень 3)
    for (auto& group : system.getGroups()) {
        if (shouldMutate()) {
            // Мутация базовой высоты покоя (resting elevation)
            float newBaseElevation = group.getBaseElevation() + randomMutation();
            group.setBaseElevation(newBaseElevation);
            
            std::cout << "Эволюционная мутация высоты группы" << std::endl;
        }
    }
*/
// Старое
    if (in_stasis) {
        applyMinimalMutation(system);
        return true;
    }
    
    if (!canReduceComplexity()) {
        std::cout << "Mutation on cooldown, skipping..." << std::endl;
        return false;
    }
    
    std::cout << "Proposing mutation at step " << total_steps << std::endl;
    
    if (immutable_core.requestPermission("system_mutation")) {
        mutateParameters(system);
        recordReduction();
        return true;
    } else {
        enterStasis(system);
        return false;
    }
}

// НОВЫЙ МЕТОД: мутация параметров групп
void EvolutionModule::mutateParameters(NeuralFieldSystem& system) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> normalDist(0.0, 0.1); // нормальное распределение для мутаций
    
    std::cout << "  Mutating group parameters..." << std::endl;
    
    for (auto& group : system.getGroups()) {
        // 10% шанс мутации для каждой группы
        if (std::uniform_real_distribution<>(0, 1)(gen) < mutation_rate) {
            // Мутация learning rate (до 20% изменения)
            double lr = group.getLearningRate();
            lr *= (1.0 + normalDist(gen) * 0.2);
            lr = std::clamp(lr, 0.0001, 0.1);
            group.setLearningRate(lr);
            
            // Мутация порога активации
            double thr = group.getThreshold();
            thr += normalDist(gen) * 0.05;
            thr = std::clamp(thr, 0.1, 0.9);
            group.setThreshold(thr);
        }
    }
    
    // Мутация межгрупповых связей
    std::cout << "  Mutating inter-group connections..." << std::endl;
    for (int i = 0; i < NeuralFieldSystem::NUM_GROUPS; ++i) {
        for (int j = i + 1; j < NeuralFieldSystem::NUM_GROUPS; ++j) {
            if (std::uniform_real_distribution<>(0, 1)(gen) < mutation_rate * 0.5) {
                double delta = normalDist(gen) * 0.05;
                system.strengthenInterConnection(i, j, delta);
            }
        }
    }
}

void EvolutionModule::testEvolutionMethods() {
    std::cout << "🧪 TESTING Evolution Methods:" << std::endl;
    std::cout << " - Mutation rate: " << mutation_rate << std::endl;
    std::cout << " - Best fitness: " << best_fitness << std::endl;
}

// ===== УДАЛЕНЫ ВСЕ МЕТОДЫ ОПТИМИЗАЦИИ КОДА =====
// evolveCodeOptimization, optimizeConfigFiles, createOptimalConfig,
// generateOptimizedCode, generateOptimizedDynamics, generateOptimizedLearning,
// analyzeCodeEfficiency - все удалены
/*
void EvolutionModule::optimizeSystemParameters() {
    // Заглушка - параметры мутируются в mutateParameters
}
*/

void EvolutionModule::applyMinimalMutation(NeuralFieldSystem& system) {
    std::cout << " Minimal mutation in stasis mode" << std::endl;
    mutateParameters(system); // просто вызываем ту же мутацию
}

// ========== МЕТОДЫ ЗАЩИТЫ И БЭКАПОВ ==========
bool EvolutionModule::createBackup() {
    try {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        
        std::string backup_name = backup_dir + "/backup_" + std::to_string(timestamp);
        std::filesystem::create_directories(backup_name);
        
        // Сохраняем метаданные бэкапа
        std::ofstream meta(backup_name + "/backup_meta.txt");
        if (meta.is_open()) {
            meta << "Backup created: " << timestamp << "\n";
            meta << "Fitness: " << current_metrics.overall_fitness << "\n";
            meta << "Steps: " << total_steps << "\n";
            meta << "Best fitness: " << best_fitness << "\n";
            meta.close();
        }
        
        std::cout << "Backup created: " << backup_name << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "Backup creation failed: " << e.what() << std::endl;
        return false;
    }
}

bool EvolutionModule::restoreFromBackup() {
    try {
        std::string latest_backup;
        std::filesystem::file_time_type latest_time = std::filesystem::file_time_type::min();
        
        for (const auto& entry : std::filesystem::directory_iterator(backup_dir)) {
            if (entry.is_directory() && 
                entry.path().string().find("backup_") != std::string::npos) {
                auto write_time = std::filesystem::last_write_time(entry);
                if (write_time > latest_time) {
                    latest_time = write_time;
                    latest_backup = entry.path().string();
                }
            }
        }
        
        if (latest_backup.empty()) {
            std::cout << "No backups found to restore" << std::endl;
            return false;
        }
        
        std::cout << "System restored from backup: " << latest_backup << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "Backup restoration failed: " << e.what() << std::endl;
        return false;
    }
}

bool EvolutionModule::checkForDegradation() {
    if (history.size() < 10) return false;
    
    double recent_avg = 0.0;
    int count = 0;
    
    for (int i = std::max(0, (int)history.size() - 10); i < history.size(); i++) {
        recent_avg += history[i].overall_fitness;
        count++;
    }
    recent_avg /= count;
    
    return (current_metrics.overall_fitness < best_fitness * 0.7) ||
           (recent_avg < best_fitness * 0.8);
}

void EvolutionModule::rollbackToBestVersion() {
    std::cout << "Attempting to rollback to best version..." << std::endl;
    
    if (restoreFromBackup()) {
        best_fitness = 0.0;
        current_metrics = EvolutionMetrics();
        std::cout << "Rollback completed successfully" << std::endl;
    } else {
        std::cout << "Rollback failed" << std::endl;
    }
}

double EvolutionModule::getCurrentCodeHash() const {
    return static_cast<double>(total_steps); // простой хэш
}

bool EvolutionModule::validateImprovement() const {
    if (history.size() < 2) return true;
    
    const auto& previous = history[history.size() - 2];
    const auto& current = history[history.size() - 1];
    
    double improvement = (current.overall_fitness - previous.overall_fitness) / previous.overall_fitness;
    return improvement > -0.1;
}

void EvolutionModule::saveEvolutionState() {
    EvolutionDumpData data;
    data.generation = total_steps / 1000;
    data.metrics = current_metrics;
    data.energy_state = 0; // или вычислить
    data.code_hash = getCurrentCodeHash();
    
    // Используем MemoryManager
    memoryManager.saveEvolutionState(data, "evolution_state.bin");
}

void EvolutionModule::enterStasis(NeuralFieldSystem& system) {
    in_stasis = true;
    system.enterLowPowerMode();
    std::cout << "SYSTEM ENTERED STASIS MODE" << std::endl;
    saveEvolutionState();
}

void EvolutionModule::exitStasis() {
    in_stasis = false;
    std::cout << "SYSTEM EXITED STASIS MODE" << std::endl;
}

bool EvolutionModule::isInStasis() const {
    return in_stasis;
}