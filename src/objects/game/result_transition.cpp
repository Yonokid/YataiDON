#include "result_transition.h"
#include "../../libs/texture.h"

ResultTransition::ResultTransition(PlayerNum player_num)
    : player_num(player_num), is_finished(false), is_started(false) {

    move = (MoveAnimation*)global_tex.get_animation(5);
    move->reset();

    if (!load("ResultTransition", "result_transition", static_cast<int>(player_num))) return;
    fn_start       = lua_object["start"];
    fn_update      = lua_object["update"];
    fn_draw        = lua_object["draw"];
    fn_is_finished = lua_object["is_finished"];
}

void ResultTransition::start() {
    move->start();
    call(fn_start, "ResultTransition:start");
}

void ResultTransition::update(double current_ms) {
    move->update(current_ms);
    is_started = move->is_started;
    is_finished = move->is_finished;

    if (!is_started) { is_finished = false; return; }

    call(fn_update, "ResultTransition:update", current_ms);
    auto done = call_r<bool>(fn_is_finished, "ResultTransition:is_finished");
    if (done.has_value()) is_finished = done.value();
}

void ResultTransition::draw() {
    if (fn_draw.valid()) {
        call(fn_draw, "ResultTransition:draw");
        return;
    }
    draw_default();
}

void ResultTransition::draw_default() {
    float x = 0;
    while (x < tex.screen_width) {
        float tex_height = global_tex.textures[RESULT_TRANSITION::_1P_SHUTTER_FOOTER]->height;

        if (player_num == PlayerNum::TWO_PLAYER) {
            global_tex.draw_texture(RESULT_TRANSITION::_1P_SHUTTER, {
                .frame = 0,
                .x = x,
                .y = (float)(-tex.screen_height + move->attribute)
            });
            global_tex.draw_texture(RESULT_TRANSITION::_2P_SHUTTER, {
                .frame = 0,
                .x = x,
                .y = (float)(tex.screen_height - move->attribute)
            });
            global_tex.draw_texture(RESULT_TRANSITION::_1P_SHUTTER_FOOTER, {
                .x = x,
                .y = (float)(-(tex_height * 3) + move->attribute)
            });
            global_tex.draw_texture(RESULT_TRANSITION::_2P_SHUTTER_FOOTER, {
                .x = x,
                .y = (float)(tex.screen_height + (tex_height * 2) - move->attribute)
            });
        } else {
            std::string player_str = std::to_string(static_cast<int>(player_num)) + "p";
            global_tex.draw_texture(tex.get_enum("result_transition/" + (player_str + "_shutter")), {
                .frame = 0,
                .x = x,
                .y = (float)(-tex.screen_height + move->attribute)
            });
            global_tex.draw_texture(tex.get_enum("result_transition/" + (player_str + "_shutter")), {
                .frame = 0,
                .x = x,
                .y = (float)(tex.screen_height - move->attribute)
            });
            global_tex.draw_texture(tex.get_enum("result_transition/" + (player_str + "_shutter_footer")), {
                .x = x,
                .y = (float)(-(tex_height * 3) + move->attribute)
            });
            global_tex.draw_texture(tex.get_enum("result_transition/" + (player_str + "_shutter_footer")), {
                .x = x,
                .y = (float)(tex.screen_height + (tex_height * 2) - move->attribute)
            });
        }
        x += tex.screen_width / 5.0f;
    }
}
