// modules/learning/EffectiveLearning.cpp
#include "EffectiveLearning.hpp"
#include "../lang/LanguageModule.hpp"  // Полное определение LanguageModule
#include <cmath>
#include <iostream>
#include <algorithm>
#include <numeric>

EffectiveLearning::EffectiveLearning(NeuralFieldSystem& ns, LanguageModule& lang, 
                                     MemoryManager& mem, LanguageKnowledgeBase& languageBase)
    : neuralSystem_(ns), languageModule_(lang), memoryManager_(mem), languageBase_(languageBase) {
    
    currentLearningRate_ = baseLearningRate_;
    loadCurriculum();
    std::cout << "EffectiveLearning module initialized with " 
              << curriculum_.size() << " training examples" << std::endl;
}

void EffectiveLearning::loadCurriculum() {
    addToCurriculum("hello", "Hello! How can I help you?", 0.1f, "greeting");
    addToCurriculum("hi", "Hi there!", 0.1f, "greeting");
    addToCurriculum("hey", "Hey! How's it going?", 0.1f, "greeting");
    addToCurriculum("how are you", "I'm doing well, thanks!", 0.2f, "greeting");
    addToCurriculum("are you ok", "Yes, I'm functioning optimally!", 0.2f, "greeting");
    addToCurriculum("what is your name", "My name is Mary!", 0.3f, "question");
    addToCurriculum("who are you", "I'm Mary, your AI assistant!", 0.3f, "question");
    addToCurriculum("thanks", "You're welcome!", 0.2f, "courtesy");
    addToCurriculum("thank you", "Happy to help!", 0.2f, "courtesy");
    addToCurriculum("bye", "Goodbye! Talk to you soon!", 0.2f, "farewell");
    addToCurriculum("goodbye", "See you later!", 0.2f, "farewell");
    addToCurriculum("what can you do", "I can chat, learn, generate words, and help with various tasks!", 0.5f, "question");
    addToCurriculum("help me", "I'd be happy to help! What do you need?", 0.5f, "question");
    addToCurriculum("what is ai", "AI stands for Artificial Intelligence - systems that can learn and adapt!", 0.7f, "fact");
    addToCurriculum("what is neural network", "A neural network is a computing system inspired by biological brains!", 0.7f, "fact");
    addToCurriculum("tell me a story", "Once upon a time, there was a curious neural network...", 0.9f, "creative");
    addToCurriculum("generate something", "I'll try to generate something creative for you!", 0.9f, "creative");
}

void EffectiveLearning::addToCurriculum(const std::string& input, const std::string& response, 
                                        float difficulty, const std::string& category) {
    curriculum_.push_back({input, response, difficulty, category, 0, difficulty});
}

void EffectiveLearning::trainBatch(const std::vector<TrainingExample>& batch) {
    if (batch.empty()) return;
    
    std::vector<std::vector<double>> accumulatedGradients;
    
    for (const auto& example : batch) {
        activateForInput(example.input);
        auto targetActivity = textToActivity(example.expectedResponse);
        
        for (int g = 16; g <= 30; g++) {
            auto& group = neuralSystem_.getGroups()[g];
            group.computeGradients(targetActivity);
            
            if (accumulatedGradients.size() <= g) {
                accumulatedGradients.resize(g + 1);
            }
        }
    }
    
    for (int g = 16; g <= 30; g++) {
        auto& group = neuralSystem_.getGroups()[g];
        group.applyGradients();
    }
    
    languageModule_.giveFeedback(1.0f, true);
    
    for (int i = 0; i < 5; i++) {
        neuralSystem_.step(1.0f, totalSteps_++);
    }
}

void EffectiveLearning::adjustLearningRate(float performance) {
    const float minLR = 0.0001f;
    const float maxLR = baseLearningRate_;
    const int cycleLength = 1000;
    
    float progress = static_cast<float>(totalSteps_ % cycleLength) / cycleLength;
    float cosine = (1 + cos(progress * M_PI)) / 2;
    currentLearningRate_ = minLR + (maxLR - minLR) * cosine;
    
    if (performance > 0.8f) {
        currentLearningRate_ *= 1.1f;
    } else if (performance < 0.3f) {
        currentLearningRate_ *= 0.9f;
    }
    
    for (auto& group : neuralSystem_.getGroups()) {
        group.setLearningRate(currentLearningRate_);
    }
}

void EffectiveLearning::addToReplay(const std::string& input, const std::string& response, float priority) {
    if (replayBuffer_.size() >= MAX_BUFFER_SIZE) {
        replayBuffer_.pop_front();
    }
    
    float difficulty = estimateDifficulty(input);
    TrainingExample example{input, response, difficulty, "user", 1, priority};
    replayBuffer_.push_back(example);
}

std::vector<TrainingExample> EffectiveLearning::sampleFromReplay(int batchSize) {
    std::vector<TrainingExample> batch;
    
    if (replayBuffer_.empty()) return batch;
    
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

float EffectiveLearning::estimateDifficulty(const std::string& text) {
    auto words = split(text);
    
    float lengthScore = std::min(1.0f, words.size() / 10.0f);
    
    float rareWordScore = 0.0f;
    for (const auto& word : words) {
        if (!languageBase_.isEnglishWord(word)) {
            rareWordScore += 0.1f;
        }
    }
    rareWordScore = std::min(1.0f, rareWordScore);
    
    return (lengthScore * 0.5f + rareWordScore * 0.5f);
}

void EffectiveLearning::activateForInput(const std::string& input) {
    auto embedding = languageBase_.embedPhrase(input);
    
    for (int g = 16; g <= 24; g++) {
        auto& group = neuralSystem_.getGroups()[g];
        auto& phi = group.getPhiNonConst();
        
        for (int n = 0; n < phi.size() && n < embedding.size(); n++) {
            phi[n] = std::clamp(embedding[n] * 0.5f + 0.5f, 0.0, 1.0);
        }
    }
}

std::vector<double> EffectiveLearning::textToActivity(const std::string& text) {
    std::vector<double> activity(32, 0.0);
    auto words = split(text);
    
    for (size_t i = 0; i < words.size() && i < activity.size(); i++) {
        float freq = languageBase_.getWordFrequency(words[i]);
        activity[i] = std::clamp<double>(freq, 0.0, 1.0);
    }
    
    return activity;
}

void EffectiveLearning::runNightTraining(int hours) {
    std::cout << "\nStarting NIGHT TRAINING for " << hours << " hours..." << std::endl;
    std::cout << "======================================" << std::endl;
    
    int targetSteps = hours * 60 * 60 * 100;
    
    std::cout << "Phase 1: Curriculum Learning" << std::endl;
    for (int level = 0; level <= 9; level++) {
        float difficulty = level / 10.0f;
        trainLevel(difficulty, targetSteps / 10);
    }
    
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
    
    std::cout << "Phase 3: Consolidation" << std::endl;
    for (int i = 0; i < 10000; i++) {
        neuralSystem_.step(1.0f, totalSteps_++);
    }
    
    saveCheckpoint("night_training_complete");
    std::cout << "Night training complete!" << std::endl;
}

void EffectiveLearning::trainLevel(float difficulty, int steps) {
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

float EffectiveLearning::evaluatePerformance() {
    float totalScore = 0.0f;
    int count = 0;
    
    for (int i = 0; i < 100; i++) {
        const auto& ex = curriculum_[rand() % curriculum_.size()];
        std::string response = languageModule_.process(ex.input);
        float score = std::min(1.0f, response.length() / 50.0f);
        totalScore += score;
        count++;
    }
    
    return count > 0 ? totalScore / count : 0.0f;
}

float EffectiveLearning::evaluateOnLevel(float difficulty) {
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

void EffectiveLearning::saveCheckpoint(const std::string& name) {
    EvolutionDumpData data;
    data.generation = epoch_;
    data.metrics.code_size_score = evaluatePerformance();
    data.metrics.performance_score = currentLearningRate_;
    data.metrics.energy_score = neuralSystem_.computeTotalEnergy();
    data.metrics.overall_fitness = languageModule_.getLanguageFitness();
    
    memoryManager_.saveEvolutionState(data, "effective_learning_" + name + ".bin");
    
    std::cout << "Checkpoint saved: " << name << std::endl;
}

std::vector<std::string> EffectiveLearning::split(const std::string& text) {
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

void EffectiveLearning::printProgress(int current, int total, float performance) {
    int percent = (current * 100) / total;
    std::cout << "\r  Progress: [" << std::string(percent/2, '=') 
              << std::string(50 - percent/2, ' ') << "] " 
              << percent << "% | Perf: " << performance << "     ";
    std::cout.flush();
}