#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <fstream>
#include <iostream>
#include <chrono>
#include <filesystem>
#include <numeric>
#include <cmath>
#include <algorithm>
#include "Component.hpp"
#include "../modules/EvolutionMetrics.hpp"  // исправленный путь
#include <ctime>  // для std::time

// Сжатие данных Квантование (float → int8)
/*
float = 4 байта
int8 = 1 байт
экономия ×4
*/
struct CompressedData {
    float minVal;
    float scale;
    std::vector<int8_t> bytes;
};

// Единая структура для записи в память
struct MemoryRecord {
    std::time_t timestamp;  // вместо chrono::time_point
    std::string component;
    std::string type;
    std::vector<float> data;
    std::map<std::string, std::string> metadata;
    float importance;
    
    MemoryRecord() : timestamp(std::time(nullptr)), importance(1.0f) {}
};

// Структура для эволюционных данных (совместимость с MinimalDump)
struct EvolutionDumpData {
    size_t generation;
    EvolutionMetrics metrics;
    std::vector<double> best_weights; // Только существенные веса
    double energy_state;
    uint64_t code_hash;
    
    EvolutionDumpData() : generation(0), energy_state(0.0), code_hash(0) {}
};

class MemoryManager {
private:
    std::string dumpPath = "dump/";
    std::vector<MemoryRecord> shortTermMemory;  // кратковременная память
    std::map<std::string, std::vector<MemoryRecord>> longTermMemory; // по компонентам

    // Восстановить приближённое состояние по латентному коду
    std::vector<float> reconstructFromLatent(const std::string& component,
                                         const std::vector<double>& latent_code);
    
    // Новые поля для контроля памяти
    size_t maxShortTermMemory = 1000;        // максимум записей в кратковременной
    size_t maxLongTermPerComponent = 500;    // максимум на компонент
    float cleanupThreshold = 0.9f;            // порог автоочистки
    // Ленивое сохранение
    int writeCooldown = 0;
    const int WRITE_INTERVAL = 100;
    
    // Вспомогательные методы
    void saveToFile(const std::string& filename, const std::vector<MemoryRecord>& data);
    void loadFromFile(const std::string& filename, std::vector<MemoryRecord>& data);
    void cleanupOldRecords();  // новый метод

    // Функция квантования
    CompressedData quantize(const std::vector<float>& data) {
    CompressedData cd;
    if (data.empty()) return cd;

    auto [minIt, maxIt] = std::minmax_element(data.begin(), data.end());
    cd.minVal = *minIt;
    float maxVal = *maxIt;

    float range = maxVal - cd.minVal;
    cd.scale = (range < 1e-8f) ? 1.0f : range / 255.0f;

    cd.bytes.resize(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        float norm = (data[i] - cd.minVal) / cd.scale;
        cd.bytes[i] = static_cast<int8_t>(std::clamp(norm, 0.0f, 255.0f));
    }
    return cd;
    }
    // Обратное восстановление
    std::vector<float> dequantize(const CompressedData& cd) {
        std::vector<float> data(cd.bytes.size());
        for (size_t i = 0; i < cd.bytes.size(); ++i)
            data[i] = cd.minVal + cd.bytes[i] * cd.scale;
        return data;
    }

    // Большинство весов и признаков близки к нулю → бесполезны.
    float pruningThreshold = 0.01f;

    void pruneVector(std::vector<float>& v) {
        for (auto& x : v)
            if (std::abs(x) < pruningThreshold)
                x = 0.0f;
    }
    
    // Сжатие весов (из MinimalDump)
    std::vector<double> compressWeights(const std::vector<std::vector<double>>& weights) {
        std::vector<double> compressed;
        if (weights.empty()) return compressed;
        
        int N = weights.size();
        for (int i = 0; i < N; i++) {
            for (int j = i; j < N; j++) {
                compressed.push_back(weights[i][j]);
            }
        }
        return compressed;
    }
    
    // Извлечение весов (из MinimalDump)
    std::vector<std::vector<double>> extractWeights(const std::vector<double>& compressed, int Nside) {
        int N = Nside * Nside;
        std::vector<std::vector<double>> weights(N, std::vector<double>(N, 0.0));
        
        if (compressed.empty()) return weights;
        
        int idx = 0;
        for (int i = 0; i < N; i++) {
            for (int j = i; j < N; j++) {
                if (idx < compressed.size()) {
                    weights[i][j] = compressed[idx];
                    weights[j][i] = compressed[idx];
                    idx++;
                }
            }
        }
        return weights;
    }
    
public:
    MemoryManager();

    // Новые методы для работы с энтропийными данными
    void storeWithEntropy(const std::string& component, 
                        const std::vector<float>& data,
                        double entropy,
                        float importance = 1.0f);

    std::vector<MemoryRecord> findHighEntropyRecords(const std::string& component, 
                                                    double minEntropy) const;

    std::vector<MemoryRecord> getRecordsByIndices(
    const std::string& component,
    const std::vector<size_t>& indices) const;

    // Добавьте правильный метод getRecords (которого не хватает!)
    std::vector<MemoryRecord> getRecords(const std::string& component) const {
        auto it = longTermMemory.find(component);
        if (it != longTermMemory.end()) {
            return it->second;
        }
        return {};
    }

    // Сохранить сжатое состояние (только латентный код + ошибка)
    void storeCompressed(const std::string& component,
                        const std::vector<double>& latent_code,
                        double prediction_error,
                        float importance = 0.7f);
    
    // Получить размер кратковременной памяти
    size_t getShortTermSize() const { return shortTermMemory.size(); }

    // Геттеры
    const std::map<std::string, std::vector<MemoryRecord>>& getLongTermMemory() const;
    size_t getDumpSize() const;
    
    // Сохранение данных
    void store(const std::string& component, const std::string& type, 
               const std::vector<float>& data, 
               float importance = 1.0f,
               const std::map<std::string, std::string>& metadata = {});
    
    // Специализированное сохранение для эволюционных данных
    void saveEvolutionState(const EvolutionDumpData& data, const std::string& filename = "evolution_dump.bin");
    EvolutionDumpData loadEvolutionState(const std::string& filename = "evolution_dump.bin");
    
    // Поиск похожих записей
    std::vector<size_t> findSimilar(const std::string& component, 
                                    const std::vector<float>& query, 
                                    size_t k = 5) const;
    
    // Сохранение всего в файлы
    void saveAll();
    
    // Загрузка из файлов
    void loadAll();
    
    // Консолидация кратковременной памяти в долговременную
    void consolidate(float threshold = 0.8f);
    
    // Сохранить веса нейросети
    void saveWeights(const std::string& component, 
                    const std::vector<std::vector<double>>& weights,
                    int generation);
    
    // Загрузить веса
    std::vector<std::vector<double>> loadWeights(const std::string& component, int matrixSize);
};