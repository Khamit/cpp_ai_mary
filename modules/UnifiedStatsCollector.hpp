// modules/UnifiedStatsCollector.hpp
#pragma once
#include "../core/NeuralFieldSystem.hpp"
#include "../core/EmergentCore.hpp"
#include "lang/LanguageModule.hpp"
#include "MetaCognitiveModule.hpp"
#include <map>
#include <string>
#include <sstream>
#include <iomanip>

struct ModuleStats {
    std::string name;
    std::map<std::string, double> numeric_stats;
    std::map<std::string, std::string> string_stats;
    double last_update = 0.0;
    std::map<uint32_t, float> concept_mastery;  // Добавить
};

class UnifiedStatsCollector {
private:
    NeuralFieldSystem* neural_system = nullptr;
    EmergentMemory* memory_manager = nullptr;
    LanguageModule* language = nullptr;
    MetaCognitiveModule* metacog = nullptr;
    LearningOrchestrator* learning = nullptr;  // ИЗМЕНЕНО
    
    std::map<std::string, ModuleStats> all_stats;
    int current_step = 0;
    
public:
    void setNeuralSystem(NeuralFieldSystem* ns) { neural_system = ns; }
    void setMemoryManager(EmergentMemory* mem) { memory_manager = mem; }
    void setLanguage(LanguageModule* lang) { language = lang; }
    void setMetaCognitive(MetaCognitiveModule* meta) { metacog = meta; }
    void setLearning(LearningOrchestrator* learn) { learning = learn; }  // ИЗМЕНЕНО
    LearningOrchestrator* getLearning() const { return learning; }  // ИЗМЕНЕНО
    
void update(int step) {
    current_step = step;
    all_stats.clear();
    
    // 1. NEURAL SYSTEM STATS
    if (neural_system) {
        ModuleStats neural;
        neural.name = "NEURAL CORE";
        
        neural.numeric_stats["total_neurons"] = NeuralFieldSystem::TOTAL_NEURONS;
        neural.numeric_stats["groups"] = NeuralFieldSystem::NUM_GROUPS;
        neural.numeric_stats["energy"] = neural_system->computeTotalEnergy();
        neural.numeric_stats["entropy"] = neural_system->computeSystemEntropy();
        
        auto avgs = neural_system->getGroupAverages();
        double sum = 0.0;
        for (double v : avgs) sum += v;
        neural.numeric_stats["avg_activity"] = sum / avgs.size();
        
        // ===== ДОБАВЛЯЕМ ЭНТРОПИЙНЫЕ ЦЕЛИ ГРУПП =====
        const auto& groups = neural_system->getGroups();
        double total_entropy_error = 0.0;
        int specialized_count = 0;
        
        for (int g = 0; g < groups.size(); g++) {
            double target = groups[g].computeEntropyTarget();
            double current = groups[g].computeEntropy();
            std::string spec = groups[g].specialization;
            
            // Добавляем в числовую статистику
            neural.numeric_stats["group" + std::to_string(g) + "_target"] = target;
            neural.numeric_stats["group" + std::to_string(g) + "_current"] = current;
            
            // Добавляем в строковую статистику для специализации
            if (spec.empty() || spec == "unspecialized") {
                neural.string_stats["group" + std::to_string(g) + "_spec"] = "group" + std::to_string(g);
            } else {
                neural.string_stats["group" + std::to_string(g) + "_spec"] = spec;
            }
            
            // Считаем ошибку только для специализированных групп
            if (spec != "unspecialized" && !spec.empty()) {
                total_entropy_error += std::abs(target - current);
                specialized_count++;
            }
        }
        
        if (specialized_count > 0) {
            neural.numeric_stats["avg_entropy_error"] = total_entropy_error / specialized_count;
        }
        // ===== КОНЕЦ ДОБАВЛЕНИЯ =====
        
        all_stats["neural"] = neural;
    }
    
    // 2. MEMORY STATS
    if (memory_manager) {
        ModuleStats memory;
        memory.name = "MEMORY";
        
        // memory.numeric_stats["short_term"] = memory_manager->getShortTermSize();
        // memory.numeric_stats["long_term"] = memory_manager->getLongTermMemory().size();
        // memory.numeric_stats["dump_size"] = memory_manager->getDumpSize();
        
        all_stats["memory"] = memory;
    }
    
    // 4. LANGUAGE STATS
    if (language) {
        ModuleStats lang;
        lang.name = "LANGUAGE";
        
        lang.numeric_stats["fitness"] = language->getLanguageFitness();
        lang.numeric_stats["external_feedback"] = language->getExternalFeedbackAvg();
        lang.numeric_stats["recent_meanings"] = language->getRecentMeanings().size();
        lang.numeric_stats["auto_learning"] = language->isAutoLearningActive() ? 1.0 : 0.0;
        
        all_stats["language"] = lang;
    }
    
    // 5. META-COGNITIVE STATS
    if (metacog) {
        ModuleStats meta;
        meta.name = "META-COGNITION";
        
        auto state = neural_system ? neural_system->getReflectionState() : 
                   NeuralFieldSystem::ReflectionState{};
        
        meta.numeric_stats["confidence"] = state.confidence;
        meta.numeric_stats["curiosity"] = state.curiosity;
        meta.numeric_stats["satisfaction"] = state.satisfaction;
        meta.numeric_stats["confusion"] = state.confusion;
        
        if (!state.attention_map.empty()) {
            meta.numeric_stats["attention_0"] = state.attention_map[0];
            meta.numeric_stats["attention_1"] = state.attention_map[1];
            meta.numeric_stats["attention_2"] = state.attention_map[2];
            meta.numeric_stats["attention_3"] = state.attention_map[3];
        }
        
        all_stats["metacog"] = meta;
    }
    
        if (learning) {
            ModuleStats learn;
            learn.name = "LEARNING";
            
            all_stats["learning"] = learn;
        }
        
        // 7. SYSTEM STATS
        ModuleStats system;
        system.name = "SYSTEM";
        system.numeric_stats["step"] = step;
        all_stats["system"] = system;
    }
    
    const std::map<std::string, ModuleStats>& getAllStats() const { return all_stats; }
    
    std::string getFormattedStats(const std::string& module = "") const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2);
        
        if (module.empty()) {
            // Все модули
            for (const auto& [key, stats] : all_stats) {
                ss << "\n=== " << stats.name << " ===\n";
                for (const auto& [name, val] : stats.numeric_stats) {
                    ss << "  " << name << ": " << val << "\n";
                }
            }
        } else {
            // Конкретный модуль
            auto it = all_stats.find(module);
            if (it != all_stats.end()) {
                ss << "=== " << it->second.name << " ===\n";
                for (const auto& [name, val] : it->second.numeric_stats) {
                    ss << "  " << name << ": " << val << "\n";
                }
            }
        }
        
        return ss.str();
    }
    
    // Получить данные об энтропийных целях для конкретной группы
    std::tuple<double, double, std::string> getGroupEntropyData(int group_index) const {
        auto it = all_stats.find("neural");
        if (it != all_stats.end()) {
            const auto& neural = it->second;
            
            std::string target_key = "group" + std::to_string(group_index) + "_target";
            std::string current_key = "group" + std::to_string(group_index) + "_current";
            std::string spec_key = "group" + std::to_string(group_index) + "_spec";
            
            double target = neural.numeric_stats.count(target_key) ? 
                        neural.numeric_stats.at(target_key) : 0.0;
            double current = neural.numeric_stats.count(current_key) ? 
                            neural.numeric_stats.at(current_key) : 0.0;
            std::string spec = neural.string_stats.count(spec_key) ? 
                            neural.string_stats.at(spec_key) : "unknown";
            
            return std::make_tuple(target, current, spec);
        }
        return std::make_tuple(0.0, 0.0, "unknown");
    }

    // Получить среднюю ошибку энтропии
    double getAvgEntropyError() const {
        auto it = all_stats.find("neural");
        if (it != all_stats.end()) {
            const auto& neural = it->second;
            auto err_it = neural.numeric_stats.find("avg_entropy_error");
            if (err_it != neural.numeric_stats.end()) {
                return err_it->second;
            }
        }
        return 0.0;
    }
        
 std::string getCompactStats() const {
        std::stringstream ss;
        
        // Безопасное получение значения с дефолтом
        auto getValue = [&](const std::string& category, const std::string& key, double defaultVal = 0.0) -> double {
            auto catIt = all_stats.find(category);
            if (catIt == all_stats.end()) return defaultVal;
            auto valIt = catIt->second.numeric_stats.find(key);
            if (valIt == catIt->second.numeric_stats.end()) return defaultVal;
            return valIt->second;
        };
        
        double energy = getValue("neural", "total_energy");
        double entropy = getValue("neural", "system_entropy");
        double fitness = getValue("evolution", "overall_fitness");
        double bestFitness = getValue("evolution", "best_fitness");
        double words = getValue("language", "words_learned");
        double feedback = getValue("language", "external_feedback_avg");
        double stm = getValue("memory", "stm_size");
        double ltm = getValue("memory", "ltm_size");
        
        ss << "Energy: " << std::fixed << std::setprecision(2) << energy << "\n";
        ss << "Entropy: " << std::setprecision(2) << entropy << "\n";
        ss << "Fitness: " << std::setprecision(2) << fitness << "\n";
        ss << "Best: " << bestFitness << "\n";
        ss << "Words: " << words << "\n";
        ss << "Feedback: " << std::setprecision(2) << feedback << "\n";
        ss << "STM: " << stm << " LTM: " << ltm;
        
        return ss.str();
    }
};