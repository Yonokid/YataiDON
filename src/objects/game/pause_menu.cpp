#include "pause_menu.h"

void PauseMenu::init_textures() {
    fn_draw = {};
    if (!load("PauseMenu", "pause_menu")) {
        return;
    }
    fn_draw = lua_object["draw"];
}

void PauseMenu::unload() {
    fn_draw = {};
    lua_object = sol::table();
}

void PauseMenu::open_menu(double now_ms) {
    open = true;
    index = 0;
    opened_ms = now_ms;
}

void PauseMenu::close() {
    open = false;
}

void PauseMenu::step(bool down) {
    index = ((index + (down ? 1 : -1)) % ROW_COUNT + ROW_COUNT) % ROW_COUNT;
}

PauseMenu::Action PauseMenu::confirm() const {
    switch (index) {
    case 0:
        return Action::RESUME;
    case 1:
        return Action::RESTART;
    default:
        return Action::QUIT;
    }
}

void PauseMenu::draw(double now_ms) {
    if (open) {
        call(fn_draw, "PauseMenu:draw", index, now_ms - opened_ms);
    }
}
