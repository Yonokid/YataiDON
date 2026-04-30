#pragma once

#include "../../libs/animation.h"
#include "../../libs/texture.h"
#include "../../libs/global_data.h"

class DanGauge {
public:
    float gauge_length = 0;
    static constexpr float gauge_max = 89.0f;

    DanGauge(PlayerNum player_num, int total_notes);

    void add_good();
    void add_ok();
    void add_bad();
    void update(double current_ms);
    void draw();

    bool get_is_clear() const { return is_rainbow; }
    bool get_is_rainbow() const { return is_rainbow; }

private:
    PlayerNum player_num;
    float previous_length = 0;
    float visual_length = 0;
    int total_notes;
    bool is_rainbow = false;
    bool anims_loaded = false;

    TextureChangeAnimation* tamashii_fire_change = nullptr;
    FadeAnimation* gauge_update_anim = nullptr;
    std::optional<FadeAnimation*> rainbow_fade_in;
    TextureChangeAnimation* rainbow_animation = nullptr;
};
