#pragma once

#include <cmath>

#include "../../libs/file/script.h"
#include "../../libs/gfx/texture.h"
#include "../../libs/global_data.h"
#include "../../libs/judge.h"
#include "../../libs/math/animation.h"

class Gauge : public LuaScript, public GaugeCore {
public:
    Gauge(int total_notes, int difficulty, int level, PlayerNum player_num);

    static Gauge dan(const std::vector<DanSongEntry>& songs, int total_notes, PlayerNum player_num);

    void add_good();
    void add_ok();
    void add_bad();
    void update(double current_ms);
    void draw(float y = 0.0f);
    // a scene's own Lua child of Gauge; call before the first draw
    void use_script(std::string class_name, std::string file_name) {
        script_class = std::move(class_name);
        script_file = std::move(file_name);
    }

private:
    Gauge(const GaugeCore& core, PlayerNum player_num);

    bool lua_tried = false;
    std::string script_class = "Gauge";
    std::string script_file = "gauge";
    sol::protected_function fn_draw;
    int points_per_bar = 200;

    std::string string_diff;
    PlayerNum player_num;
    TextureChangeAnimation* tamashii_fire_change;
    FadeAnimation* gauge_update_anim;
    std::optional<FadeAnimation*> rainbow_fade_in;
    double rainbow_start_ms = -1.0;
    float rainbow_frac = 0.0f;
    bool anims_loaded = false;
    static constexpr float max_length = 100.0f;
};
