// modules/lang/LanguageModule.cpp
#include "LanguageModule.hpp"

// ------------------------------------------------------------
// Конструктор
// ------------------------------------------------------------
LanguageModule::LanguageModule(NeuralFieldSystem& system)
    : system_(system),
      rng_(std::random_device{}()),
      word_length_dist_(2, 8),
      letter_dist_(0, 25) {

    alphabet_ = {
        'a','b','c','d','e','f','g','h','i','j','k','l','m',
        'n','o','p','q','r','s','t','u','v','w','x','y','z'
    };

    initializeEnglishBigrams();
    loadBigrams();

    if (!std::ifstream("bigrams.dat").good()) {
        saveBigrams();
        std::cout << "Initial bigrams saved to file" << std::endl;
    }

    loadLearnedWords();

    std::cout << "LanguageModule initialized with " << learned_words_.size()
              << " learned words" << std::endl;

    std::cout << "Bigrams loaded from file" << std::endl;
    int t_idx = 't' - 'a';
    std::vector<std::pair<char, float>> t_bigrams;
    for (int j = 0; j < 26; ++j) {
        t_bigrams.push_back({char('a' + j), bigram_matrix_[t_idx][j]});
    }
    std::sort(t_bigrams.begin(), t_bigrams.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    std::cout << "Top bigrams for 't': ";
    for (int i = 0; i < 5 && i < t_bigrams.size(); ++i) {
        std::cout << t_bigrams[i].first << ":" << t_bigrams[i].second << " ";
    }
    std::cout << std::endl;
}

// ------------------------------------------------------------
// Публичные методы
// ------------------------------------------------------------
std::string LanguageModule::process(const std::string& input) {
    std::cout << "LanguageModule::process called with: '" << input << "'" << std::endl;

    last_input_ = input;
    std::string response;

    if (input == "generate" || input == "new word") {
        std::string new_word = generateWordFromNeurons();
        response = "I've created: '" + new_word + "' - what do you think?";
    }
    else if (input == "stats" || input == "words") {
        response = getStats();
    }
    else if (input.find("what is ") == 0) {
        std::string word = input.substr(8);
        response = getWordMeaning(word);
    }
    else if (input == "hi" || input == "hello" || input == "hey") {
        response = "Hello! I'm learning to generate words. Type 'generate' to see a new word!";
    }
    else {
        std::string new_word = generateWordFromNeurons();
        response = getRandomResponse() + " '" + new_word + "'?";
    }

    encodeToNeuralField(response);
    return response;
}

void LanguageModule::normalizeBigrams() {
    for (int i = 0; i < 26; ++i) {
        float sum = 0;
        for (int j = 0; j < 26; ++j) sum += bigram_matrix_[i][j];
        if (sum > 0) {
            for (int j = 0; j < 26; ++j) bigram_matrix_[i][j] /= sum;
        }
    }
}

void LanguageModule::saveBigrams() {
    std::ofstream file("bigrams.dat", std::ios::binary);
    file.write(reinterpret_cast<char*>(bigram_matrix_), sizeof(bigram_matrix_));
}

void LanguageModule::loadBigrams() {
    std::ifstream file("bigrams.dat", std::ios::binary);
    if (file) file.read(reinterpret_cast<char*>(bigram_matrix_), sizeof(bigram_matrix_));
    else { /* инициализация по умолчанию уже сделана в конструкторе */ }
}

void LanguageModule::giveFeedback(float rating) {
    std::cout << "Feedback received: " << rating << std::endl;
    std::string last_word = decodeLastWord();
    if (last_word.empty()) return;

    std::cout << "Feedback for word: '" << last_word << "'" << std::endl;

    updateLetterProbabilities(last_word, rating);
    learnFromFeedback(last_word, rating);

    if (rating > 0.7f) {
        for (size_t i = 0; i < last_word.length() - 1; ++i) {
            char c1 = std::tolower(last_word[i]);
            char c2 = std::tolower(last_word[i+1]);
            if (c1 >= 'a' && c1 <= 'z' && c2 >= 'a' && c2 <= 'z') {
                int idx1 = c1 - 'a';
                int idx2 = c2 - 'a';
                bigram_matrix_[idx1][idx2] += 0.1f;
            }
        }
        normalizeBigrams();
        saveWord(last_word, rating);
        std::cout << "(+) Learned new word: '" << last_word << "'" << std::endl;
    }
    else if (rating < 0.3f) {
        for (size_t i = 0; i < last_word.length() - 1; ++i) {
            char c1 = std::tolower(last_word[i]);
            char c2 = std::tolower(last_word[i+1]);
            if (c1 >= 'a' && c1 <= 'z' && c2 >= 'a' && c2 <= 'z') {
                int idx1 = c1 - 'a';
                int idx2 = c2 - 'a';
                bigram_matrix_[idx1][idx2] *= 0.9f;
            }
        }
        normalizeBigrams();
        weakenWordPattern(last_word);
        std::cout << "(-) Rejected word: '" << last_word << "'" << std::endl;
    }

    saveProbabilities();
    saveBigrams();
}

// ------------------------------------------------------------
// Приватные методы
// ------------------------------------------------------------
void LanguageModule::initializeEnglishBigrams() {
    const std::vector<std::pair<std::string, float>> englishBigrams = {
        {"th", 0.0356f}, {"he", 0.0307f}, {"in", 0.0243f}, {"er", 0.0205f},
        {"an", 0.0199f}, {"re", 0.0185f}, {"ed", 0.0175f}, {"on", 0.0174f},
        {"es", 0.0167f}, {"st", 0.0162f}, {"en", 0.0159f}, {"at", 0.0158f},
        {"to", 0.0157f}, {"nt", 0.0153f}, {"ha", 0.0147f}, {"nd", 0.0144f},
        {"ou", 0.0144f}, {"ea", 0.0138f}, {"ng", 0.0137f}, {"al", 0.0136f},
        {"it", 0.0135f}, {"is", 0.0134f}, {"or", 0.0133f}, {"ti", 0.0132f},
        {"as", 0.0131f}, {"te", 0.0130f}, {"et", 0.0129f}, {"ng", 0.0128f},
        {"of", 0.0127f}, {"al", 0.0126f}, {"de", 0.0125f}, {"se", 0.0124f},
        {"le", 0.0123f}, {"sa", 0.0122f}, {"si", 0.0121f}, {"ar", 0.0120f},
        {"ve", 0.0119f}, {"ra", 0.0118f}, {"ld", 0.0117f}, {"ur", 0.0116f}
    };

    for (int i = 0; i < 26; ++i) {
        for (int j = 0; j < 26; ++j) {
            bigram_matrix_[i][j] = 0.0001f;
        }
    }

    for (const auto& [bigram, freq] : englishBigrams) {
        if (bigram.length() == 2) {
            char c1 = std::tolower(bigram[0]);
            char c2 = std::tolower(bigram[1]);
            if (c1 >= 'a' && c1 <= 'z' && c2 >= 'a' && c2 <= 'z') {
                int idx1 = c1 - 'a';
                int idx2 = c2 - 'a';
                bigram_matrix_[idx1][idx2] = freq;
            }
        }
    }

    normalizeBigrams();
}

std::string LanguageModule::generateWordFromNeurons() {
    std::string word;
    const auto& phi = system_.getPhi();

    float total_energy = 0;
    for (int i = NeuralFieldSystem::LANGUAGE_START; i < NeuralFieldSystem::LANGUAGE_END; ++i) {
        total_energy += std::abs(static_cast<float>(phi[i]));
    }

    float energy_factor = total_energy / 50.0f;
    energy_factor = std::clamp(energy_factor, 0.3f, 1.5f);
    int base_length = word_length_dist_(rng_);
    int word_length = static_cast<int>(base_length * energy_factor);
    word_length = std::clamp(word_length, 2, 8);

    char prev_char = 0;

    for (int pos = 0; pos < word_length; ++pos) {
        float position_probs[26] = {0};

        int neuron_start = (pos * NeuralFieldSystem::LANGUAGE_NEURONS) / word_length;
        int neuron_end = ((pos + 1) * NeuralFieldSystem::LANGUAGE_NEURONS) / word_length;

        for (int i = neuron_start; i < neuron_end && i < NeuralFieldSystem::LANGUAGE_END; ++i) {
            float activation = std::abs(static_cast<float>(phi[i]));
            for (int letter = 0; letter < 26; ++letter) {
                float letter_factor = 1.0f - std::abs(letter - (i % 26)) / 26.0f;
                position_probs[letter] += activation * letter_factor;
            }
        }

        if (pos > 0 && prev_char >= 'a' && prev_char <= 'z') {
            int prev_idx = prev_char - 'a';
            for (int letter = 0; letter < 26; ++letter) {
                position_probs[letter] += bigram_matrix_[prev_idx][letter] * 4.0f;
            }
        }

        for (int letter = 0; letter < 26; ++letter) {
            position_probs[letter] += base_letter_probs_[letter] * 0.3f;
        }

        float random_factor = std::uniform_real_distribution<>(0.8f, 1.2f)(rng_);
        for (int letter = 0; letter < 26; ++letter) {
            position_probs[letter] *= random_factor;
        }

        float sum = 0;
        for (int letter = 0; letter < 26; ++letter) sum += position_probs[letter];

        if (sum > 0) {
            for (int letter = 0; letter < 26; ++letter) position_probs[letter] /= sum;
        } else {
            for (int letter = 0; letter < 26; ++letter) position_probs[letter] = 1.0f / 26.0f;
        }

        float r = std::uniform_real_distribution<>(0, 1)(rng_);
        float cumulative = 0;
        int chosen_letter = 0;
        for (int letter = 0; letter < 26; ++letter) {
            cumulative += position_probs[letter];
            if (r <= cumulative) {
                chosen_letter = letter;
                break;
            }
        }

        char chosen = alphabet_[chosen_letter];
        if (pos == 0 && std::uniform_real_distribution<>(0,1)(rng_) < 0.2f)
            chosen = std::toupper(chosen);

        word += chosen;
        prev_char = std::tolower(chosen);
    }

    word_history_.push_back(word);
    if (word_history_.size() > 20) word_history_.erase(word_history_.begin());

    enhanceNeuralPattern(word);
    return word;
}

void LanguageModule::enhanceNeuralPattern(const std::string& word) {
    auto& phi = system_.getPhi();
    auto& W = system_.getWeights();

    int word_length = word.size();

    for (int pos = 0; pos < word_length; ++pos) {
        char c = std::tolower(word[pos]);
        if (c < 'a' || c > 'z') continue;

        int letter_idx = c - 'a';
        int neuron_start = (pos * NeuralFieldSystem::LANGUAGE_NEURONS) / word_length;
        int neuron_end = ((pos + 1) * NeuralFieldSystem::LANGUAGE_NEURONS) / word_length;

        for (int i = neuron_start; i < neuron_end && i < NeuralFieldSystem::LANGUAGE_END; ++i) {
            float target_activation = 0.3f + (letter_idx / 26.0f) * 0.5f;
            phi[i] = phi[i] * 0.7f + target_activation * 0.3f;

            for (int j = neuron_start; j < neuron_end; ++j) {
                if (i != j) {
                    W[i][j] += 0.01f;
                    W[j][i] = W[i][j];
                    if (W[i][j] > 0.3f) W[i][j] = 0.3f;
                }
            }
        }
    }
}

void LanguageModule::weakenWordPattern(const std::string& word) {
    auto& phi = system_.getPhi();
    int word_length = word.size();

    for (int pos = 0; pos < word_length; ++pos) {
        int neuron_start = (pos * NeuralFieldSystem::LANGUAGE_NEURONS) / word_length;
        int neuron_end = ((pos + 1) * NeuralFieldSystem::LANGUAGE_NEURONS) / word_length;

        for (int i = neuron_start; i < neuron_end && i < NeuralFieldSystem::LANGUAGE_END; ++i) {
            phi[i] *= 0.8f;
        }
    }
}

void LanguageModule::updateLetterProbabilities(const std::string& word, float rating) {
    float learning_rate = (rating - 0.5f) * 0.3f;

    for (char c : word) {
        char lower_c = std::tolower(c);
        if (lower_c >= 'a' && lower_c <= 'z') {
            int idx = lower_c - 'a';
            base_letter_probs_[idx] += learning_rate;
            if (base_letter_probs_[idx] < 0.01f) base_letter_probs_[idx] = 0.01f;
        }
    }

    float sum = 0;
    for (int i = 0; i < 26; ++i) sum += base_letter_probs_[i];
    for (int i = 0; i < 26; ++i) base_letter_probs_[i] /= sum;
}

void LanguageModule::saveProbabilities() {
    std::ofstream file("letter_probs.txt");
    if (file.is_open()) {
        for (int i = 0; i < 26; ++i) {
            file << alphabet_[i] << ":" << base_letter_probs_[i] << "\n";
        }
    }
}

void LanguageModule::learnFromFeedback(const std::string& word, float rating) {
    auto& W = system_.getWeights();
    auto& phi = system_.getPhi();

    float learning_rate = 0.05f;
    int word_length = word.size();

    for (int pos = 0; pos < word_length; ++pos) {
        char c = std::tolower(word[pos]);
        if (c < 'a' || c > 'z') continue;

        int letter_idx = c - 'a';
        int neuron_start = (pos * NeuralFieldSystem::LANGUAGE_NEURONS) / word_length;
        int neuron_end = ((pos + 1) * NeuralFieldSystem::LANGUAGE_NEURONS) / word_length;

        for (int i = neuron_start; i < neuron_end && i < NeuralFieldSystem::LANGUAGE_END; ++i) {
            float target = 0.3f + (letter_idx / 26.0f) * 0.5f;
            double delta = (target - static_cast<float>(phi[i])) * learning_rate * 2.0;
            phi[i] = std::clamp(phi[i] + delta, 0.0, 1.0);

            for (int k = NeuralFieldSystem::LANGUAGE_START; k < NeuralFieldSystem::LANGUAGE_END; ++k) {
                if (k != i) {
                    double hebb_update = learning_rate * phi[i] * phi[k];
                    W[i][k] += hebb_update;
                    W[k][i] = W[i][k];
                    W[i][k] = std::clamp(W[i][k], -0.3, 0.3);
                    W[k][i] = W[i][k];
                }
            }
        }
    }
}

void LanguageModule::saveWord(const std::string& word, float rating) {
    LearnedWord lw;
    lw.word = word;
    lw.frequency = 1.0f;
    lw.correctness = rating;
    lw.times_rated = 1;

    for (int i = 0; i < 26; ++i) {
        lw.letter_probabilities[i] = base_letter_probs_[i];
    }

    const auto& phi = system_.getPhi();
    for (int i = NeuralFieldSystem::LANGUAGE_START; i < NeuralFieldSystem::LANGUAGE_END; ++i) {
        lw.neural_pattern.push_back(static_cast<float>(phi[i]));
    }

    learned_words_[word] = lw;
    saveLearnedWords();
}

void LanguageModule::saveLearnedWords() {
    std::ofstream file("learned_words.txt");
    if (file.is_open()) {
        for (const auto& [word, data] : learned_words_) {
            file << word << "|"
                 << data.frequency << "|"
                 << data.correctness << "|"
                 << data.times_rated << "\n";
        }
    }
    saveNeuralPatterns();
}

void LanguageModule::saveNeuralPatterns() {
    std::ofstream file("neural_patterns.dat", std::ios::binary);
    if (file.is_open()) {
        size_t count = learned_words_.size();
        file.write(reinterpret_cast<char*>(&count), sizeof(count));

        for (const auto& [word, data] : learned_words_) {
            size_t word_len = word.size();
            file.write(reinterpret_cast<char*>(&word_len), sizeof(word_len));
            file.write(word.c_str(), word_len);
            file.write(reinterpret_cast<const char*>(data.neural_pattern.data()),
                       data.neural_pattern.size() * sizeof(float));
        }
    }
}

void LanguageModule::loadLearnedWords() {
    std::ifstream file("learned_words.txt");
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            size_t pos1 = line.find('|');
            size_t pos2 = line.find('|', pos1 + 1);
            size_t pos3 = line.find('|', pos2 + 1);

            if (pos1 != std::string::npos &&
                pos2 != std::string::npos &&
                pos3 != std::string::npos) {

                LearnedWord lw;
                lw.word = line.substr(0, pos1);
                lw.frequency = std::stof(line.substr(pos1 + 1, pos2 - pos1 - 1));
                lw.correctness = std::stof(line.substr(pos2 + 1, pos3 - pos2 - 1));
                lw.times_rated = std::stoi(line.substr(pos3 + 1));

                learned_words_[lw.word] = lw;
            }
        }
    }
    loadNeuralPatterns();
    loadProbabilities();
}

void LanguageModule::loadProbabilities() {
    std::ifstream file("letter_probs.txt");
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                char letter = line[0];
                float prob = std::stof(line.substr(pos + 1));
                if (letter >= 'a' && letter <= 'z') {
                    base_letter_probs_[letter - 'a'] = prob;
                }
            }
        }
    }
}

void LanguageModule::loadNeuralPatterns() {
    std::ifstream file("neural_patterns.dat", std::ios::binary);
    if (file.is_open()) {
        size_t count;
        file.read(reinterpret_cast<char*>(&count), sizeof(count));

        for (size_t i = 0; i < count; ++i) {
            size_t word_len;
            file.read(reinterpret_cast<char*>(&word_len), sizeof(word_len));

            std::string word(word_len, ' ');
            file.read(&word[0], word_len);

            if (learned_words_.find(word) != learned_words_.end()) {
                auto& lw = learned_words_[word];
                lw.neural_pattern.resize(NeuralFieldSystem::LANGUAGE_NEURONS);
                file.read(reinterpret_cast<char*>(lw.neural_pattern.data()),
                          NeuralFieldSystem::LANGUAGE_NEURONS * sizeof(float));
            } else {
                file.seekg(NeuralFieldSystem::LANGUAGE_NEURONS * sizeof(float), std::ios::cur);
            }
        }
    }
}

std::string LanguageModule::getWordMeaning(const std::string& word) {
    auto it = learned_words_.find(word);
    if (it != learned_words_.end()) {
        return "'" + word + "' (learned, correctness: " +
               std::to_string(static_cast<int>(it->second.correctness * 100)) + "%)";
    } else {
        return "I haven't learned '" + word + "' yet. Try to generate it!";
    }
}

std::string LanguageModule::getStats() {
    std::string result = " Language Stats:\n";
    result += "Learned words: " + std::to_string(learned_words_.size()) + "\n";
    result += "Letter probabilities:\n";

    std::vector<std::pair<char, float>> probs;
    for (int i = 0; i < 26; ++i) {
        probs.push_back({alphabet_[i], base_letter_probs_[i]});
    }

    std::sort(probs.begin(), probs.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    for (int i = 0; i < 5 && i < probs.size(); ++i) {
        result += probs[i].first + std::string(": ") +
                  std::to_string(static_cast<int>(probs[i].second * 100)) + "%\n";
    }

    return result;
}

std::string LanguageModule::getRandomResponse() {
    static std::vector<std::string> responses = {
        "How about",
        "What do you think of",
        "I've been thinking about",
        "Try this one:",
        "Maybe",
        "Is this a word:"
    };
    std::uniform_int_distribution<> dist(0, responses.size() - 1);
    return responses[dist(rng_)];
}

std::string LanguageModule::decodeLastWord() {
    if (word_history_.empty()) return "";
    return word_history_.back();
}

void LanguageModule::encodeToNeuralField(const std::string& text) {
    auto& phi = system_.getPhi();

    for (int i = NeuralFieldSystem::LANGUAGE_START; i < NeuralFieldSystem::LANGUAGE_END; ++i) {
        phi[i] = 0.0;
    }

    size_t limit = std::min(text.size(), static_cast<size_t>(NeuralFieldSystem::LANGUAGE_NEURONS));
    for (size_t i = 0; i < limit; ++i) {
        phi[i] = static_cast<double>(static_cast<unsigned char>(text[i])) / 255.0;
    }
}