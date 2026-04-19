#include "indicator.h"

Indicator::Indicator(State state) : state(state) {
    don_fade = (FadeAnimation*)global_tex.get_animation(6);
    blue_arrow_move = (MoveAnimation*)global_tex.get_animation(7);
    blue_arrow_fade = (FadeAnimation*)global_tex.get_animation(8);
    select_text = std::make_unique<OutlinedText>(
        global_tex.skin_config[SC::INDICATOR_TEXT].text[global_data.config->general.language],
        global_tex.skin_config[SC::INDICATOR_TEXT].font_size,
        ray::WHITE, ray::BLANK, false, 0, -5);
}

void Indicator::update(double current_ms) {
    don_fade->update(current_ms);
    blue_arrow_move->update(current_ms);
    blue_arrow_fade->update(current_ms);
}

void Indicator::draw(float x, float y, float fade) {
    int state_val = static_cast<int>(state);
    global_tex.draw_texture(INDICATOR::BACKGROUND, {.x=x, .y=y, .fade=fade});
    global_tex.draw_texture(INDICATOR::TEXT, {.color=ray::BLACK, .frame=state_val, .x=x, .y=y, .fade=fade});
    float text_y = y + ((float)global_tex.textures[INDICATOR::BACKGROUND]->height / 2) - (select_text->height / 2);
    select_text->draw({.x=x + global_tex.skin_config[SC::INDICATOR_TEXT].x, .y=text_y, .fade=fade});
    global_tex.draw_texture(INDICATOR::DRUM_FACE, {.x=x, .y=y, .fade=fade, .index=state_val});
    if (state == State::SELECT) {
        global_tex.draw_texture(INDICATOR::DRUM_KAT, {.x=x, .y=y, .fade=std::min(fade, (float)don_fade->attribute)});
        global_tex.draw_texture(INDICATOR::DRUM_KAT, {.mirror="horizontal", .x=x + global_tex.skin_config[SC::INDICATOR_KAT_OFFSET].x, .y=y, .fade=std::min(fade, (float)don_fade->attribute)});
        global_tex.draw_texture(INDICATOR::DRUM_FACE, {.x=x + global_tex.skin_config[SC::INDICATOR_FACE_OFFSET].x, .y=y, .fade=fade});
        global_tex.draw_texture(INDICATOR::DRUM_DON, {.x=x + global_tex.skin_config[SC::INDICATOR_DON_OFFSET].x, .y=y, .fade=std::min(fade, (float)don_fade->attribute), .index=state_val});
        global_tex.draw_texture(INDICATOR::BLUE_ARROW, {.x=(float)(x - blue_arrow_move->attribute), .y=y, .fade=std::min(fade, (float)blue_arrow_fade->attribute)});
        global_tex.draw_texture(INDICATOR::BLUE_ARROW, {.mirror="horizontal", .x=(float)(x + blue_arrow_move->attribute), .y=y, .fade=std::min(fade, (float)blue_arrow_fade->attribute), .index=1});
    } else {
        global_tex.draw_texture(INDICATOR::DRUM_DON, {.x=x, .y=y, .fade=std::min(fade, (float)don_fade->attribute), .index=state_val});
    }
}
