// modules/semantic/SemanticManager.hpp
#pragma once
#include "SemanticGranule.hpp"
#include "../../core/NeuralFieldSystem.hpp"
#include "../../data/SemanticGraphDatabase.hpp" 
#include <memory>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <random>
#include <set>
#include <queue>

    // Вспомогательная функция split (была не определена)
    namespace { 
        std::vector<std::string> splitText(const std::string& text) {
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

    // ===== НОВОЕ: указатель на семантический граф =====
    SemanticGraphDatabase* semantic_graph_ = nullptr;
    
    // Для генерации ID
    uint32_t next_id = 1;

    std::map<size_t, SemanticEdge::Type> path_relations_;

    int getGroupForConcept(uint32_t id) const {
        if (!semantic_graph_) return (id % NUM_SEMANTIC_GROUPS);
        auto node = semantic_graph_->getNode(id);
        if (!node) return (id % NUM_SEMANTIC_GROUPS);
        
        // Маппинг категорий на группы (0-5 соответствуют группам 16-21)
        const std::string& cat = node->primary_category;
        
        // Группа 0 (группа 16): Физические объекты и устройства
        if (cat == "device" || cat == "sensor" || cat == "object" || 
            cat == "tool" || cat == "file" || cat == "shape" || cat == "shape_3d") {
            return 0;
        }
        
        // Группа 1 (группа 17): Действия и процессы
        if (cat == "action" || cat == "action_file" || cat == "action_control" ||
            cat == "action_modify" || cat == "function" || cat == "process" ||
            cat == "operation" || cat == "command") {
            return 1;
        }
        
        // Группа 2 (группа 18): Состояния и результаты
        if (cat == "state" || cat == "result" || cat == "property" ||
            cat == "attribute" || cat == "measurement" || cat == "quantity" ||
            cat == "duration" || cat == "frequency" || cat == "time_unit") {
            return 2;
        }
        
        // Группа 3 (группа 19): Логика и абстракции
        if (cat == "logic" || cat == "causality" || cat == "modality" ||
            cat == "abstract" || cat == "mathematics" || cat == "statistics" ||
            cat == "quantifier" || cat == "comparative" || cat == "math_operation") {
            return 3;
        }
        
        // Группа 4 (группа 20): Коммуникация и социальное
        if (cat == "communication" || cat == "social" || cat == "linguistic" ||
            cat == "pronoun" || cat == "question_word" || cat == "rhetorical" ||
            cat == "greeting" || cat == "insult" || cat == "praise") {
            return 4;
        }
        
        // Группа 5 (группа 21): Метафизика и высшие абстракции
        if (cat == "metaphysical" || cat == "value" || cat == "ethics" ||
            cat == "identity" || cat == "metacognition" || cat == "semantic_role" ||
            cat == "visualization" || cat == "affect" || cat == "behavioral" ||
            cat == "physiological" || cat == "evaluation" || cat == "security" ||
            cat == "frame" || cat == "data_structure" || cat == "pattern") {
            return 5;
        }
        
        // Категории по умолчанию
        if (cat == "general" || cat == "temporal" || cat == "space" || cat == "time" ||
            cat == "place" || cat == "weather" || cat == "color" || cat == "light") {
            return 3; // абстракции по умолчанию
        }
        
        // fallback
        return (id % NUM_SEMANTIC_GROUPS); // fallback
    }
    
public:
     // ===== НОВЫЙ КОНСТРУКТОР с графом =====
    SemanticManager(NeuralFieldSystem& ns, SemanticGraphDatabase* graph = nullptr) 
        : neural_system(ns), semantic_graph_(graph) {}

    // GET getSemanticGraph
    SemanticGraphDatabase* getSemanticGraph() const { return semantic_graph_; }
    // Добавление концепта с его словесными выражениями
    void addConcept(uint32_t id, 
                    const std::string& concept_name, 
                    const std::vector<std::string>& expressions,
                    const std::string& canonical) {
        if (granules.find(id) != granules.end()) {
            // Уже существует, не создаём заново
            return;
        }
        
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

    // Метод для получения отношения между элементами пути
    SemanticEdge::Type getPathRelation(size_t index) const {
        auto it = path_relations_.find(index);
        return (it != path_relations_.end()) ? it->second : SemanticEdge::Type::RELATED_TO;
    }
    
    // Очистка кэша отношений пути
    void clearPathRelations() { path_relations_.clear(); }
    
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

    bool hasGranule(uint32_t id) const {
        return granules.find(id) != granules.end();
    }

    int getGranuleCount() const {
        return granules.size();
    }

    void promoteMeaning(uint32_t meaning_id, float strength = 0.1f) {
    auto it = granules.find(meaning_id);
    if (it != granules.end()) {
        // Увеличиваем важность смысла
        // Можно увеличить вес в графе или повысить орбиту
        if (semantic_graph_) {
            auto node = semantic_graph_->getNode(meaning_id);
            if (node) {
                // Повышаем базовую важность
                const_cast<SemanticNode*>(node)->base_importance = 
                    std::min(1.0f, node->base_importance + strength);
            }
        }
        
        // Также можно повысить орбиту в нейросети
        int group_idx = getGroupForConcept(meaning_id);
        if (group_idx >= 0 && group_idx < 6) {
            // Находим нейроны, которые представляют этот смысл
            auto& group = neural_system.getGroupsNonConst()[SEMANTIC_GROUPS_START + group_idx];
            // Ищем нейрон с максимальной активацией
            const auto& sig = it->second->getSignature();
            float max_activation = 0;
            int best_neuron = 0;
            for (int i = 0; i < 32; i++) {
                if (sig[i] > max_activation) {
                    max_activation = sig[i];
                    best_neuron = i;
                }
            }
            // Повышаем орбиту этого нейрона
            if (group.getOrbitLevel(best_neuron) < 4) {
                group.publicPromoteToBaseOrbit(best_neuron);
                std::cout << "  Promoted neuron " << best_neuron << " for meaning " 
                          << meaning_id << std::endl;
            }
        }
    }
}

  // ===== ИСПРАВЛЕННЫЙ МЕТОД =====

std::vector<uint32_t> extractMeaningsWithOrbitPriority() {
    std::vector<std::pair<uint32_t, float>> candidates;
    
    // Получаем состояние групп 16-21
    std::vector<float> group_state(NUM_SEMANTIC_GROUPS * 32, 0.0f);
    for (int g = 0; g < NUM_SEMANTIC_GROUPS; g++) {
        const auto& group = neural_system.getGroups()[SEMANTIC_GROUPS_START + g];
        const auto& phi = group.getPhi();
        
        for (int n = 0; n < 32; n++) {
            int orbit_level = group.getOrbitLevel(n);
            float orbit_weight = 1.0f;
            
            if (orbit_level >= 3) orbit_weight = 2.0f;
            else if (orbit_level == 0) orbit_weight = 0.1f;
            
            group_state[g * 32 + n] = static_cast<float>(phi[n]) * orbit_weight;
        }
    }
    
    // Оцениваем каждый смысл
    float threshold = 0.5f;
    
    for (const auto& [id, granule] : granules) {
        int group_offset = ((getGroupForConcept(id)) * 32);
        float activation = granule->getActivation(group_state, group_offset);
        
        if (activation > threshold) {
            candidates.push_back({id, activation});
        }
    }
    
    // Сортируем по активации
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // ===== ИСПРАВЛЕНО: проверка связей через граф =====
    std::vector<uint32_t> result;
    std::set<uint32_t> selected;
    
    for (const auto& [id, strength] : candidates) {
        if (selected.size() >= 5) break;
        
        bool is_related = false;
        if (!selected.empty() && semantic_graph_) {
            for (uint32_t selected_id : selected) {
                // Проверяем связь через граф
                auto edges = semantic_graph_->getEdgesFrom(selected_id);
                for (const auto& edge : edges) {
                    if (edge.to_id == id) {
                        is_related = true;
                        break;
                    }
                }
                if (is_related) break;
            }
        } else if (selected.empty()) {
            is_related = true;  // первый смысл всегда берём
        }
        
        if (is_related) {
            result.push_back(id);
            selected.insert(id);
        }
    }
    
    // Если не нашли связанных смыслов, берём топ-3
    if (result.empty() && !candidates.empty()) {
        for (size_t i = 0; i < std::min(size_t(3), candidates.size()); i++) {
            result.push_back(candidates[i].first);
        }
    }
    
    return result;
}

/**
 * @brief Извлекает упорядоченный путь смыслов (линейную мысль)
 * @param max_length Максимальная длина пути (по умолчанию 5)
 * @return Упорядоченный вектор ID смыслов
 */
std::vector<uint32_t> extractMeaningPath(int max_length = 5) {
    if (!semantic_graph_) {
        return extractMeaningsWithOrbitPriority(); // fallback
    }
    
    // Шаг 1: Получаем все кандидаты с их активацией и орбитальным весом
    std::vector<std::pair<uint32_t, float>> candidates;
    
    std::vector<float> group_state(NUM_SEMANTIC_GROUPS * 32, 0.0f);
    for (int g = 0; g < NUM_SEMANTIC_GROUPS; g++) {
        const auto& group = neural_system.getGroups()[SEMANTIC_GROUPS_START + g];
        const auto& phi = group.getPhi();
        
        for (int n = 0; n < 32; n++) {
            int orbit_level = group.getOrbitLevel(n);
            float orbit_weight = 1.0f;
            if (orbit_level >= 3) orbit_weight = 2.0f;
            else if (orbit_level == 0) orbit_weight = 0.1f;
            
            group_state[g * 32 + n] = static_cast<float>(phi[n]) * orbit_weight;
        }
    }
    
    for (const auto& [id, granule] : granules) {
        int group_offset = getGroupForConcept(id) * 32;
        float activation = granule->getActivation(group_state, group_offset);
        
        // Добавляем бонус за важность концепта из графа
        auto node = semantic_graph_->getNode(id);
        float importance_bonus = node ? node->base_importance : 0.5f;
        float score = activation * (0.7f + 0.3f * importance_bonus);
        
        if (score > 0.3f) {  // порог для рассмотрения
            candidates.push_back({id, score});
        }
    }
    
    if (candidates.empty()) {
        return extractMeaningsWithOrbitPriority();
    }
    
    // Шаг 2: Выбираем центр (самый сильный кандидат)
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    std::vector<uint32_t> path;
    std::set<uint32_t> used;
    
    uint32_t center = candidates[0].first;
    path.push_back(center);
    used.insert(center);
    
    // Шаг 3: Жадный обход графа для построения пути
    for (int step = 1; step < max_length && !candidates.empty(); step++) {
        uint32_t last = path.back();
        float best_score = -1.0f;
        uint32_t best_id = 0;
        SemanticEdge::Type best_relation = SemanticEdge::Type::RELATED_TO;
        
        // Сначала ищем среди кандидатов, которые имеют связь с последним
        for (const auto& [cand_id, base_score] : candidates) {
            if (used.count(cand_id)) continue;
            
            // Проверяем наличие связи в графе
            auto edges = semantic_graph_->getEdgesFrom(last);
            float relation_strength = 0.0f;
            SemanticEdge::Type relation_type = SemanticEdge::Type::RELATED_TO;
            
            for (const auto& edge : edges) {
                if (edge.to_id == cand_id) {
                    relation_strength = edge.weight * edge.trust_level;
                    relation_type = edge.type;
                    break;
                }
            }
            
            // Также проверяем обратную связь
            if (relation_strength == 0.0f) {
                auto rev_edges = semantic_graph_->getEdgesFrom(cand_id);
                for (const auto& edge : rev_edges) {
                    if (edge.to_id == last) {
                        relation_strength = edge.weight * edge.trust_level * 0.8f; // обратная связь слабее
                        relation_type = edge.type;
                        break;
                    }
                }
            }
            
            // Проверяем ассоциации в SemanticGranule
            auto granule_it = granules.find(cand_id);
            bool has_association = false;
            if (granule_it != granules.end()) {
                for (uint32_t assoc : granule_it->second->getAssociations()) {
                    if (assoc == last) {
                        has_association = true;
                        break;
                    }
                }
            }
            
            // Если нет никакой связи, пропускаем
            if (relation_strength == 0.0f && !has_association) {
                continue;
            }
            
            // Вычисляем итоговый скор
            float connection_bonus = relation_strength > 0 ? relation_strength : 0.5f;
            if (has_association) connection_bonus = std::max(connection_bonus, 0.6f);
            
            // Тип связи влияет на скор
            float type_multiplier = 1.0f;
            switch (relation_type) {
                case SemanticEdge::Type::CAUSES:
                case SemanticEdge::Type::LEADS_TO:
                case SemanticEdge::Type::THEREFORE:
                    type_multiplier = 1.5f;  // причинно-следственные связи важнее
                    break;
                case SemanticEdge::Type::IS_A:
                case SemanticEdge::Type::IS_A_KIND_OF:
                    type_multiplier = 1.3f;
                    break;
                case SemanticEdge::Type::BEFORE:
                case SemanticEdge::Type::AFTER:
                    type_multiplier = 1.2f;
                    break;
                case SemanticEdge::Type::SIMILAR_TO:
                    type_multiplier = 1.0f;
                    break;
                default:
                    type_multiplier = 0.9f;
                    break;
            }
            
            float total_score = base_score * connection_bonus * type_multiplier;
            
            if (total_score > best_score) {
                best_score = total_score;
                best_id = cand_id;
                best_relation = relation_type;
            }
        }
        
        // Если нашли связанный смысл, добавляем в путь
        if (best_id != 0) {
            path.push_back(best_id);
            used.insert(best_id);
            
            // Сохраняем отношение для последующей генерации текста
            // (можно сохранить в отдельный мап)
            path_relations_[path.size() - 1] = best_relation;
        } else {
            // Если не нашли связанных, берем самый активированный из оставшихся
            for (const auto& [cand_id, base_score] : candidates) {
                if (!used.count(cand_id)) {
                    path.push_back(cand_id);
                    used.insert(cand_id);
                    break;
                }
            }
        }
    }
    
    return path;
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
    
    // Получаем состояние групп 16-21 (только для чтения)
    std::vector<float> group_state(NUM_SEMANTIC_GROUPS * 32, 0.0f);
    for (int g = 0; g < NUM_SEMANTIC_GROUPS; g++) {
        const auto& group = neural_system.getGroups()[SEMANTIC_GROUPS_START + g];  // const
        const auto& phi = group.getPhi();  // const
        for (int n = 0; n < 32; n++) {
            group_state[g * 32 + n] = static_cast<float>(phi[n]);
        }
    }
    
    // Проверяем каждый смысл
    float threshold = 0.4f;
    
    for (const auto& [id, granule] : granules) {
        int group_offset = (getGroupForConcept(id) * 32);
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
    
    // Проецирование смыслов в нейросеть (только одна копия!)
    void projectToSystem(const std::vector<uint32_t>& meanings) {
        // Очищаем группы
        for (int g = 0; g < NUM_SEMANTIC_GROUPS; g++) {
            auto& group = neural_system.getGroupsNonConst()[SEMANTIC_GROUPS_START + g];
            auto& phi = group.getPhiNonConst();
            std::fill(phi.begin(), phi.end(), 0.0);
        }
        
        // Активируем смыслы
        for (uint32_t id : meanings) {
            int group_idx = getGroupForConcept(id);  // ← ВАЖНО: используем group_idx
            auto& group = neural_system.getGroupsNonConst()[SEMANTIC_GROUPS_START + group_idx];
            auto& phi = group.getPhiNonConst();
            
            // Проверяем существование гранулы
            auto it = granules.find(id);
            if (it == granules.end()) {
                std::cerr << "ERROR: Granule not found for id " << id << std::endl;
                continue;
            }
            
            const auto& sig = it->second->getSignature();
            
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
            int group_from = SEMANTIC_GROUPS_START + getGroupForConcept(from);
            int group_to = SEMANTIC_GROUPS_START + getGroupForConcept(to);

            
            neural_system.strengthenInterConnection(group_from, group_to, 0.01);
        }
    }
    

/**
 * @brief Конвертирует путь смыслов в текст, используя типы связей из графа
 * @param meanings Вектор ID смыслов (ожидается упорядоченный путь)
 * @return Строковое представление мысли
 */
std::string meaningsToText(const std::vector<uint32_t>& meanings) {
    if (meanings.empty()) return "I'm thinking...";
    
    std::string result;
    
    for (size_t i = 0; i < meanings.size(); i++) {
        uint32_t id = meanings[i];
        
        // Получаем каноническую форму
        auto it = granule_to_canonical.find(id);
        std::string word = (it != granule_to_canonical.end()) ? it->second : "something";
        
        // Для первого элемента просто добавляем слово
        if (i == 0) {
            result += word;
            continue;
        }
        
        // Для последующих элементов добавляем связку
        uint32_t prev_id = meanings[i-1];
        
        // Пытаемся найти отношение в графе
        std::string relation_word = "and";
        bool found_relation = false;
        
        if (semantic_graph_) {
            // Ищем прямую связь
            auto edges = semantic_graph_->getEdgesFrom(prev_id);
            for (const auto& edge : edges) {
                if (edge.to_id == id) {
                    relation_word = SemanticEdge::typeToString(edge.type);
                    found_relation = true;
                    break;
                }
            }
            
            // Если не нашли, ищем обратную связь
            if (!found_relation) {
                auto rev_edges = semantic_graph_->getEdgesFrom(id);
                for (const auto& edge : rev_edges) {
                    if (edge.to_id == prev_id) {
                        relation_word = std::string(SemanticEdge::typeToString(edge.type)) + " (reverse)";
                        found_relation = true;
                        break;
                    }
                }
            }
        }
        
        // Если не нашли в графе, проверяем ассоциации в SemanticGranule
        if (!found_relation) {
            auto granule_it = granules.find(prev_id);
            if (granule_it != granules.end()) {
                for (uint32_t assoc : granule_it->second->getAssociations()) {
                    if (assoc == id) {
                        relation_word = "because";
                        found_relation = true;
                        break;
                    }
                }
            }
        }
        
        // Форматируем связку
        if (relation_word == "because" || relation_word == "therefore" || 
            relation_word == "so" || relation_word == "thus") {
            result += " " + relation_word + " " + word;
        } else if (relation_word == "before") {
            result += " then " + word;
        } else if (relation_word == "after") {
            result += " after " + word;
        } else if (relation_word == "causes" || relation_word == "leads_to") {
            result += " causes " + word;
        } else {
            result += " " + relation_word + " " + word;
        }
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