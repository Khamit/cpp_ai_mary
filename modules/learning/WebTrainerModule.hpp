// WebTrainerModule.hpp - улучшенная версия с множественными источниками и интеграцией с графом
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <random>
#include <set>
#include <fstream>
#include <sstream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "../../data/DynamicSemanticMemory.hpp"
#include "../../core/NeuralFieldSystem.hpp"

using json = nlohmann::json;

// Структура для проверенных знаний
struct ValidatedKnowledge {
    std::string query;
    std::string answer;
    std::string source;  // "duckduckgo", "local", "wikipedia", "bootstrap"
    float validation_score;
    int validation_count = 0;
    std::chrono::system_clock::time_point timestamp;
    
    // Ключевые слова для связи с графом
    std::vector<std::string> keywords;
    std::vector<std::string> related_concepts;
};

// Результат поиска из разных источников
struct SearchResult {
    std::string answer;
    std::string source;
    float confidence;
    float semantic_richness;
};

class WebTrainerModule {
private:
    NeuralFieldSystem& neural_system;
    DynamicSemanticMemory* dynamic_memory_;
    
    std::queue<std::string> training_queue;
    std::mutex queue_mutex;
    
    std::map<std::string, ValidatedKnowledge> validated_knowledge;
    
    int total_searches = 0;
    int validated_learnings = 0;
    int rejected_answers = 0;
    
    std::atomic<bool> is_running{false};
    std::thread worker_thread;
    std::mt19937 rng;
    
    // Локальная база знаний (файл)
    std::map<std::string, std::string> local_knowledge_base;
    std::string knowledge_file_path = "dump/knowledge_base.txt";
    
    // Конфигурация
    const float MIN_VALIDATION_SCORE = 0.65f;
    const int MAX_RETRIES_PER_QUERY = 2;
    const size_t MIN_ANSWER_LENGTH = 20;
    const size_t MAX_ANSWER_LENGTH = 800;
    
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
        size_t total_size = size * nmemb;
        output->append((char*)contents, total_size);
        return total_size;
    }
    
    // ===== МНОЖЕСТВЕННЫЕ ИСТОЧНИКИ ПОИСКА =====
    
    // 1. DuckDuckGo API (бесплатный, без ключа)
    std::string searchDuckDuckGo(const std::string& query) {
        CURL* curl = curl_easy_init();
        std::string response;
        
        if (curl) {
            char* encoded = curl_easy_escape(curl, query.c_str(), query.length());
            std::string url = "https://api.duckduckgo.com/?q=" + std::string(encoded) + 
                            "&format=json&no_html=1&skip_disambig=1";
            curl_free(encoded);
            
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "MaryAI/2.0");
            
            CURLcode res = curl_easy_perform(curl);
            if (res == CURLE_OK && !response.empty()) {
                curl_easy_cleanup(curl);
                return response;
            }
            curl_easy_cleanup(curl);
        }
        return "";
    }
    
    // 2. Wikipedia API (бесплатный, надёжный)
    std::string searchWikipedia(const std::string& query) {
        CURL* curl = curl_easy_init();
        std::string response;
        
        if (curl) {
            char* encoded = curl_easy_escape(curl, query.c_str(), query.length());
            std::string url = "https://en.wikipedia.org/api/rest_v1/page/summary/" + std::string(encoded);
            curl_free(encoded);
            
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "MaryAI/2.0");
            
            CURLcode res = curl_easy_perform(curl);
            if (res == CURLE_OK && !response.empty()) {
                curl_easy_cleanup(curl);
                return response;
            }
            curl_easy_cleanup(curl);
        }
        return "";
    }
    
    // 3. Локальный файл знаний (читает и пишет)
    void loadLocalKnowledgeBase() {
        std::ifstream file(knowledge_file_path);
        if (!file.is_open()) return;
        
        std::string line;
        while (std::getline(file, line)) {
            size_t sep = line.find('|');
            if (sep != std::string::npos) {
                std::string question = line.substr(0, sep);
                std::string answer = line.substr(sep + 1);
                local_knowledge_base[question] = answer;
            }
        }
        std::cout << "📂 Loaded " << local_knowledge_base.size() << " items from local knowledge base" << std::endl;
    }
    
    void saveToLocalKnowledgeBase(const std::string& question, const std::string& answer) {
        std::ofstream file(knowledge_file_path, std::ios::app);
        if (file.is_open()) {
            file << question << "|" << answer << "\n";
            local_knowledge_base[question] = answer;
        }
    }
    
    // 4. Поиск в локальной базе
    SearchResult searchLocal(const std::string& query) {
        SearchResult result;
        result.confidence = 0.0f;
        
        // Точное совпадение
        auto it = local_knowledge_base.find(query);
        if (it != local_knowledge_base.end()) {
            result.answer = it->second;
            result.source = "local_exact";
            result.confidence = 0.9f;
            result.semantic_richness = computeSemanticRichness(result.answer);
            return result;
        }
        
        // Частичное совпадение
        for (const auto& [q, a] : local_knowledge_base) {
            if (query.find(q) != std::string::npos || q.find(query) != std::string::npos) {
                result.answer = a;
                result.source = "local_partial";
                result.confidence = 0.7f;
                result.semantic_richness = computeSemanticRichness(result.answer);
                return result;
            }
        }
        
        return result;
    }
    
    // ===== ИНТЕГРАЦИЯ С ГРАФОМ =====
    
    // Извлечение ключевых слов для связи с графом
    std::vector<std::string> extractKeywordsForGraph(const std::string& text) {
        std::vector<std::string> keywords;
        std::set<std::string> unique;
        std::stringstream ss(text);
        std::string word;
        
        std::set<std::string> stop_words = {"the", "a", "an", "and", "or", "but", "in", "on", "at", "to", "for", "of", "with", "by"};
        
        while (ss >> word) {
            std::transform(word.begin(), word.end(), word.begin(), ::tolower);
            word.erase(std::remove_if(word.begin(), word.end(), ::ispunct), word.end());
            
            if (word.length() > 3 && stop_words.find(word) == stop_words.end()) {
                unique.insert(word);
            }
        }
        
        keywords.assign(unique.begin(), unique.end());
        if (keywords.size() > 10) keywords.resize(10);
        return keywords;
    }
    
    // Поиск связанных концептов в LTM
    std::vector<std::string> findRelatedConcepts(const std::vector<std::string>& keywords) {
        std::vector<std::string> related;
        
        if (!dynamic_memory_) return related;
        
        for (const auto& kw : keywords) {
            if (dynamic_memory_->hasWord(kw)) {
                related.push_back(kw);
            }
        }
        
        return related;
    }
    
    // ===== ПАРСИНГ ОТВЕТОВ ИЗ РАЗНЫХ ИСТОЧНИКОВ =====
    
    SearchResult parseDuckDuckGoResponse(const std::string& response, const std::string& query) {
        SearchResult result;
        result.confidence = 0.0f;
        
        try {
            auto data = json::parse(response);
            
            if (data.contains("AbstractText") && !data["AbstractText"].is_null()) {
                result.answer = data["AbstractText"].get<std::string>();
                result.confidence = 0.9f;
                result.source = "duckduckgo_abstract";
            }
            else if (data.contains("Definition") && !data["Definition"].is_null()) {
                result.answer = data["Definition"].get<std::string>();
                result.confidence = 0.85f;
                result.source = "duckduckgo_definition";
            }
            else if (data.contains("Heading") && !data["Heading"].is_null()) {
                result.answer = data["Heading"].get<std::string>();
                result.confidence = 0.6f;
                result.source = "duckduckgo_heading";
            }
            else if (data.contains("RelatedTopics") && data["RelatedTopics"].is_array()) {
                for (const auto& topic : data["RelatedTopics"]) {
                    if (topic.contains("Text")) {
                        result.answer = topic["Text"].get<std::string>();
                        result.confidence = 0.7f;
                        result.source = "duckduckgo_related";
                        break;
                    }
                }
            }
            
            if (!result.answer.empty()) {
                result.semantic_richness = computeSemanticRichness(result.answer);
                result.confidence *= validateAnswerQuality(result.answer, query);
            }
        } catch (const std::exception& e) {
            std::cerr << "DuckDuckGo parse error: " << e.what() << std::endl;
        }
        
        return result;
    }
    
    SearchResult parseWikipediaResponse(const std::string& response, const std::string& query) {
        SearchResult result;
        result.confidence = 0.0f;
        
        try {
            auto data = json::parse(response);
            
            if (data.contains("extract") && !data["extract"].is_null()) {
                result.answer = data["extract"].get<std::string>();
                result.confidence = 0.95f;
                result.source = "wikipedia";
                result.semantic_richness = computeSemanticRichness(result.answer);
                result.confidence *= validateAnswerQuality(result.answer, query);
            }
            else if (data.contains("description") && !data["description"].is_null()) {
                result.answer = data["description"].get<std::string>();
                result.confidence = 0.8f;
                result.source = "wikipedia_description";
                result.semantic_richness = computeSemanticRichness(result.answer);
                result.confidence *= validateAnswerQuality(result.answer, query);
            }
        } catch (const std::exception& e) {
            std::cerr << "Wikipedia parse error: " << e.what() << std::endl;
        }
        
        return result;
    }
    
    // ===== ВАЛИДАЦИЯ =====
    
    float validateAnswerQuality(const std::string& answer, const std::string& query) {
        float score = 1.0f;
        
        if (answer.length() < MIN_ANSWER_LENGTH) score *= 0.6f;
        if (answer.length() > MAX_ANSWER_LENGTH) score *= 0.9f;
        
        std::vector<std::string> bad_patterns = {
            "no results", "not found", "error", "undefined",
            "null", "empty", "please try again", "sorry", "try again"
        };
        
        std::string answer_lower = answer;
        std::transform(answer_lower.begin(), answer_lower.end(), answer_lower.begin(), ::tolower);
        
        for (const auto& pattern : bad_patterns) {
            if (answer_lower.find(pattern) != std::string::npos) {
                score *= 0.3f;
                break;
            }
        }
        
        return std::clamp(score, 0.0f, 1.0f);
    }
    
    float computeSemanticRichness(const std::string& text) {
        float richness = 0.0f;
        
        std::set<std::string> unique_words;
        std::stringstream ss(text);
        std::string word;
        while (ss >> word) {
            if (word.length() > 3) {
                unique_words.insert(word);
            }
        }
        richness += std::min(0.3f, unique_words.size() / 40.0f);
        
        int sentences = 0;
        for (char c : text) {
            if (c == '.' || c == '!' || c == '?') sentences++;
        }
        richness += std::min(0.2f, sentences / 3.0f);
        
        std::vector<std::string> connectors = {"because", "therefore", "however", "although", "thus"};
        for (const auto& conn : connectors) {
            if (text.find(conn) != std::string::npos) richness += 0.05f;
        }
        
        return std::min(1.0f, richness);
    }
    
    // ===== ОБУЧЕНИЕ С ИНТЕГРАЦИЕЙ В ГРАФ =====
    
    void learnWithGraphIntegration(const ValidatedKnowledge& knowledge) {
        if (!dynamic_memory_) return;
        
        static int step_counter = 0;
        step_counter++;
        
        std::cout << "\n🧠 INTEGRATING KNOWLEDGE INTO GRAPH (score=" << knowledge.validation_score << ")" << std::endl;
        std::cout << "   Q: " << knowledge.query << std::endl;
        std::cout << "   A: " << knowledge.answer.substr(0, 80) << "..." << std::endl;
        
        // 1. Извлекаем ключевые слова для графа
        auto keywords = extractKeywordsForGraph(knowledge.answer);
        std::cout << "   📝 Keywords: ";
        for (const auto& kw : keywords) std::cout << kw << " ";
        std::cout << std::endl;
        
        // 2. Ищем связанные концепты в существующей памяти
        auto related = findRelatedConcepts(keywords);
        if (!related.empty()) {
            std::cout << "   🔗 Related concepts in memory: ";
            for (const auto& rel : related) std::cout << rel << " ";
            std::cout << std::endl;
        }
        
        // 3. Обучаем через динамическую память (распределение по орбитам)
        dynamic_memory_->processText(knowledge.query, "web_trainer");
        dynamic_memory_->processText(knowledge.answer, "web_trainer");
        
        // 4. Усиливаем связи между связанными концептами
        auto& groups = neural_system.getGroupsNonConst();
        for (size_t i = 0; i < related.size() && i < keywords.size(); i++) {
            for (int g = 16; g <= 21; g++) {  // элитные группы
                groups[g].eliteInstruct(keywords[i], knowledge.validation_score * 0.5f);
            }
        }
        
        // 5. Сохраняем в локальную базу
        saveToLocalKnowledgeBase(knowledge.query, knowledge.answer);
        
        // 6. Даём время на эволюцию
        for (int evolve_step = 0; evolve_step < 20; evolve_step++) {
            neural_system.step(0.0f, step_counter + evolve_step);
        }
        
        // 7. Награда с учётом орбит
        float base_reward = 0.5f + knowledge.validation_score * 0.3f;
        
        for (int g = 0; g < groups.size(); g++) {
            float reward = base_reward;
            if (g >= 16 && g <= 21) reward *= 1.5f;      // элита
            else if (g == 3) reward *= 1.2f;            // смыслы
            else if (g == 2) reward *= 1.0f;            // паттерны
            else reward *= 0.5f;
            
            groups[g].learnSTDP(std::clamp(reward, 0.f, 1.f), step_counter);
        }
        
        std::cout << "   ✅ Knowledge integrated into graph!" << std::endl;
    }
    
    void saveToEmergentMemory(const ValidatedKnowledge& knowledge, int step) {
        auto& emergent = neural_system.emergentMutable();
        
        std::vector<float> pattern;
        for (char c : knowledge.query) pattern.push_back(static_cast<float>(c));
        pattern.push_back(0.0f);
        for (char c : knowledge.answer) pattern.push_back(static_cast<float>(c));
        pattern.push_back(0.0f);
        // Добавляем ключевые слова в паттерн для лучшего поиска
        for (const auto& kw : knowledge.keywords) {
            for (char c : kw) pattern.push_back(static_cast<float>(c));
            pattern.push_back(0.0f);
        }
        
        emergent.memory.writeSTM(
            pattern,
            knowledge.validation_score,
            0.5f,
            "web_knowledge_" + knowledge.query.substr(0, 20),
            step
        );
    }
    
public:
    WebTrainerModule(NeuralFieldSystem& ns, DynamicSemanticMemory* memory)
        : neural_system(ns), dynamic_memory_(memory), rng(std::random_device{}()) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        loadLocalKnowledgeBase();
        loadFromEmergentMemory();
    }
    
    ~WebTrainerModule() {
        stop();
        curl_global_cleanup();
    }

    void logTrainingStatus() {
        std::cout << "\n" << getStats() << std::endl;
        
        if (!validated_knowledge.empty()) {
            std::cout << "\nLast 5 validated items:" << std::endl;
            int count = 0;
            for (auto it = validated_knowledge.rbegin(); 
                it != validated_knowledge.rend() && count < 5; 
                ++it, ++count) {
                std::cout << "  [" << (int)(it->second.validation_score * 100) << "%] "
                        << it->first.substr(0, 40) << "..." << std::endl;
            }
        }
        
        if (!local_knowledge_base.empty()) {
            std::cout << "\nLocal knowledge base size: " << local_knowledge_base.size() << std::endl;
        }
    }
    
    void addQuestion(const std::string& question) {
        // Проверяем в validated_knowledge
        for (const auto& [q, k] : validated_knowledge) {
            if (areSimilarQuestions(q, question)) {
                std::cout << "⏭️ Already learned: " << question << std::endl;
                return;
            }
        }
        
        // Проверяем в локальной базе
        auto local = searchLocal(question);
        if (local.confidence > 0.7f) {
            std::cout << "📚 Found in local knowledge base: " << question << std::endl;
            ValidatedKnowledge knowledge;
            knowledge.query = question;
            knowledge.answer = local.answer;
            knowledge.source = local.source;
            knowledge.validation_score = local.confidence;
            knowledge.keywords = extractKeywordsForGraph(local.answer);
            validated_knowledge[question] = knowledge;
            learnWithGraphIntegration(knowledge);
            return;
        }
        
        // Добавляем в очередь
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            
            // Проверка дубликатов в очереди
            std::queue<std::string> temp = training_queue;
            while (!temp.empty()) {
                if (areSimilarQuestions(temp.front(), question)) {
                    std::cout << "⏭️ Already in queue: " << question << std::endl;
                    return;
                }
                temp.pop();
            }
            
            training_queue.push(question);
        }
        
        std::cout << "📚 Added to web queue: " << question << std::endl;
    }
    
    bool areSimilarQuestions(const std::string& q1, const std::string& q2) {
        if (q1 == q2) return true;
        
        std::set<std::string> words1, words2;
        std::stringstream ss1(q1), ss2(q2);
        std::string w;
        while (ss1 >> w) { std::transform(w.begin(), w.end(), w.begin(), ::tolower); words1.insert(w); }
        while (ss2 >> w) { std::transform(w.begin(), w.end(), w.begin(), ::tolower); words2.insert(w); }
        
        int common = 0;
        for (const auto& word : words1) {
            if (words2.count(word)) common++;
        }
        
        return common >= 2;
    }
    
    void start() {
        if (is_running) return;
        is_running = true;
        
        worker_thread = std::thread([this]() {
            std::cout << "🌐 WebTrainer started (multi-source)" << std::endl;
            while (is_running) {
                std::string question;
                {
                    std::lock_guard<std::mutex> lock(queue_mutex);
                    if (!training_queue.empty()) {
                        question = training_queue.front();
                        training_queue.pop();
                    }
                }
                
                if (!question.empty()) {
                    processAndValidateQuestion(question);
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        });
    }
    
    void processAndValidateQuestion(const std::string& question) {
        std::cout << "\n🔍 Processing: " << question << std::endl;
        
        // Пробуем все источники
        std::vector<SearchResult> results;
        
        // 1. Wikipedia (наиболее надёжный)
        auto wiki_response = searchWikipedia(question);
        if (!wiki_response.empty()) {
            auto wiki_result = parseWikipediaResponse(wiki_response, question);
            if (wiki_result.confidence > 0.7f) {
                results.push_back(wiki_result);
            }
        }
        
        // 2. DuckDuckGo
        auto ddg_response = searchDuckDuckGo(question);
        if (!ddg_response.empty()) {
            auto ddg_result = parseDuckDuckGoResponse(ddg_response, question);
            if (ddg_result.confidence > 0.6f) {
                results.push_back(ddg_result);
            }
        }
        
        // 3. Локальная база (уже проверена в addQuestion, но на всякий случай)
        auto local_result = searchLocal(question);
        if (local_result.confidence > 0.7f) {
            results.push_back(local_result);
        }
        
        if (results.empty()) {
            std::cout << "❌ No valid answers from any source for: " << question << std::endl;
            rejected_answers++;
            total_searches++;
            return;
        }
        
        // Выбираем лучший результат
        auto best = results[0];
        for (const auto& r : results) {
            if (r.confidence > best.confidence) best = r;
        }
        
        if (best.confidence >= MIN_VALIDATION_SCORE && !best.answer.empty()) {
            ValidatedKnowledge knowledge;
            knowledge.query = question;
            knowledge.answer = best.answer;
            knowledge.source = best.source;
            knowledge.validation_score = best.confidence;
            knowledge.validation_count = 1;
            knowledge.timestamp = std::chrono::system_clock::now();
            knowledge.keywords = extractKeywordsForGraph(best.answer);
            
            validated_knowledge[question] = knowledge;
            saveToEmergentMemory(knowledge, total_searches);
            learnWithGraphIntegration(knowledge);
            
            validated_learnings++;
            std::cout << "✅ VALIDATED from " << best.source << "! Score: " << knowledge.validation_score << std::endl;
        } else {
            rejected_answers++;
            std::cout << "❌ REJECTED (conf=" << best.confidence << ") from " << best.source << std::endl;
        }
        
        total_searches++;
    }
    
    void bootstrapWithBasicKnowledge() {
        std::vector<std::pair<std::string, std::string>> basic_qa = {
            {"what is your name", "My name is Mary, your AI assistant."},
            {"who created you", "I was created by developers using neural networks with orbital architecture."},
            {"what can you do", "I can learn from conversations and search the web for validated information."},
            {"how do you learn", "I learn through neural plasticity. Information flows from lower orbits to higher orbits."},
            {"what is consciousness", "Consciousness is a complex concept involving self-awareness and perception."},
            {"what is love", "Love is a complex emotion involving attachment, care, and positive feelings toward someone or something."},
            {"what is intelligence", "Intelligence is the ability to acquire knowledge, think abstractly, adapt to environment, and learn from experience."}
        };
        
        for (const auto& [q, a] : basic_qa) {
            ValidatedKnowledge knowledge;
            knowledge.query = q;
            knowledge.answer = a;
            knowledge.source = "bootstrap";
            knowledge.validation_score = 0.95f;
            knowledge.validation_count = 1;
            knowledge.keywords = extractKeywordsForGraph(a);
            validated_knowledge[q] = knowledge;
            saveToEmergentMemory(knowledge, 0);
            
            if (dynamic_memory_) {
                dynamic_memory_->processText(q, "system");
                dynamic_memory_->processText(a, "system");
            }
            
            std::cout << "📚 Bootstrapped: " << q << std::endl;
        }
        
        start();
    }
    
    void stop() {
        is_running = false;
        if (worker_thread.joinable()) {
            worker_thread.join();
        }
    }
    
    void loadFromEmergentMemory() {
        auto& emergent = neural_system.emergentMutable();
        const auto& ltm = emergent.memory.getLTM();
        
        for (const auto& record : ltm) {
            if (record.tag.find("web_knowledge_") == 0) {
                std::string query, answer;
                size_t pos = 0;
                while (pos < record.pattern.size() && record.pattern[pos] != 0.0f) {
                    query += static_cast<char>(record.pattern[pos++]);
                }
                pos++;
                while (pos < record.pattern.size() && record.pattern[pos] != 0.0f) {
                    answer += static_cast<char>(record.pattern[pos++]);
                }
                
                if (!query.empty() && !answer.empty()) {
                    ValidatedKnowledge knowledge;
                    knowledge.query = query;
                    knowledge.answer = answer;
                    knowledge.source = "emergent_ltm";
                    knowledge.validation_score = record.importance;
                    knowledge.validation_count = 1;
                    knowledge.keywords = extractKeywordsForGraph(answer);
                    validated_knowledge[query] = knowledge;
                }
            }
        }
        
        std::cout << "📂 Loaded " << validated_knowledge.size() << " knowledge items from EmergentMemory" << std::endl;
    }
    
    const ValidatedKnowledge* getValidatedKnowledge(const std::string& query) const {
        auto it = validated_knowledge.find(query);
        if (it != validated_knowledge.end()) return &it->second;
        return nullptr;
    }
    
    std::string getStats() const {
        std::stringstream ss;
        ss << "=== WEB TRAINER STATS ===\n";
        ss << "Total searches: " << total_searches << "\n";
        ss << "Validated learnings: " << validated_learnings << "\n";
        ss << "Rejected answers: " << rejected_answers << "\n";
        ss << "Validation rate: " << (total_searches > 0 ? 
            (validated_learnings * 100.0f / total_searches) : 0) << "%\n";
        ss << "Validated knowledge: " << validated_knowledge.size() << "\n";
        ss << "Local knowledge base: " << local_knowledge_base.size() << "\n";
        ss << "Queue size: " << training_queue.size() << "\n";
        return ss.str();
    }
};