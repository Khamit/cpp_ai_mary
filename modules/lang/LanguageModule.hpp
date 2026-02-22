#pragma once
#include <string>
#include <vector>
#include <cctype>
#include <algorithm>
#include "LanguageRules.hpp"
#include "../../core/NeuralFieldSystem.hpp" 

class LanguageModule {
public:
    explicit LanguageModule(NeuralFieldSystem& system) : system_(system) {}

std::string process(const std::string& input) {
    std::cout << "LanguageModule::process called with: '" << input << "'" << std::endl;
    
    last_input_ = input;

    std::string response = LanguageRules::handleQuestion(input);
    
    std::cout << "LanguageRules response: '" << response << "'" << std::endl;

    encodeToNeuralField(response);

    return response;
}

    void giveFeedback(float rating) {
        auto response = decodeFromNeuralField();

        LanguageRules::updateRule(last_input_, response, rating);

        storeSuccessfulPair(last_input_, response, rating);
    }

private:
    NeuralFieldSystem& system_;
    std::string last_input_;

    void encodeToNeuralField(const std::string& text) {
        auto& phi = system_.getPhi();

        std::fill(phi.begin(), phi.end(), 0.0);

        size_t limit = std::min(text.size(), phi.size());
        for (size_t i = 0; i < limit; ++i) {
            phi[i] = static_cast<double>(
                static_cast<unsigned char>(text[i])
            ) / 255.0;
        }
    }

    std::string decodeFromNeuralField() {
        const auto& phi = system_.getPhi();
        std::string result;

        for (size_t i = 0; i < phi.size() && result.size() < 256; ++i) {
            if (phi[i] > 0.01) {
                char c = static_cast<char>(
                    std::clamp(phi[i], 0.0, 1.0) * 255
                );
                if (std::isprint(static_cast<unsigned char>(c)))
                    result += c;
            }
        }
        return result;
    }

    void storeSuccessfulPair(const std::string& input,
                             const std::string& output,
                             float rating) {
        // Можно интегрировать MemoryController здесь
        // Например:
        // memory_.store(input, output, rating);
    }
};