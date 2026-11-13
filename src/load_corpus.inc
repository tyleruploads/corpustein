auto load_corpus = [&](const std::filesystem::path& filePath) {
    std::ifstream file(filePath);

    if (!std::filesystem::exists(filePath)) {
        std::cerr << RED << BOLD << "[ERROR] Corpus file does not exist on disk!\n" << RESET;
        return;
    }

    if (!file.is_open()) {
        std::cerr << RED << BOLD << "[ERROR] Failed to open corpus file at " << filePath << "\n";
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
