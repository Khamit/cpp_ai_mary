// SelfSignalSampler.cpp
#include "SelfSignalSampler.hpp"
#include "NeuralFieldSystem.hpp"   // full definition
#include "EmergentCore.hpp"        // EmergentSignal
#include <cmath>
#include <numeric>
#include <algorithm>

// ── sample() ─────────────────────────────────────────────────────────────────
SelfSnapshot SelfSignalSampler::sample(const NeuralFieldSystem& sys,
                                       const EmergentSignal&    sig,
                                       int                      step) {
    SelfSnapshot s{};
    const auto& groups = sys.getGroups();
    constexpr int NG = NeuralFieldSystem::NUM_GROUPS;
    constexpr int GS = NeuralFieldSystem::GROUP_SIZE;

    // ── Topology ─────────────────────────────────────────────────────────
    std::array<int, 5> orbit_counts{};
    int total_neurons = NG * GS;
    double total_curvature = 0.0;
    double total_connections = 0.0;
    double possible_connections = GS * (GS - 1) / 2.0;

    for (const auto& g : groups) {
        for (int i = 0; i < GS; ++i)
            orbit_counts[g.getOrbitLevel(i)]++;
        double c = g.scalarCurvature();
        if (std::isfinite(c)) total_curvature += c;
        // Connectivity density: fraction of intra-group weights > threshold
        const auto& W = g.getWeights();
        for (int i = 0; i < GS; ++i)
            for (int j = i + 1; j < GS; ++j)
                if (std::abs(W[i][j]) > 0.05) total_connections++;
    }

    s.orbit0_fraction = std::clamp(orbit_counts[0] / (float)total_neurons, 0.f, 1.f);
    s.orbit1_fraction = std::clamp(orbit_counts[1] / (float)total_neurons, 0.f, 1.f);
    s.orbit2_fraction = std::clamp(orbit_counts[2] / (float)total_neurons, 0.f, 1.f);
    s.orbit3_fraction = std::clamp(orbit_counts[3] / (float)total_neurons, 0.f, 1.f);
    s.orbit4_fraction = std::clamp(orbit_counts[4] / (float)total_neurons, 0.f, 1.f);

    double raw_curv = total_curvature / NG;
    s.avg_curvature = std::clamp((float)((raw_curv + 10.0) / 20.0), 0.f, 1.f); // normalise [-10,10]→[0,1]

    s.connectivity_density = std::clamp(
        (float)(total_connections / (NG * possible_connections)), 0.f, 1.f);

    // Orbit balance: compare to ideal distribution {0.20,0.30,0.30,0.15,0.05}
    const float ideal[] = {0.20f, 0.30f, 0.30f, 0.15f, 0.05f};
    float balance_err = 0.f;
    for (int i = 0; i < 5; ++i)
        balance_err += std::abs(orbit_counts[i] / (float)total_neurons - ideal[i]);
    s.orbit_balance = std::clamp(1.f - balance_err / 2.f, 0.f, 1.f);

    // ── Energy ───────────────────────────────────────────────────────────
    float total_energy = (float)sys.computeTotalEnergy();
    energy_history_.push_back(total_energy);
    if (energy_history_.size() > 64) energy_history_.pop_front();

    float hist_max = *std::max_element(energy_history_.begin(), energy_history_.end());
    if (hist_max > energy_max_) energy_max_ = hist_max;
    s.total_energy_norm = std::clamp(total_energy / (energy_max_ + 1e-6f), 0.f, 1.f);

    // Per-tier energy
    float e_elite = 0.f, e_mid = 0.f, e_low = 0.f;
    int   n_elite = 0,   n_mid = 0,   n_low = 0;
    for (const auto& g : groups) {
        for (int i = 0; i < GS; ++i) {
            float e = g.getNeuronEnergy(i);
            int   lv = g.getOrbitLevel(i);
            if (lv >= 4)       { e_elite += e; ++n_elite; }
            else if (lv >= 2)  { e_mid   += e; ++n_mid;   }
            else               { e_low   += e; ++n_low;   }
        }
    }
    // normalise by a reasonable ceiling of 5.0 energy units
    s.energy_elite = std::clamp(n_elite ? e_elite / n_elite / 5.f : 0.f, 0.f, 1.f);
    s.energy_mid   = std::clamp(n_mid   ? e_mid   / n_mid   / 5.f : 0.f, 0.f, 1.f);
    s.energy_low   = std::clamp(n_low   ? e_low   / n_low   / 5.f : 0.f, 0.f, 1.f);

    float prev_energy = energy_history_.size() > 1 ? energy_history_[energy_history_.size()-2] : total_energy;
    s.energy_change_rate = std::clamp(std::abs(total_energy - prev_energy) / (prev_energy + 1e-6f), 0.f, 1.f);

    // Mass pressure: mean(mass[i]) / max_mass across all groups
    double total_mass = 0.0;
    int    total_mass_n = 0;
    for (const auto& g : groups) {
        for (int i = 0; i < GS; ++i) { total_mass += g.getMass(i); ++total_mass_n; }
    }
    float avg_mass = total_mass_n ? (float)(total_mass / total_mass_n) : 1.f;
    s.mass_pressure = std::clamp(avg_mass / 12.f, 0.f, 1.f); // homeo.max_mass ≈ 12

    // ── Memory ───────────────────────────────────────────────────────────
    const auto& mem = sys.emergent().memory;
    float stm_cap = 128.f, ltm_cap = 1024.f;
    s.stm_fullness = std::clamp(mem.stmSize() / stm_cap, 0.f, 1.f);
    s.ltm_fullness = std::clamp(mem.ltmSize() / ltm_cap, 0.f, 1.f);
    s.ltm_avg_importance = mem.averageLTMImportance();

    // Consolidation rate: delta LTM size (EMA)
    int ltm_delta = (int)mem.ltmSize() - last_ltm_size_;
    consolidation_ema_ = ema(consolidation_ema_, std::max(0, ltm_delta) / 10.f);
    s.consolidation_rate = std::clamp(consolidation_ema_, 0.f, 1.f);

    int stm_shrink = last_stm_size_ - (int)mem.stmSize();
    discard_ema_ = ema(discard_ema_, std::max(0, stm_shrink) / 10.f);
    s.discard_rate = std::clamp(discard_ema_, 0.f, 1.f);

    last_ltm_size_ = (int)mem.ltmSize();
    last_stm_size_ = (int)mem.stmSize();

    // Novelty pressure: rough proxy — STM size relative to LTM (full STM = lots of new stuff)
    s.novelty_pressure = s.stm_fullness * (1.f - s.ltm_avg_importance);

    // ── Learning / prediction ─────────────────────────────────────────────
    s.surprise     = std::clamp(sig.surprise,    0.f, 1.f);
    s.quality      = std::clamp(sig.quality,     0.f, 1.f);
    // improvement_trend is [-inf,+inf], map to [0,1] around 0
    s.improvement_trend = std::clamp(sig.improvement_trend * 20.f + 0.5f, 0.f, 1.f);
    s.explore_flag      = sig.should_explore    ? 1.f : 0.f;
    s.consolidate_flag  = sig.should_consolidate ? 1.f : 0.f;
    s.attention_temperature = std::clamp(sys.getAttentionTemperature() / 5.f, 0.f, 1.f);

    // ── Homeostasis / entropy ─────────────────────────────────────────────
    s.system_entropy = (float)sys.getUnifiedEntropy();
    float target_ent = (float)sys.getTargetUnifiedEntropy();
    s.entropy_error  = std::clamp(std::abs(s.system_entropy - target_ent) * 5.f, 0.f, 1.f);

    // Average elevation
    float total_elev = 0.f;
    for (const auto& g : groups) total_elev += g.getElevation();
    s.avg_group_elevation = std::clamp((total_elev / NG + 1.f) / 2.f, 0.f, 1.f); // [-1,1]→[0,1]

    // Burst pressure: proxy via orbit0 fraction (many singularity neurons = pressure rising)
    s.burst_pressure = s.orbit0_fraction;

    // Singularity escape rate: orbit0 fraction change (EMA)
    static float prev_o0 = s.orbit0_fraction;
    float escape = std::max(0.f, prev_o0 - s.orbit0_fraction); // decreased orbit0 = escapes
    s.singularity_escape_rate = std::clamp(escape * 10.f, 0.f, 1.f);
    prev_o0 = s.orbit0_fraction;

    // Wave coherence: for elite neurons, mean |cos(phase_i - phase_j)|
    // (high coherence = elite neurons are synchronised)
    float phase_sum = 0.f; int phase_n = 0;
    for (const auto& g : groups) {
        for (int i = 0; i < GS; ++i) {
            if (g.getOrbitLevel(i) < 3) continue;
            for (int j = i + 1; j < GS; ++j) {
                if (g.getOrbitLevel(j) < 3) continue;
                double dp = g.getWavePhase(i) - g.getWavePhase(j);
                phase_sum += std::abs((float)std::cos(dp));
                ++phase_n;
            }
        }
    }
    s.wave_coherence = phase_n > 0 ? phase_sum / phase_n : 0.5f;

    last_snap_ = s;
    return s;
}

// ── inject() ─────────────────────────────────────────────────────────────────
// The key design choice: we write into getPhiNonConst() of groups 28–31.
// These groups then participate in normal Reentry and STDP in the next tick —
// nothing special, just ordinary neurons with a particular input.
void SelfSignalSampler::inject(NeuralFieldSystem& sys, const SelfSnapshot& snap) {
    auto signals = snap.toVector(); // 32 floats

    auto& groups = sys.getGroupsNonConst();
    float alpha = injection_strength;

    for (int gi = 0; gi < SELF_GROUP_COUNT; ++gi) {
        int group_idx = SELF_GROUP_START + gi;
        if (group_idx >= NeuralFieldSystem::NUM_GROUPS) break;

        auto& phi = groups[group_idx].getPhiNonConst();
        int base = gi * SIGNALS_PER_GROUP;

        for (int ni = 0; ni < SIGNALS_PER_GROUP; ++ni) {
            float signal = signals[base + ni]; // already in [0,1]
            // Blend: don't erase existing activity, just nudge toward signal
            phi[ni] = phi[ni] * (1.f - alpha) + signal * alpha;
        }
    }
}