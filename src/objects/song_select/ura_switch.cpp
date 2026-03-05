#include "ura_switch.h"

UraSwitchAnimation::UraSwitchAnimation() {
    texture_change = (TextureChangeAnimation*)tex.get_animation(7);
    fade_out = (FadeAnimation*)tex.get_animation(8);
}

void UraSwitchAnimation::start(bool is_backwards) {
    if (is_backwards) {
        texture_change = (TextureChangeAnimation*)tex.get_animation(6);
    }
    texture_change->start();
    fade_out->start();
}

void UraSwitchAnimation::update(double current_ms) {
    texture_change->update(current_ms);
    fade_out->update(current_ms);
}

void UraSwitchAnimation::draw() {
    tex.draw_texture("diff_select", "ura_switch", {.frame=(int)texture_change->attribute, .fade=fade_out->attribute});
}
