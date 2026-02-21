// MemoryModule.hpp
#pragma once

#include <vector>
#include <cstdint>
#include <chrono>

// Конфигурация модуля памяти (можно вынести в ConfigStructs)
struct MemoryConfig {
    size_t max_records = 5000;          // максимальное количество записей
    size_t feature_dim = 64;             // размерность вектора признаков
    float global_decay_factor = 0.995f;  // коэффициент забывания за шаг
    float similarity_threshold = 0.8f;    // порог схожести (опционально)
};

// Одна запись в памяти
struct MemoryRecord {
    using TimePoint = std::chrono::steady_clock::time_point;

    TimePoint timestamp;                  // время создания
    std::vector<float> features;          // вектор признаков
    uint8_t action;                        // предпринятое действие
    float reward;                          // полученная награда
    float utility;                         // полезность (вычисляется нейросетью)
    float decay_rate;                      // индивидуальный коэффициент забывания
    uint32_t access_count;                  // сколько раз использовалась

    MemoryRecord() = default;
    MemoryRecord(const std::vector<float>& feats, uint8_t act, float rew, float util, float decay = 1.0f);
};

class MemoryController {
public:
    explicit MemoryController(const MemoryConfig& cfg = MemoryConfig{});

    // Добавить новую запись. Если достигнут лимит, удаляет запись с наименьшей utility.
    void addRecord(const MemoryRecord& record);

    // Найти k записей, наиболее похожих на заданный вектор признаков.
    // Возвращает вектор индексов (или копий записей) в порядке убывания схожести.
    std::vector<size_t> findSimilar(const std::vector<float>& queryFeatures, size_t k) const;

    // Применить затухание полезности ко всем записям (глобальное забывание).
    void decayAll();

    // Удалить запись с наименьшей полезностью.
    void removeLowestUtility();

    // Получить текущее количество записей.
    size_t size() const { return records_.size(); }

    // Доступ к записям (для чтения/сохранения).
    const std::vector<MemoryRecord>& getRecords() const { return records_; }

    // Сохранить состояние в файл (бинарный или текстовый).
    bool saveToFile(const std::string& filename) const;

    // Загрузить состояние из файла.
    bool loadFromFile(const std::string& filename);

    // Очистить всю память.
    void clear();

private:
    MemoryConfig config_;
    std::vector<MemoryRecord> records_;

    // Вычислить косинусную близость между двумя векторами.
    float cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) const;

    // Вспомогательная структура для сортировки при поиске.
    struct SimilarityIndex {
        size_t index;
        float similarity;
        bool operator<(const SimilarityIndex& other) const {
            return similarity > other.similarity; // для сортировки по убыванию
        }
    };
};