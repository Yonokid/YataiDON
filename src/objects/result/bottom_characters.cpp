#include "bottom_characters.h"

BottomCharacters::BottomCharacters() {
    if (!load("BottomCharacters", "bottom_characters")) return;
    fn_start      = lua_object["start"];
    fn_update     = lua_object["update"];
    fn_draw       = lua_object["draw"];
    fn_is_finished = lua_object["is_finished"];
}

void BottomCharacters::start() {
    call(fn_start, "BottomCharacters:start");
}

void BottomCharacters::update(double current_ms, int state) {
    call(fn_update, "BottomCharacters:update", current_ms, state);
}

bool BottomCharacters::is_finished() {
    return call_r<bool>(fn_is_finished, "BottomCharacters:is_finished").value_or(false);
}

void BottomCharacters::draw() {
    call(fn_draw, "BottomCharacters:draw");
}
