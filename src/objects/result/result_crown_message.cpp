#include "result_crown_message.h"

ResultCrownMessage::ResultCrownMessage(int frame, bool is_2p) {
    if (!load("ResultCrownMessage", "result_crown_message", frame, is_2p)) return;
    fn_update = lua_object["update"];
    fn_draw   = lua_object["draw"];
}

void ResultCrownMessage::update(double current_ms) {
    call(fn_update, "ResultCrownMessage:update", current_ms);
}

void ResultCrownMessage::draw() {
    call(fn_draw, "ResultCrownMessage:draw");
}
