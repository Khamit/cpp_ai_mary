#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include "EvolutionMetrics.hpp"

class MinimalDump {
public:
    struct DumpData {
        size_t generation;
        EvolutionMetrics metrics;
        std::vector<double> best_weights; // Только существенные веса
        double energy_state;
        uint64_t code_hash;
        
        DumpData();
    };
    
    void saveEvolutionState(const DumpData& data, const std::string& filename = "evolution_dump.bin");
    DumpData loadEvolutionState(const std::string& filename = "evolution_dump.bin");
    size_t getDumpSize() const;
    
private:
    std::vector<double> compressWeights(const std::vector<std::vector<double>>& weights);
    std::vector<std::vector<double>> extractWeights(const std::vector<double>& compressed, int Nside);
};