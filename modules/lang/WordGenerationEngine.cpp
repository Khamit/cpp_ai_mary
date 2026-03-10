#include "WordGenerationEngine.hpp"
#include "../../core/NeuralFieldSystem.hpp"
#include "../../data/LanguageKnowledgeBase.hpp"
#include <numeric>
#include <algorithm>
#include <cctype>
#include <cmath>

WordGenerationEngine::WordGenerationEngine() {
    alphabet_ = {
        'a','b','c','d','e','f','g','h','i','j','k','l','m',
        'n','o','p','q','r','s','t','u','v','w','x','y','z'
    };

    // Initialize common bigrams
    common_bigrams_ = {
        {'t','h', 0.0356f, 0}, {'h','e', 0.0307f, 0}, {'i','n', 0.0243f, 0},
        {'e','r', 0.0205f, 0}, {'a','n', 0.0199f, 0}, {'r','e', 0.0185f, 0}
    };

    // Initialize semantic links
    semantic_links_ = {
        {"hello", "world", 0.9f, "greeting"},
        {"good", "morning", 0.8f, "time"},
        {"neural", "network", 0.8f, "technical"}
    };

    // Initialize collocations
    collocations_ = {
        {"of the", 0.9f}, {"in the", 0.9f}, {"to be", 0.8f}
    };
}

std::vector<double> WordGenerationEngine::getGroupActivities(
    NeuralFieldSystem& system,
    int start, 
    int end
) const {
    std::vector<double> activities;
    const auto& groups = system.getGroups();
    for (int g = start; g <= end && g < groups.size(); ++g) {
        activities.push_back(groups[g].getAverageActivity());
    }
    return activities;
}

std::vector<int> WordGenerationEngine::findActiveGroups(
    NeuralFieldSystem& system,
    double threshold
) const {
    std::vector<int> active;
    const auto& groups = system.getGroups();
    for (int g = 0; g < groups.size(); ++g) {
        if (groups[g].getAverageActivity() > threshold) {
            active.push_back(g);
        }
    }
    return active;
}

char WordGenerationEngine::selectNextChar(
    char prevChar, 
    std::mt19937& rng,
    const std::map<std::string, LearnedWord>& learnedWords
) const {
    std::vector<float> letterScores(26, 0.0f);
    
    // Base English letter frequencies
    const float englishFreq[26] = {
        0.0817f, 0.0149f, 0.0278f, 0.0425f, 0.1270f, 0.0223f, 0.0202f, 0.0609f,
        0.0697f, 0.0015f, 0.0077f, 0.0403f, 0.0241f, 0.0675f, 0.0751f, 0.0193f,
        0.0009f, 0.0599f, 0.0633f, 0.0906f, 0.0276f, 0.0098f, 0.0236f, 0.0015f,
        0.0197f, 0.0007f
    };
    
    for (int i = 0; i < 26; ++i) {
        letterScores[i] = englishFreq[i] * 10.0f;
    }
    
    // Bigram influence
    if (prevChar >= 'a' && prevChar <= 'z') {
        for (const auto& bigram : common_bigrams_) {
            if (bigram.first == prevChar) {
                int idx = bigram.second - 'a';
                letterScores[idx] += bigram.probability * 30.0f;
            }
        }
    }
    
    // Learned words influence
    for (const auto& [word, data] : learnedWords) {
        if (data.correctness > 0.6f) {
            for (size_t i = 0; i < word.length() - 1; ++i) {
                if (word[i] == prevChar) {
                    int idx = word[i+1] - 'a';
                    letterScores[idx] += data.correctness * 100.0f;
                }
            }
        }
    }
    
    // Small noise
    std::uniform_real_distribution<float> noiseDist(0.99f, 1.05f);
    for (float& score : letterScores) {
        score *= noiseDist(rng);
    }
    
    // Select letter
    float total = std::accumulate(letterScores.begin(), letterScores.end(), 0.0f);
    float r = std::uniform_real_distribution<float>(0.0f, total)(rng);
    float cumulative = 0.0f;
    
    for (int i = 0; i < 26; ++i) {
        cumulative += letterScores[i];
        if (r <= cumulative) {
            return alphabet_[i];
        }
    }
    
    return 'a';
}

std::string WordGenerationEngine::generateWordFromGroups(
    NeuralFieldSystem& system,
    std::mt19937& rng,
    std::vector<int>& activeGroupsHistory,
    std::vector<std::string>& wordHistory,
    std::string& lastWord,
    const std::map<std::string, LearnedWord>& learnedWords
) {
    // Use context for semantic suggestions
    if (!wordHistory.empty() && std::uniform_real_distribution<>(0, 1)(rng) < 0.3f) {
        std::string lastContextWord = wordHistory.back();
        
        // Check semantic links
        for (const auto& link : semantic_links_) {
            if (link.word1 == lastContextWord && link.strength > 0.7f) {
                wordHistory.push_back(link.word2);
                if (wordHistory.size() > 20) wordHistory.erase(wordHistory.begin());
                lastWord = link.word2;
                return link.word2;
            }
        }
    }
    
    // Generate new word
    std::string word;
    activeGroupsHistory.clear();
    
    auto semanticActivity = getGroupActivities(system, GROUP_SEMANTIC_START, GROUP_SEMANTIC_END);
    float avgSemantic = semanticActivity.empty() ? 0.5f : 
        std::accumulate(semanticActivity.begin(), semanticActivity.end(), 0.0f) / semanticActivity.size();
    
    int wordLength = 5 + static_cast<int>(avgSemantic * 5);
    wordLength = std::clamp(wordLength, 3, 12);
    
    char prevChar = 0;
    for (int pos = 0; pos < wordLength; ++pos) {
        char nextChar = selectNextChar(prevChar, rng, learnedWords);
        word += nextChar;
        prevChar = nextChar;
        
        auto currentActive = findActiveGroups(system, 0.7);
        activeGroupsHistory.insert(activeGroupsHistory.end(), 
                                   currentActive.begin(), currentActive.end());
    }
    
    lastWord = word;
    wordHistory.push_back(word);
    if (wordHistory.size() > 20) wordHistory.erase(wordHistory.begin());
    
    return word;
}

void WordGenerationEngine::learnWordPattern(
    const std::string& word, 
    float rating,
    const std::vector<float>& semanticActivity,
    const std::vector<float>& contextActivity
) {
    // Эта функция теперь только обновляет статистику
    // Сами слова хранятся в LanguageModule, здесь только вспомогательные данные
    
    // Обновляем биграммы
    for (size_t i = 0; i < word.length() - 1; ++i) {
        char c1 = std::tolower(word[i]);
        char c2 = std::tolower(word[i+1]);
        
        auto it = std::find_if(common_bigrams_.begin(), common_bigrams_.end(),
            [c1, c2](const Bigram& b) { return b.first == c1 && b.second == c2; });
        
        if (it != common_bigrams_.end()) {
            it->occurrences++;
            it->probability = std::clamp(
                it->probability * 0.9f + (rating > 0.5f ? 0.1f : -0.05f), 
                0.001f, 0.1f);
        }
    }
    
    // Обновляем семантические связи
    // (здесь можно добавить логику обновления semantic_links_)
}

float WordGenerationEngine::calculatePhoneticScore(const std::string& word) const {
    int maxConsonants = 0;
    int currentConsonants = 0;
    std::string vowels = "aeiouy";
    
    for (char c : word) {
        if (vowels.find(std::tolower(c)) == std::string::npos) {
            maxConsonants = std::max(maxConsonants, ++currentConsonants);
        } else {
            currentConsonants = 0;
        }
    }
    
    return 1.0f - (maxConsonants / 5.0f);
}

float WordGenerationEngine::calculateBigramScore(const std::string& word) const {
    float score = 0.0f;
    int validBigrams = 0;
    
    for (size_t i = 0; i < word.length() - 1; ++i) {
        char c1 = std::tolower(word[i]);
        char c2 = std::tolower(word[i+1]);
        
        auto it = std::find_if(common_bigrams_.begin(), common_bigrams_.end(),
            [c1, c2](const Bigram& b) { return b.first == c1 && b.second == c2; });
        
        if (it != common_bigrams_.end()) {
            score += it->probability;
            validBigrams++;
        }
    }
    
    return validBigrams > 0 ? score / validBigrams : 0.3f;
}

float WordGenerationEngine::calculateSemanticCoherence(
    const std::string& word,
    const std::vector<std::string>& history
) const {
    if (history.empty()) return 0.5f;
    
    std::string lastWord = history.back();
    
    for (const auto& link : semantic_links_) {
        if ((link.word1 == lastWord && link.word2 == word) ||
            (link.word2 == lastWord && link.word1 == word)) {
            return link.strength;
        }
    }
    
    return 0.4f;
}

float WordGenerationEngine::autoEvaluateWord(
    const std::string& word,
    const std::map<std::string, LearnedWord>& learnedWords,
    const std::vector<std::string>& wordHistory
) const {
    float score = 0.0f;
    
    // Проверка по словарю (внешняя база знаний)
    if (isEnglishWord(word)) score += 0.5f;
    
    // Проверка по выученным словам
    auto it = learnedWords.find(word);
    if (it != learnedWords.end()) {
        score += it->second.correctness * 0.3f;
    }
    
    // Фонетическая оценка
    score += calculatePhoneticScore(word) * 0.2f;
    
    // Биграммная оценка
    score += calculateBigramScore(word) * 0.2f;
    
    // Семантическая связность
    score += calculateSemanticCoherence(word, wordHistory) * 0.1f;
    
    return std::clamp(score, 0.0f, 1.0f);
}