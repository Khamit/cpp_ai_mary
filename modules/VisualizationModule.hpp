#pragma once
#include "../core/NeuralFieldSystem.hpp"
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <iostream>

struct VisualizationConfig {
    bool enabled = true;
    bool show_orbits = true;
    bool show_connections = true;
    bool show_neurons = true;
    bool dynamic_normalization = true;
    std::string color_scheme = "blue_red";
    double min_range = 0.1;
    float orbit_scale = 100.0f;
    float neuron_radius = 3.0f;
    float zoom_level = 1.0f;
    float pan_x = 0.0f;
    float pan_y = 0.0f;
    bool show_intra_connections = true;
    float connection_threshold = 0.1f;
    // 3D параметры
    float rotation_angle = 0.0f;      // угол поворота камеры
    float tilt_angle = 30.0f;         // наклон камеры (в градусах)
    bool auto_rotate = false;          // автоматическое вращение
};

class VisualizationModule {
public:
    VisualizationModule(const VisualizationConfig& config, int numGroups, int groupSize, int width, int height)
        : config(config), numGroups(numGroups), groupSize(groupSize), width(width), height(height) {
        
        centerX = width / 2.0f;
        centerY = height / 2.0f;
        
        groupOrbitAngles.resize(numGroups);
        groupOrbitRadii.resize(numGroups);
        
        precomputeGroupPositions();
        
        // Создаем нейроны для каждой группы
        neurons.resize(numGroups);
        for (int g = 0; g < numGroups; ++g) {
            neurons[g].resize(groupSize);
            for (int n = 0; n < groupSize; ++n) {
                neurons[g][n].setRadius(config.neuron_radius);
                neurons[g][n].setOrigin(sf::Vector2f(config.neuron_radius, config.neuron_radius));
            }
        }
        
        // Создаем орбиты (окружности)
        orbits.resize(numGroups);
        for (int g = 0; g < numGroups; ++g) {
            if (g < groupOrbitRadii.size()) {
                orbits[g].setRadius(groupOrbitRadii[g]);
                orbits[g].setOrigin(sf::Vector2f(groupOrbitRadii[g], groupOrbitRadii[g]));
                orbits[g].setPosition(sf::Vector2f(centerX + config.pan_x, centerY + config.pan_y));
                orbits[g].setFillColor(sf::Color::Transparent);
                orbits[g].setOutlineColor(sf::Color(100, 100, 150, 100));
                orbits[g].setOutlineThickness(1.0f);
            }
        }
        
        // Создаем центральную сингулярность
        singularity.setRadius(10.0f);
        singularity.setOrigin(sf::Vector2f(10.0f, 10.0f));
        singularity.setPosition(sf::Vector2f(centerX + config.pan_x, centerY + config.pan_y));
        singularity.setFillColor(sf::Color(255, 100, 100, 200));
        
        // Создаем орбитальные зоны
        orbitalZones.resize(3);
        
        float zone_radii[] = {
            (orbital_radius - orbital_width/2),
            orbital_radius,
            (orbital_radius + orbital_width/2)
        };
        
        for (int i = 0; i < 3; ++i) {
            orbitalZones[i].setRadius(zone_radii[i]);
            orbitalZones[i].setOrigin(sf::Vector2f(zone_radii[i], zone_radii[i]));
            orbitalZones[i].setPosition(sf::Vector2f(centerX + config.pan_x, centerY + config.pan_y));
            orbitalZones[i].setFillColor(sf::Color::Transparent);
            orbitalZones[i].setOutlineColor(sf::Color(100, 200, 255, 50 + i*50));
            orbitalZones[i].setOutlineThickness(2.0f);
        }
    }
    
    void updateDynamicRange(const NeuralFieldSystem& system) {
        if (!config.dynamic_normalization) return;
        
        const auto& groups = system.getGroups();
        if (groups.empty()) return;
        
        double current_min = groups[0].getAverageActivity();
        double current_max = current_min;
        
        for (size_t i = 1; i < groups.size(); ++i) {
            double avg = groups[i].getAverageActivity();
            current_min = std::min(current_min, avg);
            current_max = std::max(current_max, avg);
        }
        
        minActivity = 0.99 * minActivity + 0.01 * current_min;
        maxActivity = 0.99 * maxActivity + 0.01 * current_max;
        
        if (maxActivity - minActivity < config.min_range) {
            double center = 0.5 * (minActivity + maxActivity);
            minActivity = center - config.min_range * 0.5;
            maxActivity = center + config.min_range * 0.5;
        }
    }
    
    void handleZoom(float delta) {
        config.zoom_level *= (1.0f + delta * 0.1f);
        config.zoom_level = std::clamp(config.zoom_level, 0.5f, 3.0f);
        precomputeGroupPositions();
    }
    
    void handlePan(float dx, float dy) {
        config.pan_x += dx;
        config.pan_y += dy;
        precomputeGroupPositions();
    }
    
    void handleRotate(float delta) {
        config.rotation_angle += delta;
        if (config.rotation_angle > 360.0f) config.rotation_angle -= 360.0f;
        if (config.rotation_angle < 0.0f) config.rotation_angle += 360.0f;
    }
    
    void handleTilt(float delta) {
        config.tilt_angle += delta;
        config.tilt_angle = std::clamp(config.tilt_angle, 0.0f, 90.0f);
    }
    
    void toggleAutoRotate() {
        config.auto_rotate = !config.auto_rotate;
    }
    
    void resetView() {
        config.zoom_level = 1.0f;
        config.pan_x = 0.0f;
        config.pan_y = 0.0f;
        config.rotation_angle = 0.0f;
        config.tilt_angle = 30.0f;
        precomputeGroupPositions();
    }
    
    void draw(sf::RenderWindow& window, const NeuralFieldSystem& system) {
        if (!config.enabled) return;
        
        const auto& groups = system.getGroups();
        
        // Автоматическое вращение
        static sf::Clock rotationClock;
        if (config.auto_rotate) {
            float dt = rotationClock.restart().asSeconds();
            config.rotation_angle += dt * 10.0f;
            if (config.rotation_angle > 360.0f) config.rotation_angle -= 360.0f;
        }
        
        // Применяем трансформацию для всего вида
        sf::View view = window.getView();
        view.setCenter(sf::Vector2f(centerX + config.pan_x, centerY + config.pan_y));
        view.setSize(sf::Vector2f(width / config.zoom_level, height / config.zoom_level));
        window.setView(view);
        
        // Конвертируем углы в радианы
        float rot_rad = config.rotation_angle * M_PI / 180.0f;
        float tilt_rad = config.tilt_angle * M_PI / 180.0f;
        float cos_tilt = std::cos(tilt_rad);
        float sin_tilt = std::sin(tilt_rad);
        
        // 1. Рисуем орбитальные зоны
        if (config.show_orbits) {
            for (const auto& zone : orbitalZones) {
                window.draw(zone);
            }
        }
        
        // 2. Рисуем орбиты групп
        if (config.show_orbits) {
            for (int g = 0; g < numGroups && g < orbits.size(); ++g) {
                window.draw(orbits[g]);
            }
        }
        
        // 3. Рисуем центральную сингулярность
        window.draw(singularity);
        
        // 4. Рисуем связи между группами
        if (config.show_connections) {
            drawConnections3D(window, system, rot_rad, tilt_rad, cos_tilt, sin_tilt);
        }
        
        // 5. Рисуем внутригрупповые связи
        if (config.show_intra_connections) {
            drawIntraGroupConnections3D(window, system, rot_rad, tilt_rad, cos_tilt, sin_tilt);
        }
        
        // 6. Рисуем нейроны в 3D
        if (config.show_neurons) {
            for (int g = 0; g < numGroups && g < groups.size(); ++g) {
                const auto& group = groups[g];
                
                // Получаем 3D позиции нейронов
                const auto& positions = group.getPositions();
                
                for (int n = 0; n < groupSize && n < neurons[g].size() && n < positions.size(); ++n) {
                    // Проецируем 3D точку на 2D экран
                    sf::Vector2f screenPos = project3DTo2D(
                        positions[n].x, positions[n].y, positions[n].z,
                        rot_rad, tilt_rad, cos_tilt, sin_tilt
                    );
                    
                    // Масштабируем и добавляем смещение группы
                    screenPos.x = centerX + config.pan_x + screenPos.x * config.orbit_scale;
                    screenPos.y = centerY + config.pan_y + screenPos.y * config.orbit_scale;
                    
                    neurons[g][n].setPosition(screenPos);
                    
                    // Вычисляем активность (расстояние от центра)
                    double activity = positions[n].norm();
                    float norm = static_cast<float>((activity - minActivity) / (maxActivity - minActivity));
                    norm = std::clamp(norm, 0.0f, 1.0f);
                    
                    // Проверяем, находится ли нейрон в орбитальной зоне
                    bool inOrbit = (activity > 0.2 && activity < 0.8);
                    
                    // Цвет
                    sf::Color color;
                    if (inOrbit) {
                        color = sf::Color(100, 255, 200, 255);
                    } else {
                        if (config.color_scheme == "blue_red") {
                            std::uint8_t r_color = static_cast<std::uint8_t>(norm * 255);
                            std::uint8_t b = static_cast<std::uint8_t>((1.0f - norm) * 255);
                            color = sf::Color(r_color, 0, b);
                        } else {
                            std::uint8_t val = static_cast<std::uint8_t>(norm * 255);
                            color = sf::Color(val, val, val);
                        }
                    }
                    
                    // Размер зависит от близости к камере (z-координата)
                    float depthFactor = 0.5f + 0.5f * (positions[n].z + 1.0f) / 2.0f;
                    float size = config.neuron_radius * (0.5f + norm * 0.5f) * depthFactor;
                    
                    neurons[g][n].setRadius(size);
                    neurons[g][n].setOrigin(sf::Vector2f(size, size));
                    neurons[g][n].setFillColor(color);
                    
                    window.draw(neurons[g][n]);
                }
            }
        }
        
        window.setView(window.getDefaultView());
    }
    
    void setConfig(const VisualizationConfig& newConfig) { 
        config = newConfig; 
        precomputeGroupPositions();
    }
    
    VisualizationConfig& getConfig() { return config; }
    
private:
    // Проекция 3D точки на 2D экран
    sf::Vector2f project3DTo2D(float x, float y, float z, 
                               float rot_rad, float tilt_rad, 
                               float cos_tilt, float sin_tilt) const {
        // Поворот вокруг оси Y (rotation)
        float x_rot = x * std::cos(rot_rad) + z * std::sin(rot_rad);
        float z_rot = -x * std::sin(rot_rad) + z * std::cos(rot_rad);
        float y_rot = y;
        
        // Наклон (tilt) - поворот вокруг оси X
        float y_tilt = y_rot * cos_tilt - z_rot * sin_tilt;
        float z_tilt = y_rot * sin_tilt + z_rot * cos_tilt;
        float x_tilt = x_rot;
        
        // Простая перспективная проекция
        float perspective = 1.0f / (2.0f - z_tilt * 0.5f);
        
        return sf::Vector2f(x_tilt * perspective, y_tilt * perspective);
    }
    
    void precomputeGroupPositions() {
        orbital_radius = 0.5f * config.orbit_scale * config.zoom_level;
        orbital_width = 0.3f * config.orbit_scale * config.zoom_level;
        
        for (int g = 0; g < numGroups; ++g) {
            groupOrbitAngles[g] = 2.0f * static_cast<float>(M_PI) * g / numGroups;
            
            if (g >= 16 && g <= 21) {
                groupOrbitRadii[g] = 0.4f * config.orbit_scale * config.zoom_level;
            } else {
                groupOrbitRadii[g] = 0.8f * config.orbit_scale * config.zoom_level;
            }
        }
        
        if (!orbits.empty()) {
            for (int g = 0; g < numGroups && g < orbits.size(); ++g) {
                orbits[g].setRadius(groupOrbitRadii[g]);
                orbits[g].setOrigin(sf::Vector2f(groupOrbitRadii[g], groupOrbitRadii[g]));
                orbits[g].setPosition(sf::Vector2f(centerX + config.pan_x, centerY + config.pan_y));
            }
        }
        
        float zone_radii[] = {
            (orbital_radius - orbital_width/2),
            orbital_radius,
            (orbital_radius + orbital_width/2)
        };
        
        if (!orbitalZones.empty()) {
            for (int i = 0; i < 3 && i < orbitalZones.size(); ++i) {
                orbitalZones[i].setRadius(zone_radii[i]);
                orbitalZones[i].setOrigin(sf::Vector2f(zone_radii[i], zone_radii[i]));
                orbitalZones[i].setPosition(sf::Vector2f(centerX + config.pan_x, centerY + config.pan_y));
            }
        }
        
        singularity.setPosition(sf::Vector2f(centerX + config.pan_x, centerY + config.pan_y));
    }
    
    void drawConnections3D(sf::RenderWindow& window, const NeuralFieldSystem& system,
                          float rot_rad, float tilt_rad, float cos_tilt, float sin_tilt) {
        const auto& groups = system.getGroups();
        const auto& interWeights = system.getInterWeights();
        
        for (int g1 = 0; g1 < numGroups; ++g1) {
            for (int g2 = g1 + 1; g2 < numGroups; ++g2) {
                double weight = interWeights[g1][g2];
                if (std::abs(weight) > 0.1) {
                    
                    // Находим центр группы как среднее положение нейронов
                    sf::Vector2f pos1(0, 0), pos2(0, 0);
                    int count1 = 0, count2 = 0;
                    
                    const auto& group1 = groups[g1];
                    const auto& group2 = groups[g2];
                    const auto& pos3d1 = group1.getPositions();
                    const auto& pos3d2 = group2.getPositions();
                    
                    for (int n = 0; n < groupSize && n < pos3d1.size(); ++n) {
                        sf::Vector2f p = project3DTo2D(
                            pos3d1[n].x, pos3d1[n].y, pos3d1[n].z,
                            rot_rad, tilt_rad, cos_tilt, sin_tilt
                        );
                        pos1.x += p.x;
                        pos1.y += p.y;
                        count1++;
                    }
                    
                    for (int n = 0; n < groupSize && n < pos3d2.size(); ++n) {
                        sf::Vector2f p = project3DTo2D(
                            pos3d2[n].x, pos3d2[n].y, pos3d2[n].z,
                            rot_rad, tilt_rad, cos_tilt, sin_tilt
                        );
                        pos2.x += p.x;
                        pos2.y += p.y;
                        count2++;
                    }
                    
                    if (count1 > 0 && count2 > 0) {
                        pos1.x /= static_cast<float>(count1);
                        pos1.y /= static_cast<float>(count1);
                        pos2.x /= static_cast<float>(count2);
                        pos2.y /= static_cast<float>(count2);
                        
                        // Масштабируем и добавляем смещение
                        pos1.x = centerX + config.pan_x + pos1.x * config.orbit_scale;
                        pos1.y = centerY + config.pan_y + pos1.y * config.orbit_scale;
                        pos2.x = centerX + config.pan_x + pos2.x * config.orbit_scale;
                        pos2.y = centerY + config.pan_y + pos2.y * config.orbit_scale;
                        
                        sf::Vertex v1, v2;
                        v1.position = pos1;
                        v1.color = sf::Color(100, 100, 255, 
                                            static_cast<std::uint8_t>(std::abs(weight) * 200));
                        v2.position = pos2;
                        v2.color = sf::Color(100, 100, 255, 
                                            static_cast<std::uint8_t>(std::abs(weight) * 200));
                        
                        sf::Vertex line[] = {v1, v2};
                        window.draw(line, 2, sf::PrimitiveType::Lines);
                    }
                }
            }
        }
    }
    
    void drawIntraGroupConnections3D(sf::RenderWindow& window, const NeuralFieldSystem& system,
                                    float rot_rad, float tilt_rad, float cos_tilt, float sin_tilt) {
        const auto& groups = system.getGroups();
        
        for (int g = 0; g < numGroups && g < groups.size(); ++g) {
            const auto& group = groups[g];
            const auto& synapses = group.getSynapsesConst();
            const auto& positions = group.getPositions();
            
            int synIndex = 0;
            for (int i = 0; i < groupSize; ++i) {
                for (int j = i + 1; j < groupSize; ++j) {
                    if (synIndex < synapses.size()) {
                        float weight = synapses[synIndex].weight;
                        
                        if (std::abs(weight) > config.connection_threshold) {
                            // Проецируем позиции нейронов
                            sf::Vector2f pos_i = project3DTo2D(
                                positions[i].x, positions[i].y, positions[i].z,
                                rot_rad, tilt_rad, cos_tilt, sin_tilt
                            );
                            
                            sf::Vector2f pos_j = project3DTo2D(
                                positions[j].x, positions[j].y, positions[j].z,
                                rot_rad, tilt_rad, cos_tilt, sin_tilt
                            );
                            
                            // Масштабируем
                            pos_i.x = centerX + config.pan_x + pos_i.x * config.orbit_scale;
                            pos_i.y = centerY + config.pan_y + pos_i.y * config.orbit_scale;
                            pos_j.x = centerX + config.pan_x + pos_j.x * config.orbit_scale;
                            pos_j.y = centerY + config.pan_y + pos_j.y * config.orbit_scale;
                            
                            sf::Color lineColor;
                            if (weight > 0) {
                                std::uint8_t alpha = static_cast<std::uint8_t>(std::min(std::abs(weight) * 255.0f, 200.0f));
                                lineColor = sf::Color(255, 100, 100, alpha);
                            } else {
                                std::uint8_t alpha = static_cast<std::uint8_t>(std::min(std::abs(weight) * 255.0f, 200.0f));
                                lineColor = sf::Color(100, 100, 255, alpha);
                            }
                            
                            sf::Vertex line[2];
                            line[0].position = pos_i;
                            line[0].color = lineColor;
                            line[1].position = pos_j;
                            line[1].color = lineColor;
                            window.draw(line, 2, sf::PrimitiveType::Lines);
                        }
                        synIndex++;
                    }
                }
            }
        }
    }
    
    VisualizationConfig config;
    int numGroups, groupSize;
    int width, height;
    float centerX, centerY;
    double minActivity = 0.0;
    double maxActivity = 1.0;
    
    float orbital_radius = 50.0f;
    float orbital_width = 30.0f;
    
    std::vector<std::vector<sf::CircleShape>> neurons;
    std::vector<sf::CircleShape> orbits;
    std::vector<sf::CircleShape> orbitalZones;
    sf::CircleShape singularity;
    
    std::vector<float> groupOrbitAngles;
    std::vector<float> groupOrbitRadii;
};