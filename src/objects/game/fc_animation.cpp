#include "fc_animation.h"

FCAnimation::FCAnimation(bool is_2p, bool donderful, bool two_player) {
    if (!load("FCAnimation", "fc_animation", is_2p, donderful, two_player)) {
        return;
    }
    fn_update = lua_object["update"];
    fn_draw = lua_object["draw"];
}

void FCAnimation::update(double current_ms) {
    call(fn_update, "FCAnimation:update", current_ms);
}

void FCAnimation::draw() {
    call(fn_draw, "FCAnimation:draw");
}
