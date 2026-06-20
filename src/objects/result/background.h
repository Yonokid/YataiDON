#pragma once

#include "../../libs/script.h"
#include "../../libs/global_data.h"

class ResultBackground : public LuaScript {
    sol::protected_function fn_draw;
public:
    ResultBackground() = default;
    ResultBackground(PlayerNum player_num, float width);
    void draw();
};
