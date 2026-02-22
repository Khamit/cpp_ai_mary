//cpp_ai_test/modules/EvolutionMetrics.cpp
#include "EvolutionMetrics.hpp"

EvolutionMetrics::EvolutionMetrics() 
    : code_size_score(1.0), performance_score(1.0), 
      energy_score(1.0), overall_fitness(1.0) {}