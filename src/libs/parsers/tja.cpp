#include "tja.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <numbers>
#include <numeric>
#include <random>
#include <ranges>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "../md5.h"
#include "../profiler.h"
#include "../sdl_compat.h"
#include "../text.h"

#ifdef _WIN32
#include "platform/platform_windows.h"
#endif

double get_ms_per_measure(double bpm_val, double time_sig) {
    if (bpm_val == 0) {
        return 0;
    }
    return 60000 * (time_sig * 4) / bpm_val;
}

const std::regex TJAParser::complex_number_regex(R"(([+-]?[0-9]*\.?[0-9]+)?([+-][0-9]*\.?[0-9]+)?j?)");

int calculate_base_score(const NoteList& notes) {
    int total_notes = 0;
    int balloon_count = 0;
    double drumroll_msec = 0.0f;

    for (size_t i = 0; i < notes.notes.size(); i++) {
        Note note = notes.notes[i];

        Note next_note = *((i < notes.notes.size() - 1) ? &notes.notes[i + 1] : &notes.notes[notes.notes.size() - 1]);

        if (note.color.has_value()) { // is drumroll
            drumroll_msec += (next_note.hit_ms - note.hit_ms);
        } else if (note.count.has_value()) { // is balloon
            balloon_count += std::min(100, note.count.value());
        } else {
            if (note.type == NoteType::TAIL) {
                continue;
            }
            if (NoteType::DON <= note.type && note.type <= NoteType::KAT_L) {
                total_notes += 1;
            }
        }
    }

    if (total_notes == 0) {
        return 1000000;
    }

    double calculation =
        (1000000.0f - (balloon_count * 100.0f) - (16.920079999994086f * drumroll_msec / 1000.0f * 100.0f)) /
        total_notes / 10.0f;
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

inline std::filesystem::path get_proper_path(const std::string& path, const std::string& encoding,
                                             const std::filesystem::path& base_path) {
#ifdef _WIN32
    return win32_path_from_encoded(path, encoding, base_path);
#else
    return base_path / path;
#endif
}

// One open per chart. test_encodings + read_file_lines opened the file twice, and on Windows the
// open is most of the cost of reading a chart's metadata. Same result as those two: the encoding
// from the BOM, lines split like std::getline on '\n' (a UTF-8 BOM is skipped).
static std::vector<std::string> read_lines_once(const std::filesystem::path& path, std::string& encoding) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        encoding = "";
        throw std::runtime_error("Could not open file: " + path.string());
    }
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::string raw(size > 0 ? static_cast<size_t>(size) : 0, '\0');
    if (!raw.empty()) {
        file.read(raw.data(), static_cast<std::streamsize>(raw.size()));
    }
    raw.resize(static_cast<size_t>(file.gcount() > 0 ? file.gcount() : 0));

    const auto byte = [&](size_t i) { return i < raw.size() ? static_cast<unsigned char>(raw[i]) : 0u; };
    if (byte(0) == 0xEF && byte(1) == 0xBB && byte(2) == 0xBF) {
        encoding = "utf-8-sig";
    } else if (byte(0) == 0xFF && byte(1) == 0xFE) {
        encoding = "utf-16-le";
    } else if (byte(0) == 0xFE && byte(1) == 0xFF) {
        encoding = "utf-16-be";
    } else {
        // No BOM: UTF-8 or Shift-JIS. Shift-JIS is converted here, so titles and file names are
        // UTF-8 from here on either way (it used to stay Shift-JIS and showed as '?')
        encoding = "utf-8";
        if (!is_valid_utf8(raw)) {
            raw = utf8_from_utf8_or_sjis(raw);
        }
    }

    std::vector<std::string> lines;
    size_t start = (encoding == "utf-8-sig") ? 3 : 0;
    for (size_t nl; (nl = raw.find('\n', start)) != std::string::npos; start = nl + 1) {
        lines.emplace_back(raw, start, nl - start);
    }
    if (start < raw.size()) {
        lines.emplace_back(raw, start, raw.size() - start);
    }
    return lines;
}

TJAParser::TJAParser(const std::filesystem::path& path, int start_delay, int player_num)
    : file_path(path), start_ms(static_cast<double>(start_delay)), current_ms(static_cast<double>(start_delay)),
      player_num(player_num) {
    PROFILE_SCOPE();

    std::vector<std::string> lines = read_lines_once(file_path, encoding);

    for (const auto& line : lines) {
        std::string cleaned;
        size_t comment_pos = line.find("//");
        if (comment_pos == std::string::npos) {
            cleaned = line;
        } else if (comment_pos > 0) {
            std::string prefix = line.substr(0, comment_pos);
            auto non_space = std::ranges::find_if(prefix, [](unsigned char c) { return !std::isspace(c); });
            if (non_space != prefix.end()) {
                cleaned = std::move(prefix);
            }
        }

        size_t start = cleaned.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            continue;
        }
        size_t end = cleaned.find_last_not_of(" \t\r\n");
        data.push_back(cleaned.substr(start, end - start + 1));
    }

    metadata = TJAMetadata();
    ex_data = TJAEXData();

    try {
        get_metadata();
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse TJA metadata: " + std::string(e.what()));
    }

    master_notes = NoteList();
    branch_m = std::deque<NoteList>();
    branch_e = std::deque<NoteList>();
    branch_n = std::deque<NoteList>();
}

void TJAParser::get_metadata() {
    struct NumericField {
        std::string_view keyword;
        double TJAMetadata::* member;
    };

    static constexpr NumericField numeric_fields[] = {
        {"BPM", &TJAMetadata::bpm},
        {"OFFSET", &TJAMetadata::offset},
        {"DEMOSTART", &TJAMetadata::demostart},
        {"MOVIEOFFSET", &TJAMetadata::movieoffset},
    };

    int current_diff = -1;
    // A course can hold one chart per STYLE (Single / Double), each with its own BALLOON,
    // SCOREINIT and SCOREDIFF. A chart uses the values in effect at its #START, where a STYLE
    // section starts again from the course's values before the first STYLE line.
    struct StartValues {
        int diff;
        std::vector<int> balloon;
        std::vector<int> scoreinit;
        double scorediff;
    };
    std::map<size_t, StartValues> start_values;
    std::map<int, CourseData> style_base;

    for (size_t i = 0; i < data.size(); i++) {
        const std::string& item = data[i];
        if (item.starts_with("#START") && current_diff != -1) {
            const CourseData& cd = metadata.course_data[current_diff];
            start_values[i] = {current_diff, cd.balloon, cd.scoreinit, cd.scorediff};
        }
        if (item.starts_with("#BRANCH") && current_diff != -1) {
            metadata.course_data[current_diff].is_branching = true;
        } else if (item[0] == '#' || std::isdigit(static_cast<unsigned char>(item[0]))) {
            continue;
        } else if (item.starts_with("SUBTITLE")) {
            std::string region_code = "en";
            if (item.size() > 8 && item[8] != ':') {
                region_code = to_lower(item.substr(8, 2));
            }
            std::string value = join_after_colon(item);
            if (value.starts_with("++")) {
                metadata.subtitle_full_display = true;
                value = value.substr(2);
            } else if (value.starts_with("/ ")) {
                metadata.subtitle_full_display = true;
                value = value.substr(2);
            } else if (value.starts_with("--")) {
                metadata.subtitle_full_display = false;
                value = value.substr(2);
            }
            metadata.subtitle[region_code] = value;

            if (metadata.subtitle.contains("ja") && metadata.subtitle["ja"].contains("限定")) {
                ex_data.limited_time = true;
            }
        } else if (item.starts_with("TITLE")) {
            std::string region_code = "en";
            if (item.size() > 5 && item[5] != ':') {
                region_code = to_lower(item.substr(5, 2));
            }
            metadata.title[region_code] = join_after_colon(item);
        } else if (const auto field = std::ranges::find_if(
                       numeric_fields, [&](const NumericField& field) { return item.starts_with(field.keyword); });
                   field != std::end(numeric_fields)) {
            std::string data_str = split_after_colon(item);
            auto& value = metadata.*(field->member);
            if (data_str.empty()) {
                value = 0.0f;
            } else {
                try {
                    value = std::stof(data_str);
                } catch (const std::exception&) {
                    spdlog::warn("Invalid {} value '{}' in {}", field->keyword, data_str, file_path.string());
                    value = 0.0f;
                }
            }
        } else if (item.starts_with("WAVE")) {
            std::string data_str = trim(split_after_colon(item));

            fs::path wave_path = get_proper_path(data_str, encoding, file_path.parent_path());
            metadata.wave = wave_path;
        } else if (item.starts_with("BGMOVIE")) {
            std::string data_str = split_after_colon(item);
            if (data_str.empty()) {
                spdlog::warn("Invalid BGMOVIE value in TJA file: {}", file_path.string());
                metadata.bgmovie = std::filesystem::path();
            } else {
                metadata.bgmovie = get_proper_path(trim(data_str), encoding, file_path.parent_path());
            }
        } else if (item.starts_with("PREIMAGE")) {
            std::string data_str = split_after_colon(item);
            if (data_str.empty()) {
                metadata.preimage = std::filesystem::path();
            } else {
                metadata.preimage = get_proper_path(trim(data_str), encoding, file_path.parent_path());
            }
        } else if (item.starts_with("SCENEPRESET")) {
            metadata.scene_preset = split_after_colon(item);
        } else if (item.starts_with("COURSE")) {
            std::string course = to_lower(trim(split_after_colon(item)));

            struct CourseAlias {
                std::string_view name;
                int difficulty;
            };

            static constexpr CourseAlias course_aliases[] = {
                {"0", 0}, {"easy", 0}, {"1", 1},   {"normal", 1}, {"2", 2},     {"hard", 2}, {"3", 3},   {"oni", 3},
                {"4", 4}, {"edit", 4}, {"ura", 4}, {"5", 5},      {"tower", 5}, {"6", 6},    {"dan", 6},
            };

            const auto alias = std::ranges::find(course_aliases, course, &CourseAlias::name);
            current_diff = alias != std::end(course_aliases) ? alias->difficulty : -1;

            if (current_diff == -1) {
                spdlog::warn("Unrecognized COURSE value '{}' in {}", course, file_path.string());
            }

            if (current_diff != -1) {
                metadata.course_data[current_diff] = CourseData();
                style_base.erase(current_diff);
            }
        } else if (current_diff != -1) {
            if (item.starts_with("LEVEL")) {
                std::string data_str = split_after_colon(item);
                if (data_str.empty()) {
                    metadata.course_data[current_diff].level = 0;
                } else {
                    try {
                        metadata.course_data[current_diff].level = static_cast<int>(std::stof(data_str));
                    } catch (const std::exception&) {
                        spdlog::warn("Invalid LEVEL value '{}' in {}", data_str, file_path.string());
                        metadata.course_data[current_diff].level = 0;
                    }
                }
            } else if (item.starts_with("STYLE")) {
                CourseData& cd = metadata.course_data[current_diff];
                const CourseData& base = style_base.try_emplace(current_diff, cd).first->second;
                cd.balloon = base.balloon;
                cd.scoreinit = base.scoreinit;
                cd.scorediff = base.scorediff;
            } else if (item.starts_with("BALLOON")) {
                struct BalloonRule {
                    std::string_view keyword;
                    bool append;
                    bool clear_without_colon;
                };

                static constexpr BalloonRule rules[] = {
                    {.keyword = "BALLOONNOR", .append = true, .clear_without_colon = false},
                    {.keyword = "BALLOONEXP", .append = true, .clear_without_colon = false},
                    {.keyword = "BALLOONMAS", .append = false, .clear_without_colon = false},
                    {.keyword = "BALLOON", .append = false, .clear_without_colon = true},
                };

                const auto rule = std::ranges::find_if(
                    rules, [&](const BalloonRule& rule) { return item.starts_with(rule.keyword); });

                auto& balloons = metadata.course_data[current_diff].balloon;
                if (rule->clear_without_colon && !item.contains(':')) {
                    balloons.clear();
                    continue;
                }

                std::string balloon_data = split_after_colon(item);
                if (!balloon_data.empty()) {
                    try {
                        auto parsed = parse_balloon_data(balloon_data);
                        if (rule->append) {
                            balloons.insert(balloons.end(), parsed.begin(), parsed.end());
                        } else {
                            balloons = std::move(parsed);
                        }
                    } catch (const std::exception& e) {
                        spdlog::warn("Invalid {} value '{}' in {}: {}", rule->keyword, balloon_data, file_path.string(),
                                     e.what());
                    }
                }
            } else if (item.starts_with("SCOREINIT")) {
                std::string score_init = split_after_colon(item);
                if (!score_init.empty()) {
                    try {
                        metadata.course_data[current_diff].scoreinit = parse_balloon_data(score_init);
                    } catch (const std::exception& e) {
                        spdlog::warn("Failed to parse SCOREINIT: " + std::string(e.what()));
                        metadata.course_data[current_diff].scoreinit = {0, 0};
                    }
                }
            } else if (item.starts_with("SCOREDIFF")) {
                std::string score_diff = split_after_colon(item);
                if (!score_diff.empty()) {
                    try {
                        metadata.course_data[current_diff].scorediff = static_cast<int>(std::stof(score_diff));
                    } catch (const std::exception&) {
                        spdlog::warn("Invalid SCOREDIFF value '{}' in {}", score_diff, file_path.string());
                    }
                }
            }
        }
    }

    // The values of the chart this player plays, not of a later STYLE section or of header
    // lines between its notes
    for (auto& [diff, cd] : metadata.course_data) {
        const int start = find_chart(diff).start;
        auto it = start > 0 ? start_values.find(static_cast<size_t>(start - 1)) : start_values.end();
        if (it != start_values.end() && it->second.diff == diff) {
            cd.balloon = it->second.balloon;
            cd.scoreinit = it->second.scoreinit;
            cd.scorediff = it->second.scorediff;
        }
    }

    for (auto& [region_code, title] : metadata.title) {
        if (title.contains("-New Audio-") || title.contains("-新曲-")) {
            replace_all(title, "-New Audio-", "");
            replace_all(title, "-新曲-", "");
            ex_data.new_audio = true;
        } else if (title.contains("-Old Audio-") || title.contains("-旧曲-")) {
            replace_all(title, "-Old Audio-", "");
            replace_all(title, "-旧曲-", "");
            ex_data.old_audio = true;
        } else if (title.contains("限定")) {
            ex_data.limited_time = true;
        }
    }
}

std::tuple<NoteList, std::deque<NoteList>, std::deque<NoteList>, std::deque<NoteList>>
TJAParser::notes_to_position(int diff) {
    PROFILE_SCOPE();
    if (!metadata.course_data.contains(diff)) {
        return std::make_tuple(NoteList(), std::deque<NoteList>(), std::deque<NoteList>(), std::deque<NoteList>());
    }
    last_diff = diff;
    current_ms = start_ms;
    master_notes = NoteList();
    branch_m = std::deque<NoteList>();
    branch_e = std::deque<NoteList>();
    branch_n = std::deque<NoteList>();
    if (cached_cmds.empty()) {
        auto cmds = build_command_registry();
        cached_cmds = std::vector<std::pair<std::string, CommandHandler>>(cmds.begin(), cmds.end());
        std::ranges::sort(cached_cmds,
                          [](const auto& a, const auto& b) { return a.first.length() > b.first.length(); });
    }
    auto notes = data_to_notes(diff);

    ParserState state;
    state.bpm = metadata.bpm;
    state.balloons = metadata.course_data[diff].balloon;
    state.curr_note_list = &master_notes.notes;
    state.curr_timeline = &master_notes.timeline;
    master_notes.tempo_map.points = {{current_ms, 0.0, metadata.bpm}};
    master_notes.tempo_map.lead_bpm = metadata.bpm;

    // Process each bar
    for (const auto& bar : notes) {
        // Calculate bar length (sum of non-command parts)
        int bar_length = 0;
        for (const auto& part : bar) {
            if (part.empty() || part[0] != '#') {
                bar_length += part.length();
            }
        }

        state.barline_added = false;

        // Process each part of the bar
        for (const auto& part : bar) {
            // Handle commands (lines starting with #)
            if (!part.empty() && part[0] == '#') {
                for (const auto& [cmd_prefix, handler] : cached_cmds) {
                    if (part.starts_with(cmd_prefix)) {
                        std::string value = trim(part.substr(cmd_prefix.length()));
                        handler(value, state);
                        break;
                    }
                }
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
                increment = (bar_length > 0) ? ms_per_measure / static_cast<double>(bar_length) : 0.0;
            }

            // Process each note character
            for (char item : part) {
                // Skip empty notes (0) and non-digits
                if (item == '0' || !std::isdigit(static_cast<unsigned char>(item))) {
                    if (item != '0') {
                        spdlog::warn("Unrecognized note '{}' in {}", item, file_path.string());
                    }
                    current_ms += increment;
                    continue;
                }

                // Skip consecutive balloon end markers (9)
                if (item == '9' && !state.curr_note_list->empty()) {
                    Note* last_note = &state.curr_note_list->back();
                    if (last_note && last_note->type == NoteType::KUSUDAMA) {
                        current_ms += increment;
                        continue;
                    }
                }

                // Create and add note
                Note note = add_note(item, state);
                current_ms += increment;
                state.curr_note_list->push_back(note);
                state.index++;
            }
        }
    }
    master_notes.scroll_type = state.scroll_type;
    return {master_notes, branch_m, branch_e, branch_n};
}

std::vector<std::string> TJAParser::read_file_lines(const std::filesystem::path& path, const std::string& encoding) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + path.string());
    }

    if (encoding == "utf-8-sig") {
        file.seekg(3); // skip 3-byte UTF-8 BOM
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
    std::ranges::transform(result, result.begin(), [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string TJAParser::split_after_colon(const std::string& str) {
    size_t pos = str.find(':');
    if (pos == std::string::npos) {
        return "";
    }
    return str.substr(pos + 1);
}

std::string TJAParser::join_after_colon(const std::string& str) {
    size_t pos = str.find(':');
    if (pos == std::string::npos) {
        return "";
    }
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
    auto start = std::ranges::find_if(str, [](unsigned char ch) { return !std::isspace(ch); });
    auto end =
        std::ranges::find_if(std::views::reverse(str), [](unsigned char ch) { return !std::isspace(ch); }).base();
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
            int parsed_token;
            try {
                parsed_token = std::stoi(token);
            } catch (const std::invalid_argument&) {
                throw std::invalid_argument("Invalid balloon data: " + token);
            } catch (const std::out_of_range&) {
                spdlog::error("Balloon data out of range: {}", token);
                parsed_token = std::numeric_limits<int>::max();
            }
            result.push_back(parsed_token);
        }
    }

    return result;
}

TJAParser::ChartRange TJAParser::find_chart(int diff) {
    std::string diff_name;
    auto it = DIFFS.find(diff);
    if (it != DIFFS.end()) {
        diff_name = to_lower(it->second);
    }

    int note_start = -1;
    int note_end = -1;
    int p1_start = -1;
    int p2_start = -1;
    bool target_found = false;
    ScrollType scroll_type = ScrollType::NMSCROLL;

    for (size_t i = 0; i < data.size(); i++) {
        const std::string& line = data[i];

        if (line.starts_with("COURSE:")) {
            std::string course_value = to_lower(trim(line.substr(7)));

            bool is_digit = !course_value.empty() &&
                            std::ranges::all_of(course_value, [](unsigned char c) { return std::isdigit(c) != 0; });

            target_found = (is_digit && std::stoi(course_value) == diff) || course_value == diff_name;
        } else if (target_found) {
            if (note_start == -1) {
                if (line == "#START") {
                    note_start = i + 1;
                } else if (line == "#START P1" && p1_start == -1) {
                    p1_start = i + 1;
                } else if (line == "#START P2" && p2_start == -1) {
                    p2_start = i + 1;
                }
            }
            if (line == "#END" && note_start != -1) {
                note_end = i;
                break;
            }
            // Like Taiko-san Jiro, the scroll mode only counts before #START
            bool started = note_start != -1 || p1_start != -1 || p2_start != -1;
            if (line.contains("#NMSCROLL")) {
                if (!started) {
                    scroll_type = ScrollType::NMSCROLL;
                }
                continue;
            }

            if (line.contains("#BMSCROLL")) {
                if (!started) {
                    scroll_type = ScrollType::BMSCROLL;
                }
                continue;
            }

            if (line.contains("#HBSCROLL")) {
                if (!started) {
                    scroll_type = ScrollType::HBSCROLL;
                }
                continue;
            }
        }
    }

    if (note_start != -1) {
        if (player_num == 1 && p1_start != -1) {
            note_start = p1_start;
        } else if (player_num == 2 && p2_start != -1) {
            note_start = p2_start;
        }
    } else {
        if (player_num == 1 && p1_start != -1) {
            note_start = p1_start;
        } else if (player_num == 2 && p2_start != -1) {
            note_start = p2_start;
        } else if (player_num == 0 && p1_start != -1) {
            note_start = p1_start;
        } else if (player_num == 0 && p2_start != -1) {
            note_start = p2_start;
        }
    }

    if (note_start != -1 && note_end == -1) {
        for (size_t i = note_start; i < data.size(); i++) {
            if (data[i] == "#END") {
                note_end = i;
                break;
            }
        }
    }

    if (note_start == -1 || note_end == -1) {
        return {};
    }
    return {note_start, note_end, scroll_type};
}

std::vector<std::vector<std::string>> TJAParser::data_to_notes(int diff) {
    const ChartRange range = find_chart(diff);
    if (range.start == -1 || range.end == -1) {
        return {};
    }
    const int note_start = range.start;
    const int note_end = range.end;
    const ScrollType scroll_type = range.scroll_type;

    std::vector<std::vector<std::string>> notes;
    std::vector<std::string> bar;

    if (scroll_type == ScrollType::NMSCROLL) {
        bar.emplace_back("#NMSCROLL");
    } else if (scroll_type == ScrollType::BMSCROLL) {
        bar.emplace_back("#BMSCROLL");
    } else if (scroll_type == ScrollType::HBSCROLL) {
        bar.emplace_back("#HBSCROLL");
    }

    for (int i = note_start; i < note_end; i++) {
        const std::string& line = data[i];

        if (line[0] == '#') {
            if (line.starts_with("#NMSCROLL") || line.starts_with("#BMSCROLL") || line.starts_with("#HBSCROLL")) {
                continue;
            }
            bar.push_back(line);
        } else if (line == ",") {
            if (bar.empty() || std::ranges::all_of(bar, [](const std::string& item) { return item[0] == '#'; })) {
                bar.emplace_back("");
            }
            notes.push_back(bar);
            bar.clear();
        } else {
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

#define REGISTER_HANDLER(name)                                                                                         \
    registry["#" #name] = [this](const std::string& v, ParserState& s) { handle_##name(v, s); };

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
    REGISTER_HANDLER(LYRIC)

    register_extended_commands(registry);
    return registry;
}

#undef REGISTER_HANDLER

// TJAPlayer3-Extended commands
void TJAParser::register_extended_commands(std::map<std::string, CommandHandler>& registry) {
    auto split = [](const std::string& v) {
        std::vector<std::string> out;
        std::istringstream iss(v);
        std::string tok;
        while (std::getline(iss, tok, ',')) {
            out.push_back(trim(tok));
        }
        return out;
    };
    auto push = [this](ParserState& s, CameraEvent ev) -> TimelineObject* {
        TimelineObject t = TimelineObject();
        t.start_time = t.end_time = this->current_ms;
        t.cam = ev;
        s.curr_timeline->push_back(t);
        return &s.curr_timeline->back(); // deque: stays valid across later push_back
    };
    auto num = [this](const std::string& cmd, const std::vector<std::string>& a, size_t n, std::vector<double>& out) {
        if (a.size() < n) {
            spdlog::warn("Malformed {} in {}", cmd, file_path.string());
            return false;
        }
        try {
            for (size_t i = 0; i < n; i++) {
                out.push_back(std::stod(a[i]));
            }
        } catch (const std::exception&) {
            spdlog::warn("Invalid {} values in {}", cmd, file_path.string());
            return false;
        }
        return true;
    };

    struct Prop {
        const char* name;
        CamProp prop;
    };
    for (auto [name, prop] : {Prop{"HOFFSET", CamProp::H_OFFSET},
                              {"VOFFSET", CamProp::V_OFFSET},
                              {"ZOOM", CamProp::ZOOM},
                              {"ROTATION", CamProp::ROTATION},
                              {"HSCALE", CamProp::H_SCALE},
                              {"VSCALE", CamProp::V_SCALE}}) {
        std::string cmd = std::string("#CAM") + name;
        registry[cmd] = [=](const std::string& v, ParserState& s) {
            std::vector<double> d;
            if (!num(cmd, split(v), 1, d)) {
                return;
            }
            CameraEvent ev;
            ev.prop = prop;
            ev.to = d[0];
            push(s, ev);
        };
        // HOFFSET/VOFFSET have no eased form upstream; they are #CAMHMOVE/#CAMVMOVE
        std::string base = prop == CamProp::H_OFFSET ? "#CAMHMOVE" : prop == CamProp::V_OFFSET ? "#CAMVMOVE" : cmd;
        registry[base + "START"] = [=](const std::string& v, ParserState& s) {
            auto a = split(v);
            std::vector<double> d;
            if (!num(base + "START", a, 2, d)) {
                return;
            }
            static const std::map<std::string, EaseDir> dirs = {
                {"IN", EaseDir::IN_}, {"OUT", EaseDir::OUT_}, {"IN_OUT", EaseDir::IN_OUT}};
            static const std::map<std::string, EaseCalc> calcs = {{"CUBIC", EaseCalc::CUBIC},
                                                                  {"QUARTIC", EaseCalc::QUARTIC},
                                                                  {"QUINTIC", EaseCalc::QUINTIC},
                                                                  {"SINUSOIDAL", EaseCalc::SINUSOIDAL},
                                                                  {"EXPONENTIAL", EaseCalc::EXPONENTIAL},
                                                                  {"CIRCULAR", EaseCalc::CIRCULAR},
                                                                  {"LINEAR", EaseCalc::LINEAR}};
            CameraEvent ev;
            ev.prop = prop;
            ev.from = d[0];
            ev.to = d[1];
            ev.ease = true;
            if (a.size() > 2 && dirs.contains(a[2])) {
                ev.dir = dirs.at(a[2]);
            }
            if (a.size() > 3 && calcs.contains(a[3])) {
                ev.calc = calcs.at(a[3]);
            }
            s.open_cam[prop] = push(s, ev);
        };
        registry[base + "END"] = [=, this](const std::string&, ParserState& s) {
            auto it = s.open_cam.find(prop);
            if (it == s.open_cam.end()) {
                return;
            }
            it->second->end_time = this->current_ms;
            s.open_cam.erase(it);
        };
    }
    registry["#CAMRESET"] = [=](const std::string&, ParserState& s) {
        CameraEvent ev;
        ev.prop = CamProp::RESET;
        push(s, ev);
    };
    registry["#BORDERCOLOR"] = [=](const std::string& v, ParserState& s) {
        std::vector<double> d;
        if (!num("#BORDERCOLOR", split(v), 3, d)) {
            return;
        }
        CameraEvent ev;
        ev.prop = CamProp::BORDER_COLOR;
        ev.from = d[0];
        ev.to = d[1];
        ev.extra = d[2];
        push(s, ev);
    };
    registry["#ENABLEDORON"] = [](const std::string&, ParserState& s) { s.doron = true; };
    registry["#DISABLEDORON"] = [](const std::string&, ParserState& s) { s.doron = false; };
}

void TJAParser::handle_MEASURE(const std::string& value, ParserState& state) {
    size_t slash_pos = value.find('/');
    if (slash_pos != std::string::npos) {
        try {
            double num = std::stof(value.substr(0, slash_pos));
            double den = std::stof(value.substr(slash_pos + 1));
            // #BMSCROLL / #HBSCROLL charts pair a negative measure with a negative BPM (time
            // still moves forward while the beat count runs back), or use a zero-length measure
            // and space its notes with #DELAY
            bool beat_scroll = state.scroll_type != ScrollType::NMSCROLL;
            if (den != 0.0f && (num > 0.0f || beat_scroll)) {
                state.time_signature = num / den;
            } else {
                spdlog::warn("Ignoring degenerate #MEASURE {} in {}", value, file_path.string());
            }
        } catch (const std::exception&) {
            spdlog::warn("Invalid #MEASURE value '{}' in {}", value, file_path.string());
        }
    }
}

double safe_stof(const std::string& str) {
    try {
        return std::stof(str);
    } catch (const std::invalid_argument&) {
        return std::numeric_limits<double>::max();
    } catch (const std::out_of_range&) {
        return std::numeric_limits<double>::max();
    }
}

void TJAParser::handle_SCROLL(const std::string& value, ParserState& state) {
    if (state.scroll_type == ScrollType::BMSCROLL) {
        return;
    }
    if (scroll_disabled) {
        return;
    }

    if (value.empty()) {
        spdlog::warn("Empty #SCROLL value in {}", file_path.string());
        return;
    }

    // Polar form (TaikoManyGimmicks): #SCROLL <speed>, <rotation-lower>, <rotation-upper>, the
    // speed rotated rotation-upper/rotation-lower turns counterclockwise
    if (value.contains(',') && !value.contains('i')) {
        std::vector<double> parts;
        std::stringstream ss(value);
        std::string part;
        try {
            while (std::getline(ss, part, ',')) {
                parts.push_back(std::stod(trim(part)));
            }
        } catch (const std::exception&) {
            parts.clear();
        }
        if (parts.size() != 3 || parts[1] == 0.0) {
            spdlog::warn("Invalid polar #SCROLL value '{}' in {}", value, file_path.string());
            return;
        }
        double angle = 2.0 * std::numbers::pi * parts[2] / parts[1];
        state.scroll_x_modifier = parts[0] * std::cos(angle);
        state.scroll_y_modifier = parts[0] * std::sin(angle);
        return;
    }

    if (value.contains('i')) {
        std::string normalized = value;
        replace_all(normalized, ".i", "j");
        replace_all(normalized, "i", "j");
        replace_all(normalized, ",", "");

        std::smatch match;

        if (std::regex_match(normalized, match, complex_number_regex)) {
            double real = 0.0f;
            double imag = 0.0f;
            try {
                if (match[1].length() > 0 && match[1].str().back() != 'j') {
                    real = std::stof(match[1]);
                }
                if (match[2].length() > 0) {
                    std::string imag_str = match[2];
                    imag = std::stof(imag_str);
                } else if (match[1].length() > 0 && normalized.back() == 'j') {
                    imag = std::stof(match[1]);
                    real = 0.0f;
                }
            } catch (const std::exception&) {
                spdlog::warn("Invalid #SCROLL complex value '{}' in {}", value, file_path.string());
                return;
            }
            state.scroll_x_modifier = real;
            state.scroll_y_modifier = imag;
        } else {
            spdlog::warn("Malformed #SCROLL complex value '{}' in {}", value, file_path.string());
        }
    } else {
        try {
            state.scroll_x_modifier = std::stof(value);
            state.scroll_y_modifier = 0.0f;
        } catch (const std::exception&) {
            spdlog::warn("Invalid #SCROLL value '{}' in {}", value, file_path.string());
        }
    }
}

void TJAParser::handle_BPMCHANGE(const std::string& value, ParserState& state) {
    if (value.empty()) {
        spdlog::warn("Empty #BPMCHANGE value in {}", file_path.string());
        return;
    }
    double parsed_bpm;
    try {
        parsed_bpm = std::stof(value);
    } catch (const std::exception&) {
        spdlog::warn("Invalid #BPMCHANGE value '{}' in {}", value, file_path.string());
        return;
    }

    TimelineObject timeline_obj;
    timeline_obj.start_time = this->current_ms;
    timeline_obj.bpm = parsed_bpm;
    state.bpm = parsed_bpm;
    state.curr_timeline->push_back(timeline_obj);
    add_tempo_point(parsed_bpm);
}

// Branches rewind current_ms; their repeated tempo changes land before the last point and are
// skipped, so the beat map follows the first branch.
void TJAParser::add_tempo_point(double bpm) {
    auto& points = master_notes.tempo_map.points;
    if (current_ms < points.back().ms) {
        return;
    }
    if (current_ms == points.back().ms) {
        points.back().bpm = bpm;
        return;
    }
    points.push_back({current_ms, master_notes.tempo_map.beat_at(current_ms), bpm});
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
    if (value.empty()) {
        spdlog::warn("Empty #DELAY value in {}", file_path.string());
        return;
    }
    double delay_ms;
    try {
        delay_ms = std::stof(value) * 1000;
    } catch (const std::exception&) {
        spdlog::warn("Invalid #DELAY value '{}' in {}", value, file_path.string());
        return;
    }
    if (delay_ms > 0) {
        // The beat count stands still through a delay (seen with #BMSCROLL / #HBSCROLL)
        add_tempo_point(0.0);
        this->current_ms += delay_ms;
        add_tempo_point(state.bpm);
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
    state.branch_balloon_cursor = state.balloon_cursor;

    std::string branch_params = value;

    TimelineObject branch_obj;
    double two_measures = get_ms_per_measure(state.bpm, state.time_signature) * 2.0;
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

    if (parts.size() < 3) {
        spdlog::warn("Malformed #JPOSSCROLL (expected 3 arguments) in {}", file_path.string());
        return;
    }
    double duration_ms;
    int direction;
    try {
        duration_ms = std::stof(parts[0]) * 1000;
        direction = std::stoi(parts[2]);
    } catch (const std::exception&) {
        spdlog::warn("Invalid #JPOSSCROLL values in {}", file_path.string());
        return;
    }
    std::string distance_str = parts[1];
    replace_all(distance_str, ",", "");

    // A component is pixels ("-94") or, as in TaikoManyGimmicks, a fraction of the default note
    // field width ("3/5"); the distance is one component or a complex "x+yi" / "x-yi" / "yi"
    auto component = [&](std::string c, double& out) -> bool {
        if (c.empty() || c == "+" || c == "-") {
            c += "1";
        }
        size_t slash = c.find('/');
        try {
            if (slash == std::string::npos) {
                out = std::stod(c);
            } else {
                double den = std::stod(c.substr(slash + 1));
                if (den == 0.0) {
                    return false;
                }
                out = std::stod(c.substr(0, slash)) / den * jpos_field_width;
            }
        } catch (const std::exception&) {
            return false;
        }
        return true;
    };

    double delta_x = 0.0;
    double delta_y = 0.0;
    bool valid;
    if (!distance_str.empty() && distance_str.back() == 'i') {
        std::string body = distance_str.substr(0, distance_str.size() - 1);
        size_t split = std::string::npos;
        for (size_t k = body.size(); k-- > 1;) {
            char prev = body[k - 1];
            if ((body[k] == '+' || body[k] == '-') && prev != 'e' && prev != 'E' && prev != '/') {
                split = k;
                break;
            }
        }
        if (split == std::string::npos) {
            valid = component(body.empty() ? "1" : body, delta_y);
        } else {
            valid = component(body.substr(0, split), delta_x) && component(body.substr(split), delta_y);
        }
    } else {
        valid = component(distance_str, delta_x);
    }
    if (!valid) {
        spdlog::warn("Invalid #JPOSSCROLL distance '{}' in {}", distance_str, file_path.string());
        return;
    }

    if (direction == 0) {
        delta_x = -delta_x;
        delta_y = -delta_y;
    }

    // Iterate in reverse through curr_timeline
    for (auto it = state.curr_timeline->rbegin(); it != state.curr_timeline->rend(); ++it) {
        TimelineObject& obj = *it;
        if (!obj.delta_x.has_value() || !obj.delta_y.has_value()) {
            continue;
        }
        if (obj.end_time > this->current_ms) {
            double available_time = this->current_ms - obj.start_time;
            double total_duration = obj.end_time - obj.start_time;
            double ratio = (total_duration > 0) ? std::min(1.0, available_time / total_duration) : 1.0;

            state.judge_pos_x -= obj.delta_x.value() * (1.0 - ratio);
            state.judge_pos_y -= obj.delta_y.value() * (1.0 - ratio);

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
    jpos_scroll.seq = static_cast<int>(state.curr_timeline->size());

    state.curr_timeline->push_back(jpos_scroll);

    state.judge_pos_x += delta_x;
    state.judge_pos_y += delta_y;
}

void TJAParser::handle_N(const std::string& value, ParserState& state) {
    branch_n.emplace_back();
    state.curr_note_list = &branch_n.back().notes;
    state.curr_timeline = &branch_n.back().timeline;
    this->current_ms = state.start_branch_ms;
    state.bpm = state.start_branch_bpm;
    state.time_signature = state.start_branch_time_sig;
    state.scroll_x_modifier = state.start_branch_x_scroll;
    state.scroll_y_modifier = state.start_branch_y_scroll;
    state.barline_display = state.start_branch_barline;
    state.balloon_index = state.branch_balloon_index;
    state.balloon_cursor = state.branch_balloon_cursor;
    state.is_branching = true;
}

void TJAParser::handle_E(const std::string& value, ParserState& state) {
    branch_e.emplace_back();
    state.curr_note_list = &branch_e.back().notes;
    state.curr_timeline = &branch_e.back().timeline;
    this->current_ms = state.start_branch_ms;
    state.bpm = state.start_branch_bpm;
    state.time_signature = state.start_branch_time_sig;
    state.scroll_x_modifier = state.start_branch_x_scroll;
    state.scroll_y_modifier = state.start_branch_y_scroll;
    state.barline_display = state.start_branch_barline;
    state.balloon_index = state.branch_balloon_index;
    state.balloon_cursor = state.branch_balloon_cursor;
    state.is_branching = true;
}

void TJAParser::handle_M(const std::string& value, ParserState& state) {
    branch_m.emplace_back();
    state.curr_note_list = &branch_m.back().notes;
    state.curr_timeline = &branch_m.back().timeline;
    this->current_ms = state.start_branch_ms;
    state.bpm = state.start_branch_bpm;
    state.time_signature = state.start_branch_time_sig;
    state.scroll_x_modifier = state.start_branch_x_scroll;
    state.scroll_y_modifier = state.start_branch_y_scroll;
    state.barline_display = state.start_branch_barline;
    state.balloon_index = state.branch_balloon_index;
    state.balloon_cursor = state.branch_balloon_cursor;
    state.is_branching = true;
}

void TJAParser::handle_SECTION(const std::string& value, ParserState& state) {
    state.is_section_start = true;
    TimelineObject section;
    section.start_time = this->current_ms;
    section.section_reset = true;
    state.curr_timeline->push_back(section);
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
        double appear_duration, moving_duration;
        try {
            appear_duration = std::stof(parts[0]);
            moving_duration = std::stof(parts[1]);
        } catch (const std::exception&) {
            spdlog::warn("Invalid #SUDDEN values in {}", file_path.string());
            return;
        }
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
    bar_line.type = NoteType::BARLINE;
    bar_line.display = state.barline_display;
    bar_line.bpm = state.bpm;
    bar_line.index = state.index;
    bar_line.scroll_x = state.scroll_x_modifier;
    bar_line.scroll_y = state.scroll_y_modifier;

    if (state.barline_added) {
        bar_line.display = false;
    }

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

Note TJAParser::add_note(char item, ParserState& state) {
    Note note = Note();
    note.hit_ms = this->current_ms;
    note.display = !state.doron;
    note.type = NoteType(item - '0');
    note.index = state.index;
    note.bpm = state.bpm;
    note.scroll_x = state.scroll_x_modifier;
    note.scroll_y = state.scroll_y_modifier;

    if (state.sudden_appear > 0 || state.sudden_moving > 0) {
        note.sudden_appear_ms = state.sudden_appear;
        note.sudden_moving_ms = state.sudden_moving;
    }

    if (note.type == NoteType::ROLL_HEAD || note.type == NoteType::ROLL_HEAD_L) {
        note.color = 255;
        return note;
    }

    if (note.type == NoteType::BALLOON_HEAD || note.type == NoteType::KUSUDAMA) {
        state.balloon_index++;
        note.count = (state.balloon_cursor < state.balloons.size()) ? state.balloons[state.balloon_cursor] : 1;
        state.balloon_cursor++;
        return note;
    }

    return note;
}

const std::map<int, std::string> TJAParser::DIFFS = {{0, "easy"}, {1, "normal"}, {2, "hard"}, {3, "oni"},
                                                     {4, "edit"}, {5, "tower"},  {6, "dan"}};

void modifier_speed(NoteList& notes, float value) {
    for (auto& note : notes.notes) {
        note.scroll_x *= (value / 10.0f);
    }
}

void modifier_display(NoteList& notes) {
    for (auto& note : notes.notes) {
        note.display = false;
    }
}

void modifier_inverse(NoteList& notes) {
    std::map<NoteType, NoteType> type_mapping = {{NoteType::DON, NoteType::KAT},
                                                 {NoteType::KAT, NoteType::DON},
                                                 {NoteType::DON_L, NoteType::KAT_L},
                                                 {NoteType::KAT_L, NoteType::DON_L}};

    for (auto& note : notes.notes) {
        auto it = type_mapping.find(note.type);
        if (it != type_mapping.end()) {
            note.type = it->second;
        }
    }
}

void modifier_random(NoteList& notes, int value) {
    // value: 1 == kimagure, 2 == detarame

    if (value == 0 || notes.notes.empty()) {
        return;
    }

    int percentage = (notes.notes.size() / 5) * value;
    percentage = std::min(percentage, static_cast<int>(notes.notes.size()));

    std::random_device rd;
    std::mt19937 gen(rd());

    std::vector<int> indices(notes.notes.size());
    std::ranges::iota(indices, 0);
    std::shuffle(indices.begin(), indices.end(), gen);

    std::vector<int> selected_notes(indices.begin(), indices.begin() + percentage);

    std::map<NoteType, NoteType> type_mapping = {{NoteType::DON, NoteType::KAT},
                                                 {NoteType::KAT, NoteType::DON},
                                                 {NoteType::DON_L, NoteType::KAT_L},
                                                 {NoteType::KAT_L, NoteType::DON_L}};

    for (int i : selected_notes) {
        auto it = type_mapping.find(notes.notes[i].type);
        if (it != type_mapping.end()) {
            notes.notes[i].type = it->second;
        }
    }
}

void modifier_moji(NoteList& notes) {
    static const std::map<NoteType, int> se_notes = {
        {NoteType::DON, 0},          {NoteType::KAT, 3},       {NoteType::DON_L, 5},
        {NoteType::KAT_L, 6},        {NoteType::ROLL_HEAD, 7}, {NoteType::ROLL_HEAD_L, 8},
        {NoteType::BALLOON_HEAD, 9}, {NoteType::TAIL, 10},     {NoteType::KUSUDAMA, 11}};

    for (Note& note : notes.notes) {
        auto it = se_notes.find(note.type);
        if (it != se_notes.end()) {
            note.moji = it->second;
        }
    }

    std::vector<Interval> intervals = {Interval::EIGHTH, Interval::TWELFTH, Interval::SIXTEENTH, Interval::TWENTYFOURTH,
                                       Interval::THIRTYSECOND};

    for (const auto& interval : intervals) {
        auto streams = find_streams(notes.notes, interval);
        for (const auto& [start, length] : streams) {
            for (int i = start; i < start + length - 1; ++i) {
                if (notes.notes[i].type == NoteType::DON) {
                    notes.notes[i].moji = 1;
                } else if (notes.notes[i].type == NoteType::KAT) {
                    notes.notes[i].moji = 4;
                }
            }
            if (length == 3) {
                if (notes.notes[start + 0].type == NoteType::DON && notes.notes[start + 1].type == NoteType::DON &&
                    notes.notes[start + 2].type == NoteType::DON) {
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
    double tolerance = 15.0; // ms tolerance for timing classification

    double eighth_note = ms_per_measure / 8.0;
    double sixteenth_note = ms_per_measure / 16.0;
    double twelfth_note = ms_per_measure / 12.0;
    double twentyfourth_note = ms_per_measure / 24.0;
    double thirtysecond_note = ms_per_measure / 32.0;
    double quarter_note = ms_per_measure / 4.0;

    if (std::abs(interval_ms - eighth_note) < tolerance) {
        return Interval::EIGHTH;
    }

    if (std::abs(interval_ms - sixteenth_note) < tolerance) {
        return Interval::SIXTEENTH;
    }

    if (std::abs(interval_ms - twelfth_note) < tolerance) {
        return Interval::TWELFTH;
    }

    if (std::abs(interval_ms - twentyfourth_note) < tolerance) {
        return Interval::TWENTYFOURTH;
    }

    if (std::abs(interval_ms - thirtysecond_note) < tolerance) {
        return Interval::THIRTYSECOND;
    }

    if (std::abs(interval_ms - quarter_note) < tolerance) {
        return Interval::QUARTER;
    }
    return Interval::UNKNOWN;
}

std::vector<std::pair<int, int>> find_streams(const std::deque<Note>& modded_notes, Interval interval_type) {
    std::vector<std::pair<int, int>> streams;
    size_t i = 0;

    if (modded_notes.size() < 2) {
        return streams;
    }

    while (i < modded_notes.size() - 1) {
        if (modded_notes[i].type == NoteType::ROLL_HEAD || modded_notes[i].type == NoteType::ROLL_HEAD_L ||
            modded_notes[i].type == NoteType::BALLOON_HEAD || modded_notes[i].type == NoteType::KUSUDAMA) {
            i++;
            continue;
        }

        int stream_start = i;
        int stream_length = 1;

        while (i < modded_notes.size() - 1) {
            if (modded_notes[i + 1].type == NoteType::ROLL_HEAD || modded_notes[i + 1].type == NoteType::ROLL_HEAD_L ||
                modded_notes[i + 1].type == NoteType::BALLOON_HEAD || modded_notes[i + 1].type == NoteType::KUSUDAMA) {
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
            streams.emplace_back(stream_start, stream_length);
        }

        i++;
    }

    return streams;
}

std::string TJAParser::get_song_hash() {
    std::vector<unsigned char> buffer;
    auto absorb = [&](const void* data, size_t size) {
        const unsigned char* bytes = static_cast<const unsigned char*>(data);
        buffer.insert(buffer.end(), bytes, bytes + size);
    };

    for (const auto& [course, course_data] : metadata.course_data) {
        {
            auto [notes, branch_m, branch_e, branch_n] = notes_to_position(course);

            auto absorb_notes = [&](const NoteList& note_list) {
                for (const Note& note : note_list.notes) {
                    absorb(&note.bpm, sizeof(note.bpm));
                    absorb(&note.hit_ms, sizeof(note.hit_ms));
                    absorb(&note.scroll_x, sizeof(note.scroll_x));
                    absorb(&note.scroll_y, sizeof(note.scroll_y));
                    int t = static_cast<int>(note.type);
                    absorb(&t, sizeof(t));
                }
            };

            absorb_notes(notes);
            for (const NoteList& nl : branch_m) {
                absorb_notes(nl);
            }
            for (const NoteList& nl : branch_e) {
                absorb_notes(nl);
            }
            for (const NoteList& nl : branch_n) {
                absorb_notes(nl);
            }
        }
    }
    return md5_hexdigest(buffer);
}

std::string TJAParser::get_diff_hash(int difficulty) {
    auto [notes, branch_m, branch_e, branch_n] = notes_to_position(difficulty);
    auto total_notes = notes.notes.size();
    for (const NoteList& nl : branch_m) {
        total_notes += nl.notes.size();
    }
    for (const NoteList& nl : branch_e) {
        total_notes += nl.notes.size();
    }
    for (const NoteList& nl : branch_n) {
        total_notes += nl.notes.size();
    }

    if (total_notes == 0) {
        return "";
    }

    std::vector<unsigned char> buffer;
    auto absorb = [&](const void* data, size_t size) {
        const unsigned char* bytes = static_cast<const unsigned char*>(data);
        buffer.insert(buffer.end(), bytes, bytes + size);
    };

    auto absorb_notes = [&](const NoteList& note_list) {
        for (const Note& note : note_list.notes) {
            absorb(&note.bpm, sizeof(note.bpm));
            absorb(&note.hit_ms, sizeof(note.hit_ms));
            absorb(&note.scroll_x, sizeof(note.scroll_x));
            absorb(&note.scroll_y, sizeof(note.scroll_y));
            int t = static_cast<int>(note.type);
            absorb(&t, sizeof(t));
        }
    };

    absorb_notes(notes);
    for (const NoteList& nl : branch_m) {
        absorb_notes(nl);
    }
    for (const NoteList& nl : branch_e) {
        absorb_notes(nl);
    }
    for (const NoteList& nl : branch_n) {
        absorb_notes(nl);
    }
    return md5_hexdigest(buffer);
}
