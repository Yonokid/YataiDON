#include "result_crown.h"

ResultCrown::ResultCrown(bool is_2p) : is_2p(is_2p) {
    resize = (TextureResizeAnimation*)tex.get_animation(2);
    resize_fix = (TextureResizeAnimation*)tex.get_animation(3);
    white_fadein = (FadeAnimation*)tex.get_animation(4);
    gleam = (TextureChangeAnimation*)tex.get_animation(5);
    fadein = (FadeAnimation*)tex.get_animation(6);
    resize->start();
    resize_fix->start();
    white_fadein->start();
    gleam->start();
    fadein->start();
}

void ResultCrown::update(double current_ms) {
    fadein->update(current_ms);
    resize->update(current_ms);
    resize_fix->update(current_ms);
    white_fadein->update(current_ms);
    gleam->update(current_ms);
    if (resize_fix->is_finished && !sound_played) {
        audio->play_sound("crown", "sound");
        sound_played = true;
    }
}

void ResultCrown::draw(CrownType crown_type) {
    std::string crown_name;
    if (crown_type == CrownType::CROWN_CLEAR) {
        crown_name = "crown_clear";
    } else if (crown_type == CrownType::CROWN_FC) {
        crown_name = "crown_fc";
    } else if (crown_type == CrownType::CROWN_DFC) {
        crown_name = "crown_dfc";
    }
    float scale = resize->attribute;
    if (resize->is_finished) scale = resize_fix->attribute;
    tex.draw_texture("crown", crown_name, {.scale=scale, .center=true, .index=is_2p});
    tex.draw_texture("crown", "crown_fade", {.fade=white_fadein->attribute, .index=is_2p});
    if (gleam->attribute >= 0) {
        tex.draw_texture("crown", "gleam", {.frame=(int)gleam->attribute, .index=is_2p});
    }
}
