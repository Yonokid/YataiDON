#pragma once

#include "../../libs/global_data.h"
#include "../../libs/animation.h"
#include "../enums.h"

class ResultGauge {
public:
    ResultGauge(GaugeMode mode, PlayerNum player_num, float gauge_length, bool is_2p = false);

    void update(double current_ms);
    void draw(double external_fade = 1.0);

    ResultState get_state() const { return state; }
    bool is_clear() const { return state == ResultState::CLEAR || state == ResultState::RAINBOW; }
    bool is_finished() const { return gauge_fade_in->is_finished; }

private:
    GaugeMode mode;
    PlayerNum player_num;
    float gauge_length;
    bool is_2p;
    float scale;

    Difficulty difficulty;
    std::vector<int> clear_start;
    float gauge_max;
    std::string string_diff;

    FadeAnimation* gauge_fade_in;
    TextureChangeAnimation* tamashii_fire_change;
    bool anims_loaded = false;

    // NORMAL mode rainbow (fmod-based, same as game gauge)
    double rainbow_start_ms = -1.0;
    float rainbow_frac = 0.0f;

    ResultState state;

    // DAN mode
    float visual_length = 0;
};
