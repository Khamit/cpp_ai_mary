#pragma once
// EmergentCore.hpp
// The central missing piece: a gating system that decides what to remember,
// what to discard, and how to route reward signals based on prediction error.
//
// Architecture:
//   STM (Short-Term Memory)  — recent activations, high plasticity, decays fast
//   LTM (Long-Term Memory)   — consolidated patterns, low plasticity, decays slow
//   Gate                     — prediction-error-driven consolidation decision
//   Self-evaluator           — compares current output to stored "good" patterns
//
// The emergent loop (called once per step):
//   1. Compute prediction error against STM
//   2. If error HIGH  → explore (increase temperature, weaken consolidation)
//   3. If error LOW   → consolidate (move STM→LTM, prune LTM redundancies)
//   4. Discard LTM entries whose activation has been zero for N steps
//   5. Emit a per-group reward signal (not a flat global reward)

#include <vector>
#include <deque>
#include <unordered_map>
#include <string>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <cassert>

// ──────────────────────────────────────────────────────────────────────────────
// MemoryRecord — one item that can live in STM or LTM
// ──────────────────────────────────────────────────────────────────────────────
struct MemoryRecord {
    std::vector<float> pattern;   // feature vector (e.g. group averages, 32 floats)
    float importance  = 0.5f;     // learned importance [0,1]
    float decay_rate  = 0.01f;    // how fast importance falls per step without reactivation
    int   age         = 0;        // steps since creation
    int   last_hit    = 0;        // step at which it was last reactivated
    float entropy     = 0.5f;     // pattern entropy at time of storage
    std::string tag;              // semantic label (optional)

    // Similarity in [0,1] — cosine similarity
    float similarity(const std::vector<float>& other) const {
        if (pattern.size() != other.size()) return 0.f;
        float dot = 0.f, na = 0.f, nb = 0.f;
        for (size_t i = 0; i < pattern.size(); ++i) {
            dot += pattern[i] * other[i];
            na  += pattern[i] * pattern[i];
            nb  += other[i]   * other[i];
        }
        float denom = std::sqrt(na) * std::sqrt(nb);
        return (denom < 1e-9f) ? 0.f : std::clamp(dot / denom, 0.f, 1.f);
    }

    // Relevance = importance × recency × pattern match
    float relevance(const std::vector<float>& query, int current_step) const {
        float recency = std::exp(-decay_rate * (current_step - last_hit));
        return importance * recency * similarity(query);
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// EmergentMemory — STM + LTM with gated consolidation
// ──────────────────────────────────────────────────────────────────────────────
class EmergentMemory {
public:
    // Tunable knobs
    struct Config {
        size_t stm_capacity    = 64;     // max items in STM
        size_t ltm_capacity    = 512;    // max items in LTM
        float  consolidation_threshold = 0.6f; // min importance to move STM→LTM
        float  discard_threshold       = 0.05f; // LTM items below this are pruned
        float  similarity_merge        = 0.92f; // merge LTM items above this similarity
        int    min_age_for_ltm         = 20;    // STM item must be at least this old
        float  stm_decay               = 0.05f; // importance decay per step in STM
        float  ltm_decay               = 0.002f;// importance decay per step in LTM
    };

    Config cfg;

    EmergentMemory() = default;
    explicit EmergentMemory(Config c) : cfg(std::move(c)) {}

    // ── Write to STM ──────────────────────────────────────────────────────────
    void writeSTM(const std::vector<float>& pattern,
                  float importance,
                  float entropy,
                  const std::string& tag = "",
                  int step = 0) {
        // Check if an existing STM item is very similar → just update it
        for (auto& r : stm_) {
            if (r.similarity(pattern) > cfg.similarity_merge) {
                r.importance = std::clamp(r.importance + 0.1f, 0.f, 1.f);
                r.last_hit   = step;
                r.entropy    = entropy;
                return;
            }
        }
        MemoryRecord rec;
        rec.pattern    = pattern;
        rec.importance = importance;
        rec.decay_rate = cfg.stm_decay;
        rec.entropy    = entropy;
        rec.tag        = tag;
        rec.age        = 0;
        rec.last_hit   = step;
        stm_.push_back(std::move(rec));
        enforceSTMCapacity();
    }

    // ── Query: returns top-k relevant records from STM+LTM ───────────────────
    std::vector<const MemoryRecord*> query(const std::vector<float>& pattern,
                                           int top_k,
                                           int current_step) const {
        std::vector<std::pair<float, const MemoryRecord*>> scored;
        for (const auto& r : stm_)
            scored.push_back({r.relevance(pattern, current_step), &r});
        for (const auto& r : ltm_)
            scored.push_back({r.relevance(pattern, current_step), &r});
        std::sort(scored.begin(), scored.end(),
                  [](const auto& a, const auto& b){ return a.first > b.first; });
        std::vector<const MemoryRecord*> out;
        for (int i = 0; i < top_k && i < (int)scored.size(); ++i)
            out.push_back(scored[i].second);
        return out;
    }

    // ── Step: decay, consolidate, prune ──────────────────────────────────────
    // Returns number of items consolidated, number discarded
    std::pair<int,int> step(int current_step) {
        int consolidated = 0, discarded = 0;

        // 1. Decay STM importance
        for (auto& r : stm_) {
            r.age++;
            r.importance *= (1.f - cfg.stm_decay);
        }

        // 2. Attempt consolidation: STM → LTM
        std::deque<MemoryRecord> remaining_stm;  // ← changed from vector to deque
        for (auto& r : stm_) {
            bool old_enough  = (r.age >= cfg.min_age_for_ltm);
            bool important   = (r.importance >= cfg.consolidation_threshold);
            if (old_enough && important) {
                r.decay_rate = cfg.ltm_decay;
                consolidateToLTM(std::move(r), current_step);
                ++consolidated;
            } else if (r.importance > cfg.discard_threshold) {
                remaining_stm.push_back(std::move(r));
            } else {
                ++discarded;
            }
        }
        stm_ = std::move(remaining_stm); 
        
        // 3. Decay LTM importance
        for (auto& r : ltm_) {
            r.age++;
            r.importance *= (1.f - cfg.ltm_decay);
        }

        // 4. Prune LTM items that have decayed below threshold
        size_t before = ltm_.size();
        ltm_.erase(std::remove_if(ltm_.begin(), ltm_.end(),
            [&](const MemoryRecord& r){
                return r.importance < cfg.discard_threshold;
            }), ltm_.end());
        discarded += (int)(before - ltm_.size());

        // 5. Enforce LTM capacity — keep highest importance
        enforceLTMCapacity();

        return {consolidated, discarded};
    }

    // ── Reinforce: boost importance of items similar to pattern ──────────────
    void reinforce(const std::vector<float>& pattern, float boost, int step) {
        for (auto& r : stm_) {
            float sim = r.similarity(pattern);
            if (sim > 0.5f) {
                r.importance = std::clamp(r.importance + boost * sim, 0.f, 1.f);
                r.last_hit = step;
            }
        }
        for (auto& r : ltm_) {
            float sim = r.similarity(pattern);
            if (sim > 0.5f) {
                r.importance = std::clamp(r.importance + boost * sim * 0.5f, 0.f, 1.f);
                r.last_hit = step;
            }
        }
    }

    // ── Weaken: reduce importance of items similar to pattern ────────────────
    void weaken(const std::vector<float>& pattern, float penalty) {
        for (auto& r : ltm_) {
            float sim = r.similarity(pattern);
            if (sim > 0.7f)
                r.importance = std::clamp(r.importance - penalty * sim, 0.f, 1.f);
        }
    }

    // ── Stats ─────────────────────────────────────────────────────────────────
    size_t stmSize() const { return stm_.size(); }
    size_t ltmSize() const { return ltm_.size(); }

    float averageSTMImportance() const { return averageImportance(stm_); }
    float averageLTMImportance() const { return averageImportance(ltm_); }

    const std::deque<MemoryRecord>& getLTM() const { return ltm_; }
    const std::deque<MemoryRecord>& getSTM() const { return stm_; }

private:
    std::deque<MemoryRecord> stm_;  // fast, lossy
    std::deque<MemoryRecord> ltm_;  // slow, persistent

    void enforceSTMCapacity() {
        while (stm_.size() > cfg.stm_capacity) {
            // Drop the least important
            auto it = std::min_element(stm_.begin(), stm_.end(),
                [](const MemoryRecord& a, const MemoryRecord& b){
                    return a.importance < b.importance;
                });
            stm_.erase(it);
        }
    }

    void enforceLTMCapacity() {
        while (ltm_.size() > cfg.ltm_capacity) {
            auto it = std::min_element(ltm_.begin(), ltm_.end(),
                [](const MemoryRecord& a, const MemoryRecord& b){
                    return a.importance < b.importance;
                });
            ltm_.erase(it);
        }
    }

    void consolidateToLTM(MemoryRecord rec, int step) {
        // Check for mergeable item already in LTM
        for (auto& r : ltm_) {
            if (r.similarity(rec.pattern) > cfg.similarity_merge) {
                // Merge: weighted average pattern, boost importance
                float w1 = r.importance, w2 = rec.importance;
                float total = w1 + w2 + 1e-9f;
                for (size_t i = 0; i < r.pattern.size(); ++i)
                    r.pattern[i] = (r.pattern[i] * w1 + rec.pattern[i] * w2) / total;
                r.importance = std::clamp(r.importance + rec.importance * 0.3f, 0.f, 1.f);
                r.last_hit   = step;
                return;
            }
        }
        rec.last_hit = step;
        ltm_.push_back(std::move(rec));
    }

    float averageImportance(const std::deque<MemoryRecord>& pool) const {
        if (pool.empty()) return 0.f;
        float sum = 0.f;
        for (const auto& r : pool) sum += r.importance;
        return sum / pool.size();
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// PredictionUnit — online linear predictor per group
// Predicts next group average from current, computes error,
// returns a per-group reward signal.
// ──────────────────────────────────────────────────────────────────────────────
class PredictionUnit {
public:
    static constexpr int N = 32; // NUM_GROUPS

    PredictionUnit() {
        weights_.assign(N * N, 0.f);
        bias_.assign(N, 0.f);
        prev_state_.assign(N, 0.5f);
    }

    // Returns per-group reward in [0,1]: 1 = perfect prediction, 0 = total surprise
    std::vector<float> step(const std::vector<float>& current_state) {
        assert((int)current_state.size() == N);

        // Predict from prev_state_
        std::vector<float> predicted(N, 0.f);
        for (int j = 0; j < N; ++j) {
            float val = bias_[j];
            for (int i = 0; i < N; ++i)
                val += weights_[j * N + i] * prev_state_[i];
            predicted[j] = std::tanh(val);
        }

        // Per-group error
        std::vector<float> error(N);
        float total_error = 0.f;
        for (int j = 0; j < N; ++j) {
            float e = current_state[j] - predicted[j];
            error[j] = std::abs(e);
            total_error += error[j];
        }
        last_total_error_ = total_error / N;

        // Online Hebbian update (δ-rule) — only if we have a prev state
        if (step_count_ > 0) {
            float lr = 0.01f;
            for (int j = 0; j < N; ++j) {
                float delta = current_state[j] - predicted[j];
                bias_[j] += lr * delta;
                for (int i = 0; i < N; ++i)
                    weights_[j * N + i] += lr * delta * prev_state_[i];
            }
        }

        prev_state_ = current_state;
        ++step_count_;

        // Convert error to reward: high error → low reward
        std::vector<float> reward(N);
        for (int j = 0; j < N; ++j)
            reward[j] = std::exp(-error[j] * 5.f); // sharp sigmoid, error > 0.2 → reward ~0
        return reward;
    }

    float getLastError() const { return last_total_error_; }

    // Global surprise: [0,1], 1 = complete surprise (high error)
    float getSurprise() const { return std::tanh(last_total_error_ * 3.f); }

private:
    std::vector<float> weights_;
    std::vector<float> bias_;
    std::vector<float> prev_state_;
    float last_total_error_ = 0.f;
    int   step_count_ = 0;
};

// ──────────────────────────────────────────────────────────────────────────────
// SelfEvaluator — compares current output to the best known outputs
// Maintains a "hall of fame" of high-quality response patterns.
// Returns a quality score [0,1] for the current state.
// ──────────────────────────────────────────────────────────────────────────────
class SelfEvaluator {
public:
    struct GoodPattern {
        std::vector<float> state;
        float score;
        int step;
    };

    static constexpr int HALL_SIZE = 32;

    // Call after each step with the current group averages and an external reward
    float evaluate(const std::vector<float>& state, float external_reward, int step) {
        float internal_score = computeInternalScore(state);
        float combined = 0.7f * external_reward + 0.3f * internal_score;

        // Update hall of fame
        if (combined > 0.6f) {
            hall_.push_back({state, combined, step});
            if ((int)hall_.size() > HALL_SIZE) {
                // Remove worst
                auto it = std::min_element(hall_.begin(), hall_.end(),
                    [](const GoodPattern& a, const GoodPattern& b){ return a.score < b.score; });
                hall_.erase(it);
            }
        }

        // Trend: is the system improving?
        score_history_.push_back(combined);
        if (score_history_.size() > 200) score_history_.pop_front();

        return combined;
    }

    // Are we improving over the last window?
    float improvementTrend(int window = 50) const {
        if ((int)score_history_.size() < window * 2) return 0.f;
        float recent = 0.f, old = 0.f;
        int n = (int)score_history_.size();
        for (int i = n - window; i < n; ++i) recent += score_history_[i];
        for (int i = n - window * 2; i < n - window; ++i) old += score_history_[i];
        return (recent - old) / window;
    }

    float bestScore() const {
        if (hall_.empty()) return 0.f;
        return std::max_element(hall_.begin(), hall_.end(),
            [](const GoodPattern& a, const GoodPattern& b){ return a.score < b.score; })->score;
    }

    const std::vector<GoodPattern>& getHall() const { return hall_; }

private:
    std::vector<GoodPattern> hall_;
    std::deque<float> score_history_;

    float computeInternalScore(const std::vector<float>& state) const {
        if (hall_.empty()) return 0.5f;

        // Best match to hall of fame
        float best_sim = 0.f;
        for (const auto& g : hall_) {
            float dot = 0.f, na = 0.f, nb = 0.f;
            for (size_t i = 0; i < std::min(state.size(), g.state.size()); ++i) {
                dot += state[i] * g.state[i];
                na  += state[i] * state[i];
                nb  += g.state[i] * g.state[i];
            }
            float sim = (na > 0 && nb > 0) ? dot / (std::sqrt(na) * std::sqrt(nb)) : 0.f;
            best_sim = std::max(best_sim, sim * g.score);
        }
        return std::clamp(best_sim, 0.f, 1.f);
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// EmergentController — ties everything together.
// Call tick() once per simulation step.
// It returns an EmergentSignal that NeuralFieldSystem uses
// instead of a flat globalReward.
// ──────────────────────────────────────────────────────────────────────────────
struct EmergentSignal {
    std::vector<float> per_group_reward; // size NUM_GROUPS
    float   surprise;           // [0,1] how unexpected the current state is
    float   quality;            // [0,1] self-evaluated output quality
    float   temperature_delta;  // suggest how much to change attention temperature
    float   consolidation_pressure; // [0,1] how urgently to consolidate STM→LTM
    bool    should_explore;     // true = high surprise → exploration mode
    bool    should_consolidate; // true = low surprise, good quality → consolidate
    int     ltm_size;
    int     stm_size;
    float   improvement_trend;
};

class EmergentController {
public:
    EmergentMemory  memory;
    PredictionUnit  predictor;
    SelfEvaluator   evaluator;

    struct Config {
        float surprise_explore_threshold   = 0.4f;  // above: explore
        float surprise_consolidate_threshold = 0.15f;// below: consolidate
        float quality_consolidate_threshold  = 0.5f; // min quality to consolidate
        float temperature_explore_boost    = 0.05f;
        float temperature_exploit_decay    = 0.03f;
    } cfg;

    EmergentController() {
        EmergentMemory::Config mc;
        mc.stm_capacity    = 128;
        mc.ltm_capacity    = 1024;
        mc.consolidation_threshold = 0.55f;
        mc.discard_threshold       = 0.04f;
        memory = EmergentMemory(mc);
    }

    // group_averages: current activity of each of the 32 groups
    // external_reward: signal from outside (0 if unsupervised)
    // step: global step counter
    EmergentSignal tick(const std::vector<float>& group_averages,
                        float external_reward,
                        int   step) {
        EmergentSignal sig;

        // 1. Predict → per-group reward
        sig.per_group_reward = predictor.step(group_averages);
        sig.surprise         = predictor.getSurprise();

        // 2. Write current state to STM
        float entropy = computeEntropy(group_averages);
        memory.writeSTM(group_averages,
                        /*importance=*/ 0.3f + 0.5f * external_reward + 0.2f * (1.f - sig.surprise),
                        entropy,
                        "state",
                        step);

        // 3. Self-evaluate
        // Blend external reward with prediction-based reward
        float blended_reward = std::max(external_reward,
                                        1.f - sig.surprise); // low surprise = good
        sig.quality = evaluator.evaluate(group_averages, blended_reward, step);

        // 4. Gate decision
        sig.should_explore    = (sig.surprise > cfg.surprise_explore_threshold);
        sig.should_consolidate= (sig.surprise < cfg.surprise_consolidate_threshold &&
                                 sig.quality   > cfg.quality_consolidate_threshold);

        // 5. Temperature suggestion
        if (sig.should_explore)
            sig.temperature_delta = +cfg.temperature_explore_boost;
        else if (sig.should_consolidate)
            sig.temperature_delta = -cfg.temperature_exploit_decay;
        else
            sig.temperature_delta = 0.f;

        // 6. Consolidation pressure
        sig.consolidation_pressure = sig.should_consolidate ? sig.quality : 0.f;

        // 7. Memory step (decay, consolidate, prune)
        auto [cons, disc] = memory.step(step);
        if ((cons > 0 || disc > 0) && step % 500 == 0) {
            std::cout << "[Emergent] step=" << step
                      << " STM=" << memory.stmSize()
                      << " LTM=" << memory.ltmSize()
                      << " consolidated=" << cons
                      << " discarded=" << disc
                      << " surprise=" << sig.surprise
                      << " quality=" << sig.quality
                      << std::endl;
        }

        // 8. Reinforce memory with current state if quality is good
        if (sig.quality > 0.6f)
            memory.reinforce(group_averages, 0.1f * sig.quality, step);

        // 9. Stats
        sig.ltm_size  = (int)memory.ltmSize();
        sig.stm_size  = (int)memory.stmSize();
        sig.improvement_trend = evaluator.improvementTrend();

        return sig;
    }

    // Query LTM for context relevant to current state
    std::vector<const MemoryRecord*> queryContext(const std::vector<float>& state,
                                                  int top_k, int step) const {
        return memory.query(state, top_k, step);
    }

private:
    static float computeEntropy(const std::vector<float>& v) {
        float H = 0.f, total = 0.f;
        for (float x : v) total += x;
        if (total < 1e-9f) return 0.f;
        for (float x : v) {
            float p = x / total;
            if (p > 1e-9f) H -= p * std::log2(p);
        }
        return std::clamp(H / std::log2((float)v.size()), 0.f, 1.f);
    }
};