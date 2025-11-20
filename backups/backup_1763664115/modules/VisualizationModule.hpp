//cpp_ai_test/modules/VisualizationModule.hpp
#pragma once
#include <SFML/Graphics.hpp>
#include "../core/NeuralFieldSystem.hpp"
#include <vector>

struct VisualizationConfig {
    bool enabled = true;
    bool dynamic_normalization = true;
    std::string color_scheme = "blue_red";
    double min_range = 0.1;
};

class VisualizationModule {
public:
    // Обновленный конструктор с шириной и высотой
    VisualizationModule(const VisualizationConfig& config, int Nside, int visualizationWidth, int visualizationHeight);
    
    void draw(sf::RenderWindow& window, const NeuralFieldSystem& system);
    void updateDynamicRange(const NeuralFieldSystem& system);

private:
    VisualizationConfig config;
    const int Nside;
    const float cellSize;
    double min_phi, max_phi;
    
    sf::Color getColor(double value) const;
};