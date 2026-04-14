#pragma once
// SelfSignalSampler.hpp
//
// Reads the neural field and produces a compact introspective signal vector.
// This vector is then injected back into dedicated "self-model" groups as
// ordinary input — not as a separate symbolic layer.
//
// Signal categories (each mapped to one float in [0,1]):
//   Topology signals  — orbit distribution, curvature, connectivity pressure
//   Energy signals    — total energy, per-tier averages, dissipation rate
//   Memory signals    — STM/LTM sizes, consolidation rate, discard rate
//   Learning signals  — surprise (prediction error), improvement trend, quality
//   Homeostasis       — entropy, temperature, deviation from target
//
// Total: 32 signals → injected into groups 28–31 (8 neurons each × 4 groups)

#include <vector>
#include <deque>
#include <cmath>
#include <algorithm>
#include <numeric>

// Forward declarations (include the real headers in the .cpp)
class NeuralFieldSystem;
struct EmergentSignal;

// ── One snapshot of the system's self-knowledge ──────────────────────────────
struct SelfSnapshot {
    // ── Topology (8 signals) ──────────────────────────────────────────────
    float orbit0_fraction;     // fraction of neurons in singularity
    float orbit1_fraction;     // fraction in inner orbit
    float orbit2_fraction;     // fraction in base orbit (words)
    float orbit3_fraction;     // fraction in coordination orbit
    float orbit4_fraction;     // fraction in elite orbit
    float avg_curvature;       // mean scalar curvature across groups, normalised
    float connectivity_density;// fraction of weight matrix that is non-zero
    float orbit_balance;       // 1 = ideal pyramid, 0 = collapsed

    // ── Energy (6 signals) ────────────────────────────────────────────────
    float total_energy_norm;   // total energy / historical max
    float energy_elite;        // avg energy in orbit 4
    float energy_mid;          // avg energy in orbits 2–3
    float energy_low;          // avg energy in orbits 0–1
    float energy_change_rate;  // |E_now - E_prev| / E_prev
    float mass_pressure;       // avg mass / max_mass (saturation signal)

    // ── Memory (6 signals) ───────────────────────────────────────────────
    float stm_fullness;        // stm_size / stm_capacity
    float ltm_fullness;        // ltm_size / ltm_capacity
    float ltm_avg_importance;  // average LTM record importance
    float consolidation_rate;  // consolidated_per_step (EMA), normalised
    float discard_rate;        // discarded_per_step (EMA), normalised
    float novelty_pressure;    // fraction of STM items that are new (similarity<0.5)

    // ── Learning / prediction (6 signals) ────────────────────────────────
    float surprise;            // prediction error (from EmergentSignal)
    float quality;             // self-evaluated output quality
    float improvement_trend;   // positive = getting better, normalised to [0,1]
    float explore_flag;        // 1 if should_explore, 0 otherwise
    float consolidate_flag;    // 1 if should_consolidate, 0 otherwise
    float attention_temperature; // attention temperature, normalised

    // ── Homeostasis / entropy (6 signals) ────────────────────────────────
    float system_entropy;      // unified entropy [0,1]
    float entropy_error;       // |current - target| entropy
    float avg_group_elevation; // mean elevation across all groups
    float burst_pressure;      // fraction of groups with burst pending
    float singularity_escape_rate; // recent ejection_count normalised
    float wave_coherence;      // mean |phase difference| across elite neurons

    // Flat serialisation (32 floats in defined order)
    std::vector<float> toVector() const {
        return {
            orbit0_fraction, orbit1_fraction, orbit2_fraction,
            orbit3_fraction, orbit4_fraction, avg_curvature,
            connectivity_density, orbit_balance,

            total_energy_norm, energy_elite, energy_mid, energy_low,
            energy_change_rate, mass_pressure,

            stm_fullness, ltm_fullness, ltm_avg_importance,
            consolidation_rate, discard_rate, novelty_pressure,

            surprise, quality, improvement_trend,
            explore_flag, consolidate_flag, attention_temperature,

            system_entropy, entropy_error, avg_group_elevation,
            burst_pressure, singularity_escape_rate, wave_coherence
        };
    }
};

// ── SelfSignalSampler ─────────────────────────────────────────────────────────
class SelfSignalSampler {
public:
    // Which groups receive self-signals (treated as ordinary input groups)
    static constexpr int SELF_GROUP_START = 28;
    static constexpr int SELF_GROUP_COUNT = 4;   // groups 28, 29, 30, 31
    static constexpr int SIGNALS_PER_GROUP = 8;  // 4 × 8 = 32 total
    static constexpr int TOTAL_SIGNALS = SELF_GROUP_COUNT * SIGNALS_PER_GROUP; // == 32

    // Injection strength: how strongly the self-signal overrides existing phi
    // 0.2 = gentle nudge, 0.8 = strong override
    float injection_strength = 0.25f;

    // EMA alpha for smoothing rates
    float ema_alpha = 0.05f;

    SelfSignalSampler() {
        consolidation_ema_ = 0.f;
        discard_ema_       = 0.f;
        energy_history_.assign(64, 0.f);
        energy_max_        = 1.f;
    }

    // Call once per step AFTER the emergent tick but BEFORE STDP
    SelfSnapshot sample(const NeuralFieldSystem& sys,
                        const EmergentSignal&    sig,
                        int                      step);

    // Inject the snapshot back into groups 28–31 as ordinary phi values
    // (does NOT call learnSTDP — that happens in routeRewards as usual)
    void inject(NeuralFieldSystem& sys, const SelfSnapshot& snap);

    // Returns the last computed snapshot (useful for diagnostics)
    const SelfSnapshot& last() const { return last_snap_; }

private:
    SelfSnapshot last_snap_{};
    float consolidation_ema_ = 0.f;
    float discard_ema_       = 0.f;
    std::deque<float> energy_history_;
    float energy_max_  = 1.f;
    int   last_stm_size_ = 0;
    int   last_ltm_size_ = 0;

    // Moving average
    float ema(float old_val, float new_val) const {
        return old_val * (1.f - ema_alpha) + new_val * ema_alpha;
    }
};