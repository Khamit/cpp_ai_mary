// modules/lang/LanguageModule.hpp
#pragma once
#include <string>
#include <vector>
#include <map>
#include <random>
#include <memory>
#include <deque>
#include <thread>
#include <unordered_map>
#include <mutex>      
#include <chrono>     
#include <iostream>
#include <curl/curl.h>
#include "../../core/EmergentCore.hpp"
#include "../../core/Component.hpp"
#include "../../core/NeuralFieldSystem.hpp"
#include "../../core/ImmutableCore.hpp"
#include "../../core/IAuthorization.hpp"
#include "../../data/PersonnelData.hpp"
#include "../../data/DynamicSemanticMemory.hpp"
#include "../learning/WebTrainerModule.hpp"

class CuriosityDriver;
class LearningOrchestrator;

struct LanguageStats {
    int words_learned = 0;
    int patterns_detected = 0;
    int meanings_formed = 0;
    int user_profiles = 0;
};

struct DialogueState {
    bool expecting_answer = false;
    std::string last_question;
    std::string last_response;
    std::chrono::system_clock::time_point question_time;
};

class LanguageModule : public Component {
private:
    NeuralFieldSystem& neural_system;
    ImmutableCore& immutable_core;
    IAuthorization& auth;
    PersonnelDatabase& personnel_db_;
    std::mt19937 rng_;

    std::unique_ptr<WebTrainerModule> web_trainer_;
    DialogueState dialogue_state_;
    std::unique_ptr<DynamicSemanticMemory> dynamic_memory_;
    LearningOrchestrator* orchestrator_ = nullptr;

    int process_step_counter_ = 0;
    std::thread learning_thread_;
    std::atomic<bool> learning_active_{false};
    
    std::shared_ptr<CuriosityDriver> curiosity_driver_;
    
    float external_feedback_sum_ = 0.0f;
    int external_feedback_count_ = 0;
    bool auto_learning_active_ = false;
    std::string last_user_input_;
    
    std::string system_mode_ = "personal";
    std::string default_user_ = "user";
    bool enforce_commands_ = false;
    bool emotional_responses_ = true;

    std::deque<uint32_t> recent_meanings_;
    static constexpr size_t MAX_RECENT_MEANINGS = 50;

    std::vector<std::string> split(const std::string& text);
    std::string toLower(std::string text);
    bool canExecuteCommand(const std::string& user_name, 
                          const std::string& command,
                          AccessLevel required_level);
    std::string contextualResponse(const std::string& input, int step);
    
public:
    explicit LanguageModule(NeuralFieldSystem& system, 
                        ImmutableCore& core,
                        IAuthorization& auth,
                        PersonnelDatabase& personnel_db);
        
    ~LanguageModule();

    std::string getName() const override { return "LanguageModule"; }
    bool initialize(const Config& config) override;
    void shutdown() override;
    void update(float dt) override;

    void initWebTrainer() {
        if (!dynamic_memory_) {
            std::cerr << "⚠️ Cannot initialize WebTrainer: dynamic_memory_ is null" << std::endl;
            return;
        }
        
        web_trainer_ = std::make_unique<WebTrainerModule>(
            neural_system, 
            dynamic_memory_.get()
        );
        
        web_trainer_->bootstrapWithBasicKnowledge();
        std::cout << "🌐 WebTrainer initialized and bootstrapped!" << std::endl;
    }
    
    WebTrainerModule* getWebTrainer() { return web_trainer_.get(); }

    void applyEliteInstruction(const std::string& intent);
    std::shared_ptr<CuriosityDriver> createCuriosityDriver();
    void setOrchestrator(LearningOrchestrator* orchestrator) { orchestrator_ = orchestrator; }
    bool hasCuriosityDriver() const { return curiosity_driver_ != nullptr; }
    
    std::string process(const std::string& input, const std::string& user_name = "guest");
    
    std::string formatResponse(const std::string& base_word);
    std::string readResponseFromNeuralSystem();

    bool isSpecialCommand(const std::string& input);
    bool handleSpecialCommand(const std::string& input, std::string& output);
    
    std::string getNextQuestion();
    std::string processAnswer(const std::string& answer);
    std::string getLearningStatus() const;
    std::string getDetailedStats() const;
    
    bool isExpectingAnswer() const { return dialogue_state_.expecting_answer; }
    void resetDialogue() { dialogue_state_.expecting_answer = false; }

    void giveFeedback(float rating, bool autoFeedback = false);
    double getLanguageFitness() const;

    void runAutoLearning(int steps, LearningOrchestrator* orchestrator);
    void stopAutoLearning();
    bool isAutoLearningActive() const { return auto_learning_active_; }
    
    void saveAll() { if (dynamic_memory_) dynamic_memory_->saveToMemory(); }
    int getLearnedWordsCount() const { return dynamic_memory_ ? dynamic_memory_->getWordCount() : 0; }

    LanguageStats getLanguageStats() const {
        if (!dynamic_memory_) return LanguageStats{0,0,0,0};
        return LanguageStats{
            dynamic_memory_->getWordCount(),
            dynamic_memory_->getPatternCount(),
            dynamic_memory_->getMeaningCount(),
            static_cast<int>(dynamic_memory_->getAllProfiles().size())
        };
    }

    void addExternalFeedback(float rating) {
        external_feedback_sum_ += rating;
        external_feedback_count_++;
    }
    
    float getExternalFeedbackAvg() const {
        return external_feedback_count_ > 0 ? 
               external_feedback_sum_ / external_feedback_count_ : 0.5f;
    }

    void setSystemMode(const std::string& mode, const std::string& default_user = "user") {
        system_mode_ = mode;
        default_user_ = default_user;
        std::cout << "LanguageModule switched to " << mode << " mode" << std::endl;
    }

    void setCommandEnforcement(bool enforce) { enforce_commands_ = enforce; }
    void setEmotionalResponses(bool enable) { emotional_responses_ = enable; }
    
    void setCuriosityDriver(std::shared_ptr<CuriosityDriver> driver) {
        curiosity_driver_ = driver;
    }

    std::vector<uint32_t> getRecentMeanings() const {
        return std::vector<uint32_t>(recent_meanings_.begin(), recent_meanings_.end());
    }

    int getProcessStepCounter() const { return process_step_counter_; }
    void incrementProcessStepCounter() { process_step_counter_++; }
    float evaluateResponse(const std::string& input, const std::string& response);
    void addToRecentMeanings(uint32_t meaning_id);
    
    DynamicSemanticMemory* getDynamicMemory() { return dynamic_memory_.get(); }
};