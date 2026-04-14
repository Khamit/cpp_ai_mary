// NeuralFieldSystem.hpp — rewritten to integrate EmergentCore
#pragma once

#include "NeuralGroup.hpp"
#include "EmergentCore.hpp"
#include <vector>
#include <random>
#include <string>
#include <deque>
#include <algorithm>
#include <cmath>
#include "EventSystem.hpp"
#include "core/INeuralGroupAccess.hpp"
#include "DynamicParams.hpp"
#include "core/EmergentCore.hpp"
#include <mutex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "PredictiveCoder.hpp"
#include "SelfSignalSampler.hpp"
#include "GoalGenerator.hpp"

// ──────────────────────────────────────────────────────────────────────────────
// AttentionMechanism
// ──────────────────────────────────────────────────────────────────────────────
struct AttentionMechanism {
    std::vector<double> attention_weights;
    std::vector<float>  context_vector;
    float  temperature = 1.0f;
    double entropy     = 0.0;

    void computeAttention(const std::vector<double>& groupActivities) {
        const size_t n = groupActivities.size();
        attention_weights.resize(n);
        if (n == 0) return;

        float maxVal = static_cast<float>(
            *std::max_element(groupActivities.begin(), groupActivities.end()));
        const float temp = std::max(temperature, 1e-6f);

        float sum = 0.f;
        for (size_t i = 0; i < n; ++i) {
            float v = std::exp((static_cast<float>(groupActivities[i]) - maxVal) / temp);
            attention_weights[i] = v;
            sum += v;
        }
        if (sum < 1e-12f) {
            float u = 1.f / n;
            for (auto& w : attention_weights) w = u;
            entropy = std::log(static_cast<double>(n));
            return;
        }
        float inv = 1.f / sum;
        entropy = 0.0;
        for (auto& w : attention_weights) {
            w *= inv;
            if (w > 1e-12) entropy -= w * std::log(static_cast<double>(w));
        }
    }

    std::vector<double> computeSphericalAttention(const std::vector<double>& g) {
        const size_t n = g.size();
        std::vector<double> res(n, 0.0);
        double norm = 0.0;
        for (double a : g) norm += a * a;
        if (norm < 1e-12) return res;
        norm = std::sqrt(norm);
        double area = 2.0 * std::pow(M_PI, n / 2.0) / std::tgamma(n / 2.0 + 1);
        for (size_t i = 0; i < n; ++i) res[i] = (g[i] / norm) / area;
        return res;
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// NeuralFieldSystem
// ──────────────────────────────────────────────────────────────────────────────
class NeuralFieldSystem : public INeuralGroupAccess {
public:
    static constexpr int NUM_GROUPS   = 32;
    static constexpr int GROUP_SIZE   = 32;
    static constexpr int TOTAL_NEURONS = NUM_GROUPS * GROUP_SIZE;

    NeuralFieldSystem(double dt, EventSystem& events);

    void initializeRandom(std::mt19937& rng) { initializeWithLimits(rng, MassLimits()); }
    void initializeWithLimits(std::mt19937& rng, const MassLimits& limits);

    NeuralFieldSystem(const NeuralFieldSystem&) = delete;
    NeuralFieldSystem& operator=(const NeuralFieldSystem&) = delete;
    
    const GoalSignal& lastGoal() const { return last_goal_; }

    void step(float external_reward, int stepNumber);
    int getCurrentStep() const { return stepCounter; }

    const std::vector<double>& getPhi() const { rebuildFlatVectors(); return flatPhi; }
    const std::vector<double>& getPi()  const { rebuildFlatVectors(); return flatPi; }
    const std::vector<std::vector<double>> getWeights() const { return {}; }
    std::vector<double> getGroupAverages() const;
    const std::vector<std::vector<double>>& getInterWeights() const { return interWeights; }

    double computeTotalEnergy()  const;
    double computeSystemEntropy() const;
    double getUnifiedEntropy()   const;
    double getTargetUnifiedEntropy() const;

    const EmergentSignal& lastSignal() const { return lastSignal_; }
    const EmergentController& emergent() const { return emergent_; }
    EmergentController& emergentMutable() { return emergent_; }

    void strengthenInterConnection(int from, int to, double delta);
    void weakenInterConnection(int from, int to, double delta);

    std::vector<float> getFeatures() const;

    std::vector<NeuralGroup>& getGroupsNonConst() { return groups; }
    const std::vector<NeuralGroup>& getGroups()   const { return groups; }

    void setOperatingMode(OperatingMode::Type mode);
    bool isTrainingMode() const { return training_mode_; }
    void setTrainingMode(bool e) { training_mode_ = e; }

    float  getAttentionTemperature() const { return attention.temperature; }
    void   setAttentionTemperature(float t) { attention.temperature = t; }
    double getAttentionEntropy() const { return attention.entropy; }
    const std::vector<double>& getAttentionWeights() const { return attention.attention_weights; }

    void setMemoryManager(EmergentMemory* mm) { memory_manager = mm; }

    void applyResourcePenalty(float penalty);
    void enterLowPowerMode();
    void applyTargetPattern(const std::vector<float>& target);
    void applyTargetedMutation(double strength, int targetType);
    void applyLateralInhibition();
    void maintainCriticality();
    void diagnoseCriticality();
    double computeOptimalStructure();
    void logOrbitalHealth();

    struct ReflectionState {
        double confidence, curiosity, satisfaction, confusion;
        std::vector<double> attention_map;
    };
    ReflectionState getReflectionState() const;
    void reflect();
    void setGoal(const std::string& goal);
    bool evaluateProgress();
    void learnFromReflection(float outcome);

    std::vector<NeuralGroup*>& getHubGroups() override {
        static std::vector<NeuralGroup*> ptrs;
        ptrs.clear();
        for (int idx : hubIndices)
            if (idx >= 0 && idx < (int)groups.size()) ptrs.push_back(&groups[idx]);
        return ptrs;
    }
    const std::vector<NeuralGroup*>& getHubGroups() const override {
        static std::vector<NeuralGroup*> ptrs;
        ptrs.clear();
        for (int idx : hubIndices)
            if (idx >= 0 && idx < (int)groups.size())
                ptrs.push_back(const_cast<NeuralGroup*>(&groups[idx]));
        return ptrs;
    }
    void registerHub(int g) override {
        if (g >= 0 && g < (int)groups.size()) hubIndices.push_back(g);
    }

    void initializePredictiveCoder(EmergentMemory& memory) {
        predictive_coder = std::make_unique<PredictiveCoder>(*this, memory);
    }

    void setInputText(const std::vector<float>& sig) {
        auto& g = groups[0].getPhiNonConst();
        for (int i = 0; i < 32; ++i) g[i] = sig[i];
        for (int t = 16; t <= 21; ++t) strengthenInterConnection(0, t, 0.1);
    }
    void addExternalInput(const std::vector<float>& in) { external_inputs_ = in; }
    void clearExternalInputs() { external_inputs_.clear(); }

    void lock()   { system_mutex_.lock(); }
    void unlock() { system_mutex_.unlock(); }
    std::mutex& getMutex() { return system_mutex_; }

    class ScopedLock {
        NeuralFieldSystem& s_;
    public:
        ScopedLock(NeuralFieldSystem& s) : s_(s) { s_.lock(); }
        ~ScopedLock() { s_.unlock(); }
    };

private:
    EmergentController emergent_;
    EmergentSignal     lastSignal_;

    AttentionMechanism attention;
    EventSystem&       events;
    OperatingMode::Type current_mode_ = OperatingMode::NORMAL;
    EmergentMemory*     memory_manager = nullptr;

    std::unique_ptr<PredictiveCoder> predictive_coder;
    
    // Self-model fields
    SelfSignalSampler self_sampler_;
    GoalGenerator     goal_gen_;
    GoalSignal        last_goal_{};
    
    mutable std::mutex system_mutex_;

    double dt;
    std::vector<NeuralGroup>               groups;
    std::vector<std::vector<double>>       interWeights;

    mutable std::vector<double> flatPhi, flatPi;
    mutable bool flatDirty = true;
    void rebuildFlatVectors() const;

    int  stepCounter        = 0;
    bool training_mode_     = false;
    bool pendingEvolution_  = false;
    std::vector<int>  hubIndices;
    std::vector<float> external_inputs_;
    std::string current_goal;

    void applyReentry(int iterations);
    void applyRicciFlow();
    void consolidateInterWeights(float pressure);
    void applyPruningByElevation();
    float computeGlobalImportance();

    void routeRewards(const EmergentSignal& sig, const GoalSignal& goal, int step);

    std::deque<double> entropy_history;
    static constexpr int HISTORY_SIZE = 200;
};