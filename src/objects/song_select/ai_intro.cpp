#include "ai_intro.h"

AIIntro::AIIntro() {
    if (!load("AIIntro", "ai_intro")) {
        return;
    }
    fn_update = lua_object["update"];
    fn_input = lua_object["input"];
    fn_draw = lua_object["draw"];
    fn_is_done = lua_object["is_done"];
}

void AIIntro::update(double current_ms) {
    call(fn_update, "AIIntro:update", current_ms);
}

void AIIntro::input(Action action) {
    call(fn_input, "AIIntro:input", static_cast<int>(action));
}

void AIIntro::draw() {
    call(fn_draw, "AIIntro:draw");
}

bool AIIntro::is_done() {
    return call_r<bool>(fn_is_done, "AIIntro:is_done").value_or(true);
}
