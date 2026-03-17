// modules/StatsDumper.hpp
#pragma once
#include "UnifiedStatsCollector.hpp"
#include <fstream>
#include <filesystem>

class StatsDumper {
private:
    std::string dump_dir = "dump/stats/";
    
public:
    StatsDumper() {
        std::filesystem::create_directories(dump_dir);
    }
    
    void dumpToJSON(const UnifiedStatsCollector& collector, int step) {
        std::ofstream file(dump_dir + "stats_" + std::to_string(step) + ".json");
        if (!file.is_open()) return;
        
        file << "{\n";
        file << "  \"step\": " << step << ",\n";
        file << "  \"modules\": {\n";
        
        const auto& all_stats = collector.getAllStats();
        bool first_module = true;
        
        for (const auto& [key, stats] : all_stats) {
            if (!first_module) file << ",\n";
            first_module = false;
            
            file << "    \"" << key << "\": {\n";
            file << "      \"name\": \"" << stats.name << "\",\n";
            
            // Числовые статистики
            file << "      \"numeric\": {\n";
            bool first_num = true;
            for (const auto& [name, val] : stats.numeric_stats) {
                if (!first_num) file << ",\n";
                first_num = false;
                file << "        \"" << name << "\": " << val;
            }
            file << "\n      }\n";
            file << "    }";
        }
        
        file << "\n  }\n";
        file << "}\n";
    }
    
    void dumpPeriodic(const UnifiedStatsCollector& collector, int step) {
        if (step % 1000 == 0) {
            dumpToJSON(collector, step);
        }
    }
};