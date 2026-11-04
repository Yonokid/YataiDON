#include "entry_info.h"

EntryInfo::EntryInfo() {
    if (!load("EntryInfo", "entry_info")) {
        return;
    }
    fn_update = lua_object["update"];
    fn_input = lua_object["input"];
    fn_draw = lua_object["draw"];
    fn_is_done = lua_object["is_done"];
}

void EntryInfo::update(double current_ms) {
    call(fn_update, "EntryInfo:update", current_ms);
}
bool EntryInfo::input() {
    return call_r<bool>(fn_input, "EntryInfo:input").value_or(false);
}
void EntryInfo::draw() {
    call(fn_draw, "EntryInfo:draw");
}
bool EntryInfo::is_done() {
    return call_r<bool>(fn_is_done, "EntryInfo:is_done").value_or(true);
}
