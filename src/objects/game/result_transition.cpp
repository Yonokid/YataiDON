#include "result_transition.h"

ResultTransition::ResultTransition(PlayerNum player_num)
    : player_num(player_num), is_finished(false), is_started(false) {

    move = (MoveAnimation*)global_tex.get_animation(5);
    move->reset();
}

void ResultTransition::start() {
    move->start();
}

void ResultTransition::update(double current_ms) {
    move->update(current_ms);
    is_started = move->is_started;
    is_finished = move->is_finished;
}

void ResultTransition::draw() {
    float x = 0;
    while (x < tex.screen_width) {
        float tex_height = global_tex.textures["result_transition"]["1p_shutter_footer"]->height;

        if (player_num == PlayerNum::TWO_PLAYER) {
            global_tex.draw_texture("result_transition", "1p_shutter", {
                .frame = 0,
                .x = x,
                .y = (float)(-tex.screen_height + move->attribute)
            });
            global_tex.draw_texture("result_transition", "2p_shutter", {
                .frame = 0,
                .x = x,
                .y = (float)(tex.screen_height - move->attribute)
            });
            global_tex.draw_texture("result_transition", "1p_shutter_footer", {
                .x = x,
                .y = (float)(-(tex_height * 3) + move->attribute)
            });
            global_tex.draw_texture("result_transition", "2p_shutter_footer", {
                .x = x,
                .y = (float)(tex.screen_height + (tex_height * 2) - move->attribute)
            });
        } else {
            std::string player_str = std::to_string(static_cast<int>(player_num)) + "p";
            global_tex.draw_texture("result_transition", player_str + "_shutter", {
                .frame = 0,
                .x = x,
                .y = (float)(-tex.screen_height + move->attribute)
            });
            global_tex.draw_texture("result_transition", player_str + "_shutter", {
                .frame = 0,
                .x = x,
                .y = (float)(tex.screen_height - move->attribute)
            });
            global_tex.draw_texture("result_transition", player_str + "_shutter_footer", {
                .x = x,
                .y = (float)(-(tex_height * 3) + move->attribute)
            });
            global_tex.draw_texture("result_transition", player_str + "_shutter_footer", {
                .x = x,
                .y = (float)(tex.screen_height + (tex_height * 2) - move->attribute)
            });
        }
        x += tex.screen_width / 5.0f;
    }
}
