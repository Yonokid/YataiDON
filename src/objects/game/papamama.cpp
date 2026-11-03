#include "papamama.h"

PapamamaPopup::PapamamaPopup() {
    if (!load("Papamama", "papamama")) {
        return;
    }
    fn_update = lua_object["update"];
    fn_draw = lua_object["draw"];
}

void PapamamaPopup::update(double current_ms) {
    call(fn_update, "Papamama:update", current_ms);
}

void PapamamaPopup::draw() {
    call(fn_draw, "Papamama:draw");
}
