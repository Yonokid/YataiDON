#pragma once

#include "../libs/screen.h"
#include "../libs/text.h"
#include "../objects/result/background.h"
#include "../objects/result/player.h"
#include "../objects/result/fade_in.h"
#include "../objects/game/song_info.h"
#include "../objects/global/allnet_indicator.h"
#include "../objects/global/coin_overlay.h"

class ResultScreen : public Screen {
protected:
    std::unique_ptr<OutlinedText> song_info;
    std::unique_ptr<OutlinedText> song_info_subtitle;
    FadeAnimation* fade_out;
    AllNetIcon allnet_indicator;
    CoinOverlay coin_overlay;

    std::optional<ResultBackground> background;
    std::optional<ray::Texture2D> loading_graphic;
    double start_ms = 0;
    double skipped_time = 0;
    static constexpr double kFrameMs        = 1000.0 / 120.0;
    static constexpr double kEnableSkipMs   =  100 * kFrameMs;
    static constexpr double kWaitEffectEndMs =  500 * kFrameMs;
    static constexpr double kWaitNextSceneMs =  500 * kFrameMs;
    static constexpr double kAutoNextSceneMs = 3600 * kFrameMs;
    double skip_enabled_ms = 0;
    std::optional<ResultPlayer> player_1;
    std::optional<FadeIn> fade_in;
    std::unique_ptr<SongNum> song_num;

    void handle_input(double current_ms);

    virtual double reveal_end_ms();
    void update_input_and_timeout(double current_ms);

    void draw_overlay();

    void draw_song_info();
public:
    ResultScreen() : Screen("result") {
    }

    void on_screen_start() override;

    std::optional<Screens> update() override;

    Screens on_screen_end(Screens next_screen) override;

    void draw() override;
};
