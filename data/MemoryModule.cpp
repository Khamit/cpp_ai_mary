// MemoryModule.cpp
#include "MemoryModule.hpp"
#include <algorithm>
#include <fstream>
#include <numeric>
#include <cmath>

MemoryRecord::MemoryRecord(const std::vector<float>& feats, uint8_t act, float rew, float util, float decay)
    : timestamp(std::chrono::steady_clock::now()),
      features(feats),
      action(act),
      reward(rew),
      utility(util),
      decay_rate(decay),
      access_count(0)
{}

MemoryController::MemoryController(const MemoryConfig& cfg) : config_(cfg) {}

void MemoryController::addRecord(const MemoryRecord& record) {
    if (records_.size() >= config_.max_records) {
        removeLowestUtility();
    }
    records_.push_back(record);
}

void MemoryController::removeLowestUtility() {
    if (records_.empty()) return;
    auto it = std::min_element(records_.begin(), records_.end(),
        [](const MemoryRecord& a, const MemoryRecord& b) {
            return a.utility < b.utility;
        });
    records_.erase(it);
}

std::vector<size_t> MemoryController::findSimilar(const std::vector<float>& queryFeatures, size_t k) const {
    if (records_.empty() || k == 0) return {};

    std::vector<SimilarityIndex> sims;
    sims.reserve(records_.size());

    for (size_t i = 0; i < records_.size(); ++i) {
        float sim = cosineSimilarity(queryFeatures, records_[i].features);
        sims.push_back({i, sim});
    }

    // Частичная сортировка для получения top-k
    std::partial_sort(sims.begin(), sims.begin() + std::min(k, sims.size()), sims.end(),
        [](const SimilarityIndex& a, const SimilarityIndex& b) {
            return a.similarity > b.similarity;
        });

    std::vector<size_t> result;
    size_t count = std::min(k, sims.size());
    for (size_t i = 0; i < count; ++i) {
        if (sims[i].similarity > 0) // можно отсеять по порогу
            result.push_back(sims[i].index);
    }
    return result;
}

void MemoryController::decayAll() {
    for (auto& rec : records_) {
        rec.utility *= config_.global_decay_factor;
        // также можно уменьшать на основе decay_rate, если он индивидуален
    }
}

float MemoryController::cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) const {
    if (a.size() != b.size() || a.empty()) return 0.0f;
    float dot = 0.0f, normA = 0.0f, normB = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        normA += a[i] * a[i];
        normB += b[i] * b[i];
    }
    if (normA == 0.0f || normB == 0.0f) return 0.0f;
    return dot / (std::sqrt(normA) * std::sqrt(normB));
}

bool MemoryController::saveToFile(const std::string& filename) const {
    std::ofstream file(filename, std::ios::binary);
    if (!file) return false;

    // Сохраняем конфигурацию и количество записей
    size_t dim = config_.feature_dim;
    size_t count = records_.size();
    file.write(reinterpret_cast<const char*>(&dim), sizeof(dim));
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& rec : records_) {
        // Сохраняем features (фиксированный размер)
        file.write(reinterpret_cast<const char*>(rec.features.data()), dim * sizeof(float));
        // Сохраняем остальные поля
        file.write(reinterpret_cast<const char*>(&rec.action), sizeof(rec.action));
        file.write(reinterpret_cast<const char*>(&rec.reward), sizeof(rec.reward));
        file.write(reinterpret_cast<const char*>(&rec.utility), sizeof(rec.utility));
        file.write(reinterpret_cast<const char*>(&rec.decay_rate), sizeof(rec.decay_rate));
        file.write(reinterpret_cast<const char*>(&rec.access_count), sizeof(rec.access_count));
        // timestamp не сохраняем (при загрузке будет создано новое)
    }
    return true;
}

bool MemoryController::loadFromFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) return false;

    size_t dim, count;
    file.read(reinterpret_cast<char*>(&dim), sizeof(dim));
    if (dim != config_.feature_dim) return false; // несовместимость размерности

    file.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (count > config_.max_records) return false;

    records_.clear();
    records_.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        MemoryRecord rec;
        rec.features.resize(dim);
        file.read(reinterpret_cast<char*>(rec.features.data()), dim * sizeof(float));
        file.read(reinterpret_cast<char*>(&rec.action), sizeof(rec.action));
        file.read(reinterpret_cast<char*>(&rec.reward), sizeof(rec.reward));
        file.read(reinterpret_cast<char*>(&rec.utility), sizeof(rec.utility));
        file.read(reinterpret_cast<char*>(&rec.decay_rate), sizeof(rec.decay_rate));
        file.read(reinterpret_cast<char*>(&rec.access_count), sizeof(rec.access_count));
        rec.timestamp = std::chrono::steady_clock::now(); // установим текущее время
        records_.push_back(rec);
    }
    return true;
}

void MemoryController::clear() {
    records_.clear();
}