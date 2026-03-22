// modules/learning/NeuralTrainer.hpp
#pragma once
#include <atomic>
#include <vector>
#include <deque>
#include <random>
#include "../../core/NeuralFieldSystem.hpp"
#include "../../data/SemanticGraphDatabase.hpp"
#include "ConceptMasteryEvaluator.hpp"

// Forward declaration
class TrainingExampleManager; 

class NeuralTrainer {
private:
    NeuralFieldSystem& neural_system;
    SemanticGraphDatabase& semantic_graph;
    ConceptMasteryEvaluator& mastery_evaluator;
    
    std::mt19937 rng_;
    std::atomic<bool> training_active_{false};
    std::atomic<bool> is_processing_{false};
    
    int total_steps_ = 0;
    float current_learning_rate_ = 0.01f;
    const float base_learning_rate_ = 0.01f;
    
    std::vector<float> accuracy_history_;
    std::map<std::pair<int, int>, uint32_t> neuron_specialization;
    std::map<uint32_t, std::vector<std::pair<int, int>>> concept_neurons;

    int getGroupForConcept(uint32_t id) const {
        auto node = semantic_graph.getNode(id);
        if (!node) return id % 6;
        
        const std::string& cat = node->primary_category;
        
        if (cat == "device" || cat == "sensor" || cat == "object") return 0;
        if (cat == "action" || cat == "function" || cat == "command") return 1;
        if (cat == "state" || cat == "result" || cat == "property") return 2;
        if (cat == "logic" || cat == "causality" || cat == "abstract") return 3;
        if (cat == "communication" || cat == "social") return 4;
        if (cat == "metaphysical" || cat == "value" || cat == "identity") return 5;
        
        return id % 6;
    }
    
public:
    NeuralTrainer(NeuralFieldSystem& ns, SemanticGraphDatabase& graph, 
                  ConceptMasteryEvaluator& evaluator)
        : neural_system(ns), semantic_graph(graph), 
          mastery_evaluator(evaluator), rng_(std::random_device{}()) {}

    NeuralFieldSystem& getNeuralSystem() { return neural_system; }
    const NeuralFieldSystem& getNeuralSystem() const { return neural_system; }
    
    void activateMeaning(uint32_t meaning_id, float strength) {
        auto node = semantic_graph.getNode(meaning_id);
        if (!node) return;
        
        auto& groups = neural_system.getGroupsNonConst();
        
        float sig_sum = 0.0f;
        for (int g = 16; g <= 21; g++) {
            for (int i = 0; i < 32; i++) {
                sig_sum += node->signature[i + (g-16)*32];
            }
        }
        
        if (sig_sum < 0.001f) return;
        
        for (int g = 16; g <= 21; g++) {
            auto& phi = groups[g].getPhiNonConst();
            for (int i = 0; i < 32; i++) {
                float normalized_sig = node->signature[i + (g-16)*32] / sig_sum;
                
                float orbit_multiplier = 1.0f;
                if (groups[g].getOrbitLevel(i) >= 3) {
                    orbit_multiplier = 2.0f;
                } else if (groups[g].getOrbitLevel(i) <= 1) {
                    orbit_multiplier = 0.5f;
                }
                
                phi[i] += strength * 0.2f * normalized_sig * orbit_multiplier;
                phi[i] = std::clamp(phi[i], 0.0, 1.0);
            }
        }
    }
    
    void applySTDPReward(float reward) {
        auto& groups = neural_system.getGroupsNonConst();
        for (int g = 16; g <= 21; g++) {
            groups[g].learnSTDP(reward, total_steps_);
        }
    }
    
    float computeReward(float activation_score, float success_threshold, uint32_t concept_id = 0) {
        if (activation_score > success_threshold) {
            // Успех - большая награда с орбитальным бонусом
            float base_reward = 5.0f;
            
            if (concept_id > 0) {
                auto& groups = neural_system.getGroupsNonConst();
                int group_idx = 16 + (concept_id % 6);
                if (group_idx < groups.size()) {
                    const auto& group = groups[group_idx];
                    float avg_orbit = 0.0f;
                    for (int i = 0; i < 32; i++) {
                        avg_orbit += group.getOrbitLevel(i);
                    }
                    avg_orbit /= 32.0f;
                    base_reward *= (1.0f + avg_orbit * 0.25f);
                }
            }
            
            return base_reward;
        } else {
            // Маленькая награда за попытку
            return 0.1f;
        }
    }
    
    std::vector<float> createInputSignal(const std::string& word) {
        std::vector<float> input_signal(32, 0.0f);
        uint32_t word_hash = std::hash<std::string>{}(word);
        int neuron_idx = word_hash % 32;
        
        input_signal[neuron_idx] = 1.0f;
        
        std::uniform_real_distribution<float> noise_dist(-0.1f, 0.1f);
        for (int i = 0; i < 32; i++) {
            if (i != neuron_idx) {
                input_signal[i] += noise_dist(rng_);
            }
        }
        
        float max_val = *std::max_element(input_signal.begin(), input_signal.end());
        if (max_val > 1.0f) {
            for (auto& v : input_signal) v /= max_val;
        }
        
        return input_signal;
    }
    
    float measureActivationForMeanings(const std::vector<uint32_t>& meaning_ids) {
        float total = 0.0f;
        for (uint32_t id : meaning_ids) {
            total += mastery_evaluator.measureActivationForConcept(id);
        }
        return total;
    }
    
    void trainRelation(uint32_t from_id, uint32_t to_id, SemanticEdge::Type relation) {
        auto& groups = neural_system.getGroups();
        
        std::vector<float> before_activation;
        for (int g = 16; g <= 21; g++) {
            before_activation.push_back(groups[g].getAverageActivity());
        }
        
        activateMeaning(from_id, 1.0f);
        
        for (int i = 0; i < 10; i++) {
            neural_system.step(0.0f, total_steps_ + i);
        }
        
        float to_activation = mastery_evaluator.measureActivationForConcept(to_id);
        
        float expected_strength = 0.5f;
        switch(relation) {
            case SemanticEdge::Type::IS_A: 
            case SemanticEdge::Type::IS_A_KIND_OF: expected_strength = 0.9f; break;
            case SemanticEdge::Type::CAUSES: 
            case SemanticEdge::Type::LEADS_TO: expected_strength = 0.8f; break;
            case SemanticEdge::Type::PART_OF: 
            case SemanticEdge::Type::HAS_PART: expected_strength = 0.7f; break;
            case SemanticEdge::Type::SIMILAR_TO: 
            case SemanticEdge::Type::RELATED_TO: expected_strength = 0.6f; break;
            case SemanticEdge::Type::OPPOSITE_OF: 
            case SemanticEdge::Type::CONTRADICTS: expected_strength = 0.2f; break;
            default: expected_strength = 0.5f;
        }
        
        float error = std::abs(to_activation - expected_strength);
        float reward = 1.0f - error;
        
        if (relation == SemanticEdge::Type::OPPOSITE_OF) {
            reward = 1.0f - to_activation;
        }
        
        auto node_from = semantic_graph.getNode(from_id);
        auto node_to = semantic_graph.getNode(to_id);
        
        if (node_from && node_to) {
            for (int g_from = 16; g_from <= 21; g_from++) {
                for (int g_to = 16; g_to <= 21; g_to++) {
                    float connection_strength = 0.0f;
                    for (int i = 0; i < 32; i++) {
                        float sig_from = node_from->signature[i + (g_from-16)*32];
                        float sig_to = node_to->signature[i + (g_to-16)*32];
                        connection_strength += sig_from * sig_to;
                    }
                    
                    if (connection_strength > 0.1f) {
                        neural_system.strengthenInterConnection(
                            g_from, g_to, reward * 0.02f * connection_strength);
                    }
                }
            }
        }
        
        applySTDPReward(reward);
        total_steps_ += 10;
    }
    
    void healStuckNeurons() {
        auto& groups = neural_system.getGroupsNonConst();
        static std::mt19937 rng(std::random_device{}());
        
        for (int g = 0; g < 32; g++) {
            for (int i = 0; i < 32; i++) {
                double r = groups[g].getPositions()[i].norm();
                if (r > OrbitalParams::OUTER_ORBIT * 1.8) {
                    std::cout << "Healing stuck neuron (" << g << "," << i 
                              << ") at r=" << r << std::endl;
                    groups[g].publicResetNeuron(i, rng);
                }
            }
        }
    }
    
    void preventStagnation() {
        auto& groups = neural_system.getGroupsNonConst();
        
        int singularity_count = 0;
        int total_neurons = 0;
        float total_mass = 0;
        
        std::vector<float> all_masses;
        for (int g = 16; g <= 21; g++) {
            for (int i = 0; i < 32; i++) {
                total_neurons++;
                if (groups[g].getOrbitLevel(i) == 0) {
                    singularity_count++;
                }
                float m = groups[g].getMass(i);
                total_mass += m;
                all_masses.push_back(m);
            }
        }
        
        float avg_mass = total_mass / total_neurons;
        float mass_variance = 0;
        for (float m : all_masses) {
            mass_variance += (m - avg_mass) * (m - avg_mass);
        }
        mass_variance /= total_neurons;
        
        bool mass_stagnant = mass_variance < 0.001f;
        bool too_many_singularities = singularity_count > total_neurons * 0.9f;
        
        if (mass_stagnant || too_many_singularities) {
            std::cout << "\n⚠️ SYSTEM STAGNATION DETECTED!" << std::endl;
            
            if (mass_stagnant) {
                std::cout << "   Activating diversity burst..." << std::endl;
                for (int g = 16; g <= 21; g++) {
                    groups[g].generateBurst();
                }
            }
            
            if (too_many_singularities) {
                std::cout << "   Resurrecting dead neurons..." << std::endl;
                static std::mt19937 rng(std::random_device{}());
                for (int g = 16; g <= 21; g++) {
                    for (int i = 0; i < 32; i++) {
                        if (groups[g].getOrbitLevel(i) == 0 && rng() % 100 < 10) {
                            groups[g].publicPromoteToBaseOrbit(i);
                            float new_mass = 0.8f + (rng() % 30) / 100.0f;
                            groups[g].setMass(i, new_mass);
                        }
                    }
                }
            }
            
            float entropy = neural_system.computeSystemEntropy();
            if (entropy < 1.5f) {
                std::cout << "   Low entropy (" << entropy << "), boosting exploration..." << std::endl;
                neural_system.maintainCriticality();
            }
        }
    }
    
    void specializeNeuronsByAbstraction() {
        auto& groups = neural_system.getGroupsNonConst();
        
        std::cout << "\n=== SPECIALIZING NEURONS BY ABSTRACTION ===\n";
        
        int specialized_count = 0;
        neuron_specialization.clear();
        concept_neurons.clear();
        
        for (int g = 16; g <= 21; g++) {
            for (int i = 0; i < 32; i++) {
                int orbit = groups[g].getOrbitLevel(i);
                
                if (orbit >= 3) {
                    uint32_t best_concept = findMostAbstractConceptForNeuron(g, i);
                    
                    if (best_concept > 0) {
                        auto key = std::make_pair(g, i);
                        neuron_specialization[key] = best_concept;
                        concept_neurons[best_concept].push_back({g, i});
                        specialized_count++;
                        
                        auto node = semantic_graph.getNode(best_concept);
                        if (node) {
                            std::cout << "  Neuron (" << g << "," << i << ") orbit " << orbit 
                                      << " specialized for '" << node->name 
                                      << "' (abstraction " << node->abstraction_level << ")\n";
                        }
                    }
                }
            }
        }
        
        std::cout << "  Specialized " << specialized_count << " elite neurons\n";
        
        for (const auto& [concept_id, neurons] : concept_neurons) {
            if (neurons.size() >= 2) {
                for (size_t a = 0; a < neurons.size(); a++) {
                    for (size_t b = a + 1; b < neurons.size(); b++) {
                        auto [g1, i1] = neurons[a];
                        auto [g2, i2] = neurons[b];
                        
                        if (g1 != g2) {
                            neural_system.strengthenInterConnection(g1, g2, 0.05f);
                        }
                    }
                }
            }
        }
    }
    
    uint32_t findMostAbstractConceptForNeuron(int group_idx, int neuron_idx) {
        float best_affinity = 0.3f;
        uint32_t best_concept = 0;
        
        auto abstract_concepts = semantic_graph.getNodesByAbstraction(6, 10);
        
        for (uint32_t concept_id : abstract_concepts) {
            float affinity = mastery_evaluator.getNeuronAffinityToConcept(
                group_idx, neuron_idx, concept_id);
            float activation = mastery_evaluator.measureActivationForConcept(concept_id);
            
            float combined = affinity * (0.7f + 0.3f * activation);
            
            if (combined > best_affinity) {
                best_affinity = combined;
                best_concept = concept_id;
            }
        }
        
        return best_concept;
    }
    
    void logOrbitalDistribution() {
        auto& groups = neural_system.getGroups();
        
        std::cout << "\nORBITAL DISTRIBUTION BY GROUP:\n";
        for (int g = 16; g <= 21; g++) {
            std::vector<int> counts(5, 0);
            for (int i = 0; i < 32; i++) {
                counts[groups[g].getOrbitLevel(i)]++;
            }
            std::cout << "  Group " << g << ": ";
            for (int lvl = 0; lvl < 5; lvl++) {
                std::cout << "L" << lvl << ":" << counts[lvl] << " ";
            }
            std::cout << std::endl;
        }
        
        std::vector<int> total(5, 0);
        for (int g = 16; g <= 21; g++) {
            for (int i = 0; i < 32; i++) {
                total[groups[g].getOrbitLevel(i)]++;
            }
        }
        
        std::cout << "  TOTAL: ";
        for (int lvl = 0; lvl < 5; lvl++) {
            std::cout << "L" << lvl << ":" << total[lvl] << " ";
        }
        std::cout << std::endl;
    }
    
    // Управление обучением
    void startTraining() { training_active_ = true; }
    void stopTraining() { training_active_ = false; }
    bool isTrainingActive() const { return training_active_; }
    
    void incrementSteps() { total_steps_++; }
    int getTotalSteps() const { return total_steps_; }
    
    void setLearningRate(float rate) { current_learning_rate_ = rate; }
    float getLearningRate() const { return current_learning_rate_; }
    
    void recordAccuracy(float accuracy) {
        accuracy_history_.push_back(accuracy);
        if (accuracy_history_.size() > 100) {
            accuracy_history_.erase(accuracy_history_.begin());
        }
    }
    
    float getAverageAccuracy() const {
        if (accuracy_history_.empty()) return 0.0f;
        float sum = 0.0f;
        for (float acc : accuracy_history_) sum += acc;
        return sum / accuracy_history_.size();
    }
    
    bool isProcessing() const { return is_processing_; }
    void setProcessing(bool value) { is_processing_ = value; }
    
    /**
     * @brief Обучение на одном примере
     * @param example Пример для обучения
     * @param example_manager Ссылка на менеджер примеров (для обновления статистики)
     * @param use_curriculum Использовать ли учебную программу (адаптация сложности)
     * @return true если обучение прошло успешно
     */
    bool trainOnExample(const MeaningTrainingExample& example, 
                        TrainingExampleManager* example_manager = nullptr,
                        bool use_curriculum = true) {
        
        // ЗАЩИТА ОТ РЕКУРСИИ
        static std::atomic<bool> processing_lock{false};
        if (processing_lock.exchange(true)) {
            return false;
        }
        
        if (is_processing_.exchange(true)) {
            std::cout << "  Recursive call prevented" << std::endl;
            processing_lock = false;
            return false;
        }

        bool success = false;
        
        try {
            if (!training_active_) {
                is_processing_ = false;
                processing_lock = false;
                return false;
            }
            
            if (example.input_words.empty() || example.expected_meanings.empty()) {
                is_processing_ = false;
                processing_lock = false;
                return false;
            }
            
            auto& groups = neural_system.getGroupsNonConst();

            // ===== 0. ПРОВЕРКА НА NAN =====
            bool has_nan = false;
            for (size_t g = 0; g < groups.size(); g++) {
                const auto& phi = groups[g].getPhi();
                for (size_t i = 0; i < phi.size(); i++) {
                    if (!std::isfinite(phi[i])) {
                        has_nan = true;
                        break;
                    }
                }
            }

            if (has_nan) {
                std::cout << "⚠️ NaN detected in neural groups, resetting..." << std::endl;
                std::uniform_real_distribution<double> dist(0.3, 0.7);
                for (int g = 0; g < 32; g++) {
                    auto& phi = groups[g].getPhiNonConst();
                    for (int i = 0; i < 32; i++) {
                        phi[i] = dist(rng_);
                    }
                }
                is_processing_ = false;
                processing_lock = false;
                return false;
            }
            
            // ===== 1. ПРОВЕРЯЕМ, ГОТОВ ЛИ МОЗГ К ЭТОМУ ПРИМЕРУ (если use_curriculum) =====
            float brain_readiness = 1.0f;
            uint32_t primary_concept = example.expected_meanings.empty() ? 0 : example.expected_meanings[0];

            if (use_curriculum && primary_concept > 0) {
                // Проверяем, достаточно ли активированы базовые концепты
                for (uint32_t exp_id : example.expected_meanings) {
                    auto node = semantic_graph.getNode(exp_id);
                    if (!node) continue;
                    
                    // Если концепт слишком абстрактный, проверяем его родителей
                    if (node->abstraction_level > 5) {
                        float parent_activation = 0.0f;
                        int parent_count = 0;
                        
                        auto edges = semantic_graph.getEdgesFrom(exp_id);
                        for (const auto& edge : edges) {
                            if (edge.type == SemanticEdge::Type::IS_A || 
                                edge.type == SemanticEdge::Type::PART_OF) {
                                
                                parent_activation += mastery_evaluator.getConceptMastery(edge.to_id);
                                parent_count++;
                            }
                        }
                        
                        if (parent_count > 0) {
                            float avg_parent = parent_activation / parent_count;
                            
                            if (avg_parent < 0.3f) {
                                brain_readiness *= 0.5f;
                                std::cout << "Low parent mastery (" << avg_parent 
                                        << ") for abstract concept " << exp_id << std::endl;
                            }
                        }
                    }
                }
            }

            // ===== 2. ЕСЛИ МОЗГ НЕ ГОТОВ, ОТКЛАДЫВАЕМ ПРИМЕР =====
            if (use_curriculum && brain_readiness < 0.6f) {
                std::cout << "⏳ Deferring example - brain not ready (readiness: " 
                        << brain_readiness << ")" << std::endl;
                
                // Вознаграждаем за попытку, но слабо
                for (int g = 0; g < 32; g++) {
                    groups[g].learnSTDP(0.1f, total_steps_);
                }
                total_steps_++;
                
                if (example_manager) {
                    // Уменьшаем приоритет, но не убираем полностью
                    const_cast<MeaningTrainingExample&>(example).priority *= 0.8f;
                }
                
                is_processing_ = false;
                processing_lock = false;
                return false;
            }
            
            // ===== 3. ВЫБИРАЕМ СЛОВО ИЗ ПРИМЕРА =====
            std::string input_word = example.input_words[0]; // берём первое слово
            
            // ===== 4. СОЗДАЕМ ВХОДНОЙ СИГНАЛ С ШУМОМ =====
            std::vector<float> input_signal(32, 0.0f);
            uint32_t word_hash = std::hash<std::string>{}(input_word);
            int neuron_idx = word_hash % 32;
            
            // Основной сигнал + шум для исследования
            input_signal[neuron_idx] = 1.0f;
            
            // Добавляем случайный шум для исследования
            std::uniform_real_distribution<float> noise_dist(-0.1f, 0.1f);
            for (int i = 0; i < 32; i++) {
                if (i != neuron_idx) {
                    input_signal[i] += noise_dist(rng_);
                }
            }
            
            // Нормализуем
            float max_val = *std::max_element(input_signal.begin(), input_signal.end());
            if (max_val > 1.0f) {
                for (auto& v : input_signal) v /= max_val;
            }
            
            // ===== 5. СОХРАНЯЕМ СОСТОЯНИЕ ДО ОБУЧЕНИЯ =====
            std::vector<std::vector<double>> before_state;
            for (int g = 16; g <= 21; g++) {
                before_state.push_back(groups[g].getPhi());
            }
            
            // Подаем сигнал
            for (int i = 0; i < 32; i++) {
                groups[0].getPhiNonConst()[i] = input_signal[i];
            }
            
            // Динамическое время мышления
            int think_steps = 4;
            for (int think_step = 0; think_step < think_steps; think_step++) {
                neural_system.step(0.0f, total_steps_ + think_step);
            }
            
            // ===== 6. АНАЛИЗИРУЕМ РЕЗУЛЬТАТ =====
            float activation_score = 0.0f;
            
            for (uint32_t exp_id : example.expected_meanings) {
                int group_idx = 16 + getGroupForConcept(exp_id);
                if (group_idx <= 21) {
                    float group_activation = 0.0f;
                    for (int i = 0; i < 32; i++) {
                        group_activation += groups[group_idx].getPhi()[i];
                    }
                    activation_score += group_activation;
                }
            }
            
            // Сравниваем с состоянием до обучения
            float improvement = 0.0f;
            for (size_t g = 0; g < before_state.size(); g++) {
                float before = 0.0f, after = 0.0f;
                for (int i = 0; i < 32; i++) {
                    before += before_state[g][i];
                    after += groups[16 + g].getPhi()[i];
                }
                improvement += (after - before);
            }
            
            // ===== 7. ВЫЧИСЛЯЕМ НАГРАДУ =====
            float reward = 0.0f;
            success = false;

            // Порог успеха зависит от сложности концепта
            float success_threshold = 5.3f;

            // Если система в стазисе, снижаем порог
            if (neural_system.computeSystemEntropy() < 1.0f) {
                success_threshold = 12.0f;
                std::cout << "Lowering success threshold due to low entropy" << std::endl;
            }
            
            if (activation_score > success_threshold) {
                success = true;
                reward = 5.0f;
                
                std::cout << "✓ SUCCESS! Word: '" << input_word 
                        << "', score: " << activation_score << std::endl;
            }
            else {
                reward = 0.1f;
                
                // Не выводим для каждого слова, только статистика
                static int fail_counter = 0;
                if (++fail_counter % 10 == 0) {
                    std::cout << "Learning... current score: " << activation_score 
                            << "/" << success_threshold << std::endl;
                }
            }
            
            // ===== 8. ПРИМЕНЯЕМ НАГРАДУ =====
            for (int g = 16; g <= 21; g++) {
                groups[g].learnSTDP(reward, total_steps_);
            }
            
            total_steps_++;
            
            // ===== 9. ОБНОВЛЯЕМ ПРИОРИТЕТ ПРИМЕРА (если есть менеджер) =====
            if (example_manager) {
                if (success) {
                    // Успешные примеры - уменьшаем приоритет
                    const_cast<MeaningTrainingExample&>(example).priority *= 0.7f;
                } else {
                    // Неудачные - увеличиваем приоритет
                    const_cast<MeaningTrainingExample&>(example).priority *= 1.3f;
                }
                
                // Ограничиваем приоритет
                const_cast<MeaningTrainingExample&>(example).priority = 
                    std::clamp(example.priority, 0.1f, 2.0f);
            }
            
            // ===== 10. СОХРАНЯЕМ СТАТИСТИКУ =====
            float accuracy = success ? 100.0f : (improvement > 1.0f ? 50.0f : 0.0f);
            recordAccuracy(accuracy);
            
            // Периодический вывод с информацией о прогрессе
            if (total_steps_ % 50 == 0) {
                float avg_acc = getAverageAccuracy();
                std::cout << "Step " << total_steps_ << ": Avg accuracy = " 
                        << std::fixed << std::setprecision(1) << avg_acc << "%" << std::endl;
            }

            // Проверяем, не зависла ли система
            static int last_success_step = 0;
            static int steps_without_success = 0;

            if (success) {
                last_success_step = total_steps_;
                steps_without_success = 0;
            } else {
                steps_without_success++;
            }
            
            // Каждые 500 шагов логируем орбитальное распределение
            if (total_steps_ % 500 == 0) {
                logOrbitalDistribution();
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Exception in trainOnExample: " << e.what() << std::endl;
        }

        is_processing_ = false;
        processing_lock = false;
        
        return success;
    }
};