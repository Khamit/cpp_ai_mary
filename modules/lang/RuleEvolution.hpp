// RuleEvolution.hpp
#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <map>
#include "core/MemoryManager.hpp"

class RuleEvolution {
public:
    static std::string generateRule(const std::string& pattern,
                                    const std::string& response) {
        return pattern + " -> " + response;
    }

    static void saveRule(const std::string& rule) {
        std::ofstream file("generated_rules.txt", std::ios::app);
        file << rule << "\n";
    }

    static void analyzeMemory(const MemoryManager& memory) {
        std::map<std::string, int> patternFrequency;
        
        // Используем новый метод getRecords()
        auto records = memory.getRecords("LanguageModule");
        
        for (const auto& record : records) {
            if (record.type == "word") {
                auto it = record.metadata.find("word");
                if (it != record.metadata.end()) {
                    patternFrequency[it->second]++;
                }
            }
        }

        for (const auto& [pattern, freq] : patternFrequency) {
            if (freq > 10) {
                std::string rule = generateRule(pattern, "common response");
                saveRule(rule);
            }
        }
    }
};