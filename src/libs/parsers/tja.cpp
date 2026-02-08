#include "tja.h"

double get_ms_per_measure(double bpm_val, double time_sig) {
    if (bpm_val == 0) return 0;
    return 60000 * (time_sig * 4) / bpm_val;
}


int calculate_base_score(const NoteList& notes) {
    int total_notes = 0;
    int balloon_count = 0;
    double drumroll_msec = 0.0f;

    for (size_t i = 0; i < notes.notes.size(); i++) {
        Note note = notes.notes[i];

        Note next_note = *((i < notes.notes.size() - 1)
            ? &notes.notes[i + 1]
            : &notes.notes[notes.notes.size() - 1]);

        if (note.color.has_value()) { //is drumroll
            drumroll_msec += (next_note.hit_ms - note.hit_ms);
        } else if (note.count.has_value()) { //is balloon
            balloon_count += std::min(100, note.count.value());
        } else {
            if (note.type == 8) {
                continue;
            }
            if (1 <= note.type && note.type <= 4) total_notes += 1;
        }
    }

    if (total_notes == 0) return 1000000;

    double calculation = (1000000.0f - (balloon_count * 100.0f) - (16.920079999994086f * drumroll_msec / 1000.0f * 100.0f)) / total_notes / 10.0f;
    return static_cast<int>(std::ceil(calculation)) * 10;
}

std::string test_encodings(const std::filesystem::path& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    unsigned char bom[4] = {0};
    file.read(reinterpret_cast<char*>(bom), 4);
    file.seekg(0);

    if (bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF) {
        return "utf-8-sig";
    }

    if (bom[0] == 0xFF && bom[1] == 0xFE) {
        return "utf-16-le";
    }

    if (bom[0] == 0xFE && bom[1] == 0xFF) {
        return "utf-16-be";
    }

    return "shift-jis";
}


std::string strip_comments(const std::string& code) {
    std::string result;
    std::istringstream stream(code);
    std::string line;

    while (std::getline(stream, line)) {
        size_t comment_pos = line.find("//");

        if (comment_pos == std::string::npos) {
            result += line;
        } else if (comment_pos > 0) {
            std::string prefix = line.substr(0, comment_pos);
            auto non_space = std::find_if(prefix.begin(), prefix.end(),
                                         [](unsigned char c) { return !std::isspace(c); });

            if (non_space != prefix.end()) {
                result += prefix;
            }
        }
    }

    return result;
}

TJAParser::TJAParser(const std::filesystem::path& path, int start_delay)
    : file_path(path), current_ms(static_cast<double>(start_delay)) {

    std::string encoding = test_encodings(file_path);

    std::vector<std::string> lines = read_file_lines(file_path, encoding);

    for (const auto& line : lines) {
        std::string cleaned = strip_comments(line);
        cleaned.erase(0, cleaned.find_first_not_of(" \t\r\n"));
        cleaned.erase(cleaned.find_last_not_of(" \t\r\n") + 1);

        if (!cleaned.empty()) {
            data.push_back(cleaned);
        }
    }

    metadata = TJAMetadata();
    ex_data = TJAEXData();

    // logger.debug("Parsing TJA file: " + file_path.string());

    get_metadata();

    master_notes = NoteList();
    branch_m = std::deque<NoteList>();
    branch_e = std::deque<NoteList>();
    branch_n = std::deque<NoteList>();
}

void TJAParser::get_metadata() {
        int current_diff = -1;

        for (const auto& item : data) {
            if (item.find("#BRANCH") == 0 && current_diff != -1) {
                metadata.course_data[current_diff].is_branching = true;
            }
            else if (item[0] == '#' || std::isdigit(static_cast<unsigned char>(item[0]))) {
                continue;
            }
            else if (item.find("SUBTITLE") == 0) {
                std::string region_code = "en";
                if (item[8] != ':') {
                    region_code = to_lower(item.substr(8, 2));
                }
                std::string value = join_after_colon(item);
                replace_all(value, "--", "");
                metadata.subtitle[region_code] = value;

                if (metadata.subtitle.find("ja") != metadata.subtitle.end() &&
                    metadata.subtitle["ja"].find("限定") != std::string::npos) {
                    ex_data.limited_time = true;
                }
            }
            else if (item.find("TITLE") == 0) {
                std::string region_code = "en";
                if (item[5] != ':') {
                    region_code = to_lower(item.substr(5, 2));
                }
                metadata.title[region_code] = join_after_colon(item);
            }
            else if (item.find("BPM") == 0) {
                std::string data_str = split_after_colon(item);
                if (data_str.empty()) {
                    // logger.warning("Invalid BPM value in TJA file");
                    metadata.bpm = 0.0f;
                } else {
                    metadata.bpm = std::stof(data_str);
                }
            }
            else if (item.find("WAVE") == 0) {
                std::string data_str = trim(split_after_colon(item));
                std::filesystem::path wave_path = file_path.parent_path() / data_str;

                if (!std::filesystem::exists(wave_path)) {
                    // logger.error(data_str + ", " + file_path.string());
                    // logger.warning("Invalid WAVE value in TJA file");
                    metadata.wave = std::filesystem::path();
                } else {
                    metadata.wave = wave_path;
                }
            }
            else if (item.find("OFFSET") == 0) {
                std::string data_str = split_after_colon(item);
                if (data_str.empty()) {
                    // logger.warning("Invalid OFFSET value in TJA file");
                    metadata.offset = 0.0f;
                } else {
                    metadata.offset = std::stof(data_str);
                }
            }
            else if (item.find("DEMOSTART") == 0) {
                std::string data_str = split_after_colon(item);
                if (data_str.empty()) {
                    // logger.warning("Invalid DEMOSTART value in TJA file");
                    metadata.demostart = 0.0f;
                } else {
                    metadata.demostart = std::stof(data_str);
                }
            }
            else if (item.find("BGMOVIE") == 0) {
                std::string data_str = split_after_colon(item);
                if (data_str.empty()) {
                    // logger.warning("Invalid BGMOVIE value in TJA file");
                    metadata.bgmovie = std::filesystem::path();
                } else {
                    metadata.bgmovie = file_path.parent_path() / trim(data_str);
                }
            }
            else if (item.find("MOVIEOFFSET") == 0) {
                std::string data_str = split_after_colon(item);
                if (data_str.empty()) {
                    // logger.warning("Invalid MOVIEOFFSET value in TJA file");
                    metadata.movieoffset = 0.0f;
                } else {
                    metadata.movieoffset = std::stof(data_str);
                }
            }
            else if (item.find("SCENEPRESET") == 0) {
                metadata.scene_preset = split_after_colon(item);
            }
            else if (item.find("COURSE") == 0) {
                std::string course = to_lower(trim(split_after_colon(item)));

                if (course == "6" || course == "dan") {
                    current_diff = 6;
                } else if (course == "5" || course == "tower") {
                    current_diff = 5;
                } else if (course == "4" || course == "edit" || course == "ura") {
                    current_diff = 4;
                } else if (course == "3" || course == "oni") {
                    current_diff = 3;
                } else if (course == "2" || course == "hard") {
                    current_diff = 2;
                } else if (course == "1" || course == "normal") {
                    current_diff = 1;
                } else if (course == "0" || course == "easy") {
                    current_diff = 0;
                } else {
                    // logger.error("Course level empty in " + file_path.string());
                }

                if (current_diff != -1) {
                    metadata.course_data[current_diff] = CourseData();
                }
            }
            else if (current_diff != -1) {
                if (item.find("LEVEL") == 0) {
                    std::string data_str = split_after_colon(item);
                    if (data_str.empty()) {
                        metadata.course_data[current_diff].level = 0;
                        // logger.warning("Invalid LEVEL value in TJA file");
                    } else {
                        metadata.course_data[current_diff].level = static_cast<int>(std::stof(data_str));
                    }
                }
                else if (item.find("BALLOONNOR") == 0) {
                    std::string balloon_data = split_after_colon(item);
                    if (!balloon_data.empty()) {
                        auto balloons = parse_balloon_data(balloon_data);
                        metadata.course_data[current_diff].balloon.insert(
                            metadata.course_data[current_diff].balloon.end(),
                            balloons.begin(), balloons.end()
                        );
                    }
                }
                else if (item.find("BALLOONEXP") == 0) {
                    std::string balloon_data = split_after_colon(item);
                    if (!balloon_data.empty()) {
                        auto balloons = parse_balloon_data(balloon_data);
                        metadata.course_data[current_diff].balloon.insert(
                            metadata.course_data[current_diff].balloon.end(),
                            balloons.begin(), balloons.end()
                        );
                    }
                }
                else if (item.find("BALLOONMAS") == 0) {
                    std::string balloon_data = split_after_colon(item);
                    if (!balloon_data.empty()) {
                        metadata.course_data[current_diff].balloon = parse_balloon_data(balloon_data);
                    }
                }
                else if (item.find("BALLOON") == 0) {
                    if (item.find(':') == std::string::npos) {
                        metadata.course_data[current_diff].balloon.clear();
                        continue;
                    }
                    std::string balloon_data = split_after_colon(item);
                    if (!balloon_data.empty()) {
                        metadata.course_data[current_diff].balloon = parse_balloon_data(balloon_data);
                    }
                }
                else if (item.find("SCOREINIT") == 0) {
                    std::string score_init = split_after_colon(item);
                    if (!score_init.empty()) {
                        try {
                            metadata.course_data[current_diff].scoreinit = parse_balloon_data(score_init);
                        } catch (const std::exception& e) {
                            // logger.error("Failed to parse SCOREINIT: " + std::string(e.what()));
                            metadata.course_data[current_diff].scoreinit = {0, 0};
                        }
                    }
                }
                else if (item.find("SCOREDIFF") == 0) {
                    std::string score_diff = split_after_colon(item);
                    if (!score_diff.empty()) {
                        metadata.course_data[current_diff].scorediff = static_cast<int>(std::stof(score_diff));
                    }
                }
            }
        }

        for (auto& [region_code, title] : metadata.title) {
            if (title.find("-New Audio-") != std::string::npos ||
                title.find("-新曲-") != std::string::npos) {
                replace_all(title, "-New Audio-", "");
                replace_all(title, "-新曲-", "");
                ex_data.new_audio = true;
            }
            else if (title.find("-Old Audio-") != std::string::npos ||
                     title.find("-旧曲-") != std::string::npos) {
                replace_all(title, "-Old Audio-", "");
                replace_all(title, "-旧曲-", "");
                ex_data.old_audio = true;
            }
            else if (title.find("限定") != std::string::npos) {
                ex_data.limited_time = true;
            }
        }
}

std::tuple<NoteList, std::deque<NoteList>, std::deque<NoteList>, std::deque<NoteList>>
TJAParser::notes_to_position(int diff) {
    auto commands = build_command_registry();
    auto notes = data_to_notes(diff);

    ParserState state;
    state.bpm = metadata.bpm;
    state.bpmchange_last_bpm = metadata.bpm;
    state.balloons = metadata.course_data[diff].balloon;
    state.curr_note_list = &master_notes.notes;
    state.curr_timeline = &master_notes.timeline;

    // Process each bar
    for (const auto& bar : notes) {
        // Calculate bar length (sum of non-command parts)
        int bar_length = 0;
        for (const auto& part : bar) {
            if (part.find('#') == std::string::npos) {
                bar_length += part.length();
            }
        }

        state.barline_added = false;

        // Process each part of the bar
        for (const auto& part : bar) {
            // Handle commands (lines starting with #)
            if (!part.empty() && part[0] == '#') {
                // Sort commands by length (longest first) for proper matching
                std::vector<std::pair<std::string, CommandHandler>> sorted_cmds(
                    commands.begin(), commands.end()
                );
                std::sort(sorted_cmds.begin(), sorted_cmds.end(),
                            [](const auto& a, const auto& b) {
                                return a.first.length() > b.first.length();
                            });

                // Find and execute matching command handler
                for (const auto& [cmd_prefix, handler] : sorted_cmds) {
                    if (part.find(cmd_prefix) == 0) {
                        std::string value = trim(part.substr(cmd_prefix.length()));
                        handler(value, state);
                        break;
                    }
                }
                continue;
            }
            // Skip unrecognized non-digit lines
            else if (!part.empty() && !std::isdigit(static_cast<unsigned char>(part[0]))) {
                // logger.warning("Unrecognized command: " + part + " in TJA " + file_path.string());
                continue;
            }

            // Add barline
            double ms_per_measure = get_ms_per_measure(state.bpm, state.time_signature);
            Note barline = add_bar(state);
            state.curr_note_list->push_back(barline);
            state.barline_added = true;
            state.index++;

            // Calculate time increment per note
            double increment;
            if (part.empty()) {
                current_ms += ms_per_measure;
                increment = 0.0f;
            } else {
                increment = ms_per_measure / static_cast<double>(bar_length);
            }

            // Process each note character
            for (char item : part) {
                // Skip empty notes (0) and non-digits
                if (item == '0' || !std::isdigit(static_cast<unsigned char>(item))) {
                    state.delay_last_note_ms = current_ms;
                    current_ms += increment;
                    continue;
                }

                // Skip consecutive balloon end markers (9)
                if (item == '9' && !state.curr_note_list->empty()) {
                    Note* last_note = &state.curr_note_list->back();
                    if (last_note && last_note->type == 9) {
                        state.delay_last_note_ms = current_ms;
                        current_ms += increment;
                        continue;
                    }
                }

                // Apply delay if present
                if (state.delay_current != 0.0f) {
                    TimelineObject delay_timeline;
                    delay_timeline.start_time = state.delay_last_note_ms;
                    delay_timeline.delay = state.delay_current;
                    state.curr_timeline->push_back(delay_timeline);
                    state.delay_current = 0.0f;
                }

                // Create and add note
                Note note = add_note(std::string(1, item), state);
                current_ms += increment;
                state.curr_note_list->push_back(note);
                state.index++;

                // Update previous note
                Note* note_ptr = &note;
                if (note_ptr) {
                    state.prev_note = *note_ptr;
                }
            }
        }
    }
    return {master_notes, branch_m, branch_e, branch_n};
}

std::vector<std::string> TJAParser::read_file_lines(const std::filesystem::path& path,
                                         const std::string& encoding) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open file: " + path.string());
        }

        std::vector<std::string> lines;
        std::string line;

        while (std::getline(file, line)) {
            lines.push_back(line);
        }

        return lines;
}

std::string TJAParser::to_lower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
                        [](unsigned char c) { return std::tolower(c); });
        return result;
}

std::string TJAParser::split_after_colon(const std::string& str) {
        size_t pos = str.find(':');
        if (pos == std::string::npos) return "";
        return str.substr(pos + 1);
}

std::string TJAParser::join_after_colon(const std::string& str) {
        size_t pos = str.find(':');
        if (pos == std::string::npos) return "";
        return str.substr(pos + 1);
}

void TJAParser::replace_all(std::string& str, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = str.find(from, pos)) != std::string::npos) {
            str.replace(pos, from.length(), to);
            pos += to.length();
        }
}

std::string TJAParser::trim(const std::string& str) {
        auto start = std::find_if(str.begin(), str.end(),
                                    [](unsigned char ch) { return !std::isspace(ch); });
        auto end = std::find_if(str.rbegin(), str.rend(),
                                [](unsigned char ch) { return !std::isspace(ch); }).base();
        return (start < end) ? std::string(start, end) : std::string();
}

std::vector<int> TJAParser::parse_balloon_data(const std::string& data) {
        std::vector<int> result;
        std::string cleaned = data;
        replace_all(cleaned, ".", ",");

        std::stringstream ss(cleaned);
        std::string token;

        while (std::getline(ss, token, ',')) {
            if (!token.empty()) {
                result.push_back(std::stoi(token));
            }
        }

        return result;
}

std::vector<std::vector<std::string>> TJAParser::data_to_notes(int diff) {
        std::string diff_name;
        auto it = DIFFS.find(diff);
        if (it != DIFFS.end()) {
            diff_name = to_lower(it->second);
        }

        int note_start = -1;
        int note_end = -1;
        bool target_found = false;
        ScrollType scroll_type = ScrollType::NMSCROLL;

        for (size_t i = 0; i < data.size(); i++) {
            const std::string& line = data[i];

            if (line.find("COURSE:") == 0) {
                std::string course_value = to_lower(trim(line.substr(7)));

                bool is_digit = !course_value.empty() &&
                               std::all_of(course_value.begin(), course_value.end(), ::isdigit);

                target_found = (is_digit && std::stoi(course_value) == diff) ||
                              course_value == diff_name;
            }
            else if (target_found) {
                if (note_start == -1 && (line == "#START" || line == "#START P1")) {
                    note_start = i + 1;
                }
                else if (line == "#END" && note_start != -1) {
                    note_end = i;
                    break;
                }
                else if (line.find("#NMSCROLL") != std::string::npos) {
                    scroll_type = ScrollType::NMSCROLL;
                    continue;
                }
                else if (line.find("#BMSCROLL") != std::string::npos) {
                    scroll_type = ScrollType::BMSCROLL;
                    continue;
                }
                else if (line.find("#HBSCROLL") != std::string::npos) {
                    scroll_type = ScrollType::HBSCROLL;
                    continue;
                }
            }
        }

        if (note_start == -1 || note_end == -1) {
            return {};
        }

        std::vector<std::vector<std::string>> notes;
        std::vector<std::string> bar;

        if (scroll_type == ScrollType::NMSCROLL) {
            bar.push_back("#NMSCROLL");
        } else if (scroll_type == ScrollType::BMSCROLL) {
            bar.push_back("#BMSCROLL");
        } else if (scroll_type == ScrollType::HBSCROLL) {
            bar.push_back("#HBSCROLL");
        }

        for (int i = note_start; i < note_end; i++) {
            const std::string& line = data[i];

            if (line[0] == '#') {
                bar.push_back(line);
            }
            else if (line == ",") {
                if (bar.empty() || std::all_of(bar.begin(), bar.end(),
                                              [](const std::string& item) {
                                                  return item[0] == '#';
                                              })) {
                    bar.push_back("");
                }
                notes.push_back(bar);
                bar.clear();
            }
            else {
                if (!line.empty() && line.back() == ',') {
                    bar.push_back(line.substr(0, line.length() - 1));
                    notes.push_back(bar);
                    bar.clear();
                } else {
                    bar.push_back(line);
                }
            }
        }

        if (!bar.empty()) {
            notes.push_back(bar);
        }

        return notes;
}

float TJAParser::apply_easing(float t, EasingPoint easing_point, EasingFunction easing_function) {
        switch (easing_point) {
            case EasingPoint::IN:
                break;
            case EasingPoint::OUT:
                t = 1.0f - t;
                break;
            case EasingPoint::IN_OUT:
                if (t < 0.5f) {
                    t = t * 2.0f;
                } else {
                    t = (1.0f - t) * 2.0f;
                }
                break;
        }

        float result;
        switch (easing_function) {
            case EasingFunction::LINEAR:
                result = t;
                break;
            case EasingFunction::CUBIC:
                result = t * t * t;  // Faster than pow for small exponents
                break;
            case EasingFunction::QUARTIC:
                result = t * t * t * t;
                break;
            case EasingFunction::QUINTIC:
                result = t * t * t * t * t;
                break;
            case EasingFunction::SINUSOIDAL:
                result = 1.0f - std::cos((t * 3.14) / 2.0f);
                break;
            case EasingFunction::EXPONENTIAL:
                result = (t == 0.0f) ? 0.0f : std::pow(2.0f, 10.0f * (t - 1.0f));
                break;
            case EasingFunction::CIRCULAR:
                result = 1.0f - std::sqrt(1.0f - t * t);
                break;
            default:
                result = t;
                break;
        }

        switch (easing_point) {
            case EasingPoint::OUT:
                result = 1.0f - result;
                break;
            case EasingPoint::IN_OUT:
                if (t >= 0.5f) {
                    result = 1.0f - result;
                }
                break;
            default:
                break;
        }

        return result;
}

#define REGISTER_HANDLER(name) \
    registry["#" #name] = [this](const std::string& v, ParserState& s) { \
        handle_##name(v, s); \
    };

std::map<std::string, TJAParser::CommandHandler> TJAParser::build_command_registry() {
        std::map<std::string, CommandHandler> registry;

        REGISTER_HANDLER(BPMCHANGE)
        REGISTER_HANDLER(MEASURE)
        REGISTER_HANDLER(SCROLL)
        REGISTER_HANDLER(GOGOSTART)
        REGISTER_HANDLER(GOGOEND)
        REGISTER_HANDLER(DELAY)
        REGISTER_HANDLER(BARLINEOFF)
        REGISTER_HANDLER(BARLINEON)
        REGISTER_HANDLER(BRANCHSTART)
        REGISTER_HANDLER(BRANCHEND)
        REGISTER_HANDLER(N)
        REGISTER_HANDLER(E)
        REGISTER_HANDLER(M)
        REGISTER_HANDLER(SECTION)
        REGISTER_HANDLER(NMSCROLL)
        REGISTER_HANDLER(BMSCROLL)
        REGISTER_HANDLER(HBSCROLL)
        REGISTER_HANDLER(SUDDEN)
        REGISTER_HANDLER(JPOSSCROLL)

        return registry;
}

#undef REGISTER_HANDLER

void TJAParser::handle_MEASURE(const std::string& value, ParserState& state) {
    size_t slash_pos = value.find('/');
    if (slash_pos != std::string::npos) {
        double num = std::stof(value.substr(0, slash_pos));
        double den = std::stof(value.substr(slash_pos + 1));
        if (den != 0.0f) {
            state.time_signature = num / den;
        }
    }
}

void TJAParser::handle_SCROLL(const std::string& value, ParserState& state) {
    if (state.scroll_type == ScrollType::BMSCROLL) return;

    if (value.find('i') != std::string::npos) {
        std::string normalized = value;
        replace_all(normalized, ".i", "j");
        replace_all(normalized, "i", "j");
        replace_all(normalized, ",", "");

        std::regex complex_regex(R"(([+-]?[0-9]*\.?[0-9]+)?([+-][0-9]*\.?[0-9]+)?j?)");
        std::smatch match;

        if (std::regex_match(normalized, match, complex_regex)) {
            double real = 0.0f;
            double imag = 0.0f;

            if (match[1].length() > 0 && match[1].str().back() != 'j') {
                real = std::stof(match[1]);
            }
            if (match[2].length() > 0) {
                std::string imag_str = match[2];
                imag = std::stof(imag_str);
            } else if (match[1].length() > 0 && normalized.back() == 'j') {
                // Purely imaginary number
                imag = std::stof(match[1]);
                real = 0.0f;
            }
            state.scroll_x_modifier = real;
            state.scroll_y_modifier = imag;
        } else {
            state.scroll_x_modifier = 1.0f;
            state.scroll_y_modifier = 0.0f;
        }
    } else {
        state.scroll_x_modifier = std::stof(value);
        state.scroll_y_modifier = 0.0f;
    }
}

void TJAParser::handle_BPMCHANGE(const std::string& value, ParserState& state) {
    double parsed_bpm = std::stof(value);

    if (state.scroll_type == ScrollType::BMSCROLL ||
        state.scroll_type == ScrollType::HBSCROLL) {
        // Do not modify bpm, it needs to be changed live by bpmchange
        double bpmchange = parsed_bpm / state.bpmchange_last_bpm;
        state.bpmchange_last_bpm = parsed_bpm;

        TimelineObject bpmchange_timeline;
        bpmchange_timeline.start_time = this->current_ms;
        bpmchange_timeline.bpmchange = bpmchange;
        state.curr_timeline->push_back(bpmchange_timeline);
    } else {
        TimelineObject timeline_obj;
        timeline_obj.start_time = this->current_ms;
        timeline_obj.bpm = parsed_bpm;
        state.bpm = parsed_bpm;
        state.curr_timeline->push_back(timeline_obj);
    }
}

void TJAParser::handle_GOGOSTART(const std::string& value, ParserState& state) {
    TimelineObject timeline_obj;
    timeline_obj.start_time = current_ms;
    timeline_obj.gogo_time = true;
    state.curr_timeline->push_back(timeline_obj);
}

void TJAParser::handle_GOGOEND(const std::string& value, ParserState& state) {
    TimelineObject timeline_obj;
    timeline_obj.start_time = current_ms;
    timeline_obj.gogo_time = false;
    state.curr_timeline->push_back(timeline_obj);
}

void TJAParser::handle_DELAY(const std::string& value, ParserState& state) {
    double delay_ms = std::stof(value) * 1000;
    if (state.scroll_type == ScrollType::BMSCROLL || state.scroll_type == ScrollType::HBSCROLL) {
        if (delay_ms > 0) {
            //Do not modify current_ms, it will be modified live
            state.delay_current += delay_ms;

            //Delays will be combined between notes, and attached to previous note
        }
    } else {
        this->current_ms += delay_ms;
    }
}

void TJAParser::handle_BARLINEOFF(const std::string& value, ParserState& state) {
    state.barline_display = false;
}

void TJAParser::handle_BARLINEON(const std::string& value, ParserState& state) {
    state.barline_display = true;
}

void TJAParser::handle_BRANCHSTART(const std::string& value, ParserState& state) {
    state.start_branch_ms = this->current_ms;
    state.start_branch_bpm = state.bpm;
    state.start_branch_time_sig = state.time_signature;
    state.start_branch_x_scroll = state.scroll_x_modifier;
    state.start_branch_y_scroll = state.scroll_y_modifier;
    state.start_branch_barline = state.barline_display;
    state.branch_balloon_index = state.balloon_index;

    std::string branch_params = value;

    TimelineObject branch_obj;
    double two_measures = (240000 / state.bpm) * 2;
    if (state.section_bar.has_value()) {
        branch_obj.start_time = state.section_bar.value().hit_ms - two_measures;
        state.section_bar.reset();
    } else {
        branch_obj.start_time = this->current_ms - two_measures;
    }
    branch_obj.branch_params = branch_params;
    state.curr_timeline->push_back(branch_obj);

    if (!this->branch_m.empty()) {
        this->branch_m.back().timeline.push_back(branch_obj);
    }
    if (!this->branch_e.empty()) {
        this->branch_e.back().timeline.push_back(branch_obj);
    }
    if (!this->branch_n.empty()) {
        this->branch_n.back().timeline.push_back(branch_obj);
    }
}

void TJAParser::handle_BRANCHEND(const std::string& value, ParserState& state) {
    state.curr_note_list = &master_notes.notes;
    state.curr_timeline = &master_notes.timeline;
    state.is_branching = false;
}

void TJAParser::handle_LYRIC(const std::string& value, ParserState& state) {
    TimelineObject timeline_obj = TimelineObject();
    timeline_obj.start_time = this->current_ms;
    timeline_obj.lyric = value;
    state.curr_timeline->push_back(timeline_obj);
}

void TJAParser::handle_JPOSSCROLL(const std::string& part, ParserState& state) {
    std::istringstream iss(part);
    std::vector<std::string> parts;
    std::string token;

    while (iss >> token) {
        parts.push_back(token);
    }

    double duration_ms = std::stof(parts[0]) * 1000;
    std::string distance_str = parts[1];
    int direction = std::stoi(parts[2]);

    double delta_x = 0.0f;
    double delta_y = 0.0f;

    if (distance_str.find('i') != std::string::npos) {
        std::string normalized = distance_str;
        replace_all(normalized, ".i", "j");
        replace_all(normalized, "i", "j");
        replace_all(normalized, ",", "");

        std::regex complex_regex(R"(([+-]?[0-9]*\.?[0-9]+)?([+-][0-9]*\.?[0-9]+)?j?)");
        std::smatch match;
        if (std::regex_match(normalized, match, complex_regex)) {
            if (match[1].length() > 0 && match[1].str().back() != 'j') {
                delta_x = std::stof(match[1]);
            }
            if (match[2].length() > 0) {
                std::string imag_str = match[2];
                delta_y = std::stof(imag_str);
            } else if (match[1].length() > 0 && normalized.back() == 'j') {
                // Purely imaginary number
                delta_y = std::stof(match[1]);
                delta_x = 0.0f;
            }
        }
    } else {
        double distance = std::stof(distance_str);
        delta_x = distance;
        delta_y = 0.0f;
    }

    if (direction == 0) {
        delta_x = -delta_x;
        delta_y = -delta_y;
    }

    // Iterate in reverse through curr_timeline
    for (auto it = state.curr_timeline->rbegin(); it != state.curr_timeline->rend(); ++it) {
        TimelineObject& obj = *it;
        if (!obj.delta_x.has_value() || !obj.delta_y.has_value()) continue;
        if (obj.start_time > this->current_ms) {
            float available_time = this->current_ms - obj.start_time;
            float total_duration = obj.end_time - obj.start_time;
            double ratio = (total_duration > 0) ? std::min(1.0f, available_time / total_duration) : 1.0f;

            obj.delta_x.value() *= ratio;
            obj.delta_y.value() *= ratio;
            obj.end_time = this->current_ms;
            break;
        }
    }

    TimelineObject jpos_scroll;
    jpos_scroll.start_time = this->current_ms;
    jpos_scroll.end_time = this->current_ms + duration_ms;
    jpos_scroll.judge_pos_x = state.judge_pos_x;
    jpos_scroll.judge_pos_y = state.judge_pos_y;
    jpos_scroll.delta_x = delta_x;
    jpos_scroll.delta_y = delta_y;

    state.curr_timeline->push_back(jpos_scroll);

    state.judge_pos_x += delta_x;
    state.judge_pos_y += delta_y;
}

void TJAParser::handle_N(const std::string& value, ParserState& state) {
    branch_n.push_back(NoteList());
    state.curr_note_list = &branch_n.back().notes;
    state.curr_timeline = &branch_n.back().timeline;
    this->current_ms = state.start_branch_ms;
    state.bpm = state.start_branch_bpm;
    state.time_signature = state.start_branch_time_sig;
    state.scroll_x_modifier = state.start_branch_x_scroll;
    state.scroll_y_modifier = state.start_branch_y_scroll;
    state.barline_display = state.start_branch_barline;
    state.balloon_index = state.branch_balloon_index;
    state.is_branching = true;
}

void TJAParser::handle_E(const std::string& value, ParserState& state) {
    branch_e.push_back(NoteList());
    state.curr_note_list = &branch_e.back().notes;
    state.curr_timeline = &branch_e.back().timeline;
    this->current_ms = state.start_branch_ms;
    state.bpm = state.start_branch_bpm;
    state.time_signature = state.start_branch_time_sig;
    state.scroll_x_modifier = state.start_branch_x_scroll;
    state.scroll_y_modifier = state.start_branch_y_scroll;
    state.barline_display = state.start_branch_barline;
    state.balloon_index = state.branch_balloon_index;
    state.is_branching = true;
}

void TJAParser::handle_M(const std::string& value, ParserState& state) {
    branch_m.push_back(NoteList());
    state.curr_note_list = &branch_m.back().notes;
    state.curr_timeline = &branch_m.back().timeline;
    this->current_ms = state.start_branch_ms;
    state.bpm = state.start_branch_bpm;
    state.time_signature = state.start_branch_time_sig;
    state.scroll_x_modifier = state.start_branch_x_scroll;
    state.scroll_y_modifier = state.start_branch_y_scroll;
    state.barline_display = state.start_branch_barline;
    state.balloon_index = state.branch_balloon_index;
    state.is_branching = true;
}

void TJAParser::handle_SECTION(const std::string& value, ParserState& state) {
    state.is_section_start = true;
}

void TJAParser::handle_NMSCROLL(const std::string& value, ParserState& state) {
    state.scroll_type = ScrollType::NMSCROLL;
}

void TJAParser::handle_BMSCROLL(const std::string& value, ParserState& state) {
    state.scroll_type = ScrollType::BMSCROLL;
}

void TJAParser::handle_HBSCROLL(const std::string& value, ParserState& state) {
    state.scroll_type = ScrollType::HBSCROLL;
}

void TJAParser::handle_SUDDEN(const std::string& value, ParserState& state) {
    std::istringstream iss(value);
    std::vector<std::string> parts;
    std::string token;

    while (iss >> token) {
        parts.push_back(token);
    }

    if (parts.size() >= 2) {
        double appear_duration = std::stof(parts[0]);
        double moving_duration = std::stof(parts[1]);
        state.sudden_appear = appear_duration * 1000;
        state.sudden_moving = moving_duration * 1000;

        if (state.sudden_appear == 0) {
            state.sudden_appear = std::numeric_limits<float>::infinity();
        }
        if (state.sudden_moving == 0) {
            state.sudden_moving = std::numeric_limits<float>::infinity();
        }
    }
}

Note TJAParser::add_bar(ParserState& state) {
    Note bar_line = Note();

    bar_line.hit_ms = this->current_ms;
    bar_line.type = 0;
    bar_line.display = state.barline_display;
    bar_line.bpm = state.bpm;
    bar_line.index = state.index;
    bar_line.scroll_x = state.scroll_x_modifier;
    bar_line.scroll_y = state.scroll_y_modifier;

    if (state.barline_added) bar_line.display = false;

    if (state.is_branching) {
        bar_line.is_branch_start = true;
        state.is_branching = false;
    }

    if (state.is_section_start) {
        state.section_bar = bar_line;
        state.is_section_start = false;
    }

    return bar_line;
}

Note TJAParser::add_note(const std::string& item, ParserState& state) {
    Note note = Note();
    note.hit_ms = this->current_ms;
    state.delay_last_note_ms = this->current_ms;
    note.display = true;
    note.type = std::stof(item);
    note.index = state.index;
    note.bpm = state.bpm;
    note.scroll_x = state.scroll_x_modifier;
    note.scroll_y = state.scroll_y_modifier;

    if (state.sudden_appear > 0 || state.sudden_moving > 0) {
        note.sudden_appear_ms = state.sudden_appear;
        note.sudden_moving_ms = state.sudden_moving;
    }

    if (note.type == 5 || note.type == 6) {
        note.color = 255;
        return note;
    } else if (note.type == 7 || note.type == 9) {
        state.balloon_index++;
        note.is_kusudama = (note.type == 9);
        note.count = (!state.balloons.empty()) ? state.balloons[0] : 1;
        if (!state.balloons.empty()) {
            state.balloons.erase(state.balloons.begin());
        }
        return note;
    } else if (note.type == 8) {
        if (!state.prev_note.has_value()) {
            throw std::runtime_error("No previous note found");
        }
    }

    return note;
}

const std::map<int, std::string> TJAParser::DIFFS = {
    {0, "easy"},
    {1, "normal"},
    {2, "hard"},
    {3, "oni"},
    {4, "edit"},
    {5, "tower"},
    {6, "dan"}
};

void modifier_speed(NoteList& notes, float value) {
    for (auto& note : notes.notes) {
        note.scroll_x *= value;
    }
}

void modifier_display(NoteList& notes) {
    for (auto& note : notes.notes) {
        note.display = false;
    }
}

void modifier_inverse(NoteList& notes) {
    std::map<int, int> type_mapping = {{1, 2}, {2, 1}, {3, 4}, {4, 3}};

    for (auto& note : notes.notes) {
        auto it = type_mapping.find(note.type);
        if (it != type_mapping.end()) {
            note.type = it->second;
        }
    }
}

void modifier_random(NoteList& notes, int value) {
    // value: 1 == kimagure, 2 == detarame

    if (value == 0 || notes.notes.empty()) return;

    int percentage = (notes.notes.size() / 5) * value;
    percentage = std::min(percentage, static_cast<int>(notes.notes.size()));

    std::random_device rd;
    std::mt19937 gen(rd());

    std::vector<int> indices(notes.notes.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), gen);

    std::vector<int> selected_notes(indices.begin(), indices.begin() + percentage);

    std::map<int, int> type_mapping = {{1, 2}, {2, 1}, {3, 4}, {4, 3}};

    for (int i : selected_notes) {
        auto it = type_mapping.find(notes.notes[i].type);
        if (it != type_mapping.end()) {
            notes.notes[i].type = it->second;
        }
    }
}

void modifier_moji(NoteList& notes) {
    static const std::map<int, int> se_notes = {
        {1, 0}, {2, 3}, {3, 5}, {4, 6},
        {5, 7}, {6, 8}, {7, 9}, {8, 10}, {9, 11}
    };

    for (Note& note : notes.notes) {
        auto it = se_notes.find(note.type);
        if (it != se_notes.end()) {
            note.moji = it->second;
        }
    }

    std::vector<Interval> intervals = {Interval::EIGHTH, Interval::TWELFTH, Interval::SIXTEENTH, Interval::TWENTYFOURTH, Interval::THIRTYSECOND};

    for (const auto& interval : intervals) {
        auto streams = find_streams(notes.notes, interval);
        for (const auto& [start, length] : streams) {
            for (int i = start; i < start + length - 1; ++i) {
                if (notes.notes[i].type == 1) {
                    notes.notes[i].moji = 1;
                } else if (notes.notes[i].type == 2) {
                    notes.notes[i].moji = 4;
                }
            }
            if (length == 3) {
                if (notes.notes[start + 0].type == 1 &&
                    notes.notes[start + 1].type == 1 &&
                    notes.notes[start + 2].type == 1) {
                    notes.notes[start + 1].moji = 2;
                }
            }
        }
    }
}

void apply_modifiers(NoteList& notes, const Modifiers& modifiers) {
    if (modifiers.display) {
        modifier_display(notes);
    }

    if (modifiers.inverse) {
        modifier_inverse(notes);
    }

    if (modifiers.random > 0) {
        modifier_random(notes, modifiers.random);
    }

    modifier_speed(notes, modifiers.speed);

    modifier_moji(notes);

    // play_notes = modifier_difficulty(notes, modifiers.subdiff);
    // draw_notes = modifier_difficulty(notes, modifiers.subdiff);
}

Interval get_note_interval_type(double interval_ms, double bpm, double time_sig) {
    if (bpm == 0) {
        return Interval::UNKNOWN;
    }

    double ms_per_measure = get_ms_per_measure(bpm, time_sig) / time_sig;
    double tolerance = 15.0;  // ms tolerance for timing classification

    double eighth_note = ms_per_measure / 8.0;
    double sixteenth_note = ms_per_measure / 16.0;
    double twelfth_note = ms_per_measure / 12.0;
    double twentyfourth_note = ms_per_measure / 24.0;
    double thirtysecond_note = ms_per_measure / 32.0;
    double quarter_note = ms_per_measure / 4.0;

    if (std::abs(interval_ms - eighth_note) < tolerance) {
        return Interval::EIGHTH;
    } else if (std::abs(interval_ms - sixteenth_note) < tolerance) {
        return Interval::SIXTEENTH;
    } else if (std::abs(interval_ms - twelfth_note) < tolerance) {
        return Interval::TWELFTH;
    } else if (std::abs(interval_ms - twentyfourth_note) < tolerance) {
        return Interval::TWENTYFOURTH;
    } else if (std::abs(interval_ms - thirtysecond_note) < tolerance) {
        return Interval::THIRTYSECOND;
    } else if (std::abs(interval_ms - quarter_note) < tolerance) {
        return Interval::QUARTER;
    }
    return Interval::UNKNOWN;
}

std::vector<std::pair<int, int>> find_streams(const std::deque<Note>& modded_notes, Interval interval_type) {
    std::vector<std::pair<int, int>> streams;
    size_t i = 0;

    while (i < modded_notes.size() - 1) {
        if (modded_notes[i].type == 5 || modded_notes[i].type == 6 ||
            modded_notes[i].type == 7 || modded_notes[i].type == 9) {
            i++;
            continue;
        }

        int stream_start = i;
        int stream_length = 1;

        while (i < modded_notes.size() - 1) {
            if (modded_notes[i + 1].type == 5 || modded_notes[i + 1].type == 6 ||
                modded_notes[i + 1].type == 7 || modded_notes[i + 1].type == 9) {
                break;
            }

            double interval = modded_notes[i + 1].hit_ms - modded_notes[i].hit_ms;
            Interval note_type = get_note_interval_type(interval, modded_notes[i].bpm);

            if (note_type == interval_type) {
                stream_length++;
                i++;
            } else {
                break;
            }
        }

        if (stream_length >= 2) {
            streams.push_back({stream_start, stream_length});
        }

        i++;
    }

    return streams;
}
