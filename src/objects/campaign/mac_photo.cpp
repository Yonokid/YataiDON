#include "mac_photo.h"

MacPhoto::MacPhoto() {
    if (!load("MacPhoto", "mac_photo")) {
        return;
    }
    fn_update = lua_object["update"];
    fn_input = lua_object["input"];
    fn_draw = lua_object["draw"];
    fn_is_done = lua_object["is_done"];
    fn_get_reward = lua_object["get_reward"];
    fn_close = lua_object["close"];
}

MacPhoto::~MacPhoto() {
    if (fn_close.valid()) {
        call(fn_close, "MacPhoto:close");
    }
}

void MacPhoto::update(double current_ms) {
    call(fn_update, "MacPhoto:update", current_ms);
}
bool MacPhoto::input_decide() {
    return call_r<bool>(fn_input, "MacPhoto:input", 3).value_or(false);
}
void MacPhoto::draw() {
    call(fn_draw, "MacPhoto:draw");
}
bool MacPhoto::is_done() {
    return call_r<bool>(fn_is_done, "MacPhoto:is_done").value_or(true);
}
int MacPhoto::get_reward() {
    return call_r<int>(fn_get_reward, "MacPhoto:get_reward").value_or(-1);
}
