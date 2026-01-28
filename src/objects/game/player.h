#pragma once

#include "../../libs/parsers/tja.h"
#include "../../libs/global_data.h"
#include "../../libs/texture.h"
#include "../../libs/audio_engine.h"
#include "../../libs/utils.h"
#include "judgment.h"
#include "lane_hit_effect.h"
#include "drum_hit_effect.h"
#include "gauge_hit_effect.h"
#include "note_arc.h"
#include "combo.h"

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
    Player() = default;

    Player(std::optional<TJAParser>& parser_ref, PlayerNum player_num_param, int difficulty_param,
           bool is_2p_param, const Modifiers& modifiers_param);

    void update(double ms_from_start, double current_ms);

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

    NoteList notes;
    std::vector<NoteList> branch_m;
    std::vector<NoteList> branch_e;
    std::vector<NoteList> branch_n;

    int base_score;
    int score_init;
    int score_diff;

    std::vector<TimelineObject> timeline;
    int timeline_index;
    std::vector<Note> current_bars;
    std::vector<Note> current_notes_draw;

    std::vector<Note> draw_note_list;
    std::vector<Note> draw_bar_list;

    std::deque<Note> don_notes;
    std::deque<Note> kat_notes;
    std::deque<Note> other_notes;

    bool is_drumroll;
    int curr_drumroll_count;
    bool is_balloon;
    int curr_balloon_count;
    int balloon_index;

    bool is_branch;
    std::vector<std::string> curr_branch_reqs;
    int branch_condition_count;
    std::string branch_condition;

    float end_time;

    std::string don_hitsound;
    std::string kat_hitsound;

    std::vector<Judgment> draw_judge_list;
    std::optional<LaneHitEffect> lane_hit_effect;
    std::vector<DrumHitEffect> draw_drum_hit_list;
    std::vector<GaugeHitEffect> gauge_hit_effect;
    std::vector<NoteArc> draw_arc_list;
    Combo combo_display;

    void get_load_time(Note& note);

    void reset_chart();

    void merge_branch_section(NoteList branch_section, double current_ms);

    std::tuple<int> get_result_score();

    float get_position_x(Note note, double current_ms);

    float get_position_y(Note note, double current_ms);

    void play_note_manager(double current_ms);

    bool is_balloon_type(int type);

    void draw_note_manager(double current_ms);

    void note_manager(double current_ms);

    void note_correct(Note note, double current_time);

    void check_note(double ms_from_start, DrumType drum_type, double current_time);

    void spawn_hit_effects(DrumType drum_type, Side side);

    void handle_input(double ms_from_start, double current_ms);

    void draw_notes(double current_ms);

    void draw_overlays(ray::Shader mask_shader);
};
