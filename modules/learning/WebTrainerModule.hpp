// modules/learning/WebTrainerModule.hpp
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "../../data/DynamicSemanticMemory.hpp"
#include "../../core/NeuralFieldSystem.hpp"

using json = nlohmann::json;

struct SearchResult {
    std::string query;
    std::string answer;
    std::string source;
    float confidence;
    int usage_count = 0;
};

class WebTrainerModule {
private:
    NeuralFieldSystem& neural_system;
    DynamicSemanticMemory* dynamic_memory_;
    
    // Очередь запросов для обучения
    std::queue<std::string> training_queue;
    std::mutex queue_mutex;
    
    // Кэш найденных ответов
    std::map<std::string, SearchResult> answer_cache;
    
    // Статистика
    int total_searches = 0;
    int successful_learnings = 0;
    
    // Флаг активности
    std::atomic<bool> is_running{false};
    std::thread worker_thread;
    
    // CURL callback
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
        size_t total_size = size * nmemb;
        output->append((char*)contents, total_size);
        return total_size;
    }
    
    // Поиск в Google (через бесплатный API)
    std::string searchGoogle(const std::string& query) {
        CURL* curl = curl_easy_init();
        std::string response;
        
        if (curl) {
            // URL encode query
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
            if (res == CURLE_OK) {
                curl_easy_cleanup(curl);
                return response;
            }
            curl_easy_cleanup(curl);
        }
        return "";
    }
    
    // Извлечение ответа из JSON
    std::string extractAnswerFromJSON(const std::string& json_response, const std::string& query) {
        try {
            auto data = json::parse(json_response);
            
            // DuckDuckGo API имеет поле AbstractText
            if (data.contains("AbstractText") && !data["AbstractText"].is_null()) {
                std::string answer = data["AbstractText"];
                if (!answer.empty()) {
                    return answer;
                }
            }
            
            // Или RelatedTopics
            if (data.contains("RelatedTopics") && data["RelatedTopics"].is_array()) {
                for (const auto& topic : data["RelatedTopics"]) {
                    if (topic.contains("Text")) {
                        std::string text = topic["Text"];
                        if (!text.empty()) {
                            return text;
                        }
                    }
                }
            }
            
            // Если не нашли, возвращаем заголовок
            if (data.contains("Heading") && !data["Heading"].is_null()) {
                return data["Heading"];
            }
            
        } catch (const std::exception& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
        }
        
        return "";
    }
    
    // Обучение на найденном ответе
    void learnFromSearch(const std::string& query, const std::string& answer) {
        if (!dynamic_memory_) return;
        
        // 1. Создаём смысл из ответа
        auto meaning_ids = dynamic_memory_->processText(answer, "web_trainer");
        
        // 2. Связываем вопрос с ответом
        auto question_words = dynamic_memory_->splitText(query);
        for (const auto& word : question_words) {
            // Используем публичный метод для проверки существования слова
            if (dynamic_memory_->hasWord(word)) {
                const auto* word_code = dynamic_memory_->getWordCode(word);
                if (word_code) {
                    auto& group1 = neural_system.getGroupsNonConst()[1];
                    for (int neuron : word_code->active_neurons) {
                        // Усиливаем связи между вопросом и ответом
                        for (uint32_t meaning_id : meaning_ids) {
                            int semantic_group = 16 + (meaning_id % 6);
                            neural_system.strengthenInterConnection(1, semantic_group, 0.05f);
                        }
                    }
                }
            }
        }
        
        // 3. Сохраняем в кэш
        SearchResult result;
        result.query = query;
        result.answer = answer;
        result.source = "web_search";
        result.confidence = 0.7f;
        answer_cache[query] = result;
        
        successful_learnings++;
        std::cout << "Learned from web: '" << query << "' -> '" << answer.substr(0, 50) << "...'" << std::endl;
    }

public:
    WebTrainerModule(NeuralFieldSystem& ns, DynamicSemanticMemory* memory)
        : neural_system(ns), dynamic_memory_(memory) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
    
    ~WebTrainerModule() {
        stop();
        curl_global_cleanup();
    }
    
    // Добавить вопрос для обучения
    void addQuestion(const std::string& question) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        training_queue.push(question);
    }
    
    // Запустить фоновое обучение
    void start() {
        if (is_running) return;
        is_running = true;
        worker_thread = std::thread([this]() {
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
                    // Проверяем кэш
                    if (answer_cache.find(question) != answer_cache.end()) {
                        const auto& cached = answer_cache[question];
                        learnFromSearch(cached.query, cached.answer);
                    } else {
                        // Ищем в интернете
                        std::string json_response = searchGoogle(question);
                        if (!json_response.empty()) {
                            std::string answer = extractAnswerFromJSON(json_response, question);
                            if (!answer.empty()) {
                                learnFromSearch(question, answer);
                                total_searches++;
                            }
                        }
                    }
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }
    
    void stop() {
        is_running = false;
        if (worker_thread.joinable()) {
            worker_thread.join();
        }
    }
    
    // Получить статистику
    std::string getStats() const {
        std::stringstream ss;
        ss << "Web Trainer Stats:\n";
        ss << "  Total searches: " << total_searches << "\n";
        ss << "  Successful learnings: " << successful_learnings << "\n";
        ss << "  Cache size: " << answer_cache.size() << "\n";
        ss << "  Queue size: " << training_queue.size() << "\n";
        return ss.str();
    }
    
    // Обучить на базовых вопросах (начальная загрузка)
    void bootstrapWithBasicKnowledge() {
        std::vector<std::string> basic_questions = {
            "What is your name?",
            "Who created you?",
            "What can you do?",
            "How are you?",
            "What is AI?",
            "What is machine learning?",
            "What is a neural network?"
        };
        
        for (const auto& q : basic_questions) {
            addQuestion(q);
        }
        
        start();
    }
};