#pragma once

#include "../../libs/parsers/tja.h"
#include "../../libs/global_data.h"
#include "../../libs/texture.h"
#include "../../libs/audio.h"
#include "../../libs/utils.h"
#include "balloon_counter.h"
#include "branch_indicator.h"
#include "combo_announce.h"
#include "gauge.h"
#include "gogo_time.h"
#include "judge_counter.h"
#include "judgment.h"
#include "lane_hit_effect.h"
#include "drum_hit_effect.h"
#include "gauge_hit_effect.h"
#include "note_arc.h"
#include "combo.h"
#include "drumroll_counter.h"
#include "kusudama_counter.h"
#include "score_counter.h"
#include "score_counter_animation.h"
#include "background.h"

namespace JudgePos {
    inline float X = 414 * tex.screen_scale;
    inline float Y = 256 * tex.screen_scale;
}

namespace Timing {
    constexpr float GOOD = 25.0250015258789f;
    constexpr float OK = 75.0750045776367f;
    constexpr float BAD = 108.441665649414f;
    constexpr float GOOD_EASY = 41.7083358764648f;
    constexpr float OK_EASY = 108.441665649414f;
    constexpr float BAD_EASY = 125.125f;
}

class Player {
public:
    Player(std::optional<TJAParser>& parser_ref, PlayerNum player_num_param, int difficulty_param,
           bool is_2p_param, const Modifiers& modifiers_param);

    void update(double ms_from_start, double current_ms, std::optional<Background>& background);

    void draw(double ms_from_start, ray::Shader& mask_shader);

private:
    bool is_2p;
    bool is_dan;
    PlayerNum player_num;
    int difficulty;
    int visual_offset;
    std::string score_method;
    Modifiers modifiers;
    std::optional<TJAParser> parser;

    float bpm;

    // Score management
    int good_count;
    int ok_count;
    int bad_count;
    int combo;
    int score;
    int max_combo;
    int total_drumroll;

    int arc_points;
    float judge_x;
    float judge_y;

    bool is_gogo_time;
    Side autoplay_hit_side;
    int last_subdivision;

    std::deque<Note> don_notes;
    std::deque<Note> kat_notes;
    std::deque<Note> other_notes;

    std::deque<Note> draw_note_list;
    std::vector<Note> draw_note_buffer;

    std::deque<NoteList> branch_m;
    std::deque<NoteList> branch_e;
    std::deque<NoteList> branch_n;

    std::deque<TimelineObject> timeline;
    std::vector<TimelineObject> timeline_buffer;

    int base_score;
    int score_init;
    int score_diff;

    double end_time;

    bool is_drumroll;
    int curr_drumroll_count;
    double last_drumroll_color_time;
    bool is_balloon;
    int curr_balloon_count;
    int balloon_index;

    bool is_branch;
    std::tuple<float, float, double, int> curr_branch_reqs;
    int branch_condition_count;
    std::string branch_condition;

    std::string don_hitsound;
    std::string kat_hitsound;

    std::vector<Judgment> draw_judge_list;
    std::optional<LaneHitEffect> lane_hit_effect;
    std::vector<DrumHitEffect> draw_drum_hit_list;
    std::vector<GaugeHitEffect> gauge_hit_effect;
    std::vector<NoteArc> draw_arc_list;
    Combo combo_display;
    std::optional<DrumrollCounter> drumroll_counter;
    std::optional<BalloonCounter> balloon_counter;
    std::optional<KusudamaCounter> kusudama_counter;
    ScoreCounter score_counter;
    std::vector<ScoreCounterAnimation> base_score_list;
    std::optional<GogoTime> gogo_time;
    std::optional<double> delay_start;
    std::optional<double> delay_end;
    std::optional<ComboAnnounce> combo_announce;
    std::optional<BranchIndicator> branch_indicator;
    std::optional<JudgeCounter> judge_counter;
    std::optional<Gauge> gauge;

    void get_load_time(Note& note);

    void reset_chart();

    void handle_timeline(double ms_from_start);

    void autoplay_manager(double ms_from_start, double current_ms, std::optional<Background>& background);

    void evaluate_branch(double current_ms);

    void merge_branch_section(const NoteList& branch_section, double current_ms);

    std::tuple<int> get_result_score();

    float get_position_x(const Note& note, double current_ms);

    float get_position_y(const Note& note, double current_ms);

    void handle_scroll_type_commands(double ms_from_start, const TimelineObject& timeline_object, int buffer_index);
    void handle_gogotime(double ms_from_start, const TimelineObject& timeline_object, int buffer_index);
    void handle_judgeposition(double ms_from_start, const TimelineObject& timeline_object, int buffer_index);
    void handle_bpmchange(double ms_from_start, const TimelineObject& timeline_object, int buffer_index);
    void handle_branch_param(double ms_from_start, const TimelineObject& timeline_object, int buffer_index);

    void play_note_manager(double current_ms, std::optional<Background>& background);

    bool is_balloon_type(int type);

    void draw_note_manager(double current_ms);

    void note_manager(double current_ms, std::optional<Background>& background);

    void note_correct(const Note& note, double current_ms);

    void check_drumroll(double current_ms, DrumType drum_type, std::optional<Background>& background);

    void check_balloon(double current_ms, DrumType drum_type, const Note& balloon, std::optional<Background>& background);

    void check_kusudama(double current_ms, const Note& balloon);

    void check_note(double ms_from_start, DrumType drum_type, double current_ms, std::optional<Background>& background);

    void drumroll_counter_manager(double current_ms);

    void balloon_counter_manager(double current_ms);

    void spawn_hit_effects(DrumType drum_type, Side side);

    void handle_input(double ms_from_start, double current_ms, std::optional<Background>& background);

    void draw_bar(double current_ms, const Note& bar);

    void draw_drumroll(double current_ms, const Note& head, int current_eighth);

    void draw_balloon(double current_ms, const Note& head, int current_eighth);

    void draw_notes(double current_ms);

    void draw_modifiers();

    void draw_overlays(const ray::Shader& mask_shader);
};
