#include "allnet_indicator.h"
#include "../../libs/config.h"
#include "../../libs/network.h"

AllNetIcon::AllNetIcon() {
    if (!load("AllNetIcon", "allnet_indicator", online)) return;
    fn_update = lua_object["update"];
    fn_draw   = lua_object["draw"];
}

void AllNetIcon::update(double current_ms)
    {
        online = network.is_online();
        call(fn_update, "AllNetIcon:update", current_ms, online);
    }
void AllNetIcon::draw(float x, float y)
    {
        call(fn_draw, "AllNetIcon:draw", x, y);
    }
