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
#include <csignal>

#define ALT_BUFFER_ON "\033[?1049h"
#define ALT_BUFFER_OFF "\033[?1049l"
#define CLEAR_SCREEN "\033[2J"
#define CURSOR_HOME "\033[H"
#define BG_COLOR "\033[48;5;232m"
#define AMBER "\033[38;5;214m"
#define DARK_AMBER "\033[38;5;130m"
#define LIGHT_GRAY "\033[38;5;253m"
#define FULL_RESET "\033[0m"
#define RESET "\033[0m\033[48;5;232m"
#define BOLD "\033[1m"
#define BLINK "\033[5m"
#define RED "\033[31m"

using FunctionAction = std::function<void()>;

void initApplication() { 
    std::cout << FULL_RESET << ALT_BUFFER_ON << BG_COLOR << AMBER << CLEAR_SCREEN << CURSOR_HOME << std::flush;

    std::cout << "\033]0;Corpustein v" << APP_VERSION << "\007" << std::flush;

    std::string line_divider = "────────────────────────────\n";
    std::cout << DARK_AMBER << line_divider << RESET << AMBER << "corpustein v" << APP_VERSION << "\n" << RESET << DARK_AMBER << line_divider << RESET;

    std::cout << LIGHT_GRAY << BOLD << "Enter prompt below. Or, enter /help to view a list of commands.\n" << RESET;
}

void handle_user_exit(int signal = 0) {
    (void)signal;
    std::cout << FULL_RESET << ALT_BUFFER_OFF << std::flush;
    std::cerr << FULL_RESET << "\n\n[SYSTEM ALERT] SIGINT or other user trigger caused Corpustein abortion. Abortion has completed and all ANSI codes have been reverted.\n" << std::resetiosflags(std::ios::basefield);
    std::exit(130);
}

std::string get_seed(std::string prompt, std::vector<std::string> tokens, std::unordered_set<std::string> allowed_repeats) {
    std::stringstream ss(prompt);
    std::vector<std::string> prompt_tokens; 

    std::string word;
    while (ss >> word) prompt_tokens.push_back(word);

    std::unordered_set<std::string> cleaned_tokens;
    for (std::string token : tokens) {
        std::string cleaned_token = token;

        std::transform(cleaned_token.begin(), cleaned_token.end(), cleaned_token.begin(), [](unsigned char c) {
            return std::tolower(c);
        });

        cleaned_token.erase(std::remove_if(cleaned_token.begin(), cleaned_token.end(), [](unsigned char c) {
            return std::ispunct(c);  
        }), cleaned_token.end());

        std::erase(cleaned_token, '\'');

        cleaned_tokens.insert(cleaned_token);
    }

    auto it = std::ranges::find_if(prompt_tokens, [&](const auto &token) {
        std::string cleaned_token = token;

        std::transform(cleaned_token.begin(), cleaned_token.end(), cleaned_token.begin(), [](unsigned char c) {
            return std::tolower(c);
        });

        std::erase(cleaned_token, '\'');

        cleaned_token.erase(std::remove_if(cleaned_token.begin(), cleaned_token.end(), [](unsigned char c) {
            return std::ispunct(c);  
        }), cleaned_token.end());

        std::erase(cleaned_token, '\'');

        return !allowed_repeats.contains(cleaned_token) && cleaned_tokens.contains(cleaned_token);
    });

    if (it != prompt_tokens.end()) {
        std::string seed = *it;
        std::transform(seed.begin(), seed.end(), seed.begin(), [](unsigned char c) {
            return std::tolower(c);
        });

        seed.erase(std::remove_if(seed.begin(), seed.end(), [](unsigned char c) {
            return std::ispunct(c);
        }), seed.end());

        return seed;
    } else {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> tokens_dist(0, tokens.size() - 1);

        while (true) {
            std::string canidate = tokens[tokens_dist(gen)];

            std::string cleaned = canidate;
            std::transform(cleaned.begin(), cleaned.end(), cleaned.begin(), [](unsigned char c) {
                return std::tolower(c);
            });
 
            if (!allowed_repeats.contains(cleaned)) return cleaned;
        }
    }
}

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
    std::signal(SIGINT, handle_user_exit);
    initApplication();

    std::random_device rd;
    std::mt19937 gen(rd());

    std::filesystem::path filePath = std::filesystem::path(PROJECT_ROOT_DIR) / "src" / "corpus.txt";

    if (!std::filesystem::exists(filePath)) {
        std::cerr << RED << BOLD << "Corpus file does not exist on disk!\n" << RESET;
        return 1;
    }

    std::ifstream file(filePath);

    if (!file.is_open()) {
        std::cerr << RED << BOLD "Failed to open corpus file\n" << RESET;
        return 1;
    }
    
    std::string line;
    std::string text;
    while (getline(file, line)) text += line;

    std::regex square_brackets(R"(\s*\[\d+\])");
    text = std::regex_replace(text, square_brackets, "");

    std::regex punct_spacing(R"([.!?;:\)](?=[A-Za-z]))");
    text = std::regex_replace(text, punct_spacing, "$& ");

    std::stringstream corpus_ss(text);
    std::vector<std::string> tokens; 

    std::string word;
    while (corpus_ss >> word) tokens.push_back(word);

    if (tokens.size() < 3) {
        std::cerr << RED << BOLD << "Error: Corups is too short to build 3-grams.\n" << RESET;
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

    std::unordered_map<std::string, FunctionAction> function_runner = {
        {"/help", []() { std::cout << "Available Commands: /help, /version, /clear, /exit"; }},
        {"?", []() { std::cout << "Available Commands: /help, /clear, /exit"; }},
        {"/version", []() { std::cout << "Corpustein Version: v" << APP_VERSION; }},
        {"/clear", []() { initApplication(); }},
        {"/exit", []() { handle_user_exit(); }}
    };

    while (true) {
        std::cout << BG_COLOR << AMBER << BOLD << BLINK << "\n> " << "\033[25m";
        std::string prompt;
        std::getline(std::cin, prompt);


        std::string lowercase_prompt = prompt;
        std::transform(lowercase_prompt.begin(), lowercase_prompt.end(), lowercase_prompt.begin(), [](unsigned char c) {
            return std::tolower(c);
        });

        if (auto it = function_runner.find(prompt); it != function_runner.end()) {
            it->second();
            continue;
        }

        int token_count = 0;
        int sentence_count = 0;

        const int max_sentence_count = 5;
        const int max_token_count = max_sentence_count * 40;
 
        const size_t max_history_size = max_token_count * 0.4;
        const size_t max_window_size = 3;

        std::string seed = get_seed(prompt, tokens, allowed_repeats);

        std::string starting_ngram;
        auto it = std::ranges::find_if(ngram_contexts, [&](const auto &pair) {
            std::string lower_key = pair.first;
            std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), [](unsigned char c) {
                return std::tolower(c);
            });

            return pair.first.find(seed) != std::string::npos;
        });

        if (it != ngram_contexts.end()) {
            starting_ngram = it->first;
        }
        else {
            starting_ngram = random_ngram(gen, ngram_dis, ngram_contexts);
        }

        std::stringstream ngram_window_ss(starting_ngram); 

        std::deque<std::string> ngram_window = {
            std::istream_iterator<std::string>{ngram_window_ss},
            std::istream_iterator<std::string>{}
        };

        std::string ngram = ngram_window.at(0) + " " + ngram_window.at(1) + " " + ngram_window.at(2);

        std::cout << AMBER << BOLD << "Corpustein: " << RESET << LIGHT_GRAY << ngram << " ";

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

            std::cout << LIGHT_GRAY << ngram_window.back() << " ";

            ++token_count;
            if (token.find_first_of(".!?") != std::string::npos) ++sentence_count;

            if (sentence_count >= max_sentence_count || token_count >= max_token_count) {
                std::cout << "\n";
                break;
            } 
        }
    }
}
