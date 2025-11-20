#include "MinimalDump.hpp"
#include <fstream>
#include <iostream>
#include <cmath>

MinimalDump::DumpData::DumpData() 
    : generation(0), energy_state(0.0), code_hash(0) {}

void MinimalDump::saveEvolutionState(const DumpData& data, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to save evolution state to " << filename << std::endl;
        return;
    }
    
    // Сохраняем только минимальные данные
    file.write(reinterpret_cast<const char*>(&data.generation), sizeof(data.generation));
    
    // Сохраняем метрики по отдельности
    file.write(reinterpret_cast<const char*>(&data.metrics.code_size_score), sizeof(data.metrics.code_size_score));
    file.write(reinterpret_cast<const char*>(&data.metrics.performance_score), sizeof(data.metrics.performance_score));
    file.write(reinterpret_cast<const char*>(&data.metrics.energy_score), sizeof(data.metrics.energy_score));
    file.write(reinterpret_cast<const char*>(&data.metrics.overall_fitness), sizeof(data.metrics.overall_fitness));
    
    file.write(reinterpret_cast<const char*>(&data.energy_state), sizeof(data.energy_state));
    file.write(reinterpret_cast<const char*>(&data.code_hash), sizeof(data.code_hash));
    
    // Сохраняем сжатые веса
    size_t weights_size = data.best_weights.size();
    file.write(reinterpret_cast<const char*>(&weights_size), sizeof(weights_size));
    if (weights_size > 0) {
        file.write(reinterpret_cast<const char*>(data.best_weights.data()), 
                   weights_size * sizeof(double));
    }
    
    std::cout << "Evolution state saved to " << filename << " (" << getDumpSize() << " bytes)" << std::endl;
}

MinimalDump::DumpData MinimalDump::loadEvolutionState(const std::string& filename) {
    DumpData data;
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to load evolution state from " << filename << std::endl;
        return data;
    }
    
    file.read(reinterpret_cast<char*>(&data.generation), sizeof(data.generation));
    
    // Загружаем метрики по отдельности
    file.read(reinterpret_cast<char*>(&data.metrics.code_size_score), sizeof(data.metrics.code_size_score));
    file.read(reinterpret_cast<char*>(&data.metrics.performance_score), sizeof(data.metrics.performance_score));
    file.read(reinterpret_cast<char*>(&data.metrics.energy_score), sizeof(data.metrics.energy_score));
    file.read(reinterpret_cast<char*>(&data.metrics.overall_fitness), sizeof(data.metrics.overall_fitness));
    
    file.read(reinterpret_cast<char*>(&data.energy_state), sizeof(data.energy_state));
    file.read(reinterpret_cast<char*>(&data.code_hash), sizeof(data.code_hash));
    
    size_t weights_size;
    file.read(reinterpret_cast<char*>(&weights_size), sizeof(weights_size));
    data.best_weights.resize(weights_size);
    if (weights_size > 0) {
        file.read(reinterpret_cast<char*>(data.best_weights.data()), 
                  weights_size * sizeof(double));
    }
    
    std::cout << "Evolution state loaded from " << filename << std::endl;
    return data;
}

size_t MinimalDump::getDumpSize() const {
    // Расчет размера: generation + 4 метрики + energy_state + code_hash
    return sizeof(size_t) + sizeof(double) * 5 + sizeof(uint64_t);
}

std::vector<double> MinimalDump::compressWeights(const std::vector<std::vector<double>>& weights) {
    std::vector<double> compressed;
    int N = weights.size();
    
    if (N == 0) return compressed;
    
    // Сохраняем только верхний треугольник матрицы (симметричная матрица)
    for (int i = 0; i < N; i++) {
        for (int j = i; j < N; j++) {
            compressed.push_back(weights[i][j]);
        }
    }
    
    return compressed;
}

std::vector<std::vector<double>> MinimalDump::extractWeights(const std::vector<double>& compressed, int Nside) {
    int N = Nside * Nside;
    std::vector<std::vector<double>> weights(N, std::vector<double>(N, 0.0));
    
    if (compressed.empty()) return weights;
    
    int idx = 0;
    for (int i = 0; i < N; i++) {
        for (int j = i; j < N; j++) {
            if (idx < compressed.size()) {
                weights[i][j] = compressed[idx];
                weights[j][i] = compressed[idx]; // Симметрия
                idx++;
            }
        }
    }
    
    return weights;
}