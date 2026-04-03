#include "core/MemoryManager.hpp"
#include <iostream>
#include <filesystem>
#include <ctime>
#include <atomic>
#include <chrono>
#include <thread>

// Конструктор
MemoryManager::MemoryManager() {
    std::filesystem::create_directories(dumpPath);
    
    // Устанавливаем лимиты памяти (10% от разумного)
    maxShortTermMemory = 1000;        // было 10000, теперь 1000 (10%)
    maxLongTermPerComponent = 500;    // максимум записей на компонент
    cleanupThreshold = 0.9f;          // порог для автоочистки (90% от лимита)
    
    std::cout << "MemoryManager: лимит кратковременной памяти: " 
              << maxShortTermMemory << " записей" << std::endl;
}

// Геттеры
const std::map<std::string, std::vector<MemoryRecord>>& MemoryManager::getLongTermMemory() const {
    return longTermMemory;
}

std::atomic<bool> MemoryManager::consolidating_{false};
std::atomic<int> MemoryManager::store_depth_{0};

size_t MemoryManager::getDumpSize() const {
    return sizeof(size_t) + sizeof(double) * 5 + sizeof(uint64_t);
}

// Очистка старых записей
void MemoryManager::cleanupOldRecords() {
    auto now = std::time(nullptr);
    const int MAX_AGE_DAYS = 7;  // хранить не больше недели
    const int MAX_AGE_SECONDS = MAX_AGE_DAYS * 24 * 60 * 60;
    
    // Очищаем долговременную память
    for (auto& [comp, records] : longTermMemory) {
        // Удаляем старые записи
        records.erase(
            std::remove_if(records.begin(), records.end(),
                [now, MAX_AGE_SECONDS](const MemoryRecord& r) {
                    return (now - r.timestamp) > MAX_AGE_SECONDS;
                }),
            records.end()
        );
        
        // Если всё еще превышает лимит, удаляем самые старые
        if (records.size() > maxLongTermPerComponent) {
            records.erase(records.begin(), 
                         records.begin() + (records.size() - maxLongTermPerComponent));
            // Освобождаем память
            std::vector<MemoryRecord>(records).swap(records);
        }
    }
}

// Сохранение данных
void MemoryManager::store(const std::string& component, const std::string& type, 
                         const std::vector<float>& data, 
                         float importance,
                         const std::map<std::string, std::string>& metadata) {
    
    // Защита от рекурсии
    store_depth_++;
    if (store_depth_ > 5) {
        std::cerr << "Слишком глубокая рекурсия в store()" << std::endl;
        store_depth_--;
        return;
    }
    
    // Проверяем размер данных (не больше 1KB на запись)
    if (data.size() > 256) {  // 256 floats = 1KB
        std::cerr << "Запись слишком большая: " << data.size() << " floats" << std::endl;
        store_depth_--;
        return;
    }

    // Проверка на валидность данных
    if (data.empty()) {
        std::cerr << "Попытка сохранить пустые данные" << std::endl;
        store_depth_--;
        return;
    }

    // Проверка на NaN/Inf
    for (size_t i = 0; i < std::min(data.size(), size_t(10)); i++) {
        if (!std::isfinite(data[i])) {
            std::cerr << "Невалидные данные в store()" << std::endl;
            store_depth_--;
            return;
        }
    }

    MemoryRecord record;
    record.timestamp = std::time(nullptr);
    record.component = component;
    record.type = type;
    record.data = data;
    record.metadata = metadata;
    record.importance = importance;
    
    // ИСПРАВЛЕНИЕ: обрезаем копию, а не оригинал
    pruneVector(record.data);

    // Добавляем в кратковременную память
    shortTermMemory.push_back(record);
    
    // Если важность высокая, сразу в долговременную
    if (importance > 0.8f) {
        longTermMemory[component].push_back(record);
        
        // Проверяем лимиты долговременной памяти
        if (longTermMemory[component].size() > maxLongTermPerComponent) {
            longTermMemory[component].erase(
                longTermMemory[component].begin(),
                longTermMemory[component].begin() + 10
            );
        }
    }
    
    // Автоматическая очистка при достижении порога - НО БЕЗ РЕКУРСИИ!
    if (shortTermMemory.size() > maxShortTermMemory * cleanupThreshold && 
        !consolidating_ && store_depth_ == 1) {
        consolidate(0.5f);
    }
    
    // Ограничиваем размер кратковременной памяти
    if (shortTermMemory.size() > maxShortTermMemory) {
        size_t removeCount = shortTermMemory.size() - maxShortTermMemory;
        shortTermMemory.erase(shortTermMemory.begin(), 
                             shortTermMemory.begin() + removeCount);
        
        // Дефрагментируем память
        if (shortTermMemory.capacity() > maxShortTermMemory * 2) {
            std::vector<MemoryRecord>(shortTermMemory).swap(shortTermMemory);
        }
    }
    
    store_depth_--;
}

// Специализированное сохранение для эволюционных данных
void MemoryManager::saveEvolutionState(const EvolutionDumpData& data, const std::string& filename) {
    std::ofstream file(dumpPath + filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to save evolution state to " << filename << std::endl;
        return;
    }
    
    file.write(reinterpret_cast<const char*>(&data.generation), sizeof(data.generation));
    file.write(reinterpret_cast<const char*>(&data.metrics.code_size_score), sizeof(data.metrics.code_size_score));
    file.write(reinterpret_cast<const char*>(&data.metrics.performance_score), sizeof(data.metrics.performance_score));
    file.write(reinterpret_cast<const char*>(&data.metrics.energy_score), sizeof(data.metrics.energy_score));
    file.write(reinterpret_cast<const char*>(&data.metrics.overall_fitness), sizeof(data.metrics.overall_fitness));
    file.write(reinterpret_cast<const char*>(&data.energy_state), sizeof(data.energy_state));
    file.write(reinterpret_cast<const char*>(&data.code_hash), sizeof(data.code_hash));
    
    size_t weights_size = data.best_weights.size();
    file.write(reinterpret_cast<const char*>(&weights_size), sizeof(weights_size));
    if (weights_size > 0) {
        file.write(reinterpret_cast<const char*>(data.best_weights.data()), 
                  weights_size * sizeof(double));
    }
    
    std::cout << "Evolution state saved to " << dumpPath + filename << std::endl;
}

// Загрузка эволюционных данных
EvolutionDumpData MemoryManager::loadEvolutionState(const std::string& filename) {
    EvolutionDumpData data;
    std::ifstream file(dumpPath + filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to load evolution state from " << filename << std::endl;
        return data;
    }
    
    file.read(reinterpret_cast<char*>(&data.generation), sizeof(data.generation));
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
    
    std::cout << "Evolution state loaded from " << dumpPath + filename << std::endl;
    return data;
}

// Поиск похожих записей
std::vector<size_t> MemoryManager::findSimilar(const std::string& component, 
                                              const std::vector<float>& query, 
                                              size_t k) const {
    std::vector<size_t> result;
    auto it = longTermMemory.find(component);
    if (it == longTermMemory.end() || it->second.empty()) {
        return result;
    }
    
    const auto& records = it->second;
    std::vector<std::pair<size_t, float>> similarities;
    
    // Ограничиваем поиск первыми 100 записями для производительности
    size_t maxSearch = std::min(records.size(), size_t(100));
    
    for (size_t i = 0; i < maxSearch; ++i) {
        if (records[i].data.empty()) continue;
        if (records[i].data.size() != query.size()) continue;
        
        float dot = 0.0f, normA = 0.0f, normB = 0.0f;
        for (size_t j = 0; j < query.size(); ++j) {
            dot += query[j] * records[i].data[j];
            normA += query[j] * query[j];
            normB += records[i].data[j] * records[i].data[j];
        }
        
        if (normA > 0 && normB > 0) {
            float sim = dot / (std::sqrt(normA) * std::sqrt(normB));
            if (std::isfinite(sim)) {
                similarities.push_back({i, sim});
            }
        }
    }
    
    std::partial_sort(similarities.begin(), 
                     similarities.begin() + std::min(k, similarities.size()),
                     similarities.end(),
                     [](const auto& a, const auto& b) { return a.second > b.second; });
    
    for (size_t i = 0; i < std::min(k, similarities.size()); ++i) {
        result.push_back(similarities[i].first);
    }
    
    return result;
}

// Сохранение вектора MemoryRecord с правильной сериализацией
void MemoryManager::saveToFile(const std::string& filename, const std::vector<MemoryRecord>& data) {
    if (data.empty()) return;  // не сохраняем пустые файлы
    
    std::ofstream file(dumpPath + filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to save to " << filename << std::endl;
        return;
    }
    
    // Сохраняем размер вектора
    size_t size = data.size();
    file.write(reinterpret_cast<const char*>(&size), sizeof(size));
    
    // Сохраняем каждую запись
    for (const auto& record : data) {
        // Сохраняем timestamp
        file.write(reinterpret_cast<const char*>(&record.timestamp), sizeof(record.timestamp));
        
        // Сохраняем строки с длиной
        size_t comp_len = record.component.size();
        file.write(reinterpret_cast<const char*>(&comp_len), sizeof(comp_len));
        file.write(record.component.data(), comp_len);
        
        size_t type_len = record.type.size();
        file.write(reinterpret_cast<const char*>(&type_len), sizeof(type_len));
        file.write(record.type.data(), type_len);
        
        // Сохраняем вектор данных
        size_t data_size = record.data.size();
        file.write(reinterpret_cast<const char*>(&data_size), sizeof(data_size));
        if (data_size > 0) {
            file.write(reinterpret_cast<const char*>(record.data.data()), data_size * sizeof(float));
        }
        
        // Сохраняем importance
        file.write(reinterpret_cast<const char*>(&record.importance), sizeof(record.importance));
        
        // Сохраняем метаданные
        size_t meta_size = record.metadata.size();
        file.write(reinterpret_cast<const char*>(&meta_size), sizeof(meta_size));
        for (const auto& [key, value] : record.metadata) {
            size_t key_len = key.size();
            file.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
            file.write(key.data(), key_len);
            
            size_t value_len = value.size();
            file.write(reinterpret_cast<const char*>(&value_len), sizeof(value_len));
            file.write(value.data(), value_len);
        }
    }
}

// Загрузка вектора MemoryRecord с правильной десериализацией
void MemoryManager::loadFromFile(const std::string& filename, std::vector<MemoryRecord>& data) {
    std::ifstream file(dumpPath + filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to load from " << filename << std::endl;
        return;
    }
    
    data.clear();
    
    // Читаем размер вектора
    size_t size;
    file.read(reinterpret_cast<char*>(&size), sizeof(size));
    
    // Защита от слишком больших файлов
    if (size > 10000) {
        std::cerr << "Файл " << filename << " слишком большой: " << size << " записей" << std::endl;
        return;
    }
    
    data.reserve(size);
    
    // Читаем каждую запись
    for (size_t i = 0; i < size; i++) {
        MemoryRecord record;
        
        // Читаем timestamp
        file.read(reinterpret_cast<char*>(&record.timestamp), sizeof(record.timestamp));
        
        // Читаем component
        size_t comp_len;
        file.read(reinterpret_cast<char*>(&comp_len), sizeof(comp_len));
        if (comp_len > 256) {  // защита от поврежденных данных
            std::cerr << "Поврежденные данные в " << filename << std::endl;
            return;
        }
        record.component.resize(comp_len);
        file.read(&record.component[0], comp_len);
        
        // Читаем type
        size_t type_len;
        file.read(reinterpret_cast<char*>(&type_len), sizeof(type_len));
        if (type_len > 256) {
            std::cerr << "Поврежденные данные в " << filename << std::endl;
            return;
        }
        record.type.resize(type_len);
        file.read(&record.type[0], type_len);
        
        // Читаем вектор данных
        size_t data_size;
        file.read(reinterpret_cast<char*>(&data_size), sizeof(data_size));
        if (data_size > 1024) {  // защита от слишком больших векторов
            std::cerr << "Поврежденные данные в " << filename << std::endl;
            return;
        }
        record.data.resize(data_size);
        if (data_size > 0) {
            file.read(reinterpret_cast<char*>(record.data.data()), data_size * sizeof(float));
        }
        
        // Читаем importance
        file.read(reinterpret_cast<char*>(&record.importance), sizeof(record.importance));
        
        // Читаем метаданные
        size_t meta_size;
        file.read(reinterpret_cast<char*>(&meta_size), sizeof(meta_size));
        if (meta_size > 100) {  // защита от слишком большого количества метаданных
            std::cerr << "Поврежденные данные в " << filename << std::endl;
            return;
        }
        
        for (size_t m = 0; m < meta_size; m++) {
            size_t key_len, value_len;
            std::string key, value;
            
            file.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
            if (key_len > 256) {
                std::cerr << "Поврежденные данные в " << filename << std::endl;
                return;
            }
            key.resize(key_len);
            file.read(&key[0], key_len);
            
            file.read(reinterpret_cast<char*>(&value_len), sizeof(value_len));
            if (value_len > 256) {
                std::cerr << "Поврежденные данные в " << filename << std::endl;
                return;
            }
            value.resize(value_len);
            file.read(&value[0], value_len);
            
            record.metadata[key] = value;
        }
        
        data.push_back(std::move(record));
    }
}

// Сохранение всего в файлы
void MemoryManager::saveAll() {
    // Периодическая очистка перед сохранением
    cleanupOldRecords();
    
    // Сохраняем только если есть данные
    if (!shortTermMemory.empty()) {
        saveToFile("short_term.bin", shortTermMemory);
    }
    
    for (const auto& [comp, records] : longTermMemory) {
        if (!records.empty()) {
            saveToFile(comp + "_memory.bin", records);
        }
    }
    
    std::cout << "All memory saved to " << dumpPath << std::endl;
}

// Загрузка из файлов
void MemoryManager::loadAll() {
    // Загружаем с проверкой размера
    loadFromFile("short_term.bin", shortTermMemory);
    
    // Ограничиваем размер после загрузки
    if (shortTermMemory.size() > maxShortTermMemory) {
        shortTermMemory.erase(shortTermMemory.begin(), 
                             shortTermMemory.begin() + 
                             (shortTermMemory.size() - maxShortTermMemory));
    }
    
    std::cout << "Loaded short-term memory from " << dumpPath << std::endl;
}

// Консолидация кратковременной памяти в долговременную
void MemoryManager::consolidate(float threshold) {
    // Защита от рекурсии
    if (consolidating_.exchange(true)) {
        std::cerr << "Рекурсивный вызов consolidate предотвращен" << std::endl;
        return;
    }
    
    // Защита от слишком частых вызовов
    static auto last_call = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_call).count();
    
    if (elapsed < 100) {  // минимум 100ms между вызовами
        std::cerr << "consolidate вызывается слишком часто" << std::endl;
        consolidating_ = false;
        return;
    }
    last_call = now;
    
    if (shortTermMemory.empty()) {
        consolidating_ = false;
        return;
    }
    
    try {
        size_t consolidated = 0;
        
        // Сортируем по важности
        std::sort(shortTermMemory.begin(), shortTermMemory.end(),
                  [](const MemoryRecord& a, const MemoryRecord& b) {
                      return a.importance < b.importance;
                  });

        // Проходим по записям
        for (auto& record : shortTermMemory) {
            if (record.importance < threshold) {
                // Применяем пороговое обнуление
                pruneVector(record.data);
                
                // Сохраняем в долговременную память
                longTermMemory[record.component].push_back(record);
                consolidated++;
                
                // Ограничиваем размер
                if (longTermMemory[record.component].size() > maxLongTermPerComponent) {
                    size_t removeCount = longTermMemory[record.component].size() - maxLongTermPerComponent;
                    longTermMemory[record.component].erase(
                        longTermMemory[record.component].begin(),
                        longTermMemory[record.component].begin() + removeCount
                    );
                    std::vector<MemoryRecord>(longTermMemory[record.component]).swap(longTermMemory[record.component]);
                }
            }
        }
        
        // Очищаем кратковременную память
        shortTermMemory.clear();
        std::vector<MemoryRecord>().swap(shortTermMemory);
        
        // Очистка старых записей
        cleanupOldRecords();
        
        std::cout << "Memory consolidated: " << consolidated 
                  << " records (threshold: " << threshold << ")" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Ошибка в consolidate: " << e.what() << std::endl;
    }
    
    consolidating_ = false;
}
// Сохранить веса нейросети
void MemoryManager::saveWeights(const std::string& component, 
                               const std::vector<std::vector<double>>& weights,
                               int generation) {
    // Проверяем размер весов
    if (weights.size() > 32) {  // максимум 32x32 = 1024 веса
        std::cerr << "⚠️ Слишком большая матрица весов: " << weights.size() << "x" << weights.size() << std::endl;
        return;
    }
    
    std::vector<float> flatWeights;
    flatWeights.reserve(weights.size() * weights.size());
    
    for (const auto& row : weights) {
        for (double w : row) {
            flatWeights.push_back(static_cast<float>(w));
        }
    }
    
    std::map<std::string, std::string> metadata;
    metadata["generation"] = std::to_string(generation);
    metadata["matrix_size"] = std::to_string(weights.size());
    
    store(component, "weights", flatWeights, 0.9f, metadata);
}

// Загрузить веса
std::vector<std::vector<double>> MemoryManager::loadWeights(const std::string& component, int matrixSize) {
    auto similar = findSimilar(component, std::vector<float>(), 1);
    if (similar.empty()) return {};
    
    const auto& record = longTermMemory.at(component)[similar[0]];
    if (record.type != "weights") return {};
    
    // Проверяем, что данных достаточно
    if (record.data.size() < matrixSize * matrixSize) {
        std::cerr << "⚠️ Недостаточно данных для загрузки весов" << std::endl;
        return {};
    }
    
    std::vector<std::vector<double>> weights(matrixSize, 
                                             std::vector<double>(matrixSize, 0.0));
    
    int idx = 0;
    for (int i = 0; i < matrixSize && idx < record.data.size(); ++i) {
        for (int j = 0; j < matrixSize && idx < record.data.size(); ++j) {
            weights[i][j] = record.data[idx++];
        }
    }
    
    return weights;
}

std::vector<MemoryRecord> MemoryManager::getRecordsByIndices(
    const std::string& component,
    const std::vector<size_t>& indices) const
{
    std::vector<MemoryRecord> result;
    
    auto it = longTermMemory.find(component);
    if (it == longTermMemory.end()) return result;

    const auto& records = it->second;
    
    for (size_t idx : indices) {
        if (idx < records.size()) {
            result.push_back(records[idx]);
        }
    }
    
    return result;
}
// реализация энтропии 
void MemoryManager::storeWithEntropy(const std::string& component,
                                     const std::vector<float>& data,
                                     double entropy,
                                     float importance) {
    std::map<std::string, std::string> metadata;
    metadata["entropy"] = std::to_string(entropy);
    metadata["type"] = "entropy_pattern";
    
    store(component, "pattern", data, importance, metadata);
}

std::vector<MemoryRecord> MemoryManager::findHighEntropyRecords(const std::string& component,
                                                                double minEntropy) const {
    std::vector<MemoryRecord> result;
    
    auto it = longTermMemory.find(component);
    if (it == longTermMemory.end()) return result;
    
    for (const auto& record : it->second) {
        auto metaIt = record.metadata.find("entropy");
        if (metaIt != record.metadata.end()) {
            double entropy = std::stod(metaIt->second);
            if (entropy >= minEntropy) {
                result.push_back(record);
            }
        }
    }
    
    return result;
}