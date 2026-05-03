#include "dan_gauge.h"
#include "../../libs/texture.h"

DanGauge::DanGauge(PlayerNum player_num, int total_notes)
    : player_num(player_num), total_notes(total_notes) {}

void DanGauge::add_good() {
    if (gauge_update_anim) gauge_update_anim->start();
    previous_length = (int)gauge_length;
    gauge_length += (1.0f / (total_notes * (gauge_max / 100.0f))) * 100.0f;
    if (gauge_length > gauge_max) gauge_length = gauge_max;
    visual_length = gauge_length * tex.textures[GAUGE_DAN::_1P_BAR]->width;
}

void DanGauge::add_ok() {
    if (gauge_update_anim) gauge_update_anim->start();
    previous_length = (int)gauge_length;
    gauge_length += (0.5f / (total_notes * (gauge_max / 100.0f))) * 100.0f;
    if (gauge_length > gauge_max) gauge_length = gauge_max;
    visual_length = gauge_length * tex.textures[GAUGE_DAN::_1P_BAR]->width;
}

void DanGauge::add_bad() {
    previous_length = (int)gauge_length;
    gauge_length -= (2.0f / (total_notes * (gauge_max / 100.0f))) * 100.0f;
    if (gauge_length < 0) gauge_length = 0;
    visual_length = gauge_length * tex.textures[GAUGE_DAN::_1P_BAR]->width;
}

void DanGauge::update(double current_ms) {
    if (!anims_loaded) {
        tamashii_fire_change = (TextureChangeAnimation*)tex.get_animation(25);
        gauge_update_anim    = (FadeAnimation*)tex.get_animation(10);
        rainbow_animation    = (TextureChangeAnimation*)tex.get_animation(64);
        anims_loaded = true;
    }

    is_rainbow = (gauge_length == gauge_max);

    if (gauge_length == gauge_max && !rainbow_fade_in.has_value()) {
        rainbow_fade_in = (FadeAnimation*)tex.get_animation(63);
        rainbow_fade_in.value()->start();
    }

    gauge_update_anim->update(current_ms);
    tamashii_fire_change->update(current_ms);

    if (rainbow_fade_in.has_value())
        rainbow_fade_in.value()->update(current_ms);

    rainbow_animation->update(current_ms);
}

void DanGauge::draw() {
    tex.draw_texture(GAUGE_DAN::BORDER, {});
    tex.draw_texture(GAUGE_DAN::_1P_UNFILLED, {});
    tex.draw_texture(GAUGE_DAN::_1P_BAR, {.x2 = visual_length - 8});

    if (is_rainbow && rainbow_fade_in.has_value() && rainbow_animation != nullptr) {
        float fade = rainbow_fade_in.value()->attribute;
        if (rainbow_animation->attribute > 0 && rainbow_animation->attribute < 8) {
            tex.draw_texture(GAUGE_DAN::RAINBOW, {.frame = (int)rainbow_animation->attribute - 1, .fade = fade});
        }
        tex.draw_texture(GAUGE_DAN::RAINBOW, {.frame = (int)rainbow_animation->attribute, .fade = fade});
    }

    if ((int)gauge_length <= (int)gauge_max && (int)gauge_length > (int)previous_length) {
        tex.draw_texture(GAUGE_DAN::_1P_BAR_FADE, {.x = visual_length - 8, .fade = gauge_update_anim->attribute});
    }

    tex.draw_texture(GAUGE_DAN::OVERLAY, {.fade = 0.15f});

    if (is_rainbow) {
        tex.draw_texture(GAUGE_DAN::TAMASHII_FIRE, {.frame = (int)tamashii_fire_change->attribute, .scale = 0.75f, .center = true});
        tex.draw_texture(GAUGE_DAN::TAMASHII, {});
        int f = (int)tamashii_fire_change->attribute;
        if (f == 0 || f == 1 || f == 4 || f == 5)
            tex.draw_texture(GAUGE_DAN::TAMASHII_OVERLAY, {.fade = 0.5f});
    } else {
        tex.draw_texture(GAUGE_DAN::TAMASHII_DARK, {});
    }
}
