#pragma once

#include <array>
#include "game.h"
#include "../objects/game/dan_between.h"
#include "../objects/game/exam_caption.h"

struct DanExamInfo {
    float   progress     = 0;
    float   bar_width    = 0;
    int     counter_value = 0;
    int     red_value    = 0;
    std::string bar_texture;
    std::string bar_state = "empty";
    std::string song_state[3] = {"empty", "empty", "empty"};
    std::string exam_type;
    std::string exam_range;
    bool    gothrough    = true;
    int     song_count   = 0;               // songs finished BEFORE the current one
    int     song_value[3]    = {0, 0, 0};   // that song's own count
    float   song_progress[3] = {0, 0, 0};
};

struct DanInfoCache {
    int remaining_notes = 0;
    std::vector<DanExamInfo> exam_data;
};

class DanGameScreen : public GameScreen {
public:
    DanGameScreen() : GameScreen("game") {}

    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;

private:
    int song_index = 0;
    int total_notes = 0;
    int dan_color = 0;

    Gauge dan_gauge{GaugeMode::DAN, PlayerNum::P1, 1};  // initialized properly in init_dan()

    std::vector<bool> exam_failed;
    bool   failed_out    = false;
    double failed_out_at = 0.0;
    std::vector<std::array<bool, 3>> exam_song_failed;
    std::optional<DanInfoCache> dan_info_cache;

    DanBetween between;

    std::unique_ptr<OutlinedText> hori_name;

    ExamCaptionCache exam_captions;

    // Cumulative stat tracking across songs
    int prev_good = 0, prev_ok = 0, prev_bad = 0, prev_drumroll = 0;
    int prev_score = 0;
    struct SongStats { int good = 0, ok = 0, bad = 0, drumroll = 0, score = 0, max_combo = 0; };
    std::vector<SongStats> song_stats;
    int song_max_combo = 0;
    std::string current_song_title;

    void init_dan();
    void change_song();

    void update_skip_dan();
    void poll_skip_dan();
    void do_skip_dan();

    DanInfoCache calculate_dan_info();
    int get_exam_progress(const Exam& exam);
    int get_exam_progress_song(const Exam& exam, int song_idx);
    void draw_exam_row(const DanExamInfo& info, const Exam& exam, int index, float y);
    void fill_unplayed_songs();
    void check_exam_failures(bool course_finished = false, bool song_finished = false);

    static int exam_tier(const Exam& exam, int value);
    void save_result_data(bool all_failed);
    void trigger_fail_out(double current_ms);

    static const SkinInfo& dan_exam_info();
    void push_dan_state();
    void draw_dan_info();
    void draw_digit_counter(const std::string& digits, float margin_x, TexID tex_id, int index, float y, float x_offset = 0);
};
