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

// ИЗМЕНЕН КОНСТРУКТОР - теперь с MemoryManager
LanguageModule::LanguageModule(NeuralFieldSystem& system, EvolutionModule& evolution, MemoryManager& memory)
    : system_(system), evolution_(evolution), memory_(memory), rng_(std::random_device{}()) {
    
    wordEngine_ = std::make_unique<WordGenerationEngine>();
    
    // НОВОЕ: загружаем профиль пользователя из памяти
    user_profile_.loadFromMemory(memory_);
    
    loadAll();
    
    std::cout << "LanguageModule initialized" << std::endl;
    if (user_profile_.knowsUser()) {
        std::cout << "👤 Known user profile loaded" << std::endl;
    }
}

bool LanguageModule::initialize(const Config& config) {
    std::cout << "LanguageModule initialized from config" << std::endl;
    return true;
}

void LanguageModule::shutdown() {
    saveAll();
}

void LanguageModule::update(float dt) {
    static float mutation_timer = 0;
    mutation_timer += dt;
    if (mutation_timer > 1.0f) {
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

void LanguageModule::loadState(MemoryManager& memory) {
    // Загрузка состояния (можно реализовать позже)
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

// Word generation (теперь через WordGenerationEngine)
std::string LanguageModule::generateWordFromGroups() {
    return wordEngine_->generateWordFromGroups(
        system_,
        rng_,
        active_groups_history_,
        word_history_,
        last_generated_word_,
        learned_words_
    );
}

// Эволюция
void LanguageModule::requestConnectionMutation(int from, int to, double delta, const std::string& reason) {
    pending_mutations_.push_back({from, to, delta, reason});
}

void LanguageModule::applyPendingMutations() {
    for (const auto& mutation : pending_mutations_) {
        if (evolution_.proposeMutation(system_)) {
            system_.strengthenInterConnection(mutation.from, mutation.to, mutation.delta);
        }
    }
    pending_mutations_.clear();
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
    
    // Обновляем статистику в WordGenerationEngine
    wordEngine_->learnWordPattern(word, rating, lw.semantic_vector, lw.context_vector);
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

// Эти методы теперь пустые или вызывают WordGenerationEngine
void LanguageModule::updateBigramGroups(const std::string& word, float rating) {
    // Обновление биграмм теперь в WordGenerationEngine
}

void LanguageModule::updateSemanticGroups(const std::string& word, float rating) {
    if (word_history_.size() >= 2) {
        std::string prevWord = word_history_[word_history_.size() - 2];
        
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

// Evaluation
float LanguageModule::autoEvaluateWord(const std::string& word) const {
    return wordEngine_->autoEvaluateWord(word, learned_words_, word_history_);
}

void LanguageModule::autoEvaluateGeneratedWord(const std::string& word) {
    float score = autoEvaluateWord(word);
    
    if (score > 0.7f) {
        giveFeedback(0.8f, true);
    } else if (score < 0.3f) {
        giveFeedback(0.2f, true);
    }
}

// ========== ИСПРАВЛЕННЫЙ process С ИСПОЛЬЗОВАНИЕМ КОНТЕКСТА ==========
std::string LanguageModule::process(const std::string& input) {
    std::cout << "📝 Processing: \"" << input << "\"" << std::endl;
    
    // 1. Добавляем в контекст
    context_tracker_.addTurn("user", input);
    
    // 2. Извлекаем факты
    auto facts = fact_extractor_.extractFacts(input);
    if (!facts.empty()) {
        for (const auto& fact : facts) {
            fact_extractor_.storeFact(memory_, fact);
        }
        user_profile_.updateFromFacts(facts);
    }
    
    // 3. Генерируем ответ на основе контекста и фактов
    std::string response;
    
    // Проверяем специальные команды
    if (input == "generate" || input == "new word") {
        response = "I've created: '" + generateWordFromGroups() + "' - what do you think?";
    }
    else if (input == "stats") {
        response = getStats();
    }
    else {
        // Нормальный диалог - используем ResponseGenerator
        response = response_generator_.generateResponse(input, user_profile_, context_tracker_);
    }
    
    // 4. Добавляем ответ в контекст
    context_tracker_.addTurn("mary", response);
    
    return response;
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
    
    // НОВОЕ: также учитываем фидбек для внешней статистики
    if (!autoFeedback) {
        addExternalFeedback(rating);
        saveAll();
    }
}

void LanguageModule::setContext(const std::string& context) {
    current_context_ = context;
    updateContextGroups();
}

double LanguageModule::getLanguageFitness() const {
    // 1. Внешняя обратная связь (40%)
    float externalScore = getExternalFeedbackAvg();
    
    // 2. Знание пользователя (40%)
    float knowledgeScore = user_profile_.knowsUser() ? 0.8f : 0.3f;
    
    // 3. Разнообразие слов (20%)
    float diversityScore = std::min(1.0f, learned_words_.size() / 100.0f);
    
    return 0.4f * externalScore + 0.4f * knowledgeScore + 0.2f * diversityScore;
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
    auto facts = user_profile_.getAllFacts();
    
    std::stringstream ss;
    ss << "📊 Language Statistics:\n";
    ss << "Learned words: " << learned_words_.size() << "\n";
    ss << "Known facts: " << facts.size() << "\n";
    ss << "Word history: " << word_history_.size() << " entries\n";
    
    if (!facts.empty()) {
        ss << "\n👤 User Profile:\n";
        for (const auto& [key, value] : facts) {
            ss << "  " << key << ": " << value << "\n";
        }
    }
    
    ss << "\n💬 Recent context:\n";
    ss << context_tracker_.getConversationSummary();
    
    ss << "\nNeuron activity:\n";
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
    std::cout << "Loading language data..." << std::endl;
}