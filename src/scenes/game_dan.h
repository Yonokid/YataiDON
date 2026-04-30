#pragma once
#include "game.h"
#include "../objects/game/dan_gauge.h"
#include "../objects/song_select/dan_transition.h"

struct DanExamInfo {
    float   progress     = 0;
    float   bar_width    = 0;
    int     counter_value = 0;
    int     red_value    = 0;
    std::string bar_texture;
    std::string exam_type;
    std::string exam_range;
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

    DanGauge dan_gauge{PlayerNum::P1, 1};  // initialized properly in init_dan()

    std::vector<bool> exam_failed;
    std::optional<DanInfoCache> dan_info_cache;

    DanTransition dan_transition;

    std::unique_ptr<OutlinedText> hori_name;

    // Cumulative stat tracking across songs
    int prev_good = 0, prev_ok = 0, prev_bad = 0, prev_drumroll = 0;
    std::string current_song_title;

    void init_dan();
    void change_song();

    DanInfoCache calculate_dan_info();
    int get_exam_progress(const Exam& exam);
    void check_exam_failures();

    void draw_dan_info();
    void draw_digit_counter(const std::string& digits, float margin_x, TexID tex_id, int index, float y, float x_offset = 0);
};
