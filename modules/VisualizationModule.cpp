/*
// cpp_ai_test/modules/VisualizationModule.cpp

#include "VisualizationModule.hpp"
#include <algorithm>

void VisualizationModule::updateDynamicRange(const NeuralFieldSystem& system)
{
    if (!config.dynamic_normalization)
        return;

    const auto& groups = system.getGroups();

    if (groups.empty())
        return;

    double current_min = groups[0].getAverageActivity();
    double current_max = current_min;

    for (size_t i = 1; i < groups.size(); ++i) {
        double avg = groups[i].getAverageActivity();
        current_min = std::min(current_min, avg);
        current_max = std::max(current_max, avg);
    }

    // сглаживание диапазона
    minActivity = 0.99 * minActivity + 0.01 * current_min;
    maxActivity = 0.99 * maxActivity + 0.01 * current_max;

    if (maxActivity - minActivity < config.min_range) {
        double center = 0.5 * (minActivity + maxActivity);
        minActivity = center - config.min_range * 0.5;
        maxActivity = center + config.min_range * 0.5;
    }
}

void VisualizationModule::draw(sf::RenderWindow& window,
                               const NeuralFieldSystem& system)
{
    if (!config.enabled)
        return;

    const auto& groups = system.getGroups();

    for (size_t g = 0; g < groups.size() && g < rectangles.size(); ++g) {

        double activity = groups[g].getAverageActivity();

        float norm = static_cast<float>(
            (activity - minActivity) / (maxActivity - minActivity)
        );

        norm = std::clamp(norm, 0.0f, 1.0f);

        sf::Color color;

        if (config.color_scheme == "blue_red") {

            std::uint8_t r =
                static_cast<std::uint8_t>(norm * 255);

            std::uint8_t b =
                static_cast<std::uint8_t>((1.0f - norm) * 255);

            color = sf::Color(r, 0, b);
        }
        else {
            std::uint8_t val =
                static_cast<std::uint8_t>(norm * 255);

            color = sf::Color(val, val, val);
        }

        rectangles[g].setFillColor(color);
        window.draw(rectangles[g]);
    }
}

*/