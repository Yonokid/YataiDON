#pragma once

#include "../libs/screen.h"
#include "../libs/global_data.h"
#include "../libs/text.h"
#include "../objects/result/background.h"
#include "../objects/result/gauge.h"
#include "../objects/global/allnet_indicator.h"
#include "../objects/global/coin_overlay.h"

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
    std::unique_ptr<ResultGauge>  gauge;
    std::unique_ptr<OutlinedText>    hori_name;
    std::vector<std::unique_ptr<OutlinedText>> song_names;

    bool is_page2 = false;

    void handle_input();
    void draw_page1();
    void draw_page2(double fade);
    void draw_exam_info(double fade, float scale = 0.8f);
    void draw_digit_counter(const std::string& digits, float margin_x, TexID id, int index,
                             float y, double fade, float scale);
};
