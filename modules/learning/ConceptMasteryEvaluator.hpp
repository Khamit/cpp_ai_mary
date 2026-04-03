// modules/learning/ConceptMasteryEvaluator.hpp
#pragma once
#include <cstdint>
#include <vector>
#include <map>
#include "../../data/SemanticGraphDatabase.hpp"
#include "../../core/NeuralFieldSystem.hpp"

class ConceptMasteryEvaluator {
private:
    NeuralFieldSystem& neural_system;
    SemanticGraphDatabase& semantic_graph;
    
public:
    ConceptMasteryEvaluator(NeuralFieldSystem& ns, SemanticGraphDatabase& graph)
        : neural_system(ns), semantic_graph(graph) {}
    
    float measureActivationForConcept(uint32_t concept_id) {
        auto& groups = neural_system.getGroups();
        auto node = semantic_graph.getNode(concept_id);
        if (!node) return 0.0f;
        
        float total_activation = 0.0f;
        float max_possible = 0.0f;
        
        for (int g = 16; g <= 21; g++) {
            const auto& phi = groups[g].getPhi();
            for (int i = 0; i < 32; i++) {
                float affinity = node->signature[i + (g-16)*32];
                float activation = phi[i];
                total_activation += activation * affinity;
                max_possible += affinity;
            }
        }
        
        return max_possible > 0 ? total_activation / max_possible : 0.0f;
    }
    
    float measureContextualUsage(uint32_t concept_id) {
        auto node = semantic_graph.getNode(concept_id);
        if (!node || node->usage_contexts.empty()) return 0.5f;
        
        float score = 0.0f;
        int contexts_tested = 0;
        
        for (const auto& context : node->usage_contexts) {
            auto relevant = semantic_graph.getRelevantNodes(context, 10);
            
            bool is_relevant = false;
            for (uint32_t rel_id : relevant) {
                if (rel_id == concept_id) {
                    is_relevant = true;
                    break;
                }
            }
            
            if (is_relevant) {
                float context_activation = 0.0f;
                for (uint32_t rel_id : relevant) {
                    context_activation += measureActivationForConcept(rel_id);
                }
                context_activation /= relevant.size();
                score += context_activation;
                contexts_tested++;
            }
        }
        
        return contexts_tested > 0 ? score / contexts_tested : 0.5f;
    }
    
    float getNeuronAffinityToConcept(int group_idx, int neuron_idx, uint32_t concept_id) {
        auto node = semantic_graph.getNode(concept_id);
        if (!node) return 0.0f;
        
        int sig_idx = neuron_idx + (group_idx - 16) * 32;
        if (sig_idx >= 0 && sig_idx < 192) {
            return node->signature[sig_idx];
        }
        return 0.0f;
    }
    
    float getConceptMastery(uint32_t concept_id) {
        float direct = measureActivationForConcept(concept_id);
        
        auto similar = semantic_graph.getSimilarConcepts(concept_id, 5);
        float discrimination = 1.0f;
        int similar_count = 0;
        
        for (uint32_t sim_id : similar) {
            if (sim_id == concept_id) continue;
            float sim_activation = measureActivationForConcept(sim_id);
            float diff = std::abs(direct - sim_activation);
            discrimination *= (1.0f + diff);
            similar_count++;
        }
        
        if (similar_count > 0) {
            discrimination = std::pow(discrimination, 1.0f / similar_count);
            discrimination = std::clamp(discrimination, 0.5f, 1.5f);
        }
        
        float orbital_stability = 0.0f;
        auto& groups = neural_system.getGroups();
        int elite_count = 0;
        
        for (int g = 16; g <= 21; g++) {
            for (int i = 0; i < 32; i++) {
                if (groups[g].getOrbitLevel(i) >= 3) {
                    float affinity = getNeuronAffinityToConcept(g, i, concept_id);
                    if (affinity > 0.3f) {
                        orbital_stability += affinity;
                        elite_count++;
                    }
                }
            }
        }
        
        float elite_factor = elite_count > 0 ? 
            std::min(1.0f, orbital_stability / elite_count) : 0.5f;
        
        float contextual = measureContextualUsage(concept_id);
        
        float mastery = (direct * 0.3f + 
                        (discrimination - 0.5f) * 0.2f + 
                        contextual * 0.2f + 
                        elite_factor * 0.3f);
        
        return std::clamp(mastery, 0.0f, 1.0f);
    }
    
    std::map<uint32_t, float> getAllConceptsMastery() {
        std::map<uint32_t, float> result;
        for (uint32_t id = 1; id <= 614; id++) {
            auto node = semantic_graph.getNode(id);
            if (node && !node->canonical_form.empty()) {
                float mastery = getConceptMastery(id);
                if (mastery > 0.01f) {
                    result[id] = mastery;
                }
            }
        }
        return result;
    }
    
    float getAverageMastery() {
        float total = 0.0f;
        int count = 0;
        for (uint32_t id = 1; id <= 614; id++) {
            float mastery = getConceptMastery(id);
            if (mastery > 0.01f) {
                total += mastery;
                count++;
            }
        }
        return count > 0 ? total / count : 0.0f;
    }
    
    int getMasteredConceptsCount(float threshold = 0.7f) {
        int count = 0;
        for (uint32_t id = 1; id <= 614; id++) {
            if (getConceptMastery(id) >= threshold) count++;
        }
        return count;
    }
    
    std::map<int, float> getMasteryByAbstractionLevel() {
        std::map<int, float> level_mastery;
        std::map<int, int> level_count;
        
        for (uint32_t id = 1; id <= 614; id++) {
            auto node = semantic_graph.getNode(id);
            if (!node) continue;
            
            float mastery = getConceptMastery(id);
            int level = node->abstraction_level;
            level_mastery[level] += mastery;
            level_count[level]++;
        }
        
        for (auto& [level, total] : level_mastery) {
            if (level_count[level] > 0) {
                level_mastery[level] = total / level_count[level];
            }
        }
        return level_mastery;
    }
};