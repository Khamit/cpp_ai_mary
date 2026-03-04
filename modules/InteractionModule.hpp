#pragma once
#include "../core/NeuralFieldSystem.hpp"
#include <SFML/Graphics.hpp>

struct InteractionConfig {
    bool enabled = true;
    double mouse_impact = 0.1;
    bool local_spread = true;
    int spread_radius = 2;
};

class InteractionModule {
public:
    InteractionModule(const InteractionConfig& config, int numGroups, float cellSize) 
        : config(config), numGroups(numGroups), cellSize(cellSize) {}
    
    void handleMouseClick(const sf::Event::MouseButtonPressed& mousePressed, NeuralFieldSystem& system) {
        if (!config.enabled) return;
        
        int mouseX = mousePressed.position.x;
        int mouseY = mousePressed.position.y;
        
        int groupX = static_cast<int>(mouseX / cellSize);
        int groupY = static_cast<int>(mouseY / cellSize);
        int groupIndex = groupY * numGroups + groupX;
        
        if (groupIndex >= 0 && groupIndex < numGroups) {
            // Влияем на группу через межгрупповые связи
            system.strengthenInterConnection(31, groupIndex, config.mouse_impact);
        }
    }
    
    void setConfig(const InteractionConfig& newConfig) { config = newConfig; }

private:
    InteractionConfig config;
    int numGroups;
    float cellSize;
};