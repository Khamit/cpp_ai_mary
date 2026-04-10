// WebTrainerModule.hpp - с учётом орбитальной иерархии смыслов
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
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "../../data/DynamicSemanticMemory.hpp"
#include "../../core/NeuralFieldSystem.hpp"

using json = nlohmann::json;

// Структура для проверенных знаний с распределением по орбитам
struct ValidatedKnowledge {
    std::string query;
    std::string answer;
    std::string source;
    float validation_score;
    int validation_count = 0;
    
    // Распределение смыслов по орбитам
    std::vector<uint32_t> letter_signals;    // орбита 0-1 (буквы)
    std::vector<uint32_t> word_activations;   // орбита 2 (слова)
    std::vector<uint32_t> pattern_hashes;     // орбита 3 (координация)
    std::vector<uint32_t> meaning_hashes;     // орбита 4 (элитные смыслы)
};

class WebTrainerModule {
private:
    NeuralFieldSystem& neural_system;
    DynamicSemanticMemory* dynamic_memory_;
    
    std::queue<std::string> training_queue;
    std::mutex queue_mutex;
    
    // Кэш ТОЛЬКО для проверенных знаний
    std::map<std::string, ValidatedKnowledge> validated_knowledge;
    
    int total_searches = 0;
    int validated_learnings = 0;
    int rejected_answers = 0;
    
    std::atomic<bool> is_running{false};
    std::thread worker_thread;
    std::mt19937 rng;
    
    // Конфигурация валидации
    const float MIN_VALIDATION_SCORE = 0.65f;
    const int MAX_RETRIES_PER_QUERY = 3;
    const size_t MIN_ANSWER_LENGTH = 30;
    const size_t MAX_ANSWER_LENGTH = 500;
    
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
        size_t total_size = size * nmemb;
        output->append((char*)contents, total_size);
        return total_size;
    }
    
    // МНОГОКРАТНЫЙ ПОИСК для валидации
    std::vector<std::string> searchWebMultiple(const std::string& query, int attempts = 3) {
        std::vector<std::string> responses;
        
        for (int i = 0; i < attempts; i++) {
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
                curl_easy_setopt(curl, CURLOPT_USERAGENT, "MaryAI/1.0");
                
                CURLcode res = curl_easy_perform(curl);
                if (res == CURLE_OK && !response.empty()) {
                    responses.push_back(response);
                }
                curl_easy_cleanup(curl);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        return responses;
    }
    
    // ИЗВЛЕЧЕНИЕ и ВАЛИДАЦИЯ ответа
    struct ExtractedAnswer {
        std::string answer;
        float confidence;
        std::string source;
        float semantic_richness;  // насколько богат смыслами
    };
    
    ExtractedAnswer extractAndValidateAnswer(const std::vector<std::string>& responses, const std::string& query) {
        ExtractedAnswer best;
        best.confidence = 0.0f;
        best.semantic_richness = 0.0f;
        
        for (const auto& json_response : responses) {
            try {
                auto data = json::parse(json_response);
                ExtractedAnswer current;
                
                // Пробуем разные поля в порядке достоверности
                if (data.contains("AbstractText") && !data["AbstractText"].is_null()) {
                    current.answer = data["AbstractText"].get<std::string>();
                    current.confidence = 0.9f;
                    current.source = "abstract";
                }
                else if (data.contains("Definition") && !data["Definition"].is_null()) {
                    current.answer = data["Definition"].get<std::string>();
                    current.confidence = 0.85f;
                    current.source = "definition";
                }
                else if (data.contains("Heading") && !data["Heading"].is_null()) {
                    current.answer = data["Heading"].get<std::string>();
                    current.confidence = 0.5f;
                    current.source = "heading";
                }
                else if (data.contains("RelatedTopics") && data["RelatedTopics"].is_array()) {
                    for (const auto& topic : data["RelatedTopics"]) {
                        if (topic.contains("Text")) {
                            current.answer = topic["Text"].get<std::string>();
                            current.confidence = 0.6f;
                            current.source = "related";
                            break;
                        }
                    }
                }
                
                if (!current.answer.empty()) {
                    // Валидация качества
                    current.confidence *= validateAnswerQuality(current.answer, query);
                    current.semantic_richness = computeSemanticRichness(current.answer);
                    
                    if (current.confidence > best.confidence) {
                        best = current;
                    }
                }
                
            } catch (const std::exception& e) {
                std::cerr << "JSON parse error: " << e.what() << std::endl;
            }
        }
        
        return best;
    }
    
    // ВЫЧИСЛЕНИЕ СМЫСЛОВОЙ НАСЫЩЕННОСТИ
    float computeSemanticRichness(const std::string& text) {
        float richness = 0.0f;
        
        // 1. Количество уникальных слов
        std::set<std::string> unique_words;
        std::stringstream ss(text);
        std::string word;
        while (ss >> word) {
            if (word.length() > 3) {
                unique_words.insert(word);
            }
        }
        richness += std::min(0.3f, unique_words.size() / 50.0f);
        
        // 2. Количество предложений
        int sentences = 0;
        for (char c : text) {
            if (c == '.' || c == '!' || c == '?') sentences++;
        }
        richness += std::min(0.2f, sentences / 5.0f);
        
        // 3. Наличие связующих слов (координация)
        std::vector<std::string> connectors = {"because", "therefore", "however", "although", "thus", "hence"};
        for (const auto& conn : connectors) {
            if (text.find(conn) != std::string::npos) {
                richness += 0.05f;
            }
        }
        
        // 4. Абстрактные понятия (для элитной орбиты)
        std::vector<std::string> abstract = {"concept", "idea", "meaning", "understanding", "knowledge", "wisdom"};
        for (const auto& abs : abstract) {
            if (text.find(abs) != std::string::npos) {
                richness += 0.03f;
            }
        }
        
        return std::min(1.0f, richness);
    }
    
    // ВАЛИДАЦИЯ качества ответа
    float validateAnswerQuality(const std::string& answer, const std::string& query) {
        float score = 1.0f;
        
        // 1. Длина проверка
        if (answer.length() < MIN_ANSWER_LENGTH) score *= 0.5f;
        if (answer.length() > MAX_ANSWER_LENGTH) score *= 0.8f;
        
        // 2. Проверка на бессмысленные ответы
        std::vector<std::string> bad_patterns = {
            "no results", "not found", "error", "undefined",
            "null", "empty", "please try again", "sorry"
        };
        
        for (const auto& pattern : bad_patterns) {
            if (answer.find(pattern) != std::string::npos) {
                score *= 0.3f;
                break;
            }
        }
        
        // 3. Проверка связности с вопросом (ключевые слова)
        std::vector<std::string> query_words;
        std::stringstream ss(query);
        std::string word;
        while (ss >> word) {
            if (word.length() > 3) {
                std::transform(word.begin(), word.end(), word.begin(), ::tolower);
                query_words.push_back(word);
            }
        }
        
        int matches = 0;
        std::string answer_lower = answer;
        std::transform(answer_lower.begin(), answer_lower.end(), answer_lower.begin(), ::tolower);
        
        for (const auto& qw : query_words) {
            if (answer_lower.find(qw) != std::string::npos) {
                matches++;
            }
        }
        
        float relevance = (float)matches / std::max(1.0f, (float)query_words.size());
        score *= (0.5f + relevance * 0.5f);
        
        // 4. Разнообразие слов (энтропия)
        std::set<std::string> unique_words;
        std::stringstream ans_ss(answer);
        while (ans_ss >> word) {
            unique_words.insert(word);
        }
        
        float diversity = std::min(1.0f, (float)unique_words.size() / 25.0f);
        score *= (0.7f + diversity * 0.3f);
        
        return std::clamp(score, 0.0f, 1.0f);
    }
    
    // ОБУЧЕНИЕ с РАСПРЕДЕЛЕНИЕМ ПО ОРБИТАМ!
    void learnValidatedKnowledge(const ValidatedKnowledge& knowledge) {
    if (!dynamic_memory_) return;
        
        static int step_counter = 0;
        step_counter++;
        
        std::cout << "\n🧠 VALIDATED LEARNING (score=" << knowledge.validation_score << ")" << std::endl;
        std::cout << "   Q: " << knowledge.query << std::endl;
        std::cout << "   A: " << knowledge.answer.substr(0, 80) << "..." << std::endl;
        std::cout << "   Semantic richness: " << knowledge.validation_score << std::endl;
        
        // ===== 1. РАСПРЕДЕЛЯЕМ ЗНАНИЕ ПО ОРБИТАМ =====
        
        // Орбита 0-1 (БУКВЫ/СИГНАЛЫ) - активируем через процессор текста
        auto letter_activations = dynamic_memory_->processText(knowledge.answer, "web_trainer");
        
        // Орбита 2 (СЛОВА) - извлекаем ключевые слова
        auto words = extractKeyWords(knowledge.answer);
        for (const auto& word : words) {
            dynamic_memory_->processText(word, "web_trainer");
        }
        
        // Орбита 3 (КООРДИНАЦИЯ) - создаём связи между словами
        auto patterns = createPatternConnections(words);
        for (const auto& pattern : patterns) {
            dynamic_memory_->processText(pattern, "web_trainer");
        }
        
        // Орбита 4 (ЭЛИТНЫЕ СМЫСЛЫ) - извлекаем абстрактные концепты
        auto meanings = extractMeanings(knowledge.answer);
        for (const auto& meaning : meanings) {
            dynamic_memory_->processText(meaning, "web_trainer");
        }
        
        // ===== 2. ДАЁМ ВРЕМЯ НА ЭВОЛЮЦИЮ =====
        for (int evolve_step = 0; evolve_step < 30; evolve_step++) {
            neural_system.step(0.0f, step_counter + evolve_step);
        }
        
        // ===== 3. НАГРАДА с УЧЁТОМ ОРБИТЫ =====
        auto& groups = neural_system.getGroupsNonConst();
        
        float base_reward = 0.6f;
        float validation_bonus = knowledge.validation_score * 0.3f;
        
        // Разные награды для разных орбит!
        for (int g = 0; g < groups.size(); g++) {
            float orbit_reward = base_reward + validation_bonus;
            
            if (g == 0) {
                // Орбита 0 - буквы, маленькая награда
                orbit_reward *= 0.3f;
            } else if (g == 1) {
                // Орбита 1 - сигналы, маленькая награда
                orbit_reward *= 0.4f;
            } else if (g == 2) {
                // Орбита 2 - слова, средняя награда
                orbit_reward *= 0.7f;
            } else if (g == 3) {
                // Орбита 3 - координация, высокая награда
                orbit_reward *= 1.0f;
            } else if (g >= 16 && g <= 21) {
                // Орбита 4 - элитные смыслы, максимальная награда!
                orbit_reward *= 1.5f;
            }
            
            groups[g].learnSTDP(orbit_reward, step_counter);
        }
        
        // ===== 4. ЭСТАФЕТНОЕ ОБУЧЕНИЕ (сверху вниз) =====
        // Элита обучает менеджеров, менеджеры - рабочих
        for (int elite = 16; elite <= 21 && elite < groups.size(); elite++) {
            for (int manager = 3; manager < 4 && manager < groups.size(); manager++) {
                // Передача знаний от элиты к менеджерам
                for (int worker = 2; worker < 3 && worker < groups.size(); worker++) {
                    // Косвенное влияние
                }
            }
        }
        
        // ===== 5. ПОВТОРЕНИЕ ДЛЯ ЗАКРЕПЛЕНИЯ =====
        for (int repeat = 0; repeat < 2; repeat++) {
            dynamic_memory_->processText(knowledge.query, "web_trainer");
            dynamic_memory_->processText(knowledge.answer, "web_trainer");
        }
        
        std::cout << "   ✅ Learned with reward distribution across orbits" << std::endl;
        std::cout << "   📊 Elite reward: " << (base_reward + validation_bonus) * 1.5f << std::endl;
        std::cout << "   📊 Word reward: " << (base_reward + validation_bonus) * 0.7f << std::endl;
    }
    
    std::vector<std::string> extractKeyWords(const std::string& text) {
        std::vector<std::string> keywords;
        std::set<std::string> stop_words = {"the", "a", "an", "and", "or", "but", "in", "on", "at", "to", "for"};
        
        std::stringstream ss(text);
        std::string word;
        while (ss >> word) {
            std::transform(word.begin(), word.end(), word.begin(), ::tolower);
            word.erase(std::remove_if(word.begin(), word.end(), ::ispunct), word.end());
            
            if (word.length() > 3 && stop_words.find(word) == stop_words.end()) {
                keywords.push_back(word);
            }
        }
        
        // Оставляем уникальные
        std::sort(keywords.begin(), keywords.end());
        keywords.erase(std::unique(keywords.begin(), keywords.end()), keywords.end());
        
        if (keywords.size() > 10) keywords.resize(10);
        return keywords;
    }
    
    std::vector<std::string> createPatternConnections(const std::vector<std::string>& words) {
        std::vector<std::string> patterns;
        
        for (size_t i = 0; i < words.size() && i < 5; i++) {
            for (size_t j = i + 1; j < words.size() && j < 5; j++) {
                patterns.push_back(words[i] + " " + words[j]);
            }
        }
        
        return patterns;
    }
    
    std::vector<std::string> extractMeanings(const std::string& text) {
        std::vector<std::string> meanings;
        
        // Ищем абстрактные концепты
        std::vector<std::string> abstract_patterns = {
            "concept of", "idea that", "meaning of", "understanding",
            "knowledge about", "wisdom", "philosophy", "theory"
        };
        
        for (const auto& pattern : abstract_patterns) {
            size_t pos = text.find(pattern);
            if (pos != std::string::npos) {
                size_t end = text.find('.', pos);
                if (end != std::string::npos) {
                    std::string meaning = text.substr(pos, end - pos);
                    if (meaning.length() < 100) {
                        meanings.push_back(meaning);
                    }
                }
            }
        }
        
        if (meanings.empty() && text.length() > 50) {
            // Берём первое предложение как смысл
            size_t end = text.find('.');
            if (end != std::string::npos) {
                meanings.push_back(text.substr(0, end));
            }
        }
        
        return meanings;
    }

        void saveToEmergentMemory(const ValidatedKnowledge& knowledge, int step) {
        auto& emergent = neural_system.emergentMutable();
        
        // Сериализуем знание в паттерн
        std::vector<float> pattern;
        for (char c : knowledge.query) pattern.push_back(static_cast<float>(c));
        pattern.push_back(0.0f);  // разделитель
        for (char c : knowledge.answer) pattern.push_back(static_cast<float>(c));
        pattern.push_back(0.0f);
        
        emergent.memory.writeSTM(
            pattern,
            knowledge.validation_score,  // importance
            0.5f,                        // entropy
            "web_knowledge_" + knowledge.query.substr(0, 20),  // tag
            step
        );
    }

public:
    WebTrainerModule(NeuralFieldSystem& ns, DynamicSemanticMemory* memory)
        : neural_system(ns), dynamic_memory_(memory), rng(std::random_device{}()) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
    
    ~WebTrainerModule() {
        stop();
        curl_global_cleanup();
    }
    
    void addQuestion(const std::string& question) {
        // 1. Проверяем validated_knowledge
        for (const auto& [q, k] : validated_knowledge) {
            if (areSimilarQuestions(q, question)) {
                std::cout << "⏭️ Already learned: " << question << std::endl;
                return;
            }
        }
        
        // 2. ДОБАВИТЬ: проверка очереди на дубликаты
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            std::queue<std::string> temp = training_queue;
            while (!temp.empty()) {
                if (areSimilarQuestions(temp.front(), question)) {
                    std::cout << "⏭️ Already in queue: " << question << std::endl;
                    return;
                }
                temp.pop();
            }
        }
        
        // 3. Добавляем в очередь
        std::lock_guard<std::mutex> lock(queue_mutex);
        training_queue.push(question);
        std::cout << "📚 Added to queue: " << question << std::endl;
    }
    
    
    bool areSimilarQuestions(const std::string& q1, const std::string& q2) {
        if (q1 == q2) return true;
        if (q1.find(q2) != std::string::npos) return true;
        if (q2.find(q1) != std::string::npos) return true;
        
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
        
        std::cout << "🌐 WebTrainer thread starting. Queue size: " << training_queue.size() << std::endl;
        
        worker_thread = std::thread([this]() {
            std::cout << "🌐 WebTrainer worker thread running" << std::endl;
            while (is_running) {
                std::string question;
                {
                    std::lock_guard<std::mutex> lock(queue_mutex);
                    if (!training_queue.empty()) {
                        question = training_queue.front();
                        training_queue.pop();
                        std::cout << "📤 Processing from queue: " << question << " (remaining: " << training_queue.size() << ")" << std::endl;
                    }
                }
                
                if (!question.empty()) {
                    processAndValidateQuestion(question);
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            std::cout << "🌐 WebTrainer worker thread stopped" << std::endl;
        });
    }

    void directEliteAttention(const ValidatedKnowledge& knowledge) {
        auto& groups = neural_system.getGroupsNonConst();
        
        // Находим элитные группы (16-21)
        for (int g = 16; g <= 21 && g < groups.size(); g++) {
            // Элита получает команду: "обрати внимание на эти слова"
            if (knowledge.validation_score > 0.8f) {
                groups[g].eliteInstruct("words", knowledge.validation_score);
                groups[g].eliteInstruct("patterns", knowledge.validation_score * 0.7f);
            }
        }
    }
    
    void processAndValidateQuestion(const std::string& question) {
        std::cout << "\n🔍 Processing: " << question << std::endl;
        
        // Проверяем bootstrap кэш
        auto it = validated_knowledge.find(question);
        if (it != validated_knowledge.end()) {
            std::cout << "   ✅ Using cached answer for: " << question << std::endl;
            // Всё равно обновляем в EmergentMemory
            saveToEmergentMemory(it->second, total_searches);
            return;
        }
        
        auto responses = searchWebMultiple(question, MAX_RETRIES_PER_QUERY);
        if (responses.empty()) {
            std::cout << "❌ No web responses for: " << question << std::endl;
            return;
        }
        
        auto extracted = extractAndValidateAnswer(responses, question);
        
        if (extracted.confidence >= MIN_VALIDATION_SCORE && !extracted.answer.empty()) {
            ValidatedKnowledge knowledge;
            knowledge.query = question;
            knowledge.answer = extracted.answer;
            knowledge.source = extracted.source;
            knowledge.validation_score = extracted.confidence;
            knowledge.validation_count = 1;
            
            validated_knowledge[question] = knowledge;
            
            // ✅ СОХРАНЯЕМ В EMERGENT MEMORY
            saveToEmergentMemory(knowledge, total_searches);
            
            learnValidatedKnowledge(knowledge);
            
            if (knowledge.validation_score > 0.7f) {
                directEliteAttention(knowledge);
                std::cout << "🎯 Elite attention directed to new knowledge!" << std::endl;
            }
            
            validated_learnings++;
            std::cout << "✅ VALIDATED! Score: " << knowledge.validation_score << std::endl;
        } else {
            rejected_answers++;
            std::cout << "❌ REJECTED (conf=" << extracted.confidence << ")" << std::endl;
        }
        total_searches++;
    }
    
    void bootstrapWithBasicKnowledge() {
        std::vector<std::pair<std::string, std::string>> validated_qa = {
            {"what is your name", "My name is Mary, your AI assistant."},
            {"who created you", "I was created by developers using neural networks with orbital architecture."},
            {"what can you do", "I can learn from conversations and search the web for validated information."},
            {"how do you learn", "I learn through neural plasticity. Information flows from lower orbits to higher orbits."},
            {"what is consciousness", "Consciousness is a complex concept involving self-awareness and perception."}
        };
        
        // СНАЧАЛА сохраняем в validated_knowledge (чтобы addQuestion видел их)
        for (const auto& [q, a] : validated_qa) {
            ValidatedKnowledge knowledge;
            knowledge.query = q;
            knowledge.answer = a;
            knowledge.source = "bootstrap";
            knowledge.validation_score = 0.95f;
            knowledge.validation_count = 1;
            validated_knowledge[q] = knowledge;
            
            // ✅ СРАЗУ СОХРАНЯЕМ В EMERGENT MEMORY
            saveToEmergentMemory(knowledge, 0);
            
            std::cout << "📚 Bootstrapped: " << q << std::endl;
        }
        
        // ПОТОМ обучаем нейросистему
        for (const auto& [q, a] : validated_qa) {
            if (dynamic_memory_) {
                dynamic_memory_->processText(q, "system");
                dynamic_memory_->processText(a, "system");
            }
        }
        
        // Запускаем поток (он будет обрабатывать новые вопросы)
        start();
    }
    
    void stop() {
        is_running = false;
        if (worker_thread.joinable()) {
            worker_thread.join();
        }
    }
    
    const ValidatedKnowledge* getValidatedKnowledge(const std::string& query) const {
        auto it = validated_knowledge.find(query);
        if (it != validated_knowledge.end()) {
            return &it->second;
        }
        
        for (const auto& [q, k] : validated_knowledge) {
            if (q.find(query) != std::string::npos || query.find(q) != std::string::npos) {
                return &k;
            }
        }
        
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
        ss << "Queue size: " << training_queue.size() << "\n";
        return ss.str();
    }
    
    void logTrainingStatus() {
        std::cout << "\n" << getStats() << std::endl;
        
        if (!validated_knowledge.empty()) {
            std::cout << "\nLast 5 validated items:" << std::endl;
            int count = 0;
            for (auto it = validated_knowledge.rbegin(); 
                 it != validated_knowledge.rend() && count < 5; 
                 ++it, ++count) {
                std::cout << "  [" << (it->second.validation_score * 100) << "%] "
                          << it->first.substr(0, 40) << "..." << std::endl;
            }
        }
    }
    // ДОБАВИТЬ: загрузка из EmergentMemory при старте
    void loadFromEmergentMemory() {
        auto& emergent = neural_system.emergentMutable();
        const auto& ltm = emergent.memory.getLTM();
        
        for (const auto& record : ltm) {
            if (record.tag.find("web_knowledge_") == 0) {
                // Десериализуем знание из паттерна
                std::string query, answer;
                size_t pos = 0;
                while (pos < record.pattern.size() && record.pattern[pos] != 0.0f) {
                    query += static_cast<char>(record.pattern[pos++]);
                }
                pos++; // пропускаем разделитель
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
                    validated_knowledge[query] = knowledge;
                }
            }
        }
        
        std::cout << "📂 Loaded " << validated_knowledge.size() 
                  << " knowledge items from EmergentMemory" << std::endl;
    }
};