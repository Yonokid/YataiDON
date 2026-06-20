#include "fade_in.h"

FadeIn::FadeIn(PlayerNum player_num) {
    if (!load("ResultFadeIn", "result_fade_in", (int)player_num)) return;
    fn_update     = lua_object["update"];
    fn_is_finished = lua_object["is_finished"];
    fn_draw       = lua_object["draw"];
}

void FadeIn::update(double current_ms) {
    call(fn_update, "ResultFadeIn:update", current_ms);
}

bool FadeIn::is_finished() {
    return call_r<bool>(fn_is_finished, "ResultFadeIn:is_finished").value_or(false);
}

void FadeIn::draw() {
    call(fn_draw, "ResultFadeIn:draw");
}
