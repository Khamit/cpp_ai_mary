#include <iostream>
#include "LanguageModule.hpp"
#include "modules/EvolutionModule.hpp"
#include "../../core/Config.hpp"
#include "../../core/MemoryManager.hpp"
#include <numeric>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <data/LanguageKnowledgeBase.hpp>

LanguageModule::LanguageModule(NeuralFieldSystem& system, EvolutionModule& evolution)
    : system_(system), evolution_(evolution), rng_(std::random_device{}()) {
    
    alphabet_ = {
        'a','b','c','d','e','f','g','h','i','j','k','l','m',
        'n','o','p','q','r','s','t','u','v','w','x','y','z'
    };
    
    // Initialize common bigrams
    common_bigrams_ = {
        {'t','h', 0.0356f}, {'h','e', 0.0307f}, {'i','n', 0.0243f},
        {'e','r', 0.0205f}, {'a','n', 0.0199f}, {'r','e', 0.0185f}
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
    
    loadAll();
}

bool LanguageModule::initialize(const Config& config) {
    std::cout << "LanguageModule initialized" << std::endl;
    return true;
}

void LanguageModule::shutdown() {
    saveAll();
}
// TODO: Возможны чрезмерные вызовы! Обезопасить метод
void LanguageModule::update(float dt) {
    // Применяем накопленные мутации раз в несколько циклов
    static float mutation_timer = 0;
    mutation_timer += dt;
    if (mutation_timer > 1.0f) { // Раз в секунду
        applyPendingMutations();
        mutation_timer = 0;
    }
}

void LanguageModule::saveState(MemoryManager& memory) {
    for (const auto& [word, data] : learned_words_) {
        std::vector<float> wordData = {
            static_cast<float>(data.times_rated),
            data.correctness,
            data.frequency
        };
        
        std::map<std::string, std::string> metadata;
        metadata["word"] = word;
        
        memory.store("LanguageModule", "word", wordData, data.correctness, metadata);
    }
}
// Мутация
void LanguageModule::requestConnectionMutation(int from, int to, double delta, const std::string& reason) {
    // Запоминаем предложение мутации
    pending_mutations_.push_back({from, to, delta, reason});
    
    // Не вызываем мутацию напрямую, а предлагаем эволюционному модулю
    // Это будет обработано в следующем цикле обновления
}

void LanguageModule::applyPendingMutations() {
    for (const auto& mutation : pending_mutations_) {
        // Здесь EvolutionModule решает, применить ли мутацию
        if (evolution_.proposeMutation(system_)) {
            // Если разрешено, применяем через system_
            system_.strengthenInterConnection(mutation.from, mutation.to, mutation.delta);
        }
    }
    pending_mutations_.clear();
}

void LanguageModule::loadState(MemoryManager& memory) {
    // Implementation would depend on MemoryManager's retrieval API
    std::cout << "LanguageModule state loaded" << std::endl;
}

// Group operations
std::vector<double> LanguageModule::getGroupActivities(int start, int end) const {
    std::vector<double> activities;
    const auto& groups = system_.getGroups();
    for (int g = start; g <= end && g < groups.size(); ++g) {
        activities.push_back(groups[g].getAverageActivity());
    }
    return activities;
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

// Word generation
std::string LanguageModule::generateWordFromGroups() {
    // Use context for semantic suggestions
    if (!word_history_.empty() && std::uniform_real_distribution<>(0, 1)(rng_) < 0.3f) {
        std::string lastWord = word_history_.back();
        
        // Check semantic links
        for (const auto& link : semantic_links_) {
            if (link.word1 == lastWord && link.strength > 0.7f) {
                word_history_.push_back(link.word2);
                if (word_history_.size() > 20) word_history_.erase(word_history_.begin());
                return link.word2;
            }
        }
    }
    
    // Generate new word
    std::string word;
    active_groups_history_.clear();
    
    auto semanticActivity = getGroupActivities(GROUP_SEMANTIC_START, GROUP_SEMANTIC_END);
    float avgSemantic = semanticActivity.empty() ? 0.5f : 
        std::accumulate(semanticActivity.begin(), semanticActivity.end(), 0.0f) / semanticActivity.size();
    
    int wordLength = 5 + static_cast<int>(avgSemantic * 5);
    wordLength = std::clamp(wordLength, 3, 12);
    
    char prevChar = 0;
    for (int pos = 0; pos < wordLength; ++pos) {
        char nextChar = selectNextChar(prevChar);
        word += nextChar;
        prevChar = nextChar;
        
        auto currentActive = findActiveGroups(0.7);
        active_groups_history_.insert(active_groups_history_.end(), 
                                      currentActive.begin(), currentActive.end());
    }
    
    last_generated_word_ = word;
    word_history_.push_back(word);
    if (word_history_.size() > 20) word_history_.erase(word_history_.begin());
    
    autoEvaluateGeneratedWord(word);
    return word;
}

char LanguageModule::selectNextChar(char prevChar) {
    std::vector<float> letterScores(26, 0.0f);
    
    // Base English letter frequencies
    const float englishFreq[26] = {
        0.0817f, 0.0149f, 0.0278f, 0.0425f, 0.1270f, 0.0223f, 0.0202f, 0.0609f,
        0.0697f, 0.0015f, 0.0077f, 0.0403f, 0.0241f, 0.0675f, 0.0751f, 0.0193f,
        0.0009f, 0.0599f, 0.0633f, 0.0906f, 0.0276f, 0.0098f, 0.0236f, 0.0015f,
        0.0197f, 0.0007f
    };
    // TODO: лучше добавить fallback
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
    for (const auto& [word, data] : learned_words_) {
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
        score *= noiseDist(rng_);
    }
    
    // Select letter
    float total = std::accumulate(letterScores.begin(), letterScores.end(), 0.0f);
    float r = std::uniform_real_distribution<float>(0.0f, total)(rng_);
    float cumulative = 0.0f;
    
    for (int i = 0; i < 26; ++i) {
        cumulative += letterScores[i];
        if (r <= cumulative) {
            return alphabet_[i];
        }
    }
    
    return 'a';
}

// Learning
void LanguageModule::learnWordPattern(const std::string& word, float rating) {
    LearnedWord lw;
    lw.word = word;
    lw.frequency = 1.0f;
    lw.correctness = rating;
    lw.times_rated = 1;
    
    auto semanticActivity = getGroupActivities(GROUP_SEMANTIC_START, GROUP_SEMANTIC_END);
    lw.semantic_vector = std::vector<float>(semanticActivity.begin(), semanticActivity.end());
    
    auto contextActivity = getGroupActivities(GROUP_CONTEXT_START, GROUP_CONTEXT_END);
    lw.context_vector = std::vector<float>(contextActivity.begin(), contextActivity.end());
    
    learned_words_[word] = lw;
    reinforceActiveGroups(rating);
    updateBigramGroups(word, rating);
    updateSemanticGroups(word, rating);
}

void LanguageModule::reinforceActiveGroups(float rating) {
    std::map<int, int> groupCounts;
    for (int g : active_groups_history_) {
        groupCounts[g]++;
    }
    
    std::vector<int> uniqueGroups;
    for (const auto& [g, count] : groupCounts) {
        if (count > 1) uniqueGroups.push_back(g);
    }
    
    // ИСПРАВИТЬ: добавить цикл по uniqueGroups
    for (size_t i = 0; i < uniqueGroups.size(); ++i) {
        for (size_t j = i + 1; j < uniqueGroups.size(); ++j) {
            double delta = rating * 0.01;
            requestConnectionMutation(uniqueGroups[i], uniqueGroups[j], delta, "semantic_reinforcement");
        }
    }
    
    for (int g : uniqueGroups) {
        requestConnectionMutation(g, GROUP_OUTPUT, rating * 0.02, "output_reinforcement");
    }
}

void LanguageModule::updateBigramGroups(const std::string& word, float rating) {
    for (size_t i = 0; i < word.length() - 1; ++i) {
        char c1 = std::tolower(word[i]);
        char c2 = std::tolower(word[i+1]);
        
        auto it = std::find_if(common_bigrams_.begin(), common_bigrams_.end(),
            [c1, c2](const Bigram& b) { return b.first == c1 && b.second == c2; });
        
        if (it != common_bigrams_.end()) {
            it->occurrences++;
            it->probability = std::clamp(it->probability * 0.9f + (rating > 0.5f ? 0.1f : -0.05f), 
                                        0.001f, 0.1f);
        }
        
        int bigramGroup = GROUP_BIGRAMS_START + (c1 % 4);
        int targetGroup = GROUP_PHONETICS_START + (c2 % 4);
        requestConnectionMutation(bigramGroup, targetGroup, rating * 0.05, "bigram_learning");
    }
}

void LanguageModule::updateSemanticGroups(const std::string& word, float rating) {
    if (word_history_.size() >= 2) {
        std::string prevWord = word_history_[word_history_.size() - 2];
        
        auto it = std::find_if(semantic_links_.begin(), semantic_links_.end(),
            [&](const SemanticLink& link) {
                return (link.word1 == prevWord && link.word2 == word) ||
                       (link.word1 == word && link.word2 == prevWord);
            });
        
        if (it != semantic_links_.end()) {
            it->strength = std::clamp(it->strength + rating * 0.1f, 0.0f, 1.0f);
        } else {
            semantic_links_.push_back({prevWord, word, rating * 0.5f, "sequential"});
        }
        
        int group1 = GROUP_SEMANTIC_START + (prevWord.length() % 8);
        int group2 = GROUP_SEMANTIC_START + (word.length() % 8);
        requestConnectionMutation(group1, group2, rating * 0.1, "semantic_link");
    }
}

void LanguageModule::updateContextGroups() {
    if (current_context_.empty()) return;
    
    if (learned_words_.find(current_context_) != learned_words_.end()) {
        requestConnectionMutation(GROUP_CONTEXT_START, GROUP_OUTPUT, 0.05, "context_activation");
    }
}

std::vector<float> LanguageModule::getWordEmbedding(const std::string& word) const {
    std::vector<float> embedding(128, 0.0f); // размер как в EvolutionModule
    
    auto it = learned_words_.find(word);
    if (it != learned_words_.end()) {
        // Комбинируем семантический и контекстный векторы
        const auto& lw = it->second;
        for (size_t i = 0; i < lw.semantic_vector.size() && i < 64; ++i) {
            embedding[i] = lw.semantic_vector[i];
        }
        for (size_t i = 0; i < lw.context_vector.size() && i < 64; ++i) {
            embedding[i + 64] = lw.context_vector[i];
        }
    }
    
    return embedding;
}

// Evaluation
float LanguageModule::autoEvaluateWord(const std::string& word) const {
    float score = 0.0f;
    
    // Используем ТОЛЬКО статическую базу знаний, не learned_words_
    if (isEnglishWord(word)) score += 0.5f;
    
    // Проверка на английскую фонетику (статические правила)
    score += calculatePhoneticScore(word) * 0.3f;
    
    // Проверка на существующие биграммы из базы
    score += calculateBigramScore(word) * 0.2f;
    
    return std::clamp(score, 0.0f, 1.0f);
}

float LanguageModule::calculateSurprise(const std::string& word) const {
    // Насколько слово неожиданно для модели
    auto it = learned_words_.find(word);
    if (it == learned_words_.end()) {
        return 1.0f; // Новое слово - максимальное удивление
    }
    
    // Если слово известно, но редко используется
    float frequency = it->second.frequency;
    return 1.0f - std::min(1.0f, frequency / 10.0f);
}

float LanguageModule::getLearningRate() const {
    // Как быстро модель учится на обратной связи
    if (external_feedback_count_ < 10) return 0.0f;
    
    // Корреляция между feedback и улучшением
    float improvement = 0.0f;
    // ... вычислите, как меняется качество после feedback
    return improvement;
}

float LanguageModule::calculatePhoneticScore(const std::string& word) const {
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

float LanguageModule::calculateBigramScore(const std::string& word) const {
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

float LanguageModule::calculateSemanticCoherence(const std::string& word) const {
    if (word_history_.empty()) return 0.5f;
    
    std::string lastWord = word_history_.back();
    
    for (const auto& link : semantic_links_) {
        if ((link.word1 == lastWord && link.word2 == word) ||
            (link.word2 == lastWord && link.word1 == word)) {
            return link.strength;
        }
    }
    
    return 0.4f;
}

void LanguageModule::autoEvaluateGeneratedWord(const std::string& word) {
    float score = autoEvaluateWord(word);
    
    if (score > 0.7f) {
        giveFeedback(0.8f, true);
    } else if (score < 0.3f) {
        giveFeedback(0.2f, true);
    }
}

// Public API
std::string LanguageModule::process(const std::string& input) {
    auto words = split(input);
    
    if (!words.empty()) {
        current_context_ = words.back();
        updateContextGroups();
    }
    
    if (input == "generate" || input == "new word") {
        return "I've created: '" + generateWordFromGroups() + "' - what do you think?";
    }
    
    if (input == "stats") {
        return getStats();
    }
    
    if (input == "hi" || input == "hello") {
        return "Hello! I'm learning language. Type 'generate' to see a new word!";
    }
    
    return getRandomResponse() + " '" + generateWordFromGroups() + "'?";
}

void LanguageModule::giveFeedback(float rating, bool autoFeedback) {
    if (last_generated_word_.empty()) return;
    
    if (rating > 0.7f) {
        for (int i = 0; i < 5; i++) {
            reinforceActiveGroups(1.0f);
            learnWordPattern(last_generated_word_, rating);
        }
    } else if (rating < 0.3f) {
        reinforceActiveGroups(-0.5f);
        learnWordPattern(last_generated_word_, rating);
    }
    
    if (!autoFeedback) {
        saveAll();
    }
}

void LanguageModule::setContext(const std::string& context) {
    current_context_ = context;
}

double LanguageModule::getLanguageFitness() const {
    // 1. Внешняя обратная связь (50% веса)
    float externalScore = getExternalFeedbackAvg();
    
    // 2. Внутренняя согласованность (30% веса)
    float internalScore = 0.5f;
    if (!word_history_.empty()) {
        int knownWords = 0;
        for (const auto& word : word_history_) {
            if (isEnglishWord(word)) knownWords++;
        }
        internalScore = static_cast<float>(knownWords) / word_history_.size();
    }
    
    // 3. Разнообразие словаря (20% веса)
    std::set<std::string> uniqueWords(word_history_.begin(), word_history_.end());
    float diversityScore = std::min<float>(1.0f, static_cast<float>(uniqueWords.size()) / 20.0f);
    
    return 0.5f * externalScore + 0.3f * internalScore + 0.2f * diversityScore;
}

// Utilities
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

std::string LanguageModule::getRandomResponse() {
    static std::vector<std::string> responses = {
        "How about", "Try this:", "Maybe", "What do you think of"
    };
    std::uniform_int_distribution<> dist(0, responses.size() - 1);
    return responses[dist(rng_)];
}

NeuronStats LanguageModule::computeNeuronStats(double activeThreshold, double passiveThreshold) const {
    NeuronStats stats{};
    const auto& groups = system_.getGroups();

    for (const auto& group : groups) {
        const auto& phi = group.getPhi();
        for (double v : phi) {
            stats.avgActivity += v;
            if (v > activeThreshold) stats.active++;
            else if (v > passiveThreshold) stats.passive++;
            else stats.inactive++;
        }
    }

    if (!groups.empty()) stats.avgActivity /= NeuralFieldSystem::TOTAL_NEURONS;
    return stats;
}

std::string LanguageModule::getStats() {
    auto stats = computeNeuronStats();
    std::stringstream ss;
    ss << "📊 Language Statistics:\n";
    ss << "Learned words: " << learned_words_.size() << "\n";
    ss << "Semantic links: " << semantic_links_.size() << "\n";
    ss << "Collocations: " << collocations_.size() << "\n";
    ss << "Neuron activity:\n";
    ss << "  Active (>0.7): " << stats.active << "\n";
    ss << "  Passive (0.1-0.7): " << stats.passive << "\n";
    ss << "  Inactive (<=0.1): " << stats.inactive << "\n";
    ss << "  Avg activity: " << std::fixed << std::setprecision(3) << stats.avgActivity << "\n";
    return ss.str();
}
// Persistence
void LanguageModule::saveAll() {
    std::ofstream file("data/learned_words.json");
    if (!file.is_open()) return;

    file << "{\n";
    bool first = true;
    for (const auto& [word, data] : learned_words_) {
        if (!first) file << ",\n";
        first = false;
        file << "  \"" << word << "\": {\n";
        file << "    \"correctness\": " << data.correctness << ",\n";
        file << "    \"times_rated\": " << data.times_rated << "\n";
        file << "  }";
    }
    file << "\n}\n";

    std::cout << "LanguageModule saved" << std::endl;
}

void LanguageModule::loadAll() {
    // Load from files if they exist
    std::cout << "Loading language data..." << std::endl;
}