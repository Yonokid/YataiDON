#pragma once

#include <vector>

#include "../objects/game/tutorial_overlay.h"
#include "game.h"

class TutorialScreen : public GameScreen {
public:
    TutorialScreen() : GameScreen("game") {}

    struct Hit {
        bool kat;
        bool right;
    };

    Modifiers get_player_modifiers(PlayerNum pn) override;
    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;

protected:
    void create_players() override;
    std::string background_scene_preset() const override { return "TUTORIAL"; }

private:
    static constexpr int SKIP_HITS = 5;
    static constexpr float LANE_Y = 276.0f;
    static constexpr double FOLLOW_ALONG_MS = 250.0;

    std::optional<TutorialOverlay> overlay;
    std::vector<Hit> hits;
    int rim_count = 0;
    std::vector<double> pending_rims;
    double last_kat_ms = 0;
    fs::path prev_song;
    int prev_difficulty = 0;
    Screens return_screen = Screens::SONG_SELECT;
    Screens select_screen() const override { return return_screen; }
    PlayerNum skip_player = PlayerNum::P1;

    double chart_ms() const { return clock.position() - start_delay; }
};
