//cpp_ai_test/modules/VisualizationModule.hpp
#pragma once
#include <SFML/Graphics.hpp>
#include "../core/NeuralFieldSystem.hpp"
#include <vector>
#include "lang/LanguageModule.hpp"

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
    // язык
    void setLanguageModule(LanguageModule* module) {
        languageModule = module;
    }
private:
    VisualizationConfig config;
    const int Nside;
    const float cellSize;
    double min_phi, max_phi;

    LanguageModule* languageModule = nullptr; // - язык
    
    sf::Color getColor(double value) const;
    
};