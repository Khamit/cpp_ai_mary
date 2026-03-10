// data/ExternalKnowledge.hpp
#pragma once
#include <string>
#include <vector>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "LanguageKnowledgeBase.hpp" 

class ExternalKnowledge {
public:
    static ExternalKnowledge& getInstance() {
        static ExternalKnowledge instance;
        return instance;
    }
    
    // Wikipedia API (бесплатно)
    std::string queryWikipedia(const std::string& term) {
        CURL* curl = curl_easy_init();
        std::string response;
        
        if (curl) {
            std::string url = "https://en.wikipedia.org/api/rest_v1/page/summary/" + 
                              urlEncode(term);
            
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            
            CURLcode res = curl_easy_perform(curl);
            if (res == CURLE_OK) {
                try {
                    auto json = nlohmann::json::parse(response);
                    if (json.contains("extract")) {
                        return json["extract"];
                    }
                } catch (...) {}
            }
            curl_easy_cleanup(curl);
        }
        return "";
    }
    
    // WordNet (локальная база данных)
    std::vector<std::string> getSynonyms(const std::string& word) {
        // Используем существующую базу знаний LanguageKnowledgeBase
        std::vector<std::string> result;
        
        // Пробуем получить синонимы из встроенной базы
        auto synonyms = LanguageKnowledgeBase::getInstance().getSynonyms(word);
        if (!synonyms.empty()) {
            return synonyms;
        }
        
        // Если нет в базе, возвращаем пустой вектор
        return result;
    }
    
    // ConceptNet API (бесплатно)
    std::vector<std::string> getRelatedConcepts(const std::string& term) {
        std::vector<std::string> results;
        CURL* curl = curl_easy_init();
        
        if (curl) {
            std::string url = "http://api.conceptnet.io/c/en/" + urlEncode(term);
            std::string response;
            
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            
            CURLcode res = curl_easy_perform(curl);
            if (res == CURLE_OK) {
                try {
                    auto json = nlohmann::json::parse(response);
                    for (const auto& edge : json["edges"]) {
                        if (edge["rel"]["label"] == "RelatedTo") {
                            results.push_back(edge["end"]["label"]);
                        }
                    }
                } catch (...) {}
            }
            curl_easy_cleanup(curl);
        }
        return results;
    }
    
private:
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
        size_t totalSize = size * nmemb;
        output->append((char*)contents, totalSize);
        return totalSize;
    }
    
    std::string urlEncode(const std::string& str) {
        // Простая реализация URL encoding
        std::string encoded;
        for (char c : str) {
            if (isalnum(c) || c == '-' || c == '_' || c == '.') {
                encoded += c;
            } else {
                char hex[4];
                snprintf(hex, sizeof(hex), "%%%02X", c);
                encoded += hex;
            }
        }
        return encoded;
    }
};