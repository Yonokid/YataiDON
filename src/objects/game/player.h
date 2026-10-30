#pragma once

#include <unordered_map>

#include "../../libs/gfx/text.h"
#include "../../libs/network/network.h"
#include "../../libs/parsers/song_parser.h"
#include "../../libs/screen.h"
#include "../global/chara_3d.h"
#include "../global/nameplate.h"
#include "background.h"
#include "balloon_counter.h"
#include "branch_indicator.h"
#include "clear_animation.h"
#include "combo.h"
#include "combo_announce.h"
#include "drum_hit_effect.h"
#include "drumroll_counter.h"
#include "fail_animation.h"
#include "fc_animation.h"
#include "fireworks.h"
#include "gauge.h"
#include "gauge_hit_effect.h"
#include "gogo_time.h"
#include "judge_counter.h"
#include "judgment.h"
#include "kusudama_counter.h"
#include "lane_hit_effect.h"
#include "note_arc.h"
#include "score_counter.h"

namespace JudgePos {
    inline float X = 0.0f;
    inline float Y = 0.0f;
} // namespace JudgePos

namespace Timing {
    constexpr float GOOD = 25.0250015258789f;
    constexpr float OK = 75.0750045776367f;
    constexpr float BAD = 108.441665649414f;
    constexpr float GOOD_EASY = 41.7083358764648f;
    constexpr float OK_EASY = 108.441665649414f;
    constexpr float BAD_EASY = 125.125f;
} // namespace Timing

class Player {
public:
    double end_time;
    float bpm;
    PlayerNum player_num;
    double last_note_hit;
    std::map<double, InputLogType> input_log;

    std::vector<BranchDifficulty> branch_history;

    Player(std::optional<SongParser>& parser_ref, PlayerNum player_num_param, int difficulty_param, bool is_2p_param,
           const Modifiers& modifiers_param,
           const std::string& score_method_param = global_data.config->general.score_method);

    virtual ~Player() = default;

    std::optional<JudgeCounter> judge_counter;
    std::optional<Gauge> gauge;
    Gauge* dan_gauge = nullptr;
    Background* bg_hook = nullptr;

    Player* kusudama_partner = nullptr;
    int kusudama_shared_hits = 0;
    Player* kusudama_owner() { return (kusudama_partner && is_2p) ? kusudama_partner : this; }
    // hit notes fly to this player's soul gauge: the bottom one only when two play (2P alone plays
    // on the top lane)
    PlayerNum arc_player() const { return (is_2p && kusudama_partner) ? PlayerNum::P2 : PlayerNum::P1; }

    std::optional<Note> get_first_note();

    ResultData get_result_score();

    int get_good() const { return good_count; }
    int get_ok() const { return ok_count; }
    int get_bad() const { return bad_count; }
    std::optional<Judgments> get_note_judgment(int note_index) const {
        auto it = note_judgments.find(note_index);
        return it != note_judgments.end() ? std::optional<Judgments>(it->second) : std::nullopt;
    }
    bool is_auto_play() const { return modifiers.auto_play; }
    bool is_replay() const { return replay_active; }
    void load_replay(const std::map<double, InputLogType>& log) {
        replay_log.assign(log.begin(), log.end());
        replay_cursor = 0;
        replay_active = true;
    }
    void apply_replay_appearance(const PlayerData& pd);
    bool is_skip_enabled() const { return modifiers.skip; }
    void cut_to_end(double now, int prev_good = 0, int prev_ok = 0, int prev_bad = 0);
    bool was_skipped() const { return skipped_run; }
    const Modifiers& get_modifiers() const { return modifiers; }
    void set_auto_play(bool value) { modifiers.auto_play = value; }
    void reset_performance() {
        good_count = ok_count = bad_count = 0;
        combo = max_combo = 0;
        score = 0;
        total_drumroll = 0;
        hit_offset_sum_ms = 0.0;
        hit_offset_count = 0;
        was_gauge_full = false;
        was_gauge_clear = false;
        miss_streak = 0;
        note_judgments.clear();
        last_reported_score = -1;
        last_reported_gogo = -1;
        arc_points = 25;
        if (judge_counter) {
            judge_counter = JudgeCounter();
        }
    }
    int get_score() const { return score; }
    int get_max_combo() const { return max_combo; }
    int get_combo() const { return combo; }
    int get_total_drumroll() const { return total_drumroll; }
    int get_scissor_x() const {
        return lane_cover_tex_id ? virtual_to_screen_x(lane_cover_tex_id->scissor_right()) : 0;
    }
    void set_is_dan(bool v) { is_dan = v; }

    void reload_for_dan(std::optional<SongParser>& new_parser, int new_difficulty);

    void spawn_ending_anim(Background* background = nullptr);

    void seek_to(double resume_time);
    void set_audio_end(double chart_ms);

    void update(double ms_from_start, double current_ms, std::optional<Background>& background);

    void draw(double ms_from_start, float x, float y);

    void draw_practice(double ms_from_start, float x, float y, bool draw_notes_on);

    void draw_lyric(float y); // practice scene draws it after the large drums

    void draw_overlays(float y);
    void draw_lane_cover(float y);

protected:
    std::optional<LaneHitEffect> lane_hit_effect;
    std::vector<std::unique_ptr<DrumHitEffect>> draw_drum_hit_list;

private:
    bool is_2p;
    int judgeable_note_count = 0;
    bool skipped_run = false;
    bool replay_active = false;
    std::vector<std::pair<double, InputLogType>> replay_log;
    size_t replay_cursor = 0;
    bool is_dan{false};
    int difficulty;
    int visual_offset;
    std::string score_method;
    Modifiers modifiers;
    std::optional<SongParser> parser;
    Nameplate nameplate;
    std::unique_ptr<Chara3D> chara;

    // Score management
    int good_count{0};
    int ok_count{0};
    int bad_count{0};
    int combo{0};
    int score{0};
    int last_reported_score = -1; // last value sent to Background::handle_score
    int last_reported_gogo = -1;  // background handle_gogo edge detection
    int max_combo{0};
    int total_drumroll{0};
    double hit_offset_sum_ms = 0.0;
    int hit_offset_count = 0;
    std::unordered_map<int, Judgments> note_judgments;

    int arc_points{25};
    float judge_x{0};
    float judge_y{0};
    std::pair<double, int> last_jpos_key{-1e300, -1}; // (start_time, seq) of the newest #JPOSSCROLL applied

    bool is_gogo_time{false};
    bool was_gauge_full = false;
    bool was_gauge_clear = false;
    bool tinted_miss_streak = false;
    bool tinted_rainbow = false;
    int miss_streak = 0;
    double last_balloon_hit_ms = 0;
    bool balloon_idle = false;
    AnimIndex rest_anim() const;
    void on_miss();
    Side autoplay_hit_side{Side::LEFT};
    int last_subdivision{-1};

    std::deque<Note> don_notes;
    std::deque<Note> kat_notes;
    std::deque<Note> other_notes;
    std::deque<Note> barlines;

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

    bool is_drumroll;
    int curr_drumroll_count;
    double last_drumroll_color_time;
    bool is_balloon;
    int curr_balloon_count;
    int balloon_index;

    std::optional<OutlinedText> current_lyric;
    Background* ending_background = nullptr;
    bool practice_lyric = false;

    bool is_branch;
    std::tuple<float, float, double> curr_branch_reqs;
    float branch_p_count;
    int branch_r_count;
    int branch_note_count;

    size_t branch_checkpoint_index = 0;
    std::string branch_condition;

    double resume_filter_ms = -1e18;

    std::string don_hitsound;
    std::string kat_hitsound;

    TextureObject* lane_cover_tex_id = nullptr;
    TextureObject* lane_icon_tex_id = nullptr;
    TextureObject* note_tex_ids[10] = {};

    void init_player_textures();

    TextureObject* t_lane_background = nullptr;
    TextureObject* t_ai_lane_background = nullptr;
    TextureObject* t_lane_hit_circle = nullptr;
    TextureObject* t_dan_lane_cover = nullptr;
    TextureObject* t_drum = nullptr;
    TextureObject* t_lane_difficulty = nullptr;
    TextureObject* t_timer = nullptr;
    TextureObject* t_auto_icon = nullptr;
    TextureObject* t_lane_score_cover = nullptr;
    TextureObject* t_mod_shinuchi = nullptr;
    std::vector<TextureObject*> t_badges;

    TextureObject* t_notes_0 = nullptr;
    TextureObject* t_notes_8 = nullptr;
    TextureObject* t_notes_9 = nullptr;
    TextureObject* t_notes_10 = nullptr;
    TextureObject* t_moji = nullptr;
    TextureObject* t_moji_drumroll_mid = nullptr;
    TextureObject* t_drumroll_big_tail = nullptr;
    TextureObject* t_drumroll_tail = nullptr;

    std::vector<Judgment> draw_judge_list;
    std::vector<GaugeHitEffect> gauge_hit_effect;
    std::vector<NoteArc> draw_arc_list;
    Combo combo_display;
    std::optional<DrumrollCounter> drumroll_counter;
    std::optional<BalloonCounter> balloon_counter;
    std::optional<KusudamaCounter> kusudama_counter;
    ScoreCounter score_counter;
    std::optional<GogoTime> gogo_time;
    std::optional<Fireworks> fireworks;
    ScrollType scroll_type = ScrollType::NMSCROLL;
    TempoMap tempo_map;
    std::optional<double> audio_end_ms;
    std::optional<ComboAnnounce> combo_announce;
    std::optional<BranchIndicator> branch_indicator;
    std::optional<std::variant<FailAnimation, ClearAnimation, FCAnimation>> ending_anim;

    void get_load_time(Note& note);

    void reset_chart();
    bool unplayable(const Note& note) const;

    void handle_timeline(double ms_from_start);

    void autoplay_manager(double ms_from_start, double current_ms, std::optional<Background>& background);

    void evaluate_branch(double current_ms);

    void merge_branch_section(const NoteList& branch_section, double current_ms);

    float get_position_x(const Note& note, double current_ms);

    float get_position_y(const Note& note, double current_ms);

    void handle_gogotime(double ms_from_start, const TimelineObject& timeline_object, int buffer_index);
    void handle_judgeposition(double ms_from_start, const TimelineObject& timeline_object, int buffer_index);
    void handle_bpmchange(double ms_from_start, const TimelineObject& timeline_object, int buffer_index);
    void handle_branch_param(double ms_from_start, const TimelineObject& timeline_object, int buffer_index);
    void handle_lyric(double ms_from_start, const TimelineObject& timeline_object, int buffer_index);
    void handle_camera(double ms_from_start, const TimelineObject& timeline_object, int buffer_index);
    void update_camera(double ms_from_start);
    std::vector<TimelineObject> camera_eases; // running #CAM*START..END events (P1 only)
    void handle_section(double ms_from_start, const TimelineObject& timeline_object, int buffer_index);

    void play_note_manager(double current_ms, std::optional<Background>& background);

    bool is_balloon_type(int type);

    void draw_note_manager(double current_ms);

    void note_manager(double current_ms, std::optional<Background>& background);

    void note_correct(const Note& note, double current_ms);

    void check_drumroll(double current_ms, DrumType drum_type, std::optional<Background>& background);

    void check_balloon(double current_ms, DrumType drum_type, const Note& balloon,
                       std::optional<Background>& background);

    void check_kusudama(double current_ms, DrumType drum_type, const Note& balloon,
                        std::optional<Background>& background);

    void check_note(double ms_from_start, DrumType drum_type, double current_ms, std::optional<Background>& background);

    void drumroll_counter_manager(double current_ms);

    void balloon_counter_manager(double current_ms);

    void kusudama_counter_manager(double current_ms);

    virtual void spawn_hit_effects(DrumType drum_type, Side side);

protected:
    virtual void handle_input(double ms_from_start, double current_ms, std::optional<Background>& background);

private:
    void draw_bar(double current_ms, float y, const Note& bar);

    void draw_drumroll(double current_ms, float y, const Note& head, int current_eighth, bool moji_pass);

    void draw_balloon(double current_ms, float y, const Note& head, int current_eighth, bool moji_pass);

    void draw_notes(double current_ms, float y);

    void draw_song_timer(double current_ms, float y);

    void draw_modifiers(float y);
};
