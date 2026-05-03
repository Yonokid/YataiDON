#pragma once

#include "../../libs/global_data.h"
#include "../../libs/animation.h"
#include "../enums.h"

class ResultGauge {
private:
    PlayerNum player_num;
    int gauge_length;
    bool is_2p;

    Difficulty difficulty;
    std::vector<int> clear_start;
    int gauge_max;
    std::string string_diff;

    TextureChangeAnimation* rainbow_animation;
    FadeAnimation* gauge_fade_in;
    TextureChangeAnimation* tamashii_fire_change;

    ResultState state;

public:
    ResultGauge(PlayerNum player_num, int gauge_length, bool is_2p);

    void update(double current_ms);

    void draw();

    ResultState get_state() const { return state; }

    bool is_clear() const { return state == ResultState::CLEAR || state == ResultState::RAINBOW; }

    bool is_finished() const { return gauge_fade_in->is_finished; }
};
