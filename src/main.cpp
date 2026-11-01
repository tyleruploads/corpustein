#include <iostream>
#include <unordered_map>
#include <deque>
#include <string>
#include <sstream>
#include <vector>
#include <random>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <iterator>
#include <unordered_set>
#include <list>

std::string next_token(std::mt19937 &gen, const std::vector<std::string> &ngram_context) { 
    std::uniform_int_distribution<size_t> dis(0, ngram_context.size() - 1);
    return ngram_context[dis(gen)];
}

std::string random_ngram(std::mt19937 &gen, std::uniform_int_distribution<size_t> ngram_dis, std::unordered_map<std::string, std::vector<std::string>> &ngram_contexts) {
    auto it = ngram_contexts.begin();
    std::advance(it, ngram_dis(gen));
    return it->first;
}

int main()
{
    std::random_device rd;
    std::mt19937 gen(rd());

    std::filesystem::path filePath = std::filesystem::path(PROJECT_ROOT_DIR) / "src" / "corpus.txt";

    if (!std::filesystem::exists(filePath)) {
        std::cerr << "Corpus file does not exist on disk!\n";
        return 1;
    }

    std::ifstream file(filePath);

    if (!file.is_open()) {
        std::cerr << "Failed to open corpus file\n";
        return 1;
    }
    
    std::string line;
    std::string text;
    while (getline(file, line)) text += line;

    std::regex square_brackets(R"(\s*\[\d+\])");
    text = std::regex_replace(text, square_brackets, "");

    std::stringstream corpus_ss(text);
    std::vector<std::string> tokens; 

    std::string word;
    while (corpus_ss >> word) tokens.push_back(word);

    if (tokens.size() < 3) {
        std::cerr << "Error: Corups is too short to build 3-grams.\n";
        return 1;
    }

    std::unordered_set<std::string> allowed_repeats = {
        "and", "or", "so", "as", "if", "then", "because",
        "but", "yet", "we", "it", "me", "him", "her", "them",
        "us", "my", "your", "his", "their", "theirs", "our", "ours",
        "these", "those", "all", "any", "some", "which", "what", "who",
        "how", "why", "where", "when", "its", "myself", "yourself"
        "out", "down", "off", "no", "only", "very", "just",
        "not", "dont", "cant", "don't", "can't", "more", "most", "other"
        "i", "you", "he", "she", "they", "this", "that", "then", "here", "there"
        "the", "a", "an", "in", "on", "at", "to", "for", "of",
        "with", "by", "from", "up", "about", "into", "over", "after",
        "be", "am", "is", "are", "was", "were", "being", "been",
        "have", "has", "had", "having",
        "do", "does", "did", "im", "youre",
        "can", "could", "may", "might", "must", "shall", "should", "shouldnt", "will", "would"
    };

    std::unordered_map<std::string, std::vector<std::string>> ngram_contexts;

    for (size_t i = 0; i + 3 < tokens.size(); ++i) {
        std::string key = tokens[i] + " " + tokens[i + 1] + " " + tokens[i + 2];
        std::string &next_word = tokens[i + 3];

        auto &vec = ngram_contexts[key];
        if (std::ranges::find(vec, next_word) == vec.end()) {
            vec.push_back(next_word);
        }
    }

    std::uniform_int_distribution<size_t> ngram_dis(0, ngram_contexts.size() - 1);

    std::stringstream ngram_window_ss(random_ngram(gen, ngram_dis, ngram_contexts));
    const size_t max_window_size = 3;
    const size_t max_history_size = 60;

    std::deque<std::string> ngram_window = {
        std::istream_iterator<std::string>{ngram_window_ss},
        std::istream_iterator<std::string>{}
    };

    std::string ngram = ngram_window.at(0) + " " + ngram_window.at(1) + " " + ngram_window.at(2) + " ";

    std::deque<std::string> token_history;

    auto push_token = [&](const std::string &word) {
        ngram_window.push_back(word);
        token_history.push_back(word);

        if (ngram_window.size() > max_window_size) {
            ngram_window.pop_front();
        }

        if (token_history.size() > max_history_size) {
            token_history.pop_front();
        }
    }; 

    while (true) {
        if (!ngram_contexts.contains(ngram)) {
            std::stringstream ngram_window_ss(random_ngram(gen, ngram_dis, ngram_contexts));

            ngram_window.assign(
                std::istream_iterator<std::string>{ngram_window_ss},
                std::istream_iterator<std::string>{}
            );

            ngram = ngram_window.at(0) + " " + ngram_window.at(1) + " " + ngram_window.at(2);
        } 

        std::string context_key = ngram;

        std::string token = next_token(gen, ngram_contexts[context_key]);

        ngram = ngram_window.at(1) + " " + ngram_window.at(2) + " " + token;

        std::string stripped_token = token;

        std::transform(stripped_token.begin(), stripped_token.end(), stripped_token.begin(), [](unsigned char c) {
            return std::tolower(c);
        });

        std::erase(stripped_token, '\'');

        if (!allowed_repeats.contains(stripped_token)) {
            for (int i = 0; i < 5; i++) {
                if (std::ranges::find(token_history, token) != token_history.end()) {
                    token = next_token(gen, ngram_contexts[context_key]);
                } else {
                    break;
                }
            }

            if (std::ranges::find(token_history, token) != token_history.end()) {
                std::stringstream ngram_window_ss(random_ngram(gen, ngram_dis, ngram_contexts));

                ngram_window.assign(
                    std::istream_iterator<std::string>{ngram_window_ss},
                    std::istream_iterator<std::string>{}
                );

                ngram = ngram_window.at(0) + " " + ngram_window.at(1) + " " + ngram_window.at(2);
                continue;
            }
        }

        push_token(token);

        ngram = ngram_window.at(0) + " " + ngram_window.at(1) + " " + ngram_window.at(2);

        std::cout << ngram_window.back() << " ";
    }
}
