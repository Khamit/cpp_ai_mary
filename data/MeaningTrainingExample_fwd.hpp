// data/MeaningTrainingExample_fwd.hpp
#pragma once
#include <vector>
#include <string>
#include <cstdint>

// Включаем существующее определение AccessLevel из core
#include "../core/AccessLevel.hpp"

struct MeaningTrainingExample {
    std::vector<std::string> input_words;
    std::vector<uint32_t> expected_meanings;
    std::vector<std::pair<uint32_t, uint32_t>> cause_effect;
    float difficulty = 0.0f;
    std::string category;
    float priority = 0.0f;
    AccessLevel required_level = AccessLevel::GUEST;  // используем GUEST (значение 1)
};