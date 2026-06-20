#pragma once

#include "../../libs/script.h"

class BottomCharacters : public LuaScript {
    sol::protected_function fn_start, fn_update, fn_draw, fn_is_finished;
public:
    BottomCharacters();
    void start();
    void update(double current_ms, int state);
    bool is_finished();
    void draw();
};
