#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cmath>
#include <random>
#include <algorithm>

/**
 * @class SemanticGranule
 * @brief Неделимая единица смысла (не слово!)
 * 
 * Смысл не привязан к языку. "Приветствие" — это смысл,
 * который может быть выражен разными словами на разных языках.
 */
class SemanticGranule {
private:
    uint32_t id;                          // уникальный ID смысла
    std::vector<float> signature;          // 32-мерный вектор (на одну группу)
    std::string canonical_form;            // основное словесное выражение
    
    // Связи с другими смыслами (всего 200 мыслей)
    std::vector<uint32_t> parents;         // родительские смыслы
    std::vector<uint32_t> children;        // дочерние
    std::vector<uint32_t> associations;     // ассоциативные связи
    
    float activation_threshold = 0.6f;      // порог активации
    
public:
    SemanticGranule(uint32_t _id) : id(_id) {
        signature.resize(32, 0.0f);
        // Инициализация случайным, но стабильным вектором
        std::mt19937 rng(id * 1000); // детерминировано от id
        std::uniform_real_distribution<float> dist(0.3f, 0.8f);
        for (auto& val : signature) {
            val = dist(rng);
        }
    }
    
    // ID
    uint32_t getId() const { return id; }
    
    // Signature
    const std::vector<float>& getSignature() const { return signature; }
    std::vector<float>& getSignatureNonConst() { return signature; }
    
    // Canonical form
    void setCanonicalForm(const std::string& form) { canonical_form = form; }
    std::string getCanonicalForm() const { return canonical_form; }
    
    // Activation threshold
    float getActivationThreshold() const { return activation_threshold; }
    void setActivationThreshold(float thr) { activation_threshold = thr; }
    
    // Parent relationships
    const std::vector<uint32_t>& getParents() const { return parents; }
    std::vector<uint32_t>& getParentsNonConst() { return parents; }
    void addParent(uint32_t parent_id) { 
        if (std::find(parents.begin(), parents.end(), parent_id) == parents.end()) {
            parents.push_back(parent_id); 
        }
    }
    bool hasParent(uint32_t parent_id) const {
        return std::find(parents.begin(), parents.end(), parent_id) != parents.end();
    }
    
    // Child relationships
    const std::vector<uint32_t>& getChildren() const { return children; }
    std::vector<uint32_t>& getChildrenNonConst() { return children; }
    void addChild(uint32_t child_id) { 
        if (std::find(children.begin(), children.end(), child_id) == children.end()) {
            children.push_back(child_id); 
        }
    }
    bool hasChild(uint32_t child_id) const {
        return std::find(children.begin(), children.end(), child_id) != children.end();
    }
    
    // Association relationships
    const std::vector<uint32_t>& getAssociations() const { return associations; }
    std::vector<uint32_t>& getAssociationsNonConst() { return associations; }
    void addAssociation(uint32_t assoc_id) { 
        if (std::find(associations.begin(), associations.end(), assoc_id) == associations.end()) {
            associations.push_back(assoc_id); 
        }
    }
    bool hasAssociation(uint32_t assoc_id) const {
        return std::find(associations.begin(), associations.end(), assoc_id) != associations.end();
    }
    void removeAssociation(uint32_t assoc_id) {
        auto it = std::find(associations.begin(), associations.end(), assoc_id);
        if (it != associations.end()) {
            associations.erase(it);
        }
    }
    
    // Проверка, активирован ли смысл (по входному вектору)
    float getActivation(const std::vector<float>& input, int group_offset) const {
        float similarity = 0.0f;
        float norm_input = 0.0f;
        float norm_sig = 0.0f;
        
        for (int i = 0; i < 32; i++) {
            similarity += input[group_offset + i] * signature[i];
            norm_input += input[group_offset + i] * input[group_offset + i];
            norm_sig += signature[i] * signature[i];
        }
        
        if (norm_input > 0 && norm_sig > 0) {
            similarity /= (std::sqrt(norm_input) * std::sqrt(norm_sig));
        }
        
        return similarity;
    }
    
    bool isActivatedBy(const std::vector<float>& input, int group_offset) const {
        return getActivation(input, group_offset) > activation_threshold;
    }
};