#pragma once

#include "../libs/global_data.h"
#include "../libs/screen.h"
#include "../objects/global/allnet_indicator.h"
#include "../objects/global/chara_3d.h"
#include "../objects/global/coin_overlay.h"
#include "../objects/global/nameplate.h"
#include "../objects/result/background.h"
#include "../objects/result/dan_result_draw.h"

class DanResultScreen : public Screen {
public:
    DanResultScreen() : Screen("dan_result") {}

    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;

private:
    AllNetIcon allnet_indicator;
    CoinOverlay coin_overlay;
    FadeAnimation* fade_out = nullptr;
    FadeAnimation* page2_fade = nullptr;

    std::optional<ResultBackground> background;
    std::optional<DanResultDraw> draw_seq;

    Nameplate nameplate;
    std::unique_ptr<Chara3D> chara;

    bool is_page2 = false;
    double page_start_ms = 0.0;
    double page1_start_ms = 0.0;

    double totals_start = 0;
    double totals_end = 0;
    std::vector<DanResultRowSchedule> rows;
    double stamp_at = 0;
    double voice_at = 0;
    bool page2_skipped = false;

    bool se_total_intro = false, se_countup = false, se_gauge_max = false, se_stamp = false, se_voice = false;
    std::vector<bool> se_row_fill, se_row_judge;
    int page1_plates_played = 0;

    int gauge_exam = -1;
    int gauge_value = 0;
    int gauge_border = 0;

    bool shodan = false;
    int prev_best = 0;
    bool celebrating = false;
    double celebrate_start_ms = 0;
    bool se_advance = false, se_shogo = false;

    bool congrats_due = false;
    bool congrats_showing = false;
    double congrats_start_ms = 0;
    bool se_congrats = false, se_staffroll = false;
    bool has_staffroll = false;
    int prev_best_score = 0;
    bool best_score_show = false;
    int prev_arrival = 0;
    int nameplate_last_dan = -2;

    void handle_input(double current_ms);
    void build_page2_timeline();
    void update_sounds(double now);
    void apply_reward();
};
