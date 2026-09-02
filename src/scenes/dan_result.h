#pragma once

#include "../libs/screen.h"
#include "../libs/global_data.h"
#include "../libs/text.h"
#include "../objects/result/background.h"
#include "../objects/game/gauge.h"
#include "../objects/global/allnet_indicator.h"
#include "../objects/global/coin_overlay.h"
#include "../objects/global/nameplate.h"
#include "../objects/global/chara_3d.h"
#include "../objects/game/exam_caption.h"

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
    FadeAnimation* fade_out   = nullptr;
    FadeAnimation* page2_fade = nullptr;

    std::optional<ResultBackground> background;
    std::unique_ptr<Gauge>        gauge;
    std::unique_ptr<OutlinedText>    hori_name;
    std::vector<std::unique_ptr<OutlinedText>> song_names;

    ExamCaptionCache exam_captions;

    Nameplate nameplate;
    std::unique_ptr<Chara3D> chara;

    bool is_page2 = false;
    double page_start_ms = 0.0;
    double page1_start_ms = 0.0;
    std::vector<bool> page1_armed;

    double totals_start = 0;
    double totals_end   = 0;
    struct RowSchedule {
        double land  = 0;
        double fill0 = 0;
        double filld = 0;
        double numin = 0;
    };
    std::vector<RowSchedule> rows;
    double stamp_at = 0;
    double voice_at = 0;
    bool page2_skipped = false;

    bool se_total_intro = false, se_countup = false, se_gauge_max = false,
         se_stamp = false, se_voice = false;
    std::vector<bool> se_row_fill, se_row_judge;
    int  page1_plates_played = 0;

    int    gauge_exam   = -1;
    int    gauge_value  = 0;
    int    gauge_border = 0;

    bool shodan       = false;
    int  prev_best    = 0;
    bool celebrating  = false;
    double celebrate_start_ms = 0;
    bool se_advance = false, se_shogo = false;

    bool   congrats_due     = false;
    bool   congrats_showing = false;
    double congrats_start_ms = 0;
    bool   se_congrats = false;
    int  prev_best_score = 0;
    bool best_score_show = false;
    int  prev_arrival    = 0;
    void draw_best_score(double fade, double on_page);
    void draw_congrats(double now);

    void handle_input(double current_ms);
    void build_page2_timeline();
    void update_sounds(double now);
    void apply_reward();
    void draw_page1(double now);
    void draw_page2(double fade, double now);
    void draw_exam_info(double fade, double now, float scale = 1.0f);
    void draw_gauge_row(const Exam& exam, float y, double fade, double now, float scale);
    void draw_celebration(double now);
    void draw_digit_counter(const std::string& digits, float margin_x, TexID id,
                             int index, float y, double fade, float scale,
                             float x_off = 0.0f, double roll_t = -1.0);
    void draw_chara_and_plate();
};
