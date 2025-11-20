//cpp_ai_test/modules/VisualizationModule.cpp
#include "VisualizationModule.hpp"
#include <algorithm>

// Обновленный конструктор
VisualizationModule::VisualizationModule(const VisualizationConfig& config, int Nside, int visualizationWidth, int visualizationHeight)
    : config(config), Nside(Nside), 
      cellSize(std::min(visualizationWidth, visualizationHeight) / float(Nside)), // Используем минимальный размер для квадратных ячеек
      min_phi(-1.0), max_phi(1.0) {}

void VisualizationModule::draw(sf::RenderWindow& window, const NeuralFieldSystem& system) {
    const auto& phi = system.getPhi();
    
    for (int y = 0; y < Nside; y++) {
        for (int x = 0; x < Nside; x++) {
            int idx = y * Nside + x;
            double value = phi[idx];
            double norm = config.dynamic_normalization ? 
                         (value - min_phi) / (max_phi - min_phi) :
                         (value + 0.2) / 0.4;
            
            if (norm < 0) norm = 0;
            if (norm > 1) norm = 1;
            
            sf::RectangleShape cell(sf::Vector2f(cellSize, cellSize));
            cell.setPosition(sf::Vector2f(x * cellSize, y * cellSize));
            cell.setFillColor(getColor(norm));
            window.draw(cell);
        }
    }
}

void VisualizationModule::updateDynamicRange(const NeuralFieldSystem& system) {
    if (!config.dynamic_normalization) return;
    
    const auto& phi = system.getPhi();
    double current_min = phi[0], current_max = phi[0];
    
    for (int i = 1; i < system.N; i++) {
        if (phi[i] < current_min) current_min = phi[i];
        if (phi[i] > current_max) current_max = phi[i];
    }
    
    min_phi = 0.99 * min_phi + 0.01 * current_min;
    max_phi = 0.99 * max_phi + 0.01 * current_max;
    
    if (max_phi - min_phi < config.min_range) {
        double center = (min_phi + max_phi) / 2;
        min_phi = center - config.min_range / 2;
        max_phi = center + config.min_range / 2;
    }
}

sf::Color VisualizationModule::getColor(double norm) const {
    if (config.color_scheme == "blue_red") {
        if (norm < 0.5) {
            double intensity = norm * 2.0;
            return sf::Color(0, 0, static_cast<unsigned char>(intensity * 255));
        } else {
            double intensity = (norm - 0.5) * 2.0;
            return sf::Color(static_cast<unsigned char>(intensity * 255), 0, 0);
        }
    } else {
        return sf::Color(
            static_cast<unsigned char>(norm * 255),
            0,
            static_cast<unsigned char>((1.0 - norm) * 255)
        );
    }
}