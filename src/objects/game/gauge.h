#pragma once

#include "../../libs/animation.h"
#include "../../libs/texture.h"
#include "../../libs/global_data.h"
#include <string>
#include <vector>
#include <map>

class Gauge {
private:
    bool is_2p;
    PlayerNum player_num;
    std::string string_diff;
    float gauge_length;
    float previous_length;
    int total_notes;
    int difficulty;
    std::vector<int> clear_start;
    float gauge_max;
    int level;
    TextureChangeAnimation* tamashii_fire_change;
    bool is_clear;
    bool is_rainbow;

    struct GaugeTable {
        float clear_rate;
        float ok_multiplier;
        float bad_multiplier;
    };

    std::vector<std::vector<GaugeTable>> table;

    TextureChangeAnimation* gauge_update_anim;
    FadeAnimation* rainbow_fade_in;
    TextureChangeAnimation* rainbow_animation;

public:
    Gauge(PlayerNum player_num, int difficulty, int level, int total_notes, bool is_2p);

    void add_good();
    void add_ok();
    void add_bad();
    void update(double current_ms);
    void draw();

    bool get_is_clear() const { return is_clear; }
    bool get_is_rainbow() const { return is_rainbow; }
};
