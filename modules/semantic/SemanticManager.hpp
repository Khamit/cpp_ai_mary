// modules/semantic/SemanticManager.hpp
#pragma once
#include "SemanticGranule.hpp"
#include "../../core/NeuralFieldSystem.hpp"
#include <memory>
#include <sstream>
#include <algorithm>
#include <cctype>

// Вспомогательная функция split (была не определена)
inline std::vector<std::string> splitText(const std::string& text) {
    std::vector<std::string> words;
    std::stringstream ss(text);
    std::string word;
    while (ss >> word) {
        // Очистка от пунктуации
        word.erase(std::remove_if(word.begin(), word.end(), 
                   [](char c) { return std::ispunct(static_cast<unsigned char>(c)); }), 
                   word.end());
        // В нижний регистр
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        if (!word.empty()) {
            words.push_back(word);
        }
    }
    return words;
}

class SemanticManager {
private:
    // Всего 200 смыслов (мыслей)
    std::map<uint32_t, std::unique_ptr<SemanticGranule>> granules;
    
    // Маппинг слов на смыслы (для языка)
    std::map<std::string, std::vector<uint32_t>> word_to_granules;
    
    // Обратный маппинг для генерации текста
    std::map<uint32_t, std::string> granule_to_canonical;
    
    // 6 групп нейронов для обработки смыслов
    static constexpr int SEMANTIC_GROUPS_START = 16; // используем группы 16-21
    static constexpr int SEMANTIC_GROUPS_END = 21;   // 6 групп
    static constexpr int NUM_SEMANTIC_GROUPS = 6;
    
    NeuralFieldSystem& neural_system;
    
    // Для генерации ID
    uint32_t next_id = 1;
    
public:
    SemanticManager(NeuralFieldSystem& ns) : neural_system(ns) {
        initializeBasicConcepts();
    }
    
    // Инициализация базовых понятий (то, что нужно для ответов о системе)
    void initializeBasicConcepts() {
        // Категория 1: Система и состояние (id 1-40)
        addConcept("system_identity", {"mary", "ai", "assistant"}, "Mary");
        addConcept("system_capability", {"can", "able", "capable"}, "can do");
        addConcept("system_energy", {"energy", "power", "battery"}, "energy level");
        addConcept("system_learning", {"learn", "train", "adapt"}, "learning");
        
        // Категория 2: Коммуникация (id 41-80)
        addConcept("greeting", {"hello", "hi", "hey"}, "hello");
        addConcept("farewell", {"bye", "goodbye"}, "goodbye");
        addConcept("question", {"what", "who", "where", "when", "why", "how"}, "question");
        addConcept("acknowledgment", {"thanks", "ok", "understood"}, "okay");
        
        // Категория 3: Действия (id 81-120)
        addConcept("action_help", {"help", "assist"}, "help");
        addConcept("action_stop", {"stop", "pause", "halt"}, "stop");
        addConcept("action_start", {"start", "begin", "run"}, "start");
        addConcept("action_status", {"status", "report", "state"}, "status");
        
        // Категория 4: Отношения (id 121-160)
        addConcept("causality", {"because", "since", "therefore"}, "because");
        addConcept("possibility", {"maybe", "perhaps", "possibly"}, "maybe");
        addConcept("certainty", {"definitely", "certainly", "absolutely"}, "definitely");
        addConcept("uncertainty", {"unsure", "doubt", "uncertain"}, "not sure");
        
        // Категория 5: Оценка (id 161-200)
        addConcept("positive", {"good", "great", "excellent"}, "good");
        addConcept("negative", {"bad", "terrible", "awful"}, "bad");
        addConcept("sufficiency", {"enough", "sufficient", "adequate"}, "enough");
        addConcept("insufficiency", {"insufficient", "lacking", "missing"}, "insufficient");
        
        // Устанавливаем связи между смыслами (причина-следствие)
        establishRelationships();
    }
    
    // Добавление концепта с его словесными выражениями
    void addConcept(const std::string& concept_name, 
                    const std::vector<std::string>& expressions,
                    const std::string& canonical) {
        uint32_t id = next_id++;
        
        auto granule = std::make_unique<SemanticGranule>(id);
        
        // Устанавливаем сигнатуру (уникальный паттерн для группы)
        auto& sig = granule->getSignatureNonConst();
        // Используем хэш от id для создания уникального, но стабильного паттерна
        std::mt19937 rng(id * 1000);
        std::uniform_real_distribution<float> dist(0.3f, 0.8f);
        for (int i = 0; i < 32; i++) {
            sig[i] = dist(rng);
        }
        
        granule->setCanonicalForm(canonical);
        granules[id] = std::move(granule);
        granule_to_canonical[id] = canonical;
        
        // Связываем слова со смыслом
        for (const auto& word : expressions) {
            word_to_granules[word].push_back(id);
        }
    }
    
    // Установка отношений между смыслами
    void establishRelationships() {
        // Пример: вопрос -> ответ (причина-следствие)
        if (granules.count(43) && granules.count(1)) { // question -> system_identity
            granules[43]->addAssociation(1);
        }
        
        // greeting -> positive
        if (granules.count(41) && granules.count(161)) {
            granules[41]->addAssociation(161);
        }
        
        // help -> action_start
        if (granules.count(81) && granules.count(83)) {
            granules[81]->addAssociation(83);
        }
    }
    
    // Активация смыслов по входному тексту
    std::vector<float> activateFromText(const std::string& text) {
        auto words = splitText(text);
        std::vector<float> activation(NUM_SEMANTIC_GROUPS * 32, 0.0f); // 6 групп * 32 нейрона
        
        for (const auto& word : words) {
            auto it = word_to_granules.find(word);
            if (it != word_to_granules.end()) {
                for (uint32_t gid : it->second) {
                    auto& granule = granules[gid];
                    // Смысл активирует свою группу (определяем по id % 6)
                    int group_idx = (gid % NUM_SEMANTIC_GROUPS) * 32;
                    const auto& sig = granule->getSignature();
                    
                    // Активируем соответствующий паттерн
                    for (int i = 0; i < 32; i++) {
                        activation[group_idx + i] += sig[i];
                    }
                    
                    // Также активируем ассоциированные смыслы
                    for (uint32_t assoc_id : granules[gid]->getAssociations()) {
                        int assoc_group = (assoc_id % NUM_SEMANTIC_GROUPS) * 32;
                        const auto& assoc_sig = granules[assoc_id]->getSignature();
                        for (int i = 0; i < 32; i++) {
                            activation[assoc_group + i] += assoc_sig[i] * 0.5f; // слабее
                        }
                    }
                }
            }
        }
        
        // Нормализация
        for (auto& val : activation) {
            val = std::min(val, 1.0f);
        }
        
        return activation;
    }
    
    // Извлечение активных смыслов из состояния нейросети
std::vector<uint32_t> extractMeaningsFromSystem() {
    std::vector<uint32_t> active_meanings;
    std::map<uint32_t, float> activation_strength;
    
    // Получаем состояние групп 16-21
    std::vector<float> group_state(NUM_SEMANTIC_GROUPS * 32, 0.0f);
    for (int g = 0; g < NUM_SEMANTIC_GROUPS; g++) {
        const auto& group = neural_system.getGroups()[SEMANTIC_GROUPS_START + g];
        const auto& phi = group.getPhi();
        for (int n = 0; n < 32; n++) {
            group_state[g * 32 + n] = static_cast<float>(phi[n]);
        }
    }
    
    // Проверяем каждый смысл с ПОНИЖЕННЫМ порогом
    float threshold = 0.4f;  // было 0.5, снизили до 0.4
    
    for (const auto& [id, granule] : granules) {
        int group_offset = ((id % NUM_SEMANTIC_GROUPS) * 32);
        float activation = granule->getActivation(group_state, group_offset);
        
        if (activation > threshold) {
            activation_strength[id] = activation;
        }
    }
    
    // Сортируем по силе активации и берем топ-5
    std::vector<std::pair<float, uint32_t>> sorted;
    for (const auto& [id, strength] : activation_strength) {
        sorted.push_back({strength, id});
    }
    std::sort(sorted.begin(), sorted.end(), std::greater<>());
    
    for (size_t i = 0; i < std::min(size_t(5), sorted.size()); i++) {
        active_meanings.push_back(sorted[i].second);
    }
    
    return active_meanings;
}
    
    // Проецирование смыслов в нейросеть
    void projectToSystem(const std::vector<uint32_t>& meanings) {
        // Очищаем группы
        for (int g = 0; g < NUM_SEMANTIC_GROUPS; g++) {
            auto& group = neural_system.getGroups()[SEMANTIC_GROUPS_START + g];
            auto& phi = group.getPhiNonConst();
            std::fill(phi.begin(), phi.end(), 0.0);
        }
        
        // Активируем смыслы
        for (uint32_t id : meanings) {
            int group_idx = id % NUM_SEMANTIC_GROUPS;
            auto& group = neural_system.getGroups()[SEMANTIC_GROUPS_START + group_idx];
            auto& phi = group.getPhiNonConst();
            const auto& sig = granules[id]->getSignature();
            
            for (int i = 0; i < 32; i++) {
                phi[i] = std::max(phi[i], static_cast<double>(sig[i]));
            }
        }
    }
    
    // Предсказание следующего смысла (причина-следствие)
    std::vector<uint32_t> predictNextMeanings(const std::vector<uint32_t>& current) {
        std::map<uint32_t, float> candidates;
        
        for (uint32_t id : current) {
            // Смотрим ассоциации текущих смыслов
            for (uint32_t assoc : granules[id]->getAssociations()) {
                candidates[assoc] += 1.0f;
            }
            // Смотрим дочерние смыслы
            for (uint32_t child : granules[id]->getChildren()) {
                candidates[child] += 0.8f;
            }
        }

        
        // Возвращаем топ-3 кандидата
        std::vector<std::pair<float, uint32_t>> sorted;
        for (const auto& [id, strength] : candidates) {
            sorted.push_back({strength, id});
        }
        std::sort(sorted.begin(), sorted.end(), std::greater<>());
        
        std::vector<uint32_t> result;
        for (size_t i = 0; i < std::min(size_t(3), sorted.size()); i++) {
            result.push_back(sorted[i].second);
        }
        
        return result;
    }
    
    // Обучение на последовательности смыслов
    void trainOnThoughtSequence(const std::vector<uint32_t>& thought_sequence) {
        for (size_t i = 0; i < thought_sequence.size() - 1; i++) {
            uint32_t from = thought_sequence[i];
            uint32_t to = thought_sequence[i+1];
            
            // Добавляем ассоциацию, если её ещё нет
            auto& assoc = granules[from]->getAssociationsNonConst();
            if (std::find(assoc.begin(), assoc.end(), to) == assoc.end()) {
                assoc.push_back(to);
            }

            
            // Укрепляем межгрупповые связи в нейросети
            int group_from = SEMANTIC_GROUPS_START + (from % NUM_SEMANTIC_GROUPS);
            int group_to = SEMANTIC_GROUPS_START + (to % NUM_SEMANTIC_GROUPS);
            
            neural_system.strengthenInterConnection(group_from, group_to, 0.01);
        }
    }
    
    // Конвертация смыслов в текст (для ответа)
    std::string meaningsToText(const std::vector<uint32_t>& meanings) {
        if (meanings.empty()) return "I'm thinking...";
        
        std::string result;
        
        // Простейшая грамматика
        for (size_t i = 0; i < meanings.size(); i++) {
            if (i > 0) {
                // Проверяем, есть ли связь "причина-следствие"
                bool has_causal = false;
                for (uint32_t assoc : granules[meanings[i-1]]->getAssociations()) {
                    if (assoc == meanings[i]) {
                        result += " because ";
                        has_causal = true;
                        break;
                    }
                }
                if (!has_causal) {
                    result += " and ";
                }
            }
            result += granule_to_canonical[meanings[i]];
        }
        
        // Капитализация первой буквы
        if (!result.empty()) {
            result[0] = std::toupper(result[0]);
        }
        result += ".";
        
        return result;
    }
    
    // Получить ID смысла по канонической форме
    uint32_t getMeaningId(const std::string& canonical) const {
        for (const auto& [id, form] : granule_to_canonical) {
            if (form == canonical) return id;
        }
        return 0;
    }

        const SemanticGranule* getGranule(uint32_t id) const {
        auto it = granules.find(id);
        if (it != granules.end()) {
            return it->second.get();
        }
        return nullptr;
    }
};