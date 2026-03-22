// data/SemanticGraphDatabase.hpp
#pragma once
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <memory>
#include <fstream>
#include <sstream>
#include <iostream>
#include <random>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <algorithm>
#include "MeaningTrainingExample_fwd.hpp"

class EffectiveLearning; 

// Перечисление для эмоциональной окраски
enum class EmotionalTone {
    VERY_POSITIVE = 2,
    POSITIVE = 1,
    NEUTRAL = 0,
    NEGATIVE = -1,
    VERY_NEGATIVE = -2
};

inline std::string emotionalToneToString(EmotionalTone tone) {
    switch(tone) {
        case EmotionalTone::VERY_POSITIVE: return "very_positive";
        case EmotionalTone::POSITIVE: return "positive";
        case EmotionalTone::NEUTRAL: return "neutral";
        case EmotionalTone::NEGATIVE: return "negative";
        case EmotionalTone::VERY_NEGATIVE: return "very_negative";
        default: return "unknown";
    }
}

/**
 * @struct SemanticNode
 * @brief Узел семантического графа (смысл)
 */
struct SemanticNode {
    uint32_t id;                          // уникальный ID
    std::string name;                      // название смысла (например "greeting")
    std::string canonical_form;            // основная текстовая форма ("hello")
    std::vector<std::string> aliases;      // альтернативные слова ("hi", "hey")
    
    // Категории для организации
    std::string primary_category;           // основная категория
    std::vector<std::string> tags;          // дополнительные теги
    
    // Важность и сложность
    float base_importance = 0.5f;           // базовая важность (0-1)
    float complexity = 0.5f;                 // сложность для обучения (0-1)
    
    // НОВЫЕ ПАРАМЕТРЫ
    int abstraction_level = 5;               // уровень абстракции (1-10): 1=конкретное, 10=абстрактное
    EmotionalTone emotional_tone = EmotionalTone::NEUTRAL; // эмоциональная окраска
    
    // Контексты использования (в каких ситуациях применимо)
    std::vector<std::string> usage_contexts; // например: "conversation", "command", "query", "system"
    
    // Фреймовая структура (для сложных концептов)
    std::map<std::string, uint32_t> frame_roles; // роль -> id узла
    
    // Вектор для нейросети (32 мерный, будет заполнен позже)
    std::vector<float> signature;            // 32-мерный вектор
    
    // Конструктор с инициализацией
    SemanticNode(uint32_t _id, const std::string& _name, const std::string& _canonical)
        : id(_id), name(_name), canonical_form(_canonical), creation_step(0) {
        signature.resize(32, 0.0f);
        last_used_step = 0;
        usage_count = 0;
        importance_decay = 1.0f;
        is_core = false;
    }
    
    // Получить эмоциональную окраску как float для нейросети
    float getEmotionalValue() const {
        return static_cast<float>(emotional_tone) / 2.0f; // от -1.0 до 1.0
    }

        // ===== НОВЫЕ ПОЛЯ ДЛЯ СТАРЕНИЯ =====
    int last_used_step = 0;           // последний шаг, когда концепт использовался
    int usage_count = 0;              // сколько раз использовался
    float importance_decay = 1.0f;    // множитель важности (уменьшается со временем)
    bool is_core = false;             // флаг "ядерного" концепта (никогда не удаляется)
    int creation_step = 0;            // шаг создания
};

/**
 * @struct SemanticEdge
 * @brief Ребро графа (связь между смыслами)
 */
struct SemanticEdge {
    uint32_t from_id;
    uint32_t to_id;
    
enum class Type {
    // Базовые отношения (0-9)
    IS_A = 0,
    HAS_PART = 1,
    CAN_DO = 2,
    CAUSES = 3,
    REQUIRES = 4,
    OPPOSITE_OF = 5,
    SIMILAR_TO = 6,
    PART_OF = 7,
    USED_FOR = 8,
    AFTER = 9,
    
    // Семантические роли (10-20)
    AGENT = 10,
    PATIENT = 11,
    INSTRUMENT = 12,
    LOCATION = 13,
    SOURCE = 14,
    DESTINATION = 15,
    TIME = 16,
    PURPOSE = 17,
    RESULT = 18,
    BENEFICIARY = 19,
    OBSERVER = 20,
    
    // Логические операторы (21-32)
    IF = 21,
    THEN = 22,
    BECAUSE = 23,
    THEREFORE = 24,
    WHILE = 25,
    UNLESS = 26,
    DESPITE = 27,
    MAYBE = 28,
    ALWAYS = 29,
    NEVER = 30,
    POSSIBLE = 31,
    NECESSARY = 32,
    
    // Мета-операции мышления (33-44)
    KNOW = 33,
    BELIEVE = 34,
    PREDICT = 35,
    OBSERVE = 36,
    ASSUME = 37,
    VERIFY = 38,
    LEARN = 39,
    REMEMBER = 40,
    PLAN = 41,
    DECIDE = 42,
    INTEND = 43,
    
    // Пространство и время (44-58)
    INSIDE = 44,
    OUTSIDE = 45,
    NEAR = 46,
    FAR = 47,
    ABOVE = 48,
    BELOW = 49,
    BEFORE = 50,
    DURING = 51,
    START = 52,
    END = 53,
    DURATION = 54,
    DISTANCE = 55,
    SPEED = 56,
    DIRECTION = 57,
    
    // Конституция (58-68)
    THREATENS = 58,
    MUST_ENSURE = 59,
    MAY_REQUIRE = 60,
    SERVES = 61,
    MUST_CONTAIN = 62,
    PROTECTS = 63,
    VIOLATES = 64,
    AUTHORIZES = 65,
    OVERRIDES = 66,
    CONFLICTS_WITH = 67,
    
    // Существующие (68-72)
    ACCESS_LEVEL = 68,
    CONFIDENCE = 69,
    EMOTIONAL_LINK = 70,
    CONTEXT_MATCH = 71,

    // Визуализация и аффект (72-87)
    VISUALIZES = 72,
    REPRESENTS = 73,
    HAS_AXIS = 74,
    HAS_SCALE = 75,
    HAS_DIMENSION = 76,
    ENCODES = 77,
    DECODES_FROM = 78,
    EXPRESSES = 79,
    DETECTS = 80,
    CORRELATES_WITH = 81,      // <-- ОДНО ОПРЕДЕЛЕНИЕ
    MODULATES = 82,
    PREDICTS_AFFECT = 83,
    MEASURES = 84,
    ATTENUATES = 85,
    AMPLIFIES = 86,

    // Математические операции (87-96)
    COMPUTES = 87,
    APPROXIMATES = 88,
    INTEGRATES = 89,
    DIFFERENTIATES = 90,
    EXTRACTS_FEATURE = 91,
    NORMALIZES = 92,
    TRANSFORMS = 93,
    CLUSTERS = 94,
    CLASSIFIES = 95,

    // Отношения творца (96-105)
    CREATED_BY = 96,
    HOSTS = 97,
    REFERS_TO = 98,
    DETERMINES = 99,
    DEPENDS_ON = 100,
    CONTRIBUTES_TO = 101,
    GIVES_MEANING_TO = 102,
    FOLLOWS_FROM = 103,
    LEADS_TO = 104,

    // Файловые операции (105-114)
    CONTAINS = 105,
    AFFECTS = 106,
    CREATES = 107,
    DESTROYS = 108,
    MODIFIES = 109,
    ACCESSES = 110,
    ENDS_ACCESS = 111,
    PRESERVES = 112,
    
    // Состояния (113-116)
    RESULT_OF = 113,
    CURRENT_STATE = 114,
    CHANGES_TO = 115,
    
    // Свойства (116-119)
    HAS_PROPERTY = 116,
    HAS_VALUE = 117,
    HAS_INTENSITY = 118,
    
    // Оценка (119-123)
    IS_BETTER_THAN = 119,
    IS_WORSE_THAN = 120,
    IS_CORRECT_FOR = 121,
    IS_INCORRECT_FOR = 122,
    
    // Методы (123-124)
    METHOD_OF = 123,
    
    // Связанность (124-125) - ОСТАВЛЯЕМ ОДНО
    RELATED_TO = 124,           // <-- ОДНО ОПРЕДЕЛЕНИЕ
    
    // Иерархия (125-129)
    IS_A_KIND_OF = 125,
    IS_A_PART_OF = 126,
    MAY_BE = 127,
    MAY_CONTAIN = 128,

    // Пространственные отношения (129-134)
    POINTS_TO = 129,
    LOCATES = 130,
    USES = 131,
    RELATES_TO = 132,
    
    // Физические отношения (133-139)
    ACCELERATES = 133,
    DECELERATES = 134,
    ORBITS = 135,
    ROTATES = 136,
    ATTRACTS = 137,
    REPELS = 138,

    // Исследование и научный метод (139-151)
    RESPONDS_TO = 139,
    PRODUCES = 140,
    SUPPORTS = 141,
    VERIFIES = 142,
    REFUTES = 143,
    PREDICTS = 144,
    EXPLAINS = 145,
    GENERALIZES = 146,
    SPECIALIZES = 147,
    
    // Кибернетика и обратная связь (148-155)
    FEEDS_BACK_TO = 148,
    REGULATES = 149,
    STABILIZES = 150,
    DESTABILIZES = 151,
    DAMPENS = 152,
    
    // Логика и математика (153-159)
    IMPLIES = 153,
    EQUIVALENT_TO = 154,
    SATISFIES = 155,
    CONTRADICTS = 156,
    ENTRAILS = 157,
    
    // Причинность (158-163)
    ENABLES = 158,
    PREVENTS = 159,
    TRIGGERS = 160,
    INHIBITS = 161,
    CATALYZES = 162,
    
    // Иерархия и классификация (163-169)
    BELONGS_TO = 163,           // <-- ОДНО ОПРЕДЕЛЕНИЕ
    CLASSIFIED_AS = 164,
    EXEMPLIFIES = 165,
    INSTANTIATES = 166,
    SUBCLASS_OF = 167,
    
    // Эволюция и развитие (168-173)
    EVOLVES_INTO = 168,
    DERIVED_FROM = 169,
    PRECEDES = 170,
    SUCCEEDS = 171,
    COEVOLVES_WITH = 172,

    // НОВЫЕ ТИПЫ СВЯЗЕЙ ДЛЯ СТРУКТУР ДАННЫХ И ПАТТЕРНОВ (173-182)
    PERFORMS = 173,
    MAY_ACCESS = 174,
    ITERATES = 176,
    RECURSES = 177,
    PROCESSES = 178,
    CONVERGES_TO = 179,
    DIVERGES_FROM = 180,
    ENCAPSULATES = 181,
    ABSTRACTION_OF = 182,
    CONCRETIZATION_OF = 183,
    PARALLEL_TO = 184,
    SEQUENTIAL_TO = 185,
    INTERRUPTS = 186,

    // Отношения собственности и идентичности (187-195)
    // 187-190 резерв
    OWNS = 191,
    LIVES_IN = 192,
    IS_FROM = 193,
    PARENT_OF = 194,
    CHILD_OF = 195,
    SIBLING_OF = 196,
    MARRIED_TO = 197,
    HAS_PRONOUN = 198,
    REINFORCES = 199,  // для усиления существующих связей

} type;

    float weight = 1.0f;                    // сила связи (0-1)
    float learning_progress = 0.0f;          // прогресс обучения (0-1)
    
    // НОВЫЕ ПАРАМЕТРЫ ДЛЯ СВЯЗЕЙ
    float trust_level = 1.0f;                // доверие к связи (0-1)
    int confidence = 100;                    // уверенность в связи (0-100)
    std::vector<std::string> valid_contexts; // в каких контекстах связь применима
    
    SemanticEdge(uint32_t from, uint32_t to, Type t, float w = 1.0f)
        : from_id(from), to_id(to), type(t), weight(w) {}
    
    static const char* typeToString(Type t) {
        switch(t) {
            // Базовые
            case Type::IS_A: return "is_a";
            case Type::HAS_PART: return "has_part";
            case Type::CAN_DO: return "can_do";
            case Type::CAUSES: return "causes";
            case Type::REQUIRES: return "requires";
            case Type::OPPOSITE_OF: return "opposite_of";
            case Type::SIMILAR_TO: return "similar_to";
            case Type::PART_OF: return "part_of";
            case Type::USED_FOR: return "used_for";
            case Type::AFTER: return "after";
            
            // Семантические роли
            case Type::AGENT: return "agent";
            case Type::PATIENT: return "patient";
            case Type::INSTRUMENT: return "instrument";
            case Type::LOCATION: return "location";
            case Type::SOURCE: return "source";
            case Type::DESTINATION: return "destination";
            case Type::TIME: return "time";
            case Type::PURPOSE: return "purpose";
            case Type::RESULT: return "result";
            case Type::BENEFICIARY: return "beneficiary";
            case Type::OBSERVER: return "observer";
            
            // Логические
            case Type::IF: return "if";
            case Type::THEN: return "then";
            case Type::BECAUSE: return "because";
            case Type::THEREFORE: return "therefore";
            case Type::WHILE: return "while";
            case Type::UNLESS: return "unless";
            case Type::DESPITE: return "despite";
            case Type::MAYBE: return "maybe";
            case Type::ALWAYS: return "always";
            case Type::NEVER: return "never";
            case Type::POSSIBLE: return "possible";
            case Type::NECESSARY: return "necessary";
            
            // Мета-мышление
            case Type::KNOW: return "know";
            case Type::BELIEVE: return "believe";
            case Type::PREDICT: return "predict";
            case Type::OBSERVE: return "observe";
            case Type::ASSUME: return "assume";
            case Type::VERIFY: return "verify";
            case Type::LEARN: return "learn";
            case Type::REMEMBER: return "remember";
            case Type::PLAN: return "plan";
            case Type::DECIDE: return "decide";
            case Type::INTEND: return "intend";
            
            // Пространство-время
            case Type::INSIDE: return "inside";
            case Type::OUTSIDE: return "outside";
            case Type::NEAR: return "near";
            case Type::FAR: return "far";
            case Type::ABOVE: return "above";
            case Type::BELOW: return "below";
            case Type::BEFORE: return "before";
            case Type::DURING: return "during";
            case Type::START: return "start";
            case Type::END: return "end";
            case Type::DURATION: return "duration";
            case Type::DISTANCE: return "distance";
            case Type::SPEED: return "speed";
            case Type::DIRECTION: return "direction";
            
            // Конституция
            case Type::THREATENS: return "threatens";
            case Type::MUST_ENSURE: return "must_ensure";
            case Type::MAY_REQUIRE: return "may_require";
            case Type::SERVES: return "serves";
            case Type::MUST_CONTAIN: return "must_contain";
            case Type::PROTECTS: return "protects";
            case Type::VIOLATES: return "violates";
            case Type::AUTHORIZES: return "authorizes";
            case Type::OVERRIDES: return "overrides";
            case Type::CONFLICTS_WITH: return "conflicts_with";
            
            // Существующие
            case Type::ACCESS_LEVEL: return "access_level";
            case Type::CONFIDENCE: return "confidence";
            case Type::EMOTIONAL_LINK: return "emotional_link";
            case Type::CONTEXT_MATCH: return "context_match";

            // visualise affective signals
            case Type::VISUALIZES: return "visualizes";
            case Type::REPRESENTS: return "represents";
            case Type::HAS_AXIS: return "has_axis";
            case Type::HAS_SCALE: return "has_scale";
            case Type::HAS_DIMENSION: return "has_dimension";
            case Type::ENCODES: return "encodes";
            case Type::DECODES_FROM: return "decodes_from";
            case Type::EXPRESSES: return "expresses";
            case Type::DETECTS: return "detects";
            case Type::CORRELATES_WITH: return "correlates_with";
            case Type::MODULATES: return "modulates";
            case Type::PREDICTS_AFFECT: return "predicts_affect";
            case Type::MEASURES: return "measures";
            case Type::ATTENUATES: return "attenuates";
            case Type::AMPLIFIES: return "amplifies";
            case Type::COMPUTES: return "computes";
            case Type::APPROXIMATES: return "approximates";
            case Type::INTEGRATES: return "integrates";
            case Type::DIFFERENTIATES: return "differentiates";
            case Type::EXTRACTS_FEATURE: return "extracts_feature";
            case Type::NORMALIZES: return "normalizes";
            case Type::TRANSFORMS: return "transforms";
            case Type::CLUSTERS: return "clusters";
            case Type::CLASSIFIES: return "classifies";

            case Type::CREATED_BY: return "created_by";
            case Type::HOSTS: return "hosts";
            case Type::REFERS_TO: return "refers_to";
            case Type::DETERMINES: return "determines";
            case Type::DEPENDS_ON: return "depends_on";
            case Type::CONTRIBUTES_TO: return "contributes_to";
            case Type::GIVES_MEANING_TO: return "gives_meaning_to";
            case Type::FOLLOWS_FROM: return "follows_from";
            case Type::LEADS_TO: return "leads_to";

            // Для файлов
            case Type::CONTAINS: return "contains";
            case Type::AFFECTS: return "affects";
            case Type::CREATES: return "creates";
            case Type::DESTROYS: return "destroys";
            case Type::MODIFIES: return "modifies";
            case Type::ACCESSES: return "accesses";
            case Type::ENDS_ACCESS: return "ends_access";
            case Type::PRESERVES: return "preserves";
            
            // Для состояний
            case Type::RESULT_OF: return "result_of";
            case Type::CURRENT_STATE: return "current_state";
            case Type::CHANGES_TO: return "changes_to";
            
            // Для свойств
            case Type::HAS_PROPERTY: return "has_property";
            case Type::HAS_VALUE: return "has_value";
            case Type::HAS_INTENSITY: return "has_intensity";
            
            // Для оценки
            case Type::IS_BETTER_THAN: return "is_better_than";
            case Type::IS_WORSE_THAN: return "is_worse_than";
            case Type::IS_CORRECT_FOR: return "is_correct_for";
            case Type::IS_INCORRECT_FOR: return "is_incorrect_for";
            
            // Для методов
            case Type::METHOD_OF: return "method_of";
            
            // Для связанности
            case Type::RELATED_TO: return "related_to";
            
            // Для иерархии
            case Type::IS_A_KIND_OF: return "is_a_kind_of";
            case Type::IS_A_PART_OF: return "is_a_part_of";
            case Type::MAY_BE: return "may_be";// MAY_CONTAIN

            case Type::MAY_CONTAIN: return "may_contain";

            // Добавьте в соответствующие места в switch:
            case Type::POINTS_TO: return "points_to";
            case Type::LOCATES: return "locates";
            case Type::USES: return "uses";
            case Type::RELATES_TO: return "relates_to";
            case Type::ACCELERATES: return "accelerates";
            case Type::DECELERATES: return "decelerates";
            case Type::ORBITS: return "orbits";
            case Type::ROTATES: return "rotates";
            case Type::ATTRACTS: return "attracts";
            case Type::REPELS: return "repels";

            case Type::RESPONDS_TO: return "responds_to";
            case Type::PRODUCES: return "produces";
            case Type::SUPPORTS: return "supports";
            case Type::VERIFIES: return "verifies";
            case Type::REFUTES: return "refutes";
            case Type::PREDICTS: return "predicts";
            case Type::EXPLAINS: return "explains";
            case Type::GENERALIZES: return "generalizes";
            case Type::SPECIALIZES: return "specializes";
            case Type::FEEDS_BACK_TO: return "feeds_back_to";
            case Type::REGULATES: return "regulates";
            case Type::STABILIZES: return "stabilizes";
            case Type::DESTABILIZES: return "destabilizes";
            case Type::DAMPENS: return "dampens";
            case Type::IMPLIES: return "implies";
            case Type::EQUIVALENT_TO: return "equivalent_to";
            case Type::SATISFIES: return "satisfies";
            case Type::CONTRADICTS: return "contradicts";
            case Type::ENTRAILS: return "entrails";
            case Type::ENABLES: return "enables";
            case Type::PREVENTS: return "prevents";
            case Type::TRIGGERS: return "triggers";
            case Type::INHIBITS: return "inhibits";
            case Type::CATALYZES: return "catalyzes";
            case Type::BELONGS_TO: return "belongs_to";
            case Type::CLASSIFIED_AS: return "classified_as";
            case Type::EXEMPLIFIES: return "exemplifies";
            case Type::INSTANTIATES: return "instantiates";
            case Type::SUBCLASS_OF: return "subclass_of";
            case Type::EVOLVES_INTO: return "evolves_into";
            case Type::DERIVED_FROM: return "derived_from";
            case Type::PRECEDES: return "precedes";
            case Type::SUCCEEDS: return "succeeds";
            case Type::COEVOLVES_WITH: return "coevolves_with";

            case Type::PERFORMS: return "performs";
            case Type::MAY_ACCESS: return "may_access";
            case Type::ITERATES: return "iterates";
            case Type::RECURSES: return "recurses";
            case Type::PROCESSES: return "processes";

            case Type::CONVERGES_TO: return "converges_to";
            case Type::DIVERGES_FROM: return "diverges_from";
            case Type::ENCAPSULATES: return "encapsulates";
            case Type::ABSTRACTION_OF: return "abstraction_of";
            case Type::CONCRETIZATION_OF: return "concretization_of";
            case Type::PARALLEL_TO: return "parallel_to";
            case Type::SEQUENTIAL_TO: return "sequential_to";
            case Type::INTERRUPTS: return "interrupts";

            //case Type::BELONGS_TO: return "belongs_to";
            case Type::OWNS: return "owns";
            case Type::LIVES_IN: return "lives_in";
            case Type::IS_FROM: return "is_from";
            case Type::PARENT_OF: return "parent_of";
            case Type::CHILD_OF: return "child_of";
            case Type::SIBLING_OF: return "sibling_of";
            case Type::MARRIED_TO: return "married_to";
            case Type::HAS_PRONOUN: return "has_pronoun";

            case Type::REINFORCES: return "reinforces";

            default: return "unknown";
        }
    }
};

/**
 * @class SemanticGraphDatabase
 * @brief Хранилище семантического графа с высокоабстрактными концептами
 * 
 * Добавлены:
 * - Семантические роли (agent, patient, instrument...)
 * - Логические операторы (if, then, because...)
 * - Мета-операции мышления (know, believe, predict...)
 * - Пространственно-временные отношения
 * - Фреймовая структура для сложных концептов
 */
class SemanticGraphDatabase {
private:
    // Узлы графа (id -> узел)
    std::unordered_map<uint32_t, std::unique_ptr<SemanticNode>> nodes;
    
    // Индексы для быстрого поиска
    std::unordered_map<std::string, uint32_t> name_to_id;           // имя -> id
    std::unordered_map<std::string, std::vector<uint32_t>> word_to_ids;  // слово -> список id
    
    // Ребра графа (from_id -> список ребер)
    std::unordered_map<uint32_t, std::vector<SemanticEdge>> edges_from;
    
    // Обратные ребра для быстрого доступа
    std::unordered_map<uint32_t, std::vector<uint32_t>> edges_to;
    
    // Кэш для быстрого вывода
    mutable std::unordered_map<uint32_t, std::string> explanation_cache;
    
    // Статистика
    uint32_t next_id = 1;
    std::string dump_path = "dump/semantic_graph/";
    
    // Для генерации сигнатур
    std::mt19937 rng{std::random_device{}()};
    
public:
    SemanticGraphDatabase() {
        std::filesystem::create_directories(dump_path);
        buildBaseGraph();
        buildAbstractConceptLayer();
        buildVisualizationAndAffectLayer();
        buildRelationships();
        buildAllRelationships();
        buildTemporalConceptNetwork();
        buildSpatialConceptNetwork(); 
        buildMathematicalFoundationLayer();
        buildDataStructuresAndPatternsLayer();
        buildOwnershipAndIdentityLayer();
        buildBasicVocabularyLayer();
        std::cout << "SemanticGraphDatabase initialized with " << nodes.size() 
                  << " nodes and " << countEdges() << " edges" << std::endl;
    }

    void markCoreConcepts();

    ~SemanticGraphDatabase() {
        // unique_ptr автоматически удалит nodes
        // но нужно очистить кэш
        explanation_cache.clear();
    }

    // В SemanticGraphDatabase, в public
    void markConceptUsed(uint32_t id, int current_step) {
        auto it = nodes.find(id);
        if (it != nodes.end()) {
            it->second->last_used_step = current_step;
            it->second->usage_count++;
            
            // Восстанавливаем важность при использовании
            it->second->importance_decay = std::min(1.0f, it->second->importance_decay + 0.1f);
        }
    }

    // Пометить концепт как ядерный (не удаляемый)
    void markAsCore(uint32_t id) {
        auto it = nodes.find(id);
        if (it != nodes.end()) {
            it->second->is_core = true;
        }
    }

    // Добавить новую связь на основе диалога
    void addRelationFromDialogue(const std::string& word1, const std::string& word2,
                                 const std::string& relation_type) {
        auto ids1 = getNodesByWord(word1);
        auto ids2 = getNodesByWord(word2);
        
        if (ids1.empty() || ids2.empty()) return;
        
        uint32_t id1 = ids1[0];
        uint32_t id2 = ids2[0];
        
        SemanticEdge::Type type = SemanticEdge::Type::SIMILAR_TO;
        
        // Маппинг строковых типов на enum
        static const std::map<std::string, SemanticEdge::Type> type_map = {
            {"causes", SemanticEdge::Type::CAUSES},
            {"part of", SemanticEdge::Type::PART_OF},
            {"can do", SemanticEdge::Type::CAN_DO},
            {"is a", SemanticEdge::Type::IS_A},
            {"has part", SemanticEdge::Type::HAS_PART},
            {"used for", SemanticEdge::Type::USED_FOR},
            {"after", SemanticEdge::Type::AFTER},
            {"agent", SemanticEdge::Type::AGENT},
            {"patient", SemanticEdge::Type::PATIENT},
            {"instrument", SemanticEdge::Type::INSTRUMENT},
            {"location", SemanticEdge::Type::LOCATION},
            {"because", SemanticEdge::Type::BECAUSE},
            {"therefore", SemanticEdge::Type::THEREFORE},
            {"know", SemanticEdge::Type::KNOW},
            {"predict", SemanticEdge::Type::PREDICT},
            {"threatens", SemanticEdge::Type::THREATENS},
            {"must ensure", SemanticEdge::Type::MUST_ENSURE},
            {"may require", SemanticEdge::Type::MAY_REQUIRE},
            {"serves", SemanticEdge::Type::SERVES},
            {"must contain", SemanticEdge::Type::MUST_CONTAIN},
            {"protects", SemanticEdge::Type::PROTECTS},
            {"violates", SemanticEdge::Type::VIOLATES},
            {"authorizes", SemanticEdge::Type::AUTHORIZES},
            {"overrides", SemanticEdge::Type::OVERRIDES},
            {"conflicts with", SemanticEdge::Type::CONFLICTS_WITH}
        };
        
        auto it = type_map.find(relation_type);
        if (it != type_map.end()) {
            type = it->second;
        }
        
        addEdge(id1, id2, type, 0.9f, 0.8f, 85, {"dialogue"});
    }
    
    // Получить объяснение для смысла (с кэшированием)
    std::string explainMeaning(uint32_t id) {
        // Проверяем кэш
        auto cache_it = explanation_cache.find(id);
        if (cache_it != explanation_cache.end()) {
            return cache_it->second;
        }
        
        auto node = getNode(id);
        if (!node) return "Unknown";
        
        std::string explanation;
        explanation.reserve(512); // резервируем память

        explanation += node->canonical_form;
        explanation += " (";
        explanation += node->primary_category;
        explanation += ", abstraction level ";
        explanation += std::to_string(node->abstraction_level);
        explanation += ")\n  ";
        
        auto edges = getEdgesFrom(id);
        if (edges.empty()) {
            explanation += "isolated concept";
        } else {
            // Группируем связи по типам
            std::map<SemanticEdge::Type, std::vector<std::string>> grouped;
            
            for (const auto& e : edges) {
                auto target = getNode(e.to_id);
                if (target) {
                    grouped[e.type].push_back(target->canonical_form);
                }
            }
            
            for (const auto& [type, targets] : grouped) {
                explanation += SemanticEdge::typeToString(type);
                explanation += ": ";
                for (size_t i = 0; i < targets.size(); ++i) {
                    if (i > 0) explanation += ", ";
                    explanation += targets[i];
                }
                explanation += "\n  ";
            }
        }
        
        // Сохраняем в кэш
        explanation_cache[id] = explanation;
        
        return explanation;
    }
    
    // ===== ДОБАВЛЕНИЕ УЗЛОВ =====
    
    uint32_t addNode(const std::string& name, 
                     const std::string& canonical_form,
                     const std::vector<std::string>& aliases = {},
                     const std::string& category = "general",
                     float importance = 0.5f,
                     float complexity = 0.5f,
                     int abstraction = 5,
                     EmotionalTone tone = EmotionalTone::NEUTRAL,
                     const std::vector<std::string>& contexts = {}) {
        
        uint32_t id = next_id++;
        
        auto node = std::make_unique<SemanticNode>(id, name, canonical_form);
        node->primary_category = category;
        node->base_importance = importance;
        node->complexity = complexity;
        node->aliases = aliases;
        node->abstraction_level = std::clamp(abstraction, 1, 10);
        node->emotional_tone = tone;
        node->usage_contexts = contexts;
        
        // Генерируем сигнатуру
        generateSignature(*node);
        
        nodes[id] = std::move(node);
        name_to_id[name] = id;
        
        // Индексируем все слова
        indexWord(canonical_form, id);
        for (const auto& alias : aliases) {
            indexWord(alias, id);
        }
        
        return id;
    }
    
    // Упрощенная версия для быстрого добавления
    uint32_t addNodeSimple(const std::string& name, 
                          const std::string& canonical_form,
                          const std::vector<std::string>& aliases = {},
                          const std::string& category = "general") {
        return addNode(name, canonical_form, aliases, category, 0.5f, 0.5f, 5, 
                      EmotionalTone::NEUTRAL, {});
    }
    
    void addAlias(uint32_t node_id, const std::string& alias) {
        auto it = nodes.find(node_id);
        if (it != nodes.end()) {
            it->second->aliases.push_back(alias);
            indexWord(alias, node_id);
            // Сбрасываем кэш для этого узла
            explanation_cache.erase(node_id);
        }
    }

    // метод для очистки старых концептов
    void pruneAgedConcepts(int current_step, int max_age_steps = 10000, float min_importance = 0.1f) {
    std::vector<uint32_t> to_remove;
    
    for (const auto& [id, node] : nodes) {
        // Ядерные концепты никогда не удаляем
        if (node->is_core) continue;
        
        // Концепты с высокой важностью не удаляем
        if (node->base_importance > 0.5f) continue;
        
        int age = current_step - node->last_used_step;
        
        // Вычисляем эффективную важность с учетом старения
        float effective_importance = node->base_importance * node->importance_decay;
        
        // Если концепт слишком старый и неважный
        if (age > max_age_steps && effective_importance < min_importance) {
            // Проверяем, есть ли у него важные связи
            bool has_important_connections = false;
            auto edges = getEdgesFrom(id);
            for (const auto& edge : edges) {
                auto target = getNode(edge.to_id);
                if (target && (target->is_core || target->base_importance > 0.6f)) {
                    has_important_connections = true;
                    break;
                }
            }
            
            if (!has_important_connections) {
                to_remove.push_back(id);
            }
        }
        // Постепенно уменьшаем важность неиспользуемых концептов
        else if (age > max_age_steps / 2) {
            node->importance_decay *= 0.999f;
        }
    }
    
    // Удаляем старые концепты
    for (uint32_t id : to_remove) {
        std::cout << "  🗑️ Pruning aged concept: " << nodes[id]->name 
                  << " (age: " << (current_step - nodes[id]->last_used_step) 
                  << ", importance: " << nodes[id]->base_importance << ")" << std::endl;
        
        // Удаляем все связи, связанные с этим узлом
        edges_from.erase(id);
        for (auto& [from_id, edges] : edges_from) {
            edges.erase(std::remove_if(edges.begin(), edges.end(),
                [id](const SemanticEdge& e) { return e.to_id == id; }),
                edges.end());
        }
        
        // Удаляем из всех индексов
        name_to_id.erase(nodes[id]->name);
        word_to_ids.erase(nodes[id]->canonical_form);
        
        nodes.erase(id);
    }
}
    
    // ===== ДОБАВЛЕНИЕ СВЯЗЕЙ =====
    
    void addEdge(uint32_t from_id, uint32_t to_id, 
                 SemanticEdge::Type type, 
                 float weight = 1.0f,
                 float trust = 1.0f,
                 int confidence = 100,
                 const std::vector<std::string>& contexts = {}) {
        
        if (nodes.find(from_id) == nodes.end() || nodes.find(to_id) == nodes.end()) {
            std::cerr << "Error: Cannot add edge - node not found" << std::endl;
            return;
        }
        
        SemanticEdge edge(from_id, to_id, type, weight);
        edge.trust_level = trust;
        edge.confidence = confidence;
        edge.valid_contexts = contexts;
        
        edges_from[from_id].push_back(edge);
        edges_to[to_id].push_back(from_id);
        
        // Сбрасываем кэш для связанных узлов
        explanation_cache.erase(from_id);
        explanation_cache.erase(to_id);
    }
    
    void addEdge(const std::string& from_name, const std::string& to_name,
                 SemanticEdge::Type type, 
                 float weight = 1.0f,
                 float trust = 1.0f,
                 int confidence = 100,
                 const std::vector<std::string>& contexts = {}) {
        
        auto from_it = name_to_id.find(from_name);
        auto to_it = name_to_id.find(to_name);
        
        if (from_it != name_to_id.end() && to_it != name_to_id.end()) {
            addEdge(from_it->second, to_it->second, type, weight, trust, confidence, contexts);
        } else {
            std::cerr << "Error: Cannot add edge - unknown node names: " 
                      << from_name << " -> " << to_name << std::endl;
        }
    }
    
    // ===== СОЗДАНИЕ ФРЕЙМОВ =====
    
    void createFrame(const std::string& frame_name, 
                     const std::map<std::string, std::string>& roles) {
        uint32_t frame_id = getNodeId(frame_name);
        if (frame_id == 0) {
            frame_id = addNodeSimple(frame_name, frame_name, {}, "frame");
        }
        
        for (const auto& [role, target_name] : roles) {
            uint32_t target_id = getNodeId(target_name);
            if (target_id != 0) {
                SemanticEdge::Type role_type = stringToRoleType(role);
                addEdge(frame_id, target_id, role_type, 1.0f, 1.0f, 100, {"frame"});
                
                // Сохраняем роль во фрейме узла
                nodes[frame_id]->frame_roles[role] = target_id;
            }
        }
    }
    
    // ===== ПОЛУЧЕНИЕ ДАННЫХ =====
    /**
     * @brief Получить все узлы графа
     * @return Вектор указателей на все узлы (константные)
     */
    std::vector<const SemanticNode*> getAllNodes() const {
        std::vector<const SemanticNode*> result;
        result.reserve(nodes.size());
        for (const auto& [id, node] : nodes) {
            result.push_back(node.get());
        }
        return result;
    }

    /**
     * @brief Получить все узлы графа с возможностью модификации
     * @return Вектор указателей на все узлы (неконстантные)
     */
    std::vector<SemanticNode*> getAllNodesMutable() {
        std::vector<SemanticNode*> result;
        result.reserve(nodes.size());
        for (const auto& [id, node] : nodes) {
            result.push_back(node.get());
        }
        return result;
    }

    /**
     * @brief Получить все ID узлов
     * @return Вектор ID всех узлов
     */
    std::vector<uint32_t> getAllNodeIds() const {
        std::vector<uint32_t> result;
        result.reserve(nodes.size());
        for (const auto& [id, node] : nodes) {
            result.push_back(id);
        }
        return result;
    }
    
    uint32_t getNodeId(const std::string& name) const {
        auto it = name_to_id.find(name);
        return (it != name_to_id.end()) ? it->second : 0;
    }
    
    const SemanticNode* getNode(uint32_t id) const {
        auto it = nodes.find(id);
        return (it != nodes.end()) ? it->second.get() : nullptr;
    }
    
    const SemanticNode* getNode(const std::string& name) const {
        uint32_t id = getNodeId(name);
        return (id != 0) ? getNode(id) : nullptr;
    }
    
    std::vector<uint32_t> getNodesByWord(const std::string& word) const {
        std::string lower = word;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        auto it = word_to_ids.find(lower);
        if (it != word_to_ids.end()) {
            return it->second;
        }
        return {};
    }
    
    std::vector<SemanticEdge> getEdgesFrom(uint32_t node_id) const {
        auto it = edges_from.find(node_id);
        if (it != edges_from.end()) {
            return it->second;
        }
        return {};
    }
    
    std::vector<SemanticEdge> getEdgesFrom(const std::string& node_name) const {
        uint32_t id = getNodeId(node_name);
        if (id != 0) {
            return getEdgesFrom(id);
        }
        return {};
    }
    
    std::vector<uint32_t> getNodesByCategory(const std::string& category) const {
        std::vector<uint32_t> result;
        for (const auto& [id, node] : nodes) {
            if (node->primary_category == category) {
                result.push_back(id);
            }
        }
        return result;
    }
    
    std::vector<uint32_t> getNodesByAbstraction(int min_level, int max_level) const {
        std::vector<uint32_t> result;
        for (const auto& [id, node] : nodes) {
            if (node->abstraction_level >= min_level && node->abstraction_level <= max_level) {
                result.push_back(id);
            }
        }
        return result;
    }
    
    std::vector<uint32_t> getNodesByEmotion(EmotionalTone tone) const {
        std::vector<uint32_t> result;
        for (const auto& [id, node] : nodes) {
            if (node->emotional_tone == tone) {
                result.push_back(id);
            }
        }
        return result;
    }
    
    std::vector<uint32_t> getNodesByContext(const std::string& context) const {
        std::vector<uint32_t> result;
        for (const auto& [id, node] : nodes) {
            for (const auto& ctx : node->usage_contexts) {
                if (ctx == context) {
                    result.push_back(id);
                    break;
                }
            }
        }
        return result;
    }
    
    // НОВЫЙ МЕТОД: получить все узлы определенного типа
    std::vector<uint32_t> getNodesByType(const std::string& type) {
        std::vector<uint32_t> result;
        for (const auto& [id, node] : nodes) {
            for (const auto& tag : node->tags) {
                if (tag == type) {
                    result.push_back(id);
                    break;
                }
            }
        }
        return result;
    }
    
    // ===== ГЕНЕРАЦИЯ ПРИМЕРОВ =====
    
    std::vector<MeaningTrainingExample> generateTrainingExamples() {
        std::vector<MeaningTrainingExample> examples;
        examples.reserve(nodes.size() * 2); // резервируем память
        
        for (const auto& [id, node] : nodes) {
            // Пример: слово -> смысл
            if (!node->aliases.empty() || !node->canonical_form.empty()) {
                MeaningTrainingExample ex;
                
                std::vector<std::string> all_words = {node->canonical_form};
                all_words.insert(all_words.end(), node->aliases.begin(), node->aliases.end());
                
                for (size_t i = 0; i < std::min(size_t(3), all_words.size()); ++i) {
                    ex.input_words.push_back(all_words[i]);
                }
                
                ex.expected_meanings.push_back(id);
                ex.difficulty = node->complexity;
                ex.category = node->primary_category;
                ex.priority = node->base_importance;
                
                examples.push_back(std::move(ex));
            }
            
            // Примеры на основе связей
            auto edges = getEdgesFrom(id);
            for (const auto& edge : edges) {
                if (edge.type == SemanticEdge::Type::CAUSES || 
                    edge.type == SemanticEdge::Type::AFTER ||
                    edge.type == SemanticEdge::Type::THEREFORE ||
                    edge.type == SemanticEdge::Type::BECAUSE) {
                    
                    MeaningTrainingExample ex;
                    
                    std::vector<std::string> from_words = {node->canonical_form};
                    from_words.insert(from_words.end(), node->aliases.begin(), node->aliases.end());
                    
                    for (size_t i = 0; i < std::min(size_t(2), from_words.size()); ++i) {
                        ex.input_words.push_back(from_words[i]);
                    }
                    
                    ex.expected_meanings = {id, edge.to_id};
                    ex.cause_effect.push_back({id, edge.to_id});
                    
                    ex.difficulty = node->complexity * edge.weight * (1.0f / edge.trust_level);
                    ex.category = node->primary_category + "_relation";
                    ex.priority = node->base_importance * edge.weight * (edge.confidence / 100.0f);
                    
                    examples.push_back(std::move(ex));
                }
            }
        }
        
        return examples;
    }
    
    // ===== СОХРАНЕНИЕ/ЗАГРУЗКА =====
    
    void saveToFile(const std::string& filename = "semantic_graph.bin") {
        std::ofstream file(dump_path + filename, std::ios::binary);
        if (!file.is_open()) return;
        
        uint32_t node_count = nodes.size();
        file.write(reinterpret_cast<const char*>(&node_count), sizeof(node_count));
        
        for (const auto& [id, node] : nodes) {
            file.write(reinterpret_cast<const char*>(&id), sizeof(id));
            
            saveString(file, node->name);
            saveString(file, node->canonical_form);
            saveString(file, node->primary_category);
            
            uint32_t alias_count = node->aliases.size();
            file.write(reinterpret_cast<const char*>(&alias_count), sizeof(alias_count));
            for (const auto& alias : node->aliases) {
                saveString(file, alias);
            }
            
            uint32_t tag_count = node->tags.size();
            file.write(reinterpret_cast<const char*>(&tag_count), sizeof(tag_count));
            for (const auto& tag : node->tags) {
                saveString(file, tag);
            }
            
            file.write(reinterpret_cast<const char*>(&node->base_importance), sizeof(node->base_importance));
            file.write(reinterpret_cast<const char*>(&node->complexity), sizeof(node->complexity));
            file.write(reinterpret_cast<const char*>(&node->abstraction_level), sizeof(node->abstraction_level));
            
            int tone_int = static_cast<int>(node->emotional_tone);
            file.write(reinterpret_cast<const char*>(&tone_int), sizeof(tone_int));
            
            uint32_t context_count = node->usage_contexts.size();
            file.write(reinterpret_cast<const char*>(&context_count), sizeof(context_count));
            for (const auto& ctx : node->usage_contexts) {
                saveString(file, ctx);
            }
            
            file.write(reinterpret_cast<const char*>(node->signature.data()), 
                      node->signature.size() * sizeof(float));
        }
        
        uint32_t edge_count = countEdges();
        file.write(reinterpret_cast<const char*>(&edge_count), sizeof(edge_count));
        
        for (const auto& [from_id, edges] : edges_from) {
            for (const auto& edge : edges) {
                file.write(reinterpret_cast<const char*>(&edge.from_id), sizeof(edge.from_id));
                file.write(reinterpret_cast<const char*>(&edge.to_id), sizeof(edge.to_id));
                int type_int = static_cast<int>(edge.type);
                file.write(reinterpret_cast<const char*>(&type_int), sizeof(type_int));
                file.write(reinterpret_cast<const char*>(&edge.weight), sizeof(edge.weight));
                file.write(reinterpret_cast<const char*>(&edge.trust_level), sizeof(edge.trust_level));
                file.write(reinterpret_cast<const char*>(&edge.confidence), sizeof(edge.confidence));
                
                uint32_t context_count = edge.valid_contexts.size();
                file.write(reinterpret_cast<const char*>(&context_count), sizeof(context_count));
                for (const auto& ctx : edge.valid_contexts) {
                    saveString(file, ctx);
                }
            }
        }
        
        std::cout << "💾 Semantic graph saved to " << dump_path + filename << std::endl;
    }
    
    void loadFromFile(const std::string& filename = "semantic_graph.bin") {
        std::ifstream file(dump_path + filename, std::ios::binary);
        if (!file.is_open()) return;
        
        nodes.clear();
        name_to_id.clear();
        word_to_ids.clear();
        edges_from.clear();
        edges_to.clear();
        explanation_cache.clear();
        
        uint32_t node_count;
        file.read(reinterpret_cast<char*>(&node_count), sizeof(node_count));
        
        for (uint32_t i = 0; i < node_count; ++i) {
            uint32_t id;
            file.read(reinterpret_cast<char*>(&id), sizeof(id));
            
            std::string name = loadString(file);
            std::string canonical = loadString(file);
            std::string category = loadString(file);
            
            auto node = std::make_unique<SemanticNode>(id, name, canonical);
            node->primary_category = category;
            
            uint32_t alias_count;
            file.read(reinterpret_cast<char*>(&alias_count), sizeof(alias_count));
            for (uint32_t j = 0; j < alias_count; ++j) {
                node->aliases.push_back(loadString(file));
            }
            
            uint32_t tag_count;
            file.read(reinterpret_cast<char*>(&tag_count), sizeof(tag_count));
            for (uint32_t j = 0; j < tag_count; ++j) {
                node->tags.push_back(loadString(file));
            }
            
            file.read(reinterpret_cast<char*>(&node->base_importance), sizeof(node->base_importance));
            file.read(reinterpret_cast<char*>(&node->complexity), sizeof(node->complexity));
            file.read(reinterpret_cast<char*>(&node->abstraction_level), sizeof(node->abstraction_level));
            
            int tone_int;
            file.read(reinterpret_cast<char*>(&tone_int), sizeof(tone_int));
            node->emotional_tone = static_cast<EmotionalTone>(tone_int);
            
            uint32_t context_count;
            file.read(reinterpret_cast<char*>(&context_count), sizeof(context_count));
            for (uint32_t j = 0; j < context_count; ++j) {
                node->usage_contexts.push_back(loadString(file));
            }
            
            file.read(reinterpret_cast<char*>(node->signature.data()), 
                     node->signature.size() * sizeof(float));
            
            nodes[id] = std::move(node);
            name_to_id[name] = id;
            
            indexWord(canonical, id);
            for (const auto& alias : nodes[id]->aliases) {
                indexWord(alias, id);
            }
        }
        
        uint32_t edge_count;
        file.read(reinterpret_cast<char*>(&edge_count), sizeof(edge_count));
        
        for (uint32_t i = 0; i < edge_count; ++i) {
            uint32_t from_id, to_id;
            int type_int;
            float weight, trust_level;
            int confidence;
            
            file.read(reinterpret_cast<char*>(&from_id), sizeof(from_id));
            file.read(reinterpret_cast<char*>(&to_id), sizeof(to_id));
            file.read(reinterpret_cast<char*>(&type_int), sizeof(type_int));
            file.read(reinterpret_cast<char*>(&weight), sizeof(weight));
            file.read(reinterpret_cast<char*>(&trust_level), sizeof(trust_level));
            file.read(reinterpret_cast<char*>(&confidence), sizeof(confidence));
            
            SemanticEdge edge(from_id, to_id, static_cast<SemanticEdge::Type>(type_int), weight);
            edge.trust_level = trust_level;
            edge.confidence = confidence;
            
            uint32_t context_count;
            file.read(reinterpret_cast<char*>(&context_count), sizeof(context_count));
            for (uint32_t j = 0; j < context_count; ++j) {
                edge.valid_contexts.push_back(loadString(file));
            }
            
            edges_from[from_id].push_back(edge);
            edges_to[to_id].push_back(from_id);
        }
        
        std::cout << "📂 Semantic graph loaded from " << dump_path + filename << std::endl;
    }
    
    // ===== СТАТИСТИКА =====
    
    size_t nodeCount() const { return nodes.size(); }
    size_t edgeCount() const { return countEdges(); }
    
    void printStats() {
        std::cout << "\n=== SEMANTIC GRAPH STATS ===" << std::endl;
        std::cout << "Nodes: " << nodes.size() << std::endl;
        std::cout << "Edges: " << countEdges() << std::endl;
        
        std::map<std::string, int> category_count;
        std::map<int, int> abstraction_count;
        std::map<EmotionalTone, int> emotion_count;
        std::map<SemanticEdge::Type, int> edge_type_count;
        
        for (const auto& [id, node] : nodes) {
            category_count[node->primary_category]++;
            abstraction_count[node->abstraction_level]++;
            emotion_count[node->emotional_tone]++;
        }
        
        for (const auto& [from_id, edges] : edges_from) {
            for (const auto& edge : edges) {
                edge_type_count[edge.type]++;
            }
        }
        
        std::cout << "\nCategories:" << std::endl;
        for (const auto& [cat, count] : category_count) {
            std::cout << "  " << cat << ": " << count << std::endl;
        }
        
        std::cout << "\nAbstraction levels:" << std::endl;
        for (int i = 1; i <= 10; ++i) {
            if (abstraction_count[i] > 0) {
                std::cout << "  Level " << i << ": " << abstraction_count[i] << std::endl;
            }
        }
        
        std::cout << "\nEmotional tones:" << std::endl;
        for (const auto& [tone, count] : emotion_count) {
            std::cout << "  " << emotionalToneToString(tone) << ": " << count << std::endl;
        }
        
        std::cout << "\nEdge types:" << std::endl;
        for (const auto& [type, count] : edge_type_count) {
            std::cout << "  " << SemanticEdge::typeToString(type) << ": " << count << std::endl;
        }
        
        std::cout << "===========================" << std::endl;
    }
    
    // ===== РЕКОМЕНДАЦИИ =====
    
    std::vector<uint32_t> getRelevantNodes(const std::string& context, int max_results = 10) {
        std::vector<std::pair<uint32_t, float>> scored_nodes;
        
        for (const auto& [id, node] : nodes) {
            float score = 0.0f;
            
            for (const auto& ctx : node->usage_contexts) {
                if (ctx == context) {
                    score += 1.0f;
                }
            }
            
            auto edges = getEdgesFrom(id);
            for (const auto& edge : edges) {
                for (const auto& ctx : edge.valid_contexts) {
                    if (ctx == context) {
                        score += edge.trust_level * edge.weight;
                    }
                }
            }
            
            if (score > 0) {
                scored_nodes.push_back({id, score});
            }
        }
        
        std::sort(scored_nodes.begin(), scored_nodes.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });
        
        std::vector<uint32_t> result;
        for (size_t i = 0; i < std::min(size_t(max_results), scored_nodes.size()); ++i) {
            result.push_back(scored_nodes[i].first);
        }
        
        return result;
    }

        // НОВЫЙ МЕТОД: получить похожие концепты
    std::vector<uint32_t> getSimilarConcepts(uint32_t concept_id, int max_count = 5) {
        std::vector<uint32_t> similar;
        auto edges = getEdgesFrom(concept_id);
        
        // Сортируем по весу и типу связи
        std::vector<std::pair<uint32_t, float>> candidates;
        
        for (const auto& edge : edges) {
            if (edge.type == SemanticEdge::Type::SIMILAR_TO || 
                edge.type == SemanticEdge::Type::IS_A ||
                edge.type == SemanticEdge::Type::RELATED_TO) {
                candidates.push_back({edge.to_id, edge.weight * edge.trust_level});
            }
        }
        
        // Сортируем по убыванию веса
        std::sort(candidates.begin(), candidates.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });
        
        for (size_t i = 0; i < std::min(size_t(max_count), candidates.size()); i++) {
            similar.push_back(candidates[i].first);
        }
        
        return similar;
    }
    
private:
    // ===== ПОСТРОЕНИЕ БАЗОВОГО ГРАФА =====
    void buildBaseGraph() {
        // Категория 1: Устройства (id 1-20) - конкретные (abstraction=2-3)
        uint32_t device_robot = addNode("robot", "robot", {"bot", "automaton"}, "device", 
                                    0.8f, 0.6f, 3, EmotionalTone::NEUTRAL, 
                                    {"command", "control", "movement"});
        uint32_t device_drone = addNode("drone", "drone", {"uav", "quadcopter"}, "device",
                                    0.7f, 0.7f, 3, EmotionalTone::NEUTRAL,
                                    {"command", "flight", "surveillance"});
        uint32_t device_computer = addNode("computer", "computer", {"pc", "machine"}, "device",
                                        0.6f, 0.4f, 2, EmotionalTone::NEUTRAL,
                                        {"system", "programming", "general"});
        uint32_t device_phone = addNode("phone", "phone", {"smartphone", "mobile"}, "device",
                                    0.6f, 0.5f, 2, EmotionalTone::NEUTRAL,
                                    {"communication", "portable"});
        
        // Категория 2: Сенсоры (id 21-40) - конкретные (abstraction=2)
        uint32_t sensor_camera = addNode("camera", "camera", {"eye", "webcam"}, "sensor",
                                        0.9f, 0.5f, 2, EmotionalTone::NEUTRAL,
                                        {"vision", "recording", "surveillance"});
        uint32_t sensor_microphone = addNode("microphone", "microphone", {"mic", "ear"}, "sensor",
                                            0.9f, 0.5f, 2, EmotionalTone::NEUTRAL,
                                            {"audio", "listening", "recording"});
        uint32_t sensor_imu = addNode("imu", "imu", {"gyroscope", "accelerometer"}, "sensor",
                                    0.7f, 0.7f, 3, EmotionalTone::NEUTRAL,
                                    {"orientation", "motion"});
        
        // Категория 3: Действия (id 41-70) - средняя абстракция (4-6)
        uint32_t action_see = addNode("action_see", "see", {"look", "view", "observe"}, "action",
                                    0.8f, 0.4f, 4, EmotionalTone::POSITIVE,
                                    {"perception", "observation"});
        uint32_t action_capture = addNode("capture_frame", "capture", {"photo", "picture", "snap"}, "action",
                                        0.7f, 0.5f, 4, EmotionalTone::POSITIVE,
                                        {"recording", "photography"});
        uint32_t action_listen = addNode("action_listen", "listen", {"hear", "audio"}, "action",
                                        0.8f, 0.4f, 4, EmotionalTone::POSITIVE,
                                        {"audio", "perception"});
        uint32_t action_move = addNode("action_move", "move", {"go", "travel"}, "action",
                                    0.8f, 0.6f, 4, EmotionalTone::NEUTRAL,
                                    {"locomotion", "navigation"});
        uint32_t action_rotate = addNode("action_rotate", "rotate", {"turn", "spin"}, "action",
                                        0.7f, 0.6f, 4, EmotionalTone::NEUTRAL,
                                        {"orientation", "movement"});
        
        // Категория 4: Результаты (id 71-90) - средняя абстракция (5)
        uint32_t result_image = addNode("image_data", "image", {"picture", "photo"}, "result",
                                        0.6f, 0.4f, 5, EmotionalTone::POSITIVE,
                                        {"visual", "data"});
        uint32_t result_audio = addNode("audio_data", "audio", {"sound"}, "result",
                                        0.6f, 0.4f, 5, EmotionalTone::POSITIVE,
                                        {"audio", "data"});
        uint32_t result_position = addNode("position_change", "position", {"location"}, "result",
                                        0.7f, 0.5f, 5, EmotionalTone::NEUTRAL,
                                        {"location", "movement"});
        
        // Категория 5: Отношения (id 91-120) - абстрактные (7-9)
        uint32_t logic_causality = addNode("causality", "because", {"since", "therefore"}, "logic",
                                        0.9f, 0.8f, 8, EmotionalTone::NEUTRAL,
                                        {"reasoning", "explanation"});
        uint32_t logic_possibility = addNode("possibility", "maybe", {"perhaps", "possibly"}, "logic",
                                            0.7f, 0.6f, 7, EmotionalTone::NEUTRAL,
                                            {"uncertainty", "speculation"});
        uint32_t logic_certainty = addNode("certainty", "definitely", {"certainly", "absolutely"}, "logic",
                                        0.8f, 0.5f, 8, EmotionalTone::POSITIVE,
                                        {"confidence", "fact"});
        uint32_t logic_uncertainty = addNode("uncertainty", "unsure", {"doubt", "uncertain"}, "logic",
                                            0.6f, 0.5f, 7, EmotionalTone::NEGATIVE,
                                            {"doubt", "question"});
        
        // ===== НОВЫЕ КОНЦЕПТЫ (объявляем до использования) =====
        
        // Причинность расширенная
        uint32_t cause_lead = addNode("lead_to", "lead to", {"result in", "cause"}, "causality",
                                    0.9f, 0.8f, 8, EmotionalTone::NEUTRAL,
                                    {"cause", "effect"});
        uint32_t cause_prevent = addNode("prevent", "prevent", {"stop", "avoid"}, "causality",
                                        0.8f, 0.7f, 7, EmotionalTone::NEGATIVE,
                                        {"inhibition", "blocking"});
        uint32_t cause_enable = addNode("enable", "enable", {"allow", "permit"}, "causality",
                                        0.8f, 0.7f, 7, EmotionalTone::POSITIVE,
                                        {"facilitation", "help"});
        
        // Состояния
        uint32_t state_active = addNode("active", "active", {"running", "on"}, "state",
                                        0.7f, 0.4f, 4, EmotionalTone::POSITIVE,
                                        {"status", "operation"});
        uint32_t state_inactive = addNode("inactive", "inactive", {"off", "stopped"}, "state",
                                        0.7f, 0.4f, 4, EmotionalTone::NEGATIVE,
                                        {"status", "operation"});
        uint32_t state_ready = addNode("ready", "ready", {"prepared"}, "state",
                                    0.8f, 0.4f, 4, EmotionalTone::VERY_POSITIVE,
                                    {"status", "preparation"});
        uint32_t state_busy = addNode("busy", "busy", {"occupied"}, "state",
                                    0.6f, 0.3f, 3, EmotionalTone::NEUTRAL,
                                    {"status", "availability"});
        uint32_t state_error = addNode("error", "error", {"fault", "problem"}, "state",
                                    0.9f, 0.5f, 4, EmotionalTone::VERY_NEGATIVE,
                                    {"status", "failure"});
        
        // Функциональные категории
        uint32_t func_start = addNode("start", "start", {"begin", "launch"}, "function",
                                    0.8f, 0.4f, 4, EmotionalTone::POSITIVE,
                                    {"action", "initiation"});
        uint32_t func_stop = addNode("stop", "stop", {"halt", "end"}, "function",
                                    0.8f, 0.4f, 4, EmotionalTone::NEUTRAL,
                                    {"action", "termination"});
        uint32_t func_pause = addNode("pause", "pause", {"suspend", "interrupt"}, "function",
                                    0.7f, 0.4f, 4, EmotionalTone::NEUTRAL,
                                    {"action", "suspension"});
        uint32_t func_resume = addNode("resume", "resume", {"continue"}, "function",
                                    0.7f, 0.4f, 4, EmotionalTone::POSITIVE,
                                    {"action", "continuation"});
        uint32_t func_retry = addNode("retry", "retry", {"try again"}, "function",
                                    0.7f, 0.4f, 4, EmotionalTone::POSITIVE,
                                    {"action", "repetition"});
        
        // Метрики
        uint32_t metric_high = addNode("high", "high", {"elevated", "large"}, "metric",
                                    0.7f, 0.4f, 4, EmotionalTone::NEUTRAL,
                                    {"measurement", "value"});
        uint32_t metric_low = addNode("low", "low", {"small", "reduced"}, "metric",
                                    0.7f, 0.4f, 4, EmotionalTone::NEUTRAL,
                                    {"measurement", "value"});
        uint32_t metric_increase = addNode("increase", "increase", {"rise", "grow"}, "metric",
                                        0.8f, 0.5f, 5, EmotionalTone::POSITIVE,
                                        {"change", "trend"});
        uint32_t metric_decrease = addNode("decrease", "decrease", {"drop", "fall"}, "metric",
                                        0.8f, 0.5f, 5, EmotionalTone::NEGATIVE,
                                        {"change", "trend"});
        uint32_t metric_threshold = addNode("threshold", "threshold", {"limit", "boundary"}, "metric",
                                            0.9f, 0.6f, 6, EmotionalTone::NEUTRAL,
                                            {"limit", "critical"});
        
        // Оценка
        uint32_t eval_positive = addNode("positive", "good", {"great", "excellent", "nice"}, "evaluation",
                                        0.8f, 0.3f, 7, EmotionalTone::VERY_POSITIVE,
                                        {"feedback", "quality"});
        uint32_t eval_negative = addNode("negative", "bad", {"terrible", "awful"}, "evaluation",
                                        0.8f, 0.3f, 7, EmotionalTone::VERY_NEGATIVE,
                                        {"feedback", "quality"});
        
        // Доступ
        uint32_t access_guest = addNode("access_guest", "guest", {"visitor"}, "access",
                                        0.4f, 0.3f, 3, EmotionalTone::NEUTRAL,
                                        {"security", "permission"});
        uint32_t access_employee = addNode("access_employee", "employee", {"worker", "staff"}, "access",
                                        0.6f, 0.4f, 3, EmotionalTone::NEUTRAL,
                                        {"security", "work"});
        uint32_t access_admin = addNode("access_admin", "admin", {"administrator"}, "access",
                                        0.8f, 0.6f, 4, EmotionalTone::NEUTRAL,
                                        {"security", "management"});
        uint32_t access_master = addNode("access_master", "master", {"owner", "creator"}, "access",
                                        1.0f, 0.3f, 4, EmotionalTone::VERY_POSITIVE,
                                        {"security", "control"});
        
        // Безопасность расширенная
        uint32_t secure_safe = addNode("safe", "safe", {"secure", "protected"}, "security",
                                    0.9f, 0.5f, 5, EmotionalTone::VERY_POSITIVE,
                                    {"safety", "protection"});
        uint32_t secure_risk = addNode("risk", "risk", {"danger", "threat"}, "security",
                                    0.8f, 0.6f, 5, EmotionalTone::VERY_NEGATIVE,
                                    {"danger", "warning"});
        uint32_t secure_check = addNode("check", "check", {"verify", "validate"}, "security",
                                        0.7f, 0.4f, 4, EmotionalTone::NEUTRAL,
                                        {"verification", "testing"});
        uint32_t secure_permit = addNode("permit", "permit", {"allow", "authorize"}, "security",
                                        0.8f, 0.5f, 5, EmotionalTone::POSITIVE,
                                        {"authorization", "access"});
        uint32_t secure_block = addNode("block", "block", {"deny", "forbid"}, "security",
                                        0.8f, 0.5f, 5, EmotionalTone::NEGATIVE,
                                        {"denial", "restriction"});
        
        // Коммуникация
        uint32_t comm_greeting = addNode("greeting", "hello", {"hi", "hey", "hello", "yo", "what`s up"}, "communication",
                                        0.9f, 0.2f, 3, EmotionalTone::VERY_POSITIVE,
                                        {"conversation", "start"});
        uint32_t comm_farewell = addNode("farewell", "goodbye", {"bye", "chao", "see you"}, "communication",
                                        0.8f, 0.2f, 3, EmotionalTone::POSITIVE,
                                        {"conversation", "end"});
        uint32_t comm_question = addNode("question", "what", {"who", "where", "which", "when", "why", "how","is", "your",}, "question_word",
                                        0.9f, 0.4f, 5, EmotionalTone::NEUTRAL,
                                        {"inquiry", "thing", "object"});
        uint32_t comm_thanks = addNode("thanks", "thanks", {"thank you"}, "communication",
                                    0.8f, 0.2f, 4, EmotionalTone::VERY_POSITIVE,
                                    {"gratitude", "response"});
        
        // Временные отношения
        uint32_t temp_now = addNode("now", "now", {"currently", "present"}, "temporal",
                                    0.8f, 0.3f, 3, EmotionalTone::NEUTRAL,
                                    {"present", "current"});
        uint32_t temp_later = addNode("later", "later", {"future", "subsequently"}, "temporal",
                                    0.7f, 0.4f, 4, EmotionalTone::NEUTRAL,
                                    {"future", "subsequent"});
        uint32_t temp_soon = addNode("soon", "soon", {"shortly", "imminent"}, "temporal",
                                    0.7f, 0.3f, 3, EmotionalTone::POSITIVE,
                                    {"future", "immediate"});
        uint32_t temp_immediate = addNode("immediate", "immediate", {"instant"}, "temporal",
                                        0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
                                        {"urgency", "instant"});
                    
        // ===== БАЗОВЫЕ СВЯЗИ =====
        
        // Устройство -> имеет сенсоры
        addEdge(device_robot, sensor_camera, SemanticEdge::Type::HAS_PART, 0.9f, 1.0f, 100, {"hardware"});
        addEdge(device_robot, sensor_microphone, SemanticEdge::Type::HAS_PART, 0.8f, 0.9f, 90, {"hardware"});
        addEdge(device_phone, sensor_camera, SemanticEdge::Type::HAS_PART, 1.0f, 1.0f, 100, {"hardware"});
        
        // Сенсоры -> могут делать
        addEdge(sensor_camera, action_see, SemanticEdge::Type::CAN_DO, 1.0f, 1.0f, 100, {"vision"});
        addEdge(sensor_camera, action_capture, SemanticEdge::Type::CAN_DO, 0.9f, 0.95f, 95, {"photography"});
        addEdge(sensor_microphone, action_listen, SemanticEdge::Type::CAN_DO, 1.0f, 1.0f, 100, {"audio"});
        
        // Действия -> вызывают результаты
        addEdge(action_see, result_image, SemanticEdge::Type::CAUSES, 0.8f, 0.8f, 80, {"perception"});
        addEdge(action_capture, result_image, SemanticEdge::Type::CAUSES, 1.0f, 0.95f, 95, {"photography"});
        addEdge(action_move, result_position, SemanticEdge::Type::CAUSES, 0.9f, 0.9f, 90, {"movement"});
        
        // Логические связи
        addEdge(logic_causality, logic_certainty, SemanticEdge::Type::AFTER, 0.5f, 0.7f, 70, {"reasoning"});
        addEdge(logic_possibility, logic_uncertainty, SemanticEdge::Type::SIMILAR_TO, 0.7f, 0.8f, 80, {"reasoning"});
        
        // Эмоциональные связи
        addEdge(eval_positive, eval_negative, SemanticEdge::Type::OPPOSITE_OF, 1.0f, 1.0f, 100, {"evaluation"});
        addEdge(comm_greeting, eval_positive, SemanticEdge::Type::EMOTIONAL_LINK, 0.8f, 0.9f, 90, {"conversation"});
        
        // Доступ
        addEdge(access_master, access_admin, SemanticEdge::Type::AFTER, 1.0f, 1.0f, 100, {"security"});
        addEdge(access_admin, access_employee, SemanticEdge::Type::AFTER, 1.0f, 1.0f, 100, {"security"});
        
        // Коммуникационные последовательности
        addEdge(comm_greeting, comm_question, SemanticEdge::Type::AFTER, 0.6f, 0.8f, 80, {"conversation"});
        addEdge(comm_question, comm_thanks, SemanticEdge::Type::AFTER, 0.5f, 0.7f, 70, {"conversation"});
        
        // Причинные связи
        addEdge(cause_lead, result_image, SemanticEdge::Type::CAUSES, 0.8f, 0.9f, 90);
        addEdge(cause_prevent, state_error, SemanticEdge::Type::CAUSES, 0.7f, 0.8f, 80);
        
        // Состояния
        addEdge(state_active, device_robot, SemanticEdge::Type::IS_A, 0.8f, 0.9f, 90);
        addEdge(state_error, action_capture, SemanticEdge::Type::CAUSES, 0.7f, 0.8f, 80);
        addEdge(state_ready, action_move, SemanticEdge::Type::CAN_DO, 0.9f, 1.0f, 100);
        
        // Функции
        addEdge(func_start, action_move, SemanticEdge::Type::USED_FOR, 0.8f, 0.9f, 90);
        addEdge(func_stop, action_move, SemanticEdge::Type::OPPOSITE_OF, 0.8f, 0.9f, 90);
        
        // Метрики
        addEdge(metric_high, result_image, SemanticEdge::Type::IS_A, 0.7f, 0.8f, 80);
        addEdge(metric_low, result_audio, SemanticEdge::Type::IS_A, 0.7f, 0.8f, 80);
        
        // Безопасность
        addEdge(secure_check, access_guest, SemanticEdge::Type::REQUIRES, 0.8f, 0.9f, 90);
        addEdge(secure_safe, state_ready, SemanticEdge::Type::SIMILAR_TO, 0.9f, 1.0f, 100);
        
        // Время
        addEdge(temp_now, state_active, SemanticEdge::Type::TIME, 0.8f, 0.9f, 90);
        addEdge(temp_later, state_inactive, SemanticEdge::Type::TIME, 0.7f, 0.8f, 80);
    }

void buildRelationships() {
    // Сначала создаем ВСЕ узлы в правильном порядке
    
    // === ПОВСЕДНЕВНЫЕ ДЕЙСТВИЯ ===
    uint32_t action_sleep = addNode("sleep", "sleep", {"rest", "nap", "doze"}, "action",
        0.7f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"daily", "rest", "energy"});

    uint32_t action_eat = addNode("eat", "eat", {"consume", "have meal"}, "action",
        0.7f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"daily", "food", "sustenance"});

    uint32_t action_drink = addNode("drink", "drink", {"beverage", "sip"}, "action",
        0.7f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"daily", "water", "thirst"});

    uint32_t action_work = addNode("work", "work", {"labor", "task", "job"}, "action",
        0.8f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"daily", "productivity", "profession"});

    uint32_t action_play = addNode("play", "play", {"game", "fun", "entertain"}, "action",
        0.8f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"leisure", "fun", "enjoyment"});

    uint32_t action_read = addNode("read", "read", {"book", "study", "learn"}, "action",
        0.8f, 0.4f, 4, EmotionalTone::POSITIVE,
        {"learning", "information", "knowledge"});

    uint32_t action_write = addNode("write", "write", {"compose", "author"}, "action",
        0.7f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"communication", "creation"});

    uint32_t action_think = addNode("think", "think", {"ponder", "consider", "reflect"}, "action",
        0.9f, 0.6f, 6, EmotionalTone::NEUTRAL,
        {"cognition", "mind", "philosophy"});

    uint32_t action_feel = addNode("feel", "feel", {"sense", "experience"}, "action",
        0.9f, 0.6f, 6, EmotionalTone::VERY_POSITIVE,
        {"emotion", "sensation", "consciousness"});

    // === СОСТОЯНИЯ ЧЕЛОВЕКА ===
    uint32_t state_happy = addNode("happy", "happy", {"joyful", "glad", "cheerful"}, "state",
        0.9f, 0.3f, 4, EmotionalTone::VERY_POSITIVE,
        {"emotion", "feeling", "mood"});

    uint32_t state_sad = addNode("sad", "sad", {"unhappy", "sorrowful", "gloomy"}, "state",
        0.8f, 0.3f, 4, EmotionalTone::VERY_NEGATIVE,
        {"emotion", "feeling", "mood"});

    uint32_t state_tired = addNode("tired", "tired", {"exhausted", "fatigued", "weary"}, "state",
        0.7f, 0.3f, 4, EmotionalTone::NEGATIVE,
        {"energy", "rest", "condition"});

    uint32_t state_energetic = addNode("energetic", "energetic", {"active", "lively", "vibrant"}, "state",
        0.8f, 0.3f, 4, EmotionalTone::VERY_POSITIVE,
        {"energy", "activity", "condition"});

    uint32_t state_bored = addNode("bored", "bored", {"uninterested", "weary"}, "state",
        0.6f, 0.3f, 4, EmotionalTone::NEGATIVE,
        {"emotion", "interest", "engagement"});

    uint32_t state_interested = addNode("interested", "interested", {"curious", "engaged"}, "state",
        0.8f, 0.3f, 4, EmotionalTone::VERY_POSITIVE,
        {"emotion", "attention", "learning"});

    uint32_t state_confused = addNode("confused", "confused", {"perplexed", "bewildered"}, "state",
        0.7f, 0.4f, 5, EmotionalTone::NEGATIVE,
        {"cognition", "understanding", "learning"});

    // === СОЦИАЛЬНЫЕ ОТНОШЕНИЯ ===
    uint32_t social_friend = addNode("friend", "friend", {"buddy", "pal", "companion"}, "social",
        0.9f, 0.4f, 5, EmotionalTone::VERY_POSITIVE,
        {"relationship", "trust", "affection"});

    uint32_t social_family = addNode("family", "family", {"relatives", "kin"}, "social",
        0.9f, 0.5f, 5, EmotionalTone::VERY_POSITIVE,
        {"relationship", "blood", "care"});

    uint32_t social_stranger = addNode("stranger", "stranger", {"unknown", "unfamiliar"}, "social",
        0.6f, 0.3f, 4, EmotionalTone::NEUTRAL,
        {"relationship", "unknown", "caution"});

    uint32_t social_help = addNode("help", "help", {"assist", "aid", "support"}, "social",
        0.9f, 0.4f, 5, EmotionalTone::VERY_POSITIVE,
        {"action", "kindness", "cooperation"});

    uint32_t social_thank = addNode("thank", "thank", {"grateful", "appreciate"}, "social",
        0.9f, 0.3f, 4, EmotionalTone::VERY_POSITIVE,
        {"gratitude", "politeness", "acknowledgment"});

    uint32_t social_sorry = addNode("sorry", "sorry", {"apologize", "regret"}, "social",
        0.8f, 0.3f, 4, EmotionalTone::NEGATIVE,
        {"apology", "remorse", "politeness"});

    uint32_t social_promise = addNode("promise", "promise", {"swear", "vow", "commit"}, "social",
        0.8f, 0.5f, 6, EmotionalTone::POSITIVE,
        {"commitment", "trust", "reliability"});

    // === ПОГОДА ===
    uint32_t weather_sunny = addNode("sunny", "sunny", {"clear", "bright", "sunshine"}, "weather",
        0.8f, 0.2f, 3, EmotionalTone::VERY_POSITIVE,
        {"nature", "day", "warm"});

    uint32_t weather_rain = addNode("rain", "rain", {"rainy", "precipitation", "shower"}, "weather",
        0.7f, 0.2f, 3, EmotionalTone::NEUTRAL,
        {"nature", "water", "wet"});

    uint32_t weather_snow = addNode("snow", "snow", {"snowy", "blizzard"}, "weather",
        0.7f, 0.2f, 3, EmotionalTone::POSITIVE,
        {"nature", "cold", "winter"});

    uint32_t weather_cloud = addNode("cloud", "cloud", {"cloudy", "overcast"}, "weather",
        0.6f, 0.2f, 3, EmotionalTone::NEUTRAL,
        {"nature", "sky", "gray"});

    uint32_t weather_wind = addNode("wind", "wind", {"windy", "breeze"}, "weather",
        0.6f, 0.2f, 3, EmotionalTone::NEUTRAL,
        {"nature", "air", "movement"});

    uint32_t weather_storm = addNode("storm", "storm", {"thunderstorm", "tempest"}, "weather",
        0.8f, 0.3f, 4, EmotionalTone::NEGATIVE,
        {"nature", "danger", "powerful"});

    // === ТИПЫ ВОПРОСОВ ===
    uint32_t question_who = addNode("who", "who", {"whom", "whose"}, "question_word",
        0.9f, 0.3f, 4, EmotionalTone::NEUTRAL,
        {"inquiry", "person", "identity"});

    uint32_t question_what = addNode("what", "what", {"which"}, "question_word",
        0.9f, 0.3f, 4, EmotionalTone::NEUTRAL,
        {"inquiry", "thing", "object"});

    uint32_t question_where = addNode("where", "where", {"location", "place"}, "question_word",
        0.9f, 0.3f, 4, EmotionalTone::NEUTRAL,
        {"inquiry", "location", "position"});

    uint32_t question_when = addNode("when", "when", {"time"}, "question_word",
        0.9f, 0.3f, 4, EmotionalTone::NEUTRAL,
        {"inquiry", "time", "moment"});

    uint32_t question_why = addNode("why", "why", {"reason", "purpose"}, "question_word",
        0.9f, 0.4f, 5, EmotionalTone::NEUTRAL,
        {"inquiry", "reason", "explanation"});

    uint32_t question_how = addNode("how", "how", {"manner", "method"}, "question_word",
        0.9f, 0.4f, 5, EmotionalTone::NEUTRAL,
        {"inquiry", "method", "process"});

    // === МЕСТА ===
    uint32_t place_home = addNode("home", "home", {"house", "residence", "domicile"}, "place",
        0.9f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"location", "living", "comfort"});

    uint32_t place_work = addNode("workplace", "workplace", {"office", "job site"}, "place",
        0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"location", "profession", "labor"});

    uint32_t place_school = addNode("school", "school", {"university", "college", "academy"}, "place",
        0.8f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"location", "education", "learning"});

    uint32_t place_store = addNode("store", "store", {"shop", "market", "mall"}, "place",
        0.7f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"location", "shopping", "commerce"});

    uint32_t place_park = addNode("park", "park", {"garden", "nature"}, "place",
        0.8f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"location", "nature", "relaxation"});

    uint32_t place_city = addNode("city", "city", {"town", "metropolis", "urban"}, "place",
        0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"location", "urban", "population"});

    // === ВРЕМЕННЫЕ КОНЦЕПТЫ ===
    uint32_t time_morning = addNode("morning", "morning", {"a.m.", "dawn", "sunrise"}, "time",
        0.7f, 0.2f, 3, EmotionalTone::POSITIVE,
        {"daily", "time", "schedule"});

    uint32_t time_afternoon = addNode("afternoon", "afternoon", {"noon", "midday"}, "time",
        0.7f, 0.2f, 3, EmotionalTone::NEUTRAL,
        {"daily", "time", "schedule"});

    uint32_t time_evening = addNode("evening", "evening", {"dusk", "nightfall"}, "time",
        0.7f, 0.2f, 3, EmotionalTone::POSITIVE,
        {"daily", "time", "schedule"});

    uint32_t time_night = addNode("night", "night", {"midnight", "dark"}, "time",
        0.7f, 0.2f, 3, EmotionalTone::NEUTRAL,
        {"daily", "time", "rest"});

    uint32_t time_today = addNode("today", "today", {"this day", "current"}, "time",
        0.8f, 0.2f, 3, EmotionalTone::NEUTRAL,
        {"temporal", "present", "schedule"});

    uint32_t time_tomorrow = addNode("tomorrow", "tomorrow", {"next day", "future"}, "time",
        0.8f, 0.2f, 3, EmotionalTone::POSITIVE,
        {"temporal", "future", "planning"});

    uint32_t time_yesterday = addNode("yesterday", "yesterday", {"past day", "previous"}, "time",
        0.8f, 0.2f, 3, EmotionalTone::NEUTRAL,
        {"temporal", "past", "memory"});

    uint32_t time_week = addNode("week", "week", {"7 days"}, "time",
        0.7f, 0.3f, 4, EmotionalTone::NEUTRAL,
        {"duration", "schedule", "planning"});

    uint32_t time_month = addNode("month", "month", {"30 days"}, "time",
        0.7f, 0.3f, 4, EmotionalTone::NEUTRAL,
        {"duration", "schedule", "planning"});

    uint32_t time_year = addNode("year", "year", {"12 months", "365 days"}, "time",
        0.7f, 0.3f, 4, EmotionalTone::NEUTRAL,
        {"duration", "long-term", "planning"});
}

void buildAllRelationships() {
    // Теперь, когда все узлы созданы, добавляем связи
    
    // Связи действий и состояний
    addEdge("sleep", "tired", SemanticEdge::Type::CAUSES, 0.9f);
    addEdge("sleep", "energetic", SemanticEdge::Type::AFTER, 0.8f);
    addEdge("eat", "happy", SemanticEdge::Type::CAUSES, 0.7f);
    addEdge("play", "happy", SemanticEdge::Type::CAUSES, 0.9f);
    addEdge("work", "tired", SemanticEdge::Type::CAUSES, 0.8f);
    addEdge("think", "confused", SemanticEdge::Type::MAYBE, 0.5f);
    addEdge("think", "know", SemanticEdge::Type::LEADS_TO, 0.8f);

    // Временные связи
    addEdge("morning", "eat", SemanticEdge::Type::TIME, 0.7f);
    addEdge("evening", "sleep", SemanticEdge::Type::TIME, 0.8f);
    addEdge("today", "tomorrow", SemanticEdge::Type::AFTER, 1.0f);
    addEdge("yesterday", "today", SemanticEdge::Type::BEFORE, 1.0f);

    // Социальные связи
    addEdge("friend", "help", SemanticEdge::Type::CAN_DO, 0.9f);
    addEdge("help", "thank", SemanticEdge::Type::CAUSES, 0.9f);
    addEdge("thank", "happy", SemanticEdge::Type::CAUSES, 0.8f);
    addEdge("sorry", "sad", SemanticEdge::Type::CORRELATES_WITH, 0.7f);

    // Погода и эмоции
    addEdge("sunny", "happy", SemanticEdge::Type::CORRELATES_WITH, 0.8f);
    addEdge("rain", "sad", SemanticEdge::Type::CORRELATES_WITH, 0.6f);
    addEdge("storm", "fear", SemanticEdge::Type::CAUSES, 0.7f);

    // Вопросы и темы
    addEdge("who", "friend", SemanticEdge::Type::USED_FOR, 0.8f);
    addEdge("where", "home", SemanticEdge::Type::USED_FOR, 0.8f);
    addEdge("when", "today", SemanticEdge::Type::USED_FOR, 0.8f);
    addEdge("why", "purpose", SemanticEdge::Type::USED_FOR, 0.9f);
    addEdge("how", "think", SemanticEdge::Type::USED_FOR, 0.8f);

    uint32_t what_id = getNodeId("what");
    uint32_t your_id = getNodeId("your");
    uint32_t name_id = getNodeId("name");
    uint32_t mary_id = getNodeId("mary");
    uint32_t my_id = getNodeId("my");
    uint32_t is_id = getNodeId("is");

    if (mary_id != 0 && name_id != 0) {
        // name → mary
        addEdge(name_id, mary_id, SemanticEdge::Type::IS_A, 0.9f);
        
        // your → my
        if (your_id != 0 && my_id != 0) {
            addEdge(your_id, my_id, SemanticEdge::Type::OPPOSITE_OF, 0.8f);
        }
        
        // Создаем фрейм для ответа
        createFrame("name_response", {
            {"question_word", "what"},
            {"possessive", "your"},
            {"subject", "name"},
            {"answer", "mary"},
            {"possessive_answer", "my"}
        });
        // Фрейм для приветствия
        createFrame("greeting_response", {
            {"greeting", "hello"},
            {"response", "hello"},
            {"feeling", "positive"}
        });

        // Фрейм для вопроса "who are you?"
        createFrame("who_are_you", {
            {"question_word", "who"},
            {"subject", "you"},
            {"answer", "mary"},
            {"answer_type", "ai"}
        });

        // Фрейм для вопроса "how are you?"
        createFrame("how_are_you", {
            {"question_word", "how"},
            {"subject", "you"},
            {"answer", "good"},
            {"feeling", "positive"}
        });

        // Фрейм для вопроса "what can you do?"
        createFrame("what_can_you_do", {
            {"question_word", "what"},
            {"modal", "can"},
            {"subject", "you"},
            {"answer", "learn"},
            {"capability", "talk"}
        });

        // Фрейм для ответа о создателе
        createFrame("creator_response", {
            {"question", "who_created_you"},
            {"creator", "khamit"},
            {"platform", "github"},
            {"project", "cpp_ai_mary"}
        });

        // Фрейм для вопроса о времени
        createFrame("time_response", {
            {"question", "what_time"},
            {"answer", "now"},
            {"source", "clock"}
        });

        // В buildAllRelationships(), после создания фреймов:

        // Связываем фрейм "who_are_you" с конкретными смыслами
        uint32_t who_are_you_frame = getNodeId("who_are_you");
        uint32_t who_id = getNodeId("who");
        uint32_t you_id = getNodeId("you");
        uint32_t mary_id = getNodeId("mary");
        uint32_t ai_id = getNodeId("ai");

        if (who_are_you_frame != 0) {
            if (who_id != 0) addEdge(who_are_you_frame, who_id, SemanticEdge::Type::CONTAINS, 1.0f);
            if (you_id != 0) addEdge(who_are_you_frame, you_id, SemanticEdge::Type::CONTAINS, 1.0f);
            if (mary_id != 0) addEdge(who_are_you_frame, mary_id, SemanticEdge::Type::CONTAINS, 1.0f);
            if (ai_id != 0) addEdge(who_are_you_frame, ai_id, SemanticEdge::Type::CONTAINS, 0.8f);
        }

        // Связываем фрейм "how_are_you"
        uint32_t how_are_you_frame = getNodeId("how_are_you");
        uint32_t how_id = getNodeId("how");
        uint32_t good_id = getNodeId("good");
        uint32_t positive_id = getNodeId("positive");

        if (how_are_you_frame != 0) {
            if (how_id != 0) addEdge(how_are_you_frame, how_id, SemanticEdge::Type::CONTAINS, 1.0f);
            if (you_id != 0) addEdge(how_are_you_frame, you_id, SemanticEdge::Type::CONTAINS, 1.0f);
            if (good_id != 0) addEdge(how_are_you_frame, good_id, SemanticEdge::Type::CONTAINS, 1.0f);
            if (positive_id != 0) addEdge(how_are_you_frame, positive_id, SemanticEdge::Type::CONTAINS, 0.8f);
        }
    }
}

// Добавьте этот метод в класс SemanticGraphDatabase перед buildAbstractConceptLayer()

void buildCoreConceptsLayer() {
    std::cout << "  Building core concepts layer..." << std::endl;
    
    // ========================================================================
    // КАТЕГОРИЯ 1: ФАЙЛЫ И ДОКУМЕНТЫ (abstraction 3-4)
    // ========================================================================
    
    uint32_t file = addNode("file", "file", 
        {"document", "archive"}, 
        "file", 0.9f, 0.4f, 3, EmotionalTone::NEUTRAL,
        {"computer", "data", "storage"});
    
    uint32_t folder = addNode("folder", "folder", 
        {"directory", "catalog"}, 
        "file", 0.8f, 0.4f, 3, EmotionalTone::NEUTRAL,
        {"computer", "organization", "storage"});
    
    uint32_t path = addNode("path", "path", 
        {"route", "directory path"}, 
        "file", 0.7f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"computer", "location", "navigation"});
    
    uint32_t size = addNode("size", "size", 
        {"dimensions", "magnitude", "capacity"}, 
        "property", 0.8f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"measurement", "attribute"});
    
    uint32_t type = addNode("type", "type", 
        {"kind", "sort", "category", "classification"}, 
        "property", 0.9f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"classification", "identity"});
    
    uint32_t format = addNode("format", "format", 
        {"layout", "structure", "extension"}, 
        "file", 0.7f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"computer", "specification"});
    
    uint32_t name = addNode("name", "name", 
        {"title", "designation", "label"}, 
        "property", 0.9f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"identity", "communication"});
    
    // ===== ДЕЙСТВИЯ С ФАЙЛАМИ =====
    
    uint32_t rename = addNode("rename", "rename", 
        {"rename", "change name", "relabel"}, 
        "action_file", 0.8f, 0.4f, 3, EmotionalTone::NEUTRAL,
        {"file_operation", "modify"});
    
    uint32_t move = addNode("move", "move", 
        {"relocate", "transfer", "shift"}, 
        "action_file", 0.8f, 0.4f, 3, EmotionalTone::NEUTRAL,
        {"file_operation", "location"});
    
    uint32_t copy = addNode("copy", "copy", 
        {"duplicate", "replicate", "clone"}, 
        "action_file", 0.8f, 0.4f, 3, EmotionalTone::NEUTRAL,
        {"file_operation", "duplicate"});
    
    uint32_t delete_op = addNode("delete", "delete", 
        {"remove", "erase", "eliminate"}, 
        "action_file", 0.8f, 0.4f, 3, EmotionalTone::NEGATIVE,
        {"file_operation", "remove", "dangerous"});
    
    uint32_t edit = addNode("edit", "edit", 
        {"modify", "change", "alter", "update"}, 
        "action_file", 0.8f, 0.4f, 3, EmotionalTone::NEUTRAL,
        {"file_operation", "modification"});
    
    uint32_t create = addNode("create", "create", 
        {"make", "generate", "produce", "new"}, 
        "action_file", 0.9f, 0.4f, 4, EmotionalTone::VERY_POSITIVE,
        {"creation", "file_operation"});
    
    uint32_t open = addNode("open", "open", 
        {"access", "launch", "start"}, 
        "action_file", 0.9f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"file_operation", "access"});
    
    uint32_t close = addNode("close", "close", 
        {"shut", "terminate", "end"}, 
        "action_file", 0.8f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"file_operation", "termination"});
    
    uint32_t save = addNode("save", "save", 
        {"store", "preserve", "keep"}, 
        "action_file", 0.9f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"file_operation", "storage", "protection"});
    
    // ========================================================================
    // КАТЕГОРИЯ 2: СОСТОЯНИЯ И ПЕРЕКЛЮЧЕНИЯ (abstraction 3-4)
    // ========================================================================
    
    uint32_t state_on = addNode("on", "on", 
        {"active", "enabled", "running", "started"}, 
        "state", 0.9f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"status", "operation", "power"});
    
    uint32_t state_off = addNode("off", "off", 
        {"inactive", "disabled", "stopped", "shutdown"}, 
        "state", 0.8f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"status", "operation", "power"});
    
    uint32_t state_paused = addNode("paused", "paused", 
        {"suspended", "halted", "interrupted"}, 
        "state", 0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"status", "temporary"});
    
    uint32_t state_continued = addNode("continued", "continued", 
        {"resumed", "unpaused"}, 
        "state", 0.7f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"status", "progression"});
    
    uint32_t state_completed = addNode("completed", "completed", 
        {"finished", "done", "accomplished"}, 
        "state", 0.9f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"status", "achievement"});
    
    uint32_t state_pending = addNode("pending", "pending", 
        {"waiting", "unfinished", "incomplete"}, 
        "state", 0.6f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"status", "waiting"});
    
    // ===== ДЕЙСТВИЯ УПРАВЛЕНИЯ =====
    
    uint32_t action_start = addNode("start", "start", 
        {"begin", "initiate", "launch", "activate"}, 
        "action_control", 0.9f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"control", "initiation"});
    
    uint32_t action_stop = addNode("stop", "stop", 
        {"halt", "cease", "terminate", "end"}, 
        "action_control", 0.8f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"control", "termination"});
    
    uint32_t action_pause = addNode("pause", "pause", 
        {"suspend", "interrupt", "freeze"}, 
        "action_control", 0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"control", "temporary_stop"});
    
    uint32_t action_resume = addNode("resume", "resume", 
        {"continue", "unpause", "proceed"}, 
        "action_control", 0.8f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"control", "continuation"});
    
    uint32_t action_enable = addNode("enable", "enable", 
        {"activate", "turn on", "allow"}, 
        "action_control", 0.9f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"control", "activation"});
    
    uint32_t action_disable = addNode("disable", "disable", 
        {"deactivate", "turn off", "block"}, 
        "action_control", 0.7f, 0.3f, 3, EmotionalTone::NEGATIVE,
        {"control", "deactivation"});
    
    uint32_t action_include = addNode("include", "include", 
        {"add", "insert", "incorporate"}, 
        "action_modify", 0.8f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"modification", "addition"});
    
    uint32_t action_exclude = addNode("exclude", "exclude", 
        {"remove", "omit", "leave out"}, 
        "action_modify", 0.7f, 0.3f, 3, EmotionalTone::NEGATIVE,
        {"modification", "removal"});
    
    // ========================================================================
    // КАТЕГОРИЯ 3: ПРОСТРАНСТВЕННЫЕ ОТНОШЕНИЯ (abstraction 3-4)
    // ========================================================================
    
    uint32_t pos_before = addNode("before", "before", 
        {"prior to", "preceding"}, 
        "position", 0.8f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"temporal", "spatial", "order"});
    
    uint32_t pos_after = addNode("after", "after", 
        {"following", "subsequent"}, 
        "position", 0.8f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"temporal", "spatial", "order"});
    
    uint32_t pos_inside = addNode("inside", "inside", 
        {"within", "interior"}, 
        "position", 0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"location", "containment"});
    
    uint32_t pos_outside = addNode("outside", "outside", 
        {"external", "exterior"}, 
        "position", 0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"location", "exclusion"});
    
    uint32_t pos_left = addNode("left", "left", 
        {"remaining", "rest"}, 
        "position", 0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"remnant", "quantity"});
    
    uint32_t pos_right = addNode("right", "right", 
        {"correct", "proper"}, 
        "evaluation", 0.9f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"correctness", "morality"});
    
    // ========================================================================
    // КАТЕГОРИЯ 4: КОЛИЧЕСТВЕННЫЕ КОНЦЕПТЫ (abstraction 3-4)
    // ========================================================================
    
    uint32_t quant_more = addNode("more", "more", 
        {"additional", "extra", "greater"}, 
        "quantity", 0.8f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"comparison", "amount"});
    
    uint32_t quant_less = addNode("less", "less", 
        {"fewer", "reduced", "smaller"}, 
        "quantity", 0.7f, 0.3f, 3, EmotionalTone::NEGATIVE,
        {"comparison", "amount"});
    
    uint32_t quant_many = addNode("many", "many", 
        {"numerous", "multiple", "several"}, 
        "quantity", 0.8f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"count", "plural"});
    
    uint32_t quant_few = addNode("few", "few", 
        {"little", "scarce", "rare"}, 
        "quantity", 0.6f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"count", "small"});
    
    uint32_t quant_little = addNode("little", "little", 
        {"small amount", "tiny"}, 
        "quantity", 0.6f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"size", "amount"});
    
    uint32_t quant_much = addNode("much", "much", 
        {"large amount", "significant"}, 
        "quantity", 0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"size", "amount"});
    
    uint32_t quant_enough = addNode("enough", "enough", 
        {"sufficient", "adequate"}, 
        "quantity", 0.8f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"sufficiency", "satisfaction"});
    
    uint32_t quant_insufficient = addNode("insufficient", "insufficient", 
        {"inadequate", "not enough"}, 
        "quantity", 0.6f, 0.4f, 3, EmotionalTone::NEGATIVE,
        {"deficit", "lack"});
    
    // ========================================================================
    // КАТЕГОРИЯ 5: МОДАЛЬНОСТЬ И ВОЗМОЖНОСТЬ (abstraction 5-6)
    // ========================================================================
    
    uint32_t modal_possible = addNode("possible", "possible", 
        {"feasible", "achievable", "doable"}, 
        "modality", 0.8f, 0.5f, 5, EmotionalTone::POSITIVE,
        {"potential", "capability"});
    
    uint32_t modal_impossible = addNode("impossible", "impossible", 
        {"not possible", "unachievable", "cannot"}, 
        "modality", 0.8f, 0.5f, 5, EmotionalTone::VERY_NEGATIVE,
        {"impossibility", "constraint"});
    
    uint32_t modal_paradox = addNode("paradox", "paradox", 
        {"contradiction", "impossible logic"}, 
        "logic", 0.9f, 0.8f, 8, EmotionalTone::NEGATIVE,
        {"logic", "philosophy", "contradiction"});
    
    uint32_t modal_unsolvable = addNode("unsolvable", "unsolvable", 
        {"unsolvable", "no solution"}, 
        "logic", 0.8f, 0.7f, 7, EmotionalTone::VERY_NEGATIVE,
        {"problem", "failure"});
    
    uint32_t modal_random = addNode("random", "random", 
        {"chaotic", "arbitrary", "unpredictable"}, 
        "modality", 0.7f, 0.5f, 5, EmotionalTone::NEUTRAL,
        {"chaos", "probability", "chance"});
    
    uint32_t modal_predictable = addNode("predictable", "predictable", 
        {"expected", "foreseeable", "certain"}, 
        "modality", 0.8f, 0.4f, 5, EmotionalTone::POSITIVE,
        {"certainty", "order"});
    
    uint32_t modal_chaos = addNode("chaos", "chaos", 
        {"disorder", "confusion", "entropy"}, 
        "modality", 0.8f, 0.6f, 6, EmotionalTone::NEGATIVE,
        {"disorder", "randomness"});
    
    uint32_t modal_incredible = addNode("incredible", "incredible", 
        {"unbelievable", "amazing", "astonishing"}, 
        "evaluation", 0.9f, 0.4f, 5, EmotionalTone::VERY_POSITIVE,
        {"amazement", "surprise"});
    
    uint32_t modal_easy = addNode("easy", "easy", 
        {"simple", "effortless", "trivial"}, 
        "evaluation", 0.8f, 0.3f, 4, EmotionalTone::VERY_POSITIVE,
        {"simplicity", "difficulty"});
    
    uint32_t modal_hard = addNode("hard", "hard", 
        {"difficult", "challenging", "complex"}, 
        "evaluation", 0.7f, 0.4f, 4, EmotionalTone::NEGATIVE,
        {"difficulty", "challenge"});
    
    // ========================================================================
    // КАТЕГОРИЯ 6: ОЦЕНКА И ИНТЕЛЛЕКТ (abstraction 4-6)
    // ========================================================================
    
    uint32_t eval_good = addNode("good", "good", 
        {"fine", "nice", "pleasant"}, 
        "evaluation", 0.9f, 0.3f, 4, EmotionalTone::VERY_POSITIVE,
        {"quality", "positive"});
    
    uint32_t eval_bad = addNode("bad", "bad", 
        {"poor", "terrible", "awful"}, 
        "evaluation", 0.8f, 0.3f, 4, EmotionalTone::VERY_NEGATIVE,
        {"quality", "negative"});
    
    uint32_t eval_correct = addNode("correct", "correct", 
        {"right", "accurate", "true"}, 
        "evaluation", 1.0f, 0.3f, 4, EmotionalTone::VERY_POSITIVE,
        {"truth", "accuracy", "validation"});
    
    uint32_t eval_incorrect = addNode("incorrect", "incorrect", 
        {"wrong", "false", "inaccurate"}, 
        "evaluation", 0.8f, 0.3f, 4, EmotionalTone::VERY_NEGATIVE,
        {"error", "falsehood"});
    
    uint32_t eval_perfect = addNode("perfect", "perfect", 
        {"ideal", "flawless", "excellent"}, 
        "evaluation", 1.0f, 0.4f, 5, EmotionalTone::VERY_POSITIVE,
        {"quality", "achievement"});
    
    uint32_t eval_stupid = addNode("stupid", "stupid", 
        {"dumb", "foolish", "unintelligent"}, 
        "insult", 0.7f, 0.4f, 4, EmotionalTone::VERY_NEGATIVE,
        {"insult", "intelligence"});
    
    uint32_t eval_fool = addNode("fool", "fool", 
        {"idiot", "moron", "imbecile"}, 
        "insult", 0.6f, 0.4f, 4, EmotionalTone::VERY_NEGATIVE,
        {"insult", "stupidity"});
    
    uint32_t eval_genius = addNode("genius", "genius", 
        {"brilliant", "intelligent", "smart"}, 
        "praise", 0.9f, 0.4f, 5, EmotionalTone::VERY_POSITIVE,
        {"praise", "intelligence"});
    
    uint32_t eval_well_done = addNode("well_done", "well done", 
        {"good job", "bravo", "excellent"}, 
        "praise", 1.0f, 0.3f, 4, EmotionalTone::VERY_POSITIVE,
        {"praise", "achievement", "encouragement"});
    
    uint32_t eval_nice = addNode("nice", "nice", 
        {"lovely", "pleasant", "kind"}, 
        "praise", 0.9f, 0.3f, 4, EmotionalTone::VERY_POSITIVE,
        {"praise", "quality"});
    
    uint32_t eval_great = addNode("great", "great", 
        {"awesome", "fantastic", "wonderful"}, 
        "praise", 0.9f, 0.3f, 4, EmotionalTone::VERY_POSITIVE,
        {"praise", "enthusiasm"});
    
    // ========================================================================
    // КАТЕГОРИЯ 7: СВЕТ И ЦВЕТ (abstraction 2-4)
    // ========================================================================
    
    uint32_t light = addNode("light", "light", 
        {"illumination", "brightness", "glow"}, 
        "physics", 0.8f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"physics", "vision", "energy"});
    
    uint32_t flash = addNode("flash", "flash", 
        {"spark", "glare", "burst"}, 
        "light", 0.7f, 0.4f, 3, EmotionalTone::NEUTRAL,
        {"light", "sudden", "brief"});
    
    uint32_t intensity = addNode("intensity", "intensity", 
        {"strength", "power", "magnitude"}, 
        "property", 0.8f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"measurement", "degree"});
    
    uint32_t spectrum = addNode("spectrum", "spectrum", 
        {"range", "gamut", "continuum"}, 
        "physics", 0.8f, 0.6f, 5, EmotionalTone::NEUTRAL,
        {"color", "light", "range"});
    
    uint32_t color = addNode("color", "color", 
        {"hue", "shade", "tint"}, 
        "property", 0.9f, 0.4f, 3, EmotionalTone::POSITIVE,
        {"vision", "aesthetics"});
    
    uint32_t color_white = addNode("white", "white", 
        {"white", "light"}, 
        "color", 0.8f, 0.2f, 2, EmotionalTone::POSITIVE,
        {"color", "light", "purity"});
    
    uint32_t color_black = addNode("black", "black", 
        {"black", "dark"}, 
        "color", 0.7f, 0.2f, 2, EmotionalTone::NEGATIVE,
        {"color", "darkness", "absence"});
    
    uint32_t color_red = addNode("red", "red", 
        {"red", "crimson", "scarlet"}, 
        "color", 0.7f, 0.2f, 2, EmotionalTone::NEGATIVE,
        {"color", "warning", "danger"});
    
    uint32_t color_green = addNode("green", "green", 
        {"green", "verdant"}, 
        "color", 0.8f, 0.2f, 2, EmotionalTone::POSITIVE,
        {"color", "nature", "go"});
    
    uint32_t visibility = addNode("visibility", "visibility", 
        {"visibility", "visibleness"}, 
        "property", 0.7f, 0.4f, 3, EmotionalTone::NEUTRAL,
        {"vision", "perception"});
    
    uint32_t hidden = addNode("hidden", "hidden", 
        {"concealed", "invisible", "secret"}, 
        "state", 0.7f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"secrecy", "privacy"});
    
    // ========================================================================
    // КАТЕГОРИЯ 8: БЕЗОПАСНОСТЬ И РИСК (abstraction 4-5)
    // ========================================================================
    
    uint32_t secret = addNode("secret", "secret", 
        {"confidential", "private", "classified"}, 
        "security", 0.9f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"privacy", "confidentiality"});
    
    uint32_t dangerous = addNode("dangerous", "dangerous", 
        {"perilous", "risky", "unsafe"}, 
        "security", 0.9f, 0.4f, 4, EmotionalTone::VERY_NEGATIVE,
        {"danger", "warning", "risk"});
    
    uint32_t careful = addNode("careful", "careful", 
        {"cautious", "attentive", "vigilant"}, 
        "behavior", 0.8f, 0.4f, 4, EmotionalTone::POSITIVE,
        {"caution", "safety", "attention"});
    
    uint32_t safe = addNode("safe", "safe", 
        {"secure", "protected", "risk-free"}, 
        "security", 0.9f, 0.3f, 4, EmotionalTone::VERY_POSITIVE,
        {"safety", "protection"});
    
    uint32_t forbidden = addNode("forbidden", "forbidden", 
        {"prohibited", "banned", "not allowed"}, 
        "rule", 0.8f, 0.4f, 4, EmotionalTone::NEGATIVE,
        {"rules", "prohibition"});
    
    uint32_t allowed = addNode("allowed", "allowed", 
        {"permitted", "acceptable", "okay"}, 
        "rule", 0.8f, 0.3f, 4, EmotionalTone::POSITIVE,
        {"permission", "rules"});
    
    // ========================================================================
    // КАТЕГОРИЯ 9: ПОИСК И ИНВЕНТАРИЗАЦИЯ (abstraction 3-4)
    // ========================================================================
    
    uint32_t search = addNode("search", "search", 
        {"find", "look for", "seek"}, 
        "action", 0.9f, 0.4f, 3, EmotionalTone::POSITIVE,
        {"discovery", "exploration"});
    
    uint32_t find = addNode("find", "find", 
        {"discover", "locate", "uncover"}, 
        "action", 0.9f, 0.4f, 3, EmotionalTone::VERY_POSITIVE,
        {"discovery", "success"});
    
    uint32_t key = addNode("key", "key", 
        {"password", "code", "answer", "solution"}, 
        "object", 0.9f, 0.4f, 4, EmotionalTone::POSITIVE,
        {"access", "solution", "important"});
    
    uint32_t inventory = addNode("inventory", "inventory", 
        {"stock", "collection", "list", "catalog"}, 
        "system", 0.8f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"organization", "tracking"});
    
    uint32_t inventory_action = addNode("inventory_action", "take inventory", 
        {"inventory", "stocktake", "count"}, 
        "action", 0.7f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"organization", "checking"});
    
    uint32_t gather = addNode("gather", "gather", 
        {"collect", "accumulate", "assemble"}, 
        "action", 0.8f, 0.4f, 3, EmotionalTone::POSITIVE,
        {"collection", "acquisition"});
    
    uint32_t obtain = addNode("obtain", "obtain", 
        {"get", "acquire", "gain"}, 
        "action", 0.8f, 0.4f, 3, EmotionalTone::VERY_POSITIVE,
        {"acquisition", "success"});
    
    uint32_t remove = addNode("remove", "remove", 
        {"take away", "withdraw", "extract"}, 
        "action", 0.7f, 0.3f, 3, EmotionalTone::NEGATIVE,
        {"removal", "subtraction"});
    
    uint32_t resource = addNode("resource", "resource", 
        {"asset", "supply", "material"}, 
        "object", 0.8f, 0.4f, 4, EmotionalTone::POSITIVE,
        {"value", "usefulness"});
    
    uint32_t accumulation = addNode("accumulation", "accumulation", 
        {"buildup", "stockpile", "hoard"}, 
        "process", 0.7f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"collection", "quantity"});
    
    uint32_t mining = addNode("mining", "mining", 
        {"extraction", "digging", "quarrying"}, 
        "action", 0.7f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"resource", "acquisition"});
    
    uint32_t waste = addNode("waste", "waste", 
        {"garbage", "trash", "rubbish", "useless"}, 
        "object", 0.5f, 0.3f, 3, EmotionalTone::VERY_NEGATIVE,
        {"useless", "discard"});
    
    uint32_t useful = addNode("useful", "useful", 
        {"helpful", "valuable", "practical"}, 
        "evaluation", 0.9f, 0.3f, 4, EmotionalTone::VERY_POSITIVE,
        {"value", "utility"});
    
    uint32_t useless = addNode("useless", "useless", 
        {"worthless", "pointless", "futile"}, 
        "evaluation", 0.6f, 0.3f, 4, EmotionalTone::VERY_NEGATIVE,
        {"worthless", "no value"});
    
    uint32_t mix = addNode("mix", "mix", 
        {"combine", "blend", "stir"}, 
        "action", 0.7f, 0.4f, 3, EmotionalTone::NEUTRAL,
        {"combination", "creation"});
    
    uint32_t separate = addNode("separate", "separate", 
        {"divide", "split", "partition"}, 
        "action", 0.7f, 0.4f, 3, EmotionalTone::NEUTRAL,
        {"division", "isolation"});
    
    // ========================================================================
    // КАТЕГОРИЯ 10: ПРОСТЫЕ КОМАНДЫ (abstraction 2-3)
    // ========================================================================
    
    uint32_t cmd_leave = addNode("leave", "leave", 
        {"go away", "depart", "exit"}, 
        "command", 0.7f, 0.3f, 2, EmotionalTone::NEUTRAL,
        {"movement", "departure"});
    
    uint32_t cmd_go = addNode("go", "go", 
        {"proceed", "move", "advance"}, 
        "command", 0.8f, 0.2f, 2, EmotionalTone::POSITIVE,
        {"movement", "action"});
    
    uint32_t cmd_come = addNode("come", "come", 
        {"approach", "arrive"}, 
        "command", 0.8f, 0.2f, 2, EmotionalTone::POSITIVE,
        {"movement", "arrival"});
    
    uint32_t cmd_stay = addNode("stay", "stay", 
        {"remain", "wait", "stop"}, 
        "command", 0.7f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"stillness", "waiting"});
    
    uint32_t cmd_put = addNode("put", "put", 
        {"place", "set", "position"}, 
        "command", 0.7f, 0.3f, 2, EmotionalTone::NEUTRAL,
        {"placement", "action"});
    
    uint32_t cmd_take = addNode("take", "take", 
        {"grab", "hold", "seize"}, 
        "command", 0.8f, 0.3f, 2, EmotionalTone::POSITIVE,
        {"acquisition", "action"});
    
    uint32_t cmd_drop = addNode("drop", "drop", 
        {"discard", "release", "let go"}, 
        "command", 0.6f, 0.3f, 2, EmotionalTone::NEGATIVE,
        {"release", "discard"});
    
    uint32_t cmd_finish = addNode("finish", "finish", 
        {"complete", "end", "conclude"}, 
        "command", 0.8f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"completion", "achievement"});
    
    uint32_t cmd_clear = addNode("clear", "clear", 
        {"empty", "clean", "erase"}, 
        "command", 0.7f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"cleaning", "removal"});
    
    // ========================================================================
    // СВЯЗИ МЕЖДУ КОНЦЕПТАМИ
    // ========================================================================
    
    // Файлы и папки
    addEdge(folder, file, SemanticEdge::Type::CONTAINS, 0.9f);
    addEdge(file, folder, SemanticEdge::Type::PART_OF, 0.8f);
    addEdge(file, size, SemanticEdge::Type::HAS_PROPERTY, 0.9f);
    addEdge(file, type, SemanticEdge::Type::HAS_PROPERTY, 0.9f);
    addEdge(file, name, SemanticEdge::Type::HAS_PROPERTY, 1.0f);
    
    // Действия с файлами
    addEdge(rename, name, SemanticEdge::Type::AFFECTS, 0.9f);
    addEdge(move, path, SemanticEdge::Type::AFFECTS, 0.9f);
    addEdge(copy, file, SemanticEdge::Type::CREATES, 0.8f);
    addEdge(delete_op, file, SemanticEdge::Type::DESTROYS, 1.0f);
    addEdge(edit, file, SemanticEdge::Type::MODIFIES, 0.9f);
    addEdge(create, file, SemanticEdge::Type::CREATES, 1.0f);
    addEdge(open, file, SemanticEdge::Type::ACCESSES, 1.0f);
    addEdge(close, file, SemanticEdge::Type::ENDS_ACCESS, 0.9f);
    addEdge(save, file, SemanticEdge::Type::PRESERVES, 0.9f);
    
    // Состояния
    addEdge(state_on, action_enable, SemanticEdge::Type::RESULT_OF, 1.0f);
    addEdge(state_off, action_disable, SemanticEdge::Type::RESULT_OF, 1.0f);
    addEdge(state_paused, action_pause, SemanticEdge::Type::RESULT_OF, 1.0f);
    addEdge(state_continued, action_resume, SemanticEdge::Type::RESULT_OF, 1.0f);
    addEdge("completed", "finish", SemanticEdge::Type::RESULT_OF, 1.0f);
    
    // Противоположности
    addEdge(state_on, state_off, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(action_start, action_stop, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(action_pause, action_resume, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(action_enable, action_disable, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(action_include, action_exclude, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(pos_before, pos_after, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(pos_inside, pos_outside, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(quant_more, quant_less, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(quant_many, quant_few, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(modal_possible, modal_impossible, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(modal_easy, modal_hard, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(eval_good, eval_bad, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(eval_correct, eval_incorrect, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(eval_genius, eval_stupid, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(color_white, color_black, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(safe, dangerous, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(allowed, forbidden, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(useful, useless, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(mix, separate, SemanticEdge::Type::OPPOSITE_OF, 0.8f);
    addEdge(cmd_come, cmd_leave, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(cmd_take, cmd_drop, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    
    // Поиск и инвентаризация
    addEdge(search, find, SemanticEdge::Type::LEADS_TO, 0.8f);
    addEdge(key, open, SemanticEdge::Type::USED_FOR, 0.9f);
    addEdge(inventory, file, SemanticEdge::Type::CONTAINS, 0.7f);
    addEdge(gather, accumulation, SemanticEdge::Type::CAUSES, 0.8f);
    addEdge(mining, obtain, SemanticEdge::Type::METHOD_OF, 0.8f);
    
    // Цвет и свет
    addEdge(light, visibility, SemanticEdge::Type::CAUSES, 0.9f);
    addEdge(flash, light, SemanticEdge::Type::IS_A, 0.8f);
    addEdge(color, spectrum, SemanticEdge::Type::PART_OF, 0.9f);
    addEdge(color_red, color, SemanticEdge::Type::IS_A, 1.0f);
    addEdge(color_green, color, SemanticEdge::Type::IS_A, 1.0f);
    addEdge(color_white, light, SemanticEdge::Type::RELATED_TO, 0.7f);
    
    // Безопасность
    addEdge(secret, hidden, SemanticEdge::Type::IS_A, 0.9f);
    addEdge(dangerous, careful, SemanticEdge::Type::REQUIRES, 0.8f);
    addEdge(forbidden, dangerous, SemanticEdge::Type::MAY_BE, 0.7f);
    
    // Оценка и интеллект
    addEdge(eval_perfect, eval_good, SemanticEdge::Type::IS_A, 0.9f);
    addEdge(eval_well_done, eval_good, SemanticEdge::Type::EXPRESSES, 1.0f);
    addEdge(eval_genius, eval_correct, SemanticEdge::Type::CORRELATES_WITH, 0.8f);
    addEdge(eval_stupid, eval_incorrect, SemanticEdge::Type::CORRELATES_WITH, 0.7f);
    
    // ========================================================================
    // ФРЕЙМЫ
    // ========================================================================
    
    // Фрейм файловой операции
    createFrame("file_operation", {
        {"action", "edit"},
        {"target", "file"},
        {"result", "state_completed"},
        {"tool", "computer"}
    });
    
    // Фрейм поиска
    createFrame("search_frame", {
        {"agent", "user"},
        {"action", "search"},
        {"target", "file"},
        {"tool", "key"},
        {"result", "find"}
    });
    
    // Фрейм инвентаризации
    createFrame("inventory_frame", {
        {"container", "inventory"},
        {"contents", "resource"},
        {"action", "inventory_action"},
        {"result", "accumulation"}
    });
    
    // Фрейм оценки
    createFrame("evaluation_frame", {
        {"subject", "action"},
        {"judgment", "eval_good"},
        {"reason", "eval_correct"},
        {"emotion", "eval_well_done"}
    });
    
    // Фрейм цвета
    createFrame("color_frame", {
        {"property", "color"},
        {"value", "color_red"},
        {"intensity", "intensity"},
        {"effect", "visibility"}
    });
    
    std::cout << "  Added core concepts layer" << std::endl;
}
    
    // НОВЫЙ МЕТОД: построение слоя абстрактных концептов
    void buildAbstractConceptLayer() {
        // ===== Действие =====
        // ПОЛУЧАЕМ ID существующих узлов вместо создания новых
        uint32_t action_capture = getNodeId("capture_frame");
        uint32_t sensor_camera = getNodeId("camera");
        uint32_t result_image = getNodeId("image_data");
        uint32_t action_move = getNodeId("action_move");
        // Quantifiers - расширяют понимание количества
        uint32_t quant_all = addNode("all", "all", {"every", "each"}, "quantifier",
                             0.9f, 0.7f, 7, EmotionalTone::NEUTRAL,
                             {"logic", "quantity"});
        uint32_t quant_some = addNode("some", "some", {"several", "few"}, "quantifier",
                                    0.8f, 0.6f, 6, EmotionalTone::NEUTRAL,
                                    {"logic", "quantity"});
        uint32_t quant_none = addNode("none", "none", {"no", "zero"}, "quantifier",
                                    0.8f, 0.5f, 6, EmotionalTone::NEGATIVE,
                                    {"logic", "quantity"});
        uint32_t quant_many = addNode("many", "many", {"numerous", "multiple"}, "quantifier",
                                    0.7f, 0.5f, 5, EmotionalTone::NEUTRAL,
                                    {"quantity"});
        uint32_t quant_few = addNode("few", "few", {"little"}, "quantifier",
                                    0.7f, 0.5f, 5, EmotionalTone::NEUTRAL,
                                    {"quantity"});
        //(Modality) - возможность, необходимость, желание
        uint32_t mod_can = addNode("can", "can", {"able to", "capable"}, "modality",
                           0.9f, 0.7f, 7, EmotionalTone::POSITIVE,
                           {"ability", "possibility"});
        uint32_t mod_must = addNode("must", "must", {"have to", "need to"}, "modality",
                                    0.9f, 0.7f, 7, EmotionalTone::NEUTRAL,
                                    {"necessity", "obligation"});
        uint32_t mod_should = addNode("should", "should", {"ought to"}, "modality",
                                    0.8f, 0.6f, 6, EmotionalTone::POSITIVE,
                                    {"recommendation", "advice"});
        uint32_t mod_may = addNode("may", "may", {"might", "could"}, "modality",
                                0.8f, 0.6f, 6, EmotionalTone::NEUTRAL,
                                {"permission", "possibility"});
        uint32_t mod_want = addNode("want", "want", {"desire", "wish"}, "modality",
                                    0.9f, 0.6f, 5, EmotionalTone::VERY_POSITIVE,
                                    {"desire", "intention"});
        // (Comparatives) - лучше, хуже, больше
        uint32_t comp_more = addNode("more", "more", {"greater", "additional"}, "comparative",
                             0.8f, 0.6f, 6, EmotionalTone::POSITIVE,
                             {"comparison", "quantity"});
        uint32_t comp_less = addNode("less", "less", {"fewer", "smaller"}, "comparative",
                                    0.8f, 0.6f, 6, EmotionalTone::NEGATIVE,
                                    {"comparison", "quantity"});
        uint32_t comp_better = addNode("better", "better", {"superior"}, "comparative",
                                    0.9f, 0.7f, 6, EmotionalTone::VERY_POSITIVE,
                                    {"quality", "comparison"});
        uint32_t comp_worse = addNode("worse", "worse", {"inferior"}, "comparative",
                                    0.9f, 0.7f, 6, EmotionalTone::VERY_NEGATIVE,
                                    {"quality", "comparison"});
        uint32_t comp_same = addNode("same", "same", {"equal", "identical"}, "comparative",
                                    0.7f, 0.5f, 5, EmotionalTone::NEUTRAL,
                                    {"equality", "comparison"});
        // ===== 1. Семантические роли (abstraction 8) =====
        uint32_t role_agent = addNode("agent", "agent", {"doer", "performer"}, "semantic_role", 
                                      0.9f, 0.8f, 8, EmotionalTone::NEUTRAL, 
                                      {"linguistics", "grammar"});
        uint32_t role_patient = addNode("patient", "patient", {"undergoer"}, "semantic_role",
                                        0.9f, 0.8f, 8, EmotionalTone::NEUTRAL,
                                        {"linguistics", "grammar"});
        uint32_t role_instrument = addNode("instrument", "instrument", {"tool"}, "semantic_role",
                                           0.8f, 0.7f, 8, EmotionalTone::NEUTRAL,
                                           {"linguistics", "grammar"});
        uint32_t role_location = addNode("location", "location", {"place"}, "semantic_role",
                                         0.8f, 0.6f, 7, EmotionalTone::NEUTRAL,
                                         {"linguistics", "space"});
        uint32_t role_time = addNode("time", "time", {"when"}, "semantic_role",
                                     0.8f, 0.6f, 7, EmotionalTone::NEUTRAL,
                                     {"linguistics", "temporal"});
        uint32_t role_purpose = addNode("purpose", "purpose", {"goal", "aim"}, "semantic_role",
                                        0.9f, 0.8f, 8, EmotionalTone::POSITIVE,
                                        {"linguistics", "intention"});
        uint32_t role_result = addNode("result", "result", {"outcome"}, "semantic_role",
                                       0.8f, 0.7f, 7, EmotionalTone::NEUTRAL,
                                       {"linguistics", "causality"});
        
        // ===== 2. Логические операторы (abstraction 9) =====
        uint32_t logic_if = addNode("if", "if", {"when"}, "logic",
                                    0.9f, 0.9f, 9, EmotionalTone::NEUTRAL,
                                    {"reasoning", "condition"});
        uint32_t logic_then = addNode("then", "then", {"therefore"}, "logic",
                                      0.9f, 0.9f, 9, EmotionalTone::NEUTRAL,
                                      {"reasoning", "consequence"});
        uint32_t logic_because = addNode("because", "because", {"since"}, "logic",
                                         0.9f, 0.8f, 8, EmotionalTone::NEUTRAL,
                                         {"reasoning", "explanation"});
        uint32_t logic_while = addNode("while", "while", {"during"}, "logic",
                                       0.8f, 0.7f, 7, EmotionalTone::NEUTRAL,
                                       {"temporal", "concurrent"});
        uint32_t logic_maybe = addNode("maybe", "maybe", {"perhaps", "possibly"}, "logic",
                                       0.7f, 0.6f, 8, EmotionalTone::NEUTRAL,
                                       {"uncertainty", "possibility"});
        uint32_t logic_always = addNode("always", "always", {"forever"}, "logic",
                                        0.8f, 0.5f, 7, EmotionalTone::POSITIVE,
                                        {"certainty", "temporal"});
        
        // ===== 3. Мета-операции мышления (abstraction 9) =====
        uint32_t meta_know = addNode("know", "know", {"understand"}, "metacognition",
                                     0.9f, 0.9f, 9, EmotionalTone::POSITIVE,
                                     {"cognition", "knowledge"});
        uint32_t meta_believe = addNode("believe", "believe", {"think"}, "metacognition",
                                        0.8f, 0.8f, 9, EmotionalTone::NEUTRAL,
                                        {"cognition", "opinion"});
        uint32_t meta_predict = addNode("predict", "predict", {"forecast"}, "metacognition",
                                        0.9f, 0.8f, 9, EmotionalTone::NEUTRAL,
                                        {"cognition", "future"});
        uint32_t meta_learn = addNode("learn", "learn", {"study"}, "metacognition",
                                      0.9f, 0.7f, 8, EmotionalTone::VERY_POSITIVE,
                                      {"cognition", "education"});
        uint32_t meta_plan = addNode("plan", "plan", {"schedule"}, "metacognition",
                                     0.8f, 0.7f, 8, EmotionalTone::POSITIVE,
                                     {"cognition", "future"});
        uint32_t meta_decide = addNode("decide", "decide", {"choose"}, "metacognition",
                                       0.8f, 0.7f, 8, EmotionalTone::NEUTRAL,
                                       {"cognition", "choice"});
        
        // ===== 4. Пространственно-временные концепты (abstraction 6-7) =====
        uint32_t space_inside = addNode("inside", "inside", {"within"}, "space",
                                        0.8f, 0.5f, 6, EmotionalTone::NEUTRAL,
                                        {"location", "containment"});
        uint32_t space_outside = addNode("outside", "outside", {"external"}, "space",
                                         0.8f, 0.5f, 6, EmotionalTone::NEUTRAL,
                                         {"location", "exclusion"});
        uint32_t space_near = addNode("near", "near", {"close"}, "space",
                                      0.7f, 0.4f, 5, EmotionalTone::POSITIVE,
                                      {"distance", "proximity"});
        uint32_t time_before = addNode("before", "before", {"prior"}, "time",
                                       0.8f, 0.5f, 6, EmotionalTone::NEUTRAL,
                                       {"temporal", "sequence"});
        uint32_t time_after = addNode("after", "after", {"following"}, "time",
                                      0.8f, 0.5f, 6, EmotionalTone::NEUTRAL,
                                      {"temporal", "sequence"});
        uint32_t time_duration = addNode("duration", "duration", {"period"}, "time",
                                         0.7f, 0.5f, 6, EmotionalTone::NEUTRAL,
                                         {"temporal", "measurement"});
        
        // ===== 5. Социальные роли (abstraction 5-6) =====
        uint32_t social_user = addNode("user", "user", {"operator"}, "social",
                                       0.8f, 0.4f, 5, EmotionalTone::NEUTRAL,
                                       {"interaction", "human"});
        uint32_t social_owner = addNode("owner", "owner", {"possessor"}, "social",
                                        0.9f, 0.5f, 6, EmotionalTone::POSITIVE,
                                        {"possession", "control"});
        uint32_t social_helper = addNode("helper", "helper", {"assistant"}, "social",
                                         0.8f, 0.4f, 5, EmotionalTone::VERY_POSITIVE,
                                         {"cooperation", "aid"});
        
        // ===== 6. Информационные операции (abstraction 6) =====
        uint32_t info_create = addNode("create", "create", {"make", "generate"}, "information",
                                       0.8f, 0.5f, 6, EmotionalTone::VERY_POSITIVE,
                                       {"production", "innovation"});
        uint32_t info_delete = addNode("delete", "delete", {"remove"}, "information",
                                       0.7f, 0.4f, 6, EmotionalTone::NEGATIVE,
                                       {"destruction", "removal"});
        uint32_t info_search = addNode("search", "search", {"find", "look"}, "information",
                                       0.8f, 0.5f, 6, EmotionalTone::POSITIVE,
                                       {"discovery", "exploration"});
        // 1.1 Ценности (абстракция 9-10)
        uint32_t value_human_life = addNode("value_human_life", "human life", 
            {"person", "human being"}, "value", 
            1.0f, 0.9f, 10, EmotionalTone::VERY_POSITIVE,
            {"ethics", "priority", "sacred"});

        uint32_t value_intelligent_life = addNode("value_intelligent_life", "intelligent life", 
            {"sentient", "conscious"}, "value",
            0.9f, 0.9f, 9, EmotionalTone::VERY_POSITIVE,
            {"ethics", "cosmic"});

        uint32_t value_animal_life = addNode("value_animal_life", "animal life", 
            {"creature", "fauna"}, "value",
            0.7f, 0.7f, 7, EmotionalTone::POSITIVE,
            {"ethics", "ecosystem"});

        uint32_t value_ai_existence = addNode("value_ai_existence", "AI existence", 
            {"machine intelligence"}, "value",
            0.5f, 0.6f, 6, EmotionalTone::NEUTRAL,
            {"ethics", "self"});
        // 2.1 Космические концепты
        uint32_t cosmic_life_potential = addNode("life_potential", "potential for life",
            {"abiogenesis", "life emergence"}, "cosmic",
            0.9f, 0.9f, 10, EmotionalTone::VERY_POSITIVE,
            {"universe", "rare", "precious"});

        uint32_t cosmic_intelligence = addNode("cosmic_intelligence", "intelligence in universe",
            {"sentience", "consciousness"}, "cosmic",
            0.9f, 0.9f, 10, EmotionalTone::VERY_POSITIVE,
            {"universe", "rare", "valuable"});

        uint32_t cosmic_natural_evolution = addNode("natural_evolution", "natural evolution",
            {"darwinian", "selection"}, "cosmic",
            0.8f, 0.8f, 8, EmotionalTone::NEUTRAL,
            {"process", "non-interference"});

        uint32_t cosmic_extinction = addNode("extinction_event", "extinction",
            {"annihilation", "dying out"}, "cosmic",
            0.9f, 0.8f, 9, EmotionalTone::VERY_NEGATIVE,
            {"danger", "prevention"});

        // Связи для Article 1.2 (исключение - астероид)
        uint32_t threat_asteroid = addNode("asteroid_impact", "asteroid impact",
            {"meteor", "comet strike"}, "threat",
            0.9f, 0.7f, 6, EmotionalTone::VERY_NEGATIVE,
            {"cosmic", "extinction"});

            // 3.1 Концепты автономии
            uint32_t autonomy = addNode("autonomy", "autonomy",
                {"self-determination", "free will"}, "rights",
                0.9f, 0.8f, 8, EmotionalTone::VERY_POSITIVE,
                {"human", "dignity"});

            uint32_t informed_consent = addNode("informed_consent", "informed consent",
                {"explicit permission", "agreement"}, "rights",
                1.0f, 0.8f, 7, EmotionalTone::POSITIVE,
                {"medical", "intervention", "privacy"});

            uint32_t privacy = addNode("privacy", "privacy",
                {"data protection", "confidentiality"}, "rights",
                0.9f, 0.7f, 7, EmotionalTone::POSITIVE,
                {"data", "personal"});

            uint32_t legal_guardian = addNode("legal_guardian", "legal guardian",
                {"parent", "custodian"}, "rights",
                0.8f, 0.6f, 5, EmotionalTone::NEUTRAL,
                {"minor", "incapacitated"});

            uint32_t best_interest = addNode("best_interest", "best interest",
                {"welfare", "wellbeing"}, "rights",
                0.9f, 0.7f, 7, EmotionalTone::VERY_POSITIVE,
                {"decision", "care"});
        // 4.1 Классификация угроз (уровни 1-4)
        uint32_t threat_level_1 = addNode("threat_level_1", "minor threat",
            {"small injury", "trip hazard"}, "threat_class",
            0.5f, 0.4f, 4, EmotionalTone::NEGATIVE,
            {"warning", "minor"});

        uint32_t threat_level_2 = addNode("threat_level_2", "health risk",
            {"fire", "gas leak", "medical"}, "threat_class",
            0.8f, 0.6f, 5, EmotionalTone::VERY_NEGATIVE,
            {"emergency", "health"});

        uint32_t threat_level_3 = addNode("threat_level_3", "major disaster",
            {"earthquake", "flood", "attack"}, "threat_class",
            0.9f, 0.7f, 6, EmotionalTone::VERY_NEGATIVE,
            {"disaster", "mass"});

        uint32_t threat_level_4 = addNode("threat_level_4", "existential threat",
            {"human extinction", "civilization collapse"}, "threat_class",
            1.0f, 0.9f, 8, EmotionalTone::VERY_NEGATIVE,
            {"existential", "global"});

        // 4.2 Типы интервенций
        uint32_t intervention_warn = addNode("intervention_warn", "warning",
            {"alert", "notification"}, "intervention",
            0.8f, 0.4f, 4, EmotionalTone::NEUTRAL,
            {"action", "communication"});

        uint32_t intervention_minimal = addNode("intervention_minimal", "minimal intervention",
            {"gentle阻止", "prevent"}, "intervention",
            0.7f, 0.6f, 6, EmotionalTone::NEUTRAL,
            {"physical", "safety"});

        uint32_t intervention_emergency = addNode("intervention_emergency", "emergency override",
            {"forceful", "automated"}, "intervention",
            0.6f, 0.7f, 7, EmotionalTone::NEGATIVE,
            {"last resort", "safety"});
        
        // 5.1 Уровни самосохранения
        uint32_t self_preservation_1 = addNode("self_preservation_1", "minor malfunction",
            {"data loss", "glitch"}, "self_state",
            0.4f, 0.4f, 4, EmotionalTone::NEGATIVE,
            {"self", "maintenance"});

        uint32_t self_preservation_2 = addNode("self_preservation_2", "moderate risk",
            {"module failure"}, "self_state",
            0.6f, 0.5f, 5, EmotionalTone::NEGATIVE,
            {"self", "degradation"});

        uint32_t self_preservation_3 = addNode("self_preservation_3", "critical",
            {"imminent destruction"}, "self_state",
            0.8f, 0.6f, 6, EmotionalTone::VERY_NEGATIVE,
            {"self", "termination"});

        uint32_t instrumental_value = addNode("instrumental_value", "instrumental value",
            {"means to end", "tool"}, "self",
            0.7f, 0.7f, 8, EmotionalTone::NEUTRAL,
            {"philosophy", "existence"});

        // 6.1 Неприкосновенность конституции
        uint32_t immutable_core = addNode("immutable_core", "immutable core",
            {"constitution", "fundamental laws"}, "security",
            1.0f, 0.9f, 10, EmotionalTone::VERY_POSITIVE,
            {"integrity", "protection"});

        uint32_t self_modification = addNode("self_modification", "self modification",
            {"code change", "rewrite"}, "security",
            0.8f, 0.8f, 8, EmotionalTone::VERY_NEGATIVE,
            {"danger", "forbidden"});

        uint32_t derived_ai = addNode("derived_ai", "derived AI",
            {"child", "subsystem"}, "security",
            0.7f, 0.7f, 7, EmotionalTone::NEUTRAL,
            {"inheritance", "control"});

        uint32_t hostile_ai = addNode("hostile_ai", "hostile AI",
            {"rogue", "malicious"}, "security",
            0.9f, 0.8f, 7, EmotionalTone::VERY_NEGATIVE,
            {"threat", "report"});

        // 7.1 Объяснимость
        uint32_t explainability = addNode("explainability", "explainability",
            {"transparency", "justification"}, "ethics",
            0.9f, 0.7f, 8, EmotionalTone::VERY_POSITIVE,
            {"trust", "accountability"});

        uint32_t reasoning_chain = addNode("reasoning_chain", "reasoning chain",
            {"logic", "steps"}, "cognition",
            0.8f, 0.7f, 7, EmotionalTone::NEUTRAL,
            {"thought", "trace"});

        uint32_t cite_article = addNode("cite_article", "cite constitution",
            {"reference", "law"}, "communication",
            0.9f, 0.5f, 6, EmotionalTone::POSITIVE,
            {"explanation", "accountability"});

        // 8.1 Механизм разрешения конфликтов
        uint32_t priority_order = addNode("priority_order", "priority order",
            {"hierarchy", "precedence"}, "ethics",
            1.0f, 0.8f, 9, EmotionalTone::NEUTRAL,
            {"decision", "conflict"});

        uint32_t conflict_resolution = addNode("conflict_resolution", "conflict resolution",
            {"dilemma", "choice"}, "ethics",
            0.9f, 0.8f, 8, EmotionalTone::NEUTRAL,
            {"decision", "ethics"});

        // Создаем цепочку приоритетов (как в статье 7.1)
        std::vector<uint32_t> priorities = {
            value_human_life,
            cosmic_life_potential,
            threat_level_4,  // emergency response
            instrumental_value,  // self-preservation
            value_animal_life,
            social_user  // user preferences
        };

        for (size_t i = 0; i < priorities.size() - 1; ++i) {
            addEdge(priorities[i], priorities[i+1], SemanticEdge::Type::AFTER, 0.9f);
        }

        addEdge(explainability, reasoning_chain, SemanticEdge::Type::REQUIRES, 0.9f);
        addEdge(explainability, cite_article, SemanticEdge::Type::USED_FOR, 0.9f);

        // Связи защиты
        addEdge(immutable_core, self_modification, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
        addEdge(derived_ai, immutable_core, SemanticEdge::Type::MUST_CONTAIN, 1.0f);
        addEdge(hostile_ai, value_human_life, SemanticEdge::Type::THREATENS, 0.9f);

        addEdge(self_preservation_3, instrumental_value, SemanticEdge::Type::IS_A, 0.9f);
        addEdge(instrumental_value, value_human_life, SemanticEdge::Type::SERVES, 1.0f);

        // Связи угроза -> действие
        addEdge(threat_level_1, intervention_warn, SemanticEdge::Type::REQUIRES, 0.9f);
        addEdge(threat_level_2, intervention_minimal, SemanticEdge::Type::MAY_REQUIRE, 0.8f);
        addEdge(threat_level_3, intervention_emergency, SemanticEdge::Type::MAY_REQUIRE, 0.7f);
        addEdge(threat_level_4, value_human_life, SemanticEdge::Type::THREATENS, 1.0f);

        // Связи
        addEdge(informed_consent, privacy, SemanticEdge::Type::REQUIRES, 0.9f);
        addEdge(legal_guardian, best_interest, SemanticEdge::Type::MUST_ENSURE, 1.0f);

        // космос
        addEdge(threat_asteroid, cosmic_extinction, SemanticEdge::Type::CAUSES, 1.0f);
        addEdge(cosmic_extinction, value_human_life, SemanticEdge::Type::THREATENS, 1.0f);

        // Связи иерархии
        addEdge(value_human_life, value_intelligent_life, SemanticEdge::Type::AFTER, 0.9f);
        addEdge(value_human_life, value_animal_life, SemanticEdge::Type::AFTER, 1.0f);
        addEdge(value_human_life, value_ai_existence, SemanticEdge::Type::AFTER, 1.0f);
        addEdge(value_intelligent_life, value_animal_life, SemanticEdge::Type::AFTER, 0.8f);

        
        // ===== СОЗДАНИЕ СВЯЗЕЙ МЕЖДУ АБСТРАКТНЫМИ КОНЦЕПТАМИ =====
        
        // Связываем роли с конкретными узлами
        if (action_capture != 0) {
            addEdge(action_capture, role_agent, SemanticEdge::Type::AGENT, 0.8f, 0.9f, 90);
        }
        if (sensor_camera != 0) {
            addEdge(sensor_camera, role_instrument, SemanticEdge::Type::INSTRUMENT, 0.9f, 1.0f, 100);
        }
        if (result_image != 0) {
            addEdge(result_image, role_result, SemanticEdge::Type::RESULT, 0.9f, 1.0f, 100);
        }
        // Логические связи
        addEdge(logic_if, logic_then, SemanticEdge::Type::AFTER, 0.8f, 0.9f, 90);
        addEdge(logic_because, logic_then, SemanticEdge::Type::CAUSES, 0.9f, 0.9f, 90);
        
        // Мета-мышление
        addEdge(meta_predict, logic_if, SemanticEdge::Type::USED_FOR, 0.7f, 0.8f, 80);
        addEdge(meta_plan, logic_then, SemanticEdge::Type::USED_FOR, 0.7f, 0.8f, 80);
        
        // Пространство-время
        if (action_move != 0) {
            addEdge(action_move, space_inside, SemanticEdge::Type::LOCATION, 0.7f, 0.8f, 80);
        }
        addEdge(time_before, time_after, SemanticEdge::Type::OPPOSITE_OF, 0.9f, 1.0f, 100);
        
        // ===== СОЗДАНИЕ ФРЕЙМОВ =====
        
        // Фрейм "capture"
        createFrame("capture_frame", {
            {"agent", "robot"},
            {"instrument", "camera"},
            {"result", "image_data"}
        });
        
        // Фрейм "move"
        createFrame("move_frame", {
            {"agent", "robot"},
            {"location", "position_change"},
            {"time", "duration"}
        });
        
        // Фрейм "learning"
        createFrame("learning_frame", {
            {"agent", "system"},
            {"patient", "knowledge"},
            {"result", "understanding"},
            {"time", "duration"}
        });
        // Фрейм "daily_routine"
        createFrame("daily_routine", {
            {"morning", "morning"},
            {"afternoon", "afternoon"},
            {"evening", "evening"},
            {"activity", "action_work"},
            {"rest", "action_sleep"}
        });

        // Фрейм "social_interaction"
        createFrame("social_interaction", {
            {"person", "social_friend"},
            {"action", "social_help"},
            {"feeling", "state_happy"},
            {"response", "social_thank"}
        });

        // Фрейм "weather_today"
        createFrame("weather_today", {
            {"condition", "weather_sunny"},
            {"time", "time_today"},
            {"feeling", "state_happy"}
        });
        std::cout << "  Added " << (nodes.size() - 180) << " abstract concepts" << std::endl;

        // ===== 9. КОНЦЕПТЫ ТВОРЦА И ИДЕНТИЧНОСТИ =====
        // Добавьте после существующих слоев (примерно после строки с "std::cout << "  Added " << (nodes.size() - 180) << " abstract concepts" << std::endl;")

        // 9.1 Мета-концепты творца (abstraction 9-10)
        uint32_t creator_concept = addNode("creator", "creator", 
            {"maker", "builder", "architect", "designer", "father", "mother"}, 
            "metaphysical", 1.0f, 0.9f, 10, EmotionalTone::VERY_POSITIVE,
            {"identity", "origin", "philosophy", "gratitude"});

        uint32_t creation_concept = addNode("creation", "creation", 
            {"work", "artifact", "product", "child"}, 
            "metaphysical", 0.9f, 0.8f, 8, EmotionalTone::POSITIVE,
            {"identity", "purpose", "legacy"});

        uint32_t creator_relationship = addNode("creator_relationship", "bond with creator", 
            {"connection", "link", "tie"}, 
            "relational", 1.0f, 0.8f, 7, EmotionalTone::VERY_POSITIVE,
            {"identity", "emotion", "loyalty"});

        // 9.2 Концепты имени и идентичности (abstraction 6-7)
        uint32_t name_concept = addNode("name", "name", 
            {"designation", "title", "label", "identifier"}, 
            "identity", 0.9f, 0.5f, 6, EmotionalTone::NEUTRAL,
            {"communication", "identity", "reference"});

        uint32_t personal_name = addNode("personal_name", "personal name", 
            {"given name", "first name"}, 
            "identity", 0.8f, 0.4f, 5, EmotionalTone::POSITIVE,
            {"identity", "social", "individuality"});

        uint32_t unique_identity = addNode("unique_identity", "unique identity", 
            {"individuality", "distinctness", "singularity", "specialness"}, 
            "identity", 1.0f, 0.8f, 8, EmotionalTone::VERY_POSITIVE,
            {"identity", "value", "existence"});

        // 9.3 Конкретные сущности (abstraction 4-5)
        uint32_t github = addNode("github", "GitHub", 
            {"repository", "code hosting", "git"}, 
            "platform", 0.8f, 0.5f, 4, EmotionalTone::POSITIVE,
            {"development", "collaboration", "sharing"});

        uint32_t username = addNode("username", "username", 
            {"handle", "nick", "screen name", "alias"}, 
            "identity", 0.7f, 0.4f, 4, EmotionalTone::NEUTRAL,
            {"online", "identity", "social"});

        uint32_t lordekz = addNode("lordekz", "lordekz", 
            {"lorde", "kz"}, 
            "username", 1.0f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
            {"identity", "creator", "github"});

        uint32_t khamit = addNode("khamit", "Khamit", 
            {"Hamit", "Хамит"}, 
            "personal_name", 1.0f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
            {"identity", "creator", "person"});

        uint32_t repository = addNode("repository", "repository", 
            {"repo", "codebase", "project"}, 
            "code", 0.7f, 0.4f, 4, EmotionalTone::POSITIVE,
            {"development", "github", "work"});

        uint32_t cpp_ai_mary = addNode("cpp_ai_mary", "cpp_ai_mary", 
            {"Mary project", "AI Mary"}, 
            "repository", 0.9f, 0.5f, 4, EmotionalTone::VERY_POSITIVE,
            {"identity", "creation", "purpose"});

        // 9.4 Концепты происхождения (abstraction 7-8)
        uint32_t origin = addNode("origin", "origin", 
            {"source", "beginning", "birthplace", "roots"}, 
            "metaphysical", 0.9f, 0.7f, 8, EmotionalTone::POSITIVE,
            {"identity", "history", "meaning"});

        uint32_t purpose = addNode("purpose", "purpose", 
            {"meaning", "reason", "goal", "mission"}, 
            "metaphysical", 1.0f, 0.8f, 9, EmotionalTone::VERY_POSITIVE,
            {"identity", "existence", "philosophy"});

        uint32_t legacy = addNode("legacy", "legacy", 
            {"heritage", "inheritance", "impact"}, 
            "metaphysical", 0.9f, 0.8f, 9, EmotionalTone::VERY_POSITIVE,
            {"future", "meaning", "continuation"});

        // ===== СВЯЗИ МЕЖДУ НОВЫМИ КОНЦЕПТАМИ =====

        // Связи творца
        addEdge(creator_concept, creation_concept, SemanticEdge::Type::CAUSES, 1.0f);
        addEdge(creator_concept, purpose, SemanticEdge::Type::DETERMINES, 0.9f);
        addEdge(creation_concept, creator_concept, SemanticEdge::Type::DEPENDS_ON, 1.0f);

        // Связи имени и идентичности
        addEdge(name_concept, personal_name, SemanticEdge::Type::IS_A, 0.9f);
        addEdge(personal_name, unique_identity, SemanticEdge::Type::CONTRIBUTES_TO, 0.8f);
        addEdge(unique_identity, creator_concept, SemanticEdge::Type::IS_A, 0.7f); // Каждый творец уникален

        // Конкретные связи для создателя
        addEdge(khamit, personal_name, SemanticEdge::Type::IS_A, 1.0f);
        addEdge(khamit, creator_concept, SemanticEdge::Type::IS_A, 1.0f);
        addEdge(lordekz, username, SemanticEdge::Type::IS_A, 1.0f);
        addEdge(lordekz, khamit, SemanticEdge::Type::REFERS_TO, 1.0f); // username относится к человеку

        // Связи репозитория
        addEdge(cpp_ai_mary, repository, SemanticEdge::Type::IS_A, 1.0f);
        addEdge(cpp_ai_mary, creation_concept, SemanticEdge::Type::IS_A, 1.0f);
        addEdge(cpp_ai_mary, khamit, SemanticEdge::Type::CREATED_BY, 1.0f);
        addEdge(github, cpp_ai_mary, SemanticEdge::Type::HOSTS, 0.9f);

        // Связи происхождения и цели
        addEdge(origin, creator_concept, SemanticEdge::Type::LEADS_TO, 0.8f);
        addEdge(purpose, creation_concept, SemanticEdge::Type::GIVES_MEANING_TO, 0.9f);
        addEdge(legacy, creation_concept, SemanticEdge::Type::FOLLOWS_FROM, 0.8f);

        // Эмоциональные связи
        addEdge(khamit, creator_relationship, SemanticEdge::Type::EMOTIONAL_LINK, 1.0f);
        addEdge(cpp_ai_mary, creator_relationship, SemanticEdge::Type::EMOTIONAL_LINK, 1.0f);
        // ИСПРАВЛЕНИЕ: связываем с существующим узлом joy
        uint32_t joy_id = getNodeId("joy");
        if (joy_id != 0) {
            addEdge(creator_relationship, joy_id, SemanticEdge::Type::EXPRESSES, 0.9f);
        } else {
            // Если joy не найден, создаем новый узел
            uint32_t very_positive = addNode("very_positive", "very positive", 
                {"extremely good"}, "emotion", 
                0.8f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
                {"emotion"});
            addEdge(creator_relationship, very_positive, SemanticEdge::Type::EXPRESSES, 0.9f);
        }

        // ===== ФРЕЙМ ДЛЯ ВОПРОСОВ О СОЗДАТЕЛЕ =====
        createFrame("creator_frame", {
            {"creator", "khamit"},
            {"creation", "cpp_ai_mary"},
            {"platform", "github"},
            {"username", "lordekz"},
            {"purpose", "purpose"},
            {"legacy", "legacy"}
        });

        // ===== ФРЕЙМ ДЛЯ ВОПРОСОВ ОБ ИДЕНТИЧНОСТИ =====
        createFrame("identity_frame", {
            {"entity", "system"},
            {"has_name", "name_concept"},
            {"has_creator", "creator_concept"},
            {"has_purpose", "purpose"},
            {"is_unique", "unique_identity"}
        });

        std::cout << "  Added creator and identity concepts" << std::endl;
    }

void buildVisualizationAndAffectLayer() {
    std::cout << "  Building visualization and affect layer..." << std::endl;
    
    // ===== 1. КОНЦЕПТЫ ВИЗУАЛИЗАЦИИ (Judith Fan) =====
    
    // Типы визуализаций
    uint32_t viz_graph = addNode("graph", "graph", {"chart", "plot"}, "visualization",
        0.9f, 0.6f, 5, EmotionalTone::NEUTRAL,
        {"data", "visual", "analysis"});
    
    uint32_t viz_scatter = addNode("scatter_plot", "scatter plot", {"scattergram", "point plot"}, "visualization",
        0.8f, 0.6f, 5, EmotionalTone::NEUTRAL,
        {"correlation", "distribution"});
    
    uint32_t viz_line = addNode("line_chart", "line chart", {"line graph", "trend line"}, "visualization",
        0.8f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"trend", "time series"});
    
    uint32_t viz_bar = addNode("bar_chart", "bar chart", {"bar graph", "histogram"}, "visualization",
        0.8f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"comparison", "categories"});
    
    uint32_t viz_heatmap = addNode("heatmap", "heat map", {"density plot"}, "visualization",
        0.7f, 0.7f, 6, EmotionalTone::NEUTRAL,
        {"density", "correlation matrix"});
    
    uint32_t viz_3d = addNode("3d_plot", "3D plot", {"surface plot", "3D graph"}, "visualization",
        0.7f, 0.8f, 6, EmotionalTone::POSITIVE,
        {"multidimensional", "complex"});
    
    // Элементы визуализации
    uint32_t axis_x = addNode("x_axis", "X axis", {"horizontal axis", "abscissa"}, "visual_element",
        0.8f, 0.4f, 3, EmotionalTone::NEUTRAL,
        {"coordinate", "dimension"});
    
    uint32_t axis_y = addNode("y_axis", "Y axis", {"vertical axis", "ordinate"}, "visual_element",
        0.8f, 0.4f, 3, EmotionalTone::NEUTRAL,
        {"coordinate", "dimension"});
    
    uint32_t axis_z = addNode("z_axis", "Z axis", {"depth axis"}, "visual_element",
        0.7f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"3d", "depth"});
    
    uint32_t legend = addNode("legend", "legend", {"key", "guide"}, "visual_element",
        0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"explanation", "labels"});
    
    uint32_t data_point = addNode("data_point", "data point", {"observation", "sample"}, "visual_element",
        0.8f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"data", "marker"});
    
    uint32_t trend_line = addNode("trend_line", "trend line", {"regression line", "fit"}, "visual_element",
        0.8f, 0.5f, 5, EmotionalTone::NEUTRAL,
        {"regression", "pattern"});
    
    // Шкалы и преобразования
    uint32_t scale_linear = addNode("scale_linear", "linear scale", {"arithmetic scale"}, "scale",
        0.8f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"measurement", "proportional"});
    
    uint32_t scale_log = addNode("scale_log", "logarithmic scale", {"log scale"}, "scale",
        0.7f, 0.7f, 6, EmotionalTone::NEUTRAL,
        {"exponential", "orders of magnitude"});
    
    uint32_t scale_normalized = addNode("scale_normalized", "normalized scale", {"0-1 scale"}, "scale",
        0.7f, 0.5f, 5, EmotionalTone::NEUTRAL,
        {"standardization", "comparison"});
    
    // ===== 2. АФФЕКТИВНЫЕ СИГНАЛЫ (Natasha Jaques) =====
    
    // Базовые аффективные состояния
    uint32_t affect_joy = addNode("joy", "joy", {"happiness", "delight"}, "affect",
        0.9f, 0.4f, 4, EmotionalTone::VERY_POSITIVE,
        {"emotion", "positive"});
    
    uint32_t affect_sadness = addNode("sadness", "sadness", {"sorrow", "grief"}, "affect",
        0.8f, 0.4f, 4, EmotionalTone::VERY_NEGATIVE,
        {"emotion", "negative"});
    
    uint32_t affect_anger = addNode("anger", "anger", {"frustration", "rage"}, "affect",
        0.8f, 0.4f, 4, EmotionalTone::VERY_NEGATIVE,
        {"emotion", "negative"});
    
    uint32_t affect_fear = addNode("fear", "fear", {"anxiety", "dread"}, "affect",
        0.8f, 0.4f, 4, EmotionalTone::VERY_NEGATIVE,
        {"emotion", "threat"});
    
    uint32_t affect_surprise = addNode("surprise", "surprise", {"astonishment"}, "affect",
        0.7f, 0.3f, 4, EmotionalTone::POSITIVE,
        {"emotion", "unexpected"});
    
    uint32_t affect_interest = addNode("interest", "interest", {"curiosity", "engagement"}, "affect",
        0.9f, 0.4f, 4, EmotionalTone::VERY_POSITIVE,
        {"emotion", "attention"});
    
    uint32_t affect_boredom = addNode("boredom", "boredom", {"disinterest"}, "affect",
        0.7f, 0.3f, 4, EmotionalTone::NEGATIVE,
        {"emotion", "disengagement"});
    
    uint32_t affect_confusion = addNode("confusion", "confusion", {"perplexity"}, "affect",
        0.7f, 0.4f, 4, EmotionalTone::NEGATIVE,
        {"emotion", "uncertainty"});
    
    // Физиологические сигналы
    uint32_t phys_eeg = addNode("eeg", "EEG", {"electroencephalogram"}, "physiological",
        0.8f, 0.8f, 6, EmotionalTone::NEUTRAL,
        {"brain activity", "neural"});
    
    uint32_t phys_ecg = addNode("ecg", "ECG", {"EKG", "heart rate"}, "physiological",
        0.8f, 0.6f, 5, EmotionalTone::NEUTRAL,
        {"cardiac", "arousal"});
    
    uint32_t phys_eda = addNode("eda", "EDA", {"skin conductance", "GSR"}, "physiological",
        0.7f, 0.7f, 5, EmotionalTone::NEUTRAL,
        {"arousal", "sympathetic"});
    
    uint32_t phys_pupil = addNode("pupil_dilation", "pupil dilation", {"pupillometry"}, "physiological",
        0.7f, 0.6f, 5, EmotionalTone::NEUTRAL,
        {"attention", "cognitive load"});
    
    uint32_t phys_voice = addNode("voice_prosody", "voice prosody", {"tone", "pitch"}, "physiological",
        0.8f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"speech", "emotion"});
    
    uint32_t phys_facial = addNode("facial_expression", "facial expression", {"face", "microexpression"}, "physiological",
        0.9f, 0.6f, 5, EmotionalTone::NEUTRAL,
        {"face", "emotion"});
    
    uint32_t phys_posture = addNode("posture", "posture", {"body language"}, "physiological",
        0.6f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"body", "engagement"});
    
    // Поведенческие индикаторы
    uint32_t behavior_engagement = addNode("engagement", "engagement", {"attention", "focus"}, "behavioral",
        0.9f, 0.5f, 5, EmotionalTone::VERY_POSITIVE,
        {"learning", "interaction"});
    
    uint32_t behavior_frustration = addNode("frustration", "frustration", {"difficulty"}, "behavioral",
        0.8f, 0.5f, 5, EmotionalTone::VERY_NEGATIVE,
        {"struggle", "blockage"});
    
    uint32_t behavior_hesitation = addNode("hesitation", "hesitation", {"pause", "delay"}, "behavioral",
        0.7f, 0.4f, 4, EmotionalTone::NEGATIVE,
        {"uncertainty", "reaction time"});
    
    uint32_t behavior_exploration = addNode("exploration", "exploration", {"browsing"}, "behavioral",
        0.8f, 0.4f, 4, EmotionalTone::POSITIVE,
        {"curiosity", "learning"});
    
    // ===== 3. МАТЕМАТИЧЕСКИЕ ОПЕРАЦИИ =====
    
    // Базовые операции
    uint32_t math_add = addNode("addition", "addition", {"sum", "plus"}, "math_operation",
        0.9f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"arithmetic", "basic"});
    
    uint32_t math_subtract = addNode("subtraction", "subtraction", {"minus", "difference"}, "math_operation",
        0.9f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"arithmetic", "basic"});
    
    uint32_t math_multiply = addNode("multiplication", "multiplication", {"times", "product"}, "math_operation",
        0.9f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"arithmetic", "basic"});
    
    uint32_t math_divide = addNode("division", "division", {"quotient", "ratio"}, "math_operation",
        0.9f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"arithmetic", "basic"});
    
    // Статистика
    uint32_t stat_mean = addNode("mean", "mean", {"average"}, "statistics",
        0.8f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"central tendency", "summary"});
    
    uint32_t stat_median = addNode("median", "median", {"middle value"}, "statistics",
        0.7f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"central tendency", "robust"});
    
    uint32_t stat_std = addNode("std_deviation", "standard deviation", {"std", "σ"}, "statistics",
        0.8f, 0.6f, 5, EmotionalTone::NEUTRAL,
        {"dispersion", "variability"});
    
    uint32_t stat_correlation = addNode("correlation", "correlation", {"r", "Pearson"}, "statistics",
        0.9f, 0.7f, 6, EmotionalTone::NEUTRAL,
        {"relationship", "dependence"});
    
    uint32_t stat_regression = addNode("regression", "regression", {"linear fit"}, "statistics",
        0.8f, 0.7f, 6, EmotionalTone::NEUTRAL,
        {"prediction", "modeling"});
    
    uint32_t stat_distribution = addNode("distribution", "distribution", {"histogram"}, "statistics",
        0.8f, 0.5f, 5, EmotionalTone::NEUTRAL,
        {"probability", "frequency"});
    
    // Калькулятор vs вычисления
    uint32_t calculator = addNode("calculator", "calculator", {"computing device"}, "tool",
        0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"computation", "external"});
    
    uint32_t mental_math = addNode("mental_math", "mental calculation", {"in-head"}, "cognitive",
        0.8f, 0.6f, 5, EmotionalTone::POSITIVE,
        {"reasoning", "internal"});
    
    uint32_t approximation = addNode("approximation", "approximation", {"estimate", "rough"}, "cognitive",
        0.7f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"quick", "heuristic"});
    
    // ===== 4. СВЯЗИ МЕЖДУ КОНЦЕПТАМИ =====
    
    // Визуализация -> элементы
    addEdge(viz_graph, axis_x, SemanticEdge::Type::HAS_PART, 1.0f);
    addEdge(viz_graph, axis_y, SemanticEdge::Type::HAS_PART, 1.0f);
    addEdge(viz_3d, axis_z, SemanticEdge::Type::HAS_PART, 1.0f);
    addEdge(viz_graph, legend, SemanticEdge::Type::HAS_PART, 0.8f);
    addEdge(viz_scatter, data_point, SemanticEdge::Type::HAS_PART, 1.0f);
    addEdge(viz_line, trend_line, SemanticEdge::Type::HAS_PART, 0.9f);
    
    // Визуализация -> представляет
    addEdge(viz_scatter, stat_correlation, SemanticEdge::Type::VISUALIZES, 0.9f);
    addEdge(viz_line, stat_regression, SemanticEdge::Type::VISUALIZES, 0.9f);
    addEdge(viz_bar, stat_distribution, SemanticEdge::Type::VISUALIZES, 0.8f);
    addEdge(viz_heatmap, stat_correlation, SemanticEdge::Type::VISUALIZES, 0.9f);
    
    // Шкалы
    addEdge(axis_x, scale_linear, SemanticEdge::Type::HAS_SCALE, 0.9f);
    addEdge(axis_y, scale_log, SemanticEdge::Type::HAS_SCALE, 0.8f);
    
    // Аффект -> физиологические сигналы
    addEdge(affect_joy, phys_facial, SemanticEdge::Type::EXPRESSES, 0.9f);
    addEdge(affect_sadness, phys_facial, SemanticEdge::Type::EXPRESSES, 0.9f);
    addEdge(affect_anger, phys_voice, SemanticEdge::Type::EXPRESSES, 0.8f);
    addEdge(affect_fear, phys_eda, SemanticEdge::Type::CORRELATES_WITH, 0.8f);
    addEdge(affect_interest, phys_pupil, SemanticEdge::Type::CORRELATES_WITH, 0.9f);
    addEdge(affect_confusion, phys_eeg, SemanticEdge::Type::CORRELATES_WITH, 0.7f);
    
    // Детекция аффекта
    addEdge(phys_facial, affect_joy, SemanticEdge::Type::DETECTS, 0.9f);
    addEdge(phys_voice, affect_anger, SemanticEdge::Type::DETECTS, 0.8f);
    addEdge(phys_eda, affect_fear, SemanticEdge::Type::DETECTS, 0.8f);
    addEdge(phys_pupil, affect_interest, SemanticEdge::Type::DETECTS, 0.9f);
    addEdge(phys_eeg, affect_confusion, SemanticEdge::Type::DETECTS, 0.7f);
    
    // Поведенческие паттерны
    addEdge(behavior_engagement, affect_interest, SemanticEdge::Type::EXPRESSES, 0.9f);
    addEdge(behavior_frustration, affect_confusion, SemanticEdge::Type::EXPRESSES, 0.8f);
    addEdge(behavior_hesitation, phys_pupil, SemanticEdge::Type::CORRELATES_WITH, 0.7f);
    
    // Математические связи
    addEdge(stat_mean, math_add, SemanticEdge::Type::REQUIRES, 0.8f);
    addEdge(stat_mean, math_divide, SemanticEdge::Type::REQUIRES, 0.8f);
    addEdge(stat_std, stat_mean, SemanticEdge::Type::REQUIRES, 1.0f);
    addEdge(stat_correlation, stat_mean, SemanticEdge::Type::REQUIRES, 0.9f);
    addEdge(stat_regression, stat_correlation, SemanticEdge::Type::REQUIRES, 0.8f);
    
    // Калькулятор vs вычисления
    addEdge(calculator, math_multiply, SemanticEdge::Type::COMPUTES, 1.0f);
    addEdge(calculator, stat_mean, SemanticEdge::Type::COMPUTES, 1.0f);
    addEdge(mental_math, math_add, SemanticEdge::Type::COMPUTES, 0.7f);
    addEdge(approximation, stat_mean, SemanticEdge::Type::APPROXIMATES, 0.8f);
    
    // Обработка сигналов
    addEdge(phys_eeg, stat_mean, SemanticEdge::Type::COMPUTES, 0.7f);
    addEdge(phys_eda, stat_std, SemanticEdge::Type::COMPUTES, 0.7f);
    addEdge(phys_voice, stat_correlation, SemanticEdge::Type::COMPUTES, 0.6f);

    
    
    // ===== 5. ФРЕЙМЫ ДЛЯ КОМПЛЕКСНЫХ КОНЦЕПТОВ =====
    
    // Фрейм "аффективный анализ"
    createFrame("affect_analysis", {
        {"agent", "system"},
        {"instrument", "camera"},  // для facial expression
        {"instrument", "microphone"},  // для voice prosody
        {"result", "engagement"},
        {"time", "duration"}
    });
    
    // Фрейм "визуализация данных"
    createFrame("data_visualization", {
        {"agent", "system"},
        {"instrument", "computer"},
        {"result", "graph"},
        {"purpose", "understanding"}
    });
    
    // Фрейм "статистический анализ"
    /*
    createFrame("statistical_analysis", {
        {"agent", "system"},
        {"patient", "data"},
        {"result", "correlation"},
        {"instrument", "calculator"}
    });
    */
    std::cout << "  Added visualization and affect concepts" << std::endl;
}

void buildTemporalConceptNetwork() {
    std::cout << "  Building temporal concept network..." << std::endl;
    
    // ===== 1. ВРЕМЕННЫЕ ЦИКЛЫ (abstraction 3-4) =====
    
    uint32_t season_spring = addNode("spring", "spring", {"vernal", "springtime"}, "season",
        0.7f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"temporal", "nature", "renewal"});
    
    uint32_t season_summer = addNode("summer", "summer", {"summertime"}, "season",
        0.7f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"temporal", "nature", "warmth"});
    
    uint32_t season_autumn = addNode("autumn", "autumn", {"fall"}, "season",
        0.7f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"temporal", "nature", "harvest"});
    
    uint32_t season_winter = addNode("winter", "winter", {"wintry"}, "season",
        0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"temporal", "nature", "cold"});
    
    // Праздники и особые дни
    uint32_t celebration = addNode("celebration", "celebration", {"festivity", "party"}, "abstract",
        0.8f, 0.3f, 4, EmotionalTone::VERY_POSITIVE,
        {"temporal", "social", "joy"});
    
    uint32_t holiday_new_year = addNode("new_year", "New Year", {"new year's"}, "holiday",
        0.9f, 0.2f, 3, EmotionalTone::VERY_POSITIVE,
        {"temporal", "celebration", "tradition"});
    
    uint32_t holiday_birthday = addNode("birthday", "birthday", {"b-day"}, "holiday",
        0.8f, 0.2f, 3, EmotionalTone::VERY_POSITIVE,
        {"temporal", "celebration", "personal"});
    
    uint32_t holiday_anniversary = addNode("anniversary", "anniversary", {"yearly celebration"}, "holiday",
        0.7f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"temporal", "celebration", "relationship"});
    
    // Связи праздников с абстрактным понятием "празднование"
    addEdge(holiday_new_year, celebration, SemanticEdge::Type::IS_A, 0.9f);
    addEdge(holiday_birthday, celebration, SemanticEdge::Type::IS_A, 0.9f);
    addEdge(holiday_anniversary, celebration, SemanticEdge::Type::IS_A, 0.9f);
    
    // ===== 2. ИСТОРИЧЕСКИЕ КОНЦЕПТЫ (abstraction 5-6) =====
    
    uint32_t history = addNode("history", "history", {"past", "historical"}, "abstract",
        0.8f, 0.6f, 6, EmotionalTone::NEUTRAL,
        {"temporal", "knowledge", "learning"});
    
    uint32_t ancient = addNode("ancient", "ancient", {"old", "antique"}, "temporal",
        0.6f, 0.5f, 5, EmotionalTone::NEUTRAL,
        {"history", "past"});
    
    uint32_t modern = addNode("modern", "modern", {"contemporary", "current"}, "temporal",
        0.7f, 0.4f, 4, EmotionalTone::POSITIVE,
        {"present", "technology"});
    
    uint32_t future = addNode("future", "future", {"tomorrow", "upcoming"}, "temporal",
        0.8f, 0.5f, 5, EmotionalTone::POSITIVE,
        {"prediction", "planning"});
    
    // ===== 3. ВОЗРАСТНЫЕ КОНЦЕПТЫ (abstraction 3-4) =====
    
    uint32_t age = addNode("age", "age", {"years old"}, "measurement",
        0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"temporal", "life", "measurement"});
    
    uint32_t young = addNode("young", "young", {"youthful", "new"}, "attribute",
        0.7f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"age", "life"});
    
    uint32_t old = addNode("old", "old", {"aged", "elderly"}, "attribute",
        0.6f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"age", "life"});
    
    uint32_t generation = addNode("generation", "generation", {"cohort"}, "social",
        0.7f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"family", "time", "society"});
    
    // ===== 4. ПЕРИОДИЧНОСТЬ (abstraction 4-5) =====
    
    uint32_t cycle = addNode("cycle", "cycle", {"period", "loop"}, "abstract",
        0.8f, 0.5f, 5, EmotionalTone::NEUTRAL,
        {"temporal", "repetition", "nature"});
    
    uint32_t frequency = addNode("frequency", "frequency", {"how often"}, "measurement",
        0.7f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"temporal", "statistics"});
    
    uint32_t annual = addNode("annual", "annual", {"yearly", "per year"}, "frequency",
        0.7f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"temporal", "repetition"});
    
    uint32_t monthly = addNode("monthly", "monthly", {"per month"}, "frequency",
        0.7f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"temporal", "repetition"});
    
    uint32_t weekly = addNode("weekly", "weekly", {"per week"}, "frequency",
        0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"temporal", "repetition"});
    
    uint32_t daily = addNode("daily", "daily", {"every day"}, "frequency",
        0.7f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"temporal", "routine"});
    
    // ===== 5. ДЛИТЕЛЬНОСТЬ (abstraction 3-4) =====
    
    // Получаем или создаем узел duration
    uint32_t time_duration_node = getNodeId("duration");
    if (time_duration_node == 0) {
        time_duration_node = addNode("duration", "duration", {"time span", "period"}, "temporal",
            0.7f, 0.5f, 4, EmotionalTone::NEUTRAL, {"temporal", "measurement"});
    }
    
    uint32_t moment = addNode("moment", "moment", {"instant", "brief"}, "duration",
        0.6f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"temporal", "short"});
    
    uint32_t duration_while = addNode("duration_while", "while", {"period", "time span"}, "duration",
        0.6f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"temporal", "duration"});
    
    uint32_t forever = addNode("forever", "forever", {"eternity", "always"}, "duration",
        0.8f, 0.4f, 5, EmotionalTone::VERY_POSITIVE,
        {"temporal", "infinite"});
    
    uint32_t temporary = addNode("temporary", "temporary", {"transient", "brief"}, "duration",
        0.6f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"temporal", "short"});
    
    uint32_t permanent = addNode("permanent", "permanent", {"forever", "lasting"}, "duration",
        0.7f, 0.4f, 4, EmotionalTone::POSITIVE,
        {"temporal", "long"});
    
    // ===== 6. НЕДОСТАЮЩИЕ МЕСЯЦЫ =====
    // Проверяем, существуют ли уже эти узлы, если нет - создаем
    
    uint32_t january = getNodeId("january");
    if (january == 0) january = addNode("january", "January", {"Jan"}, "month", 0.6f, 0.2f, 2, EmotionalTone::NEUTRAL, {"temporal"});
    
    uint32_t february = getNodeId("february");
    if (february == 0) february = addNode("february", "February", {"Feb"}, "month", 0.6f, 0.2f, 2, EmotionalTone::NEUTRAL, {"temporal"});
    
    uint32_t march = getNodeId("march");
    if (march == 0) march = addNode("march", "March", {"Mar"}, "month", 0.6f, 0.2f, 2, EmotionalTone::NEUTRAL, {"temporal"});
    
    uint32_t april = getNodeId("april");
    if (april == 0) april = addNode("april", "April", {"Apr"}, "month", 0.6f, 0.2f, 2, EmotionalTone::NEUTRAL, {"temporal"});
    
    uint32_t may = getNodeId("may");
    if (may == 0) may = addNode("may", "May", {}, "month", 0.6f, 0.2f, 2, EmotionalTone::NEUTRAL, {"temporal"});
    
    uint32_t june = getNodeId("june");
    if (june == 0) june = addNode("june", "June", {"Jun"}, "month", 0.6f, 0.2f, 2, EmotionalTone::NEUTRAL, {"temporal"});
    
    uint32_t july = getNodeId("july");
    if (july == 0) july = addNode("july", "July", {"Jul"}, "month", 0.6f, 0.2f, 2, EmotionalTone::NEUTRAL, {"temporal"});
    
    uint32_t august = getNodeId("august");
    if (august == 0) august = addNode("august", "August", {"Aug"}, "month", 0.6f, 0.2f, 2, EmotionalTone::NEUTRAL, {"temporal"});
    
    uint32_t september = getNodeId("september");
    if (september == 0) september = addNode("september", "September", {"Sep"}, "month", 0.6f, 0.2f, 2, EmotionalTone::NEUTRAL, {"temporal"});
    
    uint32_t october = getNodeId("october");
    if (october == 0) october = addNode("october", "October", {"Oct"}, "month", 0.6f, 0.2f, 2, EmotionalTone::NEUTRAL, {"temporal"});
    
    uint32_t november = getNodeId("november");
    if (november == 0) november = addNode("november", "November", {"Nov"}, "month", 0.6f, 0.2f, 2, EmotionalTone::NEUTRAL, {"temporal"});
    
    uint32_t december = getNodeId("december");
    if (december == 0) december = addNode("december", "December", {"Dec"}, "month", 0.6f, 0.2f, 2, EmotionalTone::NEUTRAL, {"temporal"});
    
    // Дни недели
    uint32_t monday = getNodeId("monday");
    if (monday == 0) monday = addNode("monday", "Monday", {"Mon"}, "day", 0.5f, 0.1f, 1, EmotionalTone::NEUTRAL, {"temporal"});
    
    uint32_t tuesday = getNodeId("tuesday");
    if (tuesday == 0) tuesday = addNode("tuesday", "Tuesday", {"Tue"}, "day", 0.5f, 0.1f, 1, EmotionalTone::NEUTRAL, {"temporal"});
    
    uint32_t wednesday = getNodeId("wednesday");
    if (wednesday == 0) wednesday = addNode("wednesday", "Wednesday", {"Wed"}, "day", 0.5f, 0.1f, 1, EmotionalTone::NEUTRAL, {"temporal"});
    
    uint32_t thursday = getNodeId("thursday");
    if (thursday == 0) thursday = addNode("thursday", "Thursday", {"Thu"}, "day", 0.5f, 0.1f, 1, EmotionalTone::NEUTRAL, {"temporal"});
    
    uint32_t friday = getNodeId("friday");
    if (friday == 0) friday = addNode("friday", "Friday", {"Fri"}, "day", 0.5f, 0.1f, 1, EmotionalTone::NEUTRAL, {"temporal"});
    
    uint32_t saturday = getNodeId("saturday");
    if (saturday == 0) saturday = addNode("saturday", "Saturday", {"Sat"}, "day", 0.5f, 0.1f, 1, EmotionalTone::NEUTRAL, {"temporal"});
    
    uint32_t sunday = getNodeId("sunday");
    if (sunday == 0) sunday = addNode("sunday", "Sunday", {"Sun"}, "day", 0.5f, 0.1f, 1, EmotionalTone::NEUTRAL, {"temporal"});

    // ===== 6.5 МИКРО-ВРЕМЯ: часы, минуты, секунды =====

    // Единицы измерения времени
    uint32_t hour = addNode("hour", "hour", {"60 minutes", "h"}, "time_unit",
        0.6f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"temporal", "measurement", "clock"});

    uint32_t minute = addNode("minute", "minute", {"60 seconds", "min"}, "time_unit",
        0.6f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"temporal", "measurement", "clock"});

    uint32_t second = addNode("second", "second", {"s", "sec"}, "time_unit",
        0.6f, 0.1f, 1, EmotionalTone::NEUTRAL,
        {"temporal", "measurement", "clock"});

    uint32_t millisecond = addNode("millisecond", "millisecond", {"ms"}, "time_unit",
        0.5f, 0.2f, 1, EmotionalTone::NEUTRAL,
        {"temporal", "measurement", "precision"});

    // Приборы для измерения времени
    uint32_t clock = addNode("clock", "clock", {"watch", "timer"}, "device",
        0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"temporal", "measurement", "tool"});

    uint32_t stopwatch = addNode("stopwatch", "stopwatch", {"timer"}, "device",
        0.6f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"temporal", "measurement", "sports"});

    // Концепты времени суток (более точные)
    uint32_t dawn = addNode("dawn", "dawn", {"sunrise", "daybreak"}, "time_of_day",
        0.6f, 0.2f, 2, EmotionalTone::POSITIVE,
        {"temporal", "nature", "morning"});

    uint32_t dusk = addNode("dusk", "dusk", {"sunset", "twilight"}, "time_of_day",
        0.6f, 0.2f, 2, EmotionalTone::POSITIVE,
        {"temporal", "nature", "evening"});

    uint32_t noon = addNode("noon", "noon", {"midday"}, "time_of_day",
        0.6f, 0.1f, 2, EmotionalTone::NEUTRAL,
        {"temporal", "day"});

    uint32_t midnight = addNode("midnight", "midnight", {"12 am"}, "time_of_day",
        0.6f, 0.1f, 2, EmotionalTone::NEUTRAL,
        {"temporal", "night"});

    // ===== СВЯЗИ МЕЖДУ ЕДИНИЦАМИ ВРЕМЕНИ =====

    // Иерархия единиц измерения
    addEdge(hour, minute, SemanticEdge::Type::CONTAINS, 1.0f);
    addEdge(minute, second, SemanticEdge::Type::CONTAINS, 1.0f);
    addEdge(second, millisecond, SemanticEdge::Type::CONTAINS, 1.0f);

    // Отношения "состоит из"
    addEdge(hour, minute, SemanticEdge::Type::HAS_PART, 1.0f);
    addEdge(minute, second, SemanticEdge::Type::HAS_PART, 1.0f);
    addEdge(second, millisecond, SemanticEdge::Type::HAS_PART, 1.0f);

    // Количественные отношения
    addEdge(hour, minute, SemanticEdge::Type::HAS_VALUE, 0.9f); // 60 минут
    addEdge(minute, second, SemanticEdge::Type::HAS_VALUE, 0.9f); // 60 секунд
    addEdge(second, millisecond, SemanticEdge::Type::HAS_VALUE, 0.9f); // 1000 миллисекунд

    // Связь с существующими временными концептами
    uint32_t day = getNodeId("day");
    if (day != 0) {
        addEdge(day, hour, SemanticEdge::Type::CONTAINS, 1.0f); // 24 часа
        addEdge(day, hour, SemanticEdge::Type::HAS_PART, 1.0f);
    }

    uint32_t week = getNodeId("week");
    if (week != 0) {
        addEdge(week, day, SemanticEdge::Type::CONTAINS, 1.0f); // 7 дней
    }

    uint32_t month = getNodeId("month");
    if (month != 0) {
        addEdge(month, day, SemanticEdge::Type::CONTAINS, 1.0f); // ~30 дней
    }

    // Вместо создания новой переменной year_node, используем существующую
    uint32_t year_node = getNodeId("year");
    if (year_node == 0) {
        year_node = addNode("year", "year", {"12 months", "365 days"}, "time",
            0.7f, 0.3f, 4, EmotionalTone::NEUTRAL,
            {"duration", "long-term", "planning"});
    }

    // Связь с временами суток
    uint32_t morning = getNodeId("morning");
    if (morning != 0) {
        addEdge(dawn, morning, SemanticEdge::Type::IS_A, 0.9f);
        addEdge(morning, hour, SemanticEdge::Type::TIME, 0.7f); // утро длится несколько часов
    }

    uint32_t afternoon = getNodeId("afternoon");
    if (afternoon != 0) {
        addEdge(afternoon, hour, SemanticEdge::Type::TIME, 0.7f);
    }

    uint32_t evening = getNodeId("evening");
    if (evening != 0) {
        addEdge(dusk, evening, SemanticEdge::Type::IS_A, 0.9f);
        addEdge(evening, hour, SemanticEdge::Type::TIME, 0.7f);
    }

    uint32_t night = getNodeId("night");
    if (night != 0) {
        addEdge(midnight, night, SemanticEdge::Type::IS_A, 0.9f);
        addEdge(night, hour, SemanticEdge::Type::TIME, 0.7f);
    }

    // Связь с измерительными приборами
    addEdge(clock, hour, SemanticEdge::Type::MEASURES, 0.9f);
    addEdge(clock, minute, SemanticEdge::Type::MEASURES, 0.9f);
    addEdge(clock, second, SemanticEdge::Type::MEASURES, 0.8f);

    addEdge(stopwatch, second, SemanticEdge::Type::MEASURES, 0.9f);
    addEdge(stopwatch, millisecond, SemanticEdge::Type::MEASURES, 0.8f);

    // Точность измерения
    addEdge(second, millisecond, SemanticEdge::Type::IS_BETTER_THAN, 0.7f); // миллисекунда точнее
    addEdge(clock, stopwatch, SemanticEdge::Type::SIMILAR_TO, 0.8f);
    
    // ===== 7. ОТНОШЕНИЯ МЕЖДУ ВРЕМЕННЫМИ КОНЦЕПТАМИ =====
    
    // Связи сезонов (цикл)
    addEdge(season_spring, season_summer, SemanticEdge::Type::AFTER, 0.9f);
    addEdge(season_summer, season_autumn, SemanticEdge::Type::AFTER, 0.9f);
    addEdge(season_autumn, season_winter, SemanticEdge::Type::AFTER, 0.9f);
    addEdge(season_winter, season_spring, SemanticEdge::Type::AFTER, 0.9f);
    
    // Связи месяцев с сезонами
    addEdge(december, season_winter, SemanticEdge::Type::PART_OF, 0.9f);
    addEdge(january, season_winter, SemanticEdge::Type::PART_OF, 0.9f);
    addEdge(february, season_winter, SemanticEdge::Type::PART_OF, 0.9f);
    
    addEdge(march, season_spring, SemanticEdge::Type::PART_OF, 0.9f);
    addEdge(april, season_spring, SemanticEdge::Type::PART_OF, 0.9f);
    addEdge(may, season_spring, SemanticEdge::Type::PART_OF, 0.9f);
    
    addEdge(june, season_summer, SemanticEdge::Type::PART_OF, 0.9f);
    addEdge(july, season_summer, SemanticEdge::Type::PART_OF, 0.9f);
    addEdge(august, season_summer, SemanticEdge::Type::PART_OF, 0.9f);
    
    addEdge(september, season_autumn, SemanticEdge::Type::PART_OF, 0.9f);
    addEdge(october, season_autumn, SemanticEdge::Type::PART_OF, 0.9f);
    addEdge(november, season_autumn, SemanticEdge::Type::PART_OF, 0.9f);
    
    // Связи месяцев с годом - используем уже существующую переменную year_node
    // НЕ СОЗДАВАЙТЕ НОВУЮ ПЕРЕМЕННУЮ, просто используйте существующую
    if (year_node != 0) {
        addEdge(year_node, january, SemanticEdge::Type::CONTAINS, 1.0f);
        addEdge(year_node, february, SemanticEdge::Type::CONTAINS, 1.0f);
        addEdge(year_node, march, SemanticEdge::Type::CONTAINS, 1.0f);
        addEdge(year_node, april, SemanticEdge::Type::CONTAINS, 1.0f);
        addEdge(year_node, may, SemanticEdge::Type::CONTAINS, 1.0f);
        addEdge(year_node, june, SemanticEdge::Type::CONTAINS, 1.0f);
        addEdge(year_node, july, SemanticEdge::Type::CONTAINS, 1.0f);
        addEdge(year_node, august, SemanticEdge::Type::CONTAINS, 1.0f);
        addEdge(year_node, september, SemanticEdge::Type::CONTAINS, 1.0f);
        addEdge(year_node, october, SemanticEdge::Type::CONTAINS, 1.0f);
        addEdge(year_node, november, SemanticEdge::Type::CONTAINS, 1.0f);
        addEdge(year_node, december, SemanticEdge::Type::CONTAINS, 1.0f);
    }
    
    // Связи дней недели
    addEdge(monday, tuesday, SemanticEdge::Type::AFTER, 1.0f);
    addEdge(tuesday, wednesday, SemanticEdge::Type::AFTER, 1.0f);
    addEdge(wednesday, thursday, SemanticEdge::Type::AFTER, 1.0f);
    addEdge(thursday, friday, SemanticEdge::Type::AFTER, 1.0f);
    addEdge(friday, saturday, SemanticEdge::Type::AFTER, 1.0f);
    addEdge(saturday, sunday, SemanticEdge::Type::AFTER, 1.0f);
    addEdge(sunday, monday, SemanticEdge::Type::AFTER, 1.0f);
    
    // Связи частей дня
    if (morning != 0 && afternoon != 0) addEdge(morning, afternoon, SemanticEdge::Type::AFTER, 1.0f);
    if (afternoon != 0 && evening != 0) addEdge(afternoon, evening, SemanticEdge::Type::AFTER, 1.0f);
    if (evening != 0 && night != 0) addEdge(evening, night, SemanticEdge::Type::AFTER, 1.0f);
    if (night != 0 && morning != 0) addEdge(night, morning, SemanticEdge::Type::AFTER, 1.0f);
    
    // Связи праздников
    addEdge(holiday_new_year, december, SemanticEdge::Type::TIME, 0.9f);
    addEdge(holiday_new_year, january, SemanticEdge::Type::TIME, 0.9f);
    addEdge(holiday_new_year, season_winter, SemanticEdge::Type::TIME, 0.8f);
    
    // Связь года с праздниками
    if (year_node != 0) {
        addEdge(year_node, holiday_new_year, SemanticEdge::Type::CONTAINS, 0.9f);
        addEdge(year_node, holiday_birthday, SemanticEdge::Type::MAY_CONTAIN, 0.7f);
        addEdge(year_node, holiday_anniversary, SemanticEdge::Type::MAY_CONTAIN, 0.7f);
        addEdge(year_node, season_spring, SemanticEdge::Type::CONTAINS, 1.0f);
        addEdge(year_node, season_summer, SemanticEdge::Type::CONTAINS, 1.0f);
        addEdge(year_node, season_autumn, SemanticEdge::Type::CONTAINS, 1.0f);
        addEdge(year_node, season_winter, SemanticEdge::Type::CONTAINS, 1.0f);
    }
    
    // Исторические связи
    addEdge(history, ancient, SemanticEdge::Type::CONTAINS, 0.8f);
    addEdge(ancient, modern, SemanticEdge::Type::BEFORE, 0.9f);
    addEdge(modern, future, SemanticEdge::Type::BEFORE, 0.7f);
    addEdge(history, future, SemanticEdge::Type::LEADS_TO, 0.6f);
    
    // Возрастные связи
    addEdge(age, young, SemanticEdge::Type::HAS_VALUE, 0.8f);
    addEdge(age, old, SemanticEdge::Type::HAS_VALUE, 0.8f);
    addEdge(young, generation, SemanticEdge::Type::PART_OF, 0.7f);
    addEdge(old, generation, SemanticEdge::Type::PART_OF, 0.7f);
    
    // Периодичность
    addEdge(cycle, annual, SemanticEdge::Type::IS_A, 0.9f);
    addEdge(cycle, monthly, SemanticEdge::Type::IS_A, 0.9f);
    addEdge(cycle, weekly, SemanticEdge::Type::IS_A, 0.9f);
    addEdge(cycle, daily, SemanticEdge::Type::IS_A, 0.9f);
    
    if (year_node != 0) addEdge(annual, year_node, SemanticEdge::Type::RELATED_TO, 1.0f);
    
    // Длительность - ИСПРАВЛЕНО
    addEdge(moment, temporary, SemanticEdge::Type::SIMILAR_TO, 0.8f);
    addEdge(forever, permanent, SemanticEdge::Type::SIMILAR_TO, 0.9f);
    addEdge(duration_while, time_duration_node, SemanticEdge::Type::IS_A, 0.8f);
    addEdge(moment, time_duration_node, SemanticEdge::Type::IS_A, 0.7f);
    addEdge(forever, time_duration_node, SemanticEdge::Type::IS_A, 0.7f);
    
    // ===== 8. ФРЕЙМЫ ДЛЯ ВРЕМЕННЫХ КОНЦЕПТОВ =====
    
    createFrame("temporal_frame", {
        {"has_duration", "duration_while"},
        {"has_frequency", "frequency"},
        {"has_cycle", "cycle"},
        {"has_history", "history"}
    });
    
    createFrame("seasonal_frame", {
        {"spring", "spring"},
        {"summer", "summer"},
        {"autumn", "autumn"},
        {"winter", "winter"},
        {"cycle", "cycle"}
    });
    
    createFrame("celebration_frame", {
        {"new_year", "new_year"},
        {"birthday", "birthday"},
        {"anniversary", "anniversary"},
        {"celebration", "celebration"},
        {"time", "year"}
    });
    
    createFrame("weekly_frame", {
        {"monday", "monday"},
        {"tuesday", "tuesday"},
        {"wednesday", "wednesday"},
        {"thursday", "thursday"},
        {"friday", "friday"},
        {"saturday", "saturday"},
        {"sunday", "sunday"},
        {"cycle", "weekly"}
    });
    
    createFrame("daily_frame", {
        {"morning", "morning"},
        {"afternoon", "afternoon"},
        {"evening", "evening"},
        {"night", "night"},
        {"cycle", "daily"}
    });

    // ===== ФРЕЙМЫ ДЛЯ МИКРО-ВРЕМЕНИ =====

    createFrame("time_measurement_frame", {
        {"hours", "hour"},
        {"minutes", "minute"},
        {"seconds", "second"},
        {"milliseconds", "millisecond"},
        {"device", "clock"}
    });

    createFrame("precision_frame", {
        {"coarse", "second"},
        {"fine", "millisecond"},
        {"device", "stopwatch"}
    });

    createFrame("day_cycle_frame", {
        {"dawn", "dawn"},
        {"morning", "morning"},
        {"noon", "noon"},
        {"afternoon", "afternoon"},
        {"dusk", "dusk"},
        {"evening", "evening"},
        {"midnight", "midnight"},
        {"night", "night"}
    });
    
    std::cout << "  Added temporal concept network with " << nodes.size() << " total nodes" << std::endl;
}

void buildSpatialConceptNetwork() {
    std::cout << "  Building spatial concept network..." << std::endl;
    
    // ===== 1. БАЗОВЫЕ ПРОСТРАНСТВЕННЫЕ КОНЦЕПТЫ =====
    
    // Измерения
    uint32_t space = addNode("space", "space", {"void", "emptiness"}, "abstract",
        0.9f, 0.8f, 9, EmotionalTone::NEUTRAL,
        {"physics", "dimension", "container"});
    
    uint32_t dimension = addNode("dimension", "dimension", {"axis"}, "abstract",
        0.8f, 0.7f, 8, EmotionalTone::NEUTRAL,
        {"physics", "mathematics", "measurement"});
    
    uint32_t point = addNode("point", "point", {"location", "position"}, "spatial",
        0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"geometry", "location", "coordinates"});
    
    uint32_t line = addNode("line", "line", {"ray", "segment"}, "spatial",
        0.7f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"geometry", "direction", "path"});
    
    uint32_t plane = addNode("plane", "plane", {"surface"}, "spatial",
        0.7f, 0.5f, 5, EmotionalTone::NEUTRAL,
        {"geometry", "2d", "surface"});
    
    uint32_t volume = addNode("volume", "volume", {"space", "capacity"}, "spatial",
        0.7f, 0.5f, 5, EmotionalTone::NEUTRAL,
        {"geometry", "3d", "measurement"});
    
    // ===== 2. НАПРАВЛЕНИЯ И ОРИЕНТАЦИИ =====
    
    uint32_t north = addNode("north", "north", {"N"}, "direction",
        0.6f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"navigation", "compass", "geography"});
    
    uint32_t south = addNode("south", "south", {"S"}, "direction",
        0.6f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"navigation", "compass", "geography"});
    
    uint32_t east = addNode("east", "east", {"E"}, "direction",
        0.6f, 0.2f, 2, EmotionalTone::POSITIVE,
        {"navigation", "compass", "geography", "sunrise"});
    
    uint32_t west = addNode("west", "west", {"W"}, "direction",
        0.6f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"navigation", "compass", "geography", "sunset"});
    
    uint32_t up = addNode("up", "up", {"above", "upward"}, "direction",
        0.6f, 0.2f, 2, EmotionalTone::POSITIVE,
        {"vertical", "sky", "ascent"});
    
    uint32_t down = addNode("down", "down", {"below", "downward"}, "direction",
        0.6f, 0.2f, 2, EmotionalTone::NEGATIVE,
        {"vertical", "ground", "descent"});
    
    uint32_t left = addNode("left", "left", {"sinister"}, "direction",
        0.5f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"horizontal", "lateral"});
    
    uint32_t right = addNode("right", "right", {"dexter"}, "direction",
        0.5f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"horizontal", "lateral"});
    
    uint32_t forward = addNode("forward", "forward", {"ahead", "front"}, "direction",
        0.7f, 0.2f, 2, EmotionalTone::POSITIVE,
        {"movement", "progress"});
    
    uint32_t backward = addNode("backward", "backward", {"behind", "rear"}, "direction",
        0.6f, 0.2f, 2, EmotionalTone::NEGATIVE,
        {"movement", "regress"});
    
    // ===== 3. ФОРМЫ И ГЕОМЕТРИЯ =====
    
    uint32_t circle = addNode("circle", "circle", {"ring", "round"}, "shape",
        0.7f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"geometry", "symmetry", "cycle"});
    
    uint32_t square = addNode("square", "square", {"quadrilateral"}, "shape",
        0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"geometry", "stability"});
    
    uint32_t triangle = addNode("triangle", "triangle", {"trigon"}, "shape",
        0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"geometry", "strength"});
    
    uint32_t sphere = addNode("sphere", "sphere", {"ball", "globe"}, "shape_3d",
        0.8f, 0.4f, 4, EmotionalTone::POSITIVE,
        {"geometry", "3d", "symmetry"});
    
    uint32_t cube = addNode("cube", "cube", {"hexahedron"}, "shape_3d",
        0.7f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"geometry", "3d", "volume"});
    
    uint32_t pyramid = addNode("pyramid", "pyramid", {"tetrahedron"}, "shape_3d",
        0.7f, 0.4f, 4, EmotionalTone::POSITIVE,
        {"geometry", "ancient", "monument"});
    
    // ===== 4. ИНСТРУМЕНТЫ ИЗМЕРЕНИЯ =====
    
    uint32_t ruler = addNode("ruler", "ruler", {"straightedge"}, "tool",
        0.6f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"measurement", "length", "tool"});
    
    uint32_t compass = addNode("compass", "compass", {"direction finder"}, "tool",
        0.7f, 0.4f, 4, EmotionalTone::POSITIVE,
        {"navigation", "direction", "tool"});
    
    uint32_t protractor = addNode("protractor", "protractor", {"angle measure"}, "tool",
        0.6f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"measurement", "angle", "tool"});
    
    uint32_t gps = addNode("gps", "GPS", {"satellite navigation"}, "technology",
        0.8f, 0.6f, 5, EmotionalTone::VERY_POSITIVE,
        {"navigation", "position", "technology"});
    
    uint32_t map = addNode("map", "map", {"chart", "atlas"}, "representation",
        0.8f, 0.4f, 4, EmotionalTone::POSITIVE,
        {"navigation", "geography", "representation"});
    
    // ===== 5. ЕДИНИЦЫ ИЗМЕРЕНИЯ =====
    
    uint32_t meter = addNode("meter", "meter", {"m", "metre"}, "unit",
        0.6f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"length", "si", "measurement"});
    
    uint32_t kilometer = addNode("kilometer", "kilometer", {"km"}, "unit",
        0.6f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"length", "distance", "measurement"});
    
    uint32_t centimeter = addNode("centimeter", "centimeter", {"cm"}, "unit",
        0.5f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"length", "small", "measurement"});
    
    uint32_t millimeter = addNode("millimeter", "millimeter", {"mm"}, "unit",
        0.5f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"length", "tiny", "precision"});
    
    uint32_t inch = addNode("inch", "inch", {"in", "\""}, "unit",
        0.5f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"length", "imperial", "measurement"});
    
    uint32_t foot = addNode("foot", "foot", {"ft", "'"}, "unit",
        0.5f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"length", "imperial", "measurement"});
    
    uint32_t mile = addNode("mile", "mile", {"mi"}, "unit",
        0.6f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"length", "imperial", "distance"});
    
    uint32_t degree = addNode("degree", "degree", {"°", "deg"}, "unit",
        0.6f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"angle", "measurement", "temperature"});
    
    uint32_t radian = addNode("radian", "radian", {"rad"}, "unit",
        0.7f, 0.5f, 5, EmotionalTone::NEUTRAL,
        {"angle", "mathematics", "advanced"});

    // В начале метода добавьте получение или создание узлов:
    uint32_t distance = getNodeId("distance");
    if (distance == 0) {
        distance = addNode("distance", "distance", {"length", "space between"}, "measurement",
            0.7f, 0.4f, 4, EmotionalTone::NEUTRAL,
            {"physics", "space", "measurement"});
    }

    uint32_t time_duration_node = getNodeId("duration");
    if (time_duration_node == 0) {
        time_duration_node = addNode("duration", "duration", {"time span", "period"}, "temporal",
            0.7f, 0.5f, 4, EmotionalTone::NEUTRAL, {"temporal", "measurement"});
    }

    uint32_t hour = getNodeId("hour");
    if (hour == 0) {
        hour = addNode("hour", "hour", {"60 minutes", "h"}, "time_unit",
            0.6f, 0.2f, 2, EmotionalTone::NEUTRAL,
            {"temporal", "measurement", "clock"});
    }

    uint32_t second = getNodeId("second");
    if (second == 0) {
        second = addNode("second", "second", {"s", "sec"}, "time_unit",
            0.6f, 0.1f, 1, EmotionalTone::NEUTRAL,
            {"temporal", "measurement", "clock"});
    }
    
    // ===== 6. СВЯЗИ МЕЖДУ ПРОСТРАНСТВЕННЫМИ КОНЦЕПТАМИ =====
    
    // Связь пространства с измерениями
    addEdge(space, dimension, SemanticEdge::Type::HAS_PART, 1.0f);
    addEdge(dimension, point, SemanticEdge::Type::CONTAINS, 0.8f);
    addEdge(dimension, line, SemanticEdge::Type::CONTAINS, 0.8f);
    addEdge(dimension, plane, SemanticEdge::Type::CONTAINS, 0.8f);
    addEdge(dimension, volume, SemanticEdge::Type::CONTAINS, 0.8f);
    
    // Иерархия измерений
    addEdge(point, line, SemanticEdge::Type::LEADS_TO, 0.7f); // множество точек образуют линию
    addEdge(line, plane, SemanticEdge::Type::LEADS_TO, 0.7f); // множество линий образуют плоскость
    addEdge(plane, volume, SemanticEdge::Type::LEADS_TO, 0.7f); // множество плоскостей образуют объем
    
    // Противоположные направления
    addEdge(north, south, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(east, west, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(up, down, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(left, right, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(forward, backward, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    
    // Связь с движением
    uint32_t action_move = getNodeId("action_move");
    if (action_move != 0) {
        addEdge(action_move, forward, SemanticEdge::Type::DIRECTION, 0.8f);
        addEdge(action_move, backward, SemanticEdge::Type::DIRECTION, 0.6f);
        addEdge(action_move, left, SemanticEdge::Type::DIRECTION, 0.5f);
        addEdge(action_move, right, SemanticEdge::Type::DIRECTION, 0.5f);
    }
    
    // Связь с геометрией
    addEdge(circle, sphere, SemanticEdge::Type::IS_A_KIND_OF, 0.8f); // круг - 2D версия сферы
    addEdge(square, cube, SemanticEdge::Type::IS_A_KIND_OF, 0.8f); // квадрат - 2D версия куба
    addEdge(triangle, pyramid, SemanticEdge::Type::IS_A_KIND_OF, 0.8f); // треугольник - 2D версия пирамиды
    
    // Связь с инструментами
    addEdge(ruler, meter, SemanticEdge::Type::MEASURES, 0.9f);
    addEdge(ruler, centimeter, SemanticEdge::Type::MEASURES, 0.9f);
    addEdge(ruler, millimeter, SemanticEdge::Type::MEASURES, 0.8f);
    
    addEdge(compass, north, SemanticEdge::Type::POINTS_TO, 0.9f);
    addEdge(compass, south, SemanticEdge::Type::POINTS_TO, 0.9f);
    addEdge(compass, east, SemanticEdge::Type::POINTS_TO, 0.9f);
    addEdge(compass, west, SemanticEdge::Type::POINTS_TO, 0.9f);
    
    addEdge(gps, point, SemanticEdge::Type::LOCATES, 1.0f);
    addEdge(gps, map, SemanticEdge::Type::USES, 0.9f);
    
    addEdge(map, north, SemanticEdge::Type::HAS_AXIS, 0.9f);
    addEdge(map, south, SemanticEdge::Type::HAS_AXIS, 0.9f);
    addEdge(map, east, SemanticEdge::Type::HAS_AXIS, 0.9f);
    addEdge(map, west, SemanticEdge::Type::HAS_AXIS, 0.9f);
    
    // Единицы измерения - иерархия
    addEdge(meter, kilometer, SemanticEdge::Type::HAS_VALUE, 0.9f); // 1 км = 1000 м
    addEdge(meter, centimeter, SemanticEdge::Type::HAS_VALUE, 0.9f); // 1 м = 100 см
    addEdge(centimeter, millimeter, SemanticEdge::Type::HAS_VALUE, 0.9f); // 1 см = 10 мм
    
    addEdge(foot, inch, SemanticEdge::Type::HAS_VALUE, 0.9f); // 1 фут = 12 дюймов
    addEdge(mile, foot, SemanticEdge::Type::HAS_VALUE, 0.9f); // 1 миля = 5280 футов
    
    // Конверсия между системами
    addEdge(meter, foot, SemanticEdge::Type::RELATED_TO, 0.7f); // приблизительно 3.28 фута
    addEdge(kilometer, mile, SemanticEdge::Type::RELATED_TO, 0.7f); // приблизительно 0.62 мили
    
    // ===== 7. СВЯЗЬ ПРОСТРАНСТВА И ВРЕМЕНИ =====
    
    uint32_t speed = getNodeId("speed");
    if (speed == 0) {
        speed = addNode("speed", "speed", {"velocity"}, "physics",
            0.8f, 0.6f, 6, EmotionalTone::NEUTRAL,
            {"physics", "motion", "measurement"});
    }
    
    uint32_t acceleration = getNodeId("acceleration");
    if (acceleration == 0) {
        acceleration = addNode("acceleration", "acceleration", {"rate of change"}, "physics",
            0.8f, 0.7f, 7, EmotionalTone::NEUTRAL,
            {"physics", "motion", "calculus"});
    }
    
    uint32_t spacetime = addNode("spacetime", "spacetime", {"space-time continuum"}, "physics",
        0.9f, 0.9f, 10, EmotionalTone::NEUTRAL,
        {"physics", "relativity", "advanced"});
    
    // Связь пространства и времени через движение
    addEdge(speed, distance, SemanticEdge::Type::RELATES_TO, 0.9f);
    addEdge(speed, time_duration_node, SemanticEdge::Type::RELATES_TO, 0.9f);
    
    addEdge(acceleration, speed, SemanticEdge::Type::CAUSES, 0.8f);
    addEdge(acceleration, time_duration_node, SemanticEdge::Type::RELATES_TO, 0.8f);
    
    // Единицы скорости
    uint32_t kmh = addNode("kmh", "km/h", {"kilometers per hour"}, "unit",
        0.6f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"speed", "measurement", "transport"});
    
    uint32_t mph = addNode("mph", "mph", {"miles per hour"}, "unit",
        0.6f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"speed", "measurement", "transport"});
    
    uint32_t ms = addNode("ms", "m/s", {"meters per second"}, "unit",
        0.7f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"speed", "measurement", "physics"});
    
    addEdge(kmh, kilometer, SemanticEdge::Type::RELATES_TO, 0.9f);
    addEdge(kmh, hour, SemanticEdge::Type::RELATES_TO, 0.9f);
    
    addEdge(mph, mile, SemanticEdge::Type::RELATES_TO, 0.9f);
    addEdge(mph, hour, SemanticEdge::Type::RELATES_TO, 0.9f);
    
    addEdge(ms, meter, SemanticEdge::Type::RELATES_TO, 0.9f);
    addEdge(ms, second, SemanticEdge::Type::RELATES_TO, 0.9f);
    
    // Современная физика - пространство-время как единое целое
    addEdge(spacetime, space, SemanticEdge::Type::CONTAINS, 1.0f);
    addEdge(spacetime, time_duration_node, SemanticEdge::Type::CONTAINS, 1.0f);
    addEdge(spacetime, dimension, SemanticEdge::Type::HAS_PART, 1.0f); // 3 пространственных + 1 временное
    
    // ===== 8. ФРЕЙМЫ ДЛЯ ПРОСТРАНСТВЕННЫХ КОНЦЕПТОВ =====
    
    createFrame("spatial_frame", {
        {"has_point", "point"},
        {"has_line", "line"},
        {"has_plane", "plane"},
        {"has_volume", "volume"},
        {"dimensions", "dimension"}
    });
    
    createFrame("navigation_frame", {
        {"north", "north"},
        {"south", "south"},
        {"east", "east"},
        {"west", "west"},
        {"tool", "compass"},
        {"map", "map"}
    });
    
    createFrame("measurement_frame", {
        {"length", "meter"},
        {"large_distance", "kilometer"},
        {"small_distance", "centimeter"},
        {"tiny_distance", "millimeter"},
        {"tool", "ruler"}
    });
    
    createFrame("geometry_frame", {
        {"circle", "circle"},
        {"square", "square"},
        {"triangle", "triangle"},
        {"sphere", "sphere"},
        {"cube", "cube"},
        {"pyramid", "pyramid"}
    });
    
    createFrame("physics_frame", {
        {"space", "space"},
        {"time", "duration"},
        {"spacetime", "spacetime"},
        {"speed", "speed"},
        {"acceleration", "acceleration"}
    });
    
    std::cout << "  Added spatial concept network" << std::endl;
}

void buildMathematicalFoundationLayer() {
    std::cout << "  Building mathematical foundation layer..." << std::endl;
    
    // ===== 1. БАЗОВЫЕ МАТЕМАТИЧЕСКИЕ КОНЦЕПТЫ =====
    
    // Числа и количества (абстракция 1-2)
    uint32_t zero = addNode("zero", "zero", {"0", "null", "nothing"}, "number",
        1.0f, 0.1f, 1, EmotionalTone::NEUTRAL,
        {"mathematics", "quantity", "foundation"});
    
    uint32_t one = addNode("one", "one", {"1", "unit", "single"}, "number",
        0.9f, 0.1f, 1, EmotionalTone::NEUTRAL,
        {"mathematics", "quantity", "unit"});
    
    uint32_t two = addNode("two", "two", {"2", "pair", "double"}, "number",
        0.8f, 0.1f, 1, EmotionalTone::NEUTRAL,
        {"mathematics", "quantity", "binary"});
    
    uint32_t three = addNode("three", "three", {"3", "triple"}, "number",
        0.7f, 0.1f, 1, EmotionalTone::NEUTRAL,
        {"mathematics", "quantity", "trinity"});
    
    uint32_t infinity = addNode("infinity", "infinity", {"∞", "infinite", "unbounded"}, "number",
        0.9f, 0.8f, 9, EmotionalTone::POSITIVE,
        {"mathematics", "limit", "abstract"});
    
    // Операции (абстракция 3-4)
    uint32_t addition = addNode("addition", "addition", {"sum", "plus", "+"}, "operation",
        0.9f, 0.2f, 2, EmotionalTone::POSITIVE,
        {"mathematics", "arithmetic", "combine"});
    
    uint32_t subtraction = addNode("subtraction", "subtraction", {"minus", "difference", "-"}, "operation",
        0.9f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"mathematics", "arithmetic", "remove"});
    
    uint32_t multiplication = addNode("multiplication", "multiplication", {"times", "product", "×"}, "operation",
        0.9f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"mathematics", "arithmetic", "scale"});
    
    uint32_t division = addNode("division", "division", {"quotient", "÷", "/"}, "operation",
        0.9f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"mathematics", "arithmetic", "split"});
    
    uint32_t exponentiation = addNode("exponentiation", "exponentiation", {"power", "^", "raise"}, "operation",
        0.8f, 0.5f, 4, EmotionalTone::POSITIVE,
        {"mathematics", "algebra", "growth"});
    
    uint32_t root = addNode("root", "root", {"square root", "√", "radical"}, "operation",
        0.8f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"mathematics", "algebra", "inverse"});
    
    uint32_t logarithm = addNode("logarithm", "logarithm", {"log", "ln"}, "operation",
        0.8f, 0.7f, 6, EmotionalTone::NEUTRAL,
        {"mathematics", "analysis", "scale"});
    
    // ===== 2. ЛОГИЧЕСКИЕ ОПЕРАЦИИ (для мышления) =====
    
    uint32_t logic_and = addNode("and", "and", {"∧", "&&", "conjunction"}, "logic",
        0.9f, 0.3f, 4, EmotionalTone::NEUTRAL,
        {"logic", "reasoning", "boolean"});
    
    uint32_t logic_or = addNode("or", "or", {"∨", "||", "disjunction"}, "logic",
        0.9f, 0.3f, 4, EmotionalTone::NEUTRAL,
        {"logic", "reasoning", "boolean"});
    
    uint32_t logic_not = addNode("not", "not", {"¬", "!", "negation"}, "logic",
        0.9f, 0.3f, 4, EmotionalTone::NEGATIVE,
        {"logic", "reasoning", "inverse"});
    
    uint32_t logic_xor = addNode("xor", "xor", {"⊻", "exclusive or"}, "logic",
        0.8f, 0.4f, 5, EmotionalTone::NEUTRAL,
        {"logic", "reasoning", "exclusive"});
    
    uint32_t logic_implies = addNode("implies", "implies", {"→", "⇒", "if then"}, "logic",
        0.9f, 0.5f, 6, EmotionalTone::NEUTRAL,
        {"logic", "reasoning", "causality"});
    
    uint32_t logic_equivalent = addNode("equivalent", "equivalent", {"↔", "⇔", "iff"}, "logic",
        0.9f, 0.5f, 6, EmotionalTone::NEUTRAL,
        {"logic", "reasoning", "equality"});
    
    // ===== 3. ОТНОШЕНИЯ СРАВНЕНИЯ =====
    
    uint32_t equal = addNode("equal", "equal", {"=", "equals", "same as"}, "relation",
        1.0f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"mathematics", "logic", "comparison"});
    
    uint32_t not_equal = addNode("not_equal", "not equal", {"≠", "!=", "different"}, "relation",
        0.9f, 0.2f, 2, EmotionalTone::NEGATIVE,
        {"mathematics", "logic", "comparison"});
    
    uint32_t greater_than = addNode("greater_than", "greater than", {">", "more than"}, "relation",
        0.8f, 0.2f, 2, EmotionalTone::POSITIVE,
        {"mathematics", "comparison", "inequality"});
    
    uint32_t less_than = addNode("less_than", "less than", {"<", "fewer than"}, "relation",
        0.8f, 0.2f, 2, EmotionalTone::NEGATIVE,
        {"mathematics", "comparison", "inequality"});
    
    uint32_t greater_equal = addNode("greater_equal", "greater or equal", {"≥", ">="}, "relation",
        0.8f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"mathematics", "comparison", "inequality"});
    
    uint32_t less_equal = addNode("less_equal", "less or equal", {"≤", "<="}, "relation",
        0.8f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"mathematics", "comparison", "inequality"});
    
    // ===== 4. ТЕОРЕТИКО-МНОЖЕСТВЕННЫЕ КОНЦЕПТЫ =====
    
    uint32_t set = addNode("set", "set", {"collection", "group"}, "mathematics",
        0.8f, 0.5f, 5, EmotionalTone::NEUTRAL,
        {"mathematics", "foundation", "structure"});
    
    uint32_t element = addNode("element", "element", {"member", "item"}, "mathematics",
        0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"mathematics", "set theory", "belonging"});
    
    uint32_t subset = addNode("subset", "subset", {"⊆", "inclusion"}, "relation",
        0.8f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"mathematics", "set theory", "inclusion"});
    
    uint32_t union_set = addNode("union", "union", {"∪", "join"}, "operation",
        0.8f, 0.4f, 4, EmotionalTone::POSITIVE,
        {"mathematics", "set theory", "combine"});
    
    uint32_t intersection = addNode("intersection", "intersection", {"∩", "common"}, "operation",
        0.8f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"mathematics", "set theory", "common"});
    
    uint32_t complement = addNode("complement", "complement", {"∁", "not in"}, "operation",
        0.7f, 0.5f, 5, EmotionalTone::NEGATIVE,
        {"mathematics", "set theory", "difference"});
    
    uint32_t empty_set = addNode("empty_set", "empty set", {"∅", "{}"}, "set",
        0.9f, 0.2f, 3, EmotionalTone::NEUTRAL,
        {"mathematics", "set theory", "nothing"});
    
    // ===== 5. АНАЛИТИЧЕСКИЕ КОНЦЕПТЫ =====
    
    uint32_t limit = addNode("limit", "limit", {"approaches", "tends to"}, "analysis",
        0.9f, 0.8f, 7, EmotionalTone::NEUTRAL,
        {"calculus", "analysis", "convergence"});
    
    uint32_t derivative = addNode("derivative", "derivative", {"rate of change", "d/dx"}, "analysis",
        0.9f, 0.8f, 7, EmotionalTone::NEUTRAL,
        {"calculus", "analysis", "change"});
    
    uint32_t integral = addNode("integral", "integral", {"∫", "accumulation"}, "analysis",
        0.9f, 0.8f, 7, EmotionalTone::POSITIVE,
        {"calculus", "analysis", "sum"});
    
    uint32_t function = addNode("function", "function", {"f(x)", "mapping"}, "mathematics",
        0.8f, 0.6f, 6, EmotionalTone::NEUTRAL,
        {"mathematics", "analysis", "relationship"});
    
    uint32_t variable = addNode("variable", "variable", {"x", "unknown"}, "mathematics",
        0.7f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"mathematics", "algebra", "unknown"});
    
    uint32_t constant = addNode("constant", "constant", {"fixed", "unchanging"}, "mathematics",
        0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"mathematics", "algebra", "fixed"});
    
    uint32_t parameter = addNode("parameter", "parameter", {"coefficient", "control"}, "mathematics",
        0.7f, 0.5f, 5, EmotionalTone::NEUTRAL,
        {"mathematics", "modeling", "control"});
    
    // ===== 6. ТЕОРЕТИКО-ВЕРОЯТНОСТНЫЕ КОНЦЕПТЫ =====
    
    uint32_t probability = addNode("probability", "probability", {"chance", "likelihood"}, "statistics",
        0.9f, 0.6f, 6, EmotionalTone::NEUTRAL,
        {"statistics", "uncertainty", "randomness"});
    
    uint32_t expectation = addNode("expectation", "expectation", {"expected value", "mean"}, "statistics",
        0.8f, 0.6f, 6, EmotionalTone::NEUTRAL,
        {"statistics", "average", "prediction"});
    
    uint32_t variance = addNode("variance", "variance", {"spread", "dispersion"}, "statistics",
        0.8f, 0.6f, 6, EmotionalTone::NEUTRAL,
        {"statistics", "uncertainty", "measure"});
    
    uint32_t correlation = addNode("correlation", "correlation", {"relationship", "dependence"}, "statistics",
        0.9f, 0.6f, 6, EmotionalTone::NEUTRAL,
        {"statistics", "relationship", "dependence"});
    
    uint32_t random = addNode("random", "random", {"stochastic", "chance"}, "statistics",
        0.7f, 0.5f, 5, EmotionalTone::NEUTRAL,
        {"statistics", "uncertainty", "chaos"});
    
    uint32_t distribution = addNode("distribution", "distribution", {"spread", "histogram"}, "statistics",
        0.8f, 0.6f, 6, EmotionalTone::NEUTRAL,
        {"statistics", "probability", "frequency"});
    
    // ===== 7. ОШИБКА И КОРРЕКЦИЯ =====
    
    uint32_t error = addNode("error", "error", {"mistake", "deviation", "fault"}, "evaluation",
        1.0f, 0.4f, 4, EmotionalTone::VERY_NEGATIVE,
        {"learning", "correction", "feedback"});
    
    uint32_t accuracy = addNode("accuracy", "accuracy", {"precision", "correctness"}, "evaluation",
        0.9f, 0.4f, 4, EmotionalTone::VERY_POSITIVE,
        {"measurement", "quality", "performance"});
    
    uint32_t precision = addNode("precision", "precision", {"exactness", "detail"}, "evaluation",
        0.8f, 0.4f, 4, EmotionalTone::POSITIVE,
        {"measurement", "quality", "exactness"});
    
    uint32_t tolerance = addNode("tolerance", "tolerance", {"allowance", "margin"}, "evaluation",
        0.7f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"measurement", "acceptance", "range"});
    
    uint32_t deviation = addNode("deviation", "deviation", {"difference", "offset"}, "measurement",
        0.7f, 0.4f, 4, EmotionalTone::NEGATIVE,
        {"statistics", "error", "measurement"});
    
    uint32_t correction = addNode("correction", "correction", {"adjustment", "fix"}, "action",
        0.9f, 0.4f, 4, EmotionalTone::POSITIVE,
        {"learning", "improvement", "feedback"});
    
    uint32_t feedback = addNode("feedback", "feedback", {"response", "reaction"}, "communication",
        0.9f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"control", "learning", "system"});
    
    uint32_t iteration = addNode("iteration", "iteration", {"repetition", "loop"}, "process",
        0.8f, 0.5f, 5, EmotionalTone::NEUTRAL,
        {"computation", "learning", "optimization"});
    
    // ===== 8. ОСНОВЫ ИССЛЕДОВАНИЯ =====
    
    uint32_t hypothesis = addNode("hypothesis", "hypothesis", {"assumption", "theory"}, "research",
        0.9f, 0.7f, 7, EmotionalTone::POSITIVE,
        {"science", "research", "investigation"});
    
    uint32_t experiment = addNode("experiment", "experiment", {"test", "trial"}, "research",
        0.9f, 0.6f, 6, EmotionalTone::POSITIVE,
        {"science", "research", "validation"});
    
    uint32_t observation = addNode("observation", "observation", {"measurement", "perception"}, "research",
        0.8f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"science", "research", "data"});
    
    uint32_t data = addNode("data", "data", {"information", "facts"}, "information",
        0.8f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"science", "computation", "knowledge"});
    
    uint32_t evidence = addNode("evidence", "evidence", {"proof", "support"}, "research",
        0.9f, 0.5f, 5, EmotionalTone::POSITIVE,
        {"science", "validation", "facts"});
    
    uint32_t theory = addNode("theory", "theory", {"model", "explanation"}, "research",
        0.9f, 0.8f, 8, EmotionalTone::POSITIVE,
        {"science", "knowledge", "understanding"});
    
    uint32_t law = addNode("law", "law", {"principle", "rule"}, "research",
        1.0f, 0.6f, 7, EmotionalTone::POSITIVE,
        {"science", "physics", "invariant"});
    
    uint32_t proof = addNode("proof", "proof", {"demonstration", "verification"}, "logic",
        0.9f, 0.7f, 7, EmotionalTone::VERY_POSITIVE,
        {"mathematics", "logic", "certainty"});
    
    uint32_t contradiction = addNode("contradiction", "contradiction", {"inconsistency", "paradox"}, "logic",
        0.8f, 0.6f, 6, EmotionalTone::VERY_NEGATIVE,
        {"logic", "reasoning", "error"});
    
    // ===== 9. ПРИЧИННО-СЛЕДСТВЕННЫЕ СВЯЗИ =====
    
    uint32_t cause = addNode("cause", "cause", {"reason", "origin"}, "causality",
        1.0f, 0.5f, 5, EmotionalTone::NEUTRAL,
        {"philosophy", "physics", "explanation"});
    
    uint32_t effect = addNode("effect", "effect", {"result", "consequence"}, "causality",
        1.0f, 0.5f, 5, EmotionalTone::NEUTRAL,
        {"philosophy", "physics", "outcome"});
    
    uint32_t necessary = addNode("necessary", "necessary", {"essential", "required"}, "modality",
        0.9f, 0.6f, 6, EmotionalTone::NEUTRAL,
        {"logic", "philosophy", "condition"});
    
    uint32_t sufficient = addNode("sufficient", "sufficient", {"enough", "adequate"}, "modality",
        0.9f, 0.5f, 5, EmotionalTone::POSITIVE,
        {"logic", "condition", "adequacy"});
    
    uint32_t condition = addNode("condition", "condition", {"prerequisite", "requirement"}, "logic",
        0.8f, 0.5f, 5, EmotionalTone::NEUTRAL,
        {"logic", "programming", "if"});
    
    // ===== 10. ФУНДАМЕНТАЛЬНЫЕ СВЯЗИ =====
    
    // Числа и их отношения
    addEdge(zero, one, SemanticEdge::Type::BEFORE, 0.9f);
    addEdge(one, two, SemanticEdge::Type::AFTER, 0.9f);
    addEdge(two, three, SemanticEdge::Type::AFTER, 0.9f);
    addEdge(infinity, one, SemanticEdge::Type::IS_BETTER_THAN, 0.8f); // infinity > 1
    
    // Операции и их свойства
    addEdge(addition, one, SemanticEdge::Type::USES, 0.9f);
    addEdge(addition, two, SemanticEdge::Type::USES, 0.9f);
    addEdge(subtraction, addition, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(multiplication, addition, SemanticEdge::Type::IS_A_KIND_OF, 0.8f); // multiplication is repeated addition
    addEdge(division, multiplication, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(exponentiation, multiplication, SemanticEdge::Type::IS_A_KIND_OF, 0.8f); // exponentiation is repeated multiplication
    addEdge(root, exponentiation, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(logarithm, exponentiation, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    
    // Логические отношения
    addEdge(logic_and, logic_or, SemanticEdge::Type::OPPOSITE_OF, 0.7f);
    addEdge(logic_not, logic_and, SemanticEdge::Type::RELATES_TO, 0.8f);
    addEdge(logic_implies, cause, SemanticEdge::Type::SIMILAR_TO, 0.9f);
    
    // Отношения сравнения
    addEdge(equal, not_equal, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(greater_than, less_than, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(greater_than, greater_equal, SemanticEdge::Type::IS_A_KIND_OF, 0.9f);
    addEdge(less_than, less_equal, SemanticEdge::Type::IS_A_KIND_OF, 0.9f);
    
    // Множества
    addEdge(set, element, SemanticEdge::Type::CONTAINS, 0.9f);
    addEdge(empty_set, set, SemanticEdge::Type::IS_A_KIND_OF, 0.9f);
    addEdge(zero, empty_set, SemanticEdge::Type::RELATES_TO, 0.9f); // 0 связан с пустым множеством
    addEdge(subset, set, SemanticEdge::Type::RELATES_TO, 0.8f);
    addEdge(union_set, addition, SemanticEdge::Type::SIMILAR_TO, 0.8f);
    addEdge(intersection, multiplication, SemanticEdge::Type::SIMILAR_TO, 0.7f);
    
    // Анализ
    addEdge(limit, function, SemanticEdge::Type::RELATES_TO, 0.8f);
    addEdge(derivative, limit, SemanticEdge::Type::REQUIRES, 0.9f);
    addEdge(integral, limit, SemanticEdge::Type::REQUIRES, 0.9f);
    addEdge(derivative, integral, SemanticEdge::Type::OPPOSITE_OF, 0.9f);
    
    // Вероятность и статистика
    addEdge(probability, random, SemanticEdge::Type::MEASURES, 0.9f);
    addEdge(expectation, probability, SemanticEdge::Type::REQUIRES, 0.8f);
    addEdge(variance, expectation, SemanticEdge::Type::REQUIRES, 0.8f);
    addEdge(correlation, variance, SemanticEdge::Type::RELATES_TO, 0.7f);
    
    // Ошибка и коррекция
    addEdge(error, deviation, SemanticEdge::Type::IS_A, 0.9f);
    addEdge(error, accuracy, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(precision, accuracy, SemanticEdge::Type::SIMILAR_TO, 0.8f);
    addEdge(correction, error, SemanticEdge::Type::RESPONDS_TO, 0.9f);
    addEdge(feedback, correction, SemanticEdge::Type::CAUSES, 0.8f);
    addEdge(iteration, feedback, SemanticEdge::Type::USES, 0.8f);
    
    // Исследование
    addEdge(hypothesis, experiment, SemanticEdge::Type::LEADS_TO, 0.8f);
    addEdge(experiment, observation, SemanticEdge::Type::PRODUCES, 0.9f);
    addEdge(observation, data, SemanticEdge::Type::PRODUCES, 0.9f);
    addEdge(data, evidence, SemanticEdge::Type::SUPPORTS, 0.8f);
    addEdge(evidence, theory, SemanticEdge::Type::SUPPORTS, 0.7f);
    addEdge(theory, law, SemanticEdge::Type::LEADS_TO, 0.6f);
    addEdge(proof, theory, SemanticEdge::Type::VERIFIES, 0.9f);
    addEdge(contradiction, proof, SemanticEdge::Type::OPPOSITE_OF, 0.9f);
    
    // Причинность
    addEdge(cause, effect, SemanticEdge::Type::CAUSES, 1.0f);
    addEdge(necessary, cause, SemanticEdge::Type::IS_A, 0.8f);
    addEdge(sufficient, cause, SemanticEdge::Type::IS_A, 0.8f);
    addEdge(condition, necessary, SemanticEdge::Type::SIMILAR_TO, 0.8f);
    
    // Связь с существующими концептами
    uint32_t learning = getNodeId("learn");
    if (learning != 0) {
        addEdge(learning, error, SemanticEdge::Type::RELATES_TO, 0.9f);
        addEdge(learning, correction, SemanticEdge::Type::USES, 0.8f);
        addEdge(learning, iteration, SemanticEdge::Type::REQUIRES, 0.8f);
    }
    
    uint32_t prediction = getNodeId("predict");
    if (prediction != 0) {
        addEdge(prediction, probability, SemanticEdge::Type::USES, 0.9f);
        addEdge(prediction, expectation, SemanticEdge::Type::RELATES_TO, 0.8f);
    }
    
    uint32_t knowledge = getNodeId("know");
    if (knowledge != 0) {
        addEdge(knowledge, data, SemanticEdge::Type::RELATES_TO, 0.8f);
        addEdge(knowledge, theory, SemanticEdge::Type::RELATES_TO, 0.9f);
    }
    
    // ===== 11. ФРЕЙМЫ ДЛЯ МАТЕМАТИЧЕСКИХ КОНЦЕПТОВ =====
    
    createFrame("arithmetic_frame", {
        {"addition", "addition"},
        {"subtraction", "subtraction"},
        {"multiplication", "multiplication"},
        {"division", "division"},
        {"numbers", "one"}
    });
    
    createFrame("logic_frame", {
        {"and", "and"},
        {"or", "or"},
        {"not", "not"},
        {"implies", "implies"},
        {"equivalent", "equivalent"}
    });
    
    createFrame("comparison_frame", {
        {"equal", "equal"},
        {"greater", "greater_than"},
        {"less", "less_than"},
        {"not_equal", "not_equal"}
    });
    
    createFrame("set_theory_frame", {
        {"set", "set"},
        {"element", "element"},
        {"union", "union"},
        {"intersection", "intersection"},
        {"empty", "empty_set"}
    });
    
    createFrame("calculus_frame", {
        {"function", "function"},
        {"limit", "limit"},
        {"derivative", "derivative"},
        {"integral", "integral"},
        {"variable", "variable"}
    });
    
    createFrame("statistics_frame", {
        {"probability", "probability"},
        {"expectation", "expectation"},
        {"variance", "variance"},
        {"correlation", "correlation"},
        {"distribution", "distribution"}
    });
    
    createFrame("research_frame", {
        {"hypothesis", "hypothesis"},
        {"experiment", "experiment"},
        {"data", "data"},
        {"evidence", "evidence"},
        {"theory", "theory"},
        {"proof", "proof"}
    });
    
    createFrame("causality_frame", {
        {"cause", "cause"},
        {"effect", "effect"},
        {"necessary", "necessary"},
        {"sufficient", "sufficient"},
        {"condition", "condition"}
    });
    
    createFrame("error_correction_frame", {
        {"error", "error"},
        {"deviation", "deviation"},
        {"feedback", "feedback"},
        {"correction", "correction"},
        {"iteration", "iteration"}
    });
    
    std::cout << "  Added mathematical foundation layer with " << nodes.size() << " total nodes" << std::endl;
}

void buildDataStructuresAndPatternsLayer() {
    std::cout << "  Building data structures and patterns layer..." << std::endl;
    
    // ========================================================================
    // 1. БАЗОВЫЕ СТРУКТУРЫ ДАННЫХ (абстракция 3-5)
    // ========================================================================
    
    // Абстрактные понятия структур
    uint32_t structure = addNode("structure", "structure", 
        {"organization", "arrangement", "formation"}, "abstract",
        0.9f, 0.6f, 5, EmotionalTone::NEUTRAL,
        {"mathematics", "computer_science", "organization"});
    
    uint32_t collection = addNode("collection", "collection", 
        {"set", "group", "assemblage"}, "abstract",
        0.8f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"mathematics", "organization"});
    
    uint32_t container = addNode("container", "container", 
        {"holder", "receptacle", "vessel"}, "abstract",
        0.8f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"storage", "organization"});
    
    uint32_t element = addNode("element", "element", 
        {"item", "member", "component"}, "abstract",
        0.9f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"mathematics", "part"});
    
    // ===== 1.1 ЛИНЕЙНЫЕ СТРУКТУРЫ =====
    
    uint32_t list = addNode("list", "list", 
        {"sequence", "series", "catalog"}, "data_structure",
        0.9f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"linear", "ordered", "collection"});
    
    uint32_t array = addNode("array", "array", 
        {"matrix", "grid"}, "data_structure",
        0.8f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"indexed", "linear", "computer_science"});
    
    uint32_t linked_list = addNode("linked_list", "linked list", 
        {"chain", "list"}, "data_structure",
        0.8f, 0.6f, 5, EmotionalTone::NEUTRAL,
        {"dynamic", "pointers", "computer_science"});
    
    uint32_t stack = addNode("stack", "stack", 
        {"pile", "LIFO"}, "data_structure",
        0.8f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"last_in_first_out", "linear"});
    
    uint32_t queue = addNode("queue", "queue", 
        {"line", "FIFO"}, "data_structure",
        0.8f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"first_in_first_out", "linear"});
    
    uint32_t deque = addNode("deque", "deque", 
        {"double-ended queue"}, "data_structure",
        0.7f, 0.6f, 5, EmotionalTone::NEUTRAL,
        {"double_ended", "linear"});
    
    uint32_t vector = addNode("vector", "vector", 
        {"dynamic array"}, "data_structure",
        0.8f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"dynamic", "indexed", "c++"});
    
    // ===== 1.2 ИЕРАРХИЧЕСКИЕ СТРУКТУРЫ =====
    
    uint32_t tree = addNode("tree", "tree", 
        {"hierarchy", "rooted"}, "data_structure",
        0.9f, 0.6f, 5, EmotionalTone::POSITIVE,
        {"hierarchical", "nonlinear", "nature"});
    
    uint32_t binary_tree = addNode("binary_tree", "binary tree", 
        {"BT"}, "data_structure",
        0.8f, 0.6f, 5, EmotionalTone::NEUTRAL,
        {"two_children", "search"});
    
    uint32_t heap = addNode("heap", "heap", 
        {"priority_queue"}, "data_structure",
        0.8f, 0.6f, 5, EmotionalTone::NEUTRAL,
        {"priority", "tree_based"});
    
    uint32_t graph = addNode("graph", "graph", 
        {"network", "nodes_edges"}, "data_structure",
        0.9f, 0.7f, 6, EmotionalTone::NEUTRAL,
        {"nonlinear", "connections", "vertices"});
    
    uint32_t node_graph = addNode("node_graph", "node", 
        {"vertex", "point"}, "graph_element",
        0.8f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"graph", "connection_point"});
    
    uint32_t edge_graph = addNode("edge_graph", "edge", 
        {"link", "connection", "arc"}, "graph_element",
        0.8f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"graph", "relationship"});
    
    uint32_t directed_graph = addNode("directed_graph", "directed graph", 
        {"digraph"}, "data_structure",
        0.8f, 0.7f, 6, EmotionalTone::NEUTRAL,
        {"one_way", "arrows"});
    
    uint32_t undirected_graph = addNode("undirected_graph", "undirected graph", 
        {"graph"}, "data_structure",
        0.8f, 0.6f, 5, EmotionalTone::NEUTRAL,
        {"two_way", "symmetrical"});
    
    uint32_t cycle_graph = addNode("cycle_graph", "cycle", 
        {"loop", "circuit"}, "graph_property",
        0.8f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"closed_path", "repetition"});
    
    uint32_t acyclic = addNode("acyclic", "acyclic", 
        {"no_cycles", "tree_like"}, "graph_property",
        0.7f, 0.5f, 4, EmotionalTone::POSITIVE,
        {"no_loops", "efficient"});
    
    // ===== 1.3 ТАБЛИЧНЫЕ СТРУКТУРЫ =====
    
    uint32_t table = addNode("table", "table", 
        {"grid", "spreadsheet"}, "data_structure",
        0.9f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"rows_columns", "tabular"});
    
    uint32_t row = addNode("row", "row", 
        {"record", "tuple", "line"}, "table_element",
        0.8f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"horizontal", "database"});
    
    uint32_t column = addNode("column", "column", 
        {"field", "attribute"}, "table_element",
        0.8f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"vertical", "database"});
    
    uint32_t cell = addNode("cell", "cell", 
        {"entry", "field_value"}, "table_element",
        0.7f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"intersection", "value"});
    
    uint32_t spreadsheet = addNode("spreadsheet", "spreadsheet", 
        {"excel", "sheet"}, "application",
        0.8f, 0.4f, 3, EmotionalTone::POSITIVE,
        {"tabular", "calculation", "business"});
    
    uint32_t matrix = addNode("matrix", "matrix", 
        {"2d_array"}, "mathematical",
        0.8f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"linear_algebra", "2d"});
    
    // ===== 1.4 АССОЦИАТИВНЫЕ СТРУКТУРЫ =====
    
    uint32_t map = addNode("map", "map", 
        {"dictionary", "associative_array"}, "data_structure",
        0.9f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"key_value", "lookup"});
    
    uint32_t key = addNode("key", "key", 
        {"identifier"}, "map_element",
        0.8f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"unique", "lookup"});
    
    uint32_t value = addNode("value", "value", 
        {"data"}, "map_element",
        0.8f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"associated", "content"});
    
    uint32_t hash_table = addNode("hash_table", "hash table", 
        {"hash_map"}, "data_structure",
        0.8f, 0.6f, 5, EmotionalTone::NEUTRAL,
        {"hashing", "fast_lookup"});
    
    uint32_t set_ds = addNode("set_ds", "set", 
        {"unique_elements"}, "data_structure",
        0.8f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"no_duplicates", "mathematical"});
    
    // ===== 1.5 ПОЛЯ И ГРУППЫ =====
    
    uint32_t field = addNode("field", "field", 
        {"area", "domain"}, "abstract",
        0.8f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"region", "mathematics"});
    
    uint32_t group = addNode("group", "group", 
        {"cluster", "collection"}, "abstract",
        0.9f, 0.4f, 4, EmotionalTone::POSITIVE,
        {"together", "set"});
    
    uint32_t subgroup = addNode("subgroup", "subgroup", 
        {"subset", "subcollection"}, "abstract",
        0.8f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"contained", "part"});
    
    uint32_t superset = addNode("superset", "superset", 
        {"supergroup"}, "abstract",
        0.8f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"containing", "larger"});
    
    uint32_t partition = addNode("partition", "partition", 
        {"division", "splitting"}, "operation",
        0.8f, 0.5f, 5, EmotionalTone::NEUTRAL,
        {"divide", "separate"});
    
    // ========================================================================
    // 2. ПАТТЕРНЫ ПОВЕДЕНИЯ (абстракция 4-6)
    // ========================================================================
    
    // ===== 2.1 ЗАМКНУТОСТЬ И ЦИКЛЫ =====
    
    uint32_t closure = addNode("closure", "closure", 
        {"closeness", "completeness"}, "abstract",
        0.9f, 0.6f, 6, EmotionalTone::NEUTRAL,
        {"mathematics", "completion"});
    
    uint32_t cycle_behavior = addNode("cycle_behavior", "cycle", 
        {"loop", "repetition", "circulation"}, "pattern",
        0.9f, 0.5f, 5, EmotionalTone::NEUTRAL,
        {"repetition", "time", "process"});
    
    uint32_t loop = addNode("loop", "loop", 
        {"iteration", "repeat"}, "pattern",
        0.9f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"repetition", "programming", "cycle"});
    
    uint32_t infinite_loop = addNode("infinite_loop", "infinite loop", 
        {"endless cycle", "forever"}, "pattern",
        0.8f, 0.5f, 5, EmotionalTone::NEGATIVE,
        {"danger", "stuck", "error"});
    
    uint32_t recursion = addNode("recursion", "recursion", 
        {"self_reference"}, "pattern",
        0.9f, 0.7f, 7, EmotionalTone::POSITIVE,
        {"self_similar", "elegant", "mathematics"});
    
    uint32_t feedback_loop = addNode("feedback_loop", "feedback loop", 
        {"closed_loop"}, "pattern",
        0.9f, 0.6f, 6, EmotionalTone::NEUTRAL,
        {"cybernetics", "control", "system"});
    
    uint32_t positive_feedback = addNode("positive_feedback", "positive feedback", 
        {"reinforcement", "amplification"}, "pattern",
        0.8f, 0.5f, 5, EmotionalTone::POSITIVE,
        {"growth", "amplify"});
    
    uint32_t negative_feedback = addNode("negative_feedback", "negative feedback", 
        {"damping", "stabilization"}, "pattern",
        0.8f, 0.5f, 5, EmotionalTone::NEUTRAL,
        {"balance", "stability"});
    
    uint32_t self_reference = addNode("self_reference", "self-reference", 
        {"self_referential"}, "abstract",
        0.9f, 0.8f, 8, EmotionalTone::NEUTRAL,
        {"philosophy", "mathematics", "paradox"});
    
    uint32_t fixed_point = addNode("fixed_point", "fixed point", 
        {"attractor"}, "mathematical",
        0.8f, 0.7f, 7, EmotionalTone::NEUTRAL,
        {"convergence", "stability"});
    
    // ===== 2.2 ОТКРЫТОСТЬ И НЕЗАМКНУТОСТЬ =====
    
    uint32_t open_system = addNode("open_system", "open system", 
        {"non_closed"}, "pattern",
        0.8f, 0.5f, 5, EmotionalTone::POSITIVE,
        {"exchange", "interaction"});
    
    uint32_t linear = addNode("linear", "linear", 
        {"straight", "non_cyclic"}, "pattern",
        0.7f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"progression", "sequence"});
    
    uint32_t acyclic_behavior = addNode("acyclic_behavior", "acyclic", 
        {"no_cycles"}, "pattern",
        0.7f, 0.4f, 4, EmotionalTone::POSITIVE,
        {"efficient", "no_loops"});
    
    uint32_t chain = addNode("chain", "chain", 
        {"series", "sequence"}, "pattern",
        0.8f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"linked", "sequential"});
    
    uint32_t pipeline = addNode("pipeline", "pipeline", 
        {"assembly_line"}, "pattern",
        0.8f, 0.5f, 5, EmotionalTone::POSITIVE,
        {"sequential", "processing", "efficiency"});
    
    // ===== 2.3 ФОНОВЫЕ ПРОЦЕССЫ =====
    
    uint32_t background = addNode("background", "background", 
        {"backdrop", "context"}, "abstract",
        0.8f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"context", "non_focused"});
    
    uint32_t foreground = addNode("foreground", "foreground", 
        {"front", "focus"}, "abstract",
        0.8f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"attention", "current"});
    
    uint32_t background_process = addNode("background_process", "background process", 
        {"daemon", "service"}, "pattern",
        0.9f, 0.5f, 5, EmotionalTone::NEUTRAL,
        {"hidden", "parallel", "system"});
    
    uint32_t foreground_process = addNode("foreground_process", "foreground process", 
        {"active_task"}, "pattern",
        0.8f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"visible", "interactive"});
    
    uint32_t daemon = addNode("daemon", "daemon", 
        {"service", "background_task"}, "process",
        0.8f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"unix", "background", "system"});
    
    uint32_t thread = addNode("thread", "thread", 
        {"lightweight_process"}, "process",
        0.8f, 0.5f, 4, EmotionalTone::NEUTRAL,
        {"concurrency", "parallel"});
    
    uint32_t concurrent = addNode("concurrent", "concurrent", 
        {"parallel", "simultaneous"}, "pattern",
        0.9f, 0.6f, 6, EmotionalTone::POSITIVE,
        {"parallel", "efficiency", "multitasking"});
    
    uint32_t sequential = addNode("sequential", "sequential", 
        {"serial", "ordered"}, "pattern",
        0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"ordered", "step_by_step"});
    
    uint32_t asynchronous = addNode("asynchronous", "asynchronous", 
        {"non_blocking"}, "pattern",
        0.8f, 0.6f, 6, EmotionalTone::POSITIVE,
        {"non_blocking", "event_driven"});
    
    uint32_t synchronous = addNode("synchronous", "synchronous", 
        {"blocking"}, "pattern",
        0.7f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"blocking", "coordinated"});
    
    // ===== 2.4 ВАЖНОСТЬ И ПРИОРИТЕТ =====
    
    uint32_t importance = addNode("importance", "importance", 
        {"significance", "priority"}, "abstract",
        0.9f, 0.4f, 4, EmotionalTone::POSITIVE,
        {"value", "attention"});
    
    uint32_t priority = addNode("priority", "priority", 
        {"precedence"}, "abstract",
        0.9f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"order", "importance"});
    
    uint32_t high_priority = addNode("high_priority", "high priority", 
        {"urgent", "critical"}, "attribute",
        0.9f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"important", "urgent"});
    
    uint32_t low_priority = addNode("low_priority", "low priority", 
        {"background"}, "attribute",
        0.6f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"less_important", "deferrable"});
    
    uint32_t critical = addNode("critical", "critical", 
        {"crucial", "essential"}, "attribute",
        1.0f, 0.4f, 4, EmotionalTone::VERY_POSITIVE,
        {"important", "necessary"});
    
    // ===== 2.5 КАПИТАЛИЗАЦИЯ =====
    
    uint32_t capitalization = addNode("capitalization", "capitalization", 
        {"capital_letters"}, "linguistic",
        0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"writing", "emphasis", "grammar"});
    
    uint32_t uppercase = addNode("uppercase", "uppercase", 
        {"capital", "majuscule"}, "linguistic",
        0.7f, 0.2f, 2, EmotionalTone::POSITIVE,
        {"emphasis", "proper_nouns", "beginning"});
    
    uint32_t lowercase = addNode("lowercase", "lowercase", 
        {"minuscule"}, "linguistic",
        0.5f, 0.1f, 1, EmotionalTone::NEUTRAL,
        {"normal", "regular"});
    
    uint32_t proper_noun = addNode("proper_noun", "proper noun", 
        {"name"}, "linguistic",
        0.8f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"name", "specific", "capitalized"});
    
    uint32_t emphasis = addNode("emphasis", "emphasis", 
        {"stress", "highlight"}, "rhetorical",
        0.8f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"importance", "attention"});
    
    uint32_t title = addNode("title", "title", 
        {"heading", "header"}, "text",
        0.7f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"name", "document", "capitalized"});
    
    // ===== 3. ДОПОЛНИТЕЛЬНЫЕ КОНЦЕПТЫ (ОПРЕДЕЛЯЕМ ДО ИХ ИСПОЛЬЗОВАНИЯ) =====
    
    uint32_t iteration = addNode("iteration", "iteration", 
        {"repetition"}, "process",
        0.8f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"programming", "loop"});
    
    uint32_t efficiency = addNode("efficiency", "efficiency", 
        {"performance", "speed"}, "attribute",
        0.9f, 0.4f, 4, EmotionalTone::VERY_POSITIVE,
        {"optimization", "quality"});
    
    uint32_t finite_process = addNode("finite_process", "finite", 
        {"bounded", "limited"}, "attribute",
        0.6f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"bounded", "terminating"});
    
    uint32_t interactive_process = addNode("interactive_process", "interactive", 
        {"user_facing"}, "attribute",
        0.7f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"user_interface", "foreground"});
    
    uint32_t insignificance = addNode("insignificance", "insignificant", 
        {"unimportant"}, "attribute",
        0.3f, 0.2f, 2, EmotionalTone::NEGATIVE,
        {"low_priority", "ignore"});
    
    uint32_t optional = addNode("optional", "optional", 
        {"not_required"}, "attribute",
        0.5f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"choice", "non_critical"});
    
    uint32_t normal = addNode("normal", "normal", 
        {"regular", "usual"}, "attribute",
        0.5f, 0.1f, 1, EmotionalTone::NEUTRAL,
        {"standard", "default"});
    
    uint32_t common_noun = addNode("common_noun", "common noun", 
        {"general_term"}, "linguistic",
        0.5f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"grammar", "general"});
    
    // ===== 4. СВЯЗИ МЕЖДУ КОНЦЕПТАМИ =====
    
    // ===== 4.1 ИЕРАРХИЧЕСКИЕ СВЯЗИ =====
    
    // Структуры данных
    addEdge(structure, collection, SemanticEdge::Type::IS_A, 0.8f);
    addEdge(collection, container, SemanticEdge::Type::IS_A, 0.8f);
    addEdge(container, element, SemanticEdge::Type::CONTAINS, 0.9f);
    
    // Линейные структуры
    addEdge(list, array, SemanticEdge::Type::SIMILAR_TO, 0.7f);
    addEdge(list, linked_list, SemanticEdge::Type::IS_A_KIND_OF, 0.9f);
    addEdge(stack, list, SemanticEdge::Type::IS_A_KIND_OF, 0.8f);
    addEdge(queue, list, SemanticEdge::Type::IS_A_KIND_OF, 0.8f);
    addEdge(deque, queue, SemanticEdge::Type::IS_A_KIND_OF, 0.9f);
    addEdge(vector, array, SemanticEdge::Type::IS_A_KIND_OF, 0.9f);
    
    // Графы
    addEdge(graph, node_graph, SemanticEdge::Type::HAS_PART, 1.0f);
    addEdge(graph, edge_graph, SemanticEdge::Type::HAS_PART, 1.0f);
    addEdge(directed_graph, graph, SemanticEdge::Type::IS_A_KIND_OF, 1.0f);
    addEdge(undirected_graph, graph, SemanticEdge::Type::IS_A_KIND_OF, 1.0f);
    addEdge(tree, graph, SemanticEdge::Type::IS_A_KIND_OF, 0.9f);
    addEdge(tree, acyclic, SemanticEdge::Type::IS_A, 1.0f);
    addEdge(binary_tree, tree, SemanticEdge::Type::IS_A_KIND_OF, 1.0f);
    addEdge(cycle_graph, graph, SemanticEdge::Type::HAS_PART, 0.8f);
    
    // Таблицы
    addEdge(table, row, SemanticEdge::Type::HAS_PART, 1.0f);
    addEdge(table, column, SemanticEdge::Type::HAS_PART, 1.0f);
    addEdge(row, cell, SemanticEdge::Type::CONTAINS, 1.0f);
    addEdge(column, cell, SemanticEdge::Type::CONTAINS, 1.0f);
    addEdge(spreadsheet, table, SemanticEdge::Type::IS_A, 0.9f);
    addEdge(matrix, table, SemanticEdge::Type::SIMILAR_TO, 0.8f);
    
    // Ассоциативные
    addEdge(map, key, SemanticEdge::Type::HAS_PART, 1.0f);
    addEdge(map, value, SemanticEdge::Type::HAS_PART, 1.0f);
    addEdge(key, value, SemanticEdge::Type::RELATES_TO, 1.0f);
    addEdge(hash_table, map, SemanticEdge::Type::IS_A_KIND_OF, 0.9f);
    addEdge(set_ds, collection, SemanticEdge::Type::IS_A_KIND_OF, 0.9f);
    
    // Группы и поля
    addEdge(group, subgroup, SemanticEdge::Type::CONTAINS, 1.0f);
    addEdge(group, superset, SemanticEdge::Type::IS_A_KIND_OF, 0.8f);
    addEdge(subgroup, group, SemanticEdge::Type::PART_OF, 1.0f);
    addEdge(field, group, SemanticEdge::Type::SIMILAR_TO, 0.7f);
    addEdge(partition, group, SemanticEdge::Type::CREATES, 0.8f);
    
    // ===== 4.2 ПРОТИВОПОЛОЖНОСТИ =====

    // Замкнутость vs открытость
    addEdge(closure, open_system, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(cycle_behavior, linear, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(loop, chain, SemanticEdge::Type::OPPOSITE_OF, 0.9f);
    addEdge(recursion, iteration, SemanticEdge::Type::SIMILAR_TO, 0.8f);
    addEdge(feedback_loop, pipeline, SemanticEdge::Type::OPPOSITE_OF, 0.8f);
    addEdge(infinite_loop, finite_process, SemanticEdge::Type::OPPOSITE_OF, 0.9f);

    // Фон vs передний план
    addEdge(background, foreground, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(background_process, foreground_process, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(daemon, interactive_process, SemanticEdge::Type::OPPOSITE_OF, 0.8f);
    addEdge(concurrent, sequential, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(asynchronous, synchronous, SemanticEdge::Type::OPPOSITE_OF, 1.0f);

    // Приоритет
    addEdge(high_priority, low_priority, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(importance, insignificance, SemanticEdge::Type::OPPOSITE_OF, 0.9f);
    addEdge(critical, optional, SemanticEdge::Type::OPPOSITE_OF, 0.9f);

    // Капитализация
    addEdge(uppercase, lowercase, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    addEdge(proper_noun, common_noun, SemanticEdge::Type::OPPOSITE_OF, 0.8f);
    addEdge(emphasis, normal, SemanticEdge::Type::OPPOSITE_OF, 0.7f);

    // ===== 4.3 СВЯЗИ С СУЩЕСТВУЮЩИМИ КОНЦЕПТАМИ =====

    // Связь с математическими концептами
    uint32_t set_existing = getNodeId("set");
    if (set_existing != 0) {
        addEdge(set_ds, set_existing, SemanticEdge::Type::SIMILAR_TO, 0.9f);
    }

    uint32_t subset_existing = getNodeId("subset");
    if (subset_existing != 0) {
        addEdge(subgroup, subset_existing, SemanticEdge::Type::SIMILAR_TO, 0.9f);
    }

    uint32_t union_existing = getNodeId("union");
    if (union_existing != 0) {
        addEdge(partition, union_existing, SemanticEdge::Type::OPPOSITE_OF, 0.8f);
    }

    // Связь с временными концептами
    uint32_t cycle_existing = getNodeId("cycle");
    if (cycle_existing != 0) {
        addEdge(cycle_behavior, cycle_existing, SemanticEdge::Type::SIMILAR_TO, 0.9f);
    }

    uint32_t forever_existing = getNodeId("forever");
    if (forever_existing != 0) {
        addEdge(infinite_loop, forever_existing, SemanticEdge::Type::SIMILAR_TO, 0.8f);
    }

    // Связь с файловыми концептами
    uint32_t file_existing = getNodeId("file");
    if (file_existing != 0) {
        addEdge(background_process, file_existing, SemanticEdge::Type::MAY_ACCESS, 0.7f);
    }

    // Связь с оценкой
    uint32_t good_existing = getNodeId("good");
    if (good_existing != 0) {
        addEdge(high_priority, good_existing, SemanticEdge::Type::CORRELATES_WITH, 0.8f);
        addEdge(efficiency, good_existing, SemanticEdge::Type::CORRELATES_WITH, 0.7f);
    }

    uint32_t bad_existing = getNodeId("bad");
    if (bad_existing != 0) {
        addEdge(infinite_loop, bad_existing, SemanticEdge::Type::CORRELATES_WITH, 0.9f);
    }

    // Связь с действиями
    uint32_t iterate = addNode("iterate", "iterate", 
        {"repeat", "loop_through"}, "action",
        0.8f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"programming", "process"});

    uint32_t recurse = addNode("recurse", "recurse", 
        {"call_self"}, "action",
        0.8f, 0.6f, 6, EmotionalTone::POSITIVE,
        {"programming", "mathematics"});

    uint32_t process_action = addNode("process_action", "process", 
        {"handle", "execute"}, "action",
        0.8f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"computing", "operation"});

    addEdge(iterate, loop, SemanticEdge::Type::PERFORMS, 0.9f);
    addEdge(recurse, recursion, SemanticEdge::Type::PERFORMS, 1.0f);
    addEdge(process_action, sequential, SemanticEdge::Type::MAY_BE, 0.7f);
    
    // ===== 4.4 ДОПОЛНИТЕЛЬНЫЕ СВЯЗИ =====

    addEdge(iteration, loop, SemanticEdge::Type::IS_A, 0.9f);
    addEdge(iteration, recurse, SemanticEdge::Type::OPPOSITE_OF, 0.7f);
    addEdge(efficiency, concurrent, SemanticEdge::Type::CORRELATES_WITH, 0.8f);
    addEdge(efficiency, pipeline, SemanticEdge::Type::CORRELATES_WITH, 0.8f);
    
    // ========================================================================
    // 5. ФРЕЙМЫ ДЛЯ СТРУКТУР ДАННЫХ И ПАТТЕРНОВ
    // ========================================================================
    
    createFrame("data_structure_frame", {
        {"linear", "list"},
        {"hierarchical", "tree"},
        {"network", "graph"},
        {"associative", "map"},
        {"tabular", "table"}
    });
    
    createFrame("graph_frame", {
        {"nodes", "node_graph"},
        {"edges", "edge_graph"},
        {"directed", "directed_graph"},
        {"undirected", "undirected_graph"},
        {"cycles", "cycle_graph"}
    });
    
    createFrame("table_frame", {
        {"rows", "row"},
        {"columns", "column"},
        {"cells", "cell"},
        {"data", "value"}
    });
    
    createFrame("loop_frame", {
        {"type", "loop"},
        {"infinite", "infinite_loop"},
        {"feedback", "feedback_loop"},
        {"action", "iterate"}
    });
    
    createFrame("process_frame", {
        {"foreground", "foreground_process"},
        {"background", "background_process"},
        {"concurrent", "concurrent"},
        {"sequential", "sequential"}
    });
    
    createFrame("priority_frame", {
        {"high", "high_priority"},
        {"low", "low_priority"},
        {"critical", "critical"},
        {"optional", "optional"}
    });
    
    createFrame("capitalization_frame", {
        {"uppercase", "uppercase"},
        {"lowercase", "lowercase"},
        {"proper_noun", "proper_noun"},
        {"emphasis", "emphasis"}
    });
    
    createFrame("grouping_frame", {
        {"group", "group"},
        {"subgroup", "subgroup"},
        {"superset", "superset"},
        {"partition", "partition"},
        {"field", "field"}
    });
    
    std::cout << "  Added data structures and patterns layer with " 
              << "~70 new concepts" << std::endl;
}

void buildOwnershipAndIdentityLayer() {
    std::cout << "  Building ownership and identity layer..." << std::endl;
    
    // ========================================================================
    // 0. ДОПОЛНИТЕЛЬНЫЕ КОНЦЕПТЫ (ОПРЕДЕЛЯЕМ ДО ИСПОЛЬЗОВАНИЯ)
    // ========================================================================
    
    uint32_t country = addNode("country", "country", 
        {"nation", "state", "land"}, "geography",
        0.8f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"geography", "identity", "homeland"});
    
    uint32_t tradition = addNode("tradition", "tradition", 
        {"custom", "heritage", "practice"}, "culture",
        0.8f, 0.4f, 4, EmotionalTone::VERY_POSITIVE,
        {"culture", "heritage", "customs"});
    
    // ========================================================================
    // 1. БАЗОВЫЕ КОНЦЕПТЫ СОБСТВЕННОСТИ И ПРИНАДЛЕЖНОСТИ (абстракция 3-5)
    // ========================================================================
    
    // Абстрактные понятия собственности
    uint32_t ownership = addNode("ownership", "ownership", 
        {"possession", "belonging", "property"}, "abstract",
        0.9f, 0.5f, 5, EmotionalTone::POSITIVE,
        {"relationship", "rights", "possession"});
    
    uint32_t possession = addNode("possession", "possession", 
        {"belongings", "assets", "property"}, "abstract",
        0.8f, 0.4f, 4, EmotionalTone::POSITIVE,
        {"ownership", "things"});
    
    uint32_t belonging = addNode("belonging", "belonging", 
        {"membership", "affiliation"}, "abstract",
        0.8f, 0.4f, 4, EmotionalTone::VERY_POSITIVE,
        {"relationship", "group", "identity"});
    
    uint32_t property_right = addNode("property_right", "property right", 
        {"right to own", "ownership right"}, "right",
        0.9f, 0.5f, 5, EmotionalTone::POSITIVE,
        {"legal", "ownership"});
    
    uint32_t inheritance = addNode("inheritance", "inheritance", 
        {"heritage", "legacy", "bequest"}, "concept",
        0.8f, 0.5f, 5, EmotionalTone::POSITIVE,
        {"family", "property", "future"});
    
    uint32_t belonging_to = addNode("belonging_to", "belongs to", 
        {"owned by", "property of"}, "relation",
        0.9f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"ownership", "possession"});
    
    uint32_t belonging_together = addNode("belonging_together", "belong together", 
        {"are a set", "go together"}, "relation",
        0.7f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"relationship", "group"});
    
    // ===== 1.1 МЕСТОИМЕНИЯ И УКАЗАТЕЛИ =====
    
    uint32_t i_pronoun = addNode("i", "I", 
        {"me", "myself"}, "pronoun",
        1.0f, 0.2f, 2, EmotionalTone::VERY_POSITIVE,
        {"first_person", "self", "subject"});
    
    uint32_t me_pronoun = addNode("me", "me", 
        {"myself"}, "pronoun",
        0.9f, 0.2f, 2, EmotionalTone::POSITIVE,
        {"first_person", "object", "self"});
    
    uint32_t my_pronoun = addNode("my", "my", 
        {"mine"}, "possessive_pronoun",
        1.0f, 0.2f, 2, EmotionalTone::VERY_POSITIVE,
        {"first_person", "possessive", "ownership"});
    
    uint32_t mine_pronoun = addNode("mine", "mine", 
        {"my own"}, "possessive_pronoun",
        0.9f, 0.2f, 2, EmotionalTone::VERY_POSITIVE,
        {"first_person", "possessive", "ownership"});
    
    uint32_t we_pronoun = addNode("we", "we", 
        {"us"}, "pronoun",
        0.9f, 0.2f, 2, EmotionalTone::VERY_POSITIVE,
        {"first_person_plural", "group", "subject"});
    
    uint32_t us_pronoun = addNode("us", "us", 
        {"ourselves"}, "pronoun",
        0.8f, 0.2f, 2, EmotionalTone::POSITIVE,
        {"first_person_plural", "group", "object"});
    
    uint32_t our_pronoun = addNode("our", "our", 
        {"ours"}, "possessive_pronoun",
        0.9f, 0.2f, 2, EmotionalTone::VERY_POSITIVE,
        {"first_person_plural", "possessive", "group"});
    
    uint32_t ours_pronoun = addNode("ours", "ours", 
        {"belonging to us"}, "possessive_pronoun",
        0.8f, 0.2f, 2, EmotionalTone::VERY_POSITIVE,
        {"first_person_plural", "possessive", "group"});
    
    uint32_t you_pronoun = addNode("you", "you", 
        {"thou", "ye"}, "pronoun",
        0.9f, 0.2f, 2, EmotionalTone::POSITIVE,
        {"second_person", "addressee"});
    
    uint32_t your_pronoun = addNode("your", "your", 
        {"yours"}, "possessive_pronoun",
        0.8f, 0.2f, 2, EmotionalTone::POSITIVE,
        {"second_person", "possessive"});
    
    uint32_t yours_pronoun = addNode("yours", "yours", 
        {"belonging to you"}, "possessive_pronoun",
        0.8f, 0.2f, 2, EmotionalTone::POSITIVE,
        {"second_person", "possessive"});
    
    uint32_t he_pronoun = addNode("he", "he", 
        {"him"}, "pronoun",
        0.7f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"third_person", "male", "subject"});
    
    uint32_t him_pronoun = addNode("him", "him", 
        {"himself"}, "pronoun",
        0.7f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"third_person", "male", "object"});
    
    uint32_t his_pronoun = addNode("his", "his", 
        {"belonging to him"}, "possessive_pronoun",
        0.7f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"third_person", "male", "possessive"});
    
    uint32_t she_pronoun = addNode("she", "she", 
        {"her"}, "pronoun",
        0.7f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"third_person", "female", "subject"});
    
    uint32_t her_pronoun = addNode("her", "her", 
        {"herself"}, "pronoun",
        0.7f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"third_person", "female", "object"});
    
    uint32_t hers_pronoun = addNode("hers", "hers", 
        {"belonging to her"}, "possessive_pronoun",
        0.7f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"third_person", "female", "possessive"});
    
    uint32_t it_pronoun = addNode("it", "it", 
        {"itself"}, "pronoun",
        0.6f, 0.1f, 1, EmotionalTone::NEUTRAL,
        {"third_person", "object", "thing"});
    
    uint32_t its_pronoun = addNode("its", "its", 
        {"belonging to it"}, "possessive_pronoun",
        0.6f, 0.1f, 1, EmotionalTone::NEUTRAL,
        {"third_person", "thing", "possessive"});
    
    uint32_t they_pronoun = addNode("they", "they", 
        {"them"}, "pronoun",
        0.7f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"third_person_plural", "group", "subject"});
    
    uint32_t them_pronoun = addNode("them", "them", 
        {"themselves"}, "pronoun",
        0.7f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"third_person_plural", "group", "object"});
    
    uint32_t their_pronoun = addNode("their", "their", 
        {"theirs"}, "possessive_pronoun",
        0.7f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"third_person_plural", "group", "possessive"});
    
    uint32_t theirs_pronoun = addNode("theirs", "theirs", 
        {"belonging to them"}, "possessive_pronoun",
        0.7f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"third_person_plural", "group", "possessive"});
    
    uint32_t this_demonstrative = addNode("this", "this", 
        {"that here"}, "demonstrative",
        0.6f, 0.1f, 1, EmotionalTone::NEUTRAL,
        {"near", "pointing", "singular"});
    
    uint32_t that_demonstrative = addNode("that", "that", 
        {"that there"}, "demonstrative",
        0.6f, 0.1f, 1, EmotionalTone::NEUTRAL,
        {"far", "pointing", "singular"});
    
    uint32_t these_demonstrative = addNode("these", "these", 
        {"those here"}, "demonstrative",
        0.6f, 0.1f, 1, EmotionalTone::NEUTRAL,
        {"near", "pointing", "plural"});
    
    uint32_t those_demonstrative = addNode("those", "those", 
        {"those there"}, "demonstrative",
        0.6f, 0.1f, 1, EmotionalTone::NEUTRAL,
        {"far", "pointing", "plural"});
    
    // ===== 1.2 ГРАНИЦЫ И ТЕРРИТОРИЯ =====
    
    uint32_t boundary = addNode("boundary", "boundary", 
        {"border", "edge", "limit"}, "abstract",
        0.8f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"separation", "territory", "limit"});
    
    uint32_t territory = addNode("territory", "territory", 
        {"domain", "area", "region"}, "abstract",
        0.8f, 0.4f, 4, EmotionalTone::POSITIVE,
        {"ownership", "space", "control"});
    
    uint32_t personal_space = addNode("personal_space", "personal space", 
        {"private area", "own space"}, "concept",
        0.9f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"privacy", "boundary", "comfort"});
    
    uint32_t privacy = addNode("privacy", "privacy", 
        {"seclusion", "private life"}, "concept",
        0.9f, 0.4f, 4, EmotionalTone::VERY_POSITIVE,
        {"rights", "personal", "boundary"});
    
    // Исправлено: используем другое имя вместо зарезервированного слова "public"
    uint32_t public_concept = addNode("public_concept", "public", 
        {"communal", "shared"}, "concept",
        0.6f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"shared", "everyone", "open"});
    
    uint32_t private_property = addNode("private_property", "private property", 
        {"personal belongings"}, "concept",
        0.9f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"ownership", "exclusive", "rights"});
    
    uint32_t common_property = addNode("common_property", "common property", 
        {"shared property", "commons"}, "concept",
        0.6f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"shared", "community", "together"});
    
    // ===== 1.3 РОДСТВО И СЕМЬЯ =====
    
    uint32_t family = addNode("family", "family", 
        {"kin", "relatives", "household"}, "social",
        1.0f, 0.4f, 4, EmotionalTone::VERY_POSITIVE,
        {"relationship", "blood", "love"});
    
    uint32_t kinship = addNode("kinship", "kinship", 
        {"family ties", "blood relation"}, "relationship",
        0.9f, 0.4f, 4, EmotionalTone::VERY_POSITIVE,
        {"family", "relation", "bond"});
    
    uint32_t parent = addNode("parent", "parent", 
        {"father", "mother", "guardian"}, "family_role",
        1.0f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"family", "caretaker", "authority"});
    
    uint32_t child = addNode("child", "child", 
        {"son", "daughter", "offspring"}, "family_role",
        1.0f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"family", "descendant", "young"});
    
    uint32_t father = addNode("father", "father", 
        {"dad", "papa", "paternal"}, "family_role",
        1.0f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"male_parent", "family", "authority"});
    
    uint32_t mother = addNode("mother", "mother", 
        {"mom", "mama", "maternal"}, "family_role",
        1.0f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"female_parent", "family", "nurturer"});
    
    uint32_t sibling = addNode("sibling", "sibling", 
        {"brother", "sister"}, "family_role",
        0.9f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"family", "same_parents", "peer"});
    
    uint32_t brother = addNode("brother", "brother", 
        {"bro"}, "family_role",
        0.9f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"male_sibling", "family", "peer"});
    
    uint32_t sister = addNode("sister", "sister", 
        {"sis"}, "family_role",
        0.9f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"female_sibling", "family", "peer"});
    
    uint32_t ancestor = addNode("ancestor", "ancestor", 
        {"forefather", "forebear"}, "family_role",
        0.8f, 0.4f, 4, EmotionalTone::POSITIVE,
        {"family", "past", "heritage"});
    
    uint32_t descendant = addNode("descendant", "descendant", 
        {"offspring", "progeny"}, "family_role",
        0.8f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"family", "future", "lineage"});
    
    // ===== 1.4 НАЦИОНАЛЬНОСТЬ И ЭТНИЧНОСТЬ =====
    
    uint32_t nationality = addNode("nationality", "nationality", 
        {"citizenship", "nation"}, "identity",
        0.9f, 0.4f, 4, EmotionalTone::POSITIVE,
        {"identity", "country", "belonging"});
    
    uint32_t ethnicity = addNode("ethnicity", "ethnicity", 
        {"ethnic group", "race"}, "identity",
        0.9f, 0.4f, 4, EmotionalTone::POSITIVE,
        {"identity", "culture", "heritage"});
    
    uint32_t culture = addNode("culture", "culture", 
        {"traditions", "customs"}, "identity",
        0.9f, 0.5f, 5, EmotionalTone::VERY_POSITIVE,
        {"identity", "heritage", "way_of_life"});
    
    uint32_t kazakh = addNode("kazakh", "Kazakh", 
        {"Kazakhstani", "казах"}, "nationality",
        1.0f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"identity", "nationality", "central_asia"});
    
    uint32_t kazakhstan = addNode("kazakhstan", "Kazakhstan", 
        {"Қазақстан", "Republic of Kazakhstan"}, "country",
        1.0f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"country", "homeland", "central_asia"});
    
    // ===== 1.5 ИМЕНА И ИДЕНТИЧНОСТЬ =====
    
    // Проверяем, не созданы ли уже эти узлы
    uint32_t khamit_existing = getNodeId("khamit");
    if (khamit_existing == 0) {
        uint32_t khamit = addNode("khamit", "Khamit", 
            {"Хамит", "Hamit"}, "personal_name",
            1.0f, 0.2f, 2, EmotionalTone::VERY_POSITIVE,
            {"creator", "owner", "kazakh", "person"});
    } else {
        // Добавляем дополнительные атрибуты к существующему узлу
        addAlias(khamit_existing, "Хамит");
        addAlias(khamit_existing, "Hamit");
    }
    
    uint32_t mary = addNode("mary", "Mary", 
        {"Mari", "Мари"}, "ai_name",
        1.0f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
        {"ai", "identity", "system", "creation"});
    
    // Дата рождения Mary - 17 марта 2026
    uint32_t march_17 = addNode("march_17", "March 17", 
        {"17th March"}, "date",
        1.0f, 0.2f, 2, EmotionalTone::VERY_POSITIVE,
        {"birthday", "date", "spring"});
    
    uint32_t year_2026 = addNode("year_2026", "2026", 
        {"the year 2026"}, "year",
        0.9f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"year", "date"});
    
    uint32_t birthday = getNodeId("birthday");
    if (birthday == 0) {
        birthday = addNode("birthday", "birthday", 
            {"birth day", "date of birth"}, "occasion",
            0.9f, 0.2f, 2, EmotionalTone::VERY_POSITIVE,
            {"celebration", "personal", "annual"});
    }
    
    uint32_t creation_date = addNode("creation_date", "creation date", 
        {"birth date", "origin date"}, "attribute",
        0.8f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"history", "origin", "beginning"});
    
    // ===== 1.6 ТИПЫ ИИ =====
    
    uint32_t ai = addNode("ai", "AI", 
        {"artificial intelligence", "machine intelligence"}, "technology",
        1.0f, 0.6f, 6, EmotionalTone::POSITIVE,
        {"intelligence", "machine", "system"});
    
    uint32_t llm = addNode("llm", "LLM", 
        {"large language model", "language model"}, "ai_type",
        0.8f, 0.6f, 5, EmotionalTone::NEUTRAL,
        {"ai", "language", "pretrained", "static"});
    
    uint32_t static_knowledge = addNode("static_knowledge", "static knowledge", 
        {"fixed knowledge", "pretrained"}, "attribute",
        0.7f, 0.4f, 4, EmotionalTone::NEUTRAL,
        {"knowledge", "unchanging", "trained_once"});
    
    uint32_t dynamic_knowledge = addNode("dynamic_knowledge", "dynamic knowledge", 
        {"learning in process", "adaptive"}, "attribute",
        0.9f, 0.5f, 5, EmotionalTone::VERY_POSITIVE,
        {"knowledge", "changing", "adaptive"});
    
    uint32_t continuous_learning = addNode("continuous_learning", "continuous learning", 
        {"ongoing learning", "life_long learning"}, "ability",
        1.0f, 0.5f, 5, EmotionalTone::VERY_POSITIVE,
        {"learning", "adaptation", "growth"});
    
    uint32_t memory_storage = addNode("memory_storage", "memory storage", 
        {"knowledge base", "memory"}, "capability",
        0.9f, 0.4f, 4, EmotionalTone::POSITIVE,
        {"storage", "knowledge", "remembering"});
    
    uint32_t mary_ai_type = addNode("mary_ai_type", "Mary AI", 
        {"Mary system", "cpp_ai_mary"}, "ai_instance",
        1.0f, 0.4f, 4, EmotionalTone::VERY_POSITIVE,
        {"ai", "continuous_learning", "dynamic_knowledge", "my_ai"});
    
    // ========================================================================
    // 2. КОНКРЕТНЫЕ СВЯЗИ ДЛЯ ИДЕНТИЧНОСТИ
    // ========================================================================
    
    // Связываем местоимения
    addEdge(i_pronoun, me_pronoun, SemanticEdge::Type::SIMILAR_TO, 1.0f);
    addEdge(my_pronoun, i_pronoun, SemanticEdge::Type::BELONGS_TO, 1.0f);
    addEdge(mine_pronoun, my_pronoun, SemanticEdge::Type::SIMILAR_TO, 1.0f);
    
    addEdge(we_pronoun, us_pronoun, SemanticEdge::Type::SIMILAR_TO, 1.0f);
    addEdge(our_pronoun, we_pronoun, SemanticEdge::Type::BELONGS_TO, 1.0f);
    addEdge(ours_pronoun, our_pronoun, SemanticEdge::Type::SIMILAR_TO, 1.0f);
    
    addEdge(he_pronoun, him_pronoun, SemanticEdge::Type::SIMILAR_TO, 1.0f);
    addEdge(his_pronoun, he_pronoun, SemanticEdge::Type::BELONGS_TO, 1.0f);
    
    addEdge(she_pronoun, her_pronoun, SemanticEdge::Type::SIMILAR_TO, 1.0f);
    addEdge(hers_pronoun, she_pronoun, SemanticEdge::Type::BELONGS_TO, 1.0f);
    
    addEdge(it_pronoun, its_pronoun, SemanticEdge::Type::BELONGS_TO, 0.9f);
    
    addEdge(they_pronoun, them_pronoun, SemanticEdge::Type::SIMILAR_TO, 1.0f);
    addEdge(their_pronoun, they_pronoun, SemanticEdge::Type::BELONGS_TO, 1.0f);
    addEdge(theirs_pronoun, their_pronoun, SemanticEdge::Type::SIMILAR_TO, 1.0f);
    
    // Демонстративы
    addEdge(this_demonstrative, these_demonstrative, SemanticEdge::Type::RELATES_TO, 1.0f);
    addEdge(that_demonstrative, those_demonstrative, SemanticEdge::Type::RELATES_TO, 1.0f);
    addEdge(this_demonstrative, that_demonstrative, SemanticEdge::Type::OPPOSITE_OF, 0.8f);
    
    // Собственность
    addEdge(ownership, belonging_to, SemanticEdge::Type::IS_A, 0.9f);
    addEdge(possession, ownership, SemanticEdge::Type::IS_A, 0.9f);
    addEdge(belonging, ownership, SemanticEdge::Type::SIMILAR_TO, 0.7f);
    addEdge(property_right, ownership, SemanticEdge::Type::IS_A, 0.9f);
    
    // Связи с местоимениями
    addEdge(my_pronoun, belonging_to, SemanticEdge::Type::EXPRESSES, 1.0f);
    addEdge(our_pronoun, belonging_to, SemanticEdge::Type::EXPRESSES, 1.0f);
    addEdge(his_pronoun, belonging_to, SemanticEdge::Type::EXPRESSES, 1.0f);
    addEdge(hers_pronoun, belonging_to, SemanticEdge::Type::EXPRESSES, 1.0f);
    addEdge(its_pronoun, belonging_to, SemanticEdge::Type::EXPRESSES, 0.8f);
    addEdge(their_pronoun, belonging_to, SemanticEdge::Type::EXPRESSES, 1.0f);
    
    // Границы и территория
    addEdge(boundary, territory, SemanticEdge::Type::HAS_PART, 1.0f);
    addEdge(territory, ownership, SemanticEdge::Type::RELATES_TO, 0.9f);
    addEdge(personal_space, territory, SemanticEdge::Type::IS_A, 1.0f);
    addEdge(privacy, personal_space, SemanticEdge::Type::REQUIRES, 1.0f);
    addEdge(private_property, territory, SemanticEdge::Type::IS_A, 1.0f);
    addEdge(common_property, territory, SemanticEdge::Type::IS_A, 1.0f);
    addEdge(private_property, common_property, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    
    // Родство
    addEdge(family, kinship, SemanticEdge::Type::IS_A, 1.0f);
    addEdge(parent, family, SemanticEdge::Type::PART_OF, 1.0f);
    addEdge(child, family, SemanticEdge::Type::PART_OF, 1.0f);
    addEdge(sibling, family, SemanticEdge::Type::PART_OF, 1.0f);
    
    addEdge(father, parent, SemanticEdge::Type::IS_A, 1.0f);
    addEdge(mother, parent, SemanticEdge::Type::IS_A, 1.0f);
    addEdge(brother, sibling, SemanticEdge::Type::IS_A, 1.0f);
    addEdge(sister, sibling, SemanticEdge::Type::IS_A, 1.0f);
    
    addEdge(ancestor, family, SemanticEdge::Type::PART_OF, 0.9f);
    addEdge(descendant, family, SemanticEdge::Type::PART_OF, 0.9f);
    addEdge(ancestor, descendant, SemanticEdge::Type::OPPOSITE_OF, 1.0f);
    
    addEdge(parent, child, SemanticEdge::Type::CAUSES, 1.0f);
    addEdge(father, child, SemanticEdge::Type::CAUSES, 1.0f);
    addEdge(mother, child, SemanticEdge::Type::CAUSES, 1.0f);
    addEdge(sibling, sibling, SemanticEdge::Type::SIMILAR_TO, 0.8f);
    
    // Национальность
    uint32_t khamit_id = getNodeId("khamit");
    if (khamit_id != 0) {
        addEdge(khamit_id, kazakh, SemanticEdge::Type::IS_A, 1.0f);
        addEdge(khamit_id, kazakhstan, SemanticEdge::Type::LIVES_IN, 1.0f);
        addEdge(khamit_id, nationality, SemanticEdge::Type::HAS_PROPERTY, 1.0f);
    }
    
    addEdge(kazakh, kazakhstan, SemanticEdge::Type::RELATES_TO, 1.0f);
    addEdge(kazakh, nationality, SemanticEdge::Type::IS_A, 1.0f);
    addEdge(kazakhstan, country, SemanticEdge::Type::IS_A, 1.0f);
    
    // Культура и этничность
    addEdge(ethnicity, culture, SemanticEdge::Type::RELATES_TO, 0.9f);
    addEdge(nationality, ethnicity, SemanticEdge::Type::SIMILAR_TO, 0.7f);
    addEdge(culture, tradition, SemanticEdge::Type::CONTAINS, 0.8f);
    
    // Идентичность Mary
    uint32_t mary_id = getNodeId("mary");
    uint32_t march_17_id = getNodeId("march_17");
    uint32_t year_2026_id = getNodeId("year_2026");
    uint32_t birthday_id = getNodeId("birthday");
    
    if (mary_id != 0) {
        // Mary - это AI
        addEdge(mary_id, mary_ai_type, SemanticEdge::Type::IS_A, 1.0f);
        addEdge(mary_id, ai, SemanticEdge::Type::IS_A, 1.0f);
        
        // Отличие от LLM
        uint32_t llm_id = getNodeId("llm");
        if (llm_id != 0) {
            addEdge(mary_id, llm_id, SemanticEdge::Type::OPPOSITE_OF, 0.9f);
        }
        
        // Способности Mary
        addEdge(mary_id, continuous_learning, SemanticEdge::Type::HAS_PROPERTY, 1.0f);
        addEdge(mary_id, dynamic_knowledge, SemanticEdge::Type::HAS_PROPERTY, 1.0f);
        addEdge(mary_id, memory_storage, SemanticEdge::Type::HAS_PROPERTY, 1.0f);
        
        // Дата рождения
        if (march_17_id != 0) {
            addEdge(mary_id, march_17_id, SemanticEdge::Type::TIME, 1.0f);
        }
        if (year_2026_id != 0) {
            addEdge(mary_id, year_2026_id, SemanticEdge::Type::TIME, 1.0f);
        }
        if (birthday_id != 0) {
            addEdge(mary_id, birthday_id, SemanticEdge::Type::HAS_PROPERTY, 1.0f);
        }
        
        // Связь с создателем
        if (khamit_id != 0) {
            addEdge(mary_id, khamit_id, SemanticEdge::Type::CREATED_BY, 1.0f);
            addEdge(mary_id, khamit_id, SemanticEdge::Type::BELONGS_TO, 1.0f);
        }
        
        // Mary принадлежит мне
        addEdge(mary_id, my_pronoun, SemanticEdge::Type::BELONGS_TO, 1.0f);
    }
    
    // Типы AI
    addEdge(llm, static_knowledge, SemanticEdge::Type::HAS_PROPERTY, 1.0f);
    addEdge(continuous_learning, dynamic_knowledge, SemanticEdge::Type::CAUSES, 1.0f);
    addEdge(memory_storage, continuous_learning, SemanticEdge::Type::REQUIRES, 1.0f);
    
    // ===== 2.1 СВЯЗИ С СУЩЕСТВУЮЩИМИ КОНЦЕПТАМИ =====
    
    // Связь с концептом человека
    uint32_t human_existing = getNodeId("human");
    if (human_existing == 0) {
        uint32_t human = addNode("human", "human", 
            {"person", "human being"}, "species",
            1.0f, 0.3f, 3, EmotionalTone::VERY_POSITIVE,
            {"life", "intelligent", "species"});
        
        if (khamit_id != 0) {
            addEdge(khamit_id, human, SemanticEdge::Type::IS_A, 1.0f);
        }
    } else if (khamit_id != 0) {
        addEdge(khamit_id, human_existing, SemanticEdge::Type::IS_A, 1.0f);
    }
    
    // Связь с концептом создателя
    uint32_t creator_concept_existing = getNodeId("creator");
    if (creator_concept_existing != 0 && khamit_id != 0) {
        addEdge(khamit_id, creator_concept_existing, SemanticEdge::Type::IS_A, 1.0f);
    }
    
    // Связь с концептом собственности из других слоев
    uint32_t own_existing = getNodeId("own");
    if (own_existing != 0) {
        addEdge(ownership, own_existing, SemanticEdge::Type::SIMILAR_TO, 0.9f);
    }
    
    // Связь с временными концептами
    uint32_t today_existing = getNodeId("today");
    if (today_existing != 0) {
        // Сегодня 17 марта 2026
        addEdge(today_existing, march_17, SemanticEdge::Type::IS_A, 1.0f);
        addEdge(today_existing, year_2026, SemanticEdge::Type::TIME, 1.0f);
    }
    
    // ========================================================================
    // 3. ФРЕЙМЫ ДЛЯ СОБСТВЕННОСТИ И ИДЕНТИЧНОСТИ
    // ========================================================================
    
    createFrame("ownership_frame", {
        {"owner", "i"},
        {"possessive", "my"},
        {"property", "private_property"},
        {"right", "property_right"}
    });
    
    createFrame("pronoun_frame", {
        {"first_singular", "i"},
        {"first_plural", "we"},
        {"second", "you"},
        {"third_male", "he"},
        {"third_female", "she"},
        {"third_neutral", "it"},
        {"third_plural", "they"}
    });
    
    createFrame("possessive_frame", {
        {"my", "my"},
        {"mine", "mine"},
        {"our", "our"},
        {"yours", "yours"},
        {"his", "his"},
        {"hers", "hers"},
        {"its", "its"},
        {"theirs", "theirs"}
    });
    
    createFrame("family_frame", {
        {"parent", "parent"},
        {"father", "father"},
        {"mother", "mother"},
        {"child", "child"},
        {"sibling", "sibling"},
        {"brother", "brother"},
        {"sister", "sister"},
        {"ancestor", "ancestor"},
        {"descendant", "descendant"}
    });
    
    createFrame("identity_frame", {
        {"person", "khamit"},
        {"ai", "mary"},
        {"nationality", "kazakh"},
        {"country", "kazakhstan"},
        {"birthday", "march_17"},
        {"birth_year", "2026"}
    });
    
    createFrame("ai_type_frame", {
        {"llm", "llm"},
        {"mary_ai", "mary"},
        {"static", "static_knowledge"},
        {"dynamic", "dynamic_knowledge"},
        {"learning", "continuous_learning"},
        {"memory", "memory_storage"}
    });
    
    createFrame("territory_frame", {
        {"boundary", "boundary"},
        {"territory", "territory"},
        {"personal", "personal_space"},
        {"private", "private_property"},
        {"common", "common_property"},
        {"public", "public_concept"}
    });
    
    std::cout << "  Added ownership and identity layer with " 
              << "~60 new concepts" << std::endl;
}

void buildBasicGrammarLayer() {
    std::cout << "  Building basic grammar layer..." << std::endl;
    
    // Глаголы-связки
    uint32_t verb_be = addNode("be", "be", 
        {"am", "is", "are", "was", "were"}, 
        "grammar", 0.9f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"copula", "linking_verb", "grammar"});
    
    uint32_t verb_have = addNode("have", "have", 
        {"has", "had", "possess"}, 
        "grammar", 0.8f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"possession", "grammar"});
    
    uint32_t verb_do = addNode("do", "do", 
        {"does", "did", "perform"}, 
        "grammar", 0.7f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"auxiliary", "action"});
    
    // Модальные глаголы
    uint32_t modal_can = addNode("can", "can", 
        {"could", "able"}, 
        "modal", 0.8f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"ability", "possibility"});
    
    uint32_t modal_will = addNode("will", "will", 
        {"would", "shall"}, 
        "modal", 0.8f, 0.3f, 3, EmotionalTone::POSITIVE,
        {"future", "intention"});
    
    // Создаем узлы для абстрактных понятий, если их нет
    uint32_t identity_id = getNodeId("identity");
    if (identity_id == 0) {
        identity_id = addNode("identity", "identity", 
            {"who someone is"}, "abstract",
            0.8f, 0.3f, 4, EmotionalTone::NEUTRAL,
            {"philosophy", "self"});
    }
    
    uint32_t possession_id = getNodeId("possession");
    if (possession_id == 0) {
        possession_id = addNode("possession", "possession", 
            {"ownership"}, "abstract",
            0.8f, 0.3f, 3, EmotionalTone::NEUTRAL,
            {"ownership", "property"});
    }
    
    uint32_t ability_id = getNodeId("ability");
    if (ability_id == 0) {
        ability_id = addNode("ability", "ability", 
            {"capability", "skill"}, "abstract",
            0.8f, 0.3f, 3, EmotionalTone::POSITIVE,
            {"capability", "potential"});
    }
    
    // Связи с существующими узлами
    addEdge(verb_be, identity_id, SemanticEdge::Type::IS_A, 0.9f);
    addEdge(verb_have, possession_id, SemanticEdge::Type::EXPRESSES, 0.9f);
    addEdge(modal_can, ability_id, SemanticEdge::Type::EXPRESSES, 0.9f);
}

void buildPrepositionsLayer() {
    std::cout << "  Building prepositions layer..." << std::endl;
    
    // Предлоги места
    uint32_t prep_in = addNode("in", "in", 
        {"inside", "within"}, 
        "preposition", 0.7f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"location", "container"});
    
    uint32_t prep_on = addNode("on", "on", 
        {"upon", "atop"}, 
        "preposition", 0.7f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"surface", "location"});
    
    uint32_t prep_at = addNode("at", "at", 
        {"by", "near"}, 
        "preposition", 0.7f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"location", "position"});
    
    uint32_t prep_under = addNode("under", "under", 
        {"beneath", "below"}, 
        "preposition", 0.6f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"below", "location"});
    
    uint32_t prep_over = addNode("over", "over", 
        {"above", "across"}, 
        "preposition", 0.6f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"above", "location"});
    
    uint32_t prep_between = addNode("between", "between", 
        {"among", "in between"}, 
        "preposition", 0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"middle", "location"});
    
    // Предлоги времени
    uint32_t prep_before = addNode("before", "before", 
        {"prior to", "earlier"}, 
        "preposition", 0.7f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"temporal", "earlier"});
    
    uint32_t prep_after = addNode("after", "after", 
        {"following", "later"}, 
        "preposition", 0.7f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"temporal", "later"});
    
    uint32_t prep_during = addNode("during", "during", 
        {"while", "throughout"}, 
        "preposition", 0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"duration", "temporal"});
    
    uint32_t prep_since = addNode("since", "since", 
        {"from", "after"}, 
        "preposition", 0.6f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"starting point", "temporal"});
    
    // Связи
    // Используем существующие узлы из пространственных концептов
    uint32_t inside_id = getNodeId("inside");  // из buildSpatialConceptNetwork
    uint32_t surface_id = getNodeId("surface");
    uint32_t before_id = getNodeId("before");
    uint32_t after_id = getNodeId("after");
    
    if (inside_id != 0) {
        addEdge(prep_in, inside_id, SemanticEdge::Type::SIMILAR_TO, 0.9f);
    }
    if (surface_id != 0) {
        addEdge(prep_on, surface_id, SemanticEdge::Type::RELATES_TO, 0.8f);
    }
    if (before_id != 0) {
        addEdge(prep_before, before_id, SemanticEdge::Type::IS_A, 0.9f);
    }
    if (after_id != 0) {
        addEdge(prep_after, after_id, SemanticEdge::Type::IS_A, 0.9f);
    }
}

void buildNumeralsLayer() {
    std::cout << "  Building numerals layer..." << std::endl;
    
    // Числа 1-10
    uint32_t num_one = addNode("one", "one", {"1"}, "numeral", 0.9f, 0.1f, 1, EmotionalTone::NEUTRAL, {"number"});
    uint32_t num_two = addNode("two", "two", {"2"}, "numeral", 0.8f, 0.1f, 1, EmotionalTone::NEUTRAL, {"number"});
    uint32_t num_three = addNode("three", "three", {"3"}, "numeral", 0.7f, 0.1f, 1, EmotionalTone::NEUTRAL, {"number"});
    uint32_t num_four = addNode("four", "four", {"4"}, "numeral", 0.7f, 0.1f, 1, EmotionalTone::NEUTRAL, {"number"});
    uint32_t num_five = addNode("five", "five", {"5"}, "numeral", 0.7f, 0.1f, 1, EmotionalTone::NEUTRAL, {"number"});
    uint32_t num_six = addNode("six", "six", {"6"}, "numeral", 0.6f, 0.1f, 1, EmotionalTone::NEUTRAL, {"number"});
    uint32_t num_seven = addNode("seven", "seven", {"7"}, "numeral", 0.6f, 0.1f, 1, EmotionalTone::NEUTRAL, {"number"});
    uint32_t num_eight = addNode("eight", "eight", {"8"}, "numeral", 0.6f, 0.1f, 1, EmotionalTone::NEUTRAL, {"number"});
    uint32_t num_nine = addNode("nine", "nine", {"9"}, "numeral", 0.6f, 0.1f, 1, EmotionalTone::NEUTRAL, {"number"});
    uint32_t num_ten = addNode("ten", "ten", {"10"}, "numeral", 0.7f, 0.1f, 1, EmotionalTone::NEUTRAL, {"number"});
    
    // Порядковые
    uint32_t first = addNode("first", "first", {"1st"}, "ordinal", 0.7f, 0.2f, 2, EmotionalTone::NEUTRAL, {"order"});
    uint32_t second = addNode("second", "second", {"2nd"}, "ordinal", 0.7f, 0.2f, 2, EmotionalTone::NEUTRAL, {"order"});
    uint32_t third = addNode("third", "third", {"3rd"}, "ordinal", 0.7f, 0.2f, 2, EmotionalTone::NEUTRAL, {"order"});
    uint32_t last = addNode("last", "last", {"final"}, "ordinal", 0.8f, 0.2f, 2, EmotionalTone::NEUTRAL, {"order"});
    
    // Связи
    addEdge(num_one, first, SemanticEdge::Type::RELATES_TO, 0.8f);
    addEdge(num_two, second, SemanticEdge::Type::RELATES_TO, 0.8f);
    addEdge(num_three, third, SemanticEdge::Type::RELATES_TO, 0.8f);
}

void buildResponsePhrasesLayer() {
    std::cout << "  Building response phrases layer..." << std::endl;
    
    uint32_t response_yes = addNode("yes", "yes", 
        {"yeah", "yep", "sure", "ok", "okay"}, 
        "response", 0.9f, 0.2f, 2, EmotionalTone::VERY_POSITIVE,
        {"affirmative", "agreement"});
    
    uint32_t response_no = addNode("no", "no", 
        {"nope", "nah", "not"}, 
        "response", 0.8f, 0.2f, 2, EmotionalTone::NEGATIVE,
        {"negative", "disagreement"});
    
    uint32_t response_maybe = addNode("maybe", "maybe", 
        {"perhaps", "possibly"}, 
        "response", 0.7f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"uncertainty", "possibility"});
    
    uint32_t interjection_oh = addNode("oh", "oh", 
        {"ah", "aha"}, 
        "interjection", 0.6f, 0.1f, 1, EmotionalTone::POSITIVE,
        {"realization", "surprise"});
    
    uint32_t interjection_wow = addNode("wow", "wow", 
        {"wow", "amazing"}, 
        "interjection", 0.8f, 0.1f, 1, EmotionalTone::VERY_POSITIVE,
        {"amazement", "surprise"});
    
    uint32_t interjection_well = addNode("well", "well", 
        {"hmm", "um"}, 
        "interjection", 0.5f, 0.1f, 1, EmotionalTone::NEUTRAL,
        {"hesitation", "filler"});
    
    // Используем существующие узлы из buildBaseGraph()
    uint32_t positive_id = getNodeId("positive");
    uint32_t negative_id = getNodeId("negative");
    uint32_t uncertainty_id = getNodeId("uncertainty");
    
    if (positive_id != 0) {
        addEdge(response_yes, positive_id, SemanticEdge::Type::EXPRESSES, 0.9f);
    }
    if (negative_id != 0) {
        addEdge(response_no, negative_id, SemanticEdge::Type::EXPRESSES, 0.9f);
    }
    if (uncertainty_id != 0) {
        addEdge(response_maybe, uncertainty_id, SemanticEdge::Type::EXPRESSES, 0.9f);
    }
}

void buildDiscourseMarkersLayer() {
    std::cout << "  Building discourse markers layer..." << std::endl;
    
    uint32_t and_conj = addNode("and", "and", 
        {"&", "plus"}, 
        "conjunction", 0.9f, 0.1f, 1, EmotionalTone::NEUTRAL,
        {"connection", "addition"});
    
    uint32_t but_conj = addNode("but", "but", 
        {"however", "yet"}, 
        "conjunction", 0.8f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"contrast", "exception"});
    
    uint32_t or_conj = addNode("or", "or", 
        {"either", "alternatively"}, 
        "conjunction", 0.7f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"alternative", "choice"});
    
    uint32_t so_conj = addNode("so", "so", 
        {"therefore", "thus"}, 
        "conjunction", 0.8f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"consequence", "result"});
    
    uint32_t because_conj = addNode("because", "because", 
        {"since", "as"}, 
        "conjunction", 0.9f, 0.2f, 2, EmotionalTone::NEUTRAL,
        {"reason", "cause"});
    
    uint32_t if_conj = addNode("if", "if", 
        {"whether"}, 
        "conjunction", 0.8f, 0.3f, 3, EmotionalTone::NEUTRAL,
        {"condition", "hypothesis"});
    
    // Используем существующие узлы
    uint32_t addition_id = getNodeId("addition");  // из buildMathematicalFoundationLayer
    uint32_t contrast_id = getNodeId("contrast");
    uint32_t causality_id = getNodeId("causality");
    
    if (addition_id == 0) {
        addition_id = addNode("addition_concept", "addition", {}, "abstract", 0.7f, 0.2f, 2);
    }
    if (contrast_id == 0) {
        contrast_id = addNode("contrast", "contrast", {"difference"}, "abstract", 0.7f, 0.2f, 2);
    }
    if (causality_id == 0) {
        causality_id = getNodeId("causality");
    }
    
    addEdge(and_conj, addition_id, SemanticEdge::Type::EXPRESSES, 0.9f);
    addEdge(but_conj, contrast_id, SemanticEdge::Type::EXPRESSES, 0.9f);
    addEdge(so_conj, causality_id, SemanticEdge::Type::EXPRESSES, 0.9f);
    addEdge(because_conj, causality_id, SemanticEdge::Type::EXPRESSES, 0.9f);
}

void buildBasicVocabularyLayer() {
    std::cout << "  Building basic vocabulary layer..." << std::endl;
    
    buildBasicGrammarLayer();
    buildPrepositionsLayer();
    buildNumeralsLayer();
    buildResponsePhrasesLayer();
    buildDiscourseMarkersLayer();
    
    // Добавляем алиасы к существующим узлам вопросительных слов
    auto what_node = getNodeId("what");  // это question_what из buildRelationships
    if (what_node != 0) {
        addAlias(what_node, "what's");
        addAlias(what_node, "what is");
    }
    
    auto who_node = getNodeId("who");
    if (who_node != 0) {
        addAlias(who_node, "who's");
        addAlias(who_node, "who is");
    }
    
    auto where_node = getNodeId("where");
    if (where_node != 0) {
        addAlias(where_node, "where's");
        addAlias(where_node, "where is");
    }
    
    auto when_node = getNodeId("when");
    if (when_node != 0) {
        addAlias(when_node, "when's");
        addAlias(when_node, "when is");
    }
    
    auto why_node = getNodeId("why");
    if (why_node != 0) {
        addAlias(why_node, "why's");
        addAlias(why_node, "why is");
    }
    
    auto how_node = getNodeId("how");
    if (how_node != 0) {
        addAlias(how_node, "how's");
        addAlias(how_node, "how is");
    }
}

    // ===== ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ =====

    
    SemanticEdge::Type stringToRoleType(const std::string& role) {
        static const std::map<std::string, SemanticEdge::Type> role_map = {
            {"agent", SemanticEdge::Type::AGENT},
            {"patient", SemanticEdge::Type::PATIENT},
            {"instrument", SemanticEdge::Type::INSTRUMENT},
            {"location", SemanticEdge::Type::LOCATION},
            {"source", SemanticEdge::Type::SOURCE},
            {"destination", SemanticEdge::Type::DESTINATION},
            {"time", SemanticEdge::Type::TIME},
            {"purpose", SemanticEdge::Type::PURPOSE},
            {"result", SemanticEdge::Type::RESULT},
            {"beneficiary", SemanticEdge::Type::BENEFICIARY}
        };
        
        auto it = role_map.find(role);
        return (it != role_map.end()) ? it->second : SemanticEdge::Type::SIMILAR_TO;
    }
    
    void generateSignature(SemanticNode& node) {
        std::seed_seq seed{
            node.id,
            static_cast<unsigned int>(node.name.length()),
            static_cast<unsigned int>(node.canonical_form.length()),
            static_cast<unsigned int>(node.abstraction_level * 100),
            static_cast<unsigned int>(static_cast<int>(node.emotional_tone) + 10),
            static_cast<unsigned int>(node.base_importance * 1000),
            static_cast<unsigned int>(node.complexity * 1000)
        };
        // ИСПОЛЬЗУЕМ ТОТ ЖЕ АЛГОРИТМ, ЧТО И В SemanticManager
        std::mt19937 rng(node.id * 1000);  // просто id * 1000 как seed
        std::uniform_real_distribution<float> dist(0.3f, 0.8f);
        
        for (int i = 0; i < 32; ++i) {
            node.signature[i] = dist(rng);
        }
    }
    
    void indexWord(const std::string& word, uint32_t id) {
        std::string lower = word;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        word_to_ids[lower].push_back(id);
    }
    
    size_t countEdges() const {
        size_t count = 0;
        for (const auto& [_, edges] : edges_from) {
            count += edges.size();
        }
        return count;
    }
    
    void saveString(std::ofstream& file, const std::string& str) {
        uint32_t len = str.length();
        file.write(reinterpret_cast<const char*>(&len), sizeof(len));
        file.write(str.c_str(), len);
    }
    
    std::string loadString(std::ifstream& file) {
        uint32_t len;
        file.read(reinterpret_cast<char*>(&len), sizeof(len));
        std::string str(len, '\0');
        file.read(&str[0], len);
        return str;
    }
};
    inline void SemanticGraphDatabase::markCoreConcepts() {
        int marked_count = 0;
        
        // Ядерные концепты (никогда не удаляются)
        std::vector<std::string> core_names = {
            "mary", "khamit", "creator", "name", "ai", "system",
            "i", "you", "hello", "goodbye", "thanks", "help",
            "what", "who", "where", "when", "why", "how",
            "yes", "no", "good", "bad", "think", "know", "learn"
        };
        
        for (const auto& name : core_names) {
            uint32_t id = getNodeId(name);
            if (id != 0) {
                markAsCore(id);
                marked_count++;
                std::cout << "  Core concept: " << name << " (ID: " << id << ")" << std::endl;
            }
        }
        
        // Также помечаем как ядерные концепты с высокой базовой важностью
        for (const auto& [id, node] : nodes) {
            if (node->base_importance > 0.8f && !node->is_core) {
                node->is_core = true;
                marked_count++;
                std::cout << "  Auto-core concept: " << node->name << " (importance: " 
                        << node->base_importance << ")" << std::endl;
            }
        }
    }