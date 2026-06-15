#pragma once

#include "../../libs/script.h"

class AllNetIcon : public LuaScript {
    sol::protected_function fn_update;
    sol::protected_function fn_draw;
public:
    AllNetIcon();
    void update(double current_ms);
    void draw(float x = 0, float y = 0);
};
