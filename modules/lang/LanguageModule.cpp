#include "LanguageModule.hpp"
#include "../../data/LanguageKnowledgeBase.hpp"
#include <numeric>
#include <iomanip>
#include <sstream>      // для std::stringstream
#include <algorithm>    // для std::transform
#include <cctype>       // для std::tolower
#include <iterator>     // для итераторов (если понадобятся)
#include <cmath>  // для std::fabs

LanguageModule::LanguageModule(NeuralFieldSystem& system)
    : system_(system), rng_(std::random_device{}()) {
    
    alphabet_ = {
        'a','b','c','d','e','f','g','h','i','j','k','l','m',
        'n','o','p','q','r','s','t','u','v','w','x','y','z'
    };
    
    // Инициализация частых биграмм для автоматической оценки
    common_bigrams_ = {
        {'t','h', 0.0356f, 0}, {'h','e', 0.0307f, 0}, {'i','n', 0.0243f, 0},
        {'e','r', 0.0205f, 0}, {'a','n', 0.0199f, 0}, {'r','e', 0.0185f, 0},
        {'e','d', 0.0175f, 0}, {'o','n', 0.0174f, 0}, {'e','s', 0.0167f, 0},
        {'s','t', 0.0162f, 0}, {'e','n', 0.0159f, 0}, {'a','t', 0.0158f, 0},
        {'t','o', 0.0157f, 0}, {'n','t', 0.0153f, 0}, {'h','a', 0.0147f, 0},
        {'n','d', 0.0144f, 0}, {'o','u', 0.0144f, 0}, {'e','a', 0.0138f, 0},
        {'n','g', 0.0137f, 0}, {'a','l', 0.0136f, 0}, {'i','t', 0.0135f, 0}
    };
    
    // Инициализация семантических связей (часто используемые пары)
    semantic_links_ = {
        {"hello", "world", 0.9f, "greeting"},
        {"good", "morning", 0.8f, "time"},
        {"good", "night", 0.8f, "time"},
        {"how", "are", 0.7f, "question"},
        {"what", "is", 0.7f, "question"},
        {"thank", "you", 0.9f, "politeness"},
        {"neural", "network", 0.8f, "technical"},
        {"artificial", "intelligence", 0.9f, "technical"},
        {"machine", "learning", 0.8f, "technical"},
        {"deep", "learning", 0.8f, "technical"}
    };
    
    // Коллокации (частые словосочетания)
    collocations_ = {
        {"of the", 0.9f}, {"in the", 0.9f}, {"to be", 0.8f},
        {"is a", 0.8f}, {"as a", 0.7f}, {"with the", 0.7f},
        {"for the", 0.7f}, {"on the", 0.7f}, {"at the", 0.7f},
        {"by the", 0.6f}, {"from the", 0.6f}, {"have been", 0.6f}
    };
    
    loadAll();
    
    std::cout << "LanguageModule initialized with " << learned_words_.size() 
              << " learned words and " << semantic_links_.size() << " semantic links" << std::endl;
}

std::vector<double> LanguageModule::getGroupActivities(int startGroup, int endGroup) const {
    std::vector<double> activities;
    const auto& groups = system_.getGroups();
    for (int g = startGroup; g < endGroup && g < groups.size(); ++g) {
        activities.push_back(groups[g].getAverageActivity());
    }
    return activities;
}

void LanguageModule::activateGroup(int groupIndex, double strength) {
    // Через межгрупповые связи или прямое воздействие
    if (groupIndex >= 0 && groupIndex < NeuralFieldSystem::NUM_GROUPS) {
        // Усиливаем связи от мета-группы к целевой группе
        system_.strengthenInterConnection(GROUP_META, groupIndex, strength * 0.1);
    }
}

void LanguageModule::strengthenInterGroupConnection(int fromGroup, int toGroup, double delta) {
    system_.strengthenInterConnection(fromGroup, toGroup, delta);
}

std::vector<int> LanguageModule::findActiveGroups(double threshold) const {
    std::vector<int> active;
    const auto& groups = system_.getGroups();
    for (int g = 0; g < groups.size(); ++g) {
        if (groups[g].getAverageActivity() > threshold) {
            active.push_back(g);
        }
    }
    return active;
}

// Генерация слова через группы
std::string LanguageModule::generateWordFromGroups() {
    // 30% шанс использовать слово из базы знаний, если есть контекст
    if (!word_history_.empty() && std::uniform_real_distribution<>(0, 1)(rng_) < 0.3f) {
        std::string lastWord = word_history_.back();
        
        // Получаем семантические связи из базы
        auto links = getSemanticLinks(lastWord);
        if (!links.empty()) {
            std::uniform_int_distribution<> dist(0, links.size() - 1);
            std::string suggested = links[dist(rng_)];
            
            // Проверяем, что слово подходит по длине
            if (suggested.length() >= 2 && suggested.length() <= 8) {
                last_generated_word_ = suggested;
                word_history_.push_back(suggested);
                if (word_history_.size() > 20) word_history_.erase(word_history_.begin());
                return suggested;
            }
        }
        
        // Или используем коллокации
        float bestStrength = 0.0f;
        std::string bestSuggestion;
        for (const auto& [phrase, strength] : collocations_) {
            size_t spacePos = phrase.find(' ');
            if (spacePos != std::string::npos) {
                std::string first = phrase.substr(0, spacePos);
                if (first == lastWord && strength > bestStrength) {
                    bestStrength = strength;
                    bestSuggestion = phrase.substr(spacePos + 1);
                }
            }
        }
        if (!bestSuggestion.empty() && std::uniform_real_distribution<>(0, 1)(rng_) < bestStrength) {
            last_generated_word_ = bestSuggestion;
            word_history_.push_back(bestSuggestion);
            if (word_history_.size() > 20) word_history_.erase(word_history_.begin());
            return bestSuggestion;
        }
    }
    
    // 20% шанс использовать хорошо выученное слово
    if (!learned_words_.empty() && std::uniform_real_distribution<>(0, 1)(rng_) < 0.2f) {
        std::vector<std::string> goodWords;
        for (const auto& [word, data] : learned_words_) {
            if (data.correctness > 0.8f) {
                goodWords.push_back(word);
            }
        }
        if (!goodWords.empty()) {
            std::uniform_int_distribution<> dist(0, goodWords.size() - 1);
            std::string selected = goodWords[dist(rng_)];
            last_generated_word_ = selected;
            word_history_.push_back(selected);
            if (word_history_.size() > 20) word_history_.erase(word_history_.begin());
            return selected;
        }
    }
    
    // Иначе генерируем новое слово
    std::string word;
    active_groups_history_.clear();
    
    // ... остальной код генерации ...
    
    // Определяем длину слова на основе активности семантических групп
    auto semanticActivity = getGroupActivities(GROUP_SEMANTIC_START, GROUP_SEMANTIC_END);
    float avgSemantic = std::accumulate(semanticActivity.begin(), semanticActivity.end(), 0.0f) / semanticActivity.size();
    
    int wordLength = 3 + static_cast<int>(avgSemantic * 5);
    wordLength = std::clamp(wordLength, 2, 8);
    
    char prevChar = 0;
    
    for (int pos = 0; pos < wordLength; ++pos) {
        // Какие группы активны на этой позиции?
        std::vector<int> activeGroups;
        
        // Фонетические группы (0-3) влияют на выбор букв
        auto phoneticActivity = getGroupActivities(GROUP_PHONETICS_START, GROUP_PHONETICS_END);
        
        // Биграммные группы (4-7) влияют на последовательности
        auto bigramActivity = getGroupActivities(GROUP_BIGRAMS_START, GROUP_BIGRAMS_END);
        
        // Контекстные группы (28-29) влияют на общий тон
        auto contextActivity = getGroupActivities(GROUP_CONTEXT_START, GROUP_CONTEXT_END);
        
        char nextChar = selectNextChar(prevChar, activeGroups);
        word += nextChar;
        prevChar = nextChar;
        
        // Запоминаем активные группы для последующего обучения
        auto currentActive = findActiveGroups(0.7);
        active_groups_history_.insert(active_groups_history_.end(), 
                                      currentActive.begin(), currentActive.end());
    }
    
    // Сохраняем сгенерированное слово
    last_generated_word_ = word;
    word_history_.push_back(word);
    if (word_history_.size() > 20) word_history_.erase(word_history_.begin());
    
    // Автоматически оцениваем слово
    autoEvaluateGeneratedWord(word);
    
    return word;
}

char LanguageModule::selectNextChar(char prevChar, const std::vector<int>& activeGroups) {
    std::vector<float> letterScores(26, 0.0f);
    
    // 1. Базовые вероятности из частотности букв в английском (НОРМАЛИЗОВАННЫЕ)
    const float englishFreq[26] = {
        0.0817f, 0.0149f, 0.0278f, 0.0425f, 0.1270f, 0.0223f, 0.0202f, 0.0609f,
        0.0697f, 0.0015f, 0.0077f, 0.0403f, 0.0241f, 0.0675f, 0.0751f, 0.0193f,
        0.0009f, 0.0599f, 0.0633f, 0.0906f, 0.0276f, 0.0098f, 0.0236f, 0.0015f,
        0.0197f, 0.0007f
    };
    
    for (int letter = 0; letter < 26; ++letter) {
        letterScores[letter] = englishFreq[letter] * 10.0f; // базовый уровень
    }
    
    // 2. Влияние биграмм из базы знаний!
    if (prevChar >= 'a' && prevChar <= 'z') {
        std::string prevStr(1, prevChar);
        
        // Проходим по всем возможным следующим буквам
        for (char next = 'a'; next <= 'z'; ++next) {
            std::string nextStr(1, next);
            float bigramProb = evaluateBigram(prevStr, nextStr); // ИЗ БАЗЫ ЗНАНИЙ!
            
            if (bigramProb > 0.1f) { // только значимые биграммы
                int letterIdx = next - 'a';
                letterScores[letterIdx] += bigramProb * 30.0f; // СИЛЬНОЕ влияние
            }
        }
        
        // Дополнительно: известные биграммы из common_bigrams_ (как резерв)
        for (const auto& bigram : common_bigrams_) {
            if (bigram.first == prevChar) {
                int letterIdx = bigram.second - 'a';
                letterScores[letterIdx] += bigram.probability * 20.0f;
            }
        }
    }
    
    // 3. Влияние семантических связей из базы знаний!
    // Если у нас есть контекст (предыдущее слово), используем семантические связи
    if (!word_history_.empty()) {
        std::string lastWord = word_history_.back();
        auto semanticLinks = getSemanticLinks(lastWord); // ИЗ БАЗЫ ЗНАНИЙ!
        
        for (const auto& linkedWord : semanticLinks) {
            if (!linkedWord.empty()) {
                // Первая буква связанного слова получает бонус
                char firstChar = linkedWord[0];
                if (firstChar >= 'a' && firstChar <= 'z') {
                    int letterIdx = firstChar - 'a';
                    letterScores[letterIdx] += 50.0f; // ОЧЕНЬ СИЛЬНОЕ влияние
                }
            }
        }
        
        // Также проверяем коллокации
        float collStrength = getCollocationStrength(lastWord, std::string(1, prevChar));
        if (collStrength > 0) {
            // Если есть сильная коллокация, влияем на следующую букву
            for (char next = 'a'; next <= 'z'; ++next) {
                float strength = getCollocationStrength(lastWord, std::string(1, next));
                if (strength > 0) {
                    int letterIdx = next - 'a';
                    letterScores[letterIdx] += strength * 40.0f;
                }
            }
        }
    }
    
    // 4. Влияние выученных слов (из опыта)
    for (const auto& [word, data] : learned_words_) {
        if (data.correctness > 0.6f) { // только хорошие слова
            for (size_t i = 0; i < word.length() - 1; ++i) {
                if (word[i] == prevChar) {
                    int nextIdx = word[i+1] - 'a';
                    // Чем лучше слово, тем сильнее влияние
                    float influence = data.correctness * 100.0f;
                    
                    // Дополнительный бонус, если слово частотное в базе
                    if (isCommonWord(word)) {
                        influence *= 1.5f;
                    }
                    
                    letterScores[nextIdx] += influence;
                }
            }
        }
    }
    
    // 5. Влияние фонетических групп (меньше, чем база знаний)
    auto phoneticActivity = getGroupActivities(GROUP_PHONETICS_START, GROUP_PHONETICS_END);
    for (size_t g = 0; g < phoneticActivity.size(); ++g) {
        float activity = static_cast<float>(phoneticActivity[g]);
        for (int letter = 0; letter < 26; ++letter) {
            float diff = static_cast<float>(letter) - static_cast<float>(g * 8);
            float preference = 1.0f - std::fabs(diff) / 26.0f;
            letterScores[letter] += activity * preference * 5.0f; // уменьшено
        }
    }
    
    // 6. Очень маленький шум для разнообразия
    std::uniform_real_distribution<float> noiseDist(0.99f, 1.01f); // всего 1% шума
    for (int letter = 0; letter < 26; ++letter) {
        letterScores[letter] *= noiseDist(rng_);
    }
    
    // Выбор буквы (без нормализации, просто по весам)
    float total = std::accumulate(letterScores.begin(), letterScores.end(), 0.0f);
    float r = std::uniform_real_distribution<float>(0.0f, total)(rng_);
    float cumulative = 0.0f;
    
    for (int letter = 0; letter < 26; ++letter) {
        cumulative += letterScores[letter];
        if (r <= cumulative) {
            return alphabet_[letter];
        }
    }
    
    return 'a';
}

// Обучение слову через группы
void LanguageModule::learnWordPattern(const std::string& word, float rating) {
    // 1. Запоминаем слово
    LearnedWord lw;
    lw.word = word;
    lw.frequency = 1.0f;
    lw.correctness = rating;
    lw.times_rated = 1;
    
    // Семантический вектор = активности семантических групп
    auto semanticActivity = getGroupActivities(GROUP_SEMANTIC_START, GROUP_SEMANTIC_END);
    lw.semantic_vector = std::vector<float>(semanticActivity.begin(), semanticActivity.end());
    
    // Контекстный вектор
    auto contextActivity = getGroupActivities(GROUP_CONTEXT_START, GROUP_CONTEXT_END);
    lw.context_vector = std::vector<float>(contextActivity.begin(), contextActivity.end());
    
    learned_words_[word] = lw;
    
    // 2. Укрепляем межгрупповые связи между активными группами
    reinforceActiveGroups(rating);
    
    // 3. Обновляем биграммные группы
    updateBigramGroups(word, rating);
    
    // 4. Обновляем семантические связи
    updateSemanticGroups(word, rating);
    
    // 5. Обновляем коллокации
    updateCollocations(word);
}

void LanguageModule::updateContextGroups() {
    // Обновляем контекстные группы (28-29) на основе текущего контекста
    if (current_context_.empty()) return;
    
    // Активируем контекстные группы в зависимости от контекста
    float contextStrength = 0.0f;
    
    // Проверяем, есть ли слово в базе знаний
    if (isEnglishWord(current_context_)) {
        contextStrength = getWordFrequency(current_context_);
        
        // Активируем группу 28 для позитивного/нейтрального контекста
        // и группу 29 для специфического контекста
        if (contextStrength > 0.01f) {
            system_.strengthenInterConnection(GROUP_CONTEXT_START, GROUP_OUTPUT, 0.05);
        }
        
        // Если слово из технической области
        if (getSemanticField(current_context_) == "technology") {
            system_.strengthenInterConnection(GROUP_CONTEXT_START + 1, GROUP_SEMANTIC_START, 0.1);
        }
    }
    
    // Добавляем в историю контекста
    context_history_.push_back(current_context_);
    if (context_history_.size() > 10) {
        context_history_.erase(context_history_.begin());
    }
}

std::string LanguageModule::suggestNextWord(const std::string& currentWord) {
    if (currentWord.empty()) return "";
    
    // Ищем в семантических связях
    auto links = getSemanticLinks(currentWord);
    if (!links.empty()) {
        // Возвращаем случайную связь
        std::uniform_int_distribution<> dist(0, links.size() - 1);
        return links[dist(rng_)];
    }
    
    // Проверяем коллокации
    float bestStrength = 0.0f;
    std::string bestSuggestion;
    
    for (const auto& [phrase, strength] : collocations_) {
        size_t spacePos = phrase.find(' ');
        if (spacePos != std::string::npos) {
            std::string first = phrase.substr(0, spacePos);
            if (first == currentWord && strength > bestStrength) {
                bestStrength = strength;
                bestSuggestion = phrase.substr(spacePos + 1);
            }
        }
    }
    
    return bestSuggestion;
}

void LanguageModule::reinforceActiveGroups(float rating) {
    // Находим группы, которые были активны при генерации
    std::map<int, int> groupCounts;
    for (int g : active_groups_history_) {
        groupCounts[g]++;
    }
    
    // Укрепляем связи между часто совместно активными группами
    std::vector<int> uniqueGroups;
    for (const auto& [g, count] : groupCounts) {
        if (count > 1) uniqueGroups.push_back(g);
    }
    
    for (size_t i = 0; i < uniqueGroups.size(); ++i) {
        for (size_t j = i+1; j < uniqueGroups.size(); ++j) {
            double delta = rating * 0.01;
            strengthenInterGroupConnection(uniqueGroups[i], uniqueGroups[j], delta);
            strengthenInterGroupConnection(uniqueGroups[j], uniqueGroups[i], delta);
        }
    }
    
    // Особое укрепление связей с выходной группой
    for (int g : uniqueGroups) {
        double delta = rating * 0.02;
        strengthenInterGroupConnection(g, GROUP_OUTPUT, delta);
    }
}

void LanguageModule::updateBigramGroups(const std::string& word, float rating) {
    for (size_t i = 0; i < word.length() - 1; ++i) {
        char c1 = std::tolower(word[i]);
        char c2 = std::tolower(word[i+1]);
        
        // Обновляем статистику биграмм
        auto it = std::find_if(common_bigrams_.begin(), common_bigrams_.end(),
            [c1, c2](const Bigram& b) { return b.first == c1 && b.second == c2; });
        
        if (it != common_bigrams_.end()) {
            it->occurrences++;
            it->probability = (it->probability * 0.9f + (rating > 0.5f ? 0.1f : -0.05f));
            it->probability = std::clamp(it->probability, 0.001f, 0.1f);
        }
        
        // Укрепляем связи в биграммных группах (4-7)
        int bigramGroup = GROUP_BIGRAMS_START + (c1 % 4);
        int targetGroup = GROUP_PHONETICS_START + (c2 % 4);
        double delta = rating * 0.05;
        strengthenInterGroupConnection(bigramGroup, targetGroup, delta);
    }
}

void LanguageModule::updateSemanticGroups(const std::string& word, float rating) {
    // Ищем семантические связи с предыдущими словами
    if (word_history_.size() >= 2) {
        std::string prevWord = word_history_[word_history_.size() - 2];
        
        // Создаём или укрепляем семантическую связь
        auto it = std::find_if(semantic_links_.begin(), semantic_links_.end(),
            [&](const SemanticLink& link) {
                return (link.word1 == prevWord && link.word2 == word) ||
                       (link.word1 == word && link.word2 == prevWord);
            });
        
        if (it != semantic_links_.end()) {
            it->strength += rating * 0.1f;
            it->strength = std::clamp(it->strength, 0.0f, 1.0f);
        } else {
            SemanticLink newLink;
            newLink.word1 = prevWord;
            newLink.word2 = word;
            newLink.strength = rating * 0.5f;
            newLink.relation = "sequential";
            semantic_links_.push_back(newLink);
        }
        
        // Укрепляем связи между семантическими группами
        int group1 = GROUP_SEMANTIC_START + (prevWord.length() % 8);
        int group2 = GROUP_SEMANTIC_START + (word.length() % 8);
        strengthenInterGroupConnection(group1, group2, rating * 0.1);
    }
}

// Автоматическая оценка слова
void LanguageModule::autoEvaluateGeneratedWord(const std::string& word) {
    float score = autoEvaluateWord(word);
    
    // Если оценка высокая, автоматически даём положительный фидбек
    if (score > 0.7f) {
        giveFeedback(0.8f, true);
        std::cout << "✓ Auto-positive feedback for '" << word << "' (score: " << score << ")" << std::endl;
    }
    // Если оценка очень низкая, автоматически даём отрицательный фидбек
    else if (score < 0.3f) {
        giveFeedback(0.2f, true);
        std::cout << "✗ Auto-negative feedback for '" << word << "' (score: " << score << ")" << std::endl;
    }
}



float LanguageModule::autoEvaluateWord(const std::string& word) {
    float score = 0.5f; // нейтральная основа

        // 1. Проверка по словарю частотных слов
    if (isEnglishWord(word)) {
        score += getWordFrequency(word) * 2.0f; // + до 0.12 для самых частотных
    }
    
    
    // 1. Проверка по словарю (если слово уже выучено)
    if (learned_words_.find(word) != learned_words_.end()) {
        score += 0.3f;
    }
    
    // 2. Фонетическая оценка (сочетаемость букв)
    score += calculatePhoneticScore(word) * 0.2f;
    
    // 3. Биграммная оценка с использованием базы
    float bigramScore = 0.0f;
    int bigramCount = 0;
    for (size_t i = 0; i < word.length() - 1; ++i) {
        std::string w1(1, word[i]);
        std::string w2(1, word[i+1]);
        bigramScore += evaluateBigram(w1, w2);
        bigramCount++;
    }
    if (bigramCount > 0) {
        score += (bigramScore / bigramCount) * 0.3f;
    }
    
    // 4. Семантическая когерентность с контекстом
    score += calculateSemanticCoherence(word) * 0.2f;

        // Бонус для частотных слов
    if (isCommonWord(word)) {
        score += 0.1f;
    }
    
    return std::clamp(score, 0.0f, 1.0f);
}

float LanguageModule::calculatePhoneticScore(const std::string& word) {
    // Простая оценка: избегаем длинных согласных последовательностей
    int maxConsonants = 0;
    int currentConsonants = 0;
    std::string vowels = "aeiouy";
    
    for (char c : word) {
        if (vowels.find(std::tolower(c)) == std::string::npos) {
            currentConsonants++;
            maxConsonants = std::max(maxConsonants, currentConsonants);
        } else {
            currentConsonants = 0;
        }
    }
    
    // Чем меньше максимальная последовательность согласных, тем лучше
    return 1.0f - (maxConsonants / 5.0f);
}

float LanguageModule::calculateBigramScore(const std::string& word) {
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

float LanguageModule::calculateSemanticCoherence(const std::string& word) {
    if (word_history_.empty()) return 0.5f;
    
    std::string lastWord = word_history_.back();
    
    // 1. Проверка коллокаций из базы
    float collStrength = getCollocationStrength(lastWord, word);
    if (collStrength > 0) {
        return collStrength;
    }
    
    // 2. Проверка семантических связей
    auto links = getSemanticLinks(lastWord);
    if (std::find(links.begin(), links.end(), word) != links.end()) {
        return 0.8f;
    }
    
    // 3. Проверка по частям речи (грамматическая сочетаемость)
    std::string pos1 = getPartOfSpeech(lastWord);
    std::string pos2 = getPartOfSpeech(word);
    
    // Грамматически правильные комбинации
    if ((pos1 == "determiner" && pos2 == "noun") ||
        (pos1 == "adjective" && pos2 == "noun") ||
        (pos1 == "pronoun" && pos2 == "verb") ||
        (pos1 == "verb" && pos2 == "preposition")) {
        return 0.6f;
    }
    
    return 0.4f;
}

void LanguageModule::updateCollocations(const std::string& word) {
    if (word_history_.size() >= 2) {
        std::string prevWord = word_history_[word_history_.size() - 2];
        std::string bigram = prevWord + " " + word;
        
        collocations_[bigram] = collocations_[bigram] * 0.9f + 0.1f;
        if (collocations_[bigram] > 1.0f) collocations_[bigram] = 1.0f;
    }
}

// Публичный метод обработки ввода
std::string LanguageModule::process(const std::string& input) {
    last_input_ = input;
    std::string response;
    
    auto words = split(input);
    
    // Обновляем контекст
    if (!words.empty()) {
        current_context_ = words.back();
        updateContextGroups();
    }
    
    if (input == "generate" || input == "new word") {
        std::string new_word = generateWordFromGroups();
        response = "I've created: '" + new_word + "' - what do you think?";
    }
    else if (input == "stats" || input == "words") {
        response = getStats();
    }
    else if (input.find("what is ") == 0) {
        std::string word = input.substr(8);
        response = getWordMeaning(word);
    }
    else if (input == "hi" || input == "hello" || input == "hey") {
        response = "Hello! I'm learning language through neural groups. Type 'generate' to see a new word!";
        
        // Активируем группы приветствия
        strengthenInterGroupConnection(GROUP_CONTEXT_START, GROUP_OUTPUT, 0.1);
    }
    else {
        // Пытаемся ответить, используя семантические связи
        std::string new_word = generateWordFromGroups();
        response = getRandomResponse() + " '" + new_word + "'?";
        
        // Учитываем предыдущий контекст
        if (!words.empty()) {
            std::string suggested = suggestNextWord(words.back());
            if (!suggested.empty()) {
                response = "Maybe '" + suggested + "'?";
            }
        }
    }
    
    encodeToNeuralField(response);
    return response;
}

void LanguageModule::giveFeedback(float rating, bool autoFeedback) {
    std::cout << (autoFeedback ? "[AUTO] " : "") << "Feedback received: " << rating << std::endl;
    
    if (last_generated_word_.empty()) return;
    
    // Обновляем мета-группу
    if (rating > 0.7f) {
        strengthenInterGroupConnection(GROUP_META, GROUP_OUTPUT, 0.05);
        
        // УСИЛЕНО: повторяем обучение 5 раз!
        for (int i = 0; i < 5; i++) {
            reinforceActiveGroups(1.0f);
            learnWordPattern(last_generated_word_, rating);
        }
        
        // Сохраняем слово с повышенной оценкой
        if (learned_words_.find(last_generated_word_) != learned_words_.end()) {
            learned_words_[last_generated_word_].correctness = 
                (learned_words_[last_generated_word_].correctness + rating) / 2;
        }
        
    } else if (rating < 0.3f) {
        strengthenInterGroupConnection(GROUP_META, GROUP_OUTPUT, -0.02);
        reinforceActiveGroups(-0.5f);
        learnWordPattern(last_generated_word_, rating);
    }
    
    if (!autoFeedback) {
        saveAll();
    }
}

// Вспомогательные методы
std::vector<std::string> LanguageModule::split(const std::string& text) {
    std::vector<std::string> tokens;
    std::stringstream ss(text);
    std::string token;
    while (std::getline(ss, token, ' ')) {
        if (!token.empty()) {
            tokens.push_back(toLower(token));
        }
    }
    return tokens;
}

std::string LanguageModule::toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), ::tolower);
    return text;
}

std::string LanguageModule::getStats() {
    std::stringstream ss;
    ss << " Language Statistics:\n";
    ss << "Learned words: " << learned_words_.size() << "\n";
    ss << "Semantic links: " << semantic_links_.size() << "\n";
    ss << "Collocations: " << collocations_.size() << "\n";
    
    // Активность групп
    ss << "Group activities:\n";
    for (int g = 0; g < NeuralFieldSystem::NUM_GROUPS; ++g) {
        if (g % 8 == 0) ss << "\n";
        ss << "G" << std::setw(2) << g << ": " 
           << std::fixed << std::setprecision(2) 
           << system_.getGroups()[g].getAverageActivity() << " ";
    }
    
    return ss.str();
}

void LanguageModule::saveAll() {
    saveLearnedWords();
    saveSemanticLinks();
    saveCollocations();
}

void LanguageModule::saveLearnedWords() {
    std::ofstream file("data/learned_words.json");
    if (file.is_open()) {
        file << "{\n";
        for (const auto& [word, data] : learned_words_) {
            file << "  \"" << word << "\": {\n";
            file << "    \"frequency\": " << data.frequency << ",\n";
            file << "    \"correctness\": " << data.correctness << ",\n";
            file << "    \"times_rated\": " << data.times_rated << "\n";
            file << "  },\n";
        }
        file << "}\n";
    }
}

void LanguageModule::saveSemanticLinks() {
    std::ofstream file("data/semantic_links.json");
    if (file.is_open()) {
        file << "[\n";
        for (const auto& link : semantic_links_) {
            file << "  {\n";
            file << "    \"word1\": \"" << link.word1 << "\",\n";
            file << "    \"word2\": \"" << link.word2 << "\",\n";
            file << "    \"strength\": " << link.strength << ",\n";
            file << "    \"relation\": \"" << link.relation << "\"\n";
            file << "  },\n";
        }
        file << "]\n";
    }
}

void LanguageModule::saveCollocations() {
    std::ofstream file("data/collocations.json");
    if (file.is_open()) {
        file << "{\n";
        for (const auto& [phrase, strength] : collocations_) {
            file << "  \"" << phrase << "\": " << strength << ",\n";
        }
        file << "}\n";
    }
}

void LanguageModule::loadAll() {
    // Заглушки для загрузки
    std::cout << "Loading language data..." << std::endl;
}

// Заглушки для совместимости
std::string LanguageModule::getWordMeaning(const std::string& word) {
    auto it = learned_words_.find(word);
    if (it != learned_words_.end()) {
        return "'" + word + "' (learned, correctness: " + 
               std::to_string(static_cast<int>(it->second.correctness * 100)) + "%)";
    }
    return "Word not learned yet.";
}

std::string LanguageModule::getRandomResponse() {
    static std::vector<std::string> responses = {
        "How about", "What do you think of", "Try this one:", "Maybe"
    };
    std::uniform_int_distribution<> dist(0, responses.size() - 1);
    return responses[dist(rng_)];
}

std::string LanguageModule::decodeLastWord() {
    return last_generated_word_;
}

void LanguageModule::encodeToNeuralField(const std::string& text) {
    // В новой архитектуре не требуется
}