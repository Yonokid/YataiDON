#pragma once

#include "../../libs/global_data.h"
#include "../../libs/animation.h"
#include "../enums.h"

class Gauge {
public:
    float gauge_length;
    float gauge_max;

    Gauge(GaugeMode mode, PlayerNum player_num, int total_notes, int difficulty = 0, int level = 1);

    static Gauge make_result(GaugeMode mode, PlayerNum player_num, float gauge_length, bool is_2p = false);

    void add_good();
    void add_ok();
    void add_bad();
    void update(double current_ms);
    void draw(float y = 0.0f);
    void draw_result(double external_fade = 1.0);

    bool get_is_clear() const { return is_clear; }
    bool get_is_rainbow() const { return is_rainbow; }
    double get_soul() const { return soul; }
    float get_progress() const { return gauge_length / gauge_max; }
    float get_flash_attribute() const {
        if (!gauge_update_anim) return 0.0f;
        if ((int)gauge_length <= (int)previous_length) return 0.0f;
        return gauge_update_anim->attribute;
    }

    float get_clear_progress() const {
        if (chn_model && norma > 0) return (float)norma / 10000.0f;
        if (clear_start.empty() || gauge_max <= 0.0f) return 1.0f;
        int i = difficulty;
        if (i < 0) i = 0;
        if (i >= (int)clear_start.size()) i = (int)clear_start.size() - 1;
        return clear_start[i] / gauge_max;
    }

    ResultState get_state() const { return state; }
    bool result_is_clear() const { return state == ResultState::CLEAR || state == ResultState::RAINBOW; }
    bool result_is_finished() const { return gauge_fade_in && gauge_fade_in->is_finished; }

private:
    Gauge();  // bare init for make_result

    GaugeMode mode;
    PlayerNum player_num;
    int total_notes;
    int difficulty;

    // NORMAL mode
    std::string string_diff;
    std::vector<int> clear_start;
    int level;

    bool chn_model = false;
    double soul = 0.0;
    int tp_great = 0;
    int tp_good = 0;
    int tp_loss = 0;
    int norma = 0;
    int art_index = 0;
    struct GaugeTable {
        float clear_rate;
        float ok_multiplier;
        float bad_multiplier;
    };
    std::vector<std::vector<GaugeTable>> table;
    double rainbow_start_ms = -1.0;
    float rainbow_frac = 0.0f;

    bool anims_loaded = false;

    // Result presentation
    bool is_result = false;
    bool is_2p = false;
    float result_scale = 1.0f;
    ResultState state = ResultState::FAIL;
    FadeAnimation* gauge_fade_in = nullptr;

    // Shared
    float previous_length;
    bool is_clear;
    bool is_rainbow;
    TextureChangeAnimation* tamashii_fire_change;
    FadeAnimation* gauge_update_anim;
    std::optional<FadeAnimation*> rainbow_fade_in;
};
