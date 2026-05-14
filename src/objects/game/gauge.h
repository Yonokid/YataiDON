#pragma once

#include "../../libs/global_data.h"
#include "../../libs/animation.h"
#include "../enums.h"

class Gauge {
public:
    float gauge_length;
    float gauge_max;

    Gauge(GaugeMode mode, PlayerNum player_num, int total_notes, int difficulty = 0, int level = 1);

    void add_good();
    void add_ok();
    void add_bad();
    void update(double current_ms);
    void draw(float y = 0.0f);

    bool get_is_clear() const { return is_clear; }
    bool get_is_rainbow() const { return is_rainbow; }
    float get_progress() const { return gauge_length / gauge_max; }

private:
    GaugeMode mode;
    PlayerNum player_num;
    int total_notes;
    int difficulty;

    // NORMAL mode
    std::string string_diff;
    std::vector<int> clear_start;
    int level;
    struct GaugeTable {
        float clear_rate;
        float ok_multiplier;
        float bad_multiplier;
    };
    std::vector<std::vector<GaugeTable>> table;
    double rainbow_start_ms = -1.0;
    float rainbow_frac = 0.0f;

    // DAN mode
    float visual_length = 0;
    bool anims_loaded = false;

    // Shared
    float previous_length;
    bool is_clear;
    bool is_rainbow;
    TextureChangeAnimation* tamashii_fire_change;
    FadeAnimation* gauge_update_anim;
    std::optional<FadeAnimation*> rainbow_fade_in;
};
