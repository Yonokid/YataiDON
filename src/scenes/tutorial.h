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

    std::optional<TutorialOverlay> overlay;
    std::vector<Hit> hits;
    int rim_count = 0;
    fs::path prev_song;
    int prev_difficulty = 0;

    double chart_ms() const { return clock.position() - start_delay; }
};
