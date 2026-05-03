#include "bottom_characters.h"
#include "../../libs/texture.h"

BottomCharacters::BottomCharacters() {
    move_up = (MoveAnimation*)tex.get_animation(7);
    move_down = (MoveAnimation*)tex.get_animation(8);
    bounce_up = (MoveAnimation*)tex.get_animation(9);
    bounce_down = (MoveAnimation*)tex.get_animation(10);
    move_center = (MoveAnimation*)tex.get_animation(11);
    c_bounce_up = (MoveAnimation*)tex.get_animation(12);
    c_bounce_down = (MoveAnimation*)tex.get_animation(13);
    flower_up = (MoveAnimation*)tex.get_animation(14);
}

void BottomCharacters::start() {
    move_up->start();
    move_down->start();
    c_bounce_up->start();
    c_bounce_down->start();
}

void BottomCharacters::update(ResultState state) {
    double current_ms = get_current_ms();
    this->state = state;
    if (state == ResultState::CLEAR || state == ResultState::RAINBOW) {
        chara_0_index = 1;
        chara_1_index = 1;
        if (!bounce_up->is_started) {
            bounce_up->start();
            bounce_down->start();
            move_center->start();
        }
        if (!flower_start.has_value()) {
            flower_up->start();
            flower_start = current_ms;
        }
    } else if (state == ResultState::FAIL) {
        chara_0_index = 2;
        chara_1_index = 2;
    }

    move_up->update(current_ms);
    move_down->update(current_ms);
    bounce_up->update(current_ms);
    bounce_down->update(current_ms);
    if (bounce_down->is_finished) {
        bounce_up->restart();
        bounce_down->restart();
    }
    move_center->update(current_ms);
    flower_up->update(current_ms);

    if (flower_start.has_value()) {
        if (current_ms > flower_start.value() + 116*2 + 333) {
            flower_index = 2;
        } else if (current_ms > flower_start.value() + 116 + 333) {
            flower_index = 1;
        }
    }

    c_bounce_up->update(current_ms);
    c_bounce_down->update(current_ms);
    if (c_bounce_down->is_finished) {
        c_bounce_up->restart();
        c_bounce_down->restart();
    }

    if (c_bounce_down->is_finished) {
        c_bounce_up->restart();
        c_bounce_down->restart();
    }
}

void BottomCharacters::draw_flowers() {
    tex.draw_texture(BOTTOM::FLOWERS, {.frame=flower_index, .y=(float)-flower_up->attribute});
    tex.draw_texture(BOTTOM::FLOWERS, {.frame=flower_index, .mirror="horizontal", .x=tex.skin_config[SC::RESULT_FLOWERS_OFFSET].x, .y=(float)-flower_up->attribute});
}

void BottomCharacters::draw() {
    draw_flowers();

    float y = -move_up->attribute + move_down->attribute + bounce_up->attribute - bounce_down->attribute;
    if (state == ResultState::RAINBOW) {
        float center_y = c_bounce_up->attribute - c_bounce_down->attribute;
        tex.draw_texture(BOTTOM::CHARA_CENTER, {.y=(float)-move_center->attribute + center_y});
    }

    tex.draw_texture(BOTTOM::CHARA_0, {.frame=chara_0_index, .y=y});
    tex.draw_texture(BOTTOM::CHARA_1, {.frame=chara_1_index, .y=y});
}

bool BottomCharacters::is_finished() {
    return move_down->is_finished;
}
