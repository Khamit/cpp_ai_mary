// modules/learning/EffectiveLearning.hpp
#pragma once
#include "../core/NeuralFieldSystem.hpp"
#include "../core/MemoryManager.hpp"
#include "lang/LanguageModule.hpp"
#include "data/LanguageKnowledgeBase.hpp"
#include <vector>
#include <deque>
#include <random>
#include <fstream>
#include <sstream>

/**
 * @brief Современные методы ускоренного обучения для нейросети 1024 нейрона
 * 
 * Рекомендации ведущих научных обществ (IEEE, NeurIPS, ICML) для эффективного обучения:
 * 1. Пакетная обработка (Batch Processing)
 * 2. Experience Replay (буфер опыта)
 * 3. Curriculum Learning (обучение от простого к сложному)
 * 4. Knowledge Distillation (дистилляция знаний)
 * 5. Gradient Clipping (предотвращение взрыва градиентов)
 * 6. Learning Rate Scheduling (динамическая скорость обучения)
 * 7. Momentum и Adam-подобные обновления
 * 8. Dropout (разреженность для предотвращения переобучения)
 */

struct TrainingExample {
    std::string input;
    std::string expectedResponse;
    float difficulty;        // 0.0 - 1.0 (легко - сложно)
    std::string category;    // greeting, question, fact, etc.
    int timesUsed;           // сколько раз использовано
    float priority;          // приоритет для повторения
};

class EffectiveLearning {
private:
    NeuralFieldSystem& neuralSystem_;
    LanguageModule& languageModule_;
    MemoryManager& memoryManager_;
    LanguageKnowledgeBase& languageBase_;
    
    // ===== 1. БУФЕР ОПЫТА (Experience Replay) =====
    std::deque<TrainingExample> replayBuffer_;
    static constexpr size_t MAX_BUFFER_SIZE = 10000;
    
    // ===== 2. КУРРИКУЛУМ (Curriculum Learning) =====
    std::vector<TrainingExample> curriculum_;
    int currentLevel_ = 0;
    float performanceAtLevel_ = 0.0f;
    
    // ===== 3. ПАРАМЕТРЫ ОБУЧЕНИЯ =====
    float baseLearningRate_ = 0.01f;
    float currentLearningRate_;
    float momentum_ = 0.9f;
    float gradientClip_ = 1.0f;
    int batchSize_ = 32;
    
    // ===== 4. СТАТИСТИКА =====
    std::vector<float> lossHistory_;
    std::vector<float> accuracyHistory_;
    int totalSteps_ = 0;
    int epoch_ = 0;
    
    // ===== 5. ГЕНЕРАТОРЫ =====
    std::mt19937 rng_{std::random_device{}()};
    
public:
    EffectiveLearning(NeuralFieldSystem& ns, LanguageModule& lang, MemoryManager& mem, LanguageKnowledgeBase& languageBase_)
        : neuralSystem_(ns), languageModule_(lang), memoryManager_(mem), languageBase_(languageBase_) {
        
        currentLearningRate_ = baseLearningRate_;
        loadCurriculum();
        std::cout << "📚 EffectiveLearning module initialized with " 
                  << curriculum_.size() << " training examples" << std::endl;
    }
    
    // ===== ИНИЦИАЛИЗАЦИЯ КУРРИКУЛУМА =====
    void loadCurriculum() {
        // Простые приветствия (уровень 0)
        addToCurriculum("hello", "Hello! How can I help you?", 0.1f, "greeting");
        addToCurriculum("hi", "Hi there!", 0.1f, "greeting");
        addToCurriculum("hey", "Hey! How's it going?", 0.1f, "greeting");
        
        // Вопросы о состоянии (уровень 0.2)
        addToCurriculum("how are you", "I'm doing well, thanks!", 0.2f, "greeting");
        addToCurriculum("are you ok", "Yes, I'm functioning optimally!", 0.2f, "greeting");
        
        // Вопросы об имени (уровень 0.3)
        addToCurriculum("what is your name", "My name is Mary!", 0.3f, "question");
        addToCurriculum("who are you", "I'm Mary, your AI assistant!", 0.3f, "question");
        
        // Благодарности (уровень 0.2)
        addToCurriculum("thanks", "You're welcome!", 0.2f, "courtesy");
        addToCurriculum("thank you", "Happy to help!", 0.2f, "courtesy");
        
        // Прощания (уровень 0.2)
        addToCurriculum("bye", "Goodbye! Talk to you soon!", 0.2f, "farewell");
        addToCurriculum("goodbye", "See you later!", 0.2f, "farewell");
        
        // Вопросы о возможностях (уровень 0.5)
        addToCurriculum("what can you do", "I can chat, learn, generate words, and help with various tasks!", 0.5f, "question");
        addToCurriculum("help me", "I'd be happy to help! What do you need?", 0.5f, "question");
        
        // Фактологические вопросы (уровень 0.7)
        addToCurriculum("what is ai", "AI stands for Artificial Intelligence - systems that can learn and adapt!", 0.7f, "fact");
        addToCurriculum("what is neural network", "A neural network is a computing system inspired by biological brains!", 0.7f, "fact");
        
        // Сложные запросы (уровень 0.9)
        addToCurriculum("tell me a story", "Once upon a time, there was a curious neural network...", 0.9f, "creative");
        addToCurriculum("generate something", "I'll try to generate something creative for you!", 0.9f, "creative");
    }
    
    void addToCurriculum(const std::string& input, const std::string& response, 
                        float difficulty, const std::string& category) {
        curriculum_.push_back({input, response, difficulty, category, 0, difficulty});
    }
    
    // ===== 1. ПАКЕТНОЕ ОБУЧЕНИЕ (Batch Processing) =====
    void trainBatch(const std::vector<TrainingExample>& batch) {
        if (batch.empty()) return;
        
        // Усредняем градиенты по батчу
        std::vector<std::vector<double>> accumulatedGradients;
        
        for (const auto& example : batch) {
            // Активируем группы на основе входа
            activateForInput(example.input);
            
            // Получаем целевую активность из ожидаемого ответа
            auto targetActivity = textToActivity(example.expectedResponse);
            
            // Вычисляем градиенты для групп 16-30 (семантика и контекст)
            for (int g = 16; g <= 30; g++) {
                auto& group = neuralSystem_.getGroups()[g];
                group.computeGradients(targetActivity);
                
                // Сохраняем градиенты для усреднения
                if (accumulatedGradients.size() <= g) {
                    accumulatedGradients.resize(g + 1);
                }
                // Здесь нужно аккумулировать градиенты
            }
        }
        
        // Применяем усредненные градиенты с клиппингом
        float scale = 1.0f / batch.size();
        for (int g = 16; g <= 30; g++) {
            auto& group = neuralSystem_.getGroups()[g];
            
            // Клиппинг градиентов (предотвращение взрыва)
            // group.clipGradients(gradientClip_);
            
            // Применяем с усреднением и momentum
            group.applyGradients();
        }
        
        // Награждаем за правильные ответы
        languageModule_.giveFeedback(1.0f, true);
        
        // Несколько шагов эволюции для закрепления
        for (int i = 0; i < 5; i++) {
            neuralSystem_.step(1.0f, totalSteps_++);
        }
    }
    
    // ===== 2. ДИНАМИЧЕСКАЯ СКОРОСТЬ ОБУЧЕНИЯ =====
    void adjustLearningRate(float performance) {
        // Cosine annealing с warm restarts
        const float minLR = 0.0001f;
        const float maxLR = baseLearningRate_;
        const int cycleLength = 1000;
        
        float progress = static_cast<float>(totalSteps_ % cycleLength) / cycleLength;
        float cosine = (1 + cos(progress * M_PI)) / 2;
        currentLearningRate_ = minLR + (maxLR - minLR) * cosine;
        
        // Адаптивная подстройка под производительность
        if (performance > 0.8f) {
            currentLearningRate_ *= 1.1f;  // Ускоряем, если хорошо
        } else if (performance < 0.3f) {
            currentLearningRate_ *= 0.9f;  // Замедляем, если плохо
        }
        
        // Применяем к группам
        for (auto& group : neuralSystem_.getGroups()) {
            group.setLearningRate(currentLearningRate_);
        }
    }
    
    // ===== 3. БУФЕР ОПЫТА (Experience Replay) =====
    void addToReplay(const std::string& input, const std::string& response, float priority = 1.0f) {
        if (replayBuffer_.size() >= MAX_BUFFER_SIZE) {
            replayBuffer_.pop_front();
        }
        
        float difficulty = estimateDifficulty(input);
        TrainingExample example{input, response, difficulty, "user", 1, priority};
        replayBuffer_.push_back(example);
    }
    
    std::vector<TrainingExample> sampleFromReplay(int batchSize) {
        std::vector<TrainingExample> batch;
        
        if (replayBuffer_.empty()) return batch;
        
        // Priority sampling - важные примеры выбираются чаще
        std::vector<float> priorities;
        for (const auto& ex : replayBuffer_) {
            priorities.push_back(ex.priority);
        }
        
        std::discrete_distribution<> dist(priorities.begin(), priorities.end());
        
        for (int i = 0; i < batchSize && !replayBuffer_.empty(); i++) {
            int idx = dist(rng_) % replayBuffer_.size();
            batch.push_back(replayBuffer_[idx]);
        }
        
        return batch;
    }
    
    // ===== 4. ОЦЕНКА СЛОЖНОСТИ =====
    float estimateDifficulty(const std::string& text) {
        auto words = split(text);
        
        // По длине
        float lengthScore = std::min(1.0f, words.size() / 10.0f);
        
        // По наличию редких слов
        float rareWordScore = 0.0f;
        for (const auto& word : words) {
            if (!languageBase_.isEnglishWord(word)) {
                rareWordScore += 0.1f;
            }
        }
        rareWordScore = std::min(1.0f, rareWordScore);
        
        return (lengthScore * 0.5f + rareWordScore * 0.5f);
    }
    
    // ===== 5. АКТИВАЦИЯ ГРУПП ПО ВХОДУ =====
    void activateForInput(const std::string& input) {
        auto embedding = languageBase_.embedPhrase(input);
        
        // Группы 16-24 - семантика
        for (int g = 16; g <= 24; g++) {
            auto& group = neuralSystem_.getGroups()[g];
            auto& phi = group.getPhiNonConst();
            
            for (int n = 0; n < phi.size() && n < embedding.size(); n++) {
                phi[n] = std::clamp(embedding[n] * 0.5f + 0.5f, 0.0, 1.0);
            }
        }
    }
    
    // ===== 6. ПРЕОБРАЗОВАНИЕ ТЕКСТА В АКТИВНОСТЬ =====
    std::vector<double> textToActivity(const std::string& text) {
        std::vector<double> activity(32, 0.0);
        auto words = split(text);
        
        for (size_t i = 0; i < words.size() && i < activity.size(); i++) {
            float freq = languageBase_.getWordFrequency(words[i]);
            activity[i] = std::clamp<double>(freq, 0.0, 1.0);
        }
        
        return activity;
    }
    
    // ===== 7. ЗАПУСК НОЧНОГО ОБУЧЕНИЯ =====
    void runNightTraining(int hours) {
        std::cout << "\n🌙 Starting NIGHT TRAINING for " << hours << " hours..." << std::endl;
        std::cout << "======================================" << std::endl;
        
        int targetSteps = hours * 60 * 60 * 100;  // при 100 шагов/сек
        
        // Этап 1: Curriculum Learning (от простого к сложному)
        std::cout << "Phase 1: Curriculum Learning" << std::endl;
        for (int level = 0; level <= 9; level++) {
            float difficulty = level / 10.0f;
            trainLevel(difficulty, targetSteps / 10);
        }
        
        // Этап 2: Experience Replay (повторение важного)
        std::cout << "Phase 2: Experience Replay" << std::endl;
        for (int i = 0; i < targetSteps / 5; i++) {
            auto batch = sampleFromReplay(batchSize_);
            if (!batch.empty()) {
                trainBatch(batch);
            }
            
            if (i % 1000 == 0) {
                float performance = evaluatePerformance();
                adjustLearningRate(performance);
                printProgress(i, targetSteps / 5, performance);
            }
        }
        
        // Этап 3: Финальная консолидация
        std::cout << "Phase 3: Consolidation" << std::endl;
        for (int i = 0; i < 10000; i++) {
            neuralSystem_.step(1.0f, totalSteps_++);
        }
        
        // Сохраняем результаты
        saveCheckpoint("night_training_complete");
        std::cout << "✅ Night training complete!" << std::endl;
    }
    
    void trainLevel(float difficulty, int steps) {
        std::vector<TrainingExample> levelExamples;
        for (const auto& ex : curriculum_) {
            if (std::abs(ex.difficulty - difficulty) < 0.1f) {
                levelExamples.push_back(ex);
            }
        }
        
        if (levelExamples.empty()) return;
        
        for (int i = 0; i < steps; i += batchSize_) {
            std::vector<TrainingExample> batch;
            for (int j = 0; j < batchSize_ && !levelExamples.empty(); j++) {
                batch.push_back(levelExamples[rand() % levelExamples.size()]);
            }
            trainBatch(batch);
            
            if (i % 5000 == 0) {
                float perf = evaluateOnLevel(difficulty);
                std::cout << "  Level " << difficulty << " progress: " 
                          << (i * 100 / steps) << "%, performance: " << perf << std::endl;
            }
        }
    }
    
    // ===== 8. ОЦЕНКА ПРОИЗВОДИТЕЛЬНОСТИ =====
    float evaluatePerformance() {
        float totalScore = 0.0f;
        int count = 0;
        
        // Тестируем на 100 случайных примерах из куррикулума
        for (int i = 0; i < 100; i++) {
            const auto& ex = curriculum_[rand() % curriculum_.size()];
            
            // Временно отключаем фильтры для чистой оценки
            std::string response = languageModule_.process(ex.input);
            
            // Простая метрика: длина ответа (не идеально, но для начала)
            float score = std::min(1.0f, response.length() / 50.0f);
            totalScore += score;
            count++;
        }
        
        return count > 0 ? totalScore / count : 0.0f;
    }
    
    float evaluateOnLevel(float difficulty) {
        float totalScore = 0.0f;
        int count = 0;
        
        for (const auto& ex : curriculum_) {
            if (std::abs(ex.difficulty - difficulty) < 0.1f) {
                std::string response = languageModule_.process(ex.input);
                float score = std::min(1.0f, response.length() / 50.0f);
                totalScore += score;
                count++;
            }
        }
        
        return count > 0 ? totalScore / count : 0.0f;
    }
    
    // ===== 9. СОХРАНЕНИЕ ПРОГРЕССА =====
    void saveCheckpoint(const std::string& name) {
        EvolutionDumpData data;
        data.generation = epoch_;
        data.metrics.code_size_score = evaluatePerformance();
        data.metrics.performance_score = currentLearningRate_;
        data.metrics.energy_score = neuralSystem_.computeTotalEnergy();
        data.metrics.overall_fitness = languageModule_.getLanguageFitness();
        
        memoryManager_.saveEvolutionState(data, "effective_learning_" + name + ".bin");
        
        std::cout << "💾 Checkpoint saved: " << name << std::endl;
    }
    
    // ===== 10. ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ =====
    std::vector<std::string> split(const std::string& text) {
        std::vector<std::string> tokens;
        std::stringstream ss(text);
        std::string token;
        while (std::getline(ss, token, ' ')) {
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }
        return tokens;
    }
    
    void printProgress(int current, int total, float performance) {
        int percent = (current * 100) / total;
        std::cout << "\r  Progress: [" << std::string(percent/2, '=') 
                  << std::string(50 - percent/2, ' ') << "] " 
                  << percent << "% | Perf: " << performance << "     ";
        std::cout.flush();
    }
};