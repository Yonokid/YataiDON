#pragma once

#include <cmath>

#include "../../libs/file/script.h"
#include "../../libs/gfx/texture.h"
#include "../../libs/global_data.h"
#include "../../libs/math/animation.h"

class Gauge : public LuaScript {
public:
    Gauge(int total_notes, int difficulty, int level, PlayerNum player_num);

    static Gauge dan(const std::vector<DanSongEntry>& songs, int total_notes, PlayerNum player_num);

    void add_good();
    void add_ok();
    void add_bad();
    void set_fraction(double fraction) { apply_points_clamped(fraction * max_points - points); }
    void update(double current_ms);
    void draw(float y = 0.0f);
    // a scene's own Lua child of Gauge; call before the first draw
    void use_script(std::string class_name, std::string file_name) {
        script_class = std::move(class_name);
        script_file = std::move(file_name);
    }

    bool get_is_clear() const { return points >= clear_points; }
    bool get_is_rainbow() const { return points >= max_points; }
    bool is_dan() const { return dan_mode; }
    float get_length() const { return points / max_points * 100; }

    // exam threshold check: floor like the arcade and use POINTS_EPS to absorb
    // double accumulation. computed before any narrowing to float.
    int get_percent() const { return static_cast<int>(std::floor((points + POINTS_EPS) / max_points * 100.0)); }

private:
    bool lua_tried = false;
    std::string script_class = "Gauge";
    std::string script_file = "gauge";
    sol::protected_function fn_draw;
    void apply_points_clamped(double delta);
    bool dan_mode = false;
    double good_points;
    double ok_points;
    double bad_points;
    double points = 0;
    int max_points = 10000;
    int clear_points = 8000;
    int points_per_bar = 200;
    static constexpr double POINTS_EPS = 1e-6;

    std::string string_diff;
    PlayerNum player_num;
    TextureChangeAnimation* tamashii_fire_change;
    FadeAnimation* gauge_update_anim;
    std::optional<FadeAnimation*> rainbow_fade_in;
    double rainbow_start_ms = -1.0;
    float rainbow_frac = 0.0f;
    bool anims_loaded = false;
    int difficulty;
    double previous_points = 0;
    static constexpr float max_length = 100.0f;

    struct GaugeTable {
        double soul_percent;
        double ok_multiplier;
        double bad_multiplier;
    };
    std::vector<std::vector<GaugeTable>> table = {
        // Easy (★1–5, ★6–10 unused)
        {
            {60.0, 0.75, -0.5},   // ★1
            {63.333, 0.75, -0.5}, // ★2
            {63.333, 0.75, -0.5}, // ★3
            {73.333, 0.75, -0.5}, // ★4
            {73.333, 0.75, -0.5}, // ★5
            {0.0, 0.0, 0.0},      // ★6 (unused)
            {0.0, 0.0, 0.0},      // ★7 (unused)
            {0.0, 0.0, 0.0},      // ★8 (unused)
            {0.0, 0.0, 0.0},      // ★9 (unused)
            {0.0, 0.0, 0.0},      // ★10 (unused)
        },
        // Normal (★1–7, ★8–10 unused)
        {
            {65.6, 0.75, -0.5},  // ★1 (ok/bad assumed, not directly confirmed)
            {65.6, 0.75, -0.5},  // ★2 (assumed)
            {69.5, 0.75, -0.5},  // ★3 (assumed)
            {70.3, 0.75, -0.75}, // ★4
            {75.0, 0.75, -1.0},  // ★5 (ok assumed)
            {75.0, 0.75, -1.0},  // ★6 (assumed)
            {75.0, 0.75, -1.0},  // ★7 (assumed)
            {0.0, 0.0, 0.0},     // ★8 (unused)
            {0.0, 0.0, 0.0},     // ★9 (unused)
            {0.0, 0.0, 0.0},     // ★10 (unused)
        },
        // Hard (★1–8, ★9–10 unused)
        {
            {77.6, 0.75, -0.75},  // ★1 (ok assumed)
            {77.6, 0.75, -0.75},  // ★2 (assumed)
            {72.5, 0.75, -1.0},   // ★3 (ok assumed)
            {69.15, 0.75, -1.17}, // ★4 (ok assumed)
            {67.5, 0.75, -1.25},  // ★5 (ok assumed)
            {68.74, 0.75, -1.25}, // ★6 (assumed)
            {68.74, 0.75, -1.25}, // ★7 (assumed)
            {68.74, 0.75, -1.25}, // ★8 (assumed)
            {0.0, 0.0, 0.0},      // ★9 (unused)
            {0.0, 0.0, 0.0},      // ★10 (unused)
        },
        // Oni (★1–10)
        {
            {70.75, 0.5, -1.6}, // ★1
            {70.75, 0.5, -1.6}, // ★2
            {70.75, 0.5, -1.6}, // ★3
            {70.75, 0.5, -1.6}, // ★4
            {70.75, 0.5, -1.6}, // ★5
            {70.75, 0.5, -1.6}, // ★6
            {70.75, 0.5, -1.6}, // ★7
            {70.0, 0.5, -2.0},  // ★8
            {76.75, 0.5, -2.0}, // ★9
            {76.75, 0.5, -2.0}, // ★10
        },
    };
};
