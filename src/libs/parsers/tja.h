#pragma once

#include <cmath>
#include <functional>
#include <optional>
#include <string>
#include <string>
#include <sstream>
#include <iomanip>
#include <digestpp/algorithm/md5.hpp>
#include <vector>
#include <variant>
#include <map>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include "../global_data.h"
#include <random>

namespace fs = std::filesystem;

enum class NoteType : int {
    NONE = 0,
    DON = 1,
    KAT = 2,
    DON_L = 3,
    KAT_L = 4,
    ROLL_HEAD = 5,
    ROLL_HEAD_L = 6,
    BALLOON_HEAD = 7,
    TAIL = 8,
    KUSUDAMA = 9
};

enum class ScrollType : int {
    NMSCROLL = 0,
    BMSCROLL = 1,
    HBSCROLL = 2
};

struct TimelineObject {
    double hit_ms;
    double load_ms;

    double judge_pos_x;
    double judge_pos_y;
    double delta_x;
    double delta_y;

    std::optional<double> bpm;
    double bpmchange;
    double delay;
    bool gogo_time;
    std::string branch_params;
    bool is_branch_start = false;
    bool is_section_marker = false;
    std::string lyric = "";

    bool operator<(const TimelineObject& other) const {
        return load_ms < other.load_ms;
    }
};

class Note {
public:
    int type;
    double hit_ms;
    double load_ms;
    double unload_ms;
    double bpm;
    double scroll_x;
    double scroll_y;
    std::optional<double> sudden_appear_ms;
    std::optional<double> sudden_moving_ms;
    bool display;
    int index;
    int moji;
    std::optional<std::string> branch_params;
    bool is_branch_start;
    // Drumroll specific
    std::optional<int> color;
    // Balloon specific
    std::optional<int> count;
    std::optional<bool> popped;
    std::optional<bool> is_kusudama;

    Note() : type(0), hit_ms(0.0f), load_ms(0.0f), unload_ms(0.0f),
             bpm(0.0f), scroll_x(0.0f), scroll_y(0.0f),
             display(true), index(0), moji(0),
             branch_params(""), is_branch_start(false) {}

    virtual ~Note() = default;

    bool operator<(const Note& other) const {
        return hit_ms < other.hit_ms;
    }
    bool operator<=(const Note& other) const {
        return hit_ms <= other.hit_ms;
    }
    bool operator>(const Note& other) const {
        return hit_ms > other.hit_ms;
    }
    bool operator>=(const Note& other) const {
        return hit_ms >= other.hit_ms;
    }
    bool operator==(const Note& other) const {
        return hit_ms == other.hit_ms;
    }

    virtual std::string get_hash_data() const {
        std::vector<std::string> hash_fields = {"bpm", "hit_ms", "scroll_x", "scroll_y", "type"};
        std::ostringstream oss;
        oss << "[";
        for (const auto& field : hash_fields) {
            oss << "('" << field << "', ";
            if (field == "type") oss << type;
            else if (field == "hit_ms") oss << hit_ms;
            else if (field == "bpm") oss << bpm;
            else if (field == "scroll_x") oss << scroll_x;
            else if (field == "scroll_y") oss << scroll_y;
            oss << "), ";
        }
        oss << "('__class__', 'Note')]";
        return oss.str();
    }

    std::string get_hash() const {
        std::string data = get_hash_data();

        // Use digestpp to compute MD5
        digestpp::md5 hasher;
        hasher.absorb(data);
        std::string hash_hex = hasher.hexdigest();

        return hash_hex;
    }

    size_t hash() const {
        std::string md5_hash = get_hash();
        std::string first_8_chars = md5_hash.substr(0, 8);
        return std::stoull(first_8_chars, nullptr, 16);
    }
};

namespace std {
    template<>
    struct hash<Note> {
        size_t operator()(const Note& note) const {
            return note.hash();
        }
    };
}

struct CompareNotes {
    bool operator()(const Note& a, const Note& b) const { return a < b; }

    template<typename T, typename U>
    bool operator()(const T& a, const U& b) const {
        return a.hit_ms < b.hit_ms;
    }
};

struct NoteList {
    std::vector<Note> play_notes;
    std::vector<Note> draw_notes;
    std::vector<Note> bars;
    std::vector<TimelineObject> timeline;

    NoteList operator+(const NoteList& other) const {
        NoteList result;
        result.play_notes = play_notes;
        result.play_notes.insert(result.play_notes.end(),
                                 other.play_notes.begin(),
                                 other.play_notes.end());

        result.draw_notes = draw_notes;
        result.draw_notes.insert(result.draw_notes.end(),
                                 other.draw_notes.begin(),
                                 other.draw_notes.end());

        result.bars = bars;
        result.bars.insert(result.bars.end(),
                          other.bars.begin(),
                          other.bars.end());

        result.timeline = timeline;
        result.timeline.insert(result.timeline.end(),
                              other.timeline.begin(),
                              other.timeline.end());

        return result;
    }

    NoteList& operator+=(const NoteList& other) {
        play_notes.insert(play_notes.end(),
                         other.play_notes.begin(),
                         other.play_notes.end());
        draw_notes.insert(draw_notes.end(),
                         other.draw_notes.begin(),
                         other.draw_notes.end());
        bars.insert(bars.end(),
                   other.bars.begin(),
                   other.bars.end());
        timeline.insert(timeline.end(),
                       other.timeline.begin(),
                       other.timeline.end());
        return *this;
    }
};

struct CourseData {
    double level = 0;
    std::vector<int> balloon;
    std::vector<int> scoreinit;
    double scorediff = 0;
    bool is_branching = false;
};

struct TJAMetadata {
    std::map<std::string, std::string> title = {{"en", ""}};
    std::map<std::string, std::string> subtitle = {{"en", ""}};
    std::string genre = "";
    fs::path wave;
    double demostart = 0.0f;
    double offset = 0.0f;
    double bpm = 120.0f;
    fs::path bgmovie;
    double movieoffset = 0.0f;
    std::string scene_preset = "";
    std::map<int, CourseData> course_data;
};


struct TJAEXData {
    bool new_audio = false;
    bool old_audio = false;
    bool limited_time = false;
    bool new_file = false;
};

struct ParserState {
    double time_signature = 4.0f / 4.0f;
    double bpm = 120.0f;
    double bpmchange_last_bpm = 120.0f;
    double scroll_x_modifier = 1.0f;
    double scroll_y_modifier = 0.0f;
    ScrollType scroll_type = ScrollType::NMSCROLL;
    bool barline_display = true;
    std::vector<Note>* curr_note_list;
    std::vector<Note>* curr_draw_list;
    std::vector<Note>* curr_bar_list;
    std::vector<TimelineObject>* curr_timeline;
    double index = 0;
    std::vector<int> balloons;
    double balloon_index = 0;
    std::optional<Note> prev_note;
    bool barline_added = false;
    double sudden_appear = 0.0f;
    double sudden_moving = 0.0f;
    double judge_pos_x = 0.0f;
    double judge_pos_y = 0.0f;
    double delay_current = 0.0f;
    double delay_last_note_ms = 0.0f;
    bool is_branching = false;
    bool is_section_start = false;
    double start_branch_ms = 0.0f;
    double start_branch_bpm = 120.0f;
    double start_branch_time_sig = 4.0f / 4.0f;
    double start_branch_x_scroll = 1.0f;
    double start_branch_y_scroll = 0.0f;
    bool start_branch_barline = false;
    double branch_balloon_index = 0;
    std::optional<Note> section_bar;
};

enum class Interval {
    UNKNOWN = 0,
    QUARTER = 1,
    EIGHTH = 2,
    TWELFTH = 3,
    SIXTEENTH = 4,
    TWENTYFOURTH = 6,
    THIRTYSECOND = 8
};

class TJAParser {
public:
    static const std::map<int, std::string> DIFFS;

    TJAParser(const std::filesystem::path& path, int start_delay = 0);

    std::filesystem::path file_path;
    TJAMetadata metadata;
    TJAEXData ex_data;

    void get_metadata();

    using CommandHandler = std::function<void(const std::string&, ParserState&)>;

    std::tuple<NoteList, std::vector<NoteList>, std::vector<NoteList>, std::vector<NoteList>>
    notes_to_position(int diff);

private:
    volatile double current_ms;
    NoteList master_notes;
    std::vector<std::string> data;
    std::vector<NoteList> branch_m;
    std::vector<NoteList> branch_e;
    std::vector<NoteList> branch_n;

    std::vector<std::string> read_file_lines(const std::filesystem::path& path,
                                             const std::string& encoding);

    static std::string to_lower(const std::string& str);
    static std::string split_after_colon(const std::string& str);
    static std::string join_after_colon(const std::string& str);
    static void replace_all(std::string& str, const std::string& from, const std::string& to);
    static std::string trim(const std::string& str);
    static std::vector<int> parse_balloon_data(const std::string& data);

    std::vector<std::vector<std::string>> data_to_notes(int diff);

    Note* get_note_ptr(Note& variant);

    enum class EasingPoint {
        IN,
        OUT,
        IN_OUT
    };

    enum class EasingFunction {
        LINEAR,
        CUBIC,
        QUARTIC,
        QUINTIC,
        SINUSOIDAL,
        EXPONENTIAL,
        CIRCULAR
    };

    float apply_easing(float t, EasingPoint easing_point, EasingFunction easing_function);

    void set_branch_params(std::vector<Note>& bar_list, std::string branch_params,
                          std::optional<Note> section_bar);

    std::map<std::string, CommandHandler> build_command_registry();

    void handle_MEASURE(const std::string& value, ParserState& state);
    void handle_SCROLL(const std::string& value, ParserState& state);
    void handle_BPMCHANGE(const std::string& value, ParserState& state);
    void handle_GOGOSTART(const std::string& value, ParserState& state);
    void handle_GOGOEND(const std::string& value, ParserState& state);
    void handle_DELAY(const std::string& value, ParserState& state);
    void handle_BARLINEOFF(const std::string& value, ParserState& state);
    void handle_BARLINEON(const std::string& value, ParserState& state);
    void handle_BRANCHSTART(const std::string& value, ParserState& state);
    void handle_BRANCHEND(const std::string& value, ParserState& state);
    void handle_N(const std::string& value, ParserState& state);
    void handle_E(const std::string& value, ParserState& state);
    void handle_M(const std::string& value, ParserState& state);
    void handle_SECTION(const std::string& value, ParserState& state);
    void handle_NMSCROLL(const std::string& value, ParserState& state);
    void handle_BMSCROLL(const std::string& value, ParserState& state);
    void handle_HBSCROLL(const std::string& value, ParserState& state);
    void handle_SUDDEN(const std::string& value, ParserState& state);
    void handle_JPOSSCROLL(const std::string& part, ParserState& state);
    void handle_LYRIC(const std::string& value, ParserState& state);

    Note add_bar(ParserState& state);
    Note add_note(const std::string& item, ParserState& state);
};

double get_ms_per_measure(double bpm_val, double time_sig);
int calculate_base_score(const NoteList& notes);
std::string test_encodings(const std::filesystem::path& file_path);
std::string strip_comments(const std::string& code);

Interval get_note_interval_type(double interval_ms, double bpm, double time_sig = 4.0);
std::vector<std::pair<int, int>> find_streams(const std::vector<Note>& modded_notes, Interval interval_type);
std::vector<Note> modifier_moji(const NoteList& notes);
std::pair<std::vector<Note>, std::vector<Note>> modifier_speed(const NoteList& notes, float value);
std::vector<Note> modifier_display(const NoteList& notes);
std::vector<Note> modifier_inverse(const NoteList& notes);
std::vector<Note> modifier_random(const NoteList& notes, int value);
std::tuple<std::vector<Note>, std::vector<Note>, std::vector<Note>> apply_modifiers(const NoteList& notes, const Modifiers& modifiers);
