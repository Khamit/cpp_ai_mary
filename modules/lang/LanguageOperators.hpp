#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

class LanguageOperators {
public:
    static std::string connect(const std::string& a, const std::string& b) {
        return a + " " + b;
    }

    static std::string question(const std::string& subject,
                                const std::string& predicate) {
        return "what is " + subject + " " + predicate + "?";
    }

    static std::string answer(const std::string& subject,
                              const std::string& definition) {
        return subject + " is " + definition;
    }

    static bool isSimilar(const std::string& a, const std::string& b) {
        return a.find(b) != std::string::npos ||
               b.find(a) != std::string::npos;
    }

    static std::string toLower(std::string text) {
        std::transform(text.begin(), text.end(),
                       text.begin(), ::tolower);
        return text;
    }

    static std::vector<std::string> split(const std::string& text,
                                          char delim = ' ') {
        std::vector<std::string> tokens;
        std::stringstream ss(text);
        std::string token;

        while (std::getline(ss, token, delim)) {
            if (!token.empty())
                tokens.push_back(toLower(token));
        }

        return tokens;
    }

    static std::string join(const std::vector<std::string>& parts,
                            const std::string& joiner = " ") {
        if (parts.empty()) return "";

        std::string result = parts[0];
        for (size_t i = 1; i < parts.size(); ++i)
            result += joiner + parts[i];

        return result;
    }
};