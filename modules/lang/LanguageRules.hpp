// modules/lang/LanguageRules.hpp
#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <random>
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

        // Проверяем разные форматы вопросов
        if (words.size() >= 3) {
            // Формат "what is X"
            if (words[0] == "what" && words[1] == "is") {
                std::string subject = words[2];
                std::cout << "Detected 'what is' question about: '" << subject << "'" << std::endl;
                return findBestAnswer(subject);
            }
            
            // Формат "what X" (what you do, what think)
            if (words[0] == "what" && words.size() >= 2) {
                std::string topic = words[1];
                std::cout << "Detected 'what' question about: '" << topic << "'" << std::endl;
                return handleWhatQuestion(topic, words);
            }
            
            // Формат "how are you" и подобные
            if (words[0] == "how" && words[1] == "are" && words[2] == "you") {
                return handleGreeting();
            }
        }
        
        // Проверяем одиночные слова (приветствия, сленг)
        if (words.size() == 1) {
            std::string single = words[0];
            
            // Приветствия
            if (single == "hello" || single == "hi" || single == "hey") {
                return "Hello! How can I help you today?";
            }
            
            // Сленг
            if (single == "lol") {
                return "Ha ha, glad you're enjoying the simulation!";
            }
            
            if (single == "think") {
                return "I'm thinking about neural fields and evolution...";
            }
        }

        std::cout << "No matching pattern found" << std::endl;
        return "I don't understand that. Try asking 'what is neural' or say 'hello'.";
    }
    
    static std::string handleWhatQuestion(const std::string& topic, const std::vector<std::string>& words) {
        // "what you do"
        if (topic == "you" && words.size() >= 3 && words[2] == "do") {
            return "I'm an AI assistant running on a neural field system. I help with the simulation and answer questions.";
        }
        
        // "what is your name" - обрабатывается отдельно
        if (words.size() >= 4 && words[1] == "is" && words[2] == "your" && words[3] == "name") {
            return "My name is Mary, your AI assistant.";
        }
        
        // По умолчанию ищем в знаниях
        return findBestAnswer(topic);
    }
    
    static std::string handleGreeting() {
        // Случайный выбор приветствия
        std::vector<std::string> responses = {
            "I'm doing well, thanks for asking! How about you?",
            "All systems operational and running smoothly!",
            "I'm functioning optimally. How can I assist you?",
            "Great! The neural field is evolving nicely."
        };
        
        static std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<> dist(0, responses.size() - 1);
        return responses[dist(rng)];
    }

    static std::string findBestAnswer(const std::string& subject) {
        // Проверяем точное совпадение
        for (const auto& rule : knowledge_) {
            if (LanguageOperators::isSimilar(subject, rule.subject)) {
                return subject + " is " + rule.definition;
            }
        }
        
        // Проверяем частичное совпадение
        std::string lowerSubject = LanguageOperators::toLower(subject);
        for (const auto& rule : knowledge_) {
            std::string lowerRule = LanguageOperators::toLower(rule.subject);
            if (lowerSubject.find(lowerRule) != std::string::npos || 
                lowerRule.find(lowerSubject) != std::string::npos) {
                return "Did you mean " + rule.subject + "? " + rule.subject + " is " + rule.definition;
            }
        }

        return "I don't have information about '" + subject + "' yet. You can teach me by liking/disliking responses.";
    }

    static void updateRule(const std::string& input,
                           const std::string& definition,
                           float rating) {
        auto words = LanguageOperators::split(input);

        if (words.size() < 3) return;

        // Извлекаем тему (пытаемся понять о чем речь)
        std::string subject;
        if (words[0] == "what" && words[1] == "is") {
            subject = words[2];
        } else if (words[0] == "what") {
            subject = words[1];
        } else {
            subject = words[0]; // первое слово как тема
        }

        if (rating > 0.5f)
            addOrStrengthenRule(subject, definition);
        else
            weakenRule(subject);
    }

private:
    // добавьте больше правил
    static inline std::vector<Rule> knowledge_ = {
        {"neural", "brain-like computing system that processes information through interconnected nodes", 1.0f},
        {"field", "continuous space of activation where neural activity propagates", 1.0f},
        {"system", "integrated functional unit consisting of interconnected modules", 1.0f},
        {"hello", "greeting word", 0.8f},
        {"world", "everything around us, the environment we operate in", 0.8f},
        {"ai", "artificial intelligence that can learn and adapt", 1.0f},
        {"simulation", "computational model that mimics real-world processes", 1.0f},
        {"evolution", "process of gradual improvement through mutation and selection", 1.0f},
        {"memory", "storage system for past experiences and learned patterns", 1.0f},
        {"you", "me! Your AI assistant running on this neural field system", 1.0f},
        {"name", "Mary - your personal AI assistant", 1.0f},
        {"code", "the C++ source files that make up this system", 0.9f},
        {"fitness", "measure of how well the system is performing its tasks", 0.9f},
        {"energy", "the total activity level in the neural field", 0.9f}
    };

    static void addOrStrengthenRule(const std::string& subject,
                                    const std::string& definition) {
        for (auto& rule : knowledge_) {
            if (rule.subject == subject) {
                rule.weight += 0.1f;
                rule.definition = definition; // обновляем определение
                std::cout << "Strengthened rule for '" << subject << "', weight: " << rule.weight << std::endl;
                return;
            }
        }

        knowledge_.push_back({subject, definition, 0.5f});
        std::cout << "Added new rule for '" << subject << "'" << std::endl;
    }

    static void weakenRule(const std::string& subject) {
        knowledge_.erase(
            std::remove_if(knowledge_.begin(), knowledge_.end(),
                [&](const Rule& r) {
                    bool remove = r.subject == subject && r.weight < 0.2f;
                    if (remove) std::cout << "Removed weak rule for '" << subject << "'" << std::endl;
                    return remove;
                }),
            knowledge_.end()
        );
    }
};