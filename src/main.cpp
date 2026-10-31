#include <iostream>
#include <deque>
#include <string>
#include <sstream>
#include <vector>
#include <random>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>

std::string next_token(std::mt19937 &gen, std::string last_token, std::vector<std::string> corpus) { 
    std::vector<std::string> choices;

    for (int i = 0; i < corpus.size() - 1; ++i) {
        if (corpus[i] == last_token) {
            choices.push_back(corpus[i + 1]);
        }
    }

    std::uniform_int_distribution<size_t> dis(0, choices.size() - 1);
    return choices[dis(gen)];
}

int main()
{
    std::random_device rd;
    std::mt19937 gen(rd());

    std::filesystem::path filePath = std::filesystem::path(PROJECT_ROOT_DIR) / "src" / "corpus.txt";
    std::ifstream file(filePath);

    if (!file.is_open()) {
        std::cerr << "Failed to open corpus file\n";
        return 1;
    }
    
    std::string line;
    std::string text;
    while (getline(file, line)) text += line;

    std::regex square_brackets(R"(/\[.*?\]/)");
    text = std::regex_replace(text, square_brackets, "[$&]");

    std::stringstream ss(text);
    std::vector<std::string> tokens; 

    std::string word;
    while (ss >> word) tokens.push_back(word);

    if (tokens.size() < 3) {
        std::cerr << "Error: Corups is too short to build 3-grams.\n";
        return 1;
    }

    std::vector<std::string> n_grams;
    int i = 0;
    while (true) {
        if (i + 2 >= tokens.size()) break;

        std::string n_gram = tokens[i] + " " + tokens[i + 1] + " " + tokens[i + 2] + " ";

        n_grams.push_back(n_gram);
        ++i; 
    }

    std::uniform_int_distribution<size_t> ngram_dis(0, n_grams.size() - 1);
    std::string last_token = n_grams[ngram_dis(gen)];

    while (true) {
        last_token = next_token(gen, last_token, n_grams);
        if (last_token.empty()) last_token = n_grams[ngram_dis(gen)]; 
        std::cout << last_token;
    }
}
