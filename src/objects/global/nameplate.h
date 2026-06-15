#pragma once

#include "../../libs/script.h"
#include "../../libs/global_data.h"

class Nameplate : public LuaScript {
    sol::protected_function fn_update;
    sol::protected_function fn_draw;
public:
    Nameplate() = default;
    Nameplate(std::string name, std::string title, PlayerNum player_num, int dan, bool is_gold, bool is_rainbow, int title_bg);
    void update(double current_ms);
    void draw(float x, float y, float fade = 1.0f);
};
