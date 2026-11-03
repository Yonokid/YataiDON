#include "nameplate.h"

Nameplate::Nameplate(const std::string& name, const std::string& title, PlayerNum player_num, int dan, bool is_gold,
                     bool is_rainbow, int title_bg) {
    const int seat = static_cast<int>(player_num);
    const int team = seat == 1 || seat == 2 ? global_data.kinotake_team[seat - 1] : -1;
    if (!load("Nameplate", "nameplate", name, title, seat, dan, is_gold, is_rainbow, title_bg, team)) {
        return;
    }
    fn_update = lua_object["update"];
    fn_draw = lua_object["draw"];
}

void Nameplate::update(double current_ms) {
    call(fn_update, "Nameplate:update", current_ms);
}
void Nameplate::draw(float x, float y, float fade) {
    call(fn_draw, "Nameplate:draw", x, y, fade);
}
