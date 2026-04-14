#pragma once
// GoalGenerator.hpp
//
// Reads the current self-model (groups 28–31) and EmergentMemory (LTM)
// to produce an intrinsic goal signal.
//
// Goals are NOT symbolic strings. They are:
//   1. A curiosity_weight per group [0,1] — which groups to reward extra
//   2. A boredom signal — triggers exploration when the field is too static
//   3. A satiation signal — reduces reward when the same pattern repeats
//
// The GoalGenerator plugs into routeRewards() as a multiplicative modifier.
// It does NOT create a separate symbolic planner.

#include <vector>
#include <deque>
#include <string>
#include <cmath>
#include <algorithm>
#include "SelfSignalSampler.hpp"   // SelfSnapshot
#include "EmergentCore.hpp"         // EmergentMemory

class NeuralFieldSystem;

// ── GoalSignal ────────────────────────────────────────────────────────────────
// Output of GoalGenerator, consumed by routeRewards()
struct GoalSignal {
    // Per-group curiosity weight [0,1].
    // routeRewards multiplies its base reward by (1 + curiosity_weight[g] * strength)
    std::vector<float> curiosity_weight;

    // Global modifiers
    float boredom;       // [0,1] — system is bored, needs novelty
    float satiation;     // [0,1] — system has seen this pattern too much
    float urgency;       // [0,1] — something is destabilising, act now

    // Descriptive label of the dominant drive (for logging only)
    std::string dominant_drive; // "curiosity" | "consolidation" | "repair" | "idle"
};

// ── GoalGenerator ─────────────────────────────────────────────────────────────
class GoalGenerator {
public:
    // How strongly the curiosity weight scales reward
    float curiosity_strength = 0.3f;

    // Boredom rises when system entropy doesn't change for this many steps
    int boredom_window = 200;

    // Satiation: if the same self-signal pattern repeats for this many steps, satiate
    int satiation_window = 50;

GoalGenerator() : curiosity_(32, 0.f) {  // ← 32 вместо NeuralFieldSystem::NUM_GROUPS
    boredom_        = 0.f;
    satiation_      = 0.f;
    entropy_history_.resize(boredom_window, 0.5f);
    history_idx_    = 0;
}

    // Call once per step
    GoalSignal generate(const SelfSnapshot&   snap,
                        const EmergentSignal& esig,
                        const EmergentMemory& mem,
                        int                   step);

    // Returns the last GoalSignal (for diagnostics / UI)
    const GoalSignal& last() const { return last_goal_; }

private:
    std::vector<float>  curiosity_;
    float               boredom_;
    float               satiation_;
    std::vector<float>  entropy_history_;
    int                 history_idx_     = 0;
    std::deque<float>   snap_norm_history_; // norm of recent self-snapshots
    GoalSignal          last_goal_;

    // Boredom: how flat has system entropy been over the last window?
    float computeBoredom(float current_entropy);

    // Satiation: how similar is the current snap to recent snaps?
    float computeSatiation(const std::vector<float>& snap_vec);

    // Which groups are most active and novel (deserve curiosity boost)?
    void updateCuriosity(const SelfSnapshot&   snap,
                         const EmergentSignal& esig,
                         const EmergentMemory& mem);
};


// ── Implementation ─────────────────────────────────────────────────────────────
// (Inline here for header-only convenience; split to .cpp in your project)

inline float GoalGenerator::computeBoredom(float entropy) {
    entropy_history_[history_idx_ % boredom_window] = entropy;
    ++history_idx_;

    if (history_idx_ < boredom_window) return 0.f;

    // Variance of entropy over window — low variance = boring
    float mean = 0.f;
    for (float v : entropy_history_) mean += v;
    mean /= boredom_window;

    float var = 0.f;
    for (float v : entropy_history_) var += (v - mean) * (v - mean);
    var /= boredom_window;

    // High variance = interesting, low variance = bored
    // Normalise: expected variance ~0.01, ceiling 0.05
    float boredom = std::clamp(1.f - var / 0.01f, 0.f, 1.f);
    // Smooth
    boredom_ = boredom_ * 0.95f + boredom * 0.05f;
    return boredom_;
}

inline float GoalGenerator::computeSatiation(const std::vector<float>& snap_vec) {
    // Compute L2 norm of current snap
    float norm = 0.f;
    for (float v : snap_vec) norm += v * v;
    norm = std::sqrt(norm);

    snap_norm_history_.push_back(norm);
    if ((int)snap_norm_history_.size() > satiation_window)
        snap_norm_history_.pop_front();

    if ((int)snap_norm_history_.size() < satiation_window / 2) return 0.f;

    // How much does norm vary? Low variance = same pattern = satiated
    float hist_mean = 0.f;
    for (float v : snap_norm_history_) hist_mean += v;
    hist_mean /= snap_norm_history_.size();

    float hist_var = 0.f;
    for (float v : snap_norm_history_) hist_var += (v - hist_mean) * (v - hist_mean);
    hist_var /= snap_norm_history_.size();

    satiation_ = satiation_ * 0.95f + std::clamp(1.f - hist_var / 0.005f, 0.f, 1.f) * 0.05f;
    return satiation_;
}

inline void GoalGenerator::updateCuriosity(const SelfSnapshot&   snap,
                                            const EmergentSignal& esig,
                                            const EmergentMemory& mem) {
    // Curiosity is high for groups that:
    //   a) Are in exploration mode (high surprise)
    //   b) Have low LTM coverage (novel territory for this group's patterns)
    //   c) Are "mid-tier" (orbit 2–3) — they're plastic enough to learn

    // Base curiosity from surprise
    float base = esig.surprise * 0.4f + snap.novelty_pressure * 0.3f + boredom_ * 0.3f;

    // Semantic groups (16–21) get a curiosity boost when quality is low
    for (int g = 0; g < (int)curiosity_.size(); ++g) {
        float group_curiosity = base;

        if (g >= 16 && g <= 21) {
            // Low quality → semantic groups need to learn more
            group_curiosity += (1.f - esig.quality) * 0.4f;
        } else if (g >= 28 && g <= 31) {
            // Self-model groups — curiosity tracks entropy_error
            // (the system is curious about its own state when that state is uncertain)
            group_curiosity += snap.entropy_error * 0.5f;
        } else if (g >= 3 && g <= 6) {
            // Coordination groups benefit when orbits are imbalanced
            group_curiosity += (1.f - snap.orbit_balance) * 0.3f;
        }

        // Smooth
        curiosity_[g] = curiosity_[g] * 0.9f + group_curiosity * 0.1f;
        curiosity_[g] = std::clamp(curiosity_[g], 0.f, 1.f);
    }
}

inline GoalSignal GoalGenerator::generate(const SelfSnapshot&   snap,
                                           const EmergentSignal& esig,
                                           const EmergentMemory& mem,
                                           int                   step) {
    auto snap_vec = snap.toVector();

    float boredom   = computeBoredom(snap.system_entropy);
    float satiation = computeSatiation(snap_vec);
    updateCuriosity(snap, esig, mem);

    GoalSignal gs;
    gs.curiosity_weight = curiosity_;
    gs.boredom          = boredom;
    gs.satiation        = satiation;

    // Urgency: system is destabilising
    gs.urgency = std::clamp(snap.orbit0_fraction * 2.f +   // many neurons dying
                             snap.entropy_error   * 1.f +   // entropy off target
                             snap.mass_pressure   * 0.5f,   // mass saturation
                             0.f, 1.f);

    // Dominant drive label (for logging)
    if      (gs.urgency    > 0.6f) gs.dominant_drive = "repair";
    else if (gs.boredom    > 0.6f) gs.dominant_drive = "curiosity";
    else if (gs.satiation  > 0.7f) gs.dominant_drive = "consolidation";
    else if (esig.quality  > 0.8f) gs.dominant_drive = "idle";
    else                           gs.dominant_drive = "learning";

    last_goal_ = gs;
    return gs;
}