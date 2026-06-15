#include "allnet_indicator.h"
#include "../../libs/config.h"

AllNetIcon::AllNetIcon() {
    if (!load("AllNetIcon", "allnet_indicator", get_config().general.fake_online)) return;
    fn_update = lua_object["update"];
    fn_draw   = lua_object["draw"];
}

void AllNetIcon::update(double current_ms) { call(fn_update, "AllNetIcon:update", current_ms); }
void AllNetIcon::draw(float x, float y)    { call(fn_draw,   "AllNetIcon:draw", x, y); }
