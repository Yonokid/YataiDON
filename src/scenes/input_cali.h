#pragma once

#include "game.h"

class InputCaliScreen : public GameScreen {
public:
    InputCaliScreen() : GameScreen("game") {}

    Modifiers get_player_modifiers(PlayerNum pn) override;
    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;

    std::vector<double> latencies;
    double average_latency;
    std::optional<OutlinedText> average_latency_text;
};
