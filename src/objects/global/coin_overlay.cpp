#include "coin_overlay.h"

CoinOverlay::CoinOverlay() {
    if (!load("CoinOverlay", "coin_overlay")) return;
    fn_update = lua_object["update"];
    fn_draw   = lua_object["draw"];
}

void CoinOverlay::update(double current_ms) { call(fn_update, "CoinOverlay:update", current_ms); }
void CoinOverlay::draw(float x, float y)    { call(fn_draw,   "CoinOverlay:draw", x, y); }
