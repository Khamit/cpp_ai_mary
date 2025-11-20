//cpp_ai_test/modules/InteractionModule.cpp
#include "InteractionModule.hpp"
#include <cmath>
#include <iostream>

InteractionModule::InteractionModule(const InteractionConfig& config, int Nside, float cellSize)
    : config(config), Nside(Nside), cellSize(cellSize) {}

void InteractionModule::handleMouseClick(const sf::Event::MouseButtonPressed& event, NeuralFieldSystem& system) {
    // Преобразуем координаты мыши в координаты ячейки
    int cellX = static_cast<int>(event.position.x / cellSize);
    int cellY = static_cast<int>(event.position.y / cellSize);
    
    if (cellX >= 0 && cellX < Nside && cellY >= 0 && cellY < Nside) {
        int idx = cellY * Nside + cellX;
        auto& phi = system.getPhi();
        phi[idx] += config.mouse_impact;
        
        std::cout << "Mouse click at cell (" << cellX << ", " << cellY 
                  << ") index " << idx << " - phi increased by " << config.mouse_impact << std::endl;
        
        // Локальное распространение
        if (config.local_spread) {
            for (int dy = -config.spread_radius; dy <= config.spread_radius; dy++) {
                for (int dx = -config.spread_radius; dx <= config.spread_radius; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    
                    int nx = cellX + dx;
                    int ny = cellY + dy;
                    
                    if (nx >= 0 && nx < Nside && ny >= 0 && ny < Nside) {
                        int nidx = ny * Nside + nx;
                        double distance = std::sqrt(dx*dx + dy*dy);
                        double impact = config.mouse_impact / (distance * distance + 1); // +1 чтобы избежать деления на 0
                        phi[nidx] += impact;
                    }
                }
            }
        }
    }
}