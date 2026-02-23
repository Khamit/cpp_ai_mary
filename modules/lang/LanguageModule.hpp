// modules/lang/LanguageModule.hpp - исправленная версия

#pragma once
#include <string>
#include <vector>
#include <cctype>
#include <iostream>  
#include <algorithm>
#include <random>
#include <cmath>
#include <fstream>
#include <unordered_map>
#include <set>
#include <map>
#include "../../core/NeuralFieldSystem.hpp" 

// Структура для хранения выученного слова
struct LearnedWord {
    std::string word;
    float frequency;        // как часто используется
    float correctness;      // насколько правильное (0-1)
    std::vector<float> neural_pattern; // паттерн в нейронах
    int times_rated;         // сколько раз оценивали
    float letter_probabilities[26]; // вероятности букв для этого слова
};

class LanguageModule {
public:
    explicit LanguageModule(NeuralFieldSystem& system) 
        : system_(system), 
          rng_(std::random_device{}()),
          word_length_dist_(2, 8), // от 2 до 8 букв
          letter_dist_(0, 25) {
        
        // Инициализация алфавита
        alphabet_ = {
            'a','b','c','d','e','f','g','h','i','j','k','l','m',
            'n','o','p','q','r','s','t','u','v','w','x','y','z'
        };
        
        // Инициализируем вероятности букв (изначально равномерно)
        for (int i = 0; i < 26; ++i) {
            base_letter_probs_[i] = 1.0f / 26.0f;
        }
        
        // Загружаем сохраненные слова
        loadLearnedWords();
        
        std::cout << "LanguageModule initialized with " << learned_words_.size() 
                  << " learned words" << std::endl;
    }

    std::string process(const std::string& input) {
        std::cout << "LanguageModule::process called with: '" << input << "'" << std::endl;
        
        last_input_ = input;
        std::string response;

        // Команды
        if (input == "generate" || input == "new word") {
            std::string new_word = generateWordFromNeurons();
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
            response = "Hello! I'm learning to generate words. Type 'generate' to see a new word!";
        }
        else {
            // Генерируем слово на основе нейронной активности
            std::string new_word = generateWordFromNeurons();
            response = getRandomResponse() + " '" + new_word + "'?";
        }

        encodeToNeuralField(response);
        return response;
    }

    void giveFeedback(float rating) {
        std::cout << "Feedback received: " << rating << std::endl;
        
        // Получаем последнее сгенерированное слово
        std::string last_word = decodeLastWord();
        
        if (!last_word.empty()) {
            std::cout << "Feedback for word: '" << last_word << "'" << std::endl;
            
            // Обновляем вероятности букв на основе оценки
            updateLetterProbabilities(last_word, rating);
            
            // Обучаем нейросеть
            learnFromFeedback(last_word, rating);
            
            // Сохраняем если оценка высокая
            if (rating > 0.7f) {
                saveWord(last_word, rating);
                std::cout << "(+) Learned new word: '" << last_word << "'" << std::endl;
            } else if (rating < 0.3f) {
                std::cout << "(-) Rejected word: '" << last_word << "'" << std::endl;
                // Ослабляем связи для неудачных слов
                weakenWordPattern(last_word);
            }
            
            // Сохраняем вероятности
            saveProbabilities();
        }
    }

private:
    NeuralFieldSystem& system_;
    std::mt19937 rng_;
    std::uniform_int_distribution<> word_length_dist_;
    std::uniform_int_distribution<> letter_dist_;
    std::vector<char> alphabet_;
    float base_letter_probs_[26]; // базовые вероятности букв
    
    std::string last_input_;
    std::unordered_map<std::string, LearnedWord> learned_words_;
    std::vector<std::string> word_history_;
    
    // Генерация слова на основе нейронной активности
    std::string generateWordFromNeurons() {
        std::string word;
        
        // Получаем состояние нейронов
        const auto& phi = system_.getPhi();
        
        // 1. Определяем длину слова на основе энергии нейронов
        float total_energy = 0;
        for (int i = NeuralFieldSystem::LANGUAGE_START; i < NeuralFieldSystem::LANGUAGE_END; ++i) {
            total_energy += std::abs(static_cast<float>(phi[i]));
        }
        
        // Энергия влияет на длину (больше энергия = длиннее слово)
        float energy_factor = total_energy / 50.0f; // нормализация
        energy_factor = std::clamp(energy_factor, 0.3f, 1.5f);
        
        int base_length = word_length_dist_(rng_);
        int word_length = static_cast<int>(base_length * energy_factor);
        word_length = std::clamp(word_length, 2, 8);
        
        // 2. Генерируем каждую букву на основе паттернов нейронов
        for (int pos = 0; pos < word_length; ++pos) {
            // Находим наиболее активные нейроны для этой позиции
            float position_probs[26] = {0};
            
            // Каждая позиция связана с группой нейронов
            int neuron_start = (pos * NeuralFieldSystem::LANGUAGE_NEURONS) / word_length;
            int neuron_end = ((pos + 1) * NeuralFieldSystem::LANGUAGE_NEURONS) / word_length;
            
            // Собираем активации нейронов для этой позиции
            for (int i = neuron_start; i < neuron_end && i < NeuralFieldSystem::LANGUAGE_END; ++i) {
                float activation = std::abs(static_cast<float>(phi[i]));
                
                // Каждый нейрон влияет на несколько букв
                for (int letter = 0; letter < 26; ++letter) {
                    float letter_factor = 1.0f - std::abs(letter - (i % 26)) / 26.0f;
                    position_probs[letter] += activation * letter_factor;
                }
            }
            
            // Добавляем базовые вероятности (из предыдущего опыта)
            for (int letter = 0; letter < 26; ++letter) {
                position_probs[letter] += base_letter_probs_[letter] * 0.3f;
            }
            
            // Добавляем случайность для исследования
            float random_factor = std::uniform_real_distribution<>(0.8f, 1.2f)(rng_);
            for (int letter = 0; letter < 26; ++letter) {
                position_probs[letter] *= random_factor;
            }
            
            // Нормализуем вероятности
            float sum = 0;
            for (int letter = 0; letter < 26; ++letter) {
                sum += position_probs[letter];
            }
            
            if (sum > 0) {
                for (int letter = 0; letter < 26; ++letter) {
                    position_probs[letter] /= sum;
                }
            } else {
                // Если сумма нулевая, используем равномерное распределение
                for (int letter = 0; letter < 26; ++letter) {
                    position_probs[letter] = 1.0f / 26.0f;
                }
            }
            
            // Выбираем букву на основе вероятностей
            float r = std::uniform_real_distribution<>(0, 1)(rng_);
            float cumulative = 0;
            int chosen_letter = 0;
            
            for (int letter = 0; letter < 26; ++letter) {
                cumulative += position_probs[letter];
                if (r <= cumulative) {
                    chosen_letter = letter;
                    break;
                }
            }
            
            word += alphabet_[chosen_letter];
        }
        
        // Иногда делаем первую букву заглавной
        if (std::uniform_real_distribution<>(0,1)(rng_) < 0.2f) {
            word[0] = std::toupper(word[0]);
        }
        
        // Сохраняем в историю
        word_history_.push_back(word);
        if (word_history_.size() > 20) {
            word_history_.erase(word_history_.begin());
        }
        
        // Усиливаем нейронный паттерн для этого слова
        enhanceNeuralPattern(word);
        
        return word;
    }
    
    // Усиление нейронного паттерна для слова
    void enhanceNeuralPattern(const std::string& word) {
        auto& phi = system_.getPhi();
        auto& W = system_.getWeights();
        
        int word_length = word.size();
        
        for (int pos = 0; pos < word_length; ++pos) {
            char c = std::tolower(word[pos]);
            if (c < 'a' || c > 'z') continue;
            
            int letter_idx = c - 'a';
            int neuron_start = (pos * NeuralFieldSystem::LANGUAGE_NEURONS) / word_length;
            int neuron_end = ((pos + 1) * NeuralFieldSystem::LANGUAGE_NEURONS) / word_length;
            
            // Усиливаем нейроны, соответствующие этой букве
            for (int i = neuron_start; i < neuron_end && i < NeuralFieldSystem::LANGUAGE_END; ++i) {
                float target_activation = 0.3f + (letter_idx / 26.0f) * 0.5f;
                phi[i] = phi[i] * 0.7f + target_activation * 0.3f;
                
                // Усиливаем связи между нейронами одной позиции
                for (int j = neuron_start; j < neuron_end; ++j) {
                    if (i != j) {
                        W[i][j] += 0.01f;
                        W[j][i] = W[i][j];
                        
                        if (W[i][j] > 0.3f) W[i][j] = 0.3f;
                    }
                }
            }
        }
    }
    
    // Ослабление паттерна для неудачного слова
    void weakenWordPattern(const std::string& word) {
        auto& phi = system_.getPhi();
        
        int word_length = word.size();
        
        for (int pos = 0; pos < word_length; ++pos) {
            int neuron_start = (pos * NeuralFieldSystem::LANGUAGE_NEURONS) / word_length;
            int neuron_end = ((pos + 1) * NeuralFieldSystem::LANGUAGE_NEURONS) / word_length;
            
            for (int i = neuron_start; i < neuron_end && i < NeuralFieldSystem::LANGUAGE_END; ++i) {
                phi[i] *= 0.8f; // ослабляем активацию
            }
        }
    }
    
    // Обновление вероятностей букв на основе оценки
    void updateLetterProbabilities(const std::string& word, float rating) {
        float learning_rate = (rating - 0.5f) * 0.2f;
        
        for (char c : word) {
            char lower_c = std::tolower(c);
            if (lower_c >= 'a' && lower_c <= 'z') {
                int idx = lower_c - 'a';
                // Обновляем вероятность этой буквы
                base_letter_probs_[idx] += learning_rate;
                if (base_letter_probs_[idx] < 0.01f) base_letter_probs_[idx] = 0.01f;
            }
        }
        
        // Нормализуем вероятности
        float sum = 0;
        for (int i = 0; i < 26; ++i) {
            sum += base_letter_probs_[i];
        }
        for (int i = 0; i < 26; ++i) {
            base_letter_probs_[i] /= sum;
        }
    }
    
    // Сохранение вероятностей
    void saveProbabilities() {
        std::ofstream file("letter_probs.txt");
        if (file.is_open()) {
            for (int i = 0; i < 26; ++i) {
                file << alphabet_[i] << ":" << base_letter_probs_[i] << "\n";
            }
            file.close();
        }
    }
    
    // Обучение на основе обратной связи (улучшенная версия)
    void learnFromFeedback(const std::string& word, float rating) {
        auto& W = system_.getWeights();
        auto& phi = system_.getPhi();
        
        float learning_rate = (rating - 0.5f) * 0.3f; // более агрессивное обучение
        
        int word_length = word.size();
        
        for (int pos = 0; pos < word_length; ++pos) {
            char c = std::tolower(word[pos]);
            if (c < 'a' || c > 'z') continue;
            
            int letter_idx = c - 'a';
            int neuron_start = (pos * NeuralFieldSystem::LANGUAGE_NEURONS) / word_length;
            int neuron_end = ((pos + 1) * NeuralFieldSystem::LANGUAGE_NEURONS) / word_length;
            
            for (int i = neuron_start; i < neuron_end && i < NeuralFieldSystem::LANGUAGE_END; ++i) {
                // Обновляем активацию нейрона
                float target = 0.3f + (letter_idx / 26.0f) * 0.5f;
                double delta = (target - static_cast<float>(phi[i])) * learning_rate * 2.0;
                phi[i] = std::clamp(phi[i] + delta, 0.0, 1.0);
                
                // Обновляем веса с другими нейронами
                for (int k = 0; k < NeuralFieldSystem::LANGUAGE_END; ++k) {
                    if (k != i) {
                        double hebb_update = learning_rate * phi[i] * phi[k];
                        W[i][k] += hebb_update;
                        W[k][i] = W[i][k];
                        
                        // ИСПРАВЛЕНО: используем double для clamp
                        W[i][k] = std::clamp(W[i][k], -0.3, 0.3);
                        W[k][i] = W[i][k];
                    }
                }
            }
        }
    }
    
    // Сохранение выученного слова
    void saveWord(const std::string& word, float rating) {
        LearnedWord lw;
        lw.word = word;
        lw.frequency = 1.0f;
        lw.correctness = rating;
        lw.times_rated = 1;
        
        // Сохраняем текущие вероятности букв
        for (int i = 0; i < 26; ++i) {
            lw.letter_probabilities[i] = base_letter_probs_[i];
        }
        
        // Сохраняем паттерн нейронов
        const auto& phi = system_.getPhi();
        for (int i = NeuralFieldSystem::LANGUAGE_START; i < NeuralFieldSystem::LANGUAGE_END; ++i) {
            lw.neural_pattern.push_back(static_cast<float>(phi[i]));
        }
        
        learned_words_[word] = lw;
        saveLearnedWords();
    }
    
    // Сохранение всех слов в файл
    void saveLearnedWords() {
        std::ofstream file("learned_words.txt");
        if (file.is_open()) {
            for (const auto& [word, data] : learned_words_) {
                file << word << "|" 
                     << data.frequency << "|" 
                     << data.correctness << "|" 
                     << data.times_rated << "\n";
            }
            file.close();
        }
        saveNeuralPatterns();
    }
    
    // Сохранение нейронных паттернов
    void saveNeuralPatterns() {
        std::ofstream file("neural_patterns.dat", std::ios::binary);
        if (file.is_open()) {
            size_t count = learned_words_.size();
            file.write(reinterpret_cast<char*>(&count), sizeof(count));
            
            for (const auto& [word, data] : learned_words_) {
                size_t word_len = word.size();
                file.write(reinterpret_cast<char*>(&word_len), sizeof(word_len));
                file.write(word.c_str(), word_len);
                file.write(reinterpret_cast<const char*>(data.neural_pattern.data()), 
                          data.neural_pattern.size() * sizeof(float));
            }
            file.close();
        }
    }
    
    // Загрузка выученных слов
    void loadLearnedWords() {
        std::ifstream file("learned_words.txt");
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                size_t pos1 = line.find('|');
                size_t pos2 = line.find('|', pos1 + 1);
                size_t pos3 = line.find('|', pos2 + 1);
                
                if (pos1 != std::string::npos && 
                    pos2 != std::string::npos && 
                    pos3 != std::string::npos) {
                    
                    LearnedWord lw;
                    lw.word = line.substr(0, pos1);
                    lw.frequency = std::stof(line.substr(pos1 + 1, pos2 - pos1 - 1));
                    lw.correctness = std::stof(line.substr(pos2 + 1, pos3 - pos2 - 1));
                    lw.times_rated = std::stoi(line.substr(pos3 + 1));
                    
                    learned_words_[lw.word] = lw;
                }
            }
            file.close();
        }
        loadNeuralPatterns();
        loadProbabilities();
    }
    
    // Загрузка вероятностей букв
    void loadProbabilities() {
        std::ifstream file("letter_probs.txt");
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                size_t pos = line.find(':');
                if (pos != std::string::npos) {
                    char letter = line[0];
                    float prob = std::stof(line.substr(pos + 1));
                    if (letter >= 'a' && letter <= 'z') {
                        base_letter_probs_[letter - 'a'] = prob;
                    }
                }
            }
            file.close();
        }
    }
    
    // Загрузка нейронных паттернов
    void loadNeuralPatterns() {
        std::ifstream file("neural_patterns.dat", std::ios::binary);
        if (file.is_open()) {
            size_t count;
            file.read(reinterpret_cast<char*>(&count), sizeof(count));
            
            for (size_t i = 0; i < count; ++i) {
                size_t word_len;
                file.read(reinterpret_cast<char*>(&word_len), sizeof(word_len));
                
                std::string word(word_len, ' ');
                file.read(&word[0], word_len);
                
                if (learned_words_.find(word) != learned_words_.end()) {
                    auto& lw = learned_words_[word];
                    lw.neural_pattern.resize(NeuralFieldSystem::LANGUAGE_NEURONS);
                    file.read(reinterpret_cast<char*>(lw.neural_pattern.data()), 
                             NeuralFieldSystem::LANGUAGE_NEURONS * sizeof(float));
                } else {
                    file.seekg(NeuralFieldSystem::LANGUAGE_NEURONS * sizeof(float), std::ios::cur);
                }
            }
            file.close();
        }
    }
    
    // Получение значения слова
    std::string getWordMeaning(const std::string& word) {
        auto it = learned_words_.find(word);
        if (it != learned_words_.end()) {
            return "'" + word + "' (learned, correctness: " + 
                   std::to_string(static_cast<int>(it->second.correctness * 100)) + "%)";
        } else {
            return "I haven't learned '" + word + "' yet. Try to generate it!";
        }
    }
    
    // Получение статистики
    std::string getStats() {
        std::string result = " Language Stats:\n";
        result += "Learned words: " + std::to_string(learned_words_.size()) + "\n";
        result += "Letter probabilities:\n";
        
        // Показываем топ-5 букв
        std::vector<std::pair<char, float>> probs;
        for (int i = 0; i < 26; ++i) {
            probs.push_back({alphabet_[i], base_letter_probs_[i]});
        }
        
        std::sort(probs.begin(), probs.end(), 
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        
        for (int i = 0; i < 5 && i < probs.size(); ++i) {
            result += probs[i].first + std::string(": ") + 
                      std::to_string(static_cast<int>(probs[i].second * 100)) + "%\n";
        }
        
        return result;
    }
    
    // Случайный ответ
    std::string getRandomResponse() {
        static std::vector<std::string> responses = {
            "How about",
            "What do you think of",
            "I've been thinking about",
            "Try this one:",
            "Maybe",
            "Is this a word:"
        };
        
        std::uniform_int_distribution<> dist(0, responses.size() - 1);
        return responses[dist(rng_)];
    }
    
    // Декодирование последнего слова
    std::string decodeLastWord() {
        if (word_history_.empty()) return "";
        return word_history_.back();
    }
    
    void encodeToNeuralField(const std::string& text) {
        auto& phi = system_.getPhi();
        
        for (int i = NeuralFieldSystem::LANGUAGE_START; i < NeuralFieldSystem::LANGUAGE_END; ++i) {
            phi[i] = 0.0;
        }

        size_t limit = std::min(text.size(), static_cast<size_t>(NeuralFieldSystem::LANGUAGE_NEURONS));
        for (size_t i = 0; i < limit; ++i) {
            phi[i] = static_cast<double>(
                static_cast<unsigned char>(text[i])
            ) / 255.0;
        }
    }
};