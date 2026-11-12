#include "toast.h"

#include "../../libs/notify.h"

Toast::Toast() {
    if (!load("Toast", "toast")) {
        return;
    }
    available = true;
    fn_update = lua_object["update"];
    fn_draw = lua_object["draw"];
    fn_show = lua_object["show"];
}

void Toast::update(double current_ms) {
    for (const auto& text : take_notifications()) {
        call(fn_show, "Toast:show", text);
    }
    call(fn_update, "Toast:update", current_ms);
}

void Toast::draw() {
    call(fn_draw, "Toast:draw");
}
