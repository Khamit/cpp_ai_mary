#include "EvolutionModule.hpp"
#include <iostream>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <random>
#include <chrono>
#include <functional> // для std::remove и std::remove_if

// Добавим using directive для упрощения
using namespace std::filesystem;

// Старый конструктор (для обратной совместимости)
EvolutionModule::EvolutionModule(ImmutableCore& core) 
    : immutable_core(core), 
      total_energy_consumed(0.0),
      total_steps(0),
      in_stasis(false),
      best_fitness(0.0),
      backup_dir("backups"),
      last_evaluation(std::chrono::steady_clock::now()),
      REDUCTION_COOLDOWN(std::chrono::seconds(30)),
      MAX_REDUCTIONS_PER_MINUTE(2),
      min_fitness_for_optimization(0.8), // Добавлена инициализация
      last_reduction_time(std::chrono::steady_clock::now() - REDUCTION_COOLDOWN),
      reductions_this_minute(0)
{
    std::cout << "EvolutionModule initialized with defaults" << std::endl;
    std::filesystem::create_directories(backup_dir);
    createBackup();
}

// Новый конструктор с конфигурацией
EvolutionModule::EvolutionModule(ImmutableCore& core, const EvolutionConfig& config)
    : immutable_core(core), 
      total_energy_consumed(0.0),
      total_steps(0),
      in_stasis(false),
      best_fitness(0.0),
      backup_dir("backups"),
      last_evaluation(std::chrono::steady_clock::now()),
      REDUCTION_COOLDOWN(std::chrono::seconds(config.reduction_cooldown_seconds)),
      MAX_REDUCTIONS_PER_MINUTE(config.max_reductions_per_minute),
      min_fitness_for_optimization(config.min_fitness_for_optimization), // Инициализация из конфига
      last_reduction_time(std::chrono::steady_clock::now() - REDUCTION_COOLDOWN),
      reductions_this_minute(0)
{
    std::cout << "EvolutionModule initialized with config:" << std::endl;
    std::cout << "  - Reduction cooldown: " << REDUCTION_COOLDOWN.count() << "s" << std::endl;
    std::cout << "  - Max reductions: " << MAX_REDUCTIONS_PER_MINUTE << "/min" << std::endl;
    std::cout << "  - Min fitness for optimization: " << min_fitness_for_optimization << std::endl;
    
    std::filesystem::create_directories(backup_dir);
    createBackup();
}


// НОВЫЙ МЕТОД: проверка можно ли вызывать reduce_complexity
bool EvolutionModule::canReduceComplexity() {
    auto current_time = std::chrono::steady_clock::now();
    auto time_since_last = current_time - last_reduction_time;
    
    // КУЛДАУН: не чаще чем раз в 30 секунд
    if (time_since_last < REDUCTION_COOLDOWN) {
        return false;
    }
    
    // ЛИМИТ: не более 2 раз в минуту
    if (reductions_this_minute >= MAX_REDUCTIONS_PER_MINUTE) {
        // Проверяем, не прошла ли минута
        if (time_since_last < std::chrono::minutes(1)) {
            return false;
        } else {
            // Сбрасываем счетчик
            reductions_this_minute = 0;
        }
    }
    
    return true;
}

// НОВЫЙ МЕТОД: запись вызова reduce
void EvolutionModule::recordReduction() {
    last_reduction_time = std::chrono::steady_clock::now();
    reductions_this_minute++;
    
    std::cout << "📉 Reduction complexity called (" 
              << reductions_this_minute << "/" << MAX_REDUCTIONS_PER_MINUTE 
              << " this minute)" << std::endl;
}


void EvolutionModule::evaluateFitness(const NeuralFieldSystem& system, double step_time) {
    std::cout << "🔍 DEBUG: evaluateFitness called at step " << total_steps << std::endl;
    
    double code_score = calculateCodeSizeScore();
    double perf_score = calculatePerformanceScore(step_time);
    double energy_score = calculateEnergyScore(system);
    
    std::cout << "🔍 DEBUG: Raw scores - Code: " << code_score 
              << ", Perf: " << perf_score 
              << ", Energy: " << energy_score << std::endl;
    
    current_metrics.code_size_score = code_score;
    current_metrics.performance_score = perf_score;
    current_metrics.energy_score = energy_score;
    current_metrics.overall_fitness = (code_score + perf_score + energy_score) / 3.0;
    
    std::cout << "🔍 DEBUG: Overall fitness: " << current_metrics.overall_fitness << std::endl;
    
    // Обновляем лучшую приспособленность
    if (current_metrics.overall_fitness > best_fitness) {
        best_fitness = current_metrics.overall_fitness;
        std::cout << "🎉 New best fitness: " << best_fitness << std::endl;
        
        // Создаем бэкап при улучшении
        if (createBackup()) {
            std::cout << "💾 Backup created for best version" << std::endl;
        }
    }
    
    history.push_back(current_metrics);
    total_steps++;
    
    // Проверяем деградацию каждые 500 шагов
    if (total_steps % 500 == 0) {
        if (checkForDegradation()) {
            std::cout << "⚠️ Performance degradation detected!" << std::endl;
            if (current_metrics.overall_fitness < best_fitness * 0.7) {
                std::cout << "🔄 Rolling back to best version..." << std::endl;
                rollbackToBestVersion();
            }
        }
    }
    
    // ЭВОЛЮЦИЯ КОДА каждые 1000 шагов
    if (total_steps % 1000 == 0 && !in_stasis) {
        evolveCodeOptimization();
    }
    
    // Вывод отладочной информации каждые 500 шагов
    if (total_steps % 500 == 0) {
        std::cout << "Evolution Metrics - "
                  << "Code: " << current_metrics.code_size_score
                  << " | Perf: " << current_metrics.performance_score
                  << " | Energy: " << current_metrics.energy_score
                  << " | Overall: " << current_metrics.overall_fitness 
                  << " | Best: " << best_fitness << std::endl;
    }
}

// Остальные методы calculateCodeSizeScore, calculatePerformanceScore, calculateEnergyScore
// остаются такими же как в предыдущей версии...

double EvolutionModule::calculateCodeSizeScore() const {
    try {
        size_t total_size = 0;
        int file_count = 0;
        
        for (const auto& entry : std::filesystem::recursive_directory_iterator(".")) {
            if (entry.path().extension() == ".cpp" || entry.path().extension() == ".hpp") {
                total_size += entry.file_size();
                file_count++;
            }
        }
        
        if (file_count == 0) return 0.5;
        
        // Нормализуем: 1.0 = меньше 50KB, 0.0 = больше 500KB
        double normalized = 1.0 - (total_size / 500000.0);
        return std::max(0.1, std::min(1.0, normalized));
    } catch (...) {
        return 0.5;
    }
}

double EvolutionModule::calculatePerformanceScore(double step_time) const {
    // Целевое время шага: 0.001 секунды
    double target_time = 0.001;
    if (step_time <= target_time) return 1.0;
    
    double ratio = target_time / step_time;
    return std::pow(ratio, 0.5);
}

double EvolutionModule::calculateEnergyScore(const NeuralFieldSystem& system) const {
    double energy = system.computeTotalEnergy();
    
    // Целевая энергия: 0.001
    double target_energy = 0.001;
    if (energy <= target_energy) return 1.0;
    
    double ratio = target_energy / energy;
    return std::pow(ratio, 0.3);
}

bool EvolutionModule::proposeMutation(NeuralFieldSystem& system) {
    if (in_stasis) {
        applyMinimalMutation(system);
        return true;
    }
    
    // 🔧 ЗАЩИТА ОТ ЧАСТЫХ ВЫЗОВОВ
    if (!canReduceComplexity()) {
        std::cout << "⏳ Reduction complexity on cooldown, skipping..." << std::endl;
        return false;
    }
    
    std::cout << "🔄 Proposing mutation at step " << total_steps << std::endl;
    
    if (createBackup()) {
        std::cout << "💾 Pre-mutation backup created" << std::endl;
    }
    
    if (immutable_core.requestPermission("system_mutation")) {
        system.applyTargetedMutation(0.05, 0);
        optimizeSystemParameters();
        
        evolveCodeOptimization();
        
        // 🔧 ЗАПИСЫВАЕМ ВЫЗОВ
        recordReduction();
        
        return true;
    } else {
        enterStasis(system);
        return false;
    }
}


void EvolutionModule::testEvolutionMethods() {
    std::cout << "🧪 TESTING Evolution Methods:" << std::endl;
    std::cout << " - calculateCodeSizeScore: " << calculateCodeSizeScore() << std::endl;
    std::cout << " - getCurrentCodeHash: " << getCurrentCodeHash() << std::endl;
    
    // Тестируем создание бэкапа
    if (createBackup()) {
        std::cout << "✅ Backup test: PASSED" << std::endl;
    } else {
        std::cout << "❌ Backup test: FAILED" << std::endl;
    }
}

// НОВЫЙ МЕТОД: Реальная эволюция кода
// Обновите метод evolveCodeOptimization чтобы использовать min_fitness_for_optimization:
void EvolutionModule::evolveCodeOptimization() {
    // 🔧 ТОЛЬКО ЕСЛИ ФИТНЕС НИЗКИЙ
    if (current_metrics.overall_fitness > min_fitness_for_optimization) {
        std::cout << "✅ Fitness good (" << current_metrics.overall_fitness 
                  << " > " << min_fitness_for_optimization << "), skipping optimization" << std::endl;
        return;
    }
    
    std::cout << "🔍 DEBUG: evolveCodeOptimization() CALLED at step " << total_steps << std::endl;
    std::cout << "🧬 EVOLVING CODE OPTIMIZATION (fitness: " << current_metrics.overall_fitness 
              << " < " << min_fitness_for_optimization << ")..." << std::endl;
    
    // Создаем бэкап перед изменениями
    if (!createBackup()) {
        std::cout << "❌ Cannot proceed with evolution - backup failed" << std::endl;
        return;
    }
    
    // 1. Оптимизация конфигурационных параметров
    optimizeConfigFiles();
    
    // 2. Генерация более эффективного кода
    generateOptimizedCode();
    
    // 3. Анализ и удаление неэффективного кода
    analyzeCodeEfficiency();
    
    // Проверяем улучшилась ли ситуация
    if (!validateImprovement()) {
        std::cout << "⚠️ Evolution didn't improve system, considering rollback..." << std::endl;
    }
}

void EvolutionModule::optimizeConfigFiles() {
    try {
        // Читаем текущий конфиг
        std::ifstream config_file("config/system_config.json");
        if (!config_file.is_open()) {
            // Создаем базовый конфиг если не существует
            createOptimalConfig();
            return;
        }
        
        std::string content((std::istreambuf_iterator<char>(config_file)), 
                           std::istreambuf_iterator<char>());
        config_file.close();
        
        // Простая оптимизация: уменьшаем параметры для экономии ресурсов
        if (content.find("\"learning_rate\": 0.001") != std::string::npos) {
            std::cout << "📉 Optimizing learning rate..." << std::endl;
        }
        
        if (content.find("\"damping_factor\": 0.999") != std::string::npos) {
            std::cout << "📉 Optimizing damping factor..." << std::endl;
        }
        
    } catch (...) {
        std::cout << "⚠️ Config optimization failed" << std::endl;
    }
}

void EvolutionModule::createOptimalConfig() {
    std::ofstream config_file("config/evolved_config.json");
    if (config_file.is_open()) {
        config_file << "{\n";
        config_file << "  \"system\": {\n";
        config_file << "    \"Nside\": 16,\n";
        config_file << "    \"dt\": 0.002,\n";
        config_file << "    \"m\": 0.8,\n";
        config_file << "    \"lam\": 0.3\n";
        config_file << "  },\n";
        config_file << "  \"modules\": {\n";
        config_file << "    \"learning\": {\n";
        config_file << "      \"learning_rate\": 0.0005,\n";
        config_file << "      \"weight_decay\": 0.998\n";
        config_file << "    }\n";
        config_file << "  }\n";
        config_file << "}\n";
        config_file.close();
        std::cout << "✅ Created optimized config: config/evolved_config.json" << std::endl;
    }
}

void EvolutionModule::generateOptimizedCode() {
    // Генерируем оптимизированные версии модулей
    generateOptimizedDynamics();
    generateOptimizedLearning();
}

void EvolutionModule::generateOptimizedDynamics() {
    std::ofstream dyn_file("modules/DynamicsOptimized.hpp");
    if (dyn_file.is_open()) {
        dyn_file << "#pragma once\n";
        dyn_file << "#include \"../core/NeuralFieldSystem.hpp\"\n\n";
        dyn_file << "// OPTIMIZED VERSION - Generated by EvolutionModule\n";
        dyn_file << "// More efficient damping and limits\n";
        dyn_file << "class DynamicsOptimized {\n";
        dyn_file << "public:\n";
        dyn_file << "    void applyOptimizedDynamics(NeuralFieldSystem& system) {\n";
        dyn_file << "        auto& phi = system.getPhi();\n";
        dyn_file << "        auto& pi = system.getPi();\n";
        dyn_file << "        \n";
        dyn_file << "        // Optimized damping\n";
        dyn_file << "        for (int i = 0; i < system.N; i++) {\n";
        dyn_file << "            pi[i] *= 0.995;\n";
        dyn_file << "        }\n";
        dyn_file << "        \n";
        dyn_file << "        // Optimized limits\n";
        dyn_file << "        for (int i = 0; i < system.N; i++) {\n";
        dyn_file << "            if (phi[i] > 1.5) phi[i] = 1.5;\n";
        dyn_file << "            if (phi[i] < -1.5) phi[i] = -1.5;\n";
        dyn_file << "        }\n";
        dyn_file << "    }\n";
        dyn_file << "};\n";
        dyn_file.close();
        std::cout << "✅ Generated optimized dynamics module" << std::endl;
    }
}

void EvolutionModule::generateOptimizedLearning() {
    std::ofstream learn_file("modules/LearningOptimized.hpp");
    if (learn_file.is_open()) {
        learn_file << "#pragma once\n";
        learn_file << "#include \"../core/NeuralFieldSystem.hpp\"\n\n";
        learn_file << "// OPTIMIZED VERSION - Generated by EvolutionModule\n";
        learn_file << "// More efficient Hebbian learning\n";
        learn_file << "class LearningOptimized {\n";
        learn_file << "public:\n";
        learn_file << "    void applyOptimizedLearning(NeuralFieldSystem& system) {\n";
        learn_file << "        auto& W = system.getWeights();\n";
        learn_file << "        const auto& phi = system.getPhi();\n";
        learn_file << "        \n";
        learn_file << "        // Sparse weight updates for efficiency\n";
        learn_file << "        for (int i = 0; i < system.N; i += 2) {\n";
        learn_file << "            for (int j = i + 1; j < system.N; j += 2) {\n";
        learn_file << "                double update = 0.0003 * phi[i] * phi[j];\n";
        learn_file << "                W[i][j] = W[i][j] * 0.998 + update;\n";
        learn_file << "                W[j][i] = W[i][j];\n";
        learn_file << "            }\n";
        learn_file << "        }\n";
        learn_file << "    }\n";
        learn_file << "};\n";
        learn_file.close();
        std::cout << "✅ Generated optimized learning module" << std::endl;
    }
}

void EvolutionModule::analyzeCodeEfficiency() {
    try {
        size_t original_size = 0;
        size_t optimized_size = 0;
        
        for (const auto& entry : std::filesystem::directory_iterator("modules")) {
            if (entry.path().extension() == ".cpp" || entry.path().extension() == ".hpp") {
                original_size += entry.file_size();
            }
        }
        
        if (std::filesystem::exists("modules/DynamicsOptimized.hpp")) {
            optimized_size += std::filesystem::file_size("modules/DynamicsOptimized.hpp");
        }
        if (std::filesystem::exists("modules/LearningOptimized.hpp")) {
            optimized_size += std::filesystem::file_size("modules/LearningOptimized.hpp");
        }
        
        if (optimized_size > 0 && optimized_size < original_size * 0.8) {
            std::cout << "📊 Code efficiency improved: " 
                      << (original_size - optimized_size) << " bytes saved" << std::endl;
        }
        
    } catch (...) {
        std::cout << "⚠️ Code analysis failed" << std::endl;
    }
}

void EvolutionModule::optimizeSystemParameters() {
    if (current_metrics.performance_score < 0.8) {
        std::cout << "⚡ Performance low, optimizing parameters..." << std::endl;
    }
    
    if (current_metrics.energy_score < 0.8) {
        std::cout << "🔋 Energy efficiency low, optimizing..." << std::endl;
    }
}

void EvolutionModule::applyMinimalMutation(NeuralFieldSystem& system) {
    system.applyTargetedMutation(0.001, 2);
    
    if (total_steps % 2000 == 0) {
        std::cout << "💤 Stasis mode: minimal code optimization..." << std::endl;
        createOptimalConfig();
    }
}

// ========== МЕТОДЫ ЗАЩИТЫ И БЭКАПОВ ==========

bool EvolutionModule::createBackup() {
    try {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        
        std::string backup_name = backup_dir + "/backup_" + std::to_string(timestamp);
        std::filesystem::create_directories(backup_name);
        
        // Копируем исходные файлы
        for (const auto& entry : std::filesystem::recursive_directory_iterator(".")) {
            try {
                if (entry.is_regular_file() && 
                   (entry.path().extension() == ".cpp" || 
                    entry.path().extension() == ".hpp" ||
                    entry.path().extension() == ".json")) {
                    std::string entry_str = entry.path().string();
                    // Пропускаем саму директорию бэкапов
                    if (entry_str.find(backup_dir) == std::string::npos) {
                        // Сохраняем структуру директорий
                        std::filesystem::path relative_path = std::filesystem::relative(entry.path(), ".");
                        std::filesystem::path target_path = backup_name / relative_path;
                        
                        // Создаем целевую директорию если нужно
                        if (target_path.has_parent_path()) {
                            std::filesystem::create_directories(target_path.parent_path());
                        }
                        
                        std::filesystem::copy(entry.path(), 
                                            target_path,
                                            std::filesystem::copy_options::overwrite_existing);
                    }
                }
            } catch (const std::exception& e) {
                std::cout << "⚠️ Failed to backup: " << entry.path() << " - " << e.what() << std::endl;
            }
        }
        
        // Сохраняем метаданные бэкапа
        std::ofstream meta(backup_name + "/backup_meta.txt");
        if (meta.is_open()) {
            meta << "Backup created: " << timestamp << "\n";
            meta << "Fitness: " << current_metrics.overall_fitness << "\n";
            meta << "Steps: " << total_steps << "\n";
            meta << "Code Hash: " << getCurrentCodeHash() << "\n";
            meta.close();
        }
        
        std::cout << "💾 Backup created: " << backup_name << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Backup creation failed: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cout << "❌ Backup creation failed with unknown error" << std::endl;
        return false;
    }
}

bool EvolutionModule::restoreFromBackup() {
    try {
        // Находим самый свежий бэкап
        std::string latest_backup;
        std::filesystem::file_time_type latest_time = std::filesystem::file_time_type::min();
        
        for (const auto& entry : std::filesystem::directory_iterator(backup_dir)) {
            if (entry.is_directory() && 
                entry.path().string().find("backup_") != std::string::npos) {
                auto write_time = std::filesystem::last_write_time(entry);
                // Сравниваем file_time_type с file_time_type - это корректно
                if (write_time > latest_time) {
                    latest_time = write_time;
                    latest_backup = entry.path().string();
                }
            }
        }
        
        if (latest_backup.empty()) {
            std::cout << "❌ No backups found to restore" << std::endl;
            return false;
        }
        
        // Восстанавливаем файлы из бэкапа
        for (const auto& entry : std::filesystem::recursive_directory_iterator(latest_backup)) {
            if (entry.is_regular_file()) {
                // Получаем имя файла без пути к бэкапу
                std::filesystem::path relative_path = std::filesystem::relative(entry.path(), latest_backup);
                std::filesystem::path target_path = relative_path;
                
                // Создаем директорию назначения если нужно
                if (target_path.has_parent_path()) {
                    std::filesystem::create_directories(target_path.parent_path());
                }
                
                std::filesystem::copy(entry.path(), 
                                    target_path,
                                    std::filesystem::copy_options::overwrite_existing);
            }
        }
        
        std::cout << "🔄 System restored from backup: " << latest_backup << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Backup restoration failed: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cout << "❌ Backup restoration failed with unknown error" << std::endl;
        return false;
    }
}

bool EvolutionModule::checkForDegradation() {
    // Проверяем деградацию по нескольким критериям
    if (history.size() < 10) return false;
    
    double recent_avg = 0.0;
    int count = 0;
    
    // Считаем среднюю приспособленность за последние 10 записей
    for (int i = std::max(0, (int)history.size() - 10); i < history.size(); i++) {
        recent_avg += history[i].overall_fitness;
        count++;
    }
    recent_avg /= count;
    
    // Если текущая приспособленность значительно хуже лучшей
    if (current_metrics.overall_fitness < best_fitness * 0.7) {
        return true;
    }
    
    // Если наблюдается постоянное ухудшение
    if (recent_avg < best_fitness * 0.8) {
        return true;
    }
    
    return false;
}

void EvolutionModule::rollbackToBestVersion() {
    std::cout << "🔄 Attempting to rollback to best version..." << std::endl;
    
    if (restoreFromBackup()) {
        // Сбрасываем метрики после отката
        best_fitness = 0.0;
        current_metrics = EvolutionMetrics();
        std::cout << "✅ Rollback completed successfully" << std::endl;
    } else {
        std::cout << "❌ Rollback failed" << std::endl;
    }
}

double EvolutionModule::getCurrentCodeHash() const {
    // Простой хэш для отслеживания изменений кода
    size_t hash = 0;
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(".")) {
            if (entry.is_regular_file() && 
               (entry.path().extension() == ".cpp" || entry.path().extension() == ".hpp")) {
                std::string entry_str = entry.path().string();
                if (entry_str.find(backup_dir) == std::string::npos) {
                    hash += std::filesystem::file_size(entry.path());
                    hash += entry.path().string().length();
                }
            }
        }
    } catch (...) {
        // Игнорируем ошибки доступа к файлам
    }
    return static_cast<double>(hash);
}

bool EvolutionModule::validateImprovement() const {
    // Проверяем, привела ли эволюция к улучшениям
    if (history.size() < 2) return true;
    
    const auto& previous = history[history.size() - 2];
    const auto& current = history[history.size() - 1];
    
    // Считаем улучшение по всем метрикам
    double improvement = (current.overall_fitness - previous.overall_fitness) / previous.overall_fitness;
    
    return improvement > -0.1; // Допускаем небольшое ухудшение (10%)
}

void EvolutionModule::saveEvolutionState() {
    try {
        std::ofstream state_file("evolution_state.txt");
        if (state_file.is_open()) {
            state_file << "Evolution State - Generation: " << total_steps / 1000 << "\n";
            state_file << "Current Fitness: " << current_metrics.overall_fitness << "\n";
            state_file << "Best Fitness: " << best_fitness << "\n";
            state_file << "Code Size Score: " << current_metrics.code_size_score << "\n";
            state_file << "Performance Score: " << current_metrics.performance_score << "\n";
            state_file << "Energy Score: " << current_metrics.energy_score << "\n";
            state_file << "Total Steps: " << total_steps << "\n";
            state_file << "Backups available in: " << backup_dir << "\n";
            state_file.close();
        }
    } catch (...) {
        std::cout << "⚠️ Failed to save evolution state" << std::endl;
    }
}

void EvolutionModule::enterStasis(NeuralFieldSystem& system) {
    in_stasis = true;
    system.enterLowPowerMode();
    std::cout << "💤 SYSTEM ENTERED STASIS MODE" << std::endl;
    saveEvolutionState();
}

void EvolutionModule::exitStasis() {
    in_stasis = false;
    std::cout << "⚡ SYSTEM EXITED STASIS MODE" << std::endl;
}

bool EvolutionModule::isInStasis() const {
    return in_stasis;
}