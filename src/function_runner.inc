std::unordered_map<std::string, FunctionAction> function_runner = {
    {"/help",
     [](const std::string&) {
         std::cout << "Available Commands: /help, /about, /version, /stats, /set <parameter> "
                      "<value>, "
                      "/load <type> <file>, /reload, /history <argument>, /clear, /exit";
     }},
    {"/about",
     [](const std::string&) {
         std::ostringstream oss;

         oss << "Corpustein is a blazingly fast N-gram Markov Chain model with a configurable "
                "order and a rolling window system.\n";
         oss << "It is coded only in the C++20 Standard Library, which makes it small and "
                "fast.\n";
         oss << "It even uses an alternate terminal buffer to make it look sleek and "
                "beautiful.\n";
         oss << "Usage: Type your prompt after the > symbol, and press Enter.\n";
         oss << "Run /help to view list of slash commands.\n\n";
         oss << "----------\n";
         oss << "Developed by Tyler N. <tyleruploads on GitHub>.\n";
         oss << "GitHub Repository: https://github.com/tyleruploads/corpustein\n";

         std::cout << AMBER;
         typewrite("----- Corpustein v" APP_VERSION " -----\n", 10);
         std::cout << BODY_TEXT;
         typewrite(oss.str(), 5);
     }},
    {"/version",
     [](const std::string&) { std::cout << "Corpustein Version: v" << APP_VERSION << "\n"; }},
    {"/stats",
     [&current_corpus_name, &corpus_token_count, &ngram_contexts](const std::string&) {
         size_t context_count = ngram_contexts.size();
         double health_ratio =
             static_cast<double>(corpus_token_count) / static_cast<double>(context_count);

         std::ostringstream oss;

         oss << "Corpus Name: " << current_corpus_name << "\n";
         oss << "N-gram Order Size: " << ngram_size << "\n";
         oss << "Tokens in Corpus: " << corpus_token_count << "\n";
         oss << "Unique N-gram Contexts: " << context_count << "\n";
         oss << "Health Ratio: " << health_ratio << "\n";
         oss << "Maximum Sentence Count: " << max_sentence_count << "\n";
         oss << "Maximum Token Count: " << max_token_count << "\n";
         oss << "Maximum Order: " << max_order << "\n";
         oss << "Max Tokens in History : " << max_history_size << "\n";
         oss << "Typewriting Delay (ms): " << typewriter_delay_ms << "\n";

         std::cout << AMBER;
         typewrite("----- Corpustein Statistics -----\n", 5);
         std::cout << BODY_TEXT;
         typewrite(oss.str(), 5);
     }},
    {"/set",
     [&tokens, &ngram_contexts, &ngram_dis](const std::string& args) {
         std::stringstream ss(args);
         std::string subcommand;
         std::string subcommand_flag = "";

         if (!(ss >> subcommand)) {
             std::cout << RED;
             typewrite(
                 "Usage: /set <parameter> <value>\nRun /set help for a list of subcommands\n",
                 10);
             return;
         }

         ss >> subcommand_flag; // Optional

         if (subcommand == "help") {
             std::cout << BODY_TEXT;
             typewrite(
                 "Available subcommands:\n\t/set order <integer>: Sets the order of the N-gram "
                 "model.\n\t/set delay <integer>: Sets the delay between each character "
                 "printed to the screen\n\t/set max_sentence_count <integer>: Sets the maximum "
                 "sentence count per each Corpustein response\n\t/set altbuffer <true or "
                 "false>: Enables or disables the alternate terminal buffer",
                 10);
         } else if (subcommand == "order") {
             if (subcommand_flag.empty()) {
                 std::cout << RED;
                 typewrite("Usage: /set order <integer> (e.g., /set order 3)\n", 10);
                 return;
             }

             if (subcommand_flag == "help") {
                 std::cout << BODY_TEXT;
                 typewrite("Sets the order of the N-gram model.\nUsage: /set order <integer> "
                           "(e.g., /set order 3)\n",
                           10);
                 return;
             }

             try {
                 size_t new_ngram_size = std::stoi(subcommand_flag);

                 if (new_ngram_size <= 0) {
                     std::cout << RED;
                     typewrite("[ERROR] Order must be greater than 0.\n", 10);
                     return;
                 }

                 if (static_cast<size_t>(new_ngram_size) >= tokens.size()) {
                     std::cout << RED;
                     typewrite("[ERROR] Order (n) is larger than amount of tokens in corpus. "
                               "Previous order of " +
                                   std::to_string(ngram_size) + " kept\n",
                               10);
                     return;
                 }

                 if (new_ngram_size > max_order) {
                     std::cout << RED;
                     typewrite("[ERROR] Order (" + std::to_string(new_ngram_size) +
                                   ") is larger than maximum order (" +
                                   std::to_string(max_order) + "). Previous order (" +
                                   std::to_string(ngram_size) + ") kept.\n",
                               10);
                     return;
                 }

                 ngram_size = new_ngram_size;
                 ngram_contexts = get_ngram_contexts(tokens);
                 ngram_dis =
                     std::uniform_int_distribution<size_t>(0, ngram_contexts.size() - 1);

                 std::cout << AMBER;
                 typewrite("Order (N) successfully set to " + std::to_string(ngram_size));
             } catch (const std::invalid_argument&) {
                 std::cout << RED;
                 typewrite("[ERROR] Invalid argument. Please provide a valid integer for the "
                           "order.\n",
                           10);
             } catch (const std::out_of_range&) {
                 std::cout << RED;
                 typewrite("[ERROR] Integer is lower than -2,147,483,648 or greater than "
                           "2,147,483,647. Please check your keyboard for physical defects.",
                           10);
             } catch (const std::exception& e) {
                 std::cout << RED;
                 typewrite(std::string("[ERROR] An unexpected error occurred: ") + e.what() +
                               "\n",
                           10);
             }
         } else if (subcommand == "delay") {
             if (subcommand_flag.empty()) {
                 std::cout << RED;
                 typewrite("Usage: /set delay <integer> (e.g., /set delay 15)\n", 10);
                 return;
             }

             if (subcommand_flag == "help") {
                 std::cout << BODY_TEXT;
                 typewrite("Sets the delay between each character printed to the screen when "
                           "interacting with Corpustein.\nUsage: /set delay <integer> (e.g., "
                           "/set delay 15)\n",
                           10);
                 return;
             }

             try {
                 int new_typewriter_delay_ms = std::stoi(subcommand_flag);

                 if (new_typewriter_delay_ms < 0) {
                     std::cout << RED;
                     typewrite("[ERROR] Typewriter delay must be greater than 0.\n", 10);
                     return;
                 }

                 if (new_typewriter_delay_ms > 50) {
                     std::cout << RED;
                     typewrite("Typewriter delay may be no more than 50ms.", 10);
                     return;
                 }

                 typewriter_delay_ms = new_typewriter_delay_ms;
                 std::cout << DARK_AMBER;
                 typewrite("Typewriter delay set to " + std::to_string(typewriter_delay_ms) +
                               "ms.\n",
                           typewriter_delay_ms);
                 return;
             } catch (const std::invalid_argument&) {
                 std::cout << RED;
                 typewrite("[ERROR] Invalid argument. Please provide a valid integer for the "
                           "typewriter delay in milliseconds.\n",
                           10);
             } catch (const std::out_of_range&) {
                 std::cout << RED;
                 typewrite("[ERROR] Integer is lower than -2,147,483,648 or greater than "
                           "2,147,483,647. Please check your keyboard for physical defects.",
                           10);
             } catch (const std::exception& e) {
                 std::cout << RED;
                 typewrite(std::string("[ERROR] An unexpected error occurred: ") + e.what() +
                               "\n",
                           10);
             }
         } else if (subcommand == "max_sentence_count") {
             if (subcommand_flag.empty()) {
                 std::cout << RED;
                 typewrite("Usage: /set max_sentence_count <integer> (e.g., /set "
                           "max_sentence_count 7)\n",
                           10);
                 return;
             }

             if (subcommand_flag == "help") {
                 std::cout << BODY_TEXT;
                 typewrite("Sets the maximum sentence count for each Corpustein response, and "
                           "automatically scales its maximum token count.\nUsage: /set "
                           "max_sentence_count <integer> (e.g., /set max_sentence_count 7)\n",
                           10);
                 return;
             }

             try {
                 int new_max_sentence_count = std::stoi(subcommand_flag);

                 if (new_max_sentence_count <= 0) {
                     std::cout << RED;
                     typewrite("[ERROR] Maximum sentence count must be greater than 0.\n", 10);
                     return;
                 }

                 if (new_max_sentence_count > 50) {
                     std::cout << RED;
                     typewrite("[ERROR] Maximum sentence count must not be greater than 50.\n",
                               10);
                     return;
                 }

                 max_sentence_count = new_max_sentence_count;
                 max_token_count = max_sentence_count * 40;
                 max_history_size = (max_token_count * 4) / 10;

                 std::cout << AMBER;
                 typewrite("Maximum sentence count set to " +
                               std::to_string(max_sentence_count) +
                               ". Maximum token count automatically scaled to " +
                               std::to_string(max_token_count) + ".\n",
                           10);
                 return;
             } catch (const std::invalid_argument&) {
                 std::cout << RED;
                 typewrite("[ERROR] Invalid argument. Please provide a valid integer for the "
                           "maximum sentence count.\n",
                           10);
             } catch (const std::out_of_range&) {
                 std::cout << RED;
                 typewrite("[ERROR] Integer is lower than -2,147,483,648 or greater than "
                           "2,147,463,647. Please check your keyboard for physical defects.\n",
                           10);
             } catch (const std::exception& e) {
                 std::cout << RED;
                 typewrite(std::string("[ERROR] An unexpected error occurred: ") + e.what() +
                               "\n",
                           10);
             }
         } else if (subcommand == "altbuffer") {
             if (subcommand_flag.empty()) {
                 std::cout << RED;
                 typewrite(
                     "Usage: /set altbuffer <true or false> (e.g., /set altbuffer true)\n", 10);
                 return;
             }

             if (subcommand_flag == "help") {
                 std::cout << BODY_TEXT;
                 typewrite("Enables or disables the alternate terminal buffer.\nUsage: /set "
                           "altbuffer <true or false> (e.g., /set altbuffer true)\n",
                           10);
                 return;
             }

             try {
                 std::transform(subcommand_flag.begin(), subcommand_flag.end(),
                                subcommand_flag.begin(), ::tolower);

                 constexpr std::array true_strings{"true",   "1",        "yes",     "on",
                                                   "high",   "set",      "enabled", "enable",
                                                   "active", "activate", "right",   "pass"};
                 constexpr std::array false_strings{
                     "false",    "0",       "no",         "off",        "low",   "unset",
                     "disabled", "disable", "inactivate", "deactivate", "wrong", "fail"};

                 if (std::ranges::any_of(true_strings,
                                         [&](auto s) { return subcommand_flag == s; })) {
                     use_alt_buffer = true;
                 } else if (std::ranges::any_of(false_strings,
                                                [&](auto s) { return subcommand_flag == s; })) {
                     use_alt_buffer = false;
                 } else {
                     std::cout << RED;
                     typewrite(
                         "Usage: /set altbuffer <true or false> (e.g., /set altbuffer true)\n",
                         10);
                     return;
                 }

                 initApplication();

                 std::cout << AMBER << BOLD;
                 typewrite(std::string("\nAlternate buffer successfully ") +
                               (use_alt_buffer ? "enabled" : "disabled") + "\n",
                           10);
             } catch (const std::exception& e) {
                 std::cout << RED;
                 typewrite(std::string("[ERROR] An unexpected error occurred: ") + e.what() +
                               "\n",
                           10);
             }
         } else {
             std::cout << RED;
             typewrite("Invalid subcommand: '" + subcommand +
                           "'. Run /set help for usage information.\n",
                       10);
         }
     }},
    {"/load",
     [&](const std::string& args) {
         std::stringstream ss(args);
         std::string type;
         std::string filename = "";

         if (!(ss >> type)) {
             std::cout << RED;
             typewrite("Usage: /load <type> <corpus>\nRun /load help for a list of types.\nRun "
                       "/load default without corpus to view default corpora.\n",
                       10);
             return;
         }

         ss >> filename;

         if (type == "help") {
             std::cout << BODY_TEXT;
             typewrite("Loads a default or custom corpus.\nUsage: /load <type> "
                       "<corpus>\nTypes:\n\tDefault: Load a corpus from the corpora directory. "
                       "Run without corpus to view list of corpora. Enter corpus number "
                       "provided in corora list as argument to load corpus.\n\tCustom: Load a "
                       "custom corpus file. Enter file path as argument.",
                       10);
             return;
         }

         if (type == "default") {
             if (!filename.empty() && filename == "help") {
                 std::cout << BODY_TEXT;
                 typewrite("Usage: /load default <corpus>\nPlease pass index of corpus, not "
                           "its name or file path.\nTo view list of default corpus indexes, "
                           "run /load default without a corpus argument.\n",
                           10);
                 return;
             }

             std::optional<std::map<int, std::string>> corpora_map = loadCorpora(argv);

             if (corpora_map == std::nullopt) {
#if defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
                 std::filesystem::path linuxFilePath = std::filesystem::path("/") / "usr" /
                                                       "local" / "share" / "corpustein" /
                                                       "corpora";

                 if (!std::filesystem::exists(linuxFilePath)) {
                     std::cout << RED;
                     typewrite("[ERROR] Both " + defaultFilePath.string() + " and " +
                                   linuxFilePath.string() +
                                   " do not exist on filesystem. No corpus found.\n",
                               10);
                     return;
                 }

                 std::string pathStr = linuxFilePath.string();
                 char* pathArr[] = {const_cast<char*>(pathStr.c_str()), nullptr};
                 corpora_map = loadCorpora(pathArr);

                 if (corpora_map == std::nullopt) {
                     std::cout << RED;
                     typewrite("[ERROR] Both " + std::string(argv[0]) + " and " +
                                   linuxFilePath.string() +
                                   " do not exist on filesystem, or another unexpected error "
                                   "occurred.\n",
                               10);
                     return;
                 }
#else
                 std::cout << RED;
                 typewrite("[ERROR] " + std::string(argv[0]) +
                               " does not exist on filesystem, or another unexpected error "
                               "occurred.\n",
                           10);
                 return;
#endif
             }

             if (filename.empty()) {
                 std::ostringstream oss;

                 for (const auto& [key, value] : *corpora_map) {
                     oss << "\tIndex " << std::to_string(key) << ": " << value << "\n";
                 }

                 typewrite("----- Preloaded Corpora -----\n", 10);
                 std::cout << BODY_TEXT << oss.str();
                 return;
             }

             int file_value = 0;

             auto [value_ptr, value_ec] = std::from_chars(
                 filename.data(), filename.data() + filename.size(), file_value);

             if (value_ec != std::errc{} || value_ptr != filename.data() + filename.size()) {
                 std::cout << RED;
                 typewrite(
                     "[ERROR] Please provide the index of and only the index of the corpus.\n",
                     10);
                 return;
             }

             if (corpora_map->empty()) {
                 std::cout << RED;
                 typewrite("[ERROR] No preloaded corpora. To preload a corpus, drop it in the "
                           "corpora directory, which is located in the directory where this "
                           "binary should be. It must be named as the following: "
                           "'corpus_index.txt' (e.g., 'corpus_12.txt'). The first line of text "
                           "in the corpus file is its name.");
                 return;
             }

             if (corpora_map->contains(file_value)) {
                 filePath = std::filesystem::absolute(argv[0]).parent_path() / "corpora" /
                            ("corpus_" + std::to_string(file_value) + ".txt");

#if defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
                 if (!std::filesystem::exists(filePath)) {
                     filePath = std::filesystem::path("/") / "usr" / "local" / "share" /
                                "corpustein" / "corpora" /
                                ("corpus_" + std::to_string(file_value) + ".txt");
                 }
#endif

                 load_corpus(filePath);
             } else {
                 std::cout << RED;
                 typewrite("[ERROR] Corpus index " + std::to_string(file_value) +
                               " is not in the corpora directory.",
                           10);
                 return;
             }
         } else if (type == "custom") {
             if (filename.empty()) {
                 std::cout << RED;
                 typewrite("Run /load custom help for usage\n", 10);
                 return;
             }

             if (filename == "help") {
                 std::cout << BODY_TEXT;
                 typewrite("Usage: /load custom <absolute path> (e.g., /load custom "
                           "/home/me/downloads/corpus.txt)\n",
                           10);
                 return;
             }

             filePath = (std::filesystem::current_path() / filename).lexically_normal();

             if (!std::filesystem::exists(filePath)) {
                 std::cout << RED;
                 typewrite("File at path: " + filePath.string() + " does not exist.\n", 10);
                 return;
             }

             load_corpus(filePath);
         } else {
             std::cout << RED;
             typewrite("Invalid corpus type: '" + type +
                           "'. Please run /load help for usage information.\n",
                       10);
         }
     }},
    {"/reload",
     [&](const std::string& args) {
         std::stringstream ss(args);
         std::string argument;

         ss >> argument;

         if (argument == "help") {
             std::cout << BODY_TEXT;
             typewrite("Reloads the current corpus. No arguments necessary.\n", 10);
             return;
         }

         if (!std::filesystem::exists(filePath)) {
             std::cout << RED;
             typewrite("Current corpus at path: " + filePath.string() + " no longer exists.\n",
                       10);
             return;
         }

         load_corpus(filePath);
     }},
    {"/history",
     [&gen_history](const std::string& args) {
         std::stringstream ss(args);
         std::string argument;

         if (!(ss >> argument)) {
             std::cout << RED;
             typewrite(
                 "Usage: /history <argument>\nRun /history help for a list of subcommands\n",
                 10);
             return;
         }

         if (argument == "help") {
             std::cout << BODY_TEXT;
             typewrite("View or clear history.\n\t/history <integer>: View last X "
                       "responses\n\t/history clear: Clears history\n",
                       10);
             return;
         }

         if (size_t value{};
             std::from_chars(argument.data(), argument.data() + argument.size(), value).ec ==
             std::errc{}) {
             if (gen_history.empty()) {
                 std::cout << RED;
                 typewrite("History is empty.\n");
                 return;
             }

             if (value > gen_history.size()) {
                 std::cout << RED;
                 typewrite("[ERROR] History size requested is " + std::to_string(value) +
                               ", which is greater than the history vector, which has " +
                               std::to_string(gen_history.size()) + " elements\n",
                           10);
                 return;
             }

             std::ostringstream oss;

             for (size_t i = 0; i < value; ++i) {
                 oss << "----- " << std::to_string(i + 1) << " -----\n";
                 oss << gen_history.at(gen_history.size() - value) << "\n\n";
             }

             int speed;

             if (typewriter_delay_ms < 2)
                 speed = 0;
             else
                 speed = typewriter_delay_ms / 5;

             std::cout << BODY_TEXT;
             typewrite(oss.str(), speed);
             return;
         }

         if (argument == "clear") {
             size_t gen_history_size = gen_history.size();
             gen_history.clear();

             std::cout << AMBER;
             typewrite("Successfully cleared generation history, which contained " +
                           std::to_string(gen_history_size) + " elements.\n",
                       10);
         } else {
             std::cout << RED;
             typewrite("[ERROR] Argument '" + argument + "' is invalid.\n", 10);
         }
     }},
    {"/clear", [](const std::string&) { initApplication(); }},
    {"/exit", [](const std::string&) { handle_user_exit(); }}
};
