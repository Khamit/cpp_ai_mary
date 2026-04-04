// modules/EvolutionModule.cpp
#include "EvolutionModule.hpp"
#include <iostream>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <random>
#include <chrono>
#include "lang/LanguageModule.hpp"
#include "../core/MemoryManager.hpp"

using namespace std::filesystem;

EvolutionModule::EvolutionModule(ImmutableCore& core, const EvolutionConfig& config, MemoryManager& memory)
    : immutable_core(core), 
      memoryManager(memory),
      current_metrics(),  
      detector_(),        
      history(),
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
    const size_t EMB_SIZE = 128;
    projectionMatrix.resize(NeuralFieldSystem::NUM_GROUPS);

    std::mt19937 gen(std::random_device{}());
    std::normal_distribution<float> dist(0.0f, 0.1f);

    for (auto& row : projectionMatrix) {
        row.resize(EMB_SIZE);
        for (auto& v : row) v = dist(gen);
    }
    
    std::cout << "EvolutionModule initialized with config:" << std::endl;
    std::cout << "  - Reduction cooldown: " << REDUCTION_COOLDOWN.count() << "s" << std::endl;
    std::cout << "  - Max reductions: " << MAX_REDUCTIONS_PER_MINUTE << "/min" << std::endl;
    std::cout << "  - Min fitness for optimization: " << min_fitness_for_optimization << std::endl;
    
    std::filesystem::create_directories(backup_dir);
}

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

void EvolutionModule::evaluateFitness(
    const NeuralFieldSystem& system,
    double step_time,
    LanguageModule& lang)
{
    // ===== 1. Языковая метрика =====
    double langFitness = 0.5;
    try {
        langFitness = std::clamp(lang.getLanguageFitness(), 0.0, 1.0);
    } catch (const std::exception& e) {
        std::cerr << "Error getting language fitness: " << e.what() << std::endl;
        langFitness = 0.5;
    }
    
    if (langFitness > 0.95 && total_steps < 10000) {
        langFitness = 0.6;
    }
    
    static double last_lang_fitness = 0.0;
    if (langFitness > 0.8 && last_lang_fitness < 0.3 && total_steps > 1000) {
        langFitness = last_lang_fitness * 1.2;
    }
    last_lang_fitness = langFitness;

    // ===== 2. Энергия (должна быть ненулевой) =====
    double energy = system.computeTotalEnergy();
    
    // Отладка энергии
    static int energy_debug = 0;
    if (total_steps % 500 == 0 && energy_debug++ < 10) {
        std::cout << "DEBUG: raw energy = " << energy << std::endl;
    }
    
    double normalizedEnergy = std::clamp(energy / 10.0, 0.0, 1.0);
    double energyFitness = 1.0 - normalizedEnergy;
    
    // Если энергия нулевая — это аномалия
    if (energy < 0.01 && total_steps > 1000) {
        std::cout << "WARNING: Energy is zero at step " << total_steps << std::endl;
        energyFitness = 0.3;  // разумное значение по умолчанию
    }

    // ===== 3. Энтропия =====
    double entropy = system.computeSystemEntropy();
    double entropyFitness = std::clamp(entropy / 3.0, 0.0, 1.0);

    // ===== 4. Временная эффективность =====
    double timePenalty = std::clamp(step_time / 0.005, 0.0, 1.0);
    double timeFitness = 1.0 - timePenalty;

    // ===== 5. Орбитальное здоровье =====
    double orbitHealth = 0.0;
    const auto& groups = system.getGroups();
    int elite_count = 0;
    int active_count = 0;
    for (int g = 16; g <= 21 && g < groups.size(); g++) {
        for (int i = 0; i < 32; i++) {
            int level = groups[g].getOrbitLevel(i);
            if (level >= 2) {
                elite_count++;
            }
            if (level > 0) {
                active_count++;
            }
        }
    }
    orbitHealth = 0.6 * (static_cast<double>(elite_count) / (6 * 32)) +
                  0.4 * (static_cast<double>(active_count) / (6 * 32));

    // ===== 6. Финальная формула =====
    double rawFitness = 0.40 * langFitness + 
                        0.20 * energyFitness + 
                        0.20 * entropyFitness + 
                        0.10 * timeFitness + 
                        0.10 * orbitHealth;
    rawFitness = std::clamp(rawFitness, 0.0, 1.0);

    // Защита от аномалий
    bool anomaly_detected = false;
    if (rawFitness > 0.8 && langFitness < 0.2) {
        rawFitness = 0.5;
        anomaly_detected = true;
    }
    
    static double last_raw_fitness = 0.0;
    if (total_steps > 1000) {
        double growth = rawFitness - last_raw_fitness;
        if (growth > 0.3) {
            rawFitness = last_raw_fitness + 0.1;
            anomaly_detected = true;
        }
    }
    last_raw_fitness = rawFitness;
    
    if (langFitness > 0.99 && energyFitness > 0.99 && 
        entropyFitness > 0.99 && timeFitness > 0.99 && orbitHealth > 0.99) {
        rawFitness = 0.5;
        anomaly_detected = true;
    }

    // EMA-сглаживание
    const double alpha = 0.1;
    if (total_steps == 0)
        current_metrics.overall_fitness = rawFitness;
    else
        current_metrics.overall_fitness = alpha * rawFitness + (1.0 - alpha) * current_metrics.overall_fitness;

    current_metrics.energy_score = energyFitness;
    current_metrics.performance_score = timeFitness;
    current_metrics.code_size_score = entropyFitness;

    // Обновление лучшего значения
    if (current_metrics.overall_fitness > best_fitness) {
        bool is_valid_record = true;
        if (current_metrics.overall_fitness > 0.9 && langFitness < 0.3) is_valid_record = false;
        if (current_metrics.overall_fitness > 0.8 && total_steps < 5000) is_valid_record = false;
        
        if (is_valid_record) {
            best_fitness = current_metrics.overall_fitness;
            std::cout << "\nNew best fitness: "
                      << std::fixed << std::setprecision(2) << best_fitness
                      << " | Lang: " << std::setprecision(2) << langFitness
                      << " | Energy: " << std::setprecision(2) << energyFitness
                      << " | Entropy: " << std::setprecision(2) << entropyFitness
                      << " | Time: " << std::setprecision(2) << timeFitness
                      << " | Orbit: " << std::setprecision(2) << orbitHealth
                      << std::endl;
            
            // При достижении нового рекорда делаем бэкап
            createBackup();
        }
    }

    history.push_back(current_metrics);
    total_steps++;

    // Проверка деградации
    if (total_steps % 500 == 0 && checkForDegradation()) {
        std::cout << "⚠️ Degradation detected\n";
        if (current_metrics.overall_fitness < best_fitness * 0.65) {
            rollbackToBestVersion();
        }
    }

    // Периодический вывод
    if (total_steps % 500 == 0) {
        std::cout << "Fitness Report | "
                  << "Lang: " << std::setprecision(2) << langFitness
                  << " | Energy: " << std::setprecision(2) << energyFitness
                  << " | Entropy: " << std::setprecision(2) << entropyFitness
                  << " | Time: " << std::setprecision(2) << timeFitness
                  << " | Orbit: " << std::setprecision(2) << orbitHealth
                  << " | Overall: " << std::setprecision(2) << current_metrics.overall_fitness
                  << std::endl;
    }
}

// ========== ИСПРАВЛЕННАЯ МУТАЦИЯ ==========
bool EvolutionModule::proposeMutation(NeuralFieldSystem& system) {
    if (in_stasis) {
        applyMinimalMutation(system);
        return true;
    }
    
    if (!canReduceComplexity()) {
        std::cout << "Mutation on cooldown, skipping..." << std::endl;
        return false;
    }
    
    // ===== НОВОЕ: Проверяем, нужна ли мутация =====
    if (current_metrics.overall_fitness > best_fitness * 0.9 && best_fitness > 0.4) {
        // Если система хорошо работает, не мутируем
        std::cout << "System performing well, skipping mutation" << std::endl;
        return false;
    }
    
    // ===== НОВОЕ: Анализируем, что нужно улучшить =====
    std::cout << "Analyzing system before mutation..." << std::endl;
    
    const auto& groups = system.getGroups();
    
    // Собираем статистику
    int elite_neurons = 0;
    int dead_neurons = 0;
    double avg_connectivity = 0.0;
    int connections_count = 0;
    
    for (int g = 0; g < NeuralFieldSystem::NUM_GROUPS; g++) {
        for (int i = 0; i < 32; i++) {
            int level = groups[g].getOrbitLevel(i);
            if (level >= 3) elite_neurons++;
            if (level == 0) dead_neurons++;
            
            // Считаем среднюю силу связей
            const auto& weights = groups[g].getWeights();
            for (int j = i + 1; j < 32; j++) {
                avg_connectivity += std::abs(weights[i][j]);
                connections_count++;
            }
        }
    }
    
    if (connections_count > 0) avg_connectivity /= connections_count;
    
    std::cout << "  Elite neurons: " << elite_neurons 
              << " | Dead neurons: " << dead_neurons
              << " | Avg connectivity: " << avg_connectivity << std::endl;
    
    // ===== НОВОЕ: Решение, что мутировать =====
    if (immutable_core.requestPermission("system_mutation")) {
        // Сохраняем состояние ДО мутации
        saveEvolutionState();
        
        // Выбираем тип мутации на основе анализа
        if (dead_neurons > 500 && current_metrics.overall_fitness < 0.3) {
            // Слишком много мёртвых нейронов - реанимация
            std::cout << "  Applying REANIMATION mutation (too many dead neurons)" << std::endl;
            applyReanimationMutation(system);
        } 
        else if (elite_neurons < 100 && total_steps > 5000) {
            // Мало элитных нейронов - усиление
            std::cout << "  Applying ELITE_BOOST mutation (too few elite neurons)" << std::endl;
            applyEliteBoostMutation(system);
        }
        else if (avg_connectivity < 0.1 && total_steps > 2000) {
            // Слишком слабые связи - укрепление
            std::cout << "  Applying CONNECTION_STRENGTHEN mutation (weak connections)" << std::endl;
            applyConnectionStrengthenMutation(system);
        }
        else {
            // Стандартная мутация с учётом орбит
            std::cout << "  Applying STANDARD mutation" << std::endl;
            mutateParametersSmart(system);
        }
        
        recordReduction();
        return true;
    } else {
        enterStasis(system);
        return false;
    }
}

// ===== НОВЫЕ МЕТОДЫ МУТАЦИЙ =====

void EvolutionModule::applyReanimationMutation(NeuralFieldSystem& system) {
    auto& groups = system.getGroupsNonConst();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> boost_dist(0.5, 1.5);
    
    for (int g = 0; g < NeuralFieldSystem::NUM_GROUPS; g++) {
        for (int i = 0; i < 32; i++) {
            if (groups[g].getOrbitLevel(i) == 0) {
                // Реанимируем мёртвые нейроны
                groups[g].publicPromoteToBaseOrbit(i);
                
                // Даём случайную массу
                double new_mass = 0.8 + boost_dist(gen) * 0.5;
                groups[g].setMass(i, new_mass);
                
                // Укрепляем связи с элитными нейронами
                for (int j = 0; j < 32; j++) {
                    if (groups[g].getOrbitLevel(j) >= 2) {
                        double old_weight = groups[g].getWeight(i, j);
                        groups[g].setWeight(i, j, old_weight + 0.1);
                    }
                }
            }
        }
    }
    
    std::cout << "  Reanimated dead neurons" << std::endl;
}

void EvolutionModule::applyEliteBoostMutation(NeuralFieldSystem& system) {
    auto& groups = system.getGroupsNonConst();
    std::random_device rd;
    std::mt19937 gen(rd());
    
    for (int g = 16; g <= 21 && g < groups.size(); g++) {
        // Находим нейроны с потенциалом для повышения
        std::vector<std::pair<double, int>> candidates;
        
        for (int i = 0; i < 32; i++) {
            int level = groups[g].getOrbitLevel(i);
            if (level == 2) {  // кандидаты на повышение
                double activity = groups[g].getPositions()[i].norm();
                candidates.push_back({activity, i});
            }
        }
        
        // Сортируем по активности
        std::sort(candidates.begin(), candidates.end(), 
                  [](const auto& a, const auto& b) { return a.first > b.first; });
        
        // Повышаем топ-3 кандидата
        for (int k = 0; k < std::min(3, (int)candidates.size()); k++) {
            int idx = candidates[k].second;
            groups[g].publicPromoteToBaseOrbit(idx);
            groups[g].setMass(idx, 2.0);
            std::cout << "  Boosted neuron (" << g << "," << idx << ") to elite" << std::endl;
        }
    }
}

void EvolutionModule::applyConnectionStrengthenMutation(NeuralFieldSystem& system) {
    auto& groups = system.getGroupsNonConst();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> norm_dist(0.0, 0.05);
    
    for (int g = 0; g < NeuralFieldSystem::NUM_GROUPS; g++) {
        for (int i = 0; i < 32; i++) {
            int level_i = groups[g].getOrbitLevel(i);
            if (level_i < 2) continue;  // только для активных нейронов
            
            for (int j = i + 1; j < 32; j++) {
                int level_j = groups[g].getOrbitLevel(j);
                if (level_j < 2) continue;
                
                double old_weight = groups[g].getWeight(i, j);
                // Укрепляем только если оба нейрона активны
                if (std::abs(old_weight) > 0.1) {
                    double boost = 0.05 * (1.0 + norm_dist(gen));
                    double new_weight = old_weight + boost;
                    groups[g].setWeight(i, j, std::min(new_weight, 1.0));
                }
            }
        }
    }
    
    std::cout << "  Strengthened connections between active neurons" << std::endl;
}

void EvolutionModule::mutateParametersSmart(NeuralFieldSystem& system) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> normalDist(0.0, 0.05);
    std::uniform_real_distribution<> uniformDist(0.0, 1.0);
    
    std::cout << "  Smart mutation..." << std::endl;
    
    auto& groups = system.getGroupsNonConst();
    
    for (auto& group : groups) {
        // 1. Мутация параметров с учётом состояния
        if (uniformDist(gen) < mutation_rate * 0.5) {
            // Только если группа не слишком хороша
            double group_health = 0.0;
            for (int i = 0; i < 32; i++) {
                if (group.getOrbitLevel(i) >= 2) group_health++;
            }
            group_health /= 32.0;
            
            if (group_health < 0.6) {  // только для слабых групп
                double lr = group.getLearningRate();
                lr *= (1.0 + normalDist(gen) * 0.2);
                lr = std::clamp(lr, 0.0005, 0.05);
                group.setLearningRate(lr);
            }
        }
    }
    
    // 2. Мутация межгрупповых связей - только слабые
    int mutated = 0;
    for (int i = 0; i < NeuralFieldSystem::NUM_GROUPS; ++i) {
        for (int j = i + 1; j < NeuralFieldSystem::NUM_GROUPS; ++j) {
            double current_weight = system.getInterWeights()[i][j];
            if (std::abs(current_weight) < 0.1 && uniformDist(gen) < mutation_rate * 0.3) {
                // Укрепляем очень слабые связи
                double delta = 0.02 + normalDist(gen) * 0.01;
                system.strengthenInterConnection(i, j, delta);
                mutated++;
            }
            else if (std::abs(current_weight) > 0.4 && uniformDist(gen) < mutation_rate * 0.1) {
                // Слегка ослабляем слишком сильные (чтобы не было перекоса)
                double delta = -0.01;
                system.weakenInterConnection(i, j, delta);
                mutated++;
            }
        }
    }
    
    std::cout << "  Mutated " << mutated << " inter-group connections" << std::endl;
}

void EvolutionModule::applyMinimalMutation(NeuralFieldSystem& system) {
    std::cout << " Minimal mutation in stasis mode" << std::endl;
    
    auto& groups = system.getGroupsNonConst();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dist(0.0, 0.05);
    
    // Только очень мягкие изменения
    for (auto& group : groups) {
        double lr = group.getLearningRate();
        lr *= (0.99 + dist(gen));
        lr = std::clamp(lr, 0.001, 0.02);
        group.setLearningRate(lr);
    }
}

// ========== ОСТАЛЬНЫЕ МЕТОДЫ (без изменений) ==========

void EvolutionModule::testEvolutionMethods() {
    std::cout << "🧪 TESTING Evolution Methods:" << std::endl;
    std::cout << " - Mutation rate: " << mutation_rate << std::endl;
    std::cout << " - Best fitness: " << best_fitness << std::endl;
}

bool EvolutionModule::createBackup() {
    try {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        
        std::string backup_name = backup_dir + "/backup_" + std::to_string(timestamp);
        std::filesystem::create_directories(backup_name);
        
        std::ofstream meta(backup_name + "/backup_meta.txt");
        if (meta.is_open()) {
            meta << "Backup created: " << timestamp << "\n";
            meta << "Fitness: " << current_metrics.overall_fitness << "\n";
            meta << "Steps: " << total_steps << "\n";
            meta << "Best fitness: " << best_fitness << "\n";
            meta.close();
        }
        
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
    return static_cast<double>(total_steps);
}

bool EvolutionModule::validateImprovement() const {
    if (history.size() < 2) return true;
    
    const auto& previous = history[history.size() - 2];
    const auto& current = history[history.size() - 1];
    
    double improvement = (current.overall_fitness - previous.overall_fitness) / (previous.overall_fitness + 1e-6);
    return improvement > -0.1;
}

void EvolutionModule::saveEvolutionState() {
    EvolutionDumpData data;
    data.generation = total_steps / 10000;
    data.metrics = current_metrics;
    data.energy_state = 0;
    data.code_hash = getCurrentCodeHash();
    
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