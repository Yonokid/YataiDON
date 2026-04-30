#pragma once
#include "../libs/screen.h"
#include "../libs/text.h"
#include "../libs/texture.h"
#include "../libs/audio.h"
#include "../objects/global/allnet_indicator.h"
#include "../objects/global/coin_overlay.h"
#include "../objects/result/background.h"

class DanResultGauge {
public:
    DanResultGauge(PlayerNum player_num, float gauge_length);
    void update(double current_ms);
    void draw(double fade);

private:
    PlayerNum player_num;
    float gauge_length;
    float visual_length;
    const float gauge_max = 89.0f;
    bool is_rainbow;

    bool anims_loaded = false;
    TextureChangeAnimation* tamashii_fire_change = nullptr;
    TextureChangeAnimation* rainbow_animation    = nullptr;
    FadeAnimation*          rainbow_fade_in      = nullptr;
};

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
    std::unique_ptr<DanResultGauge>  gauge;
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
