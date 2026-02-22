// RuleEvolution.hpp - как правила могут "расти" в коде
#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <map>
#include "MemoryModule.hpp"

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

    static void analyzeMemory(const MemoryController& memory) {
        std::map<std::string, int> patternFrequency;

        for (const auto& record : memory.getRecords()) {
            std::string pattern = decodeFeatures(record.features);
            patternFrequency[pattern]++;
        }

        for (const auto& [pattern, freq] : patternFrequency) {
            if (freq > 10) {
                std::string rule = generateRule(pattern, "common response");
                saveRule(rule);
            }
        }
    }

private:
    static std::string decodeFeatures(const std::vector<float>& features) {
        std::string result;
        for (auto f : features) {
            char c = static_cast<char>(f * 255);
            if (std::isprint(static_cast<unsigned char>(c)))
                result += c;
        }
        return result;
    }
};