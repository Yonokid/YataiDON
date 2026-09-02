#pragma once

#include "../../libs/global_data.h"
#include "../../libs/animation.h"

class Gauge {
public:

    Gauge(int total_notes, int difficulty, int level, PlayerNum player_num);

    void add_good();
    void add_ok();
    void add_bad();
    void update(double current_ms);
    void draw(float y = 0.0f);

    bool get_is_clear() const { return points >= clear_points; }
    bool get_is_rainbow() const { return points >= max_points; }
    float get_length() const { return (float)points / max_points * 100;}

private:
    int good_points;
    int ok_points;
    int bad_points;
    int points = 0;
    int max_points = 10000;
    int clear_points = 8000;
    int points_per_bar = 200;

    std::string string_diff;
    PlayerNum player_num;
    TextureChangeAnimation* tamashii_fire_change;
    FadeAnimation* gauge_update_anim;
    std::optional<FadeAnimation*> rainbow_fade_in;
    double rainbow_start_ms = -1.0;
    float rainbow_frac = 0.0f;
    bool anims_loaded = false;
    int difficulty;
    int previous_points = 0;
    static constexpr float max_length = 100.0f;

    struct GaugeTable {
        float soul_percent;
        float ok_multiplier;
        float bad_multiplier;
    };
    std::vector<std::vector<GaugeTable>> table = {
        // Easy (★1–5, ★6–10 unused)
        {
            {60.0f,    0.75f, -0.5f},  // ★1
            {63.333f,  0.75f, -0.5f},  // ★2
            {63.333f,  0.75f, -0.5f},  // ★3
            {73.333f,  0.75f, -0.5f},  // ★4
            {73.333f,  0.75f, -0.5f},  // ★5
            {0.0f, 0.0f, 0.0f},        // ★6 (unused)
            {0.0f, 0.0f, 0.0f},        // ★7 (unused)
            {0.0f, 0.0f, 0.0f},        // ★8 (unused)
            {0.0f, 0.0f, 0.0f},        // ★9 (unused)
            {0.0f, 0.0f, 0.0f},        // ★10 (unused)
        },
        // Normal (★1–7, ★8–10 unused)
        {
            {65.6f,  0.75f, -0.5f},   // ★1 (ok/bad assumed, not directly confirmed)
            {65.6f,  0.75f, -0.5f},   // ★2 (assumed)
            {69.5f,  0.75f, -0.5f},   // ★3 (assumed)
            {70.3f,  0.75f, -0.75f},  // ★4
            {75.0f,  0.75f, -1.0f},   // ★5 (ok assumed)
            {75.0f,  0.75f, -1.0f},   // ★6 (assumed)
            {75.0f,  0.75f, -1.0f},   // ★7 (assumed)
            {0.0f, 0.0f, 0.0f},       // ★8 (unused)
            {0.0f, 0.0f, 0.0f},       // ★9 (unused)
            {0.0f, 0.0f, 0.0f},       // ★10 (unused)
        },
        // Hard (★1–8, ★9–10 unused)
        {
            {77.6f,   0.75f, -0.75f}, // ★1 (ok assumed)
            {77.6f,   0.75f, -0.75f}, // ★2 (assumed)
            {72.5f,   0.75f, -1.0f},  // ★3 (ok assumed)
            {69.15f,  0.75f, -1.17f}, // ★4 (ok assumed)
            {67.5f,   0.75f, -1.25f}, // ★5 (ok assumed)
            {68.74f,  0.75f, -1.25f}, // ★6 (assumed)
            {68.74f,  0.75f, -1.25f}, // ★7 (assumed)
            {68.74f,  0.75f, -1.25f}, // ★8 (assumed)
            {0.0f, 0.0f, 0.0f},       // ★9 (unused)
            {0.0f, 0.0f, 0.0f},       // ★10 (unused)
        },
        // Oni (★1–10)
        {
            {70.75f, 0.5f, -1.6f},  // ★1
            {70.75f, 0.5f, -1.6f},  // ★2
            {70.75f, 0.5f, -1.6f},  // ★3
            {70.75f, 0.5f, -1.6f},  // ★4
            {70.75f, 0.5f, -1.6f},  // ★5
            {70.75f, 0.5f, -1.6f},  // ★6
            {70.75f, 0.5f, -1.6f},  // ★7
            {70.0f,  0.5f, -2.0f},  // ★8
            {76.75f, 0.5f, -2.0f},  // ★9
            {76.75f, 0.5f, -2.0f},  // ★10
        },
    };
};
