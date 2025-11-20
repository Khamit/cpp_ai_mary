//cpp_ai_test/modules/InteractionModule.hpp
#pragma once
#include "../core/NeuralFieldSystem.hpp"
#include <SFML/Window/Event.hpp>

struct InteractionConfig {
    bool enabled = true;
    double mouse_impact = 0.1;
    bool local_spread = true;
    int spread_radius = 2;
};

class InteractionModule {
public:
    InteractionModule(const InteractionConfig& config, int Nside, float cellSize);
    
    // Этот метод тоже принимает НЕконстантную ссылку, так как изменяет систему
    void handleMouseClick(const sf::Event::MouseButtonPressed& event, NeuralFieldSystem& system);

private:
    InteractionConfig config;
    const int Nside;
    const float cellSize;
};