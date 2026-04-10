#pragma once
#include "../core/NeuralFieldSystem.hpp"
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <map>
#include <sstream>
#include <iomanip> 
#include <filesystem>  // для current_path

struct VisualizationConfig {
    bool enabled = true;
    bool show_orbits = true;
    bool show_connections = true;
    bool show_neurons = true;
    bool dynamic_normalization = true;
    std::string color_scheme = "blue_red";
    double min_range = 0.1;
    float neuron_radius = 3.0f;
    float zoom_level = 1.0f;
    float pan_x = 0.0f;
    float pan_y = 0.0f;
    bool show_intra_connections = true;
    float connection_threshold = 0.1f;

    // температура Джекичанов
    bool show_mass = true;
    bool show_energy = false;
    bool show_temperature = false;
    bool show_legend = true;
    bool show_stats = true;

    // 3D параметры
    float rotation_angle = 0.0f;
    float tilt_angle = 30.0f;
    bool auto_rotate = false;

    // НОВЫЕ ПАРАМЕТРЫ ДЛЯ МЕТОДА ТЬЮРИНГА
    bool show_diffusion = true;      // показывать диффузионные связи
    bool show_inhibitor = true;      // показывать ингибиторное поле
    float inhibitor_alpha = 0.3f;    // прозрачность ингибитора
    
    // НОВЫЕ ПАРАМЕТРЫ ДЛЯ ОРБИТ
    bool show_orbit_spheres = true;           // показывать сферы орбит
    bool color_by_orbit = true;                // раскрашивать по орбитам
    float orbit_scale = 100.0f;                 // масштаб орбит
    std::vector<float> orbit_radii = {0.2f, 0.5f, 1.0f, 1.5f, 2.0f};  // радиусы орбит
    std::vector<sf::Color> orbit_colors = {
        sf::Color(100, 100, 100, 50),   // сингулярность - серый
        sf::Color(100, 150, 255, 50),   // внутренняя - голубой
        sf::Color(100, 255, 100, 50),   // базовая - зеленый
        sf::Color(255, 200, 100, 50),   // средняя - оранжевый
        sf::Color(255, 100, 100, 50)    // внешняя - красный
    };
};

class VisualizationModule {
public:
    VisualizationModule(const VisualizationConfig& config, int numGroups, int groupSize, int width, int height)
        : config(config), numGroups(numGroups), groupSize(groupSize), width(width), height(height) {
        
        centerX = width / 2.0f;
        centerY = height / 2.0f;
        
        // Загружаем шрифт - ИСПРАВЛЕНО: loadFromFile -> openFromFile
        fontLoaded = font.openFromFile("SF-Pro-Display-Regular.otf");
        if (!fontLoaded) {
            std::cerr << "Warning: Could not load font SF-Pro-Display-Regular.otf" << std::endl;
            std::cerr << "Current working directory: " << std::filesystem::current_path() << std::endl;
            
            // Пробуем запасной вариант
            fontLoaded = font.openFromFile("/System/Library/Fonts/Helvetica.ttc");
            if (!fontLoaded) {
                std::cerr << "Warning: Could not load fallback font Helvetica.ttc" << std::endl;
            }
        }
        
        initializeOrbitSpheres();
        
        neurons.resize(numGroups);
        for (int g = 0; g < numGroups; ++g) {
            neurons[g].resize(groupSize);
            for (int n = 0; n < groupSize; ++n) {
                neurons[g][n].setRadius(config.neuron_radius);
                neurons[g][n].setOrigin(sf::Vector2f(config.neuron_radius, config.neuron_radius));
            }
        }
        
        singularity.setRadius(10.0f);
        singularity.setOrigin(sf::Vector2f(10.0f, 10.0f));
        singularity.setFillColor(sf::Color(255, 100, 100, 200));
    }
    
    // НОВЫЙ МЕТОД: обновить радиусы орбит из ядра (теперь отдельный метод)
    void updateOrbitRadiiFromSystem(const NeuralFieldSystem& system) {
        if (system.getGroups().empty()) return;
        
        const auto& firstGroup = system.getGroups()[0];
        
        // Используем статические методы, которые нужно добавить в NeuralGroup
        real_radii_[0] = static_cast<float>(firstGroup.getOrbitRadius(0));
        real_radii_[1] = static_cast<float>(firstGroup.getOrbitRadius(1));
        real_radii_[2] = static_cast<float>(firstGroup.getOrbitRadius(2));
        real_radii_[3] = static_cast<float>(firstGroup.getOrbitRadius(3));
        real_radii_[4] = static_cast<float>(firstGroup.getOrbitRadius(4));
        
        initializeOrbitSpheres();
    }
    
    void initializeOrbitSpheres() {
        orbitSpheres.resize(5);
        
        for (int level = 0; level < 5; ++level) {
            float radius = real_radii_[level] * config.orbit_scale * config.zoom_level;
            
            orbitSpheres[level].setRadius(radius);
            orbitSpheres[level].setOrigin(sf::Vector2f(radius, radius));
            orbitSpheres[level].setFillColor(sf::Color::Transparent);
            orbitSpheres[level].setOutlineColor(config.orbit_colors[level]);
            orbitSpheres[level].setOutlineThickness(2.0f);
        }
    }
    
    void updateDynamicRange(const NeuralFieldSystem& system) {
        if (!config.dynamic_normalization) return;
        
        const auto& groups = system.getGroups();
        if (groups.empty()) return;
        
        double current_min = 1e9;
        double current_max = -1e9;
        
        for (size_t g = 0; g < groups.size(); ++g) {
            const auto& positions = groups[g].getPositions();
            for (size_t i = 0; i < positions.size(); ++i) {
                double r = positions[i].norm();
                current_min = std::min(current_min, r);
                current_max = std::max(current_max, r);
            }
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
        initializeOrbitSpheres();  // пересчитываем радиусы орбит
    }
    
    void handlePan(float dx, float dy) {
        config.pan_x += dx;
        config.pan_y += dy;
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
        initializeOrbitSpheres();
    }

void drawDiffusionConnections(sf::RenderWindow& window, const NeuralFieldSystem& system,
                                                   float rot_rad, float tilt_rad, float cos_tilt, float sin_tilt) {
    const auto& groups = system.getGroups();
    
    for (int g = 0; g < numGroups && g < groups.size(); ++g) {
        const auto& group = groups[g];
        const auto& positions = group.getPositions();
        
        for (int i = 0; i < groupSize; ++i) {
            sf::Vector2f pos_i = project3DTo2D(
                positions[i].x, positions[i].y, positions[i].z,
                rot_rad, tilt_rad, cos_tilt, sin_tilt
            );
            pos_i.x = centerX + config.pan_x + pos_i.x * config.orbit_scale;
            pos_i.y = centerY + config.pan_y + pos_i.y * config.orbit_scale;
            
            for (int j = i + 1; j < groupSize; ++j) {
                double weight = group.getWeight(i, j);
                if (std::abs(weight) > 0.1) {
                    sf::Vector2f pos_j = project3DTo2D(
                        positions[j].x, positions[j].y, positions[j].z,
                        rot_rad, tilt_rad, cos_tilt, sin_tilt
                    );
                    pos_j.x = centerX + config.pan_x + pos_j.x * config.orbit_scale;
                    pos_j.y = centerY + config.pan_y + pos_j.y * config.orbit_scale;
                    
                    sf::Color lineColor(100, 255, 100, static_cast<std::uint8_t>(std::abs(weight) * 150));
                    
                    sf::Vertex line[2];
                    line[0].position = pos_i;
                    line[0].color = lineColor;
                    line[1].position = pos_j;
                    line[1].color = lineColor;
                    
                    window.draw(line, 2, sf::PrimitiveType::Lines);
                }
            }
        }
    }
}

void drawInhibitorField(sf::RenderWindow& window, const NeuralFieldSystem& system,
                                             float rot_rad, float tilt_rad, float cos_tilt, float sin_tilt) {
    const auto& groups = system.getGroups();
    
    for (int g = 0; g < numGroups && g < groups.size(); ++g) {
        const auto& group = groups[g];
        const auto& positions = group.getPositions();
        const auto& inhibitor = group.getInhibitor(); // нужно добавить геттер в NeuralGroup
        
        for (int i = 0; i < groupSize; ++i) {
            // Проецируем позицию нейрона
            sf::Vector2f pos = project3DTo2D(
                positions[i].x, positions[i].y, positions[i].z,
                rot_rad, tilt_rad, cos_tilt, sin_tilt
            );
            pos.x = centerX + config.pan_x + pos.x * config.orbit_scale;
            pos.y = centerY + config.pan_y + pos.y * config.orbit_scale;
            
            // Получаем значение ингибитора (0-1)
            float inhibitor_value = static_cast<float>(std::min(1.0, inhibitor[i] / 2.0));
            
            // Создаем "ореол" вокруг нейрона, показывающий силу ингибитора
            float radius = config.neuron_radius * (1.0f + inhibitor_value * 2.0f);
            sf::CircleShape halo(radius);
            halo.setOrigin(sf::Vector2f(radius, radius));
            halo.setPosition(pos);
            
            // Цвет: от желтого (слабый) к красному (сильный)
            std::uint8_t r = static_cast<std::uint8_t>(200 + inhibitor_value * 55);
            std::uint8_t g = static_cast<std::uint8_t>(200 - inhibitor_value * 150);
            std::uint8_t b = static_cast<std::uint8_t>(100 - inhibitor_value * 80);
            
            halo.setFillColor(sf::Color(r, g, b, static_cast<std::uint8_t>(config.inhibitor_alpha * 255)));
            window.draw(halo);
        }
    }
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
        
        // 1. Рисуем орбитальные сферы (5 штук)
        if (config.show_orbits && config.show_orbit_spheres) {
            for (int level = 0; level < 5; ++level) {
                sf::CircleShape& sphere = orbitSpheres[level];
                
                // Проецируем центр сферы (0,0,0) в 2D
                sf::Vector2f centerPos = project3DTo2D(0, 0, 0, rot_rad, tilt_rad, cos_tilt, sin_tilt);
                centerPos.x = centerX + config.pan_x + centerPos.x;
                centerPos.y = centerY + config.pan_y + centerPos.y;
                
                sphere.setPosition(centerPos);
                window.draw(sphere);
            }
        }
        
        // 2. Рисуем центральную сингулярность
        sf::Vector2f singularityPos = project3DTo2D(0, 0, 0, rot_rad, tilt_rad, cos_tilt, sin_tilt);
        singularityPos.x = centerX + config.pan_x + singularityPos.x;
        singularityPos.y = centerY + config.pan_y + singularityPos.y;
        singularity.setPosition(singularityPos);
        window.draw(singularity);
        
        // 3. Рисуем связи между группами
        if (config.show_connections) {
            drawConnections3D(window, system, rot_rad, tilt_rad, cos_tilt, sin_tilt);
        }
        
        // 4. Рисуем внутригрупповые связи
        if (config.show_intra_connections) {
            drawIntraGroupConnections3D(window, system, rot_rad, tilt_rad, cos_tilt, sin_tilt);
        }

        // 5.5. Рисуем диффузионные связи (если включено)
        if (config.show_diffusion) {
            drawDiffusionConnections(window, system, rot_rad, tilt_rad, cos_tilt, sin_tilt);
        }
        
        // 5.6. Рисуем ингибиторное поле (если включено)
        if (config.show_inhibitor) {
            drawInhibitorField(window, system, rot_rad, tilt_rad, cos_tilt, sin_tilt);
        }
        
        // 5. Рисуем нейроны на их орбитах
        if (config.show_neurons) {
            for (int g = 0; g < numGroups && g < groups.size(); ++g) {
                const auto& group = groups[g];
                const auto& positions = group.getPositions();
                
                // Визуализируем нейроны задавая цвета по массе - красивые Джекичаны
                for (int n = 0; n < groupSize && n < neurons[g].size() && n < positions.size(); ++n) {
                    // Проецируем 3D точку на 2D экран
                    sf::Vector2f screenPos = project3DTo2D(
                        positions[n].x, positions[n].y, positions[n].z,
                        rot_rad, tilt_rad, cos_tilt, sin_tilt
                    );
                    
                    // Масштабируем и добавляем смещение
                    screenPos.x = centerX + config.pan_x + screenPos.x * config.orbit_scale;
                    screenPos.y = centerY + config.pan_y + screenPos.y * config.orbit_scale;
                    
                    neurons[g][n].setPosition(screenPos);
                    
                    // Получаем информацию о нейроне
                    double r = positions[n].norm();
                    int orbitLevel = group.getOrbitLevel(n);
                    
                    // Нормализуем активность для цвета
                    float norm = static_cast<float>((r - minActivity) / (maxActivity - minActivity));
                    norm = std::clamp(norm, 0.0f, 1.0f);
                    
                    // Базовый цвет: либо по орбите, либо по активности
                    sf::Color baseColor;
                    if (config.color_by_orbit) {
                        baseColor = getOrbitColor(orbitLevel, norm);
                    } else {
                        if (config.color_scheme == "blue_red") {
                            std::uint8_t r_color = static_cast<std::uint8_t>(norm * 255);
                            std::uint8_t b = static_cast<std::uint8_t>((1.0f - norm) * 255);
                            baseColor = sf::Color(r_color, 0, b);
                        } else {
                            std::uint8_t val = static_cast<std::uint8_t>(norm * 255);
                            baseColor = sf::Color(val, val, val);
                        }
                    }
                    
                    // Добавляем эффект температуры (теперь метод константный!)
                    sf::Color finalColor = baseColor;
                    if (config.show_temperature) {
                        double temp = group.getNeuronTemperature(n);
                        float glow = static_cast<float>(std::min(temp * 0.3, 1.0));
                        finalColor = sf::Color(
                            std::min(255, baseColor.r + static_cast<std::uint8_t>(glow * 80)),
                            std::max(0, baseColor.g - static_cast<std::uint8_t>(glow * 40)),
                            std::max(0, baseColor.b - static_cast<std::uint8_t>(glow * 40))
                        );
                    }
                    
                    // Размер зависит от орбиты и массы
                    double mass = group.getMass(n);
                    float sizeFactor = 0.5f + orbitLevel * 0.2f;
                    float depthFactor = 0.5f + 0.5f * (positions[n].z + 1.0f) / 2.0f;
                    float massFactor = 0.5f + static_cast<float>(mass) * 0.2f;
                    float size = config.neuron_radius * sizeFactor * depthFactor * massFactor;
                    
                    neurons[g][n].setRadius(size);
                    neurons[g][n].setOrigin(sf::Vector2f(size, size));
                    neurons[g][n].setFillColor(finalColor);
                    
                    window.draw(neurons[g][n]);
                }
            }
        }
        
        window.setView(window.getDefaultView());
    // Рисуем легенду и статистику поверх всего (в screen coordinates)
    if (config.show_legend) {
        drawOrbitLegend(window);
    }
    
    if (config.show_stats) {
        drawStats(window, system);
    }
}
    
    void setConfig(const VisualizationConfig& newConfig) { 
        config = newConfig; 
        initializeOrbitSpheres();
    }
    
    VisualizationConfig& getConfig() { return config; }
    
private:
    sf::Font font;           // шрифт как член класса
    bool fontLoaded = false; // флаг успешной загрузки
    float real_radii_[5] = {0.05f, 0.6f, 1.2f, 2.5f, 4.3f};  // будут обновлены из ядра
    
    // Проекция 3D точки на 2D экран
    sf::Vector2f project3DTo2D(float x, float y, float z, 
                            float rot_rad, float tilt_rad, 
                            float cos_tilt, float sin_tilt) const {
        // Поворот вокруг оси Y
        float x_rot = x * std::cos(rot_rad) + z * std::sin(rot_rad);
        float z_rot = -x * std::sin(rot_rad) + z * std::cos(rot_rad);
        float y_rot = y;
        
        // Наклон
        float y_tilt = y_rot * cos_tilt - z_rot * sin_tilt;
        float z_tilt = y_rot * sin_tilt + z_rot * cos_tilt;
        float x_tilt = x_rot;
        
        // ОРТОГРАФИЧЕСКАЯ проекция (без перспективного искажения)
        // Это сохранит реальные расстояния
        return sf::Vector2f(x_tilt, y_tilt);
    }

    void drawOrbitLegend(sf::RenderWindow& window) {
        if (!config.show_legend || !fontLoaded) return;
        
        int y = 10;
        for (int level = 0; level < 5; ++level) {
            std::string level_names[] = {
                "Orbit 0: Singularity",
                "Orbit 1: Inner (Candidates)",
                "Orbit 2: Base (Workers)",
                "Orbit 3: Middle (Managers)",
                "Orbit 4: Outer (Elite)"
            };
            
            sf::Text text(font);
            text.setFont(font);
            text.setString(level_names[level]);
            text.setCharacterSize(14);
            text.setFillColor(config.orbit_colors[level]);
            text.setPosition(sf::Vector2f(10, y));
            window.draw(text);
            y += 20;
        }
    }


    void drawStats(sf::RenderWindow& window, const NeuralFieldSystem& system) {
        if (!config.show_stats || !fontLoaded) return;
        
        // Собираем статистику по орбитам
        std::vector<int> orbit_counts(5, 0);
        double total_mass = 0.0;
        double total_energy = 0.0;
        int neuron_count = 0;
        
        for (const auto& group : system.getGroups()) {
            for (int i = 0; i < group.getSize(); ++i) {
                int level = group.getOrbitLevel(i);
                if (level >= 0 && level < 5) orbit_counts[level]++;
                total_mass += group.getMass(i);
                total_energy += group.getNeuronEnergy(i);
                neuron_count++;
            }
        }
        
        std::stringstream ss;
        ss << "=== Neural Statistics ===\n";
        ss << "Total neurons: " << neuron_count << "\n";
        ss << "Avg mass: " << std::fixed << std::setprecision(2) << (total_mass / neuron_count) << "\n";
        ss << "Avg energy: " << std::fixed << std::setprecision(2) << (total_energy / neuron_count) << "\n";
        ss << "\nOrbit distribution:\n";
        ss << "Orbit 4 (elite): " << orbit_counts[4] << "\n";
        ss << "Orbit 3: " << orbit_counts[3] << "\n";
        ss << "Orbit 2: " << orbit_counts[2] << "\n";
        ss << "Orbit 1: " << orbit_counts[1] << "\n";
        ss << "Orbit 0: " << orbit_counts[0] << "\n";
        
        if (neuron_count > 0) {
            ss << "\nPercentages:\n";
            ss << "Orbit 4: " << std::fixed << std::setprecision(1) << (orbit_counts[4] * 100.0 / neuron_count) << "%\n";
            ss << "Orbit 3: " << (orbit_counts[3] * 100.0 / neuron_count) << "%\n";
            ss << "Orbit 2: " << (orbit_counts[2] * 100.0 / neuron_count) << "%\n";
            ss << "Orbit 1: " << (orbit_counts[1] * 100.0 / neuron_count) << "%\n";
            ss << "Orbit 0: " << (orbit_counts[0] * 100.0 / neuron_count) << "%\n";
        }
        
        sf::Text text(font);
        text.setFont(font);
        text.setString(ss.str());
        text.setCharacterSize(14);
        text.setFillColor(sf::Color::White);
        
        // ===== ИСПРАВЛЕНО: смещаем вправо в 2 раза =====
        // Было: width - 300 (300 пикселей от правого края)
        // Теперь: width - 600 (600 пикселей от правого края, то есть левее)
        // Или если хочешь ещё правее, можно width - 800
        text.setPosition(sf::Vector2f(width - 400, 10));
        
        window.draw(text);
    }
    
    sf::Color getOrbitColor(int level, float intensity) {
        switch(level) {
            case 0: return sf::Color(100, 100, 100, 255);  // сингулярность - серый
            case 1: return sf::Color(100, 150, 255, 255);  // внутренняя - голубой
            case 2: return sf::Color(100, 255, 100, 255);  // базовая - зеленый
            case 3: return sf::Color(255, 200, 100, 255);  // средняя - оранжевый
            case 4: return sf::Color(255, 100, 100, 255);  // внешняя - красный
            default: return sf::Color(255, 255, 255, 255);
        }
    }

    sf::Color getEnergyColor(double energy, double maxEnergy = 10.0) {
    float t = static_cast<float>(std::min(energy / maxEnergy, 1.0));
    // От синего (низкая энергия) к красному (высокая)
    return sf::Color(
        static_cast<std::uint8_t>(t * 255),
        static_cast<std::uint8_t>((1.0f - std::abs(t - 0.5f) * 2) * 255),
        static_cast<std::uint8_t>((1.0f - t) * 255)
        );
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
                        float abs_weight = std::abs(weight);
                        
                        // Пропускаем очень слабые связи
                        if (abs_weight < 0.05f) {
                            synIndex++;
                            continue;
                        }
                        
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
                        
                        // Цвет и толщина зависят от силы связи
                        sf::Color lineColor;
                        float thickness = 1.0f;
                        
                        if (weight > 0) {
                            // Положительные связи - красные/оранжевые
                            if (abs_weight > 0.5f) {
                                lineColor = sf::Color(255, 50, 50, 255);  // ярко-красный
                                thickness = 3.0f;
                            } else if (abs_weight > 0.2f) {
                                lineColor = sf::Color(255, 100, 100, 200); // красный
                                thickness = 2.0f;
                            } else {
                                lineColor = sf::Color(255, 150, 150, 150); // бледно-красный
                                thickness = 1.0f;
                            }
                        } else {
                            // Отрицательные связи - синие/голубые
                            if (abs_weight > 0.5f) {
                                lineColor = sf::Color(50, 50, 255, 255);  // ярко-синий
                                thickness = 3.0f;
                            } else if (abs_weight > 0.2f) {
                                lineColor = sf::Color(100, 100, 255, 200); // синий
                                thickness = 2.0f;
                            } else {
                                lineColor = sf::Color(150, 150, 255, 150); // бледно-синий
                                thickness = 1.0f;
                            }
                        }
                        
                        // Рисуем линию с заданной толщиной
                        // SFML не поддерживает толщину линий напрямую, но можно нарисовать несколько линий
                        for (float t = -thickness/2; t <= thickness/2; t += 0.5f) {
                            sf::Vertex line[2];
                            // Смещаем линию перпендикулярно для имитации толщины
                            sf::Vector2f dir = pos_j - pos_i;
                            float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                            if (len > 0.01f) {
                                sf::Vector2f perp(-dir.y / len, dir.x / len);
                                line[0].position = pos_i + perp * t;
                                line[1].position = pos_j + perp * t;
                            } else {
                                line[0].position = pos_i;
                                line[1].position = pos_j;
                            }
                            line[0].color = lineColor;
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
    
    std::vector<std::vector<sf::CircleShape>> neurons;
    std::vector<sf::CircleShape> orbitSpheres;  // 5 орбитальных сфер
    sf::CircleShape singularity;
};