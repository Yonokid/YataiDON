#include "practice_help.h"

PracticeHelp::PracticeHelp(bool is_2p) {
    if (!load("PracticeHelp", "practice_help", is_2p)) {
        return;
    }
    fn_update = lua_object["update"];
    fn_input = lua_object["input"];
    fn_draw = lua_object["draw"];
    fn_is_done = lua_object["is_done"];
}

void PracticeHelp::update(double current_ms) {
    call(fn_update, "PracticeHelp:update", current_ms);
}
bool PracticeHelp::input(Action action) {
    return call_r<bool>(fn_input, "PracticeHelp:input", static_cast<int>(action)).value_or(false);
}
void PracticeHelp::draw() {
    call(fn_draw, "PracticeHelp:draw");
}
bool PracticeHelp::is_done() {
    return call_r<bool>(fn_is_done, "PracticeHelp:is_done").value_or(true);
}
