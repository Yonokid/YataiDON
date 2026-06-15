#pragma once

#include "../../libs/script.h"

class Indicator : public LuaScript {
public:
    enum class State {
        SKIP = 0,
        SIDE = 1,
        SELECT = 2,
        WAIT = 3
    };

private:
    sol::protected_function fn_update;
    sol::protected_function fn_draw;

public:
    Indicator(State state);
    void update(double current_ms);
    void draw(float x, float y, float fade = 1.0f);
};
