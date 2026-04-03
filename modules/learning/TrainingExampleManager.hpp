// modules/learning/TrainingExampleManager.hpp
#pragma once
#include <vector>
#include <deque>
#include <string>
#include <map>
#include <random>
#include "../../data/MeaningTrainingExample_fwd.hpp"
#include "../../core/AccessLevel.hpp"
#include "../../data/SemanticGraphDatabase.hpp"
#include "../semantic/SemanticManager.hpp"

class TrainingExampleManager {
private:
    std::deque<MeaningTrainingExample> replay_buffer_;
    static constexpr size_t MAX_BUFFER_SIZE = 10000;
    
    SemanticGraphDatabase& semantic_graph;
    SemanticManager& semantic_manager;
    
    std::map<std::string, int> word_fail_count_;
    std::map<std::string, int> word_success_count_;
    std::map<std::string, int> word_total_attempts_;
    std::mt19937 rng_;
    
    // Для циклического обучения
    std::vector<uint32_t> all_concept_ids_;
    size_t current_concept_index_ = 0;
    std::deque<uint32_t> recent_concepts_;
    static constexpr int MAX_RECENT_CONCEPTS = 20;
    
public:
    TrainingExampleManager(SemanticGraphDatabase& graph, SemanticManager& sm)
        : semantic_graph(graph), semantic_manager(sm), rng_(std::random_device{}()) {
        loadAllConceptIds();
    }
    
    void loadAllConceptIds() {
        all_concept_ids_.clear();
        for (uint32_t id = 1; id <= 614; id++) {
            if (semantic_graph.getNode(id)) {
                all_concept_ids_.push_back(id);
            }
        }
    }
    
    void addExample(const MeaningTrainingExample& example) {
        replay_buffer_.push_back(example);
        if (replay_buffer_.size() > MAX_BUFFER_SIZE) {
            replay_buffer_.pop_front();
        }
    }
    
    void addMeaningExample(
        const std::vector<std::string>& input_words,
        const std::vector<std::string>& expected_meanings,
        const std::vector<std::pair<uint32_t, uint32_t>>& cause_effect,
        float difficulty,
        const std::string& category,
        AccessLevel required = AccessLevel::GUEST) {
        
        MeaningTrainingExample example;
        example.input_words = input_words;
        example.difficulty = difficulty;
        example.category = category;
        example.priority = difficulty;
        example.required_level = required;
        
        for (const auto& meaning_name : expected_meanings) {
            uint32_t id = semantic_manager.getMeaningId(meaning_name);
            if (id > 0) {
                example.expected_meanings.push_back(id);
            }
        }
        
        example.cause_effect = cause_effect;
        addExample(example);
    }
    
    const MeaningTrainingExample* selectExampleByPriority() {
        if (replay_buffer_.empty()) return nullptr;
        
        float total_priority = 0;
        for (const auto& ex : replay_buffer_) {
            total_priority += ex.priority;
        }
        
        float r = static_cast<float>(rng_()) / RAND_MAX * total_priority;
        float cumsum = 0;
        
        for (size_t j = 0; j < replay_buffer_.size(); j++) {
            cumsum += replay_buffer_[j].priority;
            if (r <= cumsum) {
                return &replay_buffer_[j];
            }
        }
        
        return &replay_buffer_[0];
    }
    
    const MeaningTrainingExample* selectExampleForConcept(uint32_t concept_id) {
        for (const auto& ex : replay_buffer_) {
            for (uint32_t mid : ex.expected_meanings) {
                if (mid == concept_id) {
                    return &ex;
                }
            }
        }
        return nullptr;
    }
    
    void updatePriority(const MeaningTrainingExample* example, float new_priority) {
        // Находим и обновляем приоритет
        for (auto& ex : replay_buffer_) {
            if (&ex == example) {
                ex.priority = std::clamp(new_priority, 0.1f, 2.0f);
                break;
            }
        }
    }
    
    void recordWordAttempt(const std::string& word, bool success) {
        word_total_attempts_[word]++;
        if (success) {
            word_success_count_[word]++;
            word_fail_count_[word] = 0;
        } else {
            word_fail_count_[word]++;
        }
    }
    
    int getWordAttempts(const std::string& word) const {
        auto it = word_total_attempts_.find(word);
        return it != word_total_attempts_.end() ? it->second : 0;
    }
    
    int getWordSuccessCount(const std::string& word) const {
        auto it = word_success_count_.find(word);
        return it != word_success_count_.end() ? it->second : 0;
    }
    
    int getWordFailCount(const std::string& word) const {
        auto it = word_fail_count_.find(word);
        return it != word_fail_count_.end() ? it->second : 0;
    }
    
    void markConceptProcessed(uint32_t concept_id) {
        recent_concepts_.push_back(concept_id);
        if (recent_concepts_.size() > MAX_RECENT_CONCEPTS) {
            recent_concepts_.pop_front();
        }
    }
    
    bool wasConceptRecentlyProcessed(uint32_t concept_id) const {
        for (uint32_t recent_id : recent_concepts_) {
            if (recent_id == concept_id) return true;
        }
        return false;
    }
    
    uint32_t getNextConceptInCycle() {
        if (all_concept_ids_.empty()) return 0;
        uint32_t concept = all_concept_ids_[current_concept_index_];
        current_concept_index_ = (current_concept_index_ + 1) % all_concept_ids_.size();
        return concept;
    }
    
    size_t getBufferSize() const { return replay_buffer_.size(); }
    
    std::vector<MeaningTrainingExample> getExamplesByAbstraction(int min_abs, int max_abs) {
        std::vector<MeaningTrainingExample> result;
        for (const auto& ex : replay_buffer_) {
            for (uint32_t mid : ex.expected_meanings) {
                auto node = semantic_graph.getNode(mid);
                if (node && node->abstraction_level >= min_abs && 
                    node->abstraction_level <= max_abs) {
                    result.push_back(ex);
                    break;
                }
            }
        }
        return result;
    }
    
    std::vector<MeaningTrainingExample> getExamplesWithCauseEffect() {
        std::vector<MeaningTrainingExample> result;
        for (const auto& ex : replay_buffer_) {
            if (!ex.cause_effect.empty()) {
                result.push_back(ex);
            }
        }
        return result;
    }
};