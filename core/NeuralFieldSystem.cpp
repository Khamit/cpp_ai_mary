// NeuralFieldSystem.cpp — emergent rewrite
// Replace the old file entirely.  Only the step() function and helpers changed.
// Everything else (geometry, initialisation, etc.) is preserved from original.

#include "NeuralFieldSystem.hpp"
#include "DynamicParams.hpp"
#include <cmath>
#include <iostream>
#include <numeric>
#include <algorithm>
#include <iomanip>

// ── Intervals that are now *adaptive* (used as minimums) ─────────────────────
static constexpr int MIN_CONSOLIDATION_INTERVAL = 20;   // can happen sooner
static constexpr int EVOLUTION_INTERVAL          = 5000;

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / init (unchanged from original)
// ─────────────────────────────────────────────────────────────────────────────

NeuralFieldSystem::NeuralFieldSystem(double dt, EventSystem& events)
    : dt(dt), events(events),
      groups(),
      interWeights(NUM_GROUPS, std::vector<double>(NUM_GROUPS, 0.0)),
      flatPhi(TOTAL_NEURONS, 0.0),
      flatPi(TOTAL_NEURONS, 0.0),
      flatDirty(true)
{}

void NeuralFieldSystem::initializeWithLimits(std::mt19937& rng, const MassLimits& limits) {
    groups.clear();
    groups.reserve(NUM_GROUPS);
    for (int g = 0; g < NUM_GROUPS; ++g) {
        groups.emplace_back(GROUP_SIZE, dt, rng, limits);
        if (g == 0) groups.back().setInputGroup(true);
    }

    // Mark groups 28-31 as self-model groups (so DynamicSemanticMemory doesn't touch them)
    for (int g = SelfSignalSampler::SELF_GROUP_START;
         g < SelfSignalSampler::SELF_GROUP_START + SelfSignalSampler::SELF_GROUP_COUNT;
         ++g) {
        if (g < (int)groups.size()) {
            groups[g].setSelfModelGroup(true);
        }
    }

    for (auto& group : groups)
        group.setMemoryManager(memory_manager);

    interWeights.assign(NUM_GROUPS, std::vector<double>(NUM_GROUPS, 0.0));

    // Bias input group → semantic groups
    for (int g = 16; g <= 21; ++g) {
        interWeights[0][g] = 2.0;
        interWeights[g][0] = 0.5;
    }

    std::uniform_real_distribution<double> dist(-0.01, 0.01);
    for (int i = 0; i < NUM_GROUPS; ++i)
        for (int j = 0; j < NUM_GROUPS; ++j)
            if (i != j) {
                double angle = 2.0 * M_PI * std::abs(i - j) / NUM_GROUPS;
                interWeights[i][j] = dist(rng) * (std::cos(angle) * 0.5 + 0.5);
            }

    flatDirty = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// THE NEW STEP — emergent, gated, prediction-driven
// ─────────────────────────────────────────────────────────────────────────────
void NeuralFieldSystem::step(float external_reward, int stepNumber) {
    stepCounter = stepNumber;

    // ═══════════════════════════════════════════════════════════════════════
    // PHASE 1: Neural physics — every step
    // ═══════════════════════════════════════════════════════════════════════
    for (auto& g : groups) {
        g.evolve();
        g.maintainActivity(current_mode_);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // PHASE 2: Emergent tick — compute surprise, quality, per-group rewards
    // ═══════════════════════════════════════════════════════════════════════
    {
        // Build float group averages for EmergentController
        auto avgs_d = getGroupAverages();
        std::vector<float> avgs_f(avgs_d.begin(), avgs_d.end());

        lastSignal_ = emergent_.tick(avgs_f, external_reward, stepNumber);
    }

    // Apply temperature suggestion from emergent controller
    {
        float new_temp = attention.temperature + lastSignal_.temperature_delta;
        attention.temperature = std::clamp(new_temp, 0.1f, 5.0f);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // PHASE 2b: Self-model — sample introspective signals, inject into groups 28-31
    // ═══════════════════════════════════════════════════════════════════════
    {
        auto snap = self_sampler_.sample(*this, lastSignal_, stepNumber);
        self_sampler_.inject(*this, snap);

        // PHASE 2c: Generate goal signal from self-model + emergent state
        last_goal_ = goal_gen_.generate(snap, lastSignal_, emergent_.memory, stepNumber);

        // Log dominant drive occasionally
        if (stepNumber % 500 == 0) {
            std::cout << "[Goal] drive=" << last_goal_.dominant_drive
                      << " boredom=" << last_goal_.boredom
                      << " urgency=" << last_goal_.urgency
                      << " satiation=" << last_goal_.satiation
                      << std::endl;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // PHASE 3: Geometry (Ricci flow) — adaptive frequency
    // More frequent when exploring (high surprise), less when consolidating
    // ═══════════════════════════════════════════════════════════════════════
    {
        int ricci_interval = lastSignal_.should_explore ? 5 : 50;
        if (stepNumber % ricci_interval == 0)
            applyRicciFlow();
    }

    // ═══════════════════════════════════════════════════════════════════════
    // PHASE 4: Reentry
    // ═══════════════════════════════════════════════════════════════════════
    {
        int reentry_iter = (current_mode_ == OperatingMode::TRAINING) ? 3 : 1;
        applyReentry(reentry_iter);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // PHASE 5: Reward routing — per-group STDP driven by prediction error
    // ═══════════════════════════════════════════════════════════════════════
    routeRewards(lastSignal_, last_goal_, stepNumber);

    // ═══════════════════════════════════════════════════════════════════════
    // PHASE 6: Predictive coder (existing module, feeds semantic groups)
    // ═══════════════════════════════════════════════════════════════════════
    if (predictive_coder) {
        float pred_error = predictive_coder->step(stepNumber);
        float semantic_reward = 1.f - std::tanh(pred_error);

        if (pred_error > 0.3f) {
            Event ev;
            ev.type   = EventType::ANOMALY_DETECTED;
            ev.source = "predictive_coder";
            ev.value  = pred_error;
            ev.step   = stepNumber;
            events.emit(ev);
        }

        // Only semantic groups get the coder's signal on top of emergent signal
        for (int g = 16; g <= 21; ++g)
            groups[g].learnSTDP(semantic_reward, stepNumber);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // PHASE 7: Gated consolidation
    // Triggered when EmergentController says "should_consolidate",
    // or at minimum interval as a fallback.
    // ═══════════════════════════════════════════════════════════════════════
    bool force_consolidate = (stepNumber % MIN_CONSOLIDATION_INTERVAL == 0);
    if (lastSignal_.should_consolidate || force_consolidate) {
        // Importance driven by EmergentController quality, not a fixed value
        float importance = lastSignal_.consolidation_pressure;
        if (importance < 0.1f && force_consolidate) importance = 0.2f;

        for (auto& g : groups) {
            g.consolidateEligibility(importance);
            g.consolidateElevation(importance);
        }
        consolidateInterWeights(importance);
        applyPruningByElevation();

        // Update entropy history for diagnostics
        entropy_history.push_back(computeSystemEntropy());
        if (entropy_history.size() > HISTORY_SIZE) entropy_history.pop_front();
    }

    // ═══════════════════════════════════════════════════════════════════════
    // PHASE 8: Lateral inhibition (always)
    // ═══════════════════════════════════════════════════════════════════════
    applyLateralInhibition();

    // ═══════════════════════════════════════════════════════════════════════
    // PHASE 9: Elevation update — driven by quality, not flat reward
    // ═══════════════════════════════════════════════════════════════════════
    for (int g = 0; g < NUM_GROUPS; ++g) {
        float quality = lastSignal_.quality;
        groups[g].updateElevationFast(quality, groups[g].getAverageActivity());
    }

    // ═══════════════════════════════════════════════════════════════════════
    // PHASE 10: Criticality maintenance — only when NOT in an emergent state
    // (prevents it from fighting the emergent controller)
    // ═══════════════════════════════════════════════════════════════════════
    if (!lastSignal_.should_explore && !lastSignal_.should_consolidate)
        maintainCriticality();

    // ═══════════════════════════════════════════════════════════════════════
    // PHASE 11: Periodic diagnostics
    // ═══════════════════════════════════════════════════════════════════════
    if (stepNumber % 1000 == 0) {
        logOrbitalHealth();
        diagnoseCriticality();
        std::cout << "[Emergent] STM=" << lastSignal_.stm_size
                  << " LTM=" << lastSignal_.ltm_size
                  << " surprise=" << std::fixed << std::setprecision(3) << lastSignal_.surprise
                  << " quality=" << lastSignal_.quality
                  << " trend=" << lastSignal_.improvement_trend
                  << " temp=" << attention.temperature
                  << std::endl;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // PHASE 12: Evolution request (rare)
    // ═══════════════════════════════════════════════════════════════════════
    if (stepNumber % EVOLUTION_INTERVAL == 0)
        pendingEvolution_ = true;

    flatDirty = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// routeRewards — the key fix: per-group reward from prediction error
// ─────────────────────────────────────────────────────────────────────────────
void NeuralFieldSystem::routeRewards(const EmergentSignal& sig, const GoalSignal& goal, int step) {
    constexpr float SURVIVAL = 0.02f;

    for (int g = 0; g < NUM_GROUPS; ++g) {
        float pred_reward = (g < (int)sig.per_group_reward.size())
                            ? sig.per_group_reward[g]
                            : 0.5f;

        float reward;

        if (g == 0) {
            reward = SURVIVAL + 0.08f * pred_reward;
        } else if (g >= 16 && g <= 21) {
            reward = pred_reward * 0.6f + sig.quality * 0.4f;
        } else if (g >= 3 && g <= 6) {
            reward = pred_reward * 0.5f + sig.quality * 0.3f;
        } else {
            reward = pred_reward * 0.4f + SURVIVAL;
        }

        if (sig.should_explore)
            reward += 0.05f * sig.surprise;

        // ===== НОВЫЙ КОД: ПРИМЕНЕНИЕ GOAL SIGNAL =====
        // Apply curiosity weight from GoalGenerator
        if (g < (int)goal.curiosity_weight.size()) {
            float cw = goal.curiosity_weight[g];
            reward = reward * (1.f + cw * goal_gen_.curiosity_strength);
        }
        // Urgency: temporarily boost all rewards when system is destabilising
        if (goal.urgency > 0.5f) {
            reward += (goal.urgency - 0.5f) * 0.1f;
        }
        // ===== КОНЕЦ НОВОГО КОДА =====

        reward = std::clamp(reward, 0.f, 1.f);

        groups[g].learnSTDP(reward, step);
    }
}
// ─────────────────────────────────────────────────────────────────────────────
// consolidateInterWeights — now takes a pressure [0,1] instead of being fixed
// ─────────────────────────────────────────────────────────────────────────────
void NeuralFieldSystem::consolidateInterWeights(float pressure) {
    double entropy = getUnifiedEntropy();
    double entropy_factor = 1.0 / (1.0 + std::exp(-(entropy - 0.5) * 4.0));

    double geometry_factor = 1.0;
    if (!groups.empty()) {
        double avg_curvature = 0.0;
        int valid = 0;
        for (const auto& g : groups) {
            double c = g.scalarCurvature();
            if (std::isfinite(c) && std::abs(c) < 10.0) {
                avg_curvature += c;
                ++valid;
            }
        }
        if (valid > 0) geometry_factor = 1.0 / (1.0 + avg_curvature / valid);
    }

    // pressure scales how aggressively we consolidate
    double combined = entropy_factor * geometry_factor;
    double scale    = 0.999 + 0.001 * combined;
    // When consolidation pressure is high, strengthen good connections more
    double boost    = 1.0 + static_cast<double>(pressure) * 0.01;

    for (auto& row : interWeights)
        for (auto& w : row)
            w *= scale * boost;
}

// ─────────────────────────────────────────────────────────────────────────────
// The rest is unchanged from original — geometry, energy, entropy, etc.
// (copy verbatim from original NeuralFieldSystem.cpp, only step() is new)
// ─────────────────────────────────────────────────────────────────────────────

void NeuralFieldSystem::rebuildFlatVectors() const {
    if (!flatDirty) return;
    int idx = 0;
    for (const auto& grp : groups) {
        const auto& phiGrp = grp.getPhi();
        const auto& piGrp  = grp.getPi();
        for (int i = 0; i < GROUP_SIZE; ++i) {
            flatPhi[idx] = phiGrp[i];
            flatPi[idx]  = piGrp[i];
            ++idx;
        }
    }
    flatDirty = false;
}

double NeuralFieldSystem::computeSystemEntropy() const {
    rebuildFlatVectors();
    const int BINS = 20;
    std::vector<int> hist(BINS, 0);
    for (double v : flatPhi) {
        int bin = std::clamp(static_cast<int>(v * BINS), 0, BINS - 1);
        hist[bin]++;
    }
    double entropy = 0.0;
    double total = static_cast<double>(TOTAL_NEURONS);
    for (int count : hist)
        if (count > 0) {
            double p = static_cast<double>(count) / total;
            entropy -= p * std::log(p);
        }
    return entropy;
}

double NeuralFieldSystem::getUnifiedEntropy() const {
    rebuildFlatVectors();
    const int BINS = 32;
    std::vector<int> hist(BINS, 0);
    for (double v : flatPhi) {
        int bin = std::clamp(static_cast<int>(v * BINS), 0, BINS - 1);
        hist[bin]++;
    }
    double H = 0.0, total = static_cast<double>(TOTAL_NEURONS);
    for (int count : hist)
        if (count > 0) {
            double p = count / total;
            H -= p * std::log2(p);
        }
    return std::clamp(H / std::log2(static_cast<double>(BINS)), 0.0, 1.0);
}

double NeuralFieldSystem::getTargetUnifiedEntropy() const {
    double pred_error = predictive_coder ? predictive_coder->getLastError() : 0.5;
    double energy     = computeTotalEnergy();
    double base       = 0.5;
    double err_factor = 1.0 + std::min(0.5, pred_error);
    double eng_factor = 1.0 / (1.0 + energy);
    double mode_factor = 1.0;
    switch (current_mode_) {
        case OperatingMode::TRAINING: mode_factor = 1.3; break;
        case OperatingMode::IDLE:     mode_factor = 0.7; break;
        case OperatingMode::SLEEP:    mode_factor = 1.2; break;
        default: break;
    }
    return std::clamp(base * err_factor * eng_factor * mode_factor, 0.2, 0.8);
}

double NeuralFieldSystem::computeTotalEnergy() const {
    rebuildFlatVectors();
    double total = 0.0;
    for (double v : flatPi)  total += v * v * 0.5;
    for (double r : flatPhi) total += r * r * 0.5;
    for (int g = 0; g < NUM_GROUPS; ++g) {
        const auto& W = groups[g].getWeights();
        for (int i = 0; i < GROUP_SIZE; ++i)
            for (int j = i + 1; j < GROUP_SIZE; ++j)
                total += std::abs(W[i][j]) * 0.1;
    }
    return total;
}

std::vector<double> NeuralFieldSystem::getGroupAverages() const {
    std::vector<double> avgs(NUM_GROUPS);
    for (int g = 0; g < NUM_GROUPS; ++g)
        avgs[g] = groups[g].getAverageActivity();
    return avgs;
}

void NeuralFieldSystem::strengthenInterConnection(int from, int to, double delta) {
    if (from >= 0 && from < NUM_GROUPS && to >= 0 && to < NUM_GROUPS && from != to) {
        interWeights[from][to] = std::clamp(interWeights[from][to] + delta, -0.5, 0.5);
    }
}
void NeuralFieldSystem::weakenInterConnection(int from, int to, double delta) {
    strengthenInterConnection(from, to, -delta);
}

std::vector<float> NeuralFieldSystem::getFeatures() const {
    std::vector<float> features(64, 0.f);
    auto avgs = getGroupAverages();
    for (int g = 0; g < NUM_GROUPS; ++g) features[g] = static_cast<float>(avgs[g]);
    for (int g = 0; g < NUM_GROUPS; ++g) {
        const auto& phi = groups[g].getPhi();
        const int BINS = 10;
        std::vector<int> hist(BINS, 0);
        for (double v : phi) hist[std::clamp((int)(v * BINS), 0, BINS - 1)]++;
        double H = 0.0;
        for (int c : hist) if (c > 0) { double p = c / (double)GROUP_SIZE; H -= p * std::log(p); }
        features[NUM_GROUPS + g] = static_cast<float>(H);
    }
    return features;
}

void NeuralFieldSystem::setOperatingMode(OperatingMode::Type mode) {
    current_mode_ = mode;
    for (auto& g : groups) g.setCurrentMode(mode);
}

// ── Reentry (unchanged) ───────────────────────────────────────────────────────
void NeuralFieldSystem::applyReentry(int iterations) {
    std::vector<double> newGroupAvg(NUM_GROUPS, 0.0);
    std::vector<double> currAvg = getGroupAverages();

    for (int iter = 0; iter < iterations; ++iter) {
        attention.computeAttention(currAvg);
        auto spherical = attention.computeSphericalAttention(currAvg);
        std::fill(newGroupAvg.begin(), newGroupAvg.end(), 0.0);

        for (int g = 0; g < NUM_GROUPS; ++g) {
            double input = 0.0;
            for (int h = 0; h < NUM_GROUPS; ++h) {
                double att = 0.7 * attention.attention_weights[h] + 0.3 * spherical[h];
                input += interWeights[g][h] * currAvg[h] * att;
            }
            newGroupAvg[g] = std::clamp(currAvg[g] + dt * input, 0.0, 1.0);
        }
        currAvg.swap(newGroupAvg);
    }

    for (int g = 0; g < NUM_GROUPS; ++g) {
        double diff = currAvg[g] - groups[g].getAverageActivity();
        auto& phi   = groups[g].getPhiNonConst();
        for (int n = 0; n < GROUP_SIZE; ++n)
            phi[n] = std::clamp(phi[n] + dt * diff * 0.1, 0.0, 1.0);
    }
}

// ── Lateral inhibition (unchanged) ───────────────────────────────────────────
void NeuralFieldSystem::applyLateralInhibition() {
    const int NUM_SEM = 6, SEM_START = 16;
    std::vector<double> act(NUM_GROUPS, 0.0);
    for (int g = 0; g < NUM_GROUPS; ++g) {
        const auto& phi = groups[g].getPhi();
        act[g] = std::accumulate(phi.begin(), phi.end(), 0.0) / GROUP_SIZE;
    }
    const double INHIB = 0.1;
    for (int g = SEM_START; g < SEM_START + NUM_SEM; ++g) {
        double inh = 0.0;
        for (int o = SEM_START; o < SEM_START + NUM_SEM; ++o)
            if (o != g) inh += act[o];
        auto& phi = groups[g].getPhiNonConst();
        for (int i = 0; i < GROUP_SIZE; ++i)
            phi[i] = std::max(0.0, phi[i] - INHIB * inh);
    }
}

// ── Ricci flow (unchanged) ────────────────────────────────────────────────────
void NeuralFieldSystem::applyRicciFlow() {
    if (entropy_history.size() < 10) return;
    double avg_curv = 0.0; int cnt = 0;
    for (const auto& g : groups) {
        double c = g.scalarCurvature();
        if (std::isfinite(c) && std::abs(c) < 10.0) { avg_curv += c; ++cnt; }
    }
    if (cnt == 0) return;
    avg_curv /= cnt;

    auto avgs = getGroupAverages();
    double flow_rate = 0.001;
    for (int i = 0; i < NUM_GROUPS; ++i)
        for (int j = i + 1; j < NUM_GROUPS; ++j) {
            double corr  = avgs[i] * avgs[j];
            double ricci = corr - avg_curv * interWeights[i][j];
            double delta = std::clamp(-2.0 * flow_rate * ricci, -0.01, 0.01);
            interWeights[i][j] = std::clamp(interWeights[i][j] + delta, -0.5, 0.5);
            interWeights[j][i] = interWeights[i][j];
        }
}

// ── Pruning by elevation (unchanged) ─────────────────────────────────────────
void NeuralFieldSystem::applyPruningByElevation() {
    for (auto& g : groups)
        if (g.getElevation() < -0.5f)
            g.decayAllWeights(0.95f);
}

float NeuralFieldSystem::computeGlobalImportance() {
    if (entropy_history.size() < 10) return 0.5f;
    double cur = entropy_history.back();
    double old = entropy_history[entropy_history.size() - 10];
    return std::max(0.f, static_cast<float>(-(cur - old) * 10.0));
}

// ── Criticality maintenance (now only fills the gap; EmergentController leads) ─
void NeuralFieldSystem::maintainCriticality() {
    double cur_entropy = getUnifiedEntropy();
    double tgt_entropy = getTargetUnifiedEntropy();
    double err         = tgt_entropy - cur_entropy;

    if (std::abs(err) > 0.05) {
        if (err > 0) {
            attention.temperature = std::clamp(attention.temperature * 1.02f, 0.1f, 5.0f);
            for (auto& g : groups) { g.setConsolidationRate(0.003f); g.setStdpRate(0.6f); }
        } else {
            attention.temperature = std::clamp(attention.temperature * 0.98f, 0.1f, 5.0f);
            for (auto& g : groups) { g.setConsolidationRate(0.02f); g.setStdpRate(0.3f); }
        }
    }
}

// ── diagnoseCriticality / logOrbitalHealth / misc (unchanged) ─────────────────
void NeuralFieldSystem::diagnoseCriticality() {
    double H = computeSystemEntropy();
    static double last_H = H;
    double dH = std::abs(H - last_H);
    if (dH > 0.1) std::cout << "⚠️ Phase transition: ΔH=" << dH << std::endl;
    if (H < 0.5)  std::cout << "❌ Frozen. Boosting temp." << std::endl, attention.temperature *= 1.5f;
    if (H > 3.0)  std::cout << "❌ Chaotic. Reducing temp." << std::endl, attention.temperature *= 0.7f;
    last_H = H;
}

void NeuralFieldSystem::logOrbitalHealth() {
    if (stepCounter % 1000 != 0) return;
    std::vector<int> dist(5, 0);
    int total = 0;
    for (const auto& g : groups)
        for (int i = 0; i < GROUP_SIZE; ++i) { dist[g.getOrbitLevel(i)]++; total++; }
    std::cout << "\n=== ORBITAL HEALTH ===\n";
    const char* names[] = {"Singularity","Orbit1","Orbit2","Orbit3","Elite"};
    for (int l = 4; l >= 0; --l)
        std::cout << names[l] << ": " << dist[l]
                  << " (" << dist[l] * 100 / total << "%)\n";
    std::cout << "=== EMERGENT MEM ===\n"
              << "STM=" << emergent_.memory.stmSize()
              << " LTM=" << emergent_.memory.ltmSize()
              << " BestScore=" << emergent_.evaluator.bestScore() << "\n"
              << "====================\n";
}

double NeuralFieldSystem::computeOptimalStructure() {
    double temperature = attention.temperature;
    std::vector<double> probs;
    double Z = 0.0;
    for (const auto& g : groups) {
        double E   = g.getAverageEnergy();
        double arg = std::clamp(-E / (temperature + 1e-6), -50.0, 50.0);
        double p   = std::exp(arg);
        probs.push_back(p); Z += p;
    }
    if (Z < 1e-12) return 0.0;
    double H = 0.0;
    for (double p : probs) { p /= Z; if (p > 1e-12) H -= p * std::log2(p); }
    return H;
}

void NeuralFieldSystem::applyResourcePenalty(float penalty) {
    for (auto& g : groups) g.applyGlobalPenalty(penalty);
    if (penalty < -0.3f) {
        attention.temperature = std::clamp(attention.temperature * 0.98f, 0.1f, 5.0f);
    }
    if (penalty < -0.7f)
        for (auto& g : groups) g.checkForResourceExhaustion();
}

void NeuralFieldSystem::enterLowPowerMode() {
    for (auto& g : groups) {
        if (g.getAverageActivity() > 0.3) {
            auto& phi = g.getPhiNonConst();
            for (auto& v : phi) v = std::max(v * 0.7, 0.2);
        }
    }
}

void NeuralFieldSystem::applyTargetPattern(const std::vector<float>& pat) {
    const int SEM_START = 16;
    for (int g = 0; g < 6; ++g) {
        auto& phi = groups[SEM_START + g].getPhiNonConst();
        for (int i = 0; i < GROUP_SIZE; ++i) {
            float diff = pat[g * 32 + i] - static_cast<float>(phi[i]);
            phi[i] = std::clamp(phi[i] + diff * 0.1, 0.0, 1.0);
        }
    }
}

void NeuralFieldSystem::applyTargetedMutation(double strength, int targetType) {
    std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<double> d(-strength, strength);
    if (targetType == 0) {
        for (int i = 0; i < NUM_GROUPS; ++i)
            for (int j = 0; j < NUM_GROUPS; ++j)
                if (i != j) interWeights[i][j] += d(gen) * 0.1;
    } else {
        std::uniform_int_distribution<> gi(0, NUM_GROUPS - 1);
        int g = gi(gen);
        groups[g].setLearningRate(groups[g].getLearningRate() + d(gen) * 0.01);
        groups[g].setThreshold(groups[g].getThreshold() + d(gen) * 0.01);
    }
}

// ── Reflection stubs ──────────────────────────────────────────────────────────
NeuralFieldSystem::ReflectionState NeuralFieldSystem::getReflectionState() const {
    ReflectionState s;
    s.confidence   = lastSignal_.quality;
    s.curiosity    = lastSignal_.surprise;
    s.satisfaction = lastSignal_.improvement_trend > 0 ? 0.7 : 0.3;
    s.confusion    = lastSignal_.should_explore ? 0.7 : 0.2;
    s.attention_map.assign(4, 0.25);
    return s;
}
void NeuralFieldSystem::reflect() {}
void NeuralFieldSystem::setGoal(const std::string& g) { current_goal = g; }
bool NeuralFieldSystem::evaluateProgress() { return lastSignal_.improvement_trend > 0; }
void NeuralFieldSystem::learnFromReflection(float) {}
