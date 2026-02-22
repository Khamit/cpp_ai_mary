// LanguageRules.hpp - правила, сгенерированные через обучение/эволюцию
#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include "LanguageOperators.hpp"

class LanguageRules {
public:
    struct Rule {
        std::string subject;
        std::string definition;
        float weight;
    };

    static std::string handleQuestion(const std::string& question) {
        std::cout << "LanguageRules::handleQuestion: '" << question << "'" << std::endl;
        
        auto words = LanguageOperators::split(question);
        
        std::cout << "Split into " << words.size() << " words: ";
        for (const auto& w : words) std::cout << "'" << w << "' ";
        std::cout << std::endl;

        if (words.size() >= 3 && words[0] == "what" && words[1] == "is") {
            std::string subject = words[2];
            std::cout << "Detected question about: '" << subject << "'" << std::endl;
            return findBestAnswer(subject);
        }

        std::cout << "Not a 'what is' question" << std::endl;
        return "I don't understand the question format";
    }

    static std::string findBestAnswer(const std::string& subject) {
        for (const auto& rule : knowledge_) {
            if (LanguageOperators::isSimilar(subject, rule.subject)) {
                return subject + " is " + rule.definition;
            }
        }

        return subject + " is unknown to me";
    }

    static void updateRule(const std::string& input,
                           const std::string& definition,
                           float rating) {
        auto words = LanguageOperators::split(input);

        if (words.size() < 3) return;

        std::string subject = words[2];

        if (rating > 0.5f)
            addOrStrengthenRule(subject, definition);
        else
            weakenRule(subject);
    }

private:
    static inline std::vector<Rule> knowledge_ = {
        {"neural", "brain-like computing system", 1.0f},
        {"field", "continuous space of activation", 1.0f},
        {"system", "integrated functional unit", 1.0f},
        {"hello", "greeting word", 0.8f},
        {"world", "everything around us", 0.8f}
    };

    static void addOrStrengthenRule(const std::string& subject,
                                    const std::string& definition) {
        for (auto& rule : knowledge_) {
            if (rule.subject == subject) {
                rule.weight += 0.1f;
                return;
            }
        }

        knowledge_.push_back({subject, definition, 0.5f});
    }

    static void weakenRule(const std::string& subject) {
        knowledge_.erase(
            std::remove_if(knowledge_.begin(), knowledge_.end(),
                [&](const Rule& r) {
                    return r.subject == subject && r.weight < 0.2f;
                }),
            knowledge_.end()
        );
    }
};