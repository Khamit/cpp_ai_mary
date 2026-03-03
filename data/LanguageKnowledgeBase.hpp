#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <array>
#include <iostream>

/**
 * @file LanguageKnowledgeBase.hpp
 * @brief База знаний для языкового модуля с предварительно загруженными словами и связками
 * 
 * Содержит частотные слова английского языка, биграммы, семантические связи,
 * коллокации и грамматические правила для быстрой оценки и обучения.
 */

// Структура для слова с частотностью
struct WordEntry {
    std::string word;
    float frequency;           // частотность в языке (0-1)
    std::string pos;            // часть речи (noun, verb, adj, etc.)
    std::vector<std::string> common_followers; // частые следующие слова
    std::vector<std::string> synonyms;          // синонимы
    float complexity;           // сложность слова (для оценки)
};

// Структура для биграммы
struct BigramEntry {
    std::string bigram;
    float probability;
    std::string context;        // контекст использования
};

// Структура для семантического поля
struct SemanticField {
    std::string concept;        // концепт (например, "greeting", "technology")
    std::vector<std::string> words; // слова, относящиеся к концепту
    float centrality;           // важность концепта
};

class LanguageKnowledgeBase {
public:
    // Получить экземпляр (синглтон)
    static const LanguageKnowledgeBase& getInstance() {
        static LanguageKnowledgeBase instance;
        return instance;
    }

    // Проверка, является ли слово английским (частотный словарь)
    bool isEnglishWord(const std::string& word) const {
        auto it = wordFrequency_.find(word);
        return it != wordFrequency_.end();
    }

    // Получить частотность слова
    float getWordFrequency(const std::string& word) const {
        auto it = wordFrequency_.find(word);
        if (it != wordFrequency_.end()) {
            return it->second;
        }
        return 0.0f;
    }

    // Получить часть речи слова
    std::string getPartOfSpeech(const std::string& word) const {
        auto it = wordPos_.find(word);
        if (it != wordPos_.end()) {
            return it->second;
        }
        return "unknown";
    }

    // Оценить биграмму (пару слов)
    float evaluateBigram(const std::string& word1, const std::string& word2) const {
        std::string bigram = word1 + " " + word2;
        auto it = bigramProbabilities_.find(bigram);
        if (it != bigramProbabilities_.end()) {
            return it->second;
        }
        
        // Если точной биграммы нет, проверяем общую сочетаемость
        std::string reverseBigram = word2 + " " + word1;
        it = bigramProbabilities_.find(reverseBigram);
        if (it != bigramProbabilities_.end()) {
            return it->second * 0.5f; // обратная сочетаемость слабее
        }
        
        return 0.1f; // базовая вероятность
    }

    // Получить семантические связи слова
    std::vector<std::string> getSemanticLinks(const std::string& word) const {
        std::vector<std::string> links;
        auto range = semanticLinks_.equal_range(word);
        for (auto it = range.first; it != range.second; ++it) {
            links.push_back(it->second);
        }
        return links;
    }

    // Получить синонимы
    std::vector<std::string> getSynonyms(const std::string& word) const {
        auto it = synonyms_.find(word);
        if (it != synonyms_.end()) {
            return it->second;
        }
        return {};
    }

    // Проверить, является ли биграмма частотной коллокацией
    float getCollocationStrength(const std::string& word1, const std::string& word2) const {
        std::string bigram = word1 + " " + word2;
        auto it = collocations_.find(bigram);
        if (it != collocations_.end()) {
            return it->second;
        }
        return 0.0f;
    }

    // Получить семантическое поле для слова
    std::string getSemanticField(const std::string& word) const {
        auto it = wordToField_.find(word);
        if (it != wordToField_.end()) {
            return it->second;
        }
        return "general";
    }

    // Оценить грамматическую правильность последовательности
    float evaluateGrammar(const std::vector<std::string>& words) const {
        if (words.size() < 2) return 0.5f;
        
        float score = 1.0f;
        for (size_t i = 0; i < words.size() - 1; ++i) {
            std::string pattern = words[i] + " " + words[i+1];
            
            // Проверка по известным грамматическим паттернам
            auto it = grammarPatterns_.find(pattern);
            if (it != grammarPatterns_.end()) {
                score *= it->second;
            } else {
                // Штраф за неизвестную комбинацию
                score *= 0.8f;
            }
        }
        return score;
    }

    // Получить список частотных слов для автоматической оценки
    const std::vector<std::string>& getCommonWords() const {
        return commonWords_;
    }

    // Проверить, является ли слово частотным (топ-1000)
    bool isCommonWord(const std::string& word) const {
        return std::find(commonWords_.begin(), commonWords_.end(), word) != commonWords_.end();
    }

private:
    LanguageKnowledgeBase() {
        initializeWordFrequency();
        initializeBigrams();
        initializeSemanticLinks();
        initializeCollocations();
        initializeGrammarPatterns();
        initializeCommonWords();
        initializeSemanticFields();
        
        std::cout << "LanguageKnowledgeBase initialized with " 
                  << wordFrequency_.size() << " words, "
                  << bigramProbabilities_.size() << " bigrams, "
                  << semanticLinks_.size() << " semantic links" << std::endl;
    }

    void initializeWordFrequency() {
        // Топ-200 английских слов по частотности (можно расширить)
        const std::vector<std::pair<std::string, float>> words = {
            // Артикли и детерминативы
            {"the", 0.061f}, {"a", 0.042f}, {"an", 0.021f}, {"this", 0.015f},
            {"that", 0.028f}, {"these", 0.008f}, {"those", 0.007f},
            
            // Предлоги
            {"of", 0.035f}, {"in", 0.027f}, {"to", 0.026f}, {"for", 0.017f},
            {"with", 0.014f}, {"on", 0.013f}, {"at", 0.011f}, {"by", 0.010f},
            {"from", 0.009f}, {"as", 0.009f}, {"into", 0.004f}, {"through", 0.003f},
            
            // Местоимения
            {"i", 0.024f}, {"you", 0.022f}, {"he", 0.018f}, {"she", 0.012f},
            {"it", 0.020f}, {"we", 0.010f}, {"they", 0.014f}, {"me", 0.006f},
            {"him", 0.005f}, {"her", 0.008f}, {"us", 0.004f}, {"them", 0.007f},
            {"my", 0.011f}, {"your", 0.008f}, {"his", 0.010f}, {"her", 0.008f},
            {"its", 0.005f}, {"our", 0.004f}, {"their", 0.007f},
            
            // Глаголы (частотные)
            {"is", 0.023f}, {"are", 0.015f}, {"was", 0.014f}, {"were", 0.008f},
            {"be", 0.016f}, {"been", 0.006f}, {"being", 0.003f},
            {"have", 0.013f}, {"has", 0.007f}, {"had", 0.008f},
            {"do", 0.009f}, {"does", 0.004f}, {"did", 0.006f},
            {"say", 0.005f}, {"said", 0.007f}, {"says", 0.002f},
            {"get", 0.006f}, {"got", 0.005f}, {"getting", 0.002f},
            {"make", 0.005f}, {"made", 0.005f}, {"making", 0.002f},
            {"go", 0.006f}, {"went", 0.004f}, {"gone", 0.002f}, {"going", 0.004f},
            {"know", 0.006f}, {"knew", 0.002f}, {"known", 0.002f},
            {"think", 0.005f}, {"thought", 0.003f},
            {"see", 0.004f}, {"saw", 0.002f}, {"seen", 0.002f},
            {"come", 0.004f}, {"came", 0.003f}, {"come", 0.004f},
            {"want", 0.004f}, {"wanted", 0.002f},
            {"use", 0.003f}, {"used", 0.004f}, {"using", 0.002f},
            {"find", 0.003f}, {"found", 0.004f},
            {"give", 0.003f}, {"gave", 0.002f}, {"given", 0.002f},
            {"tell", 0.003f}, {"told", 0.003f},
            {"work", 0.004f}, {"worked", 0.002f}, {"working", 0.002f},
            {"call", 0.003f}, {"called", 0.003f},
            {"try", 0.003f}, {"tried", 0.002f}, {"trying", 0.002f},
            {"ask", 0.002f}, {"asked", 0.002f},
            {"need", 0.003f}, {"needed", 0.002f},
            {"feel", 0.002f}, {"felt", 0.002f},
            {"become", 0.002f}, {"became", 0.002f},
            
            // Существительные (частотные)
            {"time", 0.010f}, {"person", 0.005f}, {"year", 0.007f}, {"way", 0.005f},
            {"day", 0.006f}, {"thing", 0.005f}, {"man", 0.005f}, {"woman", 0.004f},
            {"life", 0.004f}, {"child", 0.003f}, {"world", 0.004f}, {"school", 0.003f},
            {"state", 0.003f}, {"family", 0.003f}, {"student", 0.002f}, {"group", 0.003f},
            {"country", 0.004f}, {"problem", 0.003f}, {"hand", 0.003f}, {"part", 0.003f},
            {"place", 0.003f}, {"case", 0.003f}, {"week", 0.003f}, {"company", 0.003f},
            {"system", 0.003f}, {"program", 0.002f}, {"question", 0.003f}, {"work", 0.004f},
            {"government", 0.003f}, {"number", 0.003f}, {"night", 0.003f}, {"point", 0.003f},
            {"home", 0.003f}, {"water", 0.003f}, {"room", 0.003f}, {"mother", 0.002f},
            {"father", 0.002f}, {"money", 0.003f}, {"story", 0.002f}, {"fact", 0.002f},
            {"month", 0.002f}, {"lot", 0.003f}, {"right", 0.003f}, {"study", 0.002f},
            {"book", 0.003f}, {"eye", 0.003f}, {"job", 0.003f}, {"word", 0.003f},
            {"business", 0.003f}, {"issue", 0.002f}, {"side", 0.002f}, {"kind", 0.003f},
            {"head", 0.003f}, {"house", 0.003f}, {"service", 0.002f}, {"friend", 0.003f},
            {"father", 0.002f}, {"power", 0.002f}, {"hour", 0.003f}, {"game", 0.002f},
            {"line", 0.002f}, {"end", 0.003f}, {"member", 0.002f}, {"law", 0.002f},
            {"car", 0.003f}, {"city", 0.003f}, {"community", 0.002f}, {"name", 0.003f},
            {"president", 0.002f}, {"team", 0.002f}, {"minute", 0.003f}, {"idea", 0.003f},
            {"kid", 0.002f}, {"body", 0.002f}, {"information", 0.002f}, {"back", 0.003f},
            {"parent", 0.002f}, {"face", 0.002f}, {"others", 0.002f}, {"level", 0.002f},
            {"office", 0.002f}, {"door", 0.002f}, {"health", 0.002f}, {"person", 0.003f},
            {"art", 0.002f}, {"war", 0.002f}, {"history", 0.002f}, {"party", 0.002f},
            {"result", 0.002f}, {"change", 0.003f}, {"morning", 0.003f}, {"reason", 0.002f},
            {"research", 0.002f}, {"girl", 0.002f}, {"guy", 0.002f}, {"moment", 0.002f},
            {"air", 0.002f}, {"teacher", 0.002f}, {"force", 0.002f}, {"education", 0.002f},
            
            // Прилагательные (частотные)
            {"good", 0.008f}, {"new", 0.009f}, {"first", 0.007f}, {"last", 0.005f},
            {"long", 0.005f}, {"great", 0.004f}, {"little", 0.005f}, {"own", 0.004f},
            {"other", 0.008f}, {"old", 0.005f}, {"right", 0.004f}, {"big", 0.004f},
            {"high", 0.004f}, {"different", 0.004f}, {"small", 0.003f}, {"large", 0.003f},
            {"next", 0.003f}, {"early", 0.003f}, {"young", 0.003f}, {"important", 0.003f},
            {"few", 0.003f}, {"public", 0.003f}, {"bad", 0.003f}, {"same", 0.003f},
            {"able", 0.003f}, {"political", 0.002f}, {"real", 0.003f}, {"best", 0.003f},
            {"better", 0.003f}, {"sure", 0.003f}, {"clear", 0.002f}, {"likely", 0.002f},
            {"social", 0.002f}, {"national", 0.002f}, {"local", 0.002f}, {"general", 0.002f},
            {"open", 0.002f}, {"possible", 0.002f}, {"close", 0.002f}, {"simple", 0.002f},
            {"strong", 0.002f}, {"hard", 0.002f}, {"full", 0.002f}, {"true", 0.002f},
            
            // Наречия
            {"not", 0.018f}, {"so", 0.010f}, {"very", 0.008f}, {"just", 0.007f},
            {"now", 0.007f}, {"also", 0.006f}, {"then", 0.006f}, {"well", 0.005f},
            {"only", 0.005f}, {"here", 0.004f}, {"really", 0.004f}, {"there", 0.008f},
            {"when", 0.007f}, {"where", 0.004f}, {"why", 0.003f}, {"how", 0.006f},
            {"always", 0.003f}, {"never", 0.003f}, {"sometimes", 0.002f},
            {"often", 0.002f}, {"probably", 0.002f}, {"maybe", 0.003f},
            
            // Союзы и связки
            {"and", 0.031f}, {"or", 0.010f}, {"but", 0.012f}, {"because", 0.005f},
            {"if", 0.008f}, {"though", 0.002f}, {"although", 0.002f},
            {"while", 0.003f}, {"since", 0.003f}, {"until", 0.002f},
            {"than", 0.004f}, {"as", 0.009f}, {"like", 0.005f},
            
            // Вопросительные слова
            {"what", 0.008f}, {"which", 0.005f}, {"who", 0.005f}, {"whom", 0.001f},
            {"whose", 0.001f}, {"where", 0.004f}, {"when", 0.007f}, {"why", 0.003f},
            {"how", 0.006f}
        };

        for (const auto& [word, freq] : words) {
            wordFrequency_[word] = freq;
            
            // Определяем часть речи приблизительно
            if (word == "the" || word == "a" || word == "an" || word == "this" || 
                word == "that" || word == "these" || word == "those") {
                wordPos_[word] = "determiner";
            } else if (word == "of" || word == "in" || word == "to" || word == "for" ||
                      word == "with" || word == "on" || word == "at" || word == "by" ||
                      word == "from" || word == "as" || word == "into" || word == "through") {
                wordPos_[word] = "preposition";
            } else if (word == "i" || word == "you" || word == "he" || word == "she" ||
                      word == "it" || word == "we" || word == "they" || word == "me" ||
                      word == "him" || word == "her" || word == "us" || word == "them" ||
                      word == "my" || word == "your" || word == "his" || word == "its" ||
                      word == "our" || word == "their") {
                wordPos_[word] = "pronoun";
            } else if (word == "is" || word == "are" || word == "was" || word == "were" ||
                      word == "be" || word == "been" || word == "being" || word == "have" ||
                      word == "has" || word == "had" || word == "do" || word == "does" ||
                      word == "did" || word.find("ing") != std::string::npos ||
                      word.find("ed") != std::string::npos) {
                wordPos_[word] = "verb";
            } else if (word == "good" || word == "new" || word == "first" || word == "last" ||
                      word == "long" || word == "great" || word == "little" || word == "own" ||
                      word == "other" || word == "old" || word == "right" || word == "big" ||
                      word == "high" || word == "different" || word == "small" || word == "large") {
                wordPos_[word] = "adjective";
            } else if (word == "not" || word == "so" || word == "very" || word == "just" ||
                      word == "now" || word == "also" || word == "then" || word == "well" ||
                      word == "only" || word == "here" || word == "really" || word == "there") {
                wordPos_[word] = "adverb";
            } else if (word == "and" || word == "or" || word == "but" || word == "because" ||
                      word == "if" || word == "though" || word == "although" || word == "while" ||
                      word == "since" || word == "until" || word == "than") {
                wordPos_[word] = "conjunction";
            } else if (word == "what" || word == "which" || word == "who" || word == "whom" ||
                      word == "whose" || word == "where" || word == "when" || word == "why" ||
                      word == "how") {
                wordPos_[word] = "question_word";
            } else {
                wordPos_[word] = "noun";
            }
        }
    }

    void initializeBigrams() {
        // Частотные биграммы (word pairs)
        const std::vector<std::tuple<std::string, std::string, float>> bigrams = {
            // Артикль + существительное
            {"the", "world", 0.8f}, {"the", "time", 0.7f}, {"the", "day", 0.7f},
            {"the", "way", 0.6f}, {"the", "people", 0.6f}, {"the", "thing", 0.5f},
            {"a", "lot", 0.7f}, {"a", "person", 0.5f}, {"a", "problem", 0.5f},
            {"an", "example", 0.6f}, {"an", "idea", 0.6f},
            
            // Предлог + артикль
            {"in", "the", 0.9f}, {"on", "the", 0.8f}, {"at", "the", 0.8f},
            {"for", "the", 0.7f}, {"with", "the", 0.7f}, {"from", "the", 0.7f},
            {"to", "the", 0.7f}, {"of", "the", 0.9f},
            
            // Местоимение + глагол
            {"i", "am", 0.9f}, {"i", "have", 0.8f}, {"i", "was", 0.7f},
            {"you", "are", 0.9f}, {"you", "have", 0.7f}, {"you", "can", 0.7f},
            {"he", "is", 0.9f}, {"he", "was", 0.7f}, {"he", "has", 0.6f},
            {"she", "is", 0.9f}, {"she", "was", 0.7f}, {"she", "has", 0.6f},
            {"it", "is", 0.9f}, {"it", "was", 0.7f}, {"it", "has", 0.6f},
            {"we", "are", 0.9f}, {"we", "have", 0.7f}, {"we", "were", 0.6f},
            {"they", "are", 0.9f}, {"they", "have", 0.7f}, {"they", "were", 0.6f},
            
            // Глагол + предлог
            {"go", "to", 0.7f}, {"come", "to", 0.6f}, {"get", "to", 0.5f},
            {"look", "at", 0.7f}, {"look", "for", 0.5f}, {"wait", "for", 0.6f},
            {"ask", "for", 0.5f}, {"ask", "about", 0.5f}, {"think", "about", 0.7f},
            {"think", "of", 0.6f}, {"talk", "about", 0.6f}, {"talk", "to", 0.6f},
            {"listen", "to", 0.7f}, {"belong", "to", 0.6f}, {"lead", "to", 0.5f},
            
            // Прилагательное + существительное
            {"good", "day", 0.6f}, {"good", "time", 0.6f}, {"good", "person", 0.5f},
            {"new", "world", 0.5f}, {"new", "way", 0.5f}, {"new", "system", 0.5f},
            {"first", "time", 0.7f}, {"first", "day", 0.5f}, {"last", "time", 0.6f},
            {"last", "day", 0.5f}, {"long", "time", 0.7f}, {"long", "way", 0.5f},
            {"great", "idea", 0.6f}, {"great", "job", 0.6f}, {"important", "thing", 0.5f},
            
            // Наречие + прилагательное
            {"very", "good", 0.8f}, {"very", "important", 0.7f}, {"very", "different", 0.6f},
            {"really", "good", 0.7f}, {"really", "important", 0.6f}, {"quite", "good", 0.5f},
            {"so", "good", 0.6f}, {"too", "much", 0.7f}, {"too", "many", 0.7f},
            
            // Вопросительные конструкции
            {"what", "is", 0.9f}, {"what", "are", 0.7f}, {"what", "was", 0.6f},
            {"where", "is", 0.8f}, {"where", "are", 0.7f}, {"when", "is", 0.7f},
            {"when", "was", 0.6f}, {"why", "is", 0.7f}, {"why", "are", 0.6f},
            {"how", "is", 0.7f}, {"how", "are", 0.8f}, {"how", "was", 0.6f},
            
            // Частотные коллокации
            {"of", "course", 0.9f}, {"in", "fact", 0.8f}, {"for", "example", 0.9f},
            {"such", "as", 0.8f}, {"as", "well", 0.8f}, {"as", "if", 0.5f},
            {"even", "if", 0.5f}, {"even", "though", 0.6f}, {"in", "order", 0.6f},
            {"in", "other", 0.5f}, {"other", "words", 0.6f}, {"first", "of", 0.5f},
            {"out", "of", 0.7f}, {"because", "of", 0.8f}, {"instead", "of", 0.7f},
            {"kind", "of", 0.7f}, {"sort", "of", 0.6f}, {"lot", "of", 0.8f},
            {"number", "of", 0.7f}, {"way", "of", 0.5f},
            
            // Глагольные конструкции
            {"have", "to", 0.8f}, {"has", "to", 0.7f}, {"had", "to", 0.7f},
            {"need", "to", 0.8f}, {"needs", "to", 0.7f}, {"wants", "to", 0.7f},
            {"want", "to", 0.8f}, {"going", "to", 0.9f}, {"go", "to", 0.6f},
            {"used", "to", 0.7f}, {"supposed", "to", 0.6f}, {"trying", "to", 0.6f},
            {"told", "to", 0.5f}, {"asked", "to", 0.5f},
            
            // Союзные связки
            {"and", "then", 0.7f}, {"and", "so", 0.6f}, {"but", "also", 0.5f},
            {"not", "only", 0.6f}, {"only", "if", 0.5f}, {"if", "you", 0.6f},
            {"if", "they", 0.5f}, {"when", "you", 0.6f}, {"when", "they", 0.5f},
            {"because", "it", 0.5f}, {"because", "they", 0.5f}
        };

        for (const auto& [w1, w2, prob] : bigrams) {
            bigramProbabilities_[w1 + " " + w2] = prob;
        }
    }

    void initializeSemanticLinks() {
        // Семантические связи между словами (синонимы, антонимы, тематические группы)
        const std::vector<std::tuple<std::string, std::string, std::string>> links = {
            // Приветствия
            {"hello", "hi", "greeting"}, {"hello", "hey", "greeting"},
            {"goodbye", "bye", "farewell"}, {"goodbye", "farewell", "farewell"},
            
            // Время
            {"morning", "afternoon", "time"}, {"morning", "evening", "time"},
            {"night", "day", "time"}, {"today", "tomorrow", "time"},
            {"yesterday", "today", "time"}, {"now", "then", "time"},
            
            // Вопросительные
            {"what", "which", "question"}, {"what", "who", "question"},
            {"where", "when", "question"}, {"why", "how", "question"},
            
            // Оценка
            {"good", "great", "positive"}, {"good", "excellent", "positive"},
            {"good", "fine", "positive"}, {"bad", "terrible", "negative"},
            {"bad", "awful", "negative"}, {"bad", "poor", "negative"},
            
            // Размер
            {"big", "large", "size"}, {"big", "huge", "size"},
            {"small", "little", "size"}, {"small", "tiny", "size"},
            
            // Количество
            {"many", "several", "quantity"}, {"many", "numerous", "quantity"},
            {"few", "several", "quantity"}, {"lot", "much", "quantity"},
            
            // Движение
            {"go", "come", "movement"}, {"go", "leave", "movement"},
            {"come", "arrive", "movement"}, {"enter", "exit", "movement"},
            
            // Коммуникация
            {"say", "tell", "communication"}, {"say", "speak", "communication"},
            {"talk", "speak", "communication"}, {"ask", "question", "communication"},
            
            // Мышление
            {"think", "believe", "cognition"}, {"think", "consider", "cognition"},
            {"know", "understand", "cognition"}, {"learn", "study", "cognition"},
            
            // Технические термины
            {"neural", "network", "technical"}, {"neural", "brain", "technical"},
            {"artificial", "intelligence", "technical"}, {"machine", "learning", "technical"},
            {"deep", "learning", "technical"}, {"data", "information", "technical"},
            
            // Эмоции
            {"happy", "glad", "emotion"}, {"happy", "joyful", "emotion"},
            {"sad", "unhappy", "emotion"}, {"angry", "mad", "emotion"},
            
            // Пространство
            {"here", "there", "location"}, {"above", "below", "position"},
            {"inside", "outside", "position"}, {"left", "right", "direction"},
            
            // Семейные отношения
            {"mother", "father", "family"}, {"mother", "parent", "family"},
            {"father", "parent", "family"}, {"brother", "sister", "family"},
            {"son", "daughter", "family"}, {"child", "kid", "family"},
            
            // Профессии
            {"teacher", "professor", "profession"}, {"doctor", "physician", "profession"},
            {"lawyer", "attorney", "profession"}, {"engineer", "architect", "profession"},
            
            // Повседневные действия
            {"eat", "drink", "daily"}, {"eat", "consume", "daily"},
            {"sleep", "rest", "daily"}, {"work", "study", "daily"},
            {"play", "game", "daily"}, {"read", "book", "daily"}
        };

        for (const auto& [w1, w2, relation] : links) {
            semanticLinks_.insert({w1, w2});
            semanticLinks_.insert({w2, w1}); // симметрично
            
            // Сохраняем отношение (можно расширить)
            if (relation == "greeting") {
                semanticFields_["greeting"].push_back(w1);
                semanticFields_["greeting"].push_back(w2);
            } else if (relation == "time") {
                semanticFields_["time"].push_back(w1);
                semanticFields_["time"].push_back(w2);
            } else if (relation == "technical") {
                semanticFields_["technology"].push_back(w1);
                semanticFields_["technology"].push_back(w2);
            }
        }
        
        // Синонимы
        synonyms_["good"] = {"great", "excellent", "fine", "nice"};
        synonyms_["bad"] = {"terrible", "awful", "poor", "horrible"};
        synonyms_["big"] = {"large", "huge", "enormous", "giant"};
        synonyms_["small"] = {"little", "tiny", "miniature"};
        synonyms_["happy"] = {"glad", "joyful", "pleased", "delighted"};
        synonyms_["sad"] = {"unhappy", "depressed", "gloomy"};
        synonyms_["smart"] = {"intelligent", "clever", "bright"};
        synonyms_["fast"] = {"quick", "rapid", "swift"};
        synonyms_["slow"] = {"gradual", "leisurely"};
        synonyms_["important"] = {"significant", "crucial", "vital"};
    }

    void initializeCollocations() {
        // Частотные словосочетания
        const std::vector<std::pair<std::string, float>> colls = {
            {"of the", 0.95f}, {"in the", 0.94f}, {"to the", 0.90f},
            {"on the", 0.88f}, {"for the", 0.85f}, {"with the", 0.84f},
            {"at the", 0.82f}, {"from the", 0.80f}, {"by the", 0.78f},
            {"as a", 0.85f}, {"as an", 0.75f}, {"as the", 0.70f},
            {"is a", 0.86f}, {"is the", 0.88f}, {"are the", 0.75f},
            {"was a", 0.70f}, {"were the", 0.65f},
            {"have a", 0.75f}, {"has a", 0.70f}, {"had a", 0.65f},
            {"with a", 0.80f}, {"without a", 0.60f},
            {"to be", 0.92f}, {"to have", 0.85f}, {"to do", 0.80f},
            {"to go", 0.78f}, {"to come", 0.75f}, {"to get", 0.80f},
            {"to make", 0.77f}, {"to take", 0.75f}, {"to see", 0.72f},
            {"to know", 0.70f}, {"to think", 0.68f},
            {"going to", 0.90f}, {"want to", 0.88f}, {"need to", 0.87f},
            {"have to", 0.86f}, {"has to", 0.80f}, {"used to", 0.78f},
            {"supposed to", 0.70f}, {"trying to", 0.68f},
            {"a lot", 0.85f}, {"a little", 0.80f}, {"a bit", 0.78f},
            {"a few", 0.82f}, {"a many", 0.70f}, {"a number", 0.75f},
            {"for example", 0.92f}, {"such as", 0.90f}, {"according to", 0.85f},
            {"in addition", 0.80f}, {"as well", 0.88f}, {"as if", 0.70f},
            {"even though", 0.75f}, {"even if", 0.73f},
            {"first time", 0.85f}, {"last time", 0.80f}, {"next time", 0.78f},
            {"every day", 0.82f}, {"every time", 0.80f},
            {"good morning", 0.95f}, {"good afternoon", 0.90f},
            {"good evening", 0.90f}, {"good night", 0.95f},
            {"thank you", 0.98f}, {"thanks for", 0.90f},
            {"how are", 0.92f}, {"how is", 0.85f}, {"how was", 0.80f},
            {"what is", 0.94f}, {"what are", 0.88f}, {"what was", 0.82f},
            {"where is", 0.90f}, {"where are", 0.85f}, {"when is", 0.80f},
            {"why is", 0.78f}, {"why are", 0.75f}
        };

        for (const auto& [phrase, strength] : colls) {
            collocations_[phrase] = strength;
        }
    }

    void initializeGrammarPatterns() {
        // Грамматические паттерны (последовательности частей речи)
        grammarPatterns_ = {
            // Определитель + существительное
            {"the cat", 0.9f}, {"a cat", 0.9f}, {"an apple", 0.9f},
            {"my book", 0.9f}, {"your house", 0.9f},
            
            // Прилагательное + существительное
            {"good day", 0.8f}, {"new world", 0.8f}, {"big house", 0.8f},
            
            // Наречие + прилагательное
            {"very good", 0.9f}, {"really nice", 0.8f}, {"quite interesting", 0.7f},
            
            // Глагол + существительное
            {"read book", 0.7f}, {"eat food", 0.7f}, {"drive car", 0.7f},
            
            // Глагол + предлог
            {"go to", 0.8f}, {"come from", 0.7f}, {"look at", 0.8f},
            {"listen to", 0.8f}, {"wait for", 0.7f},
            
            // Существительное + глагол
            {"cat sleeps", 0.6f}, {"dog barks", 0.6f}, {"birds fly", 0.6f},
            
            // Местоимение + глагол
            {"i am", 0.95f}, {"you are", 0.95f}, {"he is", 0.95f},
            {"she was", 0.8f}, {"they were", 0.8f}, {"we have", 0.8f},
            
            // Предлог + определитель + существительное
            {"in the house", 0.9f}, {"on the table", 0.9f}, {"at the door", 0.8f},
            
            // Вопросительные конструкции
            {"what is", 0.95f}, {"where are", 0.9f}, {"how are you", 0.95f}
        };
    }

    void initializeCommonWords() {
        // Топ-100 частотных слов для быстрой проверки
        commonWords_ = {
            "the", "be", "to", "of", "and", "a", "in", "that", "have", "i",
            "it", "for", "not", "on", "with", "he", "as", "you", "do", "at",
            "this", "but", "his", "by", "from", "they", "we", "say", "her", "she",
            "or", "an", "will", "my", "one", "all", "would", "there", "their",
            "what", "so", "up", "out", "if", "about", "who", "get", "which",
            "go", "me", "when", "make", "can", "like", "time", "no", "just",
            "him", "know", "take", "people", "into", "year", "your", "good",
            "some", "could", "them", "see", "other", "than", "then", "now",
            "look", "only", "come", "its", "over", "think", "also", "back",
            "after", "use", "two", "how", "our", "work", "first", "well",
            "way", "even", "new", "want", "because", "any", "these", "give",
            "day", "most", "us", "is", "was", "are"
        };
    }

    void initializeSemanticFields() {
        // Семантические поля (концепты)
        std::vector<std::pair<std::string, std::vector<std::string>>> fields = {
            {"greeting", {"hello", "hi", "hey", "goodbye", "bye", "welcome"}},
            {"technology", {"computer", "software", "hardware", "neural", "network", 
                           "artificial", "intelligence", "machine", "learning", "data"}},
            {"time", {"time", "day", "night", "morning", "afternoon", "evening", 
                     "hour", "minute", "second", "week", "month", "year"}},
            {"family", {"mother", "father", "brother", "sister", "son", "daughter",
                       "parent", "child", "wife", "husband"}},
            {"emotion", {"happy", "sad", "angry", "excited", "nervous", "calm",
                        "love", "hate", "like", "dislike"}},
            {"education", {"school", "university", "student", "teacher", "professor",
                          "learn", "study", "teach", "class", "lesson"}},
            {"food", {"eat", "drink", "food", "water", "bread", "meat", "fruit",
                     "vegetable", "breakfast", "lunch", "dinner"}},
            {"travel", {"go", "come", "travel", "trip", "journey", "flight",
                       "car", "bus", "train", "plane", "hotel"}},
            {"work", {"work", "job", "business", "company", "office", "employee",
                     "employer", "worker", "career", "profession"}}
        };

        for (const auto& [field, words] : fields) {
            for (const auto& word : words) {
                wordToField_[word] = field;
            }
        }
    }

private:
    // Основные контейнеры
    std::unordered_map<std::string, float> wordFrequency_;
    std::unordered_map<std::string, std::string> wordPos_;
    std::map<std::string, float> bigramProbabilities_;
    std::multimap<std::string, std::string> semanticLinks_;
    std::map<std::string, std::vector<std::string>> synonyms_;
    std::map<std::string, float> collocations_;
    std::map<std::string, float> grammarPatterns_;
    std::vector<std::string> commonWords_;
    std::map<std::string, std::string> wordToField_;
    std::map<std::string, std::vector<std::string>> semanticFields_;
};

// Удобные функции-обертки для использования в LanguageModule
inline bool isEnglishWord(const std::string& word) {
    return LanguageKnowledgeBase::getInstance().isEnglishWord(word);
}

inline float getWordFrequency(const std::string& word) {
    return LanguageKnowledgeBase::getInstance().getWordFrequency(word);
}

inline std::string getPartOfSpeech(const std::string& word) {
    return LanguageKnowledgeBase::getInstance().getPartOfSpeech(word);
}

inline float evaluateBigram(const std::string& word1, const std::string& word2) {
    return LanguageKnowledgeBase::getInstance().evaluateBigram(word1, word2);
}

inline std::vector<std::string> getSemanticLinks(const std::string& word) {
    return LanguageKnowledgeBase::getInstance().getSemanticLinks(word);
}

inline float getCollocationStrength(const std::string& word1, const std::string& word2) {
    return LanguageKnowledgeBase::getInstance().getCollocationStrength(word1, word2);
}

inline bool isCommonWord(const std::string& word) {
    return LanguageKnowledgeBase::getInstance().isCommonWord(word);
}

inline std::string getSemanticField(const std::string& word) {
    return LanguageKnowledgeBase::getInstance().getSemanticField(word);
}

//#endif // LANGUAGE_KNOWLEDGE_BASE_HPP