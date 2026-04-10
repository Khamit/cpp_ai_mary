// DynamicSemanticMemory.hpp - ИСПРАВЛЕНА: единое хранилище
#pragma once

#include <string>
#include <vector>
#include <map>
#include <deque>
#include <random>
#include <chrono>
#include <mutex>
#include <cmath>
#include "../core/NeuralFieldSystem.hpp"
#include "../core/EmergentCore.hpp"
#include "PersonnelData.hpp"

struct UserProfile {
    std::string name;
    std::vector<std::string> preferred_words;
    std::vector<std::string> disliked_words;
    std::map<std::string, float> emotional_triggers;
    std::deque<float> feedback_history;
    std::deque<float> mood_history;
    std::map<std::string, int> phrase_frequency;
    
    float average_mood = 0.5f;
    float trust_level = 0.5f;
    int interaction_count = 0;
    float energy_level = 0.7f;
    
    std::chrono::system_clock::time_point last_interaction;
    std::chrono::system_clock::time_point created_at;
    
    UserProfile() {
        created_at = std::chrono::system_clock::now();
        last_interaction = created_at;
    }
    
    void updateMood(float feedback) {
        feedback_history.push_back(feedback);
        if (feedback_history.size() > 100) feedback_history.pop_front();
        
        float sum = 0;
        for (float f : feedback_history) sum += f;
        average_mood = sum / feedback_history.size();
        energy_level = 0.5f + average_mood * 0.5f;
    }
    
    // Сериализация профиля в вектор
    std::vector<float> serialize() const {
        std::vector<float> data;
        data.push_back(average_mood);
        data.push_back(trust_level);
        data.push_back(energy_level);
        data.push_back(static_cast<float>(interaction_count));
        return data;
    }
    
    // Десериализация профиля из вектора
    void deserialize(const std::vector<float>& data) {
        if (data.size() >= 4) {
            average_mood = data[0];
            trust_level = data[1];
            energy_level = data[2];
            interaction_count = static_cast<int>(data[3]);
        }
    }
};

class DynamicSemanticMemory {
private:
    NeuralFieldSystem& neural_system;
    PersonnelDatabase& personnel_db;
    
    // Только для начальной активации — НЕ для назначения орбит!
    std::map<char, std::vector<int>> letter_activations;
    std::map<std::string, std::string>* web_trainer_cache_ = nullptr;
    // карта
    std::map<std::string, std::vector<int>> word_neuron_map;

    int process_counter_ = 0;
    static constexpr int CLEANUP_INTERVAL = 500;

    // ПРЯМОЕ отображение: слово → хэш (уже есть)
    std::map<std::string, uint32_t> word_to_hash;
    
    // ОБРАТНОЕ отображение: хэш → слово (НУЖНО ДОБАВИТЬ!)
    std::map<uint32_t, std::string> hash_to_word;
    
    // Память слов — только для частоты, НЕ для назначения нейронов
    struct WordRecord {
        std::string word;
        int frequency = 0;
        float emotional_value = 0.5f;
        float importance = 0.3f;
        
        // ограничение длины слова
        std::vector<float> serialize() const {
            std::vector<float> data;
            // Ограничиваем слово 20 символами
            std::string short_word = word.substr(0, 20);
            for (char c : short_word) data.push_back(static_cast<float>(c));
            data.push_back(0.0f);
            data.push_back(static_cast<float>(frequency));
            data.push_back(emotional_value);
            data.push_back(importance);
            return data;
        }
        
        void deserialize(const std::vector<float>& data, size_t& pos) {
            word.clear();
            while (pos < data.size() && data[pos] != 0.0f) {
                word += static_cast<char>(data[pos]);
                pos++;
            }
            pos++; // пропускаем разделитель
            if (pos + 2 < data.size()) {
                frequency = static_cast<int>(data[pos++]);
                emotional_value = data[pos++];
                importance = data[pos++];
            }
        }
    };
    std::map<std::string, WordRecord> word_records;
    
    // Паттерны — только для статистики
    struct PatternRecord {
        std::string key;
        int frequency = 0;
        float predictive_power = 0.5f;
        
        std::vector<float> serialize() const {
            std::vector<float> data;
            for (char c : key) data.push_back(static_cast<float>(c));
            data.push_back(0.0f);
            data.push_back(static_cast<float>(frequency));
            data.push_back(predictive_power);
            return data;
        }
        
        void deserialize(const std::vector<float>& data, size_t& pos) {
            key.clear();
            while (pos < data.size() && data[pos] != 0.0f) {
                key += static_cast<char>(data[pos]);
                pos++;
            }
            pos++;
            if (pos + 1 < data.size()) {
                frequency = static_cast<int>(data[pos++]);
                predictive_power = data[pos++];
            }
        }
    };
    std::map<std::string, PatternRecord> pattern_records;
    
    // Смыслы — только для статистики
    // Смыслы — храним как пары хэшей для эффективности
    struct MeaningRecord {
        uint32_t pattern1;      // первый хэш паттерна
        uint32_t pattern2;      // второй хэш паттерна
        float confidence = 0.5f;
        int usage_count = 0;
        
        // Для обратной совместимости (сериализация/десериализация)
        std::vector<float> serialize() const {
            std::vector<float> data;
            // Сохраняем как два числа
            data.push_back(static_cast<float>(pattern1));
            data.push_back(static_cast<float>(pattern2));
            data.push_back(0.0f);  // разделитель
            data.push_back(confidence);
            data.push_back(static_cast<float>(usage_count));
            return data;
        }
        
        void deserialize(const std::vector<float>& data, size_t& pos) {
            if (pos + 4 < data.size()) {
                pattern1 = static_cast<uint32_t>(data[pos++]);
                pattern2 = static_cast<uint32_t>(data[pos++]);
                pos++; // пропускаем разделитель
                confidence = data[pos++];
                usage_count = static_cast<int>(data[pos++]);
            }
        }
        
        // Для отладки - получить читаемое описание
        std::string getDescription(DynamicSemanticMemory* memory) const {
            if (memory) {
                return memory->getWordFromHash(pattern1) + " + " + memory->getWordFromHash(pattern2);
            }
            return std::to_string(pattern1) + "|" + std::to_string(pattern2);
        }
    };

    std::vector<MeaningRecord> meaning_records;
    
    // Персональные профили
    std::map<std::string, UserProfile> user_profiles;
    
    // Краткосрочная память
    std::deque<std::string> short_term_words;
    static constexpr int SHORT_TERM_SIZE = 10;
    
    std::deque<std::vector<uint32_t>> recent_meaning_hashes;
    static constexpr int CONTEXT_SIZE = 5;
    
    std::mt19937 rng;
    std::mutex memory_mutex;
    
public:
DynamicSemanticMemory(NeuralFieldSystem& ns, PersonnelDatabase& db)
    : neural_system(ns), personnel_db(db), rng(std::random_device{}()) {
    initializeLetterActivations();
    
    // НЕ загружаем данные при создании — загрузка происходит через loadFromMemory()
    std::cout << "DynamicSemanticMemory created (empty). Call loadFromMemory() to restore data." << std::endl;
}


    int getWordCount() const { return word_records.size(); }
    int getPatternCount() const { return pattern_records.size(); }
    int getMeaningCount() const { return meaning_records.size(); }

    void setWebTrainerCache(std::map<std::string, std::string>* cache) {
        web_trainer_cache_ = cache;
    }

    bool hasWord(const std::string& word) const {
        return word_records.find(word) != word_records.end();
    }

    const WordRecord* getWordRecord(const std::string& word) const {
        auto it = word_records.find(word);
        if (it != word_records.end()) return &it->second;
        return nullptr;
    }

    const std::vector<int>* getWordNeurons(const std::string& word) const {
        auto it = word_neuron_map.find(word);
        if (it != word_neuron_map.end()) {
            return &it->second;
        }
        return nullptr;
    }

    uint32_t getWordHash(const std::string& word) {
        auto it = word_to_hash.find(word);
        if (it != word_to_hash.end()) return it->second;
        
        uint32_t hash = std::hash<std::string>{}(word);
        word_to_hash[word] = hash;
        hash_to_word[hash] = word;  // ← СОХРАНЯЕМ ОБРАТНУЮ СВЯЗЬ
        return hash;
    }

    std::string getWordFromHash(uint32_t hash) {
        auto it = hash_to_word.find(hash);
        if (it != hash_to_word.end()) return it->second;
        return "unknown_" + std::to_string(hash);
    }

    // ТЕПЕРЬ смыслы можно сохранять как пары хэшей, но при выводе - расшифровывать
    std::string getMeaningDescription(uint32_t hash1, uint32_t hash2) {
        return getWordFromHash(hash1) + " + " + getWordFromHash(hash2);
    }

    const std::map<std::string, WordRecord>& getAllWordRecords() const {
        return word_records;
    }

    // Периодическая очистка (вызывать в processText)
    void checkAndCleanup() {
        process_counter_++;
        if (process_counter_ >= CLEANUP_INTERVAL) {
            process_counter_ = 0;
            cleanupOldRecords();
        }
    }
    
    // ===== ИНИЦИАЛИЗАЦИЯ — ТОЛЬКО АКТИВАЦИЯ =====
    void initializeLetterActivations() {
        std::string chars = "abcdefghijklmnopqrstuvwxyz0123456789 .,!?;:";
        
        for (size_t idx = 0; idx < chars.size(); idx++) {
            char c = chars[idx];
            std::vector<int> neurons;
            
            for (int i = 0; i < 3; i++) {
                int neuron_idx = (idx * 7 + i * 13) % 32;
                neurons.push_back(neuron_idx);
            }
            letter_activations[c] = neurons;
        }
        
        auto& group0 = neural_system.getGroupsNonConst()[0];
        for (const auto& [c, neurons] : letter_activations) {
            for (int n : neurons) {
                group0.getPhiNonConst()[n] = 0.3f;
            }
        }
    }
    
    // ===== ЕДИНОЕ СОХРАНЕНИЕ — ОДИН ФАЙЛ! =====
// ===== СОХРАНЕНИЕ В EMERGENT MEMORY (STM/LTM) =====

void saveToMemory() {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    // Получаем доступ к эмерджентной памяти через NeuralFieldSystem
    auto& emergent = neural_system.emergentMutable();
    
    int saved_count = 0;
    
    // 1. Сохраняем слова как паттерны в STM/LTM
    for (const auto& [word, record] : word_records) {
        auto ser = record.serialize();
        if (ser.size() <= 200) {
            // Используем EmergentMemory::writeSTM
            emergent.memory.writeSTM(
                ser,                           // pattern
                record.importance,             // importance
                0.5f,                          // entropy
                "word_" + word,                // tag
                neural_system.getCurrentStep() // step
            );
            saved_count++;
        }
    }
    
    // 2. Сохраняем паттерны
    for (const auto& [key, pattern] : pattern_records) {
        auto ser = pattern.serialize();
        if (ser.size() <= 200) {
            emergent.memory.writeSTM(
                ser,
                0.5f,
                0.5f,
                "pattern_" + key,
                neural_system.getCurrentStep()
            );
            saved_count++;
        }
    }
    
    // 3. Сохраняем смыслы
    for (const auto& m : meaning_records) {
        auto ser = m.serialize();
        if (ser.size() <= 200) {
            std::string key = std::to_string(m.pattern1) + "|" + std::to_string(m.pattern2);
            emergent.memory.writeSTM(
                ser,
                m.confidence,
                0.5f,
                "meaning_" + key,
                neural_system.getCurrentStep()
            );
            saved_count++;
        }
    }
    
    // 4. Сохраняем профили пользователей
    for (const auto& [name, profile] : user_profiles) {
        std::vector<float> profile_data;
        for (char c : name) profile_data.push_back(static_cast<float>(c));
        profile_data.push_back(0.0f);
        auto ser = profile.serialize();
        profile_data.insert(profile_data.end(), ser.begin(), ser.end());
        
        if (profile_data.size() <= 200) {
            emergent.memory.writeSTM(
                profile_data,
                0.7f,
                0.5f,
                "profile_" + name,
                neural_system.getCurrentStep()
            );
            saved_count++;
        }
    }
    
    // Принудительная консолидация в LTM
    emergent.memory.step(neural_system.getCurrentStep());
    
    std::cout << "💾 DynamicSemanticMemory saved to EmergentMemory (" << saved_count 
              << " records)" << std::endl;
}

void loadFromMemory() {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    std::cout << "📂 Loading DynamicSemanticMemory from EmergentMemory..." << std::endl;
    
    // Очищаем существующие данные
    word_records.clear();
    pattern_records.clear();
    meaning_records.clear();
    user_profiles.clear();
    
    auto& emergent = neural_system.emergentMutable();
    int current_step = neural_system.getCurrentStep();
    
    // Загружаем слова из LTM (паттерны с тегом "word_*")
    const auto& ltm = emergent.memory.getLTM();
    for (const auto& record : ltm) {
        if (record.tag.find("word_") == 0) {
            WordRecord wr;
            size_t pos = 0;
            wr.deserialize(record.pattern, pos);
            word_records[wr.word] = wr;
        }
        else if (record.tag.find("pattern_") == 0) {
            PatternRecord pr;
            size_t pos = 0;
            pr.deserialize(record.pattern, pos);
            pattern_records[pr.key] = pr;
        }
        else if (record.tag.find("meaning_") == 0) {
            MeaningRecord mr;
            size_t pos = 0;
            mr.deserialize(record.pattern, pos);
            meaning_records.push_back(mr);
        }
        else if (record.tag.find("profile_") == 0) {
            std::string name = record.tag.substr(8); // "profile_" length = 8
            UserProfile profile;
            profile.name = name;
            
            size_t pos = 0;
            while (pos < record.pattern.size() && record.pattern[pos] != 0.0f) {
                pos++;
            }
            pos++;
            
            std::vector<float> profile_data;
            for (int j = 0; j < 4 && pos < record.pattern.size(); j++) {
                profile_data.push_back(record.pattern[pos++]);
            }
            profile.deserialize(profile_data);
            
            user_profiles[profile.name] = profile;
        }
    }
    
    // Также проверяем STM (свежие данные)
    const auto& stm = emergent.memory.getSTM();
    for (const auto& record : stm) {
        if (record.tag.find("word_") == 0) {
            WordRecord wr;
            size_t pos = 0;
            wr.deserialize(record.pattern, pos);
            if (word_records.find(wr.word) == word_records.end()) {
                word_records[wr.word] = wr;
            }
        }
    }
    
    std::cout << "📂 Loaded " << word_records.size() << " words, "
              << pattern_records.size() << " patterns, "
              << meaning_records.size() << " meanings, "
              << user_profiles.size() << " profiles" << std::endl;
}

void cleanupOldRecords() {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    int removed_words = 0;
    int removed_patterns = 0;
    int removed_meanings = 0;
    
    // 1. Удаляем слова с низкой важностью
    for (auto it = word_records.begin(); it != word_records.end(); ) {
        bool should_remove = false;
        
        // Критерии удаления:
        // - важность < 0.1 И частота < 3
        // - или не использовалось давно (можно добавить timestamp)
        if (it->second.importance < 0.1f && it->second.frequency < 3) {
            should_remove = true;
        }
        
        if (should_remove) {
            it = word_records.erase(it);
            removed_words++;
        } else {
            ++it;
        }
    }
    
    // 2. Удаляем старые паттерны
    for (auto it = pattern_records.begin(); it != pattern_records.end(); ) {
        if (it->second.frequency < 2 && it->second.predictive_power < 0.3f) {
            it = pattern_records.erase(it);
            removed_patterns++;
        } else {
            ++it;
        }
    }
    
    // 3. Удаляем смыслы
    size_t old_meanings = meaning_records.size();
    meaning_records.erase(
        std::remove_if(meaning_records.begin(), meaning_records.end(),
            [](const MeaningRecord& m) { 
                return m.usage_count == 0 && m.confidence < 0.3f; 
            }),
        meaning_records.end());
    removed_meanings = old_meanings - meaning_records.size();
    
    if (removed_words > 0 || removed_patterns > 0 || removed_meanings > 0) {
        std::cout << "🧹 Cleaned up: " 
                  << removed_words << " words, "
                  << removed_patterns << " patterns, "
                  << removed_meanings << " meanings" << std::endl;
    }
}
    
    // ===== ОСТАЛЬНЫЕ МЕТОДЫ (без изменений) =====
    
    std::vector<uint32_t> processText(const std::string& text, const std::string& user_name = "guest") {
        std::lock_guard<std::mutex> lock(memory_mutex);
        
        auto& profile = getUserProfile(user_name);
        auto words = splitText(text);
        
        activateLetters(text);
        
        std::vector<uint32_t> word_hashes;
        for (const auto& word : words) {
            uint32_t hash = activateWord(word, profile);
            word_hashes.push_back(hash);
            short_term_words.push_back(word);
            if (short_term_words.size() > SHORT_TERM_SIZE) {
                short_term_words.pop_front();
            }
        }
        
        std::vector<uint32_t> pattern_hashes = detectPatterns(short_term_words);
        std::vector<uint32_t> meaning_hashes = updateMeanings(pattern_hashes);
        
        updateUserProfile(profile, words, meaning_hashes);
        
        recent_meaning_hashes.push_back(meaning_hashes);
        if (recent_meaning_hashes.size() > CONTEXT_SIZE) {
            recent_meaning_hashes.pop_front();
        }
        
        return meaning_hashes;
    }
    
    void activateLetters(const std::string& text) {
        auto& group0 = neural_system.getGroupsNonConst()[0];
        
        for (int i = 0; i < 32; i++) {
            group0.getPhiNonConst()[i] *= 0.7f;
        }
        
        for (char c : text) {
            char lower = std::tolower(c);
            auto it = letter_activations.find(lower);
            if (it != letter_activations.end()) {
                for (int neuron : it->second) {
                    float current = group0.getPhiNonConst()[neuron];
                    group0.getPhiNonConst()[neuron] = std::min(1.0f, current + 0.3f);
                }
            }
        }
    }
    
    uint32_t activateWord(const std::string& word, UserProfile& profile) {
        // ФИКСИРОВАННЫЙ паттерн для каждого слова!
        
        
        auto it = word_neuron_map.find(word);
        if (it == word_neuron_map.end()) {
            std::hash<std::string> hasher;
            size_t hash = hasher(word);
            
            std::vector<int> neurons;
            for (int i = 0; i < 4; i++) {  // всегда 4 нейрона
                int neuron_idx = (hash >> (i * 8)) % 32;
                neurons.push_back(neuron_idx);
            }
            word_neuron_map[word] = neurons;
            it = word_neuron_map.find(word);
        }
        
        // ИСПРАВЛЕНО: используем правильную переменную group1
        auto& group1 = neural_system.getGroupsNonConst()[1];
        
        // ВСЕГДА активируем одни и те же нейроны
        for (int neuron_idx : it->second) {
            group1.getPhiNonConst()[neuron_idx] = 1.0f;
        }
        
        // Обновляем запись слова (частота и важность)
        auto word_it = word_records.find(word);
        if (word_it == word_records.end()) {
            WordRecord record;
            record.word = word;
            record.frequency = 1;
            record.emotional_value = detectEmotionalValue(word);
            record.importance = 0.8f + record.emotional_value * 0.2f;
            word_records[word] = record;
        } else {
            word_it->second.frequency++;
            word_it->second.importance = std::min(0.8f, word_it->second.importance + 0.01f);
        }
        
        profile.phrase_frequency[word]++;
        
        std::hash<std::string> hasher;
        return static_cast<uint32_t>(hasher(word));
    }
    std::vector<uint32_t> detectPatterns(const std::deque<std::string>& recent_words) {
        std::vector<uint32_t> pattern_hashes;
        
        for (int n = 2; n <= 3; n++) {
            if (recent_words.size() >= n) {
                std::vector<std::string> ngram(recent_words.end() - n, recent_words.end());
                std::string key;
                for (const auto& w : ngram) key += w + "|";
                
                auto it = pattern_records.find(key);
                if (it == pattern_records.end()) {
                    PatternRecord pattern;
                    pattern.key = key;
                    pattern.frequency = 1;
                    pattern_records[key] = pattern;
                } else {
                    it->second.frequency++;
                }
                
                auto& group2 = neural_system.getGroupsNonConst()[2];
                std::hash<std::string> hasher;
                size_t hash = hasher(key);
                
                for (int i = 0; i < 3; i++) {
                    int neuron_idx = (hash >> (i * 8)) % 32;
                    float current = group2.getPhiNonConst()[neuron_idx];
                    group2.getPhiNonConst()[neuron_idx] = std::min(1.0f, current + 0.3f);
                }
                
                pattern_hashes.push_back(static_cast<uint32_t>(hash));
            }
        }
        
        return pattern_hashes;
    }
    
std::vector<uint32_t> updateMeanings(const std::vector<uint32_t>& pattern_hashes) {
    std::vector<uint32_t> meaning_hashes;
    auto& group3 = neural_system.getGroupsNonConst()[3];
    
    for (size_t i = 0; i < pattern_hashes.size(); i++) {
        for (size_t j = i + 1; j < pattern_hashes.size(); j++) {
            uint64_t combined = (static_cast<uint64_t>(pattern_hashes[i]) << 32) | pattern_hashes[j];
            uint32_t meaning_hash = static_cast<uint32_t>(std::hash<uint64_t>{}(combined));
            
            bool found = false;
            for (auto& m : meaning_records) {
                if (m.pattern1 == pattern_hashes[i] && m.pattern2 == pattern_hashes[j]) {
                    m.usage_count++;
                    m.confidence = std::clamp(m.confidence + 0.05f, 0.0f, 1.0f);
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                MeaningRecord meaning;
                meaning.pattern1 = pattern_hashes[i];
                meaning.pattern2 = pattern_hashes[j];
                meaning.confidence = 0.2f;
                meaning.usage_count = 1;
                meaning_records.push_back(meaning);
                
                for (int k = 0; k < 3; k++) {
                    int neuron_idx = (meaning_hash >> (k * 8)) % 32;
                    float current = group3.getPhiNonConst()[neuron_idx];
                    group3.getPhiNonConst()[neuron_idx] = std::clamp(current + 0.2f, 0.0f, 1.0f);
                }
            }
            
            meaning_hashes.push_back(meaning_hash);
        }
    }
    
    return meaning_hashes;
}

    void learnFact(const std::string& question, const std::string& answer, int step) {
        auto q_hashes = processText(question, "system");
        auto a_hashes = processText(answer, "system");
        saveToMemory();
    }
    
    // ===== ПРОФИЛИРОВАНИЕ ПОЛЬЗОВАТЕЛЯ =====
    UserProfile& getUserProfile(const std::string& name) {
        auto it = user_profiles.find(name);
        if (it == user_profiles.end()) {
            UserProfile new_profile;
            new_profile.name = name;
            
            const auto* person = personnel_db.getPerson(name);
            if (person) {
                new_profile.trust_level = person->trust_score;
                new_profile.interaction_count = person->interaction_count;
            }
            
            user_profiles[name] = new_profile;
            return user_profiles[name];
        }
        return it->second;
    }
    
    void updateUserProfile(UserProfile& profile, const std::vector<std::string>& words, 
                          const std::vector<uint32_t>& meanings) {
        profile.interaction_count++;
        profile.last_interaction = std::chrono::system_clock::now();
        
        for (const auto& word : words) {
            if (profile.phrase_frequency[word] > 10) {
                if (std::find(profile.preferred_words.begin(), profile.preferred_words.end(), word) 
                    == profile.preferred_words.end()) {
                    profile.preferred_words.push_back(word);
                }
            }
        }
    }
    
    void applyUserFeedback(const std::string& user_name, float feedback, 
                        const std::vector<uint32_t>& meanings, int current_step = 0) {
        auto& profile = getUserProfile(user_name);
        profile.updateMood(feedback);
        
        if (feedback > 0.7f) {
            profile.trust_level = std::min(1.0f, profile.trust_level + 0.05f);
        } else if (feedback < 0.3f) {
            profile.trust_level = std::max(0.0f, profile.trust_level - 0.1f);
        }
        
        auto& person = personnel_db.getOrCreatePerson(user_name);
        person.trust_score = profile.trust_level;
        person.interaction_count = profile.interaction_count;
        
        if (feedback > 0.7f && current_step > 0) {
            auto& group3 = neural_system.getGroupsNonConst()[3];
            std::vector<int> active_neurons;
            
            for (int i = 0; i < 32; i++) {
                if (group3.getPhi()[i] > 0.5f) {
                    active_neurons.push_back(i);
                }
            }
            
            for (int neuron : active_neurons) {
                group3.learnSTDP(feedback, current_step);
                float current_mass = group3.getMass(neuron);
                group3.setMass(neuron, current_mass * 1.05f);
            }
        }
        
        saveToMemory();
        personnel_db.saveAll();
        
        std::cout << "User " << user_name << " updated: trust=" << profile.trust_level
                << ", mood=" << profile.average_mood << std::endl;
    }

// ===== ГЕНЕРАЦИЯ ОТВЕТА (используя EmergentMemory) =====
std::string generateResponse(const std::string& user_name, const std::vector<uint32_t>& meanings) {
    auto& profile = getUserProfile(user_name);
    
    // 1. Проверяем web_trainer_cache (приоритет)
    if (web_trainer_cache_ && !web_trainer_cache_->empty()) {
        if (!short_term_words.empty()) {
            std::string last_input = short_term_words.back();
            for (const auto& [query, answer] : *web_trainer_cache_) {
                if (last_input.find(query) != std::string::npos ||
                    query.find(last_input) != std::string::npos) {
                    return formatResponse(answer, profile);
                }
            }
        }
        
        auto it = web_trainer_cache_->begin();
        if (it != web_trainer_cache_->end()) {
            return formatResponse(it->second, profile);
        }
    }
    
    // 2. Ищем в EmergentMemory (LTM) ответы с высоким качеством
    auto& emergent = neural_system.emergentMutable();
    const auto& ltm = emergent.memory.getLTM();
    
    // Собираем все записи с тегом "response_" и высокой важностью
    std::vector<std::pair<std::string, float>> responses;
    for (const auto& record : ltm) {
        if (record.tag.find("response_") == 0 && record.importance > 0.7f) {
            // Восстанавливаем строку ответа из паттерна
            std::string answer;
            for (float c : record.pattern) {
                if (c == 0.0f) break;
                answer += static_cast<char>(c);
            }
            if (!answer.empty()) {
                responses.push_back({answer, record.importance});
            }
        }
    }
    
    // Сортируем по важности и берём лучший
    if (!responses.empty()) {
        std::sort(responses.begin(), responses.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        return formatResponse(responses[0].first, profile);
    }
    
    // 3. Проверяем STM на свежие ответы
    const auto& stm = emergent.memory.getSTM();
    for (const auto& record : stm) {
        if (record.tag.find("response_") == 0 && record.importance > 0.6f) {
            std::string answer;
            for (float c : record.pattern) {
                if (c == 0.0f) break;
                answer += static_cast<char>(c);
            }
            if (!answer.empty()) {
                return formatResponse(answer, profile);
            }
        }
    }
    
    // 4. Fallback: последнее слово из краткосрочной памяти
    if (!short_term_words.empty()) {
        std::string last_word = short_term_words.back();
        auto it = word_records.find(last_word);
        if (it != word_records.end()) {
            return formatResponse(last_word, profile);
        }
    }
    
    return "...";
}

// Вспомогательный метод для сохранения правильных ответов в EmergentMemory
void storeCorrectResponse(const std::string& response, float importance, int step) {
    auto& emergent = neural_system.emergentMutable();
    
    // Преобразуем строку в вектор float
    std::vector<float> pattern;
    for (char c : response) {
        pattern.push_back(static_cast<float>(c));
    }
    pattern.push_back(0.0f); // терминатор
    
    // Сохраняем в STM как ответ
    emergent.memory.writeSTM(
        pattern,
        importance,
        0.5f,
        "response_" + response.substr(0, 20), // тег с префиксом
        step
    );
}
    std::string formatResponse(const std::string& base_word, const UserProfile& profile) {
        if (profile.average_mood > 0.7f) {
            return "I'm happy to say: " + base_word + "! :)";
        } else if (profile.average_mood < 0.3f) {
            return "I think... " + base_word + "... :(";
        }
        return base_word;
    }
    
    // ===== ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ =====
    float detectEmotionalValue(const std::string& word) {
        static const std::map<std::string, float> emotional_words = {
            {"good", 0.8f}, {"great", 0.9f}, {"excellent", 1.0f},
            {"bad", 0.2f}, {"terrible", 0.1f}, {"awful", 0.0f},
            {"happy", 0.9f}, {"sad", 0.2f}, {"angry", 0.1f},
            {"love", 1.0f}, {"hate", 0.0f}, {"like", 0.7f}
        };
        
        auto it = emotional_words.find(word);
        if (it != emotional_words.end()) return it->second;
        return 0.5f;
    }
    
    double computeEntropyForWord(const std::string& word) {
        auto it = word_records.find(word);
        if (it == word_records.end()) return 0.5;
        return std::min(1.0, std::log(1 + it->second.frequency) / 5.0);
    }
    
    std::vector<std::string> splitText(const std::string& text) {
        std::vector<std::string> words;
        std::stringstream ss(text);
        std::string word;
        while (ss >> word) {
            word.erase(std::remove_if(word.begin(), word.end(), ::ispunct), word.end());
            std::transform(word.begin(), word.end(), word.begin(), ::tolower);
            if (!word.empty()) words.push_back(word);
        }
        return words;
    }
    
    const UserProfile* getUserProfileConst(const std::string& name) const {
        auto it = user_profiles.find(name);
        if (it != user_profiles.end()) return &it->second;
        return nullptr;
    }
    
    const std::map<std::string, UserProfile>& getAllProfiles() const {
        return user_profiles;
    }
};