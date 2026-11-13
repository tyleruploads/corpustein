static_assert(__cplusplus >= 202002L, "Corpustein requires C++20 or later.");

#ifndef APP_VERSION
extern const char* APP_VERSION; // Silences linters
#error "BUILD ERROR: APP_VERSION must be passed by CMake definitions."
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <list>
#include <optional>
#include <random>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define ALT_BUFFER_ON "\033[?1049h"
#define ALT_BUFFER_OFF "\033[?1049l"
#define CLEAR_SCREEN "\033[2J"
#define CURSOR_HOME "\033[H"
#define BG_COLOR "\033[48;5;232m"
#define AMBER "\033[38;5;214m"
#define DARK_AMBER "\033[38;5;130m"
#define LIGHT_GRAY "\033[38;5;253m"
#define GREEN "\033[32m"
#define FULL_RESET "\033[0m"
#define SOFT_RESET "\033[0m\033[48;5;232m"
#define BOLD "\033[1m"
#define BLINK "\033[5m"
#define RED "\033[31m"

using FunctionAction = std::function<void(const std::string&)>;

int typewriter_delay_ms = 15;

size_t ngram_size = 3;

int max_sentence_count = 5;
int max_token_count = max_sentence_count * 40;

const int max_order = 8;

size_t max_history_size = (max_token_count * 4) / 10;

std::atomic<bool> is_typewriting(false);
std::atomic<bool> skip_typewriting_delay_requested(false);
bool use_alt_buffer = true;

struct Reset
{
    friend std::ostream& operator<<(std::ostream& os, const Reset&)
    {
        // Chooses whether to use full reset or soft reset based on if alternate buffer is enabled
        // SOFT_RESET includes the background, which only is applied correctly if in the alternate
        // buffer
        if (use_alt_buffer) {
            os << SOFT_RESET;
        } else {
            os << FULL_RESET;
        }
        return os;
    }
};

struct BodyText
{
    friend std::ostream& operator<<(std::ostream& os, const BodyText&)
    {
        // Chooses appropriate body text color based on if alternate buffer is enabled
        // Without alternate buffer, background could be anything, so light gray is not a safe idea
        if (use_alt_buffer) {
            os << LIGHT_GRAY;
        } else {
            os << GREEN;
        }
        return os;
    }
};

#define RESET                                                                                      \
    Reset {}
#define BODY_TEXT                                                                                  \
    BodyText {}

class Timer
{
  private:
    std::chrono::steady_clock::time_point start;

  public:
    Timer() { reset(); }

    void reset() { start = std::chrono::steady_clock::now(); }

    template <typename T>
        requires std::same_as<T, double> || std::same_as<T, std::string>
    [[nodiscard]] T ms() const
    {
        double duration =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count();

        if constexpr (std::is_same_v<T, double>) {
            return duration;
        } else if constexpr (std::is_same_v<T, std::string>) {
            return std::to_string(duration);
        }
    }
};

void initApplication()
{
    std::cout << FULL_RESET;

    if (use_alt_buffer) {
        std::cout << ALT_BUFFER_ON << BG_COLOR;
    } else {
        std::cout << ALT_BUFFER_OFF;
    }

    std::cout << AMBER << CLEAR_SCREEN << CURSOR_HOME;
    std::cout << std::flush;

    std::cout << "\033]0;Corpustein v" << APP_VERSION << "\007" << std::flush;

    std::string line_divider = "────────────────────────────\n";
    std::cout << DARK_AMBER << line_divider << RESET << AMBER << "corpustein v" << APP_VERSION
              << "\n"
              << RESET << DARK_AMBER << line_divider << RESET;

    std::cout << BODY_TEXT << BOLD
              << "Enter prompt below. Or, enter /help to view a list of commands.\n\n"
              << RESET;
}

void handle_user_exit(int signal = 0)
{
    (void)signal;

    if (is_typewriting) {
        skip_typewriting_delay_requested = true;
        return;
    }

    std::string final_reset_sequence = "\033[?25h\033[?7h\033[0m";

    if (use_alt_buffer) {
        final_reset_sequence += ALT_BUFFER_OFF;
    }

    final_reset_sequence += "\033]112\007";

    std::cout << final_reset_sequence << std::flush;

    if (!use_alt_buffer) {
        std::cout << "\r\n\r\n";
    }

    std::cout << "[SYSTEM ALERT] SIGINT or other user trigger caused Corpustein abortion. Abortion "
                 "has completed and all ANSI codes have been reverted.\n\033[39m\033[49m\033[0m\r"
              << std::resetiosflags(std::ios::basefield) << std::flush;
    std::_Exit(130);
}

void typewrite(const std::string& text, int delay_ms = 10)
{
    if (delay_ms <= 0) {
        std::cout << text;
        return;
    }

    is_typewriting = true;

    for (size_t i = 0; i < text.size(); ++i) {
        if (skip_typewriting_delay_requested) {
            std::cout << text.substr(i);
            break;
        }

        std::cout << text[i] << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }

    std::cout << RESET;

    is_typewriting = false;

    return;
}

std::optional<std::map<int, std::string>> loadCorpora(char* argv[])
{
    std::map<int, std::string> corpora_map;
    std::error_code ec;

    std::filesystem::path corpora_path =
        std::filesystem::absolute(argv[0], ec).parent_path() / "corpora";

    auto dir_it = std::filesystem::directory_iterator(corpora_path, ec);

    if (ec) {
        return std::nullopt;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir_it)) {
        std::string filename = entry.path().filename().string();

        if (filename.starts_with("corpus_") && filename.size() > 11) {
            try {
                std::string num_str = filename.substr(7, filename.size() - 7 - 4);
                int file_id = std::stoi(num_str);

                std::ifstream file(entry.path());
                std::string line;
                std::getline(file, line); // The name is on the second line

                if (file.is_open() && std::getline(file, line)) {
                    std::string_view delim = ":";
                    auto first_pos = line.find(delim);

                    if (first_pos == std::string_view::npos || first_pos != line.rfind(delim)) {
                        corpora_map[file_id] = "Unnamed";
                        continue;
                    }

                    std::string_view key(line.data(), first_pos);

                    if (key != "name") {
                        corpora_map[file_id] = "Unnamed";
                        continue;
                    }

                    std::string_view name =
                        std::string_view(line).substr(first_pos + 1, line.size() - first_pos - 1);

                    if (name.empty()) {
                        corpora_map[file_id] = "Unnamed";
                        continue;
                    }

                    corpora_map[file_id] = std::string(name);
                }
            } catch (std::exception& e) {
                std::cout << RED;
                typewrite(
                    "[ERROR] An unexpected error occurred while loading corpus file titled '" +
                        filename + "': " + e.what() + "\n",
                    10);
                return std::nullopt;
            }
        }
    }

    return corpora_map;
}

std::string get_seed(const std::string& prompt, const std::vector<std::string>& tokens,
                     const std::unordered_set<std::string>& cleaned_tokens,
                     const std::unordered_set<std::string>& allowed_repeats)
{
    std::stringstream ss(prompt);
    std::vector<std::string> prompt_tokens;

    std::string word;
    while (ss >> word)
        prompt_tokens.push_back(word);

    auto it = std::ranges::find_if(prompt_tokens, [&](const auto& token) {
        std::string cleaned_token = token;

        std::transform(cleaned_token.begin(), cleaned_token.end(), cleaned_token.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        std::erase(cleaned_token, '\'');

        cleaned_token.erase(std::remove_if(cleaned_token.begin(), cleaned_token.end(),
                                           [](unsigned char c) { return std::ispunct(c); }),
                            cleaned_token.end());

        std::erase(cleaned_token, '\'');

        return !allowed_repeats.contains(cleaned_token) && cleaned_tokens.contains(cleaned_token);
    });

    if (it != prompt_tokens.end()) {
        std::string seed = *it;
        std::transform(seed.begin(), seed.end(), seed.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        seed.erase(std::remove_if(seed.begin(), seed.end(),
                                  [](unsigned char c) { return std::ispunct(c); }),
                   seed.end());

        return seed;
    } else {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> tokens_dist(0, tokens.size() - 1);

        while (true) {
            const std::string& canidate = tokens[tokens_dist(gen)];

            std::string cleaned = canidate;
            std::transform(cleaned.begin(), cleaned.end(), cleaned.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            if (!allowed_repeats.contains(cleaned))
                return cleaned;
        }
    }
}

template <typename Iterator>
std::string get_ngram_string(const Iterator& begin, const Iterator& end)
{
    std::string ngram;
    for (auto it = begin; it != end; ++it) {
        ngram += *it;

        if (std::next(it) != end) {
            ngram += " ";
        }
    }
    return ngram;
}

std::unordered_map<std::string, std::vector<std::string>>
get_ngram_contexts(const std::vector<std::string>& tokens)
{
    Timer timer;

    std::unordered_map<std::string, std::vector<std::string>> ngram_contexts;
    ngram_contexts.max_load_factor(0.8f);
    ngram_contexts.reserve(tokens.size());

    for (size_t i = 0; i + ngram_size < tokens.size(); ++i) {
        std::string key =
            get_ngram_string(tokens.begin() + static_cast<std::ptrdiff_t>(i),
                             tokens.begin() + static_cast<std::ptrdiff_t>(i + ngram_size));
        const std::string& next_word = tokens[i + ngram_size];

        auto& vec = ngram_contexts[key];
        if (std::ranges::find(vec, next_word) == vec.end()) {
            vec.push_back(next_word);
        }
    }

    std::cout << BODY_TEXT;
    typewrite("\t" + timer.ms<std::string>() + "ms to build ngram contexts map.\n", 10);

    return ngram_contexts;
}

std::unordered_set<std::string> get_cleaned_tokens(const std::vector<std::string>& tokens)
{
    std::unordered_set<std::string> cleaned_tokens;
    for (const std::string& token : tokens) {
        std::string cleaned_token = token;

        std::transform(cleaned_token.begin(), cleaned_token.end(), cleaned_token.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        cleaned_token.erase(std::remove_if(cleaned_token.begin(), cleaned_token.end(),
                                           [](unsigned char c) { return std::ispunct(c); }),
                            cleaned_token.end());

        std::erase(cleaned_token, '\'');

        cleaned_tokens.insert(cleaned_token);
    }

    return cleaned_tokens;
}

std::string next_token(std::mt19937& gen, const std::vector<std::string>& ngram_context)
{
    std::uniform_int_distribution<size_t> dis(0, ngram_context.size() - 1);
    return ngram_context[dis(gen)];
}

std::string random_ngram(std::mt19937& gen, std::uniform_int_distribution<size_t> ngram_dis,
                         std::unordered_map<std::string, std::vector<std::string>>& ngram_contexts)
{
    auto it = ngram_contexts.begin();
    std::advance(it, ngram_dis(gen));
    return it->first;
}

int main(int argc, char* argv[])
try {
    (void)argc;

#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hIn, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
            SetConsoleMode(hIn, dwMode);
        }
    }
#endif

    std::unordered_set<std::string> allowed_repeats = {
        "and",    "or",       "so",     "as",    "if",     "then",     "because", "but",   "yet",
        "we",     "it",       "me",     "him",   "her",    "them",     "us",      "my",    "your",
        "his",    "their",    "theirs", "our",   "ours",   "these",    "those",   "all",   "any",
        "some",   "which",    "what",   "who",   "how",    "why",      "where",   "when",  "its",
        "myself", "yourself", "out",    "down",  "off",    "no",       "only",    "very",  "just",
        "not",    "dont",     "cant",   "don't", "can't",  "more",     "most",    "other", "i",
        "you",    "he",       "she",    "they",  "this",   "that",     "then",    "here",  "there",
        "the",    "a",        "an",     "in",    "on",     "at",       "to",      "for",   "of",
        "with",   "by",       "from",   "up",    "about",  "into",     "over",    "after", "be",
        "am",     "is",       "are",    "was",   "were",   "being",    "been",    "have",  "has",
        "had",    "having",   "do",     "does",  "did",    "im",       "youre",   "can",   "could",
        "may",    "might",    "must",   "shall", "should", "shouldnt", "will",    "would"};

    static_cast<void>(std::signal(SIGINT, handle_user_exit));
    initApplication();

    std::random_device rd;
    std::mt19937 gen(rd());

    std::vector<std::string> tokens;
    std::unordered_set<std::string> cleaned_tokens;
    size_t corpus_token_count = 0;

    std::unordered_map<std::string, std::vector<std::string>> ngram_contexts;
    std::uniform_int_distribution<size_t> ngram_dis;

    std::string current_corpus_name;

    std::vector<std::string> gen_history;

    auto load_corpus = [&](const std::filesystem::path& filePath) {
        std::ifstream file(filePath);

        if (!std::filesystem::exists(filePath)) {
            std::cerr << RED << BOLD << "[ERROR] Corpus file does not exist on disk!\n" << RESET;
            return;
        }

        if (!file.is_open()) {
            std::cerr << RED << BOLD << "[ERROR] Failed to open corpus file at " << filePath
                      << "\n";
            return;
        }

        bool supported_format = true;
        std::string signature_line;

        if (!std::getline(file, signature_line)) {
            std::cout << RED;
            typewrite("[ERROR] Failed to read first line of corpus file, which should contain "
                      "Corpustein signature. Check the files line count.",
                      10);
            return;
        }

        if (signature_line != "io.github.tyleruploads.corpustein:corpus-file") {
            supported_format = false;
            std::cout << DARK_AMBER;
            typewrite(
                "Corpus file is not in custom Corpustein format. Loading without extra metadata.\n",
                10);
        }

        if (supported_format) {
            std::string line;
            int line_number = 1;
            while (getline(file, line)) {
                if (line == "===")
                    break;

                ++line_number;

                if (line_number > 10) {
                    std::cout << DARK_AMBER;
                    typewrite("[WARNING] Line number is greater than 10 but end of metadata "
                              "delimeter (===) has not been reached. The rest of the corpus will "
                              "be treated as tokens. Line:\n",
                              10);
                    std::cout << BODY_TEXT;
                    typewrite("\t" + line + "\n", 10);
                    break;
                }

                std::string_view delim = ":";

                auto first_pos = line.find(delim);

                if (first_pos == std::string_view::npos || first_pos != line.rfind(delim)) {
                    std::cout << DARK_AMBER;
                    typewrite("[WARNING] Line " + std::to_string(line_number) +
                                  " either has no delimeter or many delimeters. Line: \n",
                              10);
                    std::cout << BODY_TEXT;
                    typewrite("\t" + line + "\n", 10);
                    continue;
                }

                std::string_view key(line.data(), first_pos);
                std::string_view value =
                    std::string_view(line).substr(first_pos + 1, line.size() - first_pos - 1);

                if (key == "name") {
                    std::cout << AMBER;
                    typewrite("Name: ", 15);
                    std::cout << BODY_TEXT;
                    typewrite(std::string(value) + "\n", 10);

                    current_corpus_name = value;
                } else if (key == "default_order") {
                    try {
                        ngram_size = std::stoi(std::string(value));
                    } catch (std::exception& e) {
                        std::cout << RED;
                        typewrite("[WARNING] Value for default_order key on line " + line +
                                      " of corpus metadata contains an invalid integer or integer "
                                      "overflow. Setting order to 2 as a default fallback. Line:\n",
                                  10);
                        std::cout << BODY_TEXT;
                        typewrite("\t" + line + "\n", 10);

                        ngram_size = 2;
                        break;
                    }
                } else if (key == "max_sentence_count") {
                    try {
                        max_sentence_count = std::stoi(std::string(value));
                        max_token_count = max_sentence_count * 40;
                        max_history_size = (max_token_count * 4) / 10;
                    } catch (std::exception& e) {
                        std::cout << RED;
                        typewrite("[WARNING] Value for max_sentence_count key on line " + line +
                                      " of corpus metadata contains an invalid integer or integer "
                                      "overflow, or the integer contributed to an integer overflow "
                                      "in arithmetic to calculate the maximum token count. Setting "
                                      "maximum sentence count to 10 as a default fallback. Line:\n",
                                  10);
                        std::cout << BODY_TEXT;
                        typewrite("\t" + line + "\n", 10);

                        max_sentence_count = 10;
                        max_token_count = max_sentence_count * 40;
                        max_history_size = (max_token_count * 4) / 10;
                    }
                } else {
                    std::cout << DARK_AMBER;
                    typewrite("[WARNING] Line " + std::to_string(line_number) +
                                  " contains an unrecognized metadata key: " + std::string(key) +
                                  ". Continuing.\n",
                              10);
                    std::cout << BODY_TEXT;
                    typewrite("\t" + line + "\n", 10);
                }
            }
        }

        std::string line;
        std::string text;
        while (getline(file, line)) {
            text += line;
            text.push_back('\n');
        }

        std::regex square_brackets(R"(\s*\[\d+\])");
        text = std::regex_replace(text, square_brackets, "");

        std::regex punct_spacing(R"([.!?;:\)](?=[A-Za-z]))");
        text = std::regex_replace(text, punct_spacing, "$& ");

        std::stringstream corpus_ss(text);

        std::string word;
        tokens.clear();
        while (corpus_ss >> word)
            tokens.push_back(word);

        corpus_token_count = tokens.size();

        if (corpus_token_count < ngram_size) {
            std::cerr << RED << BOLD << "Error: Corups is too short to build " << ngram_size
                      << "-grams.\n"
                      << RESET;
            return;
        }

        cleaned_tokens = get_cleaned_tokens(tokens);

        ngram_contexts = get_ngram_contexts(tokens);
        ngram_dis.param(
            std::uniform_int_distribution<size_t>::param_type(0, ngram_contexts.size() - 1));

        std::cout << AMBER;
        typewrite("Successfully loaded corpus at " + filePath.string() + "\n", 10);
        return;
    };

    std::filesystem::path defaultFilePath =
        std::filesystem::absolute(argv[0]).parent_path() / "corpora" / "corpus_0.txt";

    std::filesystem::path filePath;

    if (std::filesystem::exists(defaultFilePath)) {
        filePath = defaultFilePath;
    } else {
#if defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
        std::filesystem::path linuxFilePath = std::filesystem::path("/") / "usr" / "local" /
                                              "share" / "corpustein" / "corpora" / "corpus_0.txt";

        if (!std::filesystem::exists(linuxFilePath)) {
            std::cout << RED;
            typewrite("[ERROR] Both " + defaultFilePath.string() + " and " +
                          linuxFilePath.string() +
                          " do not exist on filesystem. No corpus found.\n",
                      10);
            return 1;
        }

        filePath = linuxFilePath;
#else
        std::cout << RED;
        typewrite("[ERROR] Default corpus not found at " + defaultFilePath.string() + "\n", 10);
        return 1;
#endif
    }

    load_corpus(filePath);

    std::cout << BODY_TEXT << BOLD;
    typewrite("\nFor information on loading a different default corpus or your own custom corpus, "
              "run /load help\n",
              5);

#include "function_runner.inc"

    while (true) {
        skip_typewriting_delay_requested = false;

        int output_token_count = 0;
        int output_sentence_count = 0;
        if (use_alt_buffer)
            std::cout << BG_COLOR;
        std::cout << AMBER << BOLD << BLINK << "\n> " << "\033[25m";
        std::string prompt;
        std::getline(std::cin, prompt);

        if (prompt.empty())
            continue;

        std::string lowercase_prompt = prompt;
        std::transform(lowercase_prompt.begin(), lowercase_prompt.end(), lowercase_prompt.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        std::stringstream input_ss(prompt);
        std::string command;
        input_ss >> command;

        if (command.starts_with("/")) {
            if (auto it = function_runner.find(command); it != function_runner.end()) {
                std::string args;
                std::getline(input_ss >> std::ws, args);
                it->second(args);
                continue;
            } else {
                function_runner.at("/help")(std::string{});
                continue;
            }
        }

        std::string seed = get_seed(prompt, tokens, allowed_repeats, cleaned_tokens);

        std::string starting_ngram;
        auto it = std::ranges::find_if(ngram_contexts, [&](const auto& pair) {
            std::string lower_key = pair.first;
            std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            return pair.first.find(seed) != std::string::npos;
        });

        if (it != ngram_contexts.end()) {
            starting_ngram = it->first;
        } else {
            starting_ngram = random_ngram(gen, ngram_dis, ngram_contexts);
        }

        std::stringstream ngram_window_ss(starting_ngram);

        std::deque<std::string> ngram_window = {std::istream_iterator<std::string>{ngram_window_ss},
                                                std::istream_iterator<std::string>{}};

        std::string ngram = get_ngram_string(ngram_window.begin(), ngram_window.end());

        std::cout << AMBER << BOLD << "Corpustein: " << RESET << BODY_TEXT;
        typewrite(ngram + " ", typewriter_delay_ms);

        std::string current_output = ngram + " ";

        std::deque<std::string> token_history;

        auto push_token = [&](const std::string& word) {
            ngram_window.push_back(word);
            token_history.push_back(word);

            if (ngram_window.size() > static_cast<size_t>(ngram_size)) {
                ngram_window.pop_front();
            }

            if (token_history.size() > static_cast<size_t>(ngram_size)) {
                token_history.pop_front();
            }
        };

        while (true) {
            if (!ngram_contexts.contains(ngram)) {
                ngram_window_ss.str(random_ngram(gen, ngram_dis, ngram_contexts));
                ngram_window_ss.clear();

                ngram_window.assign(std::istream_iterator<std::string>{ngram_window_ss},
                                    std::istream_iterator<std::string>{});

                ngram = get_ngram_string(ngram_window.begin(), ngram_window.end());
            }

            std::string context_key = ngram;

            std::string token = next_token(gen, ngram_contexts[context_key]);

            ngram = get_ngram_string(ngram_window.begin(), ngram_window.end());

            std::string stripped_token = token;

            std::transform(stripped_token.begin(), stripped_token.end(), stripped_token.begin(),
                           [](unsigned char c) { return std::tolower(c); });

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
                    ngram_window_ss.str(random_ngram(gen, ngram_dis, ngram_contexts));
                    ngram_window_ss.clear();

                    ngram_window.assign(std::istream_iterator<std::string>{ngram_window_ss},
                                        std::istream_iterator<std::string>{});

                    ngram = get_ngram_string(ngram_window.begin(), ngram_window.end());
                    continue;
                }
            }

            push_token(token);

            ngram = get_ngram_string(ngram_window.begin(), ngram_window.end());

            current_output += ngram_window.back() + " ";

            std::cout << BODY_TEXT;

            typewrite(ngram_window.back() + " ", typewriter_delay_ms);

            ++output_token_count;
            if (token.find_first_of(".!?") != std::string::npos)
                ++output_sentence_count;

            if (output_sentence_count >= max_sentence_count ||
                output_token_count >= max_token_count) {
                std::cout << "\n";
                gen_history.push_back(current_output);
                break;
            }
        }
    }
} catch (const std::exception& e) {
    std::cout << RED;
    typewrite("[ERROR] An unexpected error occurred during the main function's execution that was "
              "not handled in any downstrean function.\n\t" +
                  std::string(e.what()) + "\n",
              10);
}
