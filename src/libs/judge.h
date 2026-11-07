#pragma once

#include <cmath>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../objects/enums.h"
#include "parsers/tja.h"

namespace Timing {
    constexpr float GOOD = 25.0250015258789f;
    constexpr float OK = 75.0750045776367f;
    constexpr float BAD = 108.441665649414f;
    constexpr float GOOD_EASY = 41.7083358764648f;
    constexpr float OK_EASY = 108.441665649414f;
    constexpr float BAD_EASY = 125.125f;
} // namespace Timing

enum class InputLogType {
    KAT_L = 0,
    DON_L = 1,
    DON_R = 2,
    KAT_R = 3
};

using InputLog = std::vector<std::pair<double, InputLogType>>;

inline DrumType drum_of(InputLogType type) {
    return (type == InputLogType::DON_L || type == InputLogType::DON_R) ? DrumType::DON : DrumType::KAT;
}

// This is for 720p, and should be scaled when needed
namespace Lane {
    constexpr float TRAVEL_DISTANCE = 948.0f;
    constexpr int NOTE_HALF_W = 64;
    constexpr float JUDGE_X = 414.0f;
} // namespace Lane

struct JudgeParams {
    int start_delay = 0;
    int parse_player = 0; // TJAParser player_num
    std::optional<double> audio_end_ms;
    bool dan = false; // combo and gauge carry across dan songs, so max_combo and crown aren't per song
};

void compute_load_time(Note& note, ScrollType scroll_type, const TempoMap& tempo_map);

bool note_unplayable(const Note& note, ScrollType scroll_type, std::optional<double> audio_end_ms);

void prepare_chart(NoteList& notes, std::deque<NoteList>& branch_m, std::deque<NoteList>& branch_e,
                   std::deque<NoteList>& branch_n, const Modifiers& modifiers, std::optional<double> audio_end_ms);

bool is_judge_timeline_entry(const TimelineObject& entry);

class GaugeCore {
public:
    GaugeCore(int total_notes, int difficulty, int level);
    static GaugeCore dan(const std::vector<std::pair<int, int>>& songs, int total_notes);

    void add_good() { apply_points_clamped(good_points); }
    void add_ok() { apply_points_clamped(ok_points); }
    void add_bad() { apply_points_clamped(bad_points); }
    void set_fraction(double fraction) { apply_points_clamped(fraction * max_points - points); }

    bool get_is_clear() const { return points >= clear_points; }
    bool get_is_rainbow() const { return points >= max_points; }
    bool is_dan() const { return dan_mode; }
    float get_length() const { return points / max_points * 100; }

    int get_percent() const { return static_cast<int>(std::floor((points + POINTS_EPS) / max_points * 100.0)); }

protected:
    void apply_points_clamped(double delta);
    bool dan_mode = false;
    double good_points;
    double ok_points;
    double bad_points;
    double points = 0;
    double previous_points = 0;
    int max_points = 10000;
    int clear_points = 8000;
    int difficulty;
    static constexpr double POINTS_EPS = 1e-6;

    struct GaugeTable {
        double soul_percent;
        double ok_multiplier;
        double bad_multiplier;
    };
    static const GaugeTable& table_row(int difficulty, int level);
};

struct JudgeResult {
    int score = 0;
    int good = 0;
    int ok = 0;
    int bad = 0;
    int max_combo = 0;
    int drumroll = 0;
    bool cleared = false;
    bool rainbow = false;
};

class JudgeCore {
public:
    JudgeCore(int difficulty, const Modifiers& modifiers, bool gen3, bool is_2p = false);
    virtual ~JudgeCore() = default;

    std::vector<BranchDifficulty> branch_history;

    void load_chart(const NoteList& notes, std::deque<NoteList> branch_m, std::deque<NoteList> branch_e,
                    std::deque<NoteList> branch_n, std::map<int, CourseData>& course_data,
                    std::optional<double> audio_end_ms);

    void begin_frame(double ms_from_start);
    void check_note(double ms_from_start, DrumType drum_type, double current_ms);
    void end_frame(double ms_from_start);

    JudgeResult result() const;
    int judge_gauge_notes() const { return judgeable_note_count; }
    int judge_gauge_stars() const { return gauge_stars; }
    double judge_end_ms() const { return chart_end_ms; }

protected:
    // What the frame's judging did, for Player to draw. All no-ops here.
    // hit_drum is empty for a note that passed unhit.
    virtual void on_judgment(const Note& note, Judgments judgment, std::optional<DrumType> hit_drum) {}
    virtual void on_note_correct(const Note& note, double current_ms) {}
    virtual void on_score_popup(int amount, bool tick) {}
    virtual void on_drumroll_hit(DrumType drum_type, double current_ms) {}
    // after curr_balloon_count is counted; popped when this hit burst it
    virtual void on_balloon_hit(const Note& balloon, double current_ms, bool popped) {}
    virtual void on_balloon_start() {}
    virtual void on_balloon_missed() {}
    virtual void on_kusudama_hit(JudgeCore* owner, const Note& balloon, double current_ms, bool popped) {}
    virtual void on_branch_merged(BranchDifficulty chosen, const std::deque<Note>& notes, const NoteList& section) {}
    // 2P kusudama: the other lane, which shares the hit count
    virtual JudgeCore* judge_partner() { return nullptr; }

    void play_note_manager(double current_ms);
    void note_correct(const Note& note, double current_ms);
    void check_drumroll(double current_ms, DrumType drum_type);
    void check_balloon(double current_ms, DrumType drum_type, const Note& balloon);
    void check_kusudama(double current_ms, DrumType drum_type, const Note& balloon);
    void evaluate_branch(double current_ms);
    void merge_branch_section(const NoteList& branch_section, double current_ms);
    void handle_judge_timeline(double ms_from_start);
    void handle_judge_entry(double ms_from_start, const TimelineObject& entry);
    // practice resume: replays the timeline up to resume_time and drops notes before it
    void seek_judge(double resume_time);

    JudgeCore* kusudama_owner() {
        JudgeCore* partner = judge_partner();
        return (partner && is_2p) ? partner : this;
    }

    bool is_2p = false;
    int difficulty;
    Modifiers modifiers;
    bool gen3;
    int judgeable_note_count = 0;
    int gauge_stars = 10;
    double chart_end_ms = 0; // last judgeable note, branches included
    std::optional<GaugeCore> judge_gauge;

    int good_count{0};
    int ok_count{0};
    int bad_count{0};
    int combo{0};
    int score{0};
    int max_combo{0};
    int total_drumroll{0};
    double hit_offset_sum_ms = 0.0;
    int hit_offset_count = 0;
    std::unordered_map<int, Judgments> note_judgments;
    std::unordered_map<int, int> roll_hits;

    std::deque<Note> don_notes;
    std::deque<Note> kat_notes;
    std::deque<Note> other_notes;

    std::deque<NoteList> branch_m;
    std::deque<NoteList> branch_e;
    std::deque<NoteList> branch_n;
    std::deque<TimelineObject> judge_timeline;

    int base_score = 0;
    int score_init = 0;
    int score_diff = 0;

    bool is_drumroll = false;
    int curr_drumroll_count = 0;
    bool is_balloon = false;
    int curr_balloon_count = 0;
    int kusudama_shared_hits = 0;
    long long kusudama_key = -1;

    bool is_branch = false;
    std::tuple<float, float, double> curr_branch_reqs;
    float branch_p_count = 0;
    int branch_r_count = 0;
    int branch_note_count = 0;
    size_t branch_checkpoint_index = 0;
    std::string branch_condition;

    double resume_filter_ms = -1e18;
};
