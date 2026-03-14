// modules/lang/MeaningHierarchy.hpp
#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>

/**
 * @class MeaningNode
 * @brief Узел в иерархии смыслов
 * 
 * Каждый узел представляет один СМЫСЛ, а не слово
 * Например: "питомец", "движение", "приветствие"
 */
struct MeaningNode {
    std::string meaning_id;           // уникальный ID смысла
    std::vector<std::string> words;    // слова, относящиеся к этому смыслу
    std::vector<std::string> parent_meanings;  // родительские смыслы
    std::vector<std::string> child_meanings;   // дочерние смыслы
    float activation_threshold;         // порог активации
    int assigned_group;                 // какая группа нейронов отвечает
    
    // Статистика
    int times_activated = 0;
    float average_importance = 0.5f;
};

/**
 * @class MeaningHierarchy
 * @brief Древовидная структура смыслов
 * 
 * Не хранит слова, хранит связи между смыслами
 * Слова - это просто "ярлыки" для смыслов
 */
class MeaningHierarchy {
private:
    std::map<std::string, MeaningNode> nodes;
    std::map<std::string, std::set<std::string>> word_to_meanings;  // слово -> смыслы
    
public:
    MeaningHierarchy() {
        buildBaseHierarchy();
    }
    
    // Получить смыслы для слова
    std::vector<std::string> getMeaningsForWord(const std::string& word) {
        auto it = word_to_meanings.find(word);
        if (it != word_to_meanings.end()) {
            return {it->second.begin(), it->second.end()};
        }
        return inferMeanings(word);  // если слово новое, вывести смыслы
    }
    
    // Получить группу нейронов для смысла
    int getGroupForMeaning(const std::string& meaning_id) {
        auto it = nodes.find(meaning_id);
        if (it != nodes.end()) {
            return it->second.assigned_group;
        }
        return -1;
    }
    
    // Активировать смысл (для нейросети)
    void activateMeaning(const std::string& meaning_id, float strength) {
        // В реальности здесь будет отправка сигнала в группу нейронов
    }
    
private:
    void buildBaseHierarchy() {
        // Уровень 1: Базовые категории (группы 16-19)
        addMeaning("greeting", {"hello", "hi", "hey", "greetings"}, {});
        addMeaning("farewell", {"bye", "goodbye", "see ya"}, {});
        addMeaning("question", {"what", "who", "where", "when", "why", "how"}, {});
        addMeaning("courtesy", {"thanks", "thank", "please"}, {});
        
        // Уровень 2: Действия (группы 20-23)
        addMeaning("action_help", {"help", "assist", "support"}, {"question"});
        addMeaning("action_learn", {"learn", "study", "understand"}, {"question"});
        addMeaning("action_generate", {"generate", "create", "make"}, {"question"});
        
        // Уровень 3: Объекты (группы 24-27)
        addMeaning("object_ai", {"ai", "artificial", "intelligence"}, {});
        addMeaning("object_neural", {"neural", "network", "brain"}, {"object_ai"});
        addMeaning("object_computer", {"computer", "program", "software"}, {"object_ai"});
        
        // Уровень 4: Свойства (группы 28-30)
        addMeaning("property_good", {"good", "nice", "great", "awesome"}, {});
        addMeaning("property_bad", {"bad", "terrible", "awful"}, {});
        addMeaning("property_interesting", {"interesting", "cool", "fascinating"}, {"property_good"});
    }
    
    void addMeaning(const std::string& id, 
                    const std::vector<std::string>& words,
                    const std::vector<std::string>& parents) {
        MeaningNode node;
        node.meaning_id = id;
        node.words = words;
        node.parent_meanings = parents;
        
        // Назначаем группу (упрощённо)
        static int group_counter = 16;
        node.assigned_group = group_counter++;
        
        nodes[id] = node;
        
        for (const auto& word : words) {
            word_to_meanings[word].insert(id);
        }
    }
    
    std::vector<std::string> inferMeanings(const std::string& word) {
        // Если слово неизвестно, пытаемся вывести смысл по окончаниям/префиксам
        std::vector<std::string> inferred;
        
        if (word.length() > 3) {
            std::string suffix = word.substr(word.length() - 3);
            if (suffix == "ing") inferred.push_back("action");
            if (suffix == "er" || suffix == "or") inferred.push_back("person");
        }
        
        return inferred;
    }
};