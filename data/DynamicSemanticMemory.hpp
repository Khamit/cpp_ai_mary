// DynamicSemanticMemory.hpp - полная интеграция с NeuralFieldSystem
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
#include "../core/MemoryManager.hpp"
#include "PersonnelData.hpp"

struct UserProfile {
    std::string name;
    std::vector<std::string> preferred_words;      // любимые слова
    std::vector<std::string> disliked_words;       // нелюбимые слова
    std::map<std::string, float> emotional_triggers; // слово -> интенсивность эмоции
    std::deque<float> feedback_history;            // последние 100 фидбеков
    std::deque<float> mood_history;                // настроение по времени
    std::map<std::string, int> phrase_frequency;   // частота фраз
    
    float average_mood = 0.5f;
    float trust_level = 0.5f;
    int interaction_count = 0;
    float energy_level = 0.7f;                     // энергия пользователя
    
    // Временные метки
    std::chrono::system_clock::time_point last_interaction;
    std::chrono::system_clock::time_point created_at;
    
    UserProfile() {
        created_at = std::chrono::system_clock::now();
        last_interaction = created_at;
    }
    
    // Обновить среднее настроение
    void updateMood(float feedback) {
        feedback_history.push_back(feedback);
        if (feedback_history.size() > 100) feedback_history.pop_front();
        
        float sum = 0;
        for (float f : feedback_history) sum += f;
        average_mood = sum / feedback_history.size();
        
        // Энергия зависит от настроения
        energy_level = 0.5f + average_mood * 0.5f;
    }
};

class DynamicSemanticMemory {
private:
    NeuralFieldSystem& neural_system;
    MemoryManager& memory_manager;
    PersonnelDatabase& personnel_db;
    
    // Орбитальные уровни для разных типов знаний
    static constexpr int ORBIT_LETTERS = 0;      // буквы
    static constexpr int ORBIT_WORDS = 1;        // слова
    static constexpr int ORBIT_PATTERNS = 2;     // паттерны
    static constexpr int ORBIT_MEANINGS = 3;     // смыслы
    static constexpr int ORBIT_ELITE = 4;        // элитные знания
    
    // Кодирование букв (орбита 0)
    std::map<char, std::vector<int>> letter_codes;
    
    // Кодирование слов (орбита 1)
    struct WordCode {
        std::vector<int> active_neurons;
        std::string word;
        int frequency = 0;
        float emotional_value = 0.0f;
        float importance = 0.5f;
    };
    std::map<std::string, WordCode> word_codes;
    
    // Кодирование паттернов (орбита 2)
    struct PatternCode {
        std::vector<int> word_neurons;
        std::vector<int> pattern_neurons;
        std::vector<std::string> words;
        int frequency = 0;
        float predictive_power = 0.5f;  // насколько хорошо предсказывает следующий паттерн
    };
    std::map<std::string, PatternCode> pattern_codes;
    
    // Смыслы (орбита 3)
    struct Meaning {
        std::vector<int> pattern_neurons;
        std::vector<int> meaning_neurons;
        std::string description;
        float confidence = 0.5f;
        std::vector<std::string> associated_emotions;
    };
    std::vector<Meaning> meanings;
    
    // Персональные профили (связь с PersonnelDatabase)
    std::map<std::string, UserProfile> user_profiles;
    
    // Краткосрочная память (последние слова)
    std::deque<std::string> short_term_words;
    static constexpr int SHORT_TERM_SIZE = 10;
    
    // Контекст диалога
    std::deque<std::vector<uint32_t>> recent_meanings;
    static constexpr int CONTEXT_SIZE = 5;
    
    // Генераторы
    std::mt19937 rng;
    std::mutex memory_mutex;
    
public:
    DynamicSemanticMemory(NeuralFieldSystem& ns, MemoryManager& mm, PersonnelDatabase& db)
        : neural_system(ns), memory_manager(mm), personnel_db(db), rng(std::random_device{}()) {
        
        initializeLetterCodes();
        loadFromMemory();
    }

    int getWordCount() const { return word_codes.size(); }
    int getPatternCount() const { return pattern_codes.size(); }
    int getMeaningCount() const { return meanings.size(); }

    const std::map<std::string, WordCode>& getWordCodes() const { return word_codes; }
    std::map<std::string, WordCode>& getWordCodesNonConst() { return word_codes; }


    // Доступ к word_codes через публичные методы
    bool hasWord(const std::string& word) const {
        return word_codes.find(word) != word_codes.end();
    }

    const WordCode* getWordCode(const std::string& word) const {
        auto it = word_codes.find(word);
        if (it != word_codes.end()) {
            return &it->second;
        }
        return nullptr;
    }

    // Для итерации по словам
    const std::map<std::string, WordCode>& getAllWordCodes() const {
        return word_codes;
    }
    
    // ===== ИНИЦИАЛИЗАЦИЯ =====
    void initializeLetterCodes() {
        // 26 букв + 10 цифр + знаки препинания
        std::string chars = "abcdefghijklmnopqrstuvwxyz0123456789 .,!?;:";
        
        for (size_t idx = 0; idx < chars.size(); idx++) {
            char c = chars[idx];
            std::vector<int> neurons;
            
            // Используем 3 нейрона для кодирования каждой буквы
            for (int i = 0; i < 3; i++) {
                int neuron_idx = (idx * 7 + i * 13) % 32;
                neurons.push_back(neuron_idx);
            }
            letter_codes[c] = neurons;
        }
        
        // Активируем нейроны букв в группе 0 (орбита 0)
        auto& group0 = neural_system.getGroupsNonConst()[0];
        for (const auto& [c, neurons] : letter_codes) {
            for (int n : neurons) {
                group0.getPhiNonConst()[n] = 0.5f;
                if (group0.getOrbitLevel(n) < ORBIT_LETTERS) {
                    group0.publicPromoteToBaseOrbit(n);
                }
            }
        }
    }
    
    // ===== ЗАГРУЗКА/СОХРАНЕНИЕ =====
    void saveToMemory() {
        std::lock_guard<std::mutex> lock(memory_mutex);
        
        // Сохраняем слова
        for (const auto& [word, code] : word_codes) {
            std::vector<float> data;
            data.push_back(static_cast<float>(code.frequency));
            data.push_back(code.emotional_value);
            data.push_back(code.importance);
            for (int n : code.active_neurons) data.push_back(static_cast<float>(n));
            
            memory_manager.storeWithEntropy("words_" + word, data, 
                                           computeEntropyForWord(word), code.importance);
        }
        
        // Сохраняем паттерны
        for (const auto& [key, pattern] : pattern_codes) {
            std::vector<float> data;
            data.push_back(static_cast<float>(pattern.frequency));
            data.push_back(pattern.predictive_power);
            
            memory_manager.storeWithEntropy("pattern_" + key, data, 0.5f, pattern.predictive_power);
        }
        
        // Сохраняем смыслы
        for (size_t i = 0; i < meanings.size(); i++) {
            const auto& m = meanings[i];
            std::vector<float> data;
            data.push_back(m.confidence);
            memory_manager.storeWithEntropy("meaning_" + std::to_string(i), data, 0.7f, m.confidence);
        }
        
        // Сохраняем пользовательские профили
        for (const auto& [name, profile] : user_profiles) {
            std::vector<float> data;
            data.push_back(profile.average_mood);
            data.push_back(profile.trust_level);
            data.push_back(profile.energy_level);
            data.push_back(static_cast<float>(profile.interaction_count));
            
            memory_manager.storeWithEntropy("user_" + name, data, 0.3f, profile.trust_level);
        }
        
        std::cout << "💾 DynamicSemanticMemory saved to persistent storage" << std::endl;
    }
    
    void loadFromMemory() {
        std::lock_guard<std::mutex> lock(memory_mutex);
        
        // Загружаем пользовательские профили
        auto user_records = memory_manager.getRecords("user_");
        for (const auto& record : user_records) {
            if (record.type == "pattern" && record.data.size() >= 4) {
                std::string name = record.component.substr(5); // убираем "user_"
                UserProfile profile;
                profile.average_mood = record.data[0];
                profile.trust_level = record.data[1];
                profile.energy_level = record.data[2];
                profile.interaction_count = static_cast<int>(record.data[3]);
                user_profiles[name] = profile;
            }
        }
        
        std::cout << "📂 Loaded " << user_profiles.size() << " user profiles from memory" << std::endl;
    }
    
    // ===== ОБРАБОТКА ТЕКСТА =====
    
    std::vector<uint32_t> processText(const std::string& text, const std::string& user_name = "guest") {
        std::lock_guard<std::mutex> lock(memory_mutex);
        
        // 1. Получаем или создаём профиль пользователя
        auto& profile = getUserProfile(user_name);
        
        // 2. Разбираем текст на слова
        auto words = splitText(text);
        
        // 3. Активируем буквы (орбита 0)
        activateLetters(text);
        
        // 4. Активируем/создаём слова (орбита 1)
        std::vector<uint32_t> word_ids;
        for (const auto& word : words) {
            uint32_t word_id = activateWord(word, profile);
            word_ids.push_back(word_id);
            short_term_words.push_back(word);
            if (short_term_words.size() > SHORT_TERM_SIZE) {
                short_term_words.pop_front();
            }
        }
        
        // 5. Обнаруживаем паттерны (орбита 2)
        std::vector<uint32_t> pattern_ids = detectPatterns(short_term_words);
        
        // 6. Формируем смыслы (орбита 3)
        std::vector<uint32_t> meaning_ids = formMeanings(pattern_ids);
        
        // 7. Обновляем профиль пользователя на основе фидбека
        updateUserProfile(profile, words, meaning_ids);
        
        // 8. Сохраняем в историю
        recent_meanings.push_back(meaning_ids);
        if (recent_meanings.size() > CONTEXT_SIZE) {
            recent_meanings.pop_front();
        }
        
        return meaning_ids;
    }
    
    // ===== АКТИВАЦИЯ УРОВНЕЙ =====
    
    void activateLetters(const std::string& text) {
        auto& group0 = neural_system.getGroupsNonConst()[0];
        
        // Сбрасываем активность букв (но не полностью)
        for (int i = 0; i < 32; i++) {
            group0.getPhiNonConst()[i] *= 0.5f;
        }
        
        for (char c : text) {
            char lower = std::tolower(c);
            auto it = letter_codes.find(lower);
            if (it != letter_codes.end()) {
                for (int neuron : it->second) {
                    group0.getPhiNonConst()[neuron] = 1.0f;
                    if (group0.getOrbitLevel(neuron) < ORBIT_LETTERS) {
                        group0.publicPromoteToBaseOrbit(neuron);
                    }
                }
            }
        }
    }
    
    uint32_t activateWord(const std::string& word, UserProfile& profile) {
        auto& group1 = neural_system.getGroupsNonConst()[1];
        
        auto it = word_codes.find(word);
        if (it == word_codes.end()) {
            // Новое слово - создаём код
            WordCode code;
            code.word = word;
            code.frequency = 1;
            
            // Генерируем уникальный паттерн из 3-5 нейронов
            std::hash<std::string> hasher;
            size_t hash = hasher(word);
            
            int num_neurons = 3 + (hash % 3); // 3-5 нейронов
            for (int i = 0; i < num_neurons; i++) {
                int neuron_idx = (hash >> (i * 8)) % 32;
                code.active_neurons.push_back(neuron_idx);
                
                // Активируем нейрон
                group1.getPhiNonConst()[neuron_idx] = 1.0f;
                if (group1.getOrbitLevel(neuron_idx) < ORBIT_WORDS) {
                    group1.publicPromoteToBaseOrbit(neuron_idx);
                }
            }
            
            // Проверяем эмоциональную окраску слова
            code.emotional_value = detectEmotionalValue(word);
            code.importance = 0.5f + code.emotional_value * 0.3f;
            
            word_codes[word] = code;
            it = word_codes.find(word);
        } else {
            // Известное слово - увеличиваем частоту
            it->second.frequency++;
            it->second.importance = std::min(1.0f, it->second.importance + 0.01f);
            
            // Активируем его нейроны
            for (int neuron : it->second.active_neurons) {
                group1.getPhiNonConst()[neuron] = 1.0f;
            }
        }
        
        // Обновляем частоту в профиле пользователя
        profile.phrase_frequency[word]++;
        
        return std::hash<std::string>{}(word);
    }
    
    std::vector<uint32_t> detectPatterns(const std::deque<std::string>& recent_words) {
        auto& group2 = neural_system.getGroupsNonConst()[2];
        std::vector<uint32_t> pattern_ids;
        
        // Проверяем 2-gram и 3-gram
        for (int n = 2; n <= 3; n++) {
            if (recent_words.size() >= n) {
                std::vector<std::string> ngram(recent_words.end() - n, recent_words.end());
                std::string key;
                for (const auto& w : ngram) key += w + "|";
                
                auto it = pattern_codes.find(key);
                if (it == pattern_codes.end()) {
                    // Новый паттерн
                    PatternCode pattern;
                    pattern.words = ngram;
                    pattern.frequency = 1;
                    
                    // Кодируем в нейроны орбиты 2
                    std::hash<std::string> hasher;
                    size_t hash = hasher(key);
                    
                    for (int i = 0; i < 4; i++) {
                        int neuron_idx = (hash >> (i * 8)) % 32;
                        pattern.pattern_neurons.push_back(neuron_idx);
                        
                        group2.getPhiNonConst()[neuron_idx] = 0.8f;
                        if (group2.getOrbitLevel(neuron_idx) < ORBIT_PATTERNS) {
                            for (int step = 0; step < 5; step++) {
                                group2.publicPromoteToBaseOrbit(neuron_idx);
                            }
                        }
                    }
                    
                    pattern_codes[key] = pattern;
                    it = pattern_codes.find(key);
                } else {
                    it->second.frequency++;
                }
                
                pattern_ids.push_back(std::hash<std::string>{}(key));
            }
        }
        
        return pattern_ids;
    }
    
    std::vector<uint32_t> formMeanings(const std::vector<uint32_t>& pattern_ids) {
        auto& group3 = neural_system.getGroupsNonConst()[3];
        auto& group4 = neural_system.getGroupsNonConst()[4];
        std::vector<uint32_t> meaning_ids;
        
        // Ищем корреляции между паттернами
        for (size_t i = 0; i < pattern_ids.size(); i++) {
            for (size_t j = i + 1; j < pattern_ids.size(); j++) {
                // Проверяем, есть ли уже смысл, объединяющий эти паттерны
                bool found = false;
                for (const auto& meaning : meanings) {
                    // Проверяем, содержит ли смысл оба паттерна
                    bool has_i = false, has_j = false;
                    for (int pn : meaning.pattern_neurons) {
                        if (pn == pattern_ids[i]) has_i = true;
                        if (pn == pattern_ids[j]) has_j = true;
                    }
                    if (has_i && has_j) {
                        found = true;
                        meaning_ids.push_back(std::hash<std::string>{}(meaning.description));
                        break;
                    }
                }
                
                if (!found) {
                    // Создаём новый смысл
                    Meaning meaning;
                    meaning.pattern_neurons = {static_cast<int>(pattern_ids[i]), 
                                               static_cast<int>(pattern_ids[j])};
                    meaning.confidence = 0.3f;
                    
                    // Кодируем в орбиту 3
                    for (int pn : meaning.pattern_neurons) {
                        int target_neuron = pn % 32;
                        meaning.meaning_neurons.push_back(target_neuron);
                        
                        group3.getPhiNonConst()[target_neuron] = 0.6f;
                        if (group3.getOrbitLevel(target_neuron) < ORBIT_MEANINGS) {
                            for (int step = 0; step < 10; step++) {
                                group3.publicPromoteToBaseOrbit(target_neuron);
                            }
                        }
                    }
                    
                    meanings.push_back(meaning);
                    meaning_ids.push_back(std::hash<std::string>{}(std::to_string(meanings.size())));
                }
            }
        }
        
        return meaning_ids;
    }
    
    // ===== ПРОФИЛИРОВАНИЕ ПОЛЬЗОВАТЕЛЯ =====
    
    UserProfile& getUserProfile(const std::string& name) {
        auto it = user_profiles.find(name);
        if (it == user_profiles.end()) {
            UserProfile new_profile;
            new_profile.name = name;
            
            // Загружаем из PersonnelDatabase если есть
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
        
        // Обновляем предпочтения на основе частоты слов
        for (const auto& word : words) {
            if (profile.phrase_frequency[word] > 10) {
                // Часто используемые слова - предпочтительные
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
        
        // Обновляем доверие
        if (feedback > 0.7f) {
            profile.trust_level = std::min(1.0f, profile.trust_level + 0.05f);
        } else if (feedback < 0.3f) {
            profile.trust_level = std::max(0.0f, profile.trust_level - 0.1f);
        }
        
        // Обновляем PersonnelDatabase
        auto& person = personnel_db.getOrCreatePerson(user_name);
        person.trust_score = profile.trust_level;
        person.interaction_count = profile.interaction_count;
        
        // Подкрепляем успешные смыслы
        if (feedback > 0.7f) {
            for (uint32_t meaning_id : meanings) {
                for (auto& m : this->meanings) {
                    bool found = false;
                    for (int n : m.meaning_neurons) {
                        if (n == meaning_id) {
                            found = true;
                            break;
                        }
                    }
                    if (found) {
                        m.confidence = std::min(1.0f, m.confidence + 0.1f);
                        
                        // Продвигаем на элитную орбиту
                        auto& group4 = neural_system.getGroupsNonConst()[4];
                        for (int n : m.meaning_neurons) {
                            group4.getPhiNonConst()[n] = m.confidence;
                            if (group4.getOrbitLevel(n) < ORBIT_ELITE) {
                                group4.publicPromoteToEliteOrbit(n);
                            }
                        }
                        break;
                    }
                }
            }
        } else if (feedback < 0.3f) {
            for (uint32_t meaning_id : meanings) {
                for (auto& m : this->meanings) {
                    bool found = false;
                    for (int n : m.meaning_neurons) {
                        if (n == meaning_id) {
                            found = true;
                            break;
                        }
                    }
                    if (found) {
                        m.confidence = std::max(0.0f, m.confidence - 0.05f);
                        break;
                    }
                }
            }
        }
        
        // ДОБАВИТЬ: передать feedback в STDP всех семантических групп
        if (feedback > 0.5f && current_step > 0) {
            for (int g = 16; g <= 21; g++) {
                auto& group = neural_system.getGroupsNonConst()[g];
                group.learnSTDP(feedback, current_step);
            }
            std::cout << "STDP updated for semantic groups with reward: " << feedback << std::endl;
        }
        
        // Сохраняем состояние
        saveToMemory();
        personnel_db.saveAll();
        
        std::cout << "User " << user_name << " updated: trust=" << profile.trust_level
                << ", mood=" << profile.average_mood << std::endl;
    }
    // ===== ГЕНЕРАЦИЯ ОТВЕТА =====
    
    std::string generateResponse(const std::string& user_name, const std::vector<uint32_t>& meanings) {
        auto& profile = getUserProfile(user_name);
        
        // 1. Проверяем элитные смыслы (орбита 4)
        auto& group4 = neural_system.getGroups()[4];
        std::vector<std::pair<float, int>> elite_activations;
        
        for (int i = 0; i < 32; i++) {
            if (group4.getOrbitLevel(i) >= 4 && group4.getPhi()[i] > 0.5f) {
                elite_activations.push_back({static_cast<float>(group4.getPhi()[i]), i});
            }
        }
        
        // 2. Если есть элитные активации, используем их
        if (!elite_activations.empty()) {
            std::sort(elite_activations.begin(), elite_activations.end(),
                     [](const auto& a, const auto& b) { return a.first > b.first; });
            
            // Ищем слово, связанное с этим элитным нейроном
            for (const auto& [word, code] : word_codes) {
                for (int n : code.active_neurons) {
                    if (n == elite_activations[0].second) {
                        return formatResponse(word, profile);
                    }
                }
            }
        }
        
        // 3. Иначе используем контекст
        if (!short_term_words.empty()) {
            return formatResponse(short_term_words.back(), profile);
        }
        
        // 4. Fallback
        return "...";
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
        // Простая эвристика для эмоциональной окраски
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
        auto it = word_codes.find(word);
        if (it == word_codes.end()) return 0.5;
        
        // Энтропия тем выше, чем чаще слово встречается
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
    
    // Получить профиль пользователя (для внешнего доступа)
    const UserProfile* getUserProfileConst(const std::string& name) const {
        auto it = user_profiles.find(name);
        if (it != user_profiles.end()) return &it->second;
        return nullptr;
    }
    
    // Получить все профили
    const std::map<std::string, UserProfile>& getAllProfiles() const {
        return user_profiles;
    }
};