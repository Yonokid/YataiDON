#pragma once

#include "../libs/screen.h"
#include "../objects/global/allnet_indicator.h"
#include "../objects/global/coin_overlay.h"

class GameOverScreen : public Screen {
private:
    AllNetIcon allnet_indicator;
    CoinOverlay coin_overlay;

    MoveAnimation* curtain_pull_out;
    MoveAnimation* curtain_pull_in;
    TextureChangeAnimation* kitsune_texture_change;
    MoveAnimation* text_bounce_down;
    MoveAnimation* text_bounce_up;
    MoveAnimation* text_bounce_down_2;
    FadeAnimation* fade_out;
    FadeAnimation* copyright_fade = nullptr;
    FadeAnimation* copyright_hold = nullptr;
    bool has_copyright = false;
    FadeAnimation* blackout_cue = nullptr;
    bool ad_played;
    bool voice_played;

public:
    GameOverScreen() : Screen("game_over") {
    }

    void on_screen_start() override;

    Screens on_screen_end(Screens next_screen) override;

    std::optional<Screens> update() override;

    void draw() override;
};
