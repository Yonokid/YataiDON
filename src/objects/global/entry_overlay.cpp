#include "entry_overlay.h"
#include "../../libs/config.h"

EntryOverlay::EntryOverlay() {
    if (!load("EntryOverlay", "entry_overlay", get_config().general.fake_online)) return;
    fn_update = lua_object["update"];
    fn_draw   = lua_object["draw"];
}

void EntryOverlay::update(double current_ms) { call(fn_update, "EntryOverlay:update", current_ms); }
void EntryOverlay::draw(float x, float y)    { call(fn_draw,   "EntryOverlay:draw", x, y); }
