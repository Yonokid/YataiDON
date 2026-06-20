#pragma once

#include "../../libs/script.h"
#include "../../libs/global_data.h"

class FadeIn : public LuaScript {
    sol::protected_function fn_update, fn_is_finished, fn_draw;
public:
    FadeIn() = default;
    FadeIn(PlayerNum player_num);
    void update(double current_ms);
    bool is_finished();
    void draw();
};
