// modules/learning/EffectiveLearning.hpp
#pragma once
#include "../../core/NeuralFieldSystem.hpp"
#include "../../core/MemoryManager.hpp"
#include "../../data/LanguageKnowledgeBase.hpp"
#include <vector>
#include <deque>
#include <random>
#include <fstream>
#include <sstream>

// Forward declaration
class LanguageModule;

struct TrainingExample {
    std::string input;
    std::string expectedResponse;
    float difficulty;
    std::string category;
    int timesUsed;
    float priority;
};

class EffectiveLearning {
private:
    NeuralFieldSystem& neuralSystem_;
    LanguageModule& languageModule_;
    MemoryManager& memoryManager_;
    LanguageKnowledgeBase& languageBase_;
    
    std::deque<TrainingExample> replayBuffer_;
    static constexpr size_t MAX_BUFFER_SIZE = 10000;
    
    std::vector<TrainingExample> curriculum_;
    int currentLevel_ = 0;
    float performanceAtLevel_ = 0.0f;
    
    float baseLearningRate_ = 0.01f;
    float currentLearningRate_;
    float momentum_ = 0.9f;
    float gradientClip_ = 1.0f;
    int batchSize_ = 32;
    
    std::vector<float> lossHistory_;
    std::vector<float> accuracyHistory_;
    int totalSteps_ = 0;
    int epoch_ = 0;
    
    std::mt19937 rng_{std::random_device{}()};
    
public:
    EffectiveLearning(NeuralFieldSystem& ns, LanguageModule& lang, 
                      MemoryManager& mem, LanguageKnowledgeBase& languageBase);
    
    void loadCurriculum();
    void addToCurriculum(const std::string& input, const std::string& response, 
                        float difficulty, const std::string& category);
    void trainBatch(const std::vector<TrainingExample>& batch);
    void adjustLearningRate(float performance);
    void addToReplay(const std::string& input, const std::string& response, 
                     float priority = 1.0f);
    std::vector<TrainingExample> sampleFromReplay(int batchSize);
    float estimateDifficulty(const std::string& text);
    void activateForInput(const std::string& input);
    std::vector<double> textToActivity(const std::string& text);
    void runNightTraining(int hours);
    void trainLevel(float difficulty, int steps);
    float evaluatePerformance();
    float evaluateOnLevel(float difficulty);
    void saveCheckpoint(const std::string& name);
    std::vector<std::string> split(const std::string& text);
    void printProgress(int current, int total, float performance);
};