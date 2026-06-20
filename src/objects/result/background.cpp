#include "background.h"

ResultBackground::ResultBackground(PlayerNum player_num, float width) {
    if (!load("ResultBackground", "result_background", (int)player_num, width)) return;
    fn_draw = lua_object["draw"];
}

void ResultBackground::draw() {
    call(fn_draw, "ResultBackground:draw");
}
