#include "indicator.h"

Indicator::Indicator(State state) : state(state) {
    don_fade = (FadeAnimation*)global_tex.get_animation(6);
    blue_arrow_move = (MoveAnimation*)global_tex.get_animation(7);
    blue_arrow_fade = (FadeAnimation*)global_tex.get_animation(8);
    select_text = new OutlinedText(
        global_tex.skin_config["indicator_text"].text[get_config().general.language],
        global_tex.skin_config["indicator_text"].font_size,
        ray::WHITE, ray::BLACK, -3);
}

void Indicator::update(double current_ms) {
    don_fade->update(current_ms);
    blue_arrow_move->update(current_ms);
    blue_arrow_fade->update(current_ms);
}

void Indicator::draw(float x, float y, float fade) {
    int state_val = static_cast<int>(state);
    global_tex.draw_texture("indicator", "background", {.x=x, .y=y, .fade=fade});
    global_tex.draw_texture("indicator", "text", {.color=ray::BLACK, .frame=state_val, .x=x, .y=y, .fade=fade});
    select_text->draw({.x=x + global_tex.skin_config["indicator_text"].x, .y=y, .fade=fade});
    global_tex.draw_texture("indicator", "drum_face", {.x=x, .y=y, .fade=fade, .index=state_val});
    if (state == State::SELECT) {
        global_tex.draw_texture("indicator", "drum_kat", {.x=x, .y=y, .fade=std::min(fade, (float)don_fade->attribute)});
        global_tex.draw_texture("indicator", "drum_kat", {.mirror="horizontal", .x=x + global_tex.skin_config["indicator_kat_offset"].x, .y=y, .fade=std::min(fade, (float)don_fade->attribute)});
        global_tex.draw_texture("indicator", "drum_face", {.x=x + global_tex.skin_config["indicator_face_offset"].x, .y=y, .fade=fade});
        global_tex.draw_texture("indicator", "drum_don", {.x=x + global_tex.skin_config["indicator_don_offset"].x, .y=y, .fade=std::min(fade, (float)don_fade->attribute), .index=state_val});
        global_tex.draw_texture("indicator", "blue_arrow", {.x=(float)(x - blue_arrow_move->attribute), .y=y, .fade=std::min(fade, (float)blue_arrow_fade->attribute)});
        global_tex.draw_texture("indicator", "blue_arrow", {.mirror="horizontal", .x=(float)(x + blue_arrow_move->attribute), .y=y, .fade=std::min(fade, (float)blue_arrow_fade->attribute), .index=1});
    } else {
        global_tex.draw_texture("indicator", "drum_don", {.x=x, .y=y, .fade=std::min(fade, (float)don_fade->attribute), .index=state_val});
    }
}
