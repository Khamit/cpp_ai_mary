#pragma once
#include <cmath>
#include <algorithm>

// --------------------
// Синапс (STDP + eligibility trace)
// --------------------
struct Synapse
{
    float weight;           // вес синапса
    float lastPreFire;      // время последнего pre-спайка (шаги симуляции)
    float lastPostFire;     // время последнего post-спайка
    float eligibility;     // eligibility trace (медленная память)

    // Флаг типа (на будущее: тормозные нейроны)
    bool inhibitory;

    Synapse()
        : weight(0.1f),
          lastPreFire(-1e6f),
          lastPostFire(-1e6f),
          eligibility(0.0f),
          inhibitory(false)
    {}
};

// --------------------
// Параметры пластичности
// --------------------
struct PlasticityParams
{
    // --- Скорости обучения ---
    float stdpRate;            // скорость reward-modulated STDP
    float hebbianRate;         // медленная хеббовская адаптация
    float consolidationRate;   // перенос eligibility в вес

    // --- STDP параметры ---
    float A_plus;              // LTP амплитуда
    float A_minus;             // LTD амплитуда
    float tau_plus;            // временная константа LTP
    float tau_minus;           // временная константа LTD

    // --- Eligibility ---
    float eligibilityDecay;    // затухание eligibility trace (0.9–0.99)

    // --- Ограничения ---
    float maxWeight;           // максимум |weight|
    float minWeight;           // минимум |weight| (обычно 0)

    PlasticityParams()
        : stdpRate(0.01f),
          hebbianRate(0.001f),
          consolidationRate(0.0001f),
          A_plus(0.1f),
          A_minus(0.12f),
          tau_plus(20.0f),
          tau_minus(20.0f),
          eligibilityDecay(0.95f),
          maxWeight(1.0f),
          minWeight(0.0f)
    {}
};