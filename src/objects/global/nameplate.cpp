#include "nameplate.h"

Nameplate::Nameplate(std::string name, std::string title, PlayerNum player_num, int dan, bool is_gold, bool is_rainbow, int title_bg) {
    if (!load("Nameplate", "nameplate", name, title, static_cast<int>(player_num), dan, is_gold, is_rainbow, title_bg)) return;
    fn_update = lua_object["update"];
    fn_draw   = lua_object["draw"];
}

void Nameplate::update(double current_ms)           { call(fn_update, "Nameplate:update", current_ms); }
void Nameplate::draw(float x, float y, float fade)  { call(fn_draw,   "Nameplate:draw", x, y, fade); }
