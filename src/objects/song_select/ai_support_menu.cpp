#include "ai_support_menu.h"

AISupportMenu::AISupportMenu() {
    if (!load("AISupportMenu", "ai_support_menu")) {
        return;
    }
    fn_draw = lua_object["draw"];
}

void AISupportMenu::draw(double ms, int course, int cursor, bool choosing, bool branching, int player, int tension) {
    call(fn_draw, "AISupportMenu:draw", ms, course, cursor, choosing, branching, player, tension);
}
