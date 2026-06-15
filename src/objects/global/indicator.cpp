#include "indicator.h"

Indicator::Indicator(State state) {
    if (!load("Indicator", "indicator", static_cast<int>(state))) return;
    fn_update = lua_object["update"];
    fn_draw   = lua_object["draw"];
}

void Indicator::update(double current_ms)          { call(fn_update, "Indicator:update", current_ms); }
void Indicator::draw(float x, float y, float fade) { call(fn_draw, "Indicator:draw", x, y, fade); }
