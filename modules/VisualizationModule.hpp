#pragma once
#include "../core/NeuralFieldSystem.hpp"
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <cstdint>
struct VisualizationConfig {
    bool enabled = true;
    bool dynamic_normalization = true;
    std::string color_scheme = "blue_red";
    double min_range = 0.1;
};

class VisualizationModule {
public:
    VisualizationModule(const VisualizationConfig& config, int numGroups, int groupSize, int width, int height)
        : config(config), numGroups(numGroups), groupSize(groupSize), width(width), height(height) {
        
        cellSize = static_cast<float>(width) / numGroups;
        
        // Создаем прямоугольники для каждой группы
        rectangles.resize(numGroups * numGroups);
        for (int y = 0; y < numGroups; ++y) {
            for (int x = 0; x < numGroups; ++x) {
                int idx = y * numGroups + x;
                rectangles[idx].setSize(sf::Vector2f(cellSize - 1, cellSize - 1));
                rectangles[idx].setPosition(sf::Vector2f(x * cellSize, y * cellSize));
            }
        }
    }
    
    void updateDynamicRange(const NeuralFieldSystem& system) {
        // Вычисляем мин/макс активности
        const auto& groups = system.getGroups();
        minActivity = 1.0;
        maxActivity = 0.0;
        
        for (const auto& group : groups) {
            double avg = group.getAverageActivity();
            minActivity = std::min(minActivity, avg);
            maxActivity = std::max(maxActivity, avg);
        }
        
        if (maxActivity - minActivity < config.min_range) {
            maxActivity = minActivity + config.min_range;
        }
    }
    
    void draw(sf::RenderWindow& window, const NeuralFieldSystem& system) {
        if (!config.enabled) return;
        
        const auto& groups = system.getGroups();
        
        for (int g = 0; g < groups.size(); ++g) {
            double activity = groups[g].getAverageActivity();
            
            // Нормализуем активность для цвета
            float norm = (activity - minActivity) / (maxActivity - minActivity);
            norm = std::clamp(norm, 0.0f, 1.0f);
            
            // Выбираем цвет
            sf::Color color;
            if (config.color_scheme == "blue_red") {
                std::uint8_t r = static_cast<std::uint8_t>(norm * 255);
                std::uint8_t b = static_cast<std::uint8_t>((1.0f - norm) * 255);
                color = sf::Color(r, 0, b);
            } else {
                std::uint8_t val = static_cast<std::uint8_t>(norm * 255);
                color = sf::Color(val, val, val);
            }
            
            rectangles[g].setFillColor(color);
            window.draw(rectangles[g]);
        }
    }
    
    void setConfig(const VisualizationConfig& newConfig) { config = newConfig; }

private:
    VisualizationConfig config;
    int numGroups;
    int groupSize;
    int width;
    int height;
    float cellSize;
    std::vector<sf::RectangleShape> rectangles;
    double minActivity = 0.0;
    double maxActivity = 1.0;
};